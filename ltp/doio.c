/*
 * Copyright (c) 2000 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
/*
 * doio -	a general purpose io initiator with system call and
 *		write logging.  See doio.h for the structure which defines
 *		what doio requests should look like.
 *
 * programming
 * notes:
 * -----------
 *	messages should generally be printed using doio_fprintf().
 *
 */

#include "global.h"

#include <stdarg.h>
#include <sys/uio.h>	/* for struct iovec (readv)*/
#include <sys/mman.h>	/* for mmap(2) */
#include <sys/ipc.h>	/* for i/o buffer in shared memory */
#include <sys/shm.h>	/* for i/o buffer in shared memory */
#include <sys/wait.h>
#include <sys/time.h>	/* for delays */
#include <ctype.h>

struct io_req;
int do_xfsctl(struct io_req *);

#include "doio.h"
#include "pattern.h"
#include "write_log.h"
#include "random_range.h"
#include "string_to_tokens.h"

#ifndef O_SSD
#define O_SSD 0                /* so code compiles on a CRAY2 */
#endif

#define UINT64_T unsigned long long

#ifndef O_PARALLEL
#define O_PARALLEL 0	/* so O_PARALLEL may be used in expressions */
#endif

#define PPID_CHECK_INTERVAL 5		/* check ppid every <-- iterations */
#define	MAX_AIO		256		/* maximum number of async I/O ops */
#define	MPP_BUMP	0


#define	SYSERR strerror(errno)

/*
 * getopt() string of supported cmdline arguments.
 */

#define OPTS	"aC:d:ehm:n:kr:w:vU:V:M:N:"

#define DEF_RELEASE_INTERVAL	0

/*
 * Flags set in parse_cmdline() to indicate which options were selected
 * on the cmdline.
 */

int 	a_opt = 0;  	    /* abort on data compare errors 	*/
int	e_opt = 0;	    /* exec() after fork()'ing	        */
int	C_opt = 0;	    /* Data Check Type			*/
int	d_opt = 0;	    /* delay between operations		*/
int 	k_opt = 0;  	    /* lock file regions during writes	*/
int	m_opt = 0;	    /* generate periodic messages	*/
int 	n_opt = 0;  	    /* nprocs	    	    	    	*/
int 	r_opt = 0;  	    /* resource release interval    	*/
int 	w_opt = 0;  	    /* file write log file  	    	*/
int 	v_opt = 0;  	    /* verify writes if set 	    	*/
int 	U_opt = 0;  	    /* upanic() on varios conditions	*/
int	V_opt = 0;	    /* over-ride default validation fd type */
int	M_opt = 0;	    /* data buffer allocation types     */
char	TagName[40];	    /* name of this doio (see Monster)  */


/*
 * Misc globals initialized in parse_cmdline()
 */

char	*Prog = NULL;	    /* set up in parse_cmdline()		*/
int 	Upanic_Conditions;  /* set by args to -U    	    		*/
int 	Release_Interval;   /* arg to -r    	    	    		*/
int 	Nprocs;	    	    /* arg to -n    	    	    		*/
char	*Write_Log; 	    /* arg to -w    	    	    		*/
char	*Infile;    	    /* input file (defaults to stdin)		*/
int	*Children;	    /* pids of child procs			*/
int	Nchildren = 0;
int	Nsiblings = 0;	    /* tfork'ed siblings			*/
int	Execd = 0;
int	Message_Interval = 0;
int	Npes = 0;	    /* non-zero if built as an mpp multi-pe app */
int	Vpe = -1;	    /* Virtual pe number if Npes >= 0           */
int	Reqno = 1;	    /* request # - used in some error messages  */
int	Reqskipcnt = 0;	    /* count of I/O requests that are skipped   */
int	Validation_Flags;
char	*(*Data_Check)();   /* function to call for data checking       */
int	(*Data_Fill)();     /* function to call for data filling        */
int	Nmemalloc = 0;	    /* number of memory allocation strategies   */
int	delayop = 0;	    /* delay between operations - type of delay */
int	delaytime = 0;	    /* delay between operations - how long      */

struct wlog_file	Wlog;

int	active_mmap_rw = 0; /* Indicates that mmapped I/O is occurring. */
			    /* Used by sigbus_action() in the child doio. */
int	havesigint = 0;

#define SKIP_REQ	-2	/* skip I/O request */

#define	NMEMALLOC	32
#define	MEM_DATA	1	/* data space 				*/
#define	MEM_SHMEM	2	/* System V shared memory 		*/
#define	MEM_T3ESHMEM	3	/* T3E Shared Memory 			*/
#define	MEM_MMAP	4	/* mmap(2) 				*/

#define	MEMF_PRIVATE	0001
#define	MEMF_AUTORESRV	0002
#define	MEMF_LOCAL	0004
#define	MEMF_SHARED	0010

#define	MEMF_FIXADDR	0100
#define	MEMF_ADDR	0200
#define	MEMF_AUTOGROW	0400
#define	MEMF_FILE	01000	/* regular file -- unlink on close	*/
#define MEMF_MPIN	010000	/* use mpin(2) to lock pages in memory */

struct memalloc {
	int	memtype;
	int	flags;
	int	nblks;
	char	*name;
	void	*space;		/* memory address of allocated space */
	int	fd;		/* FD open for mmaping */
	int	size;
}	Memalloc[NMEMALLOC];

/*
 * Global file descriptors
 */

int 	Wfd_Append; 	    /* for appending to the write-log	    */
int 	Wfd_Random; 	    /* for overlaying write-log entries	    */

/*
 * Structure for maintaining open file test descriptors.  Used by
 * alloc_fd().
 */

struct fd_cache {
	char    c_file[MAX_FNAME_LENGTH+1];
	int	c_oflags;
	int	c_fd;
	long    c_rtc;
	int	c_memalign;	/* from xfsctl(XFS_IOC_DIOINFO) */
	int	c_miniosz;
	int	c_maxiosz;
	void	*c_memaddr;	/* mmapped address */
	int	c_memlen;	/* length of above region */
};

#define FD_ALLOC_INCR	32      /* allocate this many fd_map structs	*/
				/* at a time */

/*
 * Globals for tracking Sds and Core usage
 */

char	*Memptr;		/* ptr to core buffer space	    	*/
int 	Memsize;		/* # bytes pointed to by Memptr 	*/
				/* maintained by alloc_mem()    	*/

int 	Sdsptr;			/* sds offset (always 0)	    	*/
int 	Sdssize;		/* # bytes of allocated sds space	*/
				/* Maintained by alloc_sds()    	*/
char	Host[16];
char	Pattern[128];
int	Pattern_Length;

/*
 * Signal handlers, and related globals
 */

void	sigint_handler();	/* Catch SIGINT in parent doio, propagate
				 * to children, does not die. */

void	die_handler();		/* Bad sig in child doios, exit 1. */
void	cleanup_handler();	/* Normal kill, exit 0. */

void	sigbus_handler();	/* Handle sigbus--check active_mmap_rw to
				   decide if this should be a normal exit. */

void	cb_handler();		/* Posix aio callback handler. */
void	noop_handler();		/* Delayop alarm, does nothing. */
char	*hms(time_t  t);
char	*format_rw();
char	*format_sds();
char	*format_listio();
char	*check_file(char *file, int offset, int length, char *pattern,
		    int pattern_length, int patshift, int fsa);
int	doio_fprintf(FILE *stream, char *format, ...);
void	doio_upanic(int mask);
void	doio();
void	help(FILE *stream);
void	doio_delay();
int     alloc_fd( char *, int );
int     alloc_mem( int );
int     do_read( struct io_req * );
int     do_write( struct io_req * );
int     do_rw( struct io_req * );
int     do_sync( struct io_req * );
int     usage( FILE * );
int     aio_unregister( int );
int     parse_cmdline( int, char **, char * );
int     lock_file_region( char *, int, int, int, int );
struct	fd_cache *alloc_fdcache(char *, int);
int     aio_register( int, int, int );
#ifndef linux
int aio_wait(int);
#endif

/*
 * Upanic conditions, and a map from symbolics to values
 */

#define U_CORRUPTION	0001	    /* upanic on data corruption    */
#define U_IOSW	    	0002	    /* upanic on bad iosw   	    */
#define U_RVAL	    	0004	    /* upanic on bad rval   	    */

#define U_ALL	    	(U_CORRUPTION | U_IOSW | U_RVAL)

/*
 * Name-To-Value map
 * Used to map cmdline arguments to values
 */
struct smap {
	char    *string;
	int	value;
};

struct smap Upanic_Args[] = {
	{ "corruption",	U_CORRUPTION	},
	{ "iosw",	U_IOSW		},
	{ "rval",	U_RVAL  	},
	{ "all",	U_ALL   	},
	{ NULL,         0               }
};

struct aio_info {
	int			busy;
	int			id;
	int			fd;
	int			strategy;
	volatile int		done;
	int			sig;
	int			signalled;
	struct sigaction	osa;
};

struct aio_info	Aio_Info[MAX_AIO];

struct aio_info	*aio_slot();
int     aio_done( struct aio_info * );

/* -C data-fill/check type */
#define	C_DEFAULT	1
struct smap checkmap[] = {
	{ "default",	C_DEFAULT },
	{ NULL,		0 },
};

/* -d option delay types */
#define	DELAY_SELECT	1
#define	DELAY_SLEEP	2
#define	DELAY_SGINAP	3
#define	DELAY_ALARM	4
#define	DELAY_ITIMER	5	/* POSIX timer				*/

struct smap delaymap[] = {
	{ "select",	DELAY_SELECT },
	{ "sleep",	DELAY_SLEEP },
	{ "alarm",	DELAY_ALARM },
	{ NULL,	0 },
};

/******
*
* strerror() does similar actions.

char *
syserrno(int err)
{
    static char sys_errno[10];
    sprintf(sys_errno, "%d", errno);
    return(sys_errno);
}

******/

int
main(argc, argv)
int 	argc;
char	**argv;
{
	int	    	    	i, pid, stat, ex_stat;
	struct sigaction	sa;
	sigset_t		block_mask, old_mask;
	umask(0);		/* force new file modes to known values */

	TagName[0] = '\0';
	parse_cmdline(argc, argv, OPTS);

	random_range_seed(getpid());       /* initialize random number generator */

	/*	
	 * If this is a re-exec of doio, jump directly into the doio function.
	 */

	if (Execd) {
		doio();
		exit(E_SETUP);
	}

	/*
	 * Stop on all but a few signals...
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigint_handler;
	sa.sa_flags = SA_RESETHAND;	/* sigint is ignored after the */
					/* first time */
	for (i = 1; i <= NSIG; i++) {
		switch(i) {
#ifdef SIGRECOVERY
		case SIGRECOVERY:
			break;
#endif
#ifdef SIGCKPT
		case SIGCKPT:
#endif
#ifdef SIGRESTART
		case SIGRESTART:
#endif
		case SIGTSTP:
		case SIGSTOP:
		case SIGCONT:
		case SIGCLD:
		case SIGBUS:
		case SIGSEGV:
		case SIGQUIT:
			break;
		default:
			sigaction(i, &sa, NULL);
		}
	}

	/*
	 * If we're logging write operations, make a dummy call to wlog_open
	 * to initialize the write history file.  This call must be done in
	 * the parent, to ensure that the history file exists and/or has
	 * been truncated before any children attempt to open it, as the doio
	 * children are not allowed to truncate the file.
	 */

	if (w_opt) {
		strcpy(Wlog.w_file, Write_Log);

		if (wlog_open(&Wlog, 1, 0666) < 0) {
			doio_fprintf(stderr,
				     "Could not create/truncate write log %s\n",
				     Write_Log);
			exit(2);
		}

		wlog_close(&Wlog);
	}

	/*
	 * Malloc space for the children pid array.  Initialize all entries
	 * to -1.
	 */

	Children = (int *)malloc(sizeof(int) * Nprocs);
	for (i = 0; i < Nprocs; i++) {
		Children[i] = -1;
	}

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &block_mask, &old_mask);

	/*
	 * Fork Nprocs.  This [parent] process is a watchdog, to notify the
	 * invoker of procs which exit abnormally, and to make sure that all
	 * child procs get cleaned up.  If the -e option was used, we will also
	 * re-exec.  This is mostly for unicos/mk on mpp's, to ensure that not
	 * all of the doio's don't end up in the same pe.
	 *
	 * Note - if Nprocs is 1, or this doio is a multi-pe app (Npes > 1),
	 * jump directly to doio().  multi-pe apps can't fork(), and there is
	 * no reason to fork() for 1 proc.
	 */

	if (Nprocs == 1 || Npes > 1) {
		doio();
		exit(0);
	} else {
		for (i = 0; i < Nprocs; i++) {
			if ((pid = fork()) == -1) {
				doio_fprintf(stderr,
					     "(parent) Could not fork %d children:  %s (%d)\n",
					     i+1, SYSERR, errno);
				exit(E_SETUP);
			}
			
			Children[Nchildren] = pid;
			Nchildren++;
			
			if (pid == 0) {
				if (e_opt) {
					char *exec_path;

					exec_path = argv[0];
					argv[0] = (char *)malloc(strlen(exec_path + 1));
					sprintf(argv[0], "-%s", exec_path);

					execvp(exec_path, argv);
					doio_fprintf(stderr,
						     "(parent) Could not execvp %s:  %s (%d)\n",
						     exec_path, SYSERR, errno);
					exit(E_SETUP);
				} else {
					doio();
					exit(E_SETUP);
				}
			}
		}

		/*
		 * Parent spins on wait(), until all children exit.
		 */
		
		ex_stat = E_NORMAL;
		
		while (Nprocs) {
			if ((pid = wait(&stat)) == -1) {
				if (errno == EINTR)
					continue;
			}
			
			for (i = 0; i < Nchildren; i++)
				if (Children[i] == pid)
					Children[i] = -1;
			
			Nprocs--;
			
			if (WIFEXITED(stat)) {
				switch (WEXITSTATUS(stat)) {
				case E_NORMAL:
					/* noop */
					break;

				case E_INTERNAL:
					doio_fprintf(stderr,
						     "(parent) pid %d exited because of an internal error\n",
						     pid);
					ex_stat |= E_INTERNAL;
					break;

				case E_SETUP:
					doio_fprintf(stderr,
						     "(parent) pid %d exited because of a setup error\n",
						     pid);
					ex_stat |= E_SETUP;
					break;

				case E_COMPARE:
					doio_fprintf(stderr,
						     "(parent) pid %d exited because of data compare errors\n",
						     pid);

					ex_stat |= E_COMPARE;

					if (a_opt)
						kill(0, SIGINT);

					break;

				case E_USAGE:
					doio_fprintf(stderr,
						     "(parent) pid %d exited because of a usage error\n",
						     pid);

					ex_stat |= E_USAGE;
					break;

				default:
					doio_fprintf(stderr,
						     "(parent) pid %d exited with unknown status %d\n",
						     pid, WEXITSTATUS(stat));
					ex_stat |= E_INTERNAL;
					break;
				}
			} else if (WIFSIGNALED(stat) && WTERMSIG(stat) != SIGINT) {
				doio_fprintf(stderr,
					     "(parent) pid %d terminated by signal %d\n",
					     pid, WTERMSIG(stat));
				
				ex_stat |= E_SIGNAL;
			}
			
			fflush(NULL);
		}
	}

	exit(ex_stat);

}  /* main */

