#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

static void err_exit(char *op, int errn)
{
	fprintf(stderr, "%s: %s\n", op, strerror(errn));
	exit(errn);
}

void *thread_fn(void *arg)
{
	/* execve will release threads */
	while(1) sleep(1);
	return NULL;
}

struct flock fl = {
	.l_type = F_WRLCK,
	.l_whence = SEEK_SET,
	.l_start = 0,
	.l_len = 1,
};

static void checklock(int fd)
{
	pid_t pid;

	pid = fork();
	if (pid < 0)
		err_exit("fork", errno);

	if (!pid) {
		if (fcntl(fd, F_GETLK, &fl) < 0)
			err_exit("getlk", errno);
		if (fl.l_type == F_UNLCK) {
			printf("record lock is not preserved across execve(2)\n");
			exit(1);
		}
		exit(0);
	}

	waitpid(pid, NULL, 0);

	exit(0);
}

int main(int argc, char **argv)
{
	int fd, flags;
	char fdstr[10];
	char *newargv[] = { argv[0], argv[1], fdstr, NULL };
	pthread_t th;

	/* passing fd in argv[2] in execve */
	if (argc == 3) {
		fd = atoi(argv[2]);
		checklock(fd);
	}

	fd = open(argv[1], O_WRONLY|O_CREAT, 0755);
	if (fd < 0)
		err_exit("open", errno);
	if (fcntl(fd, F_SETLK, &fl) < 0)
		err_exit("setlk", errno);

	/* require multithread process to reproduce the issue */
	pthread_create(&th, NULL, thread_fn, &fd);

	if ((flags = fcntl(fd, F_GETFD)) < 0)
		err_exit("getfd", errno);
	flags &= ~FD_CLOEXEC;
	if (fcntl(fd, F_SETFD, flags) < 0)
		err_exit("setfd", errno);

	snprintf(fdstr, sizeof(fdstr), "%d", fd);
	execve(argv[0], newargv, NULL);

	return 0;
}
