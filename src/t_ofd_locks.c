#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>

/*
 * In distributions that do not have these macros ready in glibc-headers,
 * compilation fails. Adding them here to avoid build errors, relevant tests
 * would fail at the helper which requires OFD locks support and notrun if the
 * kernel does not support OFD locks. If the kernel does support OFD locks, we
 * are good to go.
 */
#ifndef F_OFD_GETLK
#define F_OFD_GETLK    36
#endif

#ifndef F_OFD_SETLK
#define F_OFD_SETLK    37
#endif

#ifndef F_OFD_SETLKW
#define F_OFD_SETLKW   38
#endif

/*
 * Usually we run getlk routine after running setlk routine
 * in background. However, getlk could be executed before setlk
 * sometimes, which is invalid for our tests. So we use semaphore
 * to synchronize between getlk and setlk.
 *
 * setlk routine:				 * getlk routine:
 *						 *
 *   start					 *   start
 *     |					 *     |
 *  open file					 *  open file
 *     |					 *     |
 *  init sem					 *     |
 *     |					 *     |
 * wait init sem done				 * wait init sem done
 *     |					 *     |
 *   setlk					 *     |
 *     |					 *     |
 *     |------------clone()--------|		 *     |
 *     |                           |		 *     |
 *     |(parent)            (child)|		 *     |
 *     |                           |		 *     |
 *     |                      close fd		 *     |
 *     |                           |		 *     |
 *     |                     set sem0=0          * wait sem0==0
 *     |                           |		 *     |
 *     |                           |		 *   getlk
 *     |                           |		 *     |
 *  wait sem1==0                   |    	 *  set sem1=0
 *     |                           |		 *     |
 *   wait child                    |    	 *     |
 *     |                           |		 *  check result
 *     |                           |		 *     |
 *    exit                       exit		 *    exit
 */

static int fd;
static int semid;

/* This is required by semctl to set semaphore value */
union semun {
       int              val;    /* Value for SETVAL */
       struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
       unsigned short  *array;  /* Array for GETALL, SETALL */
       struct seminfo  *__buf;  /* Buffer for IPC_INFO
                                   (Linux-specific) */
};

static void err_exit(char *op, int errn)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errn));
	if (fd > 0)
		close(fd);
	if (semid > 0 && semctl(semid, 2, IPC_RMID) == -1)
		perror("exit rmid");
	exit(errn);
}

/*
 * Flags that used to specify operation details.
 * They can be specified via command line options.
 *
 * option: -P
 * posix : 1 <--> test posix lock
 *	   0 <--> test OFD lock (default)
 *
 * option: -s/-g
 * lock_cmd : 1 <--> setlk (default)
 *	      0 <--> getlk
 *
 * option: -r/-w
 * lock_rw : 1 <--> set/get wrlck (default)
 *	     0 <--> set/get rdlck
 *
 * option: -o num
 * lock_start : l_start to getlk
 *
 * option: -F
 * clone_fs : clone with CLONE_FILES
 *
 * option: -d
 * use_dup : dup and close to setup condition in setlk
 *
 * option: -R/-W
 * open_rw : 1 <--> open file RDWR (default)
 *	     0 <--> open file RDONLY
 *
 * This option is for _require_ofd_locks helper, just do
 * fcntl setlk then return errno.
 * option: -t
 * testrun : 1 <--> this is a testrun, return after setlk
 *	     0 <--> this is not a testrun, run as usual
 */

static void usage(char *arg0)
{
	printf("Usage: %s [-sgrwo:l:RWPtFd] filename\n", arg0);
	printf("\t-s/-g : to setlk or to getlk\n");
	printf("\t-P : POSIX locks\n");
	printf("\t-F : clone with CLONE_FILES in setlk to setup test condition\n");
	printf("\t-d : dup and close in setlk\n");
	printf("\twithout both -F/d, use clone without CLONE_FILES\n");
	printf("\t-r/-w : set/get rdlck/wrlck\n");
	printf("\t-o num : offset start to lock, default 0\n");
	printf("\t-l num : lock length, default 10\n");
	printf("\t-R/-W : open file RDONLY/RDWR\n\n");
	printf("\tUsually we run a setlk routine in background and then\n");
	printf("\trun a getlk routine to check. They must be paired, or\n");
	printf("\ttest will hang.\n\n");
	exit(0);
}

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE] __attribute__((aligned));