/*
 * main doio function.  Each doio child starts here, and never returns.
 */

void
doio()
{
	int	    	    	rval, i, infd, nbytes;
	char			*cp;
	struct io_req   	ioreq;
	struct sigaction	sa, def_action, ignore_action, exit_action;
	struct sigaction	sigbus_action;

	Memsize = Sdssize = 0;

	/*
	 * Initialize the Pattern - write-type syscalls will replace Pattern[1]
	 * with the pattern passed in the request.  Make sure that
	 * strlen(Pattern) is not mod 16 so that out of order words will be
	 * detected.
	 */

	gethostname(Host, sizeof(Host));
	if ((cp = strchr(Host, '.')) != NULL)
		*cp = '\0';

	Pattern_Length = sprintf(Pattern, "-:%d:%s:%s*", (int)getpid(), Host, Prog);

	if (!(Pattern_Length % 16)) {
		Pattern_Length = sprintf(Pattern, "-:%d:%s:%s**",
					 (int)getpid(), Host, Prog);
	}

	/*
	 * Open a couple of descriptors for the write-log file.  One descriptor
	 * is for appending, one for random access.  Write logging is done for
	 * file corruption detection.  The program doio_check is capable of
	 * doing corruption detection based on a doio write-log.
	 */

	if (w_opt) {

		strcpy(Wlog.w_file, Write_Log);
	
		if (wlog_open(&Wlog, 0, 0666) == -1) {
			doio_fprintf(stderr,
				     "Could not open write log file (%s): wlog_open() failed\n",
				     Write_Log);
			exit(E_SETUP);
		}
	}

	/*
	 * Open the input stream - either a file or stdin
	 */

	if (Infile == NULL) {
		infd = 0;
	} else {
		if ((infd = open(Infile, O_RDWR)) == -1) {
			doio_fprintf(stderr,
				     "Could not open input file (%s):  %s (%d)\n",
				     Infile, SYSERR, errno);
			exit(E_SETUP);
		}
	}

	/*
	 * Define a set of signals that should never be masked.  Receipt of
	 * these signals generally indicates a programming error, and we want
	 * a corefile at the point of error.  We put SIGQUIT in this list so
	 * that ^\ will force a user core dump.
	 *
	 * Note:  the handler for these should be SIG_DFL, all of them 
	 * produce a corefile as the default action.
	 */

	ignore_action.sa_handler = SIG_IGN;
	ignore_action.sa_flags = 0;
	sigemptyset(&ignore_action.sa_mask);

	def_action.sa_handler = SIG_DFL;
	def_action.sa_flags = 0;
	sigemptyset(&def_action.sa_mask);

	exit_action.sa_handler = cleanup_handler;
	exit_action.sa_flags = 0;
	sigemptyset(&exit_action.sa_mask);

	sa.sa_handler = die_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	sigbus_action.sa_handler = sigbus_handler;
	sigbus_action.sa_flags = 0;
	sigemptyset(&sigbus_action.sa_mask);

	for (i = 1; i <= NSIG; i++) {
		switch(i) {
			/* Signals to terminate program on */
		case SIGINT:
			sigaction(i, &exit_action, NULL);
			break;

			/* This depends on active_mmap_rw */
		case SIGBUS:
			sigaction(i, &sigbus_action, NULL);
			break;

		    /* Signals to Ignore... */
		case SIGSTOP:
		case SIGCONT:
#ifdef SIGRECOVERY
		case SIGRECOVERY:
#endif
			sigaction(i, &ignore_action, NULL);
			break;

		    /* Signals to trap & report & die */
		/*case SIGTRAP:*/
		/*case SIGABRT:*/
#ifdef SIGERR	/* cray only signals */
		case SIGERR:
		case SIGBUFIO:
		case SIGINFO:
#endif
		/*case SIGFPE:*/
		case SIGURG:
		case SIGHUP:
		case SIGTERM:
		case SIGPIPE:
		case SIGIO:
		case SIGUSR1:
		case SIGUSR2:
			sigaction(i, &sa, NULL);
			break;


		    /* Default Action for all other signals */
		default:
			sigaction(i, &def_action, NULL);
			break;
		}
	}

	/*
	 * Main loop - each doio proc does this until the read returns eof (0).
	 * Call the appropriate io function based on the request type.
	 */

	while ((nbytes = read(infd, (char *)&ioreq, sizeof(ioreq)))) {

		/*
		 * Periodically check our ppid.  If it is 1, the child exits to
		 * help clean up in the case that the main doio process was
		 * killed.
		 */

		if (Reqno && ((Reqno % PPID_CHECK_INTERVAL) == 0)) {
			if (getppid() == 1) {
				doio_fprintf(stderr,
					     "Parent doio process has exited\n");
				alloc_mem(-1);
				exit(E_SETUP);
			}
		}

		if (nbytes == -1) {
			doio_fprintf(stderr,
				     "read of %d bytes from input failed:  %s (%d)\n",
				     sizeof(ioreq), SYSERR, errno);
			alloc_mem(-1);
			exit(E_SETUP);
		}

		if (nbytes != sizeof(ioreq)) {
			doio_fprintf(stderr,
				     "read wrong # bytes from input stream, expected %d, got %d\n",
				     sizeof(ioreq), nbytes);
			alloc_mem(-1);
			exit(E_SETUP);
		}

		if (ioreq.r_magic != DOIO_MAGIC) {
			doio_fprintf(stderr,
				     "got a bad magic # from input stream.  Expected 0%o, got 0%o\n",
				     DOIO_MAGIC, ioreq.r_magic);
			alloc_mem(-1);
			exit(E_SETUP);
		}

		/*
		 * If we're on a Release_Interval multiple, relase all ssd and
		 * core space, and close all fd's in Fd_Map[].
		 */

		if (Reqno && Release_Interval && ! (Reqno%Release_Interval)) {
			if (Memsize) {
#ifdef NOTDEF
				sbrk(-1 * Memsize);
#else
				alloc_mem(-1);
#endif
			}
			alloc_fd(NULL, 0);
		}

		switch (ioreq.r_type) {
		case READ:
		case READA:
			rval = do_read(&ioreq);
			break;

		case WRITE:
		case WRITEA:
			rval = do_write(&ioreq);
			break;

		case READV:
		case AREAD:
		case PREAD:
		case LREAD:
		case LREADA:
		case LSREAD:
		case LSREADA:
		case WRITEV:
		case AWRITE:
		case PWRITE:
		case MMAPR:
		case MMAPW:
		case LWRITE:
		case LWRITEA:
		case LSWRITE:
		case LSWRITEA:
		case LEREAD:
		case LEREADA:
		case LEWRITE:
		case LEWRITEA:
			rval = do_rw(&ioreq);
			break;
		case RESVSP:
		case UNRESVSP:
			rval = do_xfsctl(&ioreq);
			break;
		case FSYNC2:
		case FDATASYNC:
			rval = do_sync(&ioreq);
			break;
		default:
			doio_fprintf(stderr,
				     "Don't know how to handle io request type %d\n",
				     ioreq.r_type);
			alloc_mem(-1);
			exit(E_SETUP);
		}

		if (rval == SKIP_REQ){
			Reqskipcnt++;
		}
		else if (rval != 0) {
			alloc_mem(-1);
			doio_fprintf(stderr,
				     "doio(): operation %d returned != 0\n",
				     ioreq.r_type);
			exit(E_SETUP);
		}

		if (Message_Interval && Reqno % Message_Interval == 0) {
			doio_fprintf(stderr, "Info:  %d requests done (%d skipped) by this process\n", Reqno, Reqskipcnt);
		}

		Reqno++;

		if(delayop != 0)
			doio_delay();
	}

	/*
	 * Child exits normally
	 */
	alloc_mem(-1);
	exit(E_NORMAL);

}  /* doio */

void
doio_delay()
{
	struct timeval tv_delay;
	struct sigaction sa_al, sa_old;
	sigset_t al_mask;

	switch(delayop) {
	case DELAY_SELECT:
		tv_delay.tv_sec = delaytime / 1000000;
		tv_delay.tv_usec = delaytime % 1000000;
		/*doio_fprintf(stdout, "delay_select: %d %d\n", 
			    tv_delay.tv_sec, tv_delay.tv_usec);*/
		select(0, NULL, NULL, NULL, &tv_delay);
		break;

	case DELAY_SLEEP:
		sleep(delaytime);
		break;

	case DELAY_ALARM:
		sa_al.sa_flags = 0;
		sa_al.sa_handler = noop_handler;
		sigemptyset(&sa_al.sa_mask);
		sigaction(SIGALRM, &sa_al, &sa_old);
		sigemptyset(&al_mask);
		alarm(delaytime);
		sigsuspend(&al_mask);
		sigaction(SIGALRM, &sa_old, 0);
		break;
	}
}


/*
 * Format IO requests, returning a pointer to the formatted text.
 *
 * format_strat	- formats the async i/o completion strategy
 * format_rw	- formats a read[a]/write[a] request
 * format_sds	- formats a ssread/sswrite request
 * format_listio- formats a listio request
 *
 * ioreq is the doio io request structure.
 */

struct smap sysnames[] = {
	{ "READ",	READ		},
	{ "WRITE",	WRITE		},
	{ "READA",	READA		},
	{ "WRITEA",	WRITEA		},
	{ "SSREAD",	SSREAD		},
	{ "SSWRITE",	SSWRITE		},
	{ "LISTIO",  	LISTIO		},
	{ "LREAD",	LREAD		},
	{ "LREADA",	LREADA		},
	{ "LWRITE",	LWRITE		},
	{ "LWRITEA",	LWRITEA		},
	{ "LSREAD",	LSREAD		},
	{ "LSREADA",	LSREADA		},
	{ "LSWRITE",	LSWRITE		},
	{ "LSWRITEA",	LSWRITEA	},

	/* Irix System Calls */
	{ "PREAD",	PREAD		},
	{ "PWRITE",	PWRITE		},
	{ "AREAD",	AREAD		},
	{ "AWRITE",	AWRITE		},
	{ "LLREAD",	LLREAD		},
	{ "LLAREAD",	LLAREAD		},
	{ "LLWRITE",	LLWRITE		},
	{ "LLAWRITE",	LLAWRITE	},
	{ "RESVSP",	RESVSP		},
	{ "UNRESVSP",	UNRESVSP	},

	/* Irix and Linux System Calls */
	{ "READV",	READV		},
	{ "WRITEV",	WRITEV		},
	{ "MMAPR",	MMAPR		},
	{ "MMAPW",	MMAPW		},
	{ "FSYNC2",	FSYNC2		},
	{ "FDATASYNC",	FDATASYNC	},

	{ "unknown",	-1		},
};	

struct smap aionames[] = {
	{ "poll",	A_POLL		},
	{ "signal",	A_SIGNAL	},
	{ "recall",	A_RECALL	},
	{ "recalla",	A_RECALLA	},
	{ "recalls",	A_RECALLS	},
	{ "suspend",	A_SUSPEND	},
	{ "callback",	A_CALLBACK	},
	{ "synch",	0		},
	{ "unknown",	-1		},
};

