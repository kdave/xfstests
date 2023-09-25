// SPDX-License-Identifier: GPL-2.0+
/* Copyright (c) 2023 Alexander Aring.  All Rights Reserved.
 */

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <wait.h>

static char filename[PATH_MAX + 1];
static int fd;

static void usage(char *name, const char *msg)
{
	printf("Fatal: %s\nUsage:\n"
	       "%s <file for fcntl>\n", msg, name);
	_exit(1);
}

static void *do_equal_file_lock_thread(void *arg)
{
       struct flock fl = {
	       .l_type = F_WRLCK,
	       .l_whence = SEEK_SET,
	       .l_start = 0L,
	       .l_len = 1L,
       };
       int rv;

       rv = fcntl(fd, F_SETLKW, &fl);
       if (rv == -1) {
	       perror("fcntl");
	       _exit(1);
       }

       return NULL;
}

static void do_test_equal_file_lock(void)
{
	struct flock fl;
	pthread_t t[2];
	int pid, rv;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0L;
	fl.l_len = 1L;

	/* acquire range 0-0 */
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
	       perror("fcntl");
		_exit(1);
	}

	pid = fork();
	if (pid == 0) {
		rv = pthread_create(&t[0], NULL, do_equal_file_lock_thread, NULL);
		if (rv != 0) {
			fprintf(stderr, "failed to create pthread\n");
			_exit(1);
		}

		rv = pthread_create(&t[1], NULL, do_equal_file_lock_thread, NULL);
		if (rv != 0) {
			fprintf(stderr, "failed to create pthread\n");
			_exit(1);
		}

		pthread_join(t[0], NULL);
		pthread_join(t[1], NULL);

		_exit(0);
	}

	/* wait until threads block on 0-0 */
	sleep(3);

	fl.l_type = F_UNLCK;
	fl.l_start = 0;
	fl.l_len = 1;
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}

	sleep(3);

	/* check if the ->lock() implementation got the
	 * right locks granted because two waiter with the
	 * same file_lock fields are waiting
	 */
	fl.l_type = F_WRLCK;
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1 && errno == EAGAIN) {
		fprintf(stderr, "deadlock, pthread not cleaned up correctly\n");
		_exit(1);
	}

	wait(NULL);
}

static void catch_alarm(int num) { }

static void do_test_signal_interrupt_child(int *pfd)
{
	struct sigaction act;
	unsigned char m;
	struct flock fl;
	int rv;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 1;
	fl.l_len = 1;

	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}

	memset(&act, 0, sizeof(act));
	act.sa_handler = catch_alarm;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);
	sigaction(SIGALRM, &act, NULL);

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	/* interrupt SETLKW by signal in 3 secs */
	alarm(3);
	rv = fcntl(fd, F_SETLKW, &fl);
	if (rv == 0) {
		fprintf(stderr, "fcntl interrupt successful but should fail with EINTR\n");
		_exit(1);
	}

	/* synchronize to move parent to test region 1-1 */
	write(pfd[1], &m, sizeof(m));

	/* keep child alive */
	read(pfd[1], &m, sizeof(m));
}

static void do_test_signal_interrupt(void)
{
	struct flock fl;
	unsigned char m;
	int pid, rv;
	int pfd[2];

	rv = pipe(pfd);
	if (rv == -1) {
		perror("pipe");
		_exit(1);
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}

	pid = fork();
	if (pid == 0) {
		do_test_signal_interrupt_child(pfd);
		_exit(0);
	}

	/* wait until child writes */
	read(pfd[0], &m, sizeof(m));

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 1;
	fl.l_len = 1;
	/* parent testing childs region, the child will think
	 * it has region 1-1 locked because it was interrupted
	 * by region 0-0. Due bugs the interruption also unlocked
	 * region 1-1.
	 */
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == 0) {
		fprintf(stderr, "fcntl trylock successful but should fail because child still acquires region\n");
		_exit(1);
	}

	write(pfd[0], &m, sizeof(m));

	wait(NULL);

	close(pfd[0]);
	close(pfd[1]);

	/* cleanup everything */
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 2;
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}
}

static void do_test_kill_child(void)
{
	struct flock fl;
	int rv;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	rv = fcntl(fd, F_SETLKW, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}
}

static void do_test_kill(void)
{
	struct flock fl;
	int pid_to_kill;
	int rv;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}

	pid_to_kill = fork();
	if (pid_to_kill == 0) {
		do_test_kill_child();
		_exit(0);
	}

	/* wait until child blocks */
	sleep(3);

	kill(pid_to_kill, SIGKILL);

	/* wait until Linux did plock cleanup */
	sleep(3);

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	/* cleanup parent lock */
	rv = fcntl(fd, F_SETLK, &fl);
	if (rv == -1) {
		perror("fcntl");
		_exit(1);
	}

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;

	/* check if the child still holds the lock
	 * and killing the child was not cleaning
	 * up locks.
	 */
	rv = fcntl(fd, F_SETLK, &fl);
	if ((rv == -1 && errno == EAGAIN)) {
		fprintf(stderr, "fcntl trylock failed but should be successful because killing child should cleanup acquired lock\n");
		_exit(1);
	}
}

int main(int argc, char * const argv[])
{
	if (optind != argc - 1)
		usage(argv[0], "<file for fcntl> is mandatory to tell the file where to run fcntl() on it");

	strncpy(filename, argv[1], PATH_MAX);

	fd = open(filename, O_RDWR | O_CREAT, 0700);
	if (fd == -1) {
		perror("open");
		_exit(1);
	}

	/* test to have to equal struct file_lock requests in ->lock() */
	do_test_equal_file_lock();
	/* test to interrupt F_SETLKW by a signal and cleanup only canceled the pending interrupted request */
	do_test_signal_interrupt();
	/* test if cleanup is correct if a child gets killed while being blocked in F_SETLKW */
	do_test_kill();

	close(fd);

	return 0;
}