static int child_fn(void* p)
{
	union semun semu;
	int cfd = *(int *)p;

	/* close relative fd */
	if (cfd > 0 && close(cfd) == -1)
		perror("close in child");

	/* set sem0 = 0 (setlk and close fd done) */
	semu.val = 0;
	if (semctl(semid, 0, SETVAL, semu) == -1)
		err_exit("set sem0 0", errno);

	return 0;
}

int main(int argc, char **argv)
{
	int posix = 0;
	int lock_cmd = 1;
	int lock_rw = 1;
	int lock_start = 0;
	int lock_l = 10;
	int open_rw = 1;
	int clone_fs = 0;
	int use_dup = 0;
	int testrun = 0;
	int setlk_macro = F_OFD_SETLKW;
	int getlk_macro = F_OFD_GETLK;
	struct timespec ts;
	key_t semkey;
	unsigned short vals[2];
	union semun semu;
	struct semid_ds sem_ds;
	struct sembuf sop;
	int opt, ret, retry;

	while((opt = getopt(argc, argv, "sgrwo:l:PRWtFd")) != -1) {
		switch(opt) {
		case 's':
			lock_cmd = 1;
			break;
		case 'g':
			lock_cmd = 0;
			break;
		case 'r':
			lock_rw = 0;
			break;
		case 'w':
			lock_rw = 1;
			break;
		case 'o':
			lock_start = atoi(optarg);
			break;
		case 'l':
			lock_l = atoi(optarg);
			break;
		case 'P':
			posix = 1;
			break;
		case 'R':
			open_rw = 0;
			break;
		case 'W':
			open_rw = 1;
			break;
		case 't':
			testrun = 1;
			break;
		case 'F':
			clone_fs = 1;
			break;
		case 'd':
			use_dup = 1;
			break;
		default:
			usage(argv[0]);
			return -1;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		return -1;
	}

	struct flock flk = {
		.l_whence = SEEK_SET,
		.l_start = lock_start,
		.l_len = lock_l,
		.l_type = F_RDLCK,
	};

	if (posix == 0) {
		/* OFD lock requires l_pid to be zero */
		flk.l_pid = 0;
		setlk_macro = F_OFD_SETLKW;
		getlk_macro = F_OFD_GETLK;
	} else {
		setlk_macro = F_SETLKW;
		getlk_macro = F_GETLK;
	}

	if (lock_rw == 1)
		flk.l_type = F_WRLCK;
	else
		flk.l_type = F_RDLCK;

	if (open_rw == 0)
		fd = open(argv[optind], O_RDONLY);
	else
		fd = open(argv[optind], O_RDWR);
	if (fd == -1)
		err_exit("open", errno);

	/*
	 * In a testun, we do a fcntl getlk call and exit
	 * immediately no matter it succeeds or not.
	 */
	if (testrun == 1) {
		fcntl(fd, F_OFD_GETLK, &flk);
		err_exit("test_ofd_getlk", errno);
	}

	if((semkey = ftok(argv[optind], 255)) == -1)
		err_exit("ftok", errno);

	/* setlk, and always init the semaphore at setlk time */
	if (lock_cmd == 1) {
		/*
		 * Init the semaphore, with a key related to the testfile.
		 * getlk routine will wait untill this sem has been created and
		 * iniialized.
		 *
		 * We must make sure the semaphore set is newly created, rather
		 * then the one left from last run. In which case getlk will
		 * exit immediately and left setlk routine waiting forever.
		 * Also because newly created semaphore has zero sem_otime,
		 * which is used here to sync with getlk routine.
		 */
		retry = 0;
		do {
			semid = semget(semkey, 2, IPC_CREAT|IPC_EXCL);
			if (semid < 0 && errno == EEXIST) {
				/* remove sem set after one round of test */
				if (semctl(semid, 2, IPC_RMID, semu) == -1)
					err_exit("rmid 0", errno);
				retry++;
			} else if (semid < 0)
				err_exit("semget", errno);
			else
				retry = 10;
		} while (retry < 5);
		/* We can't create a new semaphore set in 5 tries */
		if (retry == 5)
			err_exit("semget", errno);

		/* Init both new sem to 1 */
		vals[0] = 1;
		vals[1] = 1;
		semu.array = vals;
		if (semctl(semid, 2, SETALL, semu) == -1)
			err_exit("init sem", errno);
		/* Inc both new sem to 2 */
		sop.sem_num = 0;
		sop.sem_op = 1;
		sop.sem_flg = 0;
		ts.tv_sec = 15;
		ts.tv_nsec = 0;
		if (semtimedop(semid, &sop, 1, &ts) == -1)
			err_exit("inc sem0 2", errno);
		sop.sem_num = 1;
		sop.sem_op = 1;
		sop.sem_flg = 0;
		ts.tv_sec = 15;
		ts.tv_nsec = 0;
		if (semtimedop(semid, &sop, 1, &ts) == -1)
			err_exit("inc sem1 2", errno);

		/*
		 * Wait initialization complete. semctl(2) only update
		 * sem_ctime, semop(2) will update sem_otime.
		 */
		ret = -1;
		do {
			memset(&sem_ds, 0, sizeof(sem_ds));
			semu.buf = &sem_ds;
			ret = semctl(semid, 0, IPC_STAT, semu);
		} while (!(ret == 0 && sem_ds.sem_otime != 0));

		/* place the lock */
		if (fcntl(fd, setlk_macro, &flk) < 0)
			err_exit("setlkw", errno);

		if (use_dup == 1) {
			/* dup fd and close the newfd */
			int dfd = dup(fd);
			if (dfd == -1)
				err_exit("dup", errno);
			close(dfd);
			/* set sem0 = 0 (setlk and close fd done) */
			semu.val = 0;
			if (semctl(semid, 0, SETVAL, semu) == -1)
				err_exit("set sem0 0", errno);
		} else {
			/*
			 * clone a child to close the fd then tell getlk to go;
			 * in parent we keep holding the lock till getlk done.
			 */
			pid_t child_pid = 0;
			if (clone_fs)
				child_pid = clone(child_fn, child_stack+STACK_SIZE,
					CLONE_FILES|CLONE_SYSVSEM|SIGCHLD, &fd);
			else
				child_pid = clone(child_fn, child_stack+STACK_SIZE,
					CLONE_SYSVSEM|SIGCHLD, &fd);
			if (child_pid == -1)
				err_exit("clone", errno);
			/* wait child done */
			waitpid(child_pid, NULL, 0);
		}

		/* "hold" lock and wait sem1 == 0 (getlk done) */
		sop.sem_num = 1;
		sop.sem_op = 0;
		sop.sem_flg = 0;
		ts.tv_sec = 15;
		ts.tv_nsec = 0;
		if (semtimedop(semid, &sop, 1, &ts) == -1)
			err_exit("wait sem1 0", errno);

		/* remove sem set after one round of test */
		if (semctl(semid, 2, IPC_RMID, semu) == -1)
			err_exit("rmid", errno);
		close(fd);
		exit(0);
	}

	/* getlck */
	if (lock_cmd == 0) {
		/* wait sem created and initialized */
		do {
			semid = semget(semkey, 2, 0);
			if (semid != -1)
				break;
			if (errno == ENOENT)
				continue;
			else
				err_exit("getlk_semget", errno);
		} while (1);
		do {
			memset(&sem_ds, 0, sizeof(sem_ds));
			semu.buf = &sem_ds;
			ret = semctl(semid, 0, IPC_STAT, semu);
		} while (!(ret == 0 && sem_ds.sem_otime != 0));

		/* wait sem0 == 0 (setlk and close fd done) */
		sop.sem_num = 0;
		sop.sem_op = 0;
		sop.sem_flg = 0;
		ts.tv_sec = 15;
		ts.tv_nsec = 0;
		if (semtimedop(semid, &sop, 1, &ts) == -1)
			err_exit("wait sem0 0", errno);

		if (fcntl(fd, getlk_macro, &flk) < 0)
			err_exit("getlk", errno);

		/* set sem1 = 0 (getlk done) */
		semu.val = 0;
		if (semctl(semid, 1, SETVAL, semu) == -1)
			err_exit("set sem1 0", errno);

		/* check result */
		switch (flk.l_type) {
		case F_UNLCK:
			printf("lock could be placed\n");
			break;
		case F_RDLCK:
			printf("get rdlck\n");
			break;
		case F_WRLCK:
			printf("get wrlck\n");
			break;
		default:
			printf("unknown lock type\n");
			break;
		}
		close(fd);
	}
	return 0;
}