char *
format_oflags(int oflags)
{
	char flags[255];


	flags[0]='\0';
	switch(oflags & 03) {
	case O_RDONLY:		strcat(flags,"O_RDONLY,");	break;
	case O_WRONLY:		strcat(flags,"O_WRONLY,");	break;
	case O_RDWR:		strcat(flags,"O_RDWR,");	break;
	default:		strcat(flags,"O_weird");	break;
	}

	if(oflags & O_EXCL)
		strcat(flags,"O_EXCL,");

	if(oflags & O_SYNC)
		strcat(flags,"O_SYNC,");

	if(oflags & O_DIRECT)
		strcat(flags,"O_DIRECT,");

	return(strdup(flags));
}

char *
format_strat(int strategy)
{
	char msg[64];
	char *aio_strat;

	switch (strategy) {
	case A_POLL:		aio_strat = "POLL";	break;
	case A_SIGNAL:		aio_strat = "SIGNAL";	break;
	case A_RECALL:		aio_strat = "RECALL";	break;
	case A_RECALLA:		aio_strat = "RECALLA";	break;
	case A_RECALLS:		aio_strat = "RECALLS";	break;
	case A_SUSPEND:		aio_strat = "SUSPEND";	break;
	case A_CALLBACK:	aio_strat = "CALLBACK";	break;
	case 0:			aio_strat = "<zero>";	break;
	default:
		sprintf(msg, "<error:%#o>", strategy);
		aio_strat = strdup(msg);
		break;
	}

	return(aio_strat);
}

char *
format_rw(
	struct	io_req	*ioreq,
	int		fd,
	void		*buffer,
	int		signo,
	char		*pattern,
	void		*iosw
	)
{
	static char		*errbuf=NULL;
	char			*aio_strat, *cp;
	struct read_req		*readp = &ioreq->r_data.read;
	struct write_req	*writep = &ioreq->r_data.write;
	struct read_req		*readap = &ioreq->r_data.read;
	struct write_req	*writeap = &ioreq->r_data.write;

	if(errbuf == NULL)
		errbuf = (char *)malloc(32768);

	cp = errbuf;
	cp += sprintf(cp, "Request number %d\n", Reqno);

	switch (ioreq->r_type) {
	case READ:
		cp += sprintf(cp, "syscall:  read(%d, %#lo, %d)\n",
			      fd, (unsigned long) buffer, readp->r_nbytes);
		cp += sprintf(cp, "          fd %d is file %s - open flags are %#o\n",
			      fd, readp->r_file, readp->r_oflags);
		cp += sprintf(cp, "          read done at file offset %d\n",
			      readp->r_offset);
		break;

	case WRITE:
		cp += sprintf(cp, "syscall:  write(%d, %#lo, %d)\n",
			      fd, (unsigned long) buffer, writep->r_nbytes);
		cp += sprintf(cp, "          fd %d is file %s - open flags are %#o\n",
			      fd, writep->r_file, writep->r_oflags);
		cp += sprintf(cp, "          write done at file offset %d - pattern is %s\n",
			      writep->r_offset, pattern);
		break;

	case READA:
		aio_strat = format_strat(readap->r_aio_strat);

		cp += sprintf(cp, "syscall:  reada(%d, %#lo, %d, %#lo, %d)\n",
			      fd, (unsigned long) buffer, readap->r_nbytes,
			      (unsigned long) iosw, signo);
		cp += sprintf(cp, "          fd %d is file %s - open flags are %#o\n",
			      fd, readap->r_file, readp->r_oflags);
		cp += sprintf(cp, "          reada done at file offset %d\n",
			      readap->r_offset);
		cp += sprintf(cp, "          async io completion strategy is %s\n",
			      aio_strat);
		break;

	case WRITEA:
		aio_strat = format_strat(writeap->r_aio_strat);

		cp += sprintf(cp, "syscall:  writea(%d, %#lo, %d, %#lo, %d)\n",
			      fd, (unsigned long) buffer, writeap->r_nbytes,
			      (unsigned long) iosw, signo);
		cp += sprintf(cp, "          fd %d is file %s - open flags are %#o\n",
			      fd, writeap->r_file, writeap->r_oflags);
		cp += sprintf(cp, "          writea done at file offset %d - pattern is %s\n",
			      writeap->r_offset, pattern);
		cp += sprintf(cp, "          async io completion strategy is %s\n",
			      aio_strat);
		break;

	}

	return errbuf;
}

/*
 * Perform the various sorts of disk reads
 */

int
do_read(req)
struct io_req	*req;
{
	int	    	    	fd, offset, nbytes, oflags, rval;
	char    	    	*addr, *file;
	struct fd_cache		*fdc;

	/*
	 * Initialize common fields - assumes r_oflags, r_file, r_offset, and
	 * r_nbytes are at the same offset in the read_req and reada_req
	 * structures.
	 */

	file = req->r_data.read.r_file;
	oflags = req->r_data.read.r_oflags;
	offset = req->r_data.read.r_offset;
	nbytes = req->r_data.read.r_nbytes;

	/*printf("read: %s, %#o, %d %d\n", file, oflags, offset, nbytes);*/

	/*
	 * Grab an open file descriptor
	 * Note: must be done before memory allocation so that the direct i/o
	 *	information is available in mem. allocate
	 */

	if ((fd = alloc_fd(file, oflags)) == -1)
		return -1;

	/*
	 * Allocate core or sds - based on the O_SSD flag
	 */

#ifndef wtob
#define wtob(x)	(x * sizeof(UINT64_T))
#endif

	/* get memory alignment for using DIRECT I/O */
	fdc = alloc_fdcache(file, oflags);

	if ((rval = alloc_mem(nbytes + wtob(1) * 2 + fdc->c_memalign)) < 0) {
		return rval;
	}

	addr = Memptr;


	if( (req->r_data.read.r_uflags & F_WORD_ALIGNED) ) {
		/*
		 * Force memory alignment for Direct I/O
		 */
		if( (oflags & O_DIRECT) && ((long)addr % fdc->c_memalign != 0) ) {
			addr += fdc->c_memalign - ((long)addr % fdc->c_memalign);
		}
	} else {
		addr += random_range(0, wtob(1) - 1, 1, NULL);
	}

	switch (req->r_type) {
	case READ:
	        /* move to the desired file position. */
		if (lseek(fd, offset, SEEK_SET) == -1) {
			doio_fprintf(stderr,
				     "lseek(%d, %d, SEEK_SET) failed:  %s (%d)\n",
				     fd, offset, SYSERR, errno);
			return -1;
		}

		if ((rval = read(fd, addr, nbytes)) == -1) {
			doio_fprintf(stderr,
				     "read() request failed:  %s (%d)\n%s\n",
				     SYSERR, errno,
				     format_rw(req, fd, addr, -1, NULL, NULL));
			doio_upanic(U_RVAL);
			return -1;
		} else if (rval != nbytes) {
			doio_fprintf(stderr,
				     "read() request returned wrong # of bytes - expected %d, got %d\n%s\n",
				     nbytes, rval, 
				     format_rw(req, fd, addr, -1, NULL, NULL));
			doio_upanic(U_RVAL);
			return -1;
		}
		break;
	}

	return 0;		/* if we get here, everything went ok */
}

/*
 * Perform the verious types of disk writes.
 */

int
do_write(req)
struct io_req	*req;
{
	static int		pid = -1;
	int	    	    	fd, nbytes, oflags;
	/* REFERENCED */
	int	    	    	logged_write, rval, got_lock;
	long    	    	offset, woffset = 0;
	char    	    	*addr, pattern, *file, *msg;
	struct wlog_rec		wrec;
	struct fd_cache		*fdc;

	/*
	 * Misc variable setup
	 */

	nbytes	= req->r_data.write.r_nbytes;
	offset	= req->r_data.write.r_offset;
	pattern	= req->r_data.write.r_pattern;
	file	= req->r_data.write.r_file;
	oflags	= req->r_data.write.r_oflags;

	/*printf("pwrite: %s, %#o, %d %d\n", file, oflags, offset, nbytes);*/

	/*
	 * Allocate core memory and possibly sds space.  Initialize the data
	 * to be written.
	 */

	Pattern[0] = pattern;


	/*
	 * Get a descriptor to do the io on
	 */

	if ((fd = alloc_fd(file, oflags)) == -1)
		return -1;

	/*printf("write: %d, %s, %#o, %d %d\n",
	       fd, file, oflags, offset, nbytes);*/

	/*
	 * Allocate SDS space for backdoor write if desired
	 */

	/* get memory alignment for using DIRECT I/O */
	fdc = alloc_fdcache(file, oflags);

	if ((rval = alloc_mem(nbytes + wtob(1) * 2 + fdc->c_memalign)) < 0) {
		return rval;
	}

	addr = Memptr;

	if( (req->r_data.write.r_uflags & F_WORD_ALIGNED) ) {
		/*
		 * Force memory alignment for Direct I/O
		 */
		if( (oflags & O_DIRECT) && ((long)addr % fdc->c_memalign != 0) ) {
			addr += fdc->c_memalign - ((long)addr % fdc->c_memalign);
		}
	} else {
		addr += random_range(0, wtob(1) - 1, 1, NULL);
	}

	(*Data_Fill)(Memptr, nbytes, Pattern, Pattern_Length, 0);
	if( addr != Memptr )
		memmove( addr, Memptr, nbytes);

	rval = -1;
	got_lock = 0;
	logged_write = 0;

	if (k_opt) {
		if (lock_file_region(file, fd, F_WRLCK, offset, nbytes) < 0) {
			alloc_mem(-1);
			exit(E_INTERNAL);
		}

		got_lock = 1;
	}

	/*
	 * Write a preliminary write-log entry.  This is done so that
	 * doio_check can do corruption detection across an interrupt/crash.
	 * Note that w_done is set to 0.  If doio_check sees this, it
	 * re-creates the file extents as if the write completed, but does not
	 * do any checking - see comments in doio_check for more details.
	 */

	if (w_opt) {
		if (pid == -1) {
			pid = getpid();
		}
		wrec.w_async = (req->r_type == WRITEA) ? 1 : 0;
		wrec.w_oflags = oflags;
		wrec.w_pid = pid;
		wrec.w_offset = offset;
		wrec.w_nbytes = nbytes;

		wrec.w_pathlen = strlen(file);
		memcpy(wrec.w_path, file, wrec.w_pathlen);
		wrec.w_hostlen = strlen(Host);
		memcpy(wrec.w_host, Host, wrec.w_hostlen);
		wrec.w_patternlen = Pattern_Length;
		memcpy(wrec.w_pattern, Pattern, wrec.w_patternlen);

		wrec.w_done = 0;

		if ((woffset = wlog_record_write(&Wlog, &wrec, -1)) == -1) {
			doio_fprintf(stderr,
				     "Could not append to write-log:  %s (%d)\n",
				     SYSERR, errno);
		} else {
			logged_write = 1;
		}
	}

	switch (req->r_type ) {
	case WRITE:
		/*
		 * sync write
		 */

		if (lseek(fd, offset, SEEK_SET) == -1) {
			doio_fprintf(stderr,
				     "lseek(%d, %d, SEEK_SET) failed:  %s (%d)\n",
				     fd, offset, SYSERR, errno);
			return -1;
		}

		rval = write(fd, addr, nbytes);

		if (rval == -1) {
			doio_fprintf(stderr,
				     "write() failed:  %s (%d)\n%s\n",
				     SYSERR, errno,
				     format_rw(req, fd, addr, -1, Pattern, NULL));
			doio_fprintf(stderr,
				     "write() failed:  %s\n\twrite(%d, %#o, %d)\n\toffset %d, nbytes%%miniou(%d)=%d, oflags=%#o memalign=%d, addr%%memalign=%d\n",
				     strerror(errno),
				     fd, addr, nbytes,
				     offset,
				     fdc->c_miniosz, nbytes%fdc->c_miniosz,
				     oflags, fdc->c_memalign, (long)addr%fdc->c_memalign);
			doio_upanic(U_RVAL);
		} else if (rval != nbytes) {
			doio_fprintf(stderr,
				     "write() returned wrong # bytes - expected %d, got %d\n%s\n",
				     nbytes, rval,
				     format_rw(req, fd, addr, -1, Pattern, NULL));
			doio_upanic(U_RVAL);
			rval = -1;
		}

		break;
	}

	/*
	 * Verify that the data was written correctly - check_file() returns
	 * a non-null pointer which contains an error message if there are
	 * problems.
	 */

	if (v_opt) {
		msg = check_file(file, offset, nbytes, Pattern, Pattern_Length,
				 0, oflags & O_PARALLEL);
		if (msg != NULL) {
		  	doio_fprintf(stderr, "%s%s\n",
				     msg,
				     format_rw(req, fd, addr, -1, Pattern, NULL)
				);
			doio_upanic(U_CORRUPTION);
			exit(E_COMPARE);

		}
	}

	/*
	 * General cleanup ...
	 *
	 * Write extent information to the write-log, so that doio_check can do
	 * corruption detection.  Note that w_done is set to 1, indicating that
	 * the write has been verified as complete.  We don't need to write the
	 * filename on the second logging.
	 */

	if (w_opt && logged_write) {
		wrec.w_done = 1;
		wlog_record_write(&Wlog, &wrec, woffset);
	}

	/*
	 * Unlock file region if necessary
	 */

	if (got_lock) {
		if (lock_file_region(file, fd, F_UNLCK, offset, nbytes) < 0) {
			alloc_mem(-1);
			exit(E_INTERNAL);
		}
	}

	return( (rval == -1) ? -1 : 0);
}


/*
 * Simple routine to lock/unlock a file using fcntl()
 */

int
lock_file_region(fname, fd, type, start, nbytes)
char	*fname;
int	fd;
int	type;
int	start;
int	nbytes;
{
	struct flock	flk;

	flk.l_type = type;
	flk.l_whence = 0;
	flk.l_start = start;
	flk.l_len = nbytes;

	if (fcntl(fd, F_SETLKW, &flk) < 0) {
		doio_fprintf(stderr,
			     "fcntl(%d, %d, %#o) failed for file %s, lock type %d, offset %d, length %d:  %s (%d), open flags: %#o\n",
			     fd, F_SETLKW, &flk, fname, type,
			     start, nbytes, SYSERR, errno,
			     fcntl(fd, F_GETFL, 0));
		return -1;
	}

	return 0;
}

int
do_listio(req)
struct io_req	*req;
{
	return -1;
}

/* ---------------------------------------------------------------------------
 *
 * A new paradigm of doing the r/w system call where there is a "stub"
 * function that builds the info for the system call, then does the system
 * call; this is called by code that is common to all system calls and does
 * the syscall return checking, async I/O wait, iosw check, etc.
 *
 * Flags:
 *	WRITE, ASYNC, SSD/SDS,
 *	FILE_LOCK, WRITE_LOG, VERIFY_DATA,
 */

struct	status {
	int	rval;		/* syscall return */
	int	err;		/* errno */
	int	*aioid;		/* list of async I/O structures */
};

struct syscall_info {
	char		*sy_name;
	int		sy_type;
	struct status	*(*sy_syscall)();
	int		(*sy_buffer)();
	char		*(*sy_format)();
	int		sy_flags;
	int		sy_bits;
};

#define	SY_WRITE		00001
#define	SY_ASYNC		00010
#define	SY_IOSW			00020
#define	SY_SDS			00100

char *
fmt_ioreq(struct io_req *ioreq, struct syscall_info *sy, int fd)
{
	static char		*errbuf=NULL;
	char			*cp;
	struct rw_req		*io;
	struct smap		*aname;

	if(errbuf == NULL)
		errbuf = (char *)malloc(32768);

	io = &ioreq->r_data.io;

	/*
	 * Look up async I/O completion strategy
	 */
	for(aname=aionames;
	    aname->value != -1 && aname->value != io->r_aio_strat;
	    aname++)
		;

	cp = errbuf;
	cp += sprintf(cp, "Request number %d\n", Reqno);

	cp += sprintf(cp, "          fd %d is file %s - open flags are %#o %s\n",
		      fd, io->r_file, io->r_oflags, format_oflags(io->r_oflags));

	if(sy->sy_flags & SY_WRITE) {
		cp += sprintf(cp, "          write done at file offset %d - pattern is %c (%#o)\n",
			      io->r_offset,
			      (io->r_pattern == '\0') ? '?' : io->r_pattern,
			      io->r_pattern);
	} else {
		cp += sprintf(cp, "          read done at file offset %d\n",
		      io->r_offset);
	}

	if(sy->sy_flags & SY_ASYNC) {
		cp += sprintf(cp, "          async io completion strategy is %s\n",
			      aname->string);
	}

	cp += sprintf(cp, "          number of requests is %d, strides per request is %d\n",
		      io->r_nent, io->r_nstrides);

	cp += sprintf(cp, "          i/o byte count = %d\n",
		      io->r_nbytes);

	cp += sprintf(cp, "          memory alignment is %s\n",
		      (io->r_uflags & F_WORD_ALIGNED) ? "aligned" : "unaligned");

	if(io->r_oflags & O_DIRECT) {
		char		*dio_env;
		struct dioattr	finfo;
		
		if(xfsctl(io->r_file, fd, XFS_IOC_DIOINFO, &finfo) == -1) {
			cp += sprintf(cp, "          Error %s (%d) getting direct I/O info\n",
				      strerror(errno), errno);
			finfo.d_mem = 1;
			finfo.d_miniosz = 1;
			finfo.d_maxiosz = 1;
		}

		dio_env = getenv("XFS_DIO_MIN");
		if (dio_env)
			finfo.d_mem = finfo.d_miniosz = atoi(dio_env);

		cp += sprintf(cp, "          DIRECT I/O: offset %% %d = %d length %% %d = %d\n",
			      finfo.d_miniosz,
			      io->r_offset % finfo.d_miniosz,
			      io->r_nbytes,
			      io->r_nbytes % finfo.d_miniosz);
		cp += sprintf(cp, "          mem alignment 0x%x xfer size: small: %d large: %d\n",
			      finfo.d_mem, finfo.d_miniosz, finfo.d_maxiosz);
	}
	return(errbuf);
}

struct status *
sy_pread(req, sysc, fd, addr)
struct io_req	*req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	int rc;
	struct status	*status;

	rc = pread(fd, addr, req->r_data.io.r_nbytes,
		   req->r_data.io.r_offset);

	status = (struct status *)malloc(sizeof(struct status));
	if( status == NULL ){
		doio_fprintf(stderr, "malloc failed, %s/%d\n",
			__FILE__, __LINE__);
		return NULL;
	}
	status->aioid = NULL;
	status->rval = rc;
	status->err = errno;

	return(status);
}

struct status *
sy_pwrite(req, sysc, fd, addr)
struct io_req	*req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	int rc;
	struct status	*status;

	rc = pwrite(fd, addr, req->r_data.io.r_nbytes,
		    req->r_data.io.r_offset);

	status = (struct status *)malloc(sizeof(struct status));
	if( status == NULL ){
		doio_fprintf(stderr, "malloc failed, %s/%d\n",
			__FILE__, __LINE__);
		return NULL;
	}
	status->aioid = NULL;
	status->rval = rc;
	status->err = errno;

	return(status);
}

char *
fmt_pread(struct io_req *req, struct syscall_info *sy, int fd, char *addr)
{
	static char	*errbuf = NULL;
	char		*cp;

	if(errbuf == NULL){
		errbuf = (char *)malloc(32768);
		if( errbuf == NULL ){
			doio_fprintf(stderr, "malloc failed, %s/%d\n",
				__FILE__, __LINE__);
			return NULL;
		}
	}

	cp = errbuf;
	cp += sprintf(cp, "syscall:  %s(%d, 0x%p, %d)\n",
		      sy->sy_name, fd, addr, req->r_data.io.r_nbytes);
	return(errbuf);
}

struct status *
sy_rwv(req, sysc, fd, addr, rw)
struct io_req	*req;
struct syscall_info *sysc;
int fd;
char *addr;
int rw;
{
	int rc;
	struct status	*status;
	struct iovec	iov[2];

	status = (struct status *)malloc(sizeof(struct status));
	if( status == NULL ){
		doio_fprintf(stderr, "malloc failed, %s/%d\n",
			__FILE__, __LINE__);
		return NULL;
	}
	status->aioid = NULL;

	/* move to the desired file position. */
	if ((rc=lseek(fd, req->r_data.io.r_offset, SEEK_SET)) == -1) {
		status->rval = rc;
		status->err = errno;
		return(status);
	}

	iov[0].iov_base = addr;
	iov[0].iov_len = req->r_data.io.r_nbytes;

	if(rw)
		rc = writev(fd, iov, 1);
	else
		rc = readv(fd, iov, 1);
	status->aioid = NULL;
	status->rval = rc;
	status->err = errno;
	return(status);
}

struct status *
sy_readv(req, sysc, fd, addr)
struct io_req	*req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	return sy_rwv(req, sysc, fd, addr, 0);
}

struct status *
sy_writev(req, sysc, fd, addr)
struct io_req	*req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	return sy_rwv(req, sysc, fd, addr, 1);
}

char *
fmt_readv(struct io_req *req, struct syscall_info *sy, int fd, char *addr)
{
	static char	errbuf[32768];
	char		*cp;

	cp = errbuf;
	cp += sprintf(cp, "syscall:  %s(%d, (iov on stack), 1)\n",
		      sy->sy_name, fd);
	return(errbuf);
}

struct status *
sy_mmrw(req, sysc, fd, addr, rw)
struct io_req *req;
struct syscall_info *sysc;
int fd;
char *addr;
int rw;
{
	/*
	 * mmap read/write
	 * This version is oriented towards mmaping the file to memory
	 * ONCE and keeping it mapped.
	 */
	struct status		*status;
	void			*mrc, *memaddr;
	struct fd_cache		*fdc;
	struct stat		sbuf;

	status = (struct status *)malloc(sizeof(struct status));
	if( status == NULL ){
		doio_fprintf(stderr, "malloc failed, %s/%d\n",
			__FILE__, __LINE__);
		return NULL;
	}
	status->aioid = NULL;
	status->rval = -1;

	fdc = alloc_fdcache(req->r_data.io.r_file, req->r_data.io.r_oflags);

	if( fdc->c_memaddr == NULL ) {
		if( fstat(fd, &sbuf) < 0 ){
			doio_fprintf(stderr, "fstat failed, errno=%d\n",
				     errno);
			status->err = errno;
			return(status);
		}

		fdc->c_memlen = (int)sbuf.st_size;
		mrc = mmap(NULL, (int)sbuf.st_size,
		     rw ? PROT_WRITE|PROT_READ : PROT_READ,
		     MAP_SHARED, fd, 0);

		if( mrc == MAP_FAILED ) {
			doio_fprintf(stderr, "mmap() failed - 0x%lx %d\n",
				mrc, errno);
			status->err = errno;
			return(status);
		}

		fdc->c_memaddr = mrc;
	}

	memaddr = (void *)((char *)fdc->c_memaddr + req->r_data.io.r_offset);

	active_mmap_rw = 1;
	if(rw)
		memcpy(memaddr, addr, req->r_data.io.r_nbytes);
	else
		memcpy(addr, memaddr, req->r_data.io.r_nbytes);
	active_mmap_rw = 0;

	status->rval = req->r_data.io.r_nbytes;
	status->err = 0;
	return(status);
}

struct status *
sy_mmread(req, sysc, fd, addr)
struct io_req *req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	return sy_mmrw(req, sysc, fd, addr, 0);
}

struct status *
sy_mmwrite(req, sysc, fd, addr)
struct io_req *req;
struct syscall_info *sysc;
int fd;
char *addr;
{
	return sy_mmrw(req, sysc, fd, addr, 1);
}

char *
fmt_mmrw(struct io_req *req, struct syscall_info *sy, int fd, char *addr)
{
	static char	errbuf[32768];
	char		*cp;
	struct fd_cache	*fdc;
	void		*memaddr;

	fdc = alloc_fdcache(req->r_data.io.r_file, req->r_data.io.r_oflags);

	cp = errbuf;
	cp += sprintf(cp, "syscall:  %s(NULL, %d, %s, MAP_SHARED, %d, 0)\n",
		      sy->sy_name,
		      fdc->c_memlen,
		      (sy->sy_flags & SY_WRITE) ? "PROT_WRITE" : "PROT_READ",
		      fd);

	cp += sprintf(cp, "\tfile is mmaped to: 0x%lx\n",
		      (unsigned long) fdc->c_memaddr);

	memaddr = (void *)((char *)fdc->c_memaddr + req->r_data.io.r_offset);

	cp += sprintf(cp, "\tfile-mem=0x%lx, length=%d, buffer=0x%lx\n",
		      (unsigned long) memaddr, req->r_data.io.r_nbytes,
		      (unsigned long) addr);
		      
	return(errbuf);
}

struct syscall_info syscalls[] = {
	{ "pread",			PREAD,
	  sy_pread,	NULL,		fmt_pread,
	  0
	},
	{ "pwrite",			PWRITE,
	  sy_pwrite,	NULL,		fmt_pread,
	  SY_WRITE
	},

	{ "readv",			READV,
	  sy_readv,	NULL,		fmt_readv,
	  0
	},
	{ "writev",			WRITEV,
	  sy_writev,	NULL,		fmt_readv,
	  SY_WRITE
	},
	{ "mmap-read",			MMAPR,
	  sy_mmread,	NULL,		fmt_mmrw,
	  0
	},
	{ "mmap-write",			MMAPW,
	  sy_mmwrite,	NULL,		fmt_mmrw,
	  SY_WRITE
	},

	{ NULL,				0,
	  0,		0,		0,
	  0
	},
};

int
do_rw(req)
	struct io_req	*req;
{
	static int		pid = -1;
	int	    		fd, offset, nbytes, nstrides, nents, oflags;
	int			rval, mem_needed, i;
	int	    		logged_write, got_lock, woffset = 0, pattern;
	int			min_byte, max_byte;
	char    		*addr, *file, *msg;
	struct status		*s;
	struct wlog_rec		wrec;
	struct syscall_info	*sy;
	struct fd_cache		*fdc;

	/*
	 * Initialize common fields - assumes r_oflags, r_file, r_offset, and
	 * r_nbytes are at the same offset in the read_req and reada_req
	 * structures.
	 */
	file	= req->r_data.io.r_file;
	oflags	= req->r_data.io.r_oflags;
	offset	= req->r_data.io.r_offset;
	nbytes	= req->r_data.io.r_nbytes;
	nstrides= req->r_data.io.r_nstrides;
	nents   = req->r_data.io.r_nent;
	pattern	= req->r_data.io.r_pattern;

	if( nents >= MAX_AIO ) {
		doio_fprintf(stderr, "do_rw: too many list requests, %d.  Maximum is %d\n",
			     nents, MAX_AIO);
		return(-1);
	}

	/*
	 * look up system call info
	 */
	for(sy=syscalls; sy->sy_name != NULL && sy->sy_type != req->r_type; sy++)
		;

	if(sy->sy_name == NULL) {
		doio_fprintf(stderr, "do_rw: unknown r_type %d.\n",
			     req->r_type);
		return(-1);
	}

	/*
	 * Get an open file descriptor
	 * Note: must be done before memory allocation so that the direct i/o
	 *	information is available in mem. allocate
	 */

	if ((fd = alloc_fd(file, oflags)) == -1)
		return -1;

	/*
	 * Allocate core memory and possibly sds space.  Initialize the
	 * data to be written.  Make sure we get enough, based on the
	 * memstride.
	 *
	 * need:
	 *	1 extra word for possible partial-word address "bump"
	 *	1 extra word for dynamic pattern overrun
	 *	MPP_BUMP extra words for T3E non-hw-aligned memory address.
	 */

	if( sy->sy_buffer != NULL ) {
		mem_needed = (*sy->sy_buffer)(req, 0, 0, NULL, NULL);
	} else {
		mem_needed = nbytes;
	}

	/* get memory alignment for using DIRECT I/O */
	fdc = alloc_fdcache(file, oflags);

	if ((rval = alloc_mem(mem_needed + wtob(1) * 2 + fdc->c_memalign)) < 0) {
		return rval;
	}

	Pattern[0] = pattern;

	/*
	 * Allocate SDS space for backdoor write if desired
	 */

	if (oflags & O_SSD) {
		doio_fprintf(stderr, "Invalid O_SSD flag was generated for non-Cray system\n");
		fflush(stderr);
		return -1;
	} else {
		addr = Memptr;

		/*
		 * if io is not raw, bump the offset by a random amount
		 * to generate non-word-aligned io.
		 *
		 * On MPP systems, raw I/O must start on an 0x80 byte boundary.
		 * For non-aligned I/O, bump the address from 1 to 8 words.
		 */

		if (! (req->r_data.io.r_uflags & F_WORD_ALIGNED)) {
			addr += random_range(0, wtob(1) - 1, 1, NULL);
		}

		/*
		 * Force memory alignment for Direct I/O
		 */
		if( (oflags & O_DIRECT) && ((long)addr % fdc->c_memalign != 0) ) {
			addr += fdc->c_memalign - ((long)addr % fdc->c_memalign);
		}

		/*
		 * FILL must be done on a word-aligned buffer.
		 * Call the fill function with Memptr which is aligned,
		 * then memmove it to the right place.
		 */
		if (sy->sy_flags & SY_WRITE) {
			(*Data_Fill)(Memptr, mem_needed, Pattern, Pattern_Length, 0);
			if( addr != Memptr )
			    memmove( addr, Memptr, mem_needed);
		}
	}

	rval = 0;
	got_lock = 0;
	logged_write = 0;

	/*
	 * Lock data if this is a write and locking option is set
	 */
	if (sy->sy_flags & SY_WRITE && k_opt) {
		if( sy->sy_buffer != NULL ) {
			(*sy->sy_buffer)(req, offset, 0, &min_byte, &max_byte);
		} else {
			min_byte = offset;
			max_byte = offset + (nbytes * nstrides * nents);
		}

		if (lock_file_region(file, fd, F_WRLCK,
				     min_byte, (max_byte-min_byte+1)) < 0) {
		    doio_fprintf(stderr, 
				"file lock failed:\n%s\n",
				fmt_ioreq(req, sy, fd));
		    doio_fprintf(stderr, 
				"          buffer(req, %d, 0, 0x%x, 0x%x)\n",
				offset, min_byte, max_byte);
		    alloc_mem(-1);
		    exit(E_INTERNAL);
		}

		got_lock = 1;
	}

	/*
	 * Write a preliminary write-log entry.  This is done so that
	 * doio_check can do corruption detection across an interrupt/crash.
	 * Note that w_done is set to 0.  If doio_check sees this, it
	 * re-creates the file extents as if the write completed, but does not
	 * do any checking - see comments in doio_check for more details.
	 */

	if (sy->sy_flags & SY_WRITE && w_opt) {
		if (pid == -1) {
			pid = getpid();
		}

		wrec.w_async = (sy->sy_flags & SY_ASYNC) ? 1 : 0;
		wrec.w_oflags = oflags;
		wrec.w_pid = pid;
		wrec.w_offset = offset;
		wrec.w_nbytes = nbytes;	/* mem_needed -- total length */

		wrec.w_pathlen = strlen(file);
		memcpy(wrec.w_path, file, wrec.w_pathlen);
		wrec.w_hostlen = strlen(Host);
		memcpy(wrec.w_host, Host, wrec.w_hostlen);
		wrec.w_patternlen = Pattern_Length;
		memcpy(wrec.w_pattern, Pattern, wrec.w_patternlen);

		wrec.w_done = 0;

		if ((woffset = wlog_record_write(&Wlog, &wrec, -1)) == -1) {
			doio_fprintf(stderr,
				     "Could not append to write-log:  %s (%d)\n",
				     SYSERR, errno);
		} else {
			logged_write = 1;
		}
	}

	s = (*sy->sy_syscall)(req, sy, fd, addr);

	if( s->rval == -1 ) {
		doio_fprintf(stderr,
			     "%s() request failed:  %s (%d)\n%s\n%s\n",
			     sy->sy_name, SYSERR, errno,
			     fmt_ioreq(req, sy, fd),
			     (*sy->sy_format)(req, sy, fd, addr));

		doio_upanic(U_RVAL);

		for(i=0; i < nents; i++) {
			if(s->aioid == NULL)
				break;
			aio_unregister(s->aioid[i]);
		}
		rval = -1;
	} else {
		/*
		 * If the syscall was async, wait for I/O to complete
		 */
#ifndef linux
		if(sy->sy_flags & SY_ASYNC) {
			for(i=0; i < nents; i++) {
				aio_wait(s->aioid[i]);
			}
		}
#endif

		/*
		 * Check the syscall how-much-data-written return.  Look
		 * for this in either the return value or the 'iosw'
		 * structure.
		 */

		if( !(sy->sy_flags & SY_IOSW) ) {

			if(s->rval != mem_needed) {
				doio_fprintf(stderr,
					     "%s() request returned wrong # of bytes - expected %d, got %d\n%s\n%s\n",
					     sy->sy_name, nbytes, s->rval,
					     fmt_ioreq(req, sy, fd),
					     (*sy->sy_format)(req, sy, fd, addr));
				rval = -1;
				doio_upanic(U_RVAL);
			}
		}
	}


	/*
	 * Verify that the data was written correctly - check_file() returns
	 * a non-null pointer which contains an error message if there are
	 * problems.
	 */

	if ( rval == 0 && sy->sy_flags & SY_WRITE && v_opt) {
		msg = check_file(file, offset, nbytes*nstrides*nents,
				 Pattern, Pattern_Length, 0,
				 oflags & O_PARALLEL);
		if (msg != NULL) {
			doio_fprintf(stderr, "%s\n%s\n%s\n",
				     msg,
				     fmt_ioreq(req, sy, fd),
				     (*sy->sy_format)(req, sy, fd, addr));
			doio_upanic(U_CORRUPTION);
			exit(E_COMPARE);
		}
	}

	/*
	 * General cleanup ...
	 *
	 * Write extent information to the write-log, so that doio_check can do
	 * corruption detection.  Note that w_done is set to 1, indicating that
	 * the write has been verified as complete.  We don't need to write the
	 * filename on the second logging.
	 */

	if (w_opt && logged_write) {
		wrec.w_done = 1;
		wlog_record_write(&Wlog, &wrec, woffset);
	}

	/*
	 * Unlock file region if necessary
	 */

	if (got_lock) {
		if (lock_file_region(file, fd, F_UNLCK,
				     min_byte, (max_byte-min_byte+1)) < 0) {
			alloc_mem(-1);
			exit(E_INTERNAL);
		}
	}

	if(s->aioid != NULL)
		free(s->aioid);
	free(s);
	return (rval == -1) ? -1 : 0;
}


/*
 * xfsctl-based requests
 *   - XFS_IOC_RESVSP
 *   - XFS_IOC_UNRESVSP
 */
int
do_xfsctl(req)
	struct io_req	*req;
{
	int	    		fd, oflags, offset, nbytes;
	int			rval, op = 0;
	int	    		got_lock;
	int			min_byte = 0, max_byte = 0;
	char    		*file, *msg = NULL;
	struct xfs_flock64    	flk;

	/*
	 * Initialize common fields - assumes r_oflags, r_file, r_offset, and
	 * r_nbytes are at the same offset in the read_req and reada_req
	 * structures.
	 */
	file	= req->r_data.io.r_file;
	oflags	= req->r_data.io.r_oflags;
	offset	= req->r_data.io.r_offset;
	nbytes	= req->r_data.io.r_nbytes;

	flk.l_type=0;
	flk.l_whence=SEEK_SET;
	flk.l_start=offset;
	flk.l_len=nbytes;

	/*
	 * Get an open file descriptor
	 */

	if ((fd = alloc_fd(file, oflags)) == -1)
		return -1;

	rval = 0;
	got_lock = 0;

	/*
	 * Lock data if this is locking option is set
	 */
	if (k_opt) {
		min_byte = offset;
		max_byte = offset + nbytes;

		if (lock_file_region(file, fd, F_WRLCK,
				     min_byte, (nbytes+1)) < 0) {
		    doio_fprintf(stderr, 
				"file lock failed:\n");
		    doio_fprintf(stderr, 
				"          buffer(req, %d, 0, 0x%x, 0x%x)\n",
				offset, min_byte, max_byte);
		    alloc_mem(-1);
		    exit(E_INTERNAL);
		}

		got_lock = 1;
	}

	switch (req->r_type) {
	case RESVSP:	op=XFS_IOC_RESVSP;	msg="resvsp";	break;
	case UNRESVSP:	op=XFS_IOC_UNRESVSP;	msg="unresvsp";	break;
	}

	rval = xfsctl(file, fd, op, &flk);

	if( rval == -1 ) {
		doio_fprintf(stderr,
"xfsctl %s request failed: %s (%d)\n\txfsctl(%d, %s %d, {%d %lld ==> %lld}\n",
			     msg, SYSERR, errno,
			     fd, msg, op, flk.l_whence, 
			     (long long)flk.l_start, 
			     (long long)flk.l_len);

		doio_upanic(U_RVAL);
		rval = -1;
	}

	/*
	 * Unlock file region if necessary
	 */

	if (got_lock) {
		if (lock_file_region(file, fd, F_UNLCK,
				     min_byte, (max_byte-min_byte+1)) < 0) {
			alloc_mem(-1);
			exit(E_INTERNAL);
		}
	}

	return (rval == -1) ? -1 : 0;
}

/*
 *  fsync(2) and fdatasync(2)
 */
int
do_sync(req)
	struct io_req	*req;
{
	int	    		fd, oflags;
	int			rval;
	char    		*file;

	/*
	 * Initialize common fields - assumes r_oflags, r_file, r_offset, and
	 * r_nbytes are at the same offset in the read_req and reada_req
	 * structures.
	 */
	file	= req->r_data.io.r_file;
	oflags	= req->r_data.io.r_oflags;

	/*
	 * Get an open file descriptor
	 */

	if ((fd = alloc_fd(file, oflags)) == -1)
		return -1;

	rval = 0;
	switch(req->r_type) {
	case FSYNC2:
		rval = fsync(fd);
		break;
	case FDATASYNC:
		rval = fdatasync(fd);
		break;
	default:
		rval = -1;
	}
	return (rval == -1) ? -1 : 0;
}

int
doio_pat_fill(char *addr, int mem_needed, char *Pattern, int Pattern_Length,
	      int shift)
{
	return pattern_fill(addr, mem_needed, Pattern, Pattern_Length, 0);
}

char *
doio_pat_check(buf, offset, length, pattern, pattern_length, patshift)
char	*buf;
int	offset;
int 	length;
char	*pattern;
int	pattern_length;
int	patshift;
{
	static char	errbuf[4096];
	int		nb, i, pattern_index;
	char    	*cp, *bufend, *ep;
	char    	actual[33], expected[33];

	if (pattern_check(buf, length, pattern, pattern_length, patshift) != 0) {
		ep = errbuf;
		ep += sprintf(ep, "Corrupt regions follow - unprintable chars are represented as '.'\n");
		ep += sprintf(ep, "-----------------------------------------------------------------\n");

		pattern_index = patshift % pattern_length;;
		cp = buf;
		bufend = buf + length;

		while (cp < bufend) {
			if (*cp != pattern[pattern_index]) {
				nb = bufend - cp;
				if (nb > sizeof(expected)-1) {
					nb = sizeof(expected)-1;
				}
			    
				ep += sprintf(ep, "corrupt bytes starting at file offset %d\n", offset + (int)(cp-buf));

				/*
				 * Fill in the expected and actual patterns
				 */
				bzero(expected, sizeof(expected));
				bzero(actual, sizeof(actual));

				for (i = 0; i < nb; i++) {
					expected[i] = pattern[(pattern_index + i) % pattern_length];
					if (! isprint((int)expected[i])) {
						expected[i] = '.';
					}

					actual[i] = cp[i];
					if (! isprint((int)actual[i])) {
						actual[i] = '.';
					}
				}

				ep += sprintf(ep, "    1st %2d expected bytes:  %s\n", nb, expected);
				ep += sprintf(ep, "    1st %2d actual bytes:    %s\n", nb, actual);
				fflush(stderr);
				return errbuf;
			} else {
				cp++;
				pattern_index++;

				if (pattern_index == pattern_length) {
					pattern_index = 0;
				}
			}
		}
		return errbuf;
	}

	return(NULL);
}


/*
 * Check the contents of a file beginning at offset, for length bytes.  It
 * is assumed that there is a string of pattern bytes in this area of the
 * file.  Use normal buffered reads to do the verification.
 *
 * If there is a data mismatch, write a detailed message into a static buffer
 * suitable for the caller to print.  Otherwise print NULL.
 *
 * The fsa flag is set to non-zero if the buffer should be read back through
 * the FSA (unicos/mk).  This implies the file will be opened
 * O_PARALLEL|O_RAW|O_WELLFORMED to do the validation.  We must do this because
 * FSA will not allow the file to be opened for buffered io if it was
 * previously opened for O_PARALLEL io.
 */

char *
check_file(file, offset, length, pattern, pattern_length, patshift, fsa)
char 	*file;
int 	offset;
int 	length;
char	*pattern;
int	pattern_length;
int	patshift;
int	fsa;
{
	static char	errbuf[4096];
	int	    	fd, nb, flags;
	char		*buf, *em, *ep;
	struct fd_cache *fdc;

	buf = Memptr;

	if (V_opt) {
		flags = Validation_Flags | O_RDONLY;
	} else {
		flags = O_RDONLY;
	}

	if ((fd = alloc_fd(file, flags)) == -1) {
		sprintf(errbuf,
			"Could not open file %s with flags %#o (%s) for data comparison:  %s (%d)\n",
			file, flags, format_oflags(flags),
			SYSERR, errno);
		return errbuf;
	}

	if (lseek(fd, offset, SEEK_SET) == -1) {
		sprintf(errbuf, 
			"Could not lseek to offset %d in %s for verification:  %s (%d)\n",
			offset, file, SYSERR, errno);
		return errbuf;
	}

	/* Guarantee a properly aligned address on Direct I/O */
	fdc = alloc_fdcache(file, flags);
	if( (flags & O_DIRECT) && ((long)buf % fdc->c_memalign != 0) ) {
		buf += fdc->c_memalign - ((long)buf % fdc->c_memalign);
	}

	if ((nb = read(fd, buf, length)) == -1) {
		sprintf(errbuf,
			"Could not read %d bytes from %s for verification:  %s (%d)\n\tread(%d, 0x%p, %d)\n\tbuf %% alignment(%d) = %ld\n",
			length, file, SYSERR, errno,
			fd, buf, length,
			fdc->c_memalign, (long)buf % fdc->c_memalign);
		return errbuf;
	}

	if (nb != length) {
		sprintf(errbuf,
			"Read wrong # bytes from %s.  Expected %d, got %d\n",
			file, length, nb);
		return errbuf;
	}
    
	if( (em = (*Data_Check)(buf, offset, length, pattern, pattern_length, patshift)) != NULL ) {
		ep = errbuf;
		ep += sprintf(ep, "*** DATA COMPARISON ERROR ***\n");
		ep += sprintf(ep, "check_file(%s, %d, %d, %s, %d, %d) failed\n\n",
			      file, offset, length, pattern, pattern_length, patshift);
		ep += sprintf(ep, "Comparison fd is %d, with open flags %#o\n",
			      fd, flags);
		strcpy(ep, em);
		return(errbuf);
	}
	return NULL;
}

/*
 * Function to single-thread stdio output.
 */

int
doio_fprintf(FILE *stream, char *format, ...)
{
	static int	pid = -1;
	char		*date;
	int		rval;
	struct flock	flk;
	va_list		arglist;

	date = hms(time(0));

	if (pid == -1) {
		pid = getpid();
	}

	flk.l_whence = flk.l_start = flk.l_len = 0;
	flk.l_type = F_WRLCK;
	fcntl(fileno(stream), F_SETLKW, &flk);

	va_start(arglist, format);
	rval = fprintf(stream, "\n%s%s (%5d) %s\n", Prog, TagName, pid, date);
	rval += fprintf(stream, "---------------------\n");
	vfprintf(stream, format, arglist);
	va_end(arglist);

	fflush(stream);

	flk.l_type = F_UNLCK;
	fcntl(fileno(stream), F_SETLKW, &flk);
 
	return rval;
}

/*
 * Simple function for allocating core memory.  Uses Memsize and Memptr to
 * keep track of the current amount allocated.
 */

int
alloc_mem(nbytes)
int nbytes;
{
	char    	*cp;
	void		*addr;
	int		me = 0, flags, key, shmid;
	static int	mturn = 0;	/* which memory type to use */
	struct memalloc	*M;
	char		filename[255];
#ifdef linux
	struct shmid_ds shm_ds;
	bzero( &shm_ds, sizeof(struct shmid_ds) );
#endif

	/* nbytes = -1 means "free all allocated memory" */
	if( nbytes == -1 ) {

		for(me=0; me < Nmemalloc; me++) {
			if(Memalloc[me].space == NULL)
				continue;

			switch(Memalloc[me].memtype) {
			case MEM_DATA:
				free(Memalloc[me].space);
				Memalloc[me].space = NULL;
				Memptr = NULL;
				Memsize = 0;
				break;
			case MEM_SHMEM:
				shmdt(Memalloc[me].space);
				Memalloc[me].space = NULL;
				shmctl(Memalloc[me].fd, IPC_RMID, &shm_ds);
				break;
			case MEM_MMAP:
				munmap(Memalloc[me].space, 
				       Memalloc[me].size);
				close(Memalloc[me].fd);
				if(Memalloc[me].flags & MEMF_FILE) {
					unlink(Memalloc[me].name);
				}
				Memalloc[me].space = NULL;
				break;
			default:
				doio_fprintf(stderr, "alloc_mem: HELP! Unknown memory space type %d index %d\n",
					     Memalloc[me].memtype, me);
				break;
			}
		}
		return 0;
	}

	/*
	 * Select a memory area (currently round-robbin)
	 */

	if(mturn >= Nmemalloc)
		mturn=0;

	M = &Memalloc[mturn];

	switch(M->memtype) {
	case MEM_DATA:
		if( nbytes > M->size ) {
			if( M->space != NULL ){
				free(M->space);
			}
			M->space = NULL;
			M->size = 0;
		}

		if( M->space == NULL ) {
			if( (cp = malloc( nbytes )) == NULL ) {
				doio_fprintf(stderr, "malloc(%d) failed:  %s (%d)\n",
					     nbytes, SYSERR, errno);
				return -1;
			}
			M->space = (void *)cp;
			M->size = nbytes;
		}
		break;

	case MEM_MMAP:
		if( nbytes > M->size ) {
			if( M->space != NULL ) {
				munmap(M->space, M->size);
				close(M->fd);
				if( M->flags & MEMF_FILE )
					unlink( M->name );
			}
			M->space = NULL;
			M->size = 0;
		}

		if( M->space == NULL ) {
			if(strchr(M->name, '%')) {
				sprintf(filename, M->name, getpid());
				M->name = strdup(filename);
			}

			if( (M->fd = open(M->name, O_CREAT|O_RDWR, 0666)) == -1) {
				doio_fprintf(stderr, "alloc_mmap: error %d (%s) opening '%s'\n",
					     errno, SYSERR, 
					     M->name);
				return(-1);
			}

			addr = NULL;
			flags = 0;
			M->size = nbytes * 4;

			/* bias addr if MEMF_ADDR | MEMF_FIXADDR */
			/* >>> how to pick a memory address? */

			/* bias flags on MEMF_PRIVATE etc */
			if(M->flags & MEMF_PRIVATE)
				flags |= MAP_PRIVATE;
			if(M->flags & MEMF_SHARED)
				flags |= MAP_SHARED;

/*printf("alloc_mem, about to mmap, fd=%d, name=(%s)\n", M->fd, M->name);*/
			if( (M->space = mmap(addr, M->size,
					     PROT_READ|PROT_WRITE,
					     flags, M->fd, 0))
			    == MAP_FAILED) {
				doio_fprintf(stderr, "alloc_mem: mmap error. errno %d (%s)\n\tmmap(addr 0x%x, size %d, read|write 0x%x, mmap flags 0x%x [%#o], fd %d, 0)\n\tfile %s\n",
					     errno, SYSERR,
					     addr, M->size,
					     PROT_READ|PROT_WRITE,
					     flags, M->flags, M->fd,
					     M->name);
				doio_fprintf(stderr, "\t%s%s%s%s%s",
					     (flags & MAP_PRIVATE) ? "private " : "",
					     (flags & MAP_SHARED) ? "shared" : "");
				return(-1);
			}
		}
		break;
		
	case MEM_SHMEM:
		if( nbytes > M->size ) {
			if( M->space != NULL ) {
				shmctl( M->fd, IPC_RMID, &shm_ds );
			}
			M->space = NULL;
			M->size = 0;
		}

		if(M->space == NULL) {
			if(!strcmp(M->name, "private")) {
				key = IPC_PRIVATE;
			} else {
				sscanf(M->name, "%i", &key);
			}

			M->size = M->nblks ? M->nblks * 512 : nbytes;

			if( nbytes > M->size ){
#ifdef DEBUG
				doio_fprintf(stderr, "MEM_SHMEM: nblks(%d) too small:  nbytes=%d  Msize=%d, skipping this req.\n",
					     M->nblks, nbytes, M->size );
#endif
				return SKIP_REQ;
			}

			shmid = shmget(key, M->size, IPC_CREAT|0666);
			if( shmid == -1 ) {
				doio_fprintf(stderr, "shmget(0x%x, %d, CREAT) failed: %s (%d)\n",
					     key, M->size, SYSERR, errno);
				return(-1);
			}
			M->fd = shmid;
			M->space = shmat(shmid, NULL, SHM_RND);
			if( M->space == (void *)-1 ) {
				doio_fprintf(stderr, "shmat(0x%x, NULL, SHM_RND) failed: %s (%d)\n", 
					     shmid, SYSERR, errno);
				return(-1);
			}
		}
		break;

	default:
		doio_fprintf(stderr, "alloc_mem: HELP! Unknown memory space type %d index %d\n",
			     Memalloc[me].memtype, mturn);
		break;
	}

	Memptr = M->space;
	Memsize = M->size;

	mturn++;
	return 0;
}

/*
 * Function to maintain a file descriptor cache, so that doio does not have
 * to do so many open() and close() calls.  Descriptors are stored in the
 * cache by file name, and open flags.  Each entry also has a _rtc value
 * associated with it which is used in aging.  If doio cannot open a file
 * because it already has too many open (ie. system limit hit) it will close
 * the one in the cache that has the oldest _rtc value.
 *
 * If alloc_fd() is called with a file of NULL, it will close all descriptors
 * in the cache, and free the memory in the cache.
 */

int
alloc_fd(file, oflags)
char	*file;
int	oflags;
{
	struct fd_cache *fdc;
	struct fd_cache *alloc_fdcache(char *file, int oflags);

	fdc = alloc_fdcache(file, oflags);
	if(fdc != NULL)
		return(fdc->c_fd);
	else
		return(-1);
}

struct fd_cache *
alloc_fdcache(file, oflags)
char	*file;
int	oflags;
{
	int			fd;
	struct fd_cache		*free_slot, *oldest_slot, *cp;
	static int		cache_size = 0;
	static struct fd_cache	*cache = NULL;
	struct dioattr	finfo;

	/*
	 * If file is NULL, it means to free up the fd cache.
	 */

	if (file == NULL && cache != NULL) {
		for (cp = cache; cp < &cache[cache_size]; cp++) {
			if (cp->c_fd != -1) {
				close(cp->c_fd);
			}
			if (cp->c_memaddr != NULL) {
				munmap(cp->c_memaddr, cp->c_memlen);
			}
		}

		free(cache);
		cache = NULL;
		cache_size = 0;
                return 0;
	}

	free_slot = NULL;
	oldest_slot = NULL;

	/*
	 * Look for a fd in the cache.  If one is found, return it directly.
	 * Otherwise, when this loop exits, oldest_slot will point to the
	 * oldest fd slot in the cache, and free_slot will point to an
	 * unoccupied slot if there are any.
	 */

	for (cp = cache; cp != NULL && cp < &cache[cache_size]; cp++) {
		if (cp->c_fd != -1 &&
		    cp->c_oflags == oflags &&
		    strcmp(cp->c_file, file) == 0) {
			cp->c_rtc = Reqno;
			return cp;
		}

		if (cp->c_fd == -1) {
			if (free_slot == NULL) {
				free_slot = cp;
			}
		} else {
			if (oldest_slot == NULL || 
			    cp->c_rtc < oldest_slot->c_rtc) {
				oldest_slot = cp;
			}
		}
	}

	/*
	 * No matching file/oflags pair was found in the cache.  Attempt to
	 * open a new fd.
	 */

	if ((fd = open(file, oflags, 0666)) < 0) {
		if (errno != EMFILE) {
			doio_fprintf(stderr,
				     "Could not open file %s with flags %#o (%s): %s (%d)\n",
				     file, oflags, format_oflags(oflags),
				     SYSERR, errno);
			alloc_mem(-1);
			exit(E_SETUP);
		}

		/*
		 * If we get here, we have as many open fd's as we can have.
		 * Close the oldest one in the cache (pointed to by
		 * oldest_slot), and attempt to re-open.
		 */

		close(oldest_slot->c_fd);
		oldest_slot->c_fd = -1;
		free_slot = oldest_slot;

		if ((fd = open(file, oflags, 0666)) < 0) {
			doio_fprintf(stderr,
				     "Could not open file %s with flags %#o (%s):  %s (%d)\n",
				     file, oflags, format_oflags(oflags),
				     SYSERR, errno);
			alloc_mem(-1);
			exit(E_SETUP);
		}
	}

/*printf("alloc_fd: new file %s flags %#o fd %d\n", file, oflags, fd);*/

	/*
	 * If we get here, fd is our open descriptor.  If free_slot is NULL,
	 * we need to grow the cache, otherwise free_slot is the slot that
	 * should hold the fd info.
	 */

	if (free_slot == NULL) {
		cache = (struct fd_cache *)realloc(cache, sizeof(struct fd_cache) * (FD_ALLOC_INCR + cache_size));
		if (cache == NULL) {
			doio_fprintf(stderr, "Could not malloc() space for fd chace");
			alloc_mem(-1);
			exit(E_SETUP);
		}

		cache_size += FD_ALLOC_INCR;

		for (cp = &cache[cache_size-FD_ALLOC_INCR];
		     cp < &cache[cache_size]; cp++) {
			cp->c_fd = -1;
		}

		free_slot = &cache[cache_size - FD_ALLOC_INCR];
	}

	/*
	 * finally, fill in the cache slot info
	 */

	free_slot->c_fd = fd;
	free_slot->c_oflags = oflags;
	strcpy(free_slot->c_file, file);
	free_slot->c_rtc = Reqno;

	if (oflags & O_DIRECT) {
		char *dio_env;

		if (xfsctl(file, fd, XFS_IOC_DIOINFO, &finfo) == -1) {
			finfo.d_mem = 1;
			finfo.d_miniosz = 1;
			finfo.d_maxiosz = 1;
		}

		dio_env = getenv("XFS_DIO_MIN");
		if (dio_env)
			finfo.d_mem = finfo.d_miniosz = atoi(dio_env);

	} else {
		finfo.d_mem = 1;
		finfo.d_miniosz = 1;
		finfo.d_maxiosz = 1;
	}

	free_slot->c_memalign = finfo.d_mem;
	free_slot->c_miniosz = finfo.d_miniosz;
	free_slot->c_maxiosz = finfo.d_maxiosz;
	free_slot->c_memaddr = NULL;
	free_slot->c_memlen = 0;

	return free_slot;
}

/*
 *
 *			Signal Handling Section
 *
 *
 */

void
cleanup_handler()
{
	havesigint=1; /* in case there's a followup signal */
	alloc_mem(-1);
	exit(0);
}

void
die_handler(sig)
int sig;
{
	doio_fprintf(stderr, "terminating on signal %d\n", sig);
	alloc_mem(-1);
	exit(1);
}

void
sigbus_handler(sig)
int sig;
{
	/* See sigbus_handler() in the 'ifdef sgi' case for details.  Here,
	   we don't have the siginfo stuff so the guess is weaker but we'll
	   do it anyway.
	*/

	if( active_mmap_rw && havesigint )
		cleanup_handler();
	else
		die_handler(sig);
}

void
noop_handler(sig)
int sig;
{
	return;
}


/*
 * SIGINT handler for the parent (original doio) process.  It simply sends
 * a SIGINT to all of the doio children.  Since they're all in the same
 * pgrp, this can be done with a single kill().
 */

void
sigint_handler()
{
	int	i;

	for (i = 0; i < Nchildren; i++) {
		if (Children[i] != -1) {
			kill(Children[i], SIGINT);
		}
	}
}

/*
 * Signal handler used to inform a process when async io completes.  Referenced
 * in do_read() and do_write().  Note that the signal handler is not
 * re-registered.
 */

void
aio_handler(sig)
int	sig;
{
	int		i;
	struct aio_info	*aiop;

	for (i = 0; i < sizeof(Aio_Info) / sizeof(Aio_Info[0]); i++) {
		aiop = &Aio_Info[i];

		if (aiop->strategy == A_SIGNAL && aiop->sig == sig) {
			aiop->signalled++;

			if (aio_done(aiop)) {
				aiop->done++;
			}
		}
	}
}

/*
 * dump info on all open aio slots
 */
void
dump_aio()
{
	int		i, count;

	count=0;
	for (i = 0; i < sizeof(Aio_Info) / sizeof(Aio_Info[0]); i++) {
		if (Aio_Info[i].busy) {
			count++;
			fprintf(stderr,
				"Aio_Info[%03d] id=%d fd=%d signal=%d signaled=%d\n",
				i, Aio_Info[i].id,
				Aio_Info[i].fd,
				Aio_Info[i].sig,
				Aio_Info[i].signalled);
			fprintf(stderr, "\tstrategy=%s\n",
				format_strat(Aio_Info[i].strategy));
		}
	}
	fprintf(stderr, "%d active async i/os\n", count);
}

struct aio_info *
aio_slot(aio_id)
int	aio_id;
{
	int		i;
	static int	id = 1;
	struct aio_info	*aiop;

	aiop = NULL;

	for (i = 0; i < sizeof(Aio_Info) / sizeof(Aio_Info[0]); i++) {
		if (aio_id == -1) {
			if (! Aio_Info[i].busy) {
				aiop = &Aio_Info[i];
				aiop->busy = 1;
				aiop->id = id++;
				break;
			}
		} else {
			if (Aio_Info[i].busy && Aio_Info[i].id == aio_id) {
				aiop = &Aio_Info[i];
				break;
			}
		}
	}

	if( aiop == NULL ){
		doio_fprintf(stderr,"aio_slot(%d) not found.  Request %d\n", 
			     aio_id, Reqno);
		dump_aio();
		alloc_mem(-1);
		exit(E_INTERNAL);
	}

	return aiop;
}

int
aio_register(fd, strategy, sig)
int		fd;
int		strategy;
int		sig;
{
	struct aio_info		*aiop;
	void			aio_handler();
	struct sigaction	sa;

	aiop = aio_slot(-1);

	aiop->fd = fd;
	aiop->strategy = strategy;
	aiop->done = 0;

	if (strategy == A_SIGNAL) {
		aiop->sig = sig;
		aiop->signalled = 0;

		sa.sa_handler = aio_handler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);

		sigaction(sig, &sa, &aiop->osa);
	} else {
		aiop->sig = -1;
		aiop->signalled = 0;
	}

	return aiop->id;
}

int
aio_unregister(aio_id)
int	aio_id;
{
	struct aio_info	*aiop;

	aiop = aio_slot(aio_id);

	if (aiop->strategy == A_SIGNAL) {
		sigaction(aiop->sig, &aiop->osa, NULL);
	}

	aiop->busy = 0;
	return 0;
}

#ifndef linux
int
aio_wait(aio_id)
int	aio_id;
{
#ifdef RECALL_SIZEOF
	long		mask[RECALL_SIZEOF];
#endif
	sigset_t	sigset;
	struct aio_info	*aiop;

/*printf("aio_wait: errno %d return %d\n", aiop->aio_errno, aiop->aio_ret);*/

	return 0;
}
#endif /* !linux */

/*
 * Format specified time into HH:MM:SS format.  t is the time to format
 * in seconds (as returned from time(2)).
 */

char *
hms(t)
time_t	t;
{
	static char	ascii_time[9];
	struct tm	*ltime;

	ltime = localtime(&t);
	strftime(ascii_time, sizeof(ascii_time), "%H:%M:%S", ltime);

	return ascii_time;
}

/*
 * Simple routine to check if an async io request has completed.
 */

int
aio_done(struct aio_info *ainfo)
{
        return -1;   /* invalid */
}

/*
 * Routine to handle upanic() - it first attempts to set the panic flag.  If
 * the flag cannot be set, an error message is issued.  A call to upanic
 * with PA_PANIC is then done unconditionally, in case the panic flag was set
 * from outside the program (as with the panic(8) program).
 *
 * Note - we only execute the upanic code if -U was used, and the passed in
 * mask is set in the Upanic_Conditions bitmask.
 */

void
doio_upanic(mask)
int	mask;
{
	if (U_opt == 0 || (mask & Upanic_Conditions) == 0) {
		return;
	}
	doio_fprintf(stderr, "WARNING - upanic() failed\n");
}

/*
 * Parse cmdline options/arguments and set appropriate global variables.
 * If the cmdline is valid, return 0 to caller.  Otherwise exit with a status
 * of 1.
 */

int
parse_cmdline(argc, argv, opts)
int 	argc;
char	**argv;
char	*opts;
{
	int	    	c;
	char    	cc, *cp, *tok = NULL;
	extern int	opterr;
	extern int	optind;
	extern char	*optarg;
	struct smap	*s;
	char		*memargs[NMEMALLOC];
	int		nmemargs, ma;
	void		parse_memalloc(char *arg);
	void		parse_delay(char *arg);
	void		dump_memalloc();

	if (*argv[0] == '-') {
		argv[0]++;
		Execd = 1;
	}
	
	if ((Prog = strrchr(argv[0], '/')) == NULL) {
		Prog = argv[0];
	} else {
		Prog++;
	}
	
	opterr = 0;
	while ((c = getopt(argc, argv, opts)) != EOF) {
		switch ((char)c) {
		case 'a':
			a_opt++;
			break;

		case 'C':
			C_opt++;
			for(s=checkmap; s->string != NULL; s++)
				if(!strcmp(s->string, optarg))
					break;
			if (s->string == NULL) {
				fprintf(stderr,
					"%s%s:  Illegal -C arg (%s).  Must be one of: ", 
					Prog, TagName, tok);

				for (s = checkmap; s->string != NULL; s++)
					fprintf(stderr, "%s ", s->string);
				fprintf(stderr, "\n");
				exit(1);
			}

			switch(s->value) {
			case C_DEFAULT:
				Data_Fill = doio_pat_fill;
				Data_Check = doio_pat_check;
				break;
			default:
				fprintf(stderr,
					"%s%s:  Unrecognised -C arg '%s' %d", 
					Prog, TagName, s->string, s->value);
				exit(1);
			}
			break;

		case 'd':	/* delay between i/o ops */
			parse_delay(optarg);
			break;

		case 'e':
			if (Npes > 1 && Nprocs > 1) {
				fprintf(stderr, "%s%s:  Warning - Program is a multi-pe application - exec option is ignored.\n", Prog, TagName);
			}
			e_opt++;
			break;

		case 'h':
			help(stdout);
			exit(0);
			break;

		case 'k':
			k_opt++;
			break;

		case 'm':
			Message_Interval = strtol(optarg, &cp, 10);
			if (*cp != '\0' || Message_Interval < 0) {
				fprintf(stderr, "%s%s:  Illegal -m arg (%s):  Must be an integer >= 0\n", Prog, TagName, optarg);
				exit(1);
			}
			m_opt++;
			break;

		case 'M':	/* memory allocation types */
			nmemargs = string_to_tokens(optarg, memargs, 32, ",");
			for(ma=0; ma < nmemargs; ma++) {
				parse_memalloc(memargs[ma]);
			}
			/*dump_memalloc();*/
			M_opt++;
			break;

		case 'N':
			sprintf( TagName, "(%.39s)", optarg );
			break;

		case 'n':
			Nprocs = strtol(optarg, &cp, 10);
			if (*cp != '\0' || Nprocs < 1) {
				fprintf(stderr,
					"%s%s:  Illegal -n arg (%s):  Must be integer > 0\n",
					Prog, TagName, optarg);
				exit(E_USAGE);
			}

			if (Npes > 1 && Nprocs > 1) {
				fprintf(stderr, "%s%s:  Program has been built as a multi-pe app.  -n1 is the only nprocs value allowed\n", Prog, TagName);
				exit(E_SETUP);
			}
			n_opt++;
			break;

		case 'r':
			Release_Interval = strtol(optarg, &cp, 10);
			if (*cp != '\0' || Release_Interval < 0) {
				fprintf(stderr,
					"%s%s:  Illegal -r arg (%s):  Must be integer >= 0\n",
					Prog, TagName, optarg);
				exit(E_USAGE);
			}

			r_opt++;
			break;

		case 'w':
			Write_Log = optarg;
			w_opt++;
			break;

		case 'v':
			v_opt++;
			break;

		case 'V':
			if (strcasecmp(optarg, "sync") == 0) {
				Validation_Flags = O_SYNC;
			} else if (strcasecmp(optarg, "buffered") == 0) {
				Validation_Flags = 0;
			} else if (strcasecmp(optarg, "direct") == 0) {
				Validation_Flags = O_DIRECT;
			} else {
				if (sscanf(optarg, "%i%c", &Validation_Flags, &cc) != 1) {
					fprintf(stderr, "%s:  Invalid -V argument (%s) - must be a decimal, hex, or octal\n", Prog, optarg);
					fprintf(stderr, "    number, or one of the following strings:  'sync',\n");
					fprintf(stderr, "    'buffered', 'parallel', 'ldraw', or 'raw'\n");
					exit(E_USAGE);
				}
			}
			V_opt++;
			break;
		case 'U':
			tok = strtok(optarg, ",");
			while (tok != NULL) {
				for (s = Upanic_Args; s->string != NULL; s++)
					if (strcmp(s->string, tok) == 0)
						break;

				if (s->string == NULL) {
					fprintf(stderr,
						"%s%s:  Illegal -U arg (%s).  Must be one of: ", 
						Prog, TagName, tok);

					for (s = Upanic_Args; s->string != NULL; s++)
						fprintf(stderr, "%s ", s->string);

					fprintf(stderr, "\n");

					exit(1);
				}

				Upanic_Conditions |= s->value;
				tok = strtok(NULL, ",");
			}

			U_opt++;
			break;

		case '?':
			usage(stderr);
			exit(E_USAGE);
			break;
		}
	}
	
	/*
	 * Supply defaults
	 */
	
	if (! C_opt) {
		Data_Fill = doio_pat_fill;
		Data_Check = doio_pat_check;
	}

	if (! U_opt)
		Upanic_Conditions = 0;

	if (! n_opt)
		Nprocs = 1;
	
	if (! r_opt)
		Release_Interval = DEF_RELEASE_INTERVAL;

	if (! M_opt) {
		Memalloc[Nmemalloc].memtype = MEM_DATA;
		Memalloc[Nmemalloc].flags = 0;
		Memalloc[Nmemalloc].name = NULL;
		Memalloc[Nmemalloc].space = NULL;
		Nmemalloc++;
	}

	/*
	 * Initialize input stream
	 */

	if (argc == optind) {
		Infile = NULL;
	} else {
		Infile = argv[optind++];
	}

	if (argc != optind) {
		usage(stderr);
		exit(E_USAGE);
	}

	return 0;
}	



/*
 * Parse memory allocation types
 *
 * Types are:
 *  Data
 *  T3E-shmem:blksize[:nblks]
 *  SysV-shmem:shmid:blksize:nblks
 *	if shmid is "private", use IPC_PRIVATE
 *	and nblks is not required
 *
 *  mmap:flags:filename:blksize[:nblks]
 *   flags are one of:
 *	p - private (MAP_PRIVATE)
 *	a - private, MAP_AUTORESRV
 *	l - local (MAP_LOCAL)
 *	s - shared (nblks required)
 *
 *   plus any of:
 *	f - fixed address (MAP_FIXED)
 *	A - use an address without MAP_FIXED
 *	a - autogrow (map once at startup)
 *
 *  mmap:flags:devzero
 *	mmap /dev/zero  (shared not allowd)
 *	maps the first 4096 bytes of /dev/zero
 *
 * - put a directory at the beginning of the shared
 *   regions saying what pid has what region.
 *	DIRMAGIC
 *	BLKSIZE
 *	NBLKS
 *	nblks worth of directories - 1 int pids
 */
void
parse_memalloc(char *arg)
{
	char		*allocargs[NMEMALLOC];
	int		nalloc;
	struct memalloc	*M;

	if(Nmemalloc >= NMEMALLOC) {
		doio_fprintf(stderr, "Error - too many memory types (%d).\n", 
			Nmemalloc);
		return;
	}

	M = &Memalloc[Nmemalloc];

	nalloc = string_to_tokens(arg, allocargs, 32, ":");
	if(!strcmp(allocargs[0], "data")) {
		M->memtype = MEM_DATA;
		M->flags = 0;
		M->name = NULL;
		M->space = NULL;
		Nmemalloc++;
		if(nalloc >= 2) {
			if(strchr(allocargs[1], 'p'))
				M->flags |= MEMF_MPIN;
		}
	} else if(!strcmp(allocargs[0], "mmap")) {
		/* mmap:flags:filename[:size] */
		M->memtype = MEM_MMAP;
		M->flags = 0;
		M->space = NULL;
		if(nalloc >= 1) {
			if(strchr(allocargs[1], 'p'))
				M->flags |= MEMF_PRIVATE;
			if(strchr(allocargs[1], 'a'))
				M->flags |= MEMF_AUTORESRV;
			if(strchr(allocargs[1], 'l'))
				M->flags |= MEMF_LOCAL;
			if(strchr(allocargs[1], 's'))
				M->flags |= MEMF_SHARED;

			if(strchr(allocargs[1], 'f'))
				M->flags |= MEMF_FIXADDR;
			if(strchr(allocargs[1], 'A'))
				M->flags |= MEMF_ADDR;
			if(strchr(allocargs[1], 'G'))
				M->flags |= MEMF_AUTOGROW;

			if(strchr(allocargs[1], 'U'))
				M->flags |= MEMF_FILE;
		} else {
			M->flags |= MEMF_PRIVATE;
		}

		if(nalloc > 2) {
			if(!strcmp(allocargs[2], "devzero")) {
				M->name = "/dev/zero";
				if(M->flags & 
				   ((MEMF_PRIVATE|MEMF_LOCAL) == 0))
					M->flags |= MEMF_PRIVATE;
			} else {
				M->name = allocargs[2];
			}
		} else {
			M->name = "/dev/zero";
			if(M->flags & 
			   ((MEMF_PRIVATE|MEMF_LOCAL) == 0))
				M->flags |= MEMF_PRIVATE;
		}
		Nmemalloc++;

	} else if(!strcmp(allocargs[0], "shmem")) {
		/* shmem:shmid:size */
		M->memtype = MEM_SHMEM;
		M->flags = 0;
		M->space = NULL;
		if(nalloc >= 2) {
			M->name = allocargs[1];
		} else {
			M->name = NULL;
		}
		if(nalloc >= 3) {
			sscanf(allocargs[2], "%i", &M->nblks);
		} else {
			M->nblks = 0;
		}
		if(nalloc >= 4) {
			if(strchr(allocargs[3], 'p'))
				M->flags |= MEMF_MPIN;
		}

		Nmemalloc++;
	} else {
		doio_fprintf(stderr, "Error - unknown memory type '%s'.\n",
			allocargs[0]);
		exit(1);
	}
}

void
dump_memalloc()
{
	int	ma;
	char	*mt;

	if(Nmemalloc == 0) {
		printf("No memory allocation strategies devined\n");
		return;
	}

	for(ma=0; ma < Nmemalloc; ma++) {
		switch(Memalloc[ma].memtype) {
		case MEM_DATA:	mt = "data";	break;
		case MEM_SHMEM:	mt = "shmem";	break;
		case MEM_MMAP:	mt = "mmap";	break;
		default:	mt = "unknown";	break;
		}
		printf("mstrat[%d] = %d %s\n", ma, Memalloc[ma].memtype, mt);
		printf("\tflags=%#o name='%s' nblks=%d\n",
		       Memalloc[ma].flags,
		       Memalloc[ma].name,
		       Memalloc[ma].nblks);
	}
}

/*
 * -d <op>:<time> - doio inter-operation delay
 *	currently this permits ONE type of delay between operations.
 */

void
parse_delay(char *arg)
{
	char		*delayargs[NMEMALLOC];
	int		ndelay;
	struct smap	*s;

	ndelay = string_to_tokens(arg, delayargs, 32, ":");
	if(ndelay < 2) {
		doio_fprintf(stderr,
			"Illegal delay arg (%s). Must be operation:time\n", arg);
		exit(1);
	}
	for(s=delaymap; s->string != NULL; s++)
		if(!strcmp(s->string, delayargs[0]))
			break;
	if (s->string == NULL) {
		fprintf(stderr,
			"Illegal Delay arg (%s).  Must be one of: ", arg);

		for (s = delaymap; s->string != NULL; s++)
			fprintf(stderr, "%s ", s->string);
		fprintf(stderr, "\n");
		exit(1);
	}

	delayop = s->value;

	sscanf(delayargs[1], "%i", &delaytime);

	if(ndelay > 2) {
		fprintf(stderr,
			"Warning: extra delay arguments ignored.\n");
	}
}


/*
 * Usage clause - obvious
 */

int
usage(stream)
FILE	*stream;
{
	/*
	 * Only do this if we are on vpe 0, to avoid seeing it from every
	 * process in the application.
	 */

	if (Npes > 1 && Vpe != 0) {
		return 0;
	}

	fprintf(stream, "usage%s:  %s [-aekv] [-m message_interval] [-n nprocs] [-r release_interval] [-w write_log] [-V validation_ftype] [-U upanic_cond] [infile]\n", TagName, Prog);
	return 0;
}

void
help(stream)
FILE	*stream;
{
	/*
	 * Only the app running on vpe 0 gets to issue help - this prevents
	 * everybody in the application from doing this.
	 */

	if (Npes > 1 && Vpe != 0) {
		return;
	}

	usage(stream);
	fprintf(stream, "\n");
	fprintf(stream, "\t-a                   abort - kill all doio processes on data compare\n");
	fprintf(stream, "\t                     errors.  Normally only the erroring process exits\n");
	fprintf(stream, "\t-C data-pattern-type \n");
	fprintf(stream, "\t                     Available data patterns are:\n");
	fprintf(stream, "\t                     default - repeating pattern\n");
	fprintf(stream, "\t-d Operation:Time    Inter-operation delay.\n");
	fprintf(stream, "\t                     Operations are:\n");
	fprintf(stream, "\t                         select:time (1 second=1000000)\n");
	fprintf(stream, "\t                         sleep:time (1 second=1)\n");
	fprintf(stream, "\t                         alarm:time (1 second=1)\n");
	fprintf(stream, "\t-e                   Re-exec children before entering the main\n");
	fprintf(stream, "\t                     loop.  This is useful for spreading\n");
	fprintf(stream, "\t                     procs around on multi-pe systems.\n");
	fprintf(stream, "\t-k                   Lock file regions during writes using fcntl()\n");
	fprintf(stream, "\t-v                   Verify writes - this is done by doing a buffered\n");
	fprintf(stream, "\t                     read() of the data if file io was done, or\n");
	fprintf(stream, "\t                     an ssread()of the data if sds io was done\n");
	fprintf(stream, "\t-M                   Data buffer allocation method\n");
	fprintf(stream, "\t                     alloc-type[,type]\n");
	fprintf(stream, "\t			    data\n");
	fprintf(stream, "\t			    shmem:shmid:size\n");
	fprintf(stream, "\t			    mmap:flags:filename\n");
	fprintf(stream, "\t			        p - private\n");
	fprintf(stream, "\t			        s - shared (shared file must exist\n"),
	fprintf(stream, "\t			            and have needed length)\n");
	fprintf(stream, "\t			        f - fixed address (not used)\n");
	fprintf(stream, "\t			        a - specify address (not used)\n");
	fprintf(stream, "\t			        U - Unlink file when done\n");
	fprintf(stream, "\t			        The default flag is private\n");
	fprintf(stream, "\n");
	fprintf(stream, "\t-m message_interval  Generate a message every 'message_interval'\n");
	fprintf(stream, "\t                     requests.  An interval of 0 suppresses\n");
	fprintf(stream, "\t                     messages.  The default is 0.\n");
	fprintf(stream, "\t-N tagname           Tag name, for Monster.\n");
	fprintf(stream, "\t-n nprocs            # of processes to start up\n");
	fprintf(stream, "\t-r release_interval  Release all memory and close\n");
	fprintf(stream, "\t                     files every release_interval operations.\n");
	fprintf(stream, "\t                     By default procs never release memory\n");
	fprintf(stream, "\t                     or close fds unless they have to.\n");
	fprintf(stream, "\t-V validation_ftype  The type of file descriptor to use for doing data\n");
	fprintf(stream, "\t                     validation.  validation_ftype may be an octal,\n");
	fprintf(stream, "\t                     hex, or decimal number representing the open()\n");
	fprintf(stream, "\t                     flags, or may be one of the following strings:\n");
	fprintf(stream, "\t                     'buffered' - validate using bufferd read\n");
	fprintf(stream, "\t                     'sync'     - validate using O_SYNC read\n");
	fprintf(stream, "\t                     'direct    - validate using O_DIRECT read'\n");
	fprintf(stream, "\t                     By default, 'parallel'\n");
	fprintf(stream, "\t                     is used if the write was done with O_PARALLEL\n");
	fprintf(stream, "\t                     or 'buffered' for all other writes.\n");
	fprintf(stream, "\t-w write_log         File to log file writes to.  The doio_check\n");
	fprintf(stream, "\t                     program can reconstruct datafiles using the\n");
	fprintf(stream, "\t                     write_log, and detect if a file is corrupt\n");
	fprintf(stream, "\t                     after all procs have exited.\n");
	fprintf(stream, "\t-U upanic_cond       Comma separated list of conditions that will\n");
	fprintf(stream, "\t                     cause a call to upanic(PA_PANIC).\n");
	fprintf(stream, "\t                     'corruption' -> upanic on bad data comparisons\n");
	fprintf(stream, "\t                     'iosw'     ---> upanic on unexpected async iosw\n");
	fprintf(stream, "\t                     'rval'     ---> upanic on unexpected syscall rvals\n");
	fprintf(stream, "\t                     'all'      ---> all of the above\n");
	fprintf(stream, "\n");
	fprintf(stream, "\tinfile               Input stream - default is stdin - must be a list\n");
	fprintf(stream, "\t                     of io_req structures (see doio.h).  Currently\n");
	fprintf(stream, "\t                     only the iogen program generates the proper\n");
	fprintf(stream, "\t                     format\n");
}	

