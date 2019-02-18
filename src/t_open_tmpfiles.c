// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 *
 * Test program to open unlinked files and leak them.
 */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "global.h"

static int min_fd = -1;
static int max_fd = -1;
static unsigned int nr_opened = 0;
static float start_time;
static int shutdown_fs = 0;

void clock_time(float *time)
{
	static clockid_t clkid = CLOCK_MONOTONIC;
	struct timespec ts;
	int ret;

retry:
	ret = clock_gettime(clkid, &ts);
	if (ret) {
		if (clkid == CLOCK_MONOTONIC) {
			clkid = CLOCK_REALTIME;
			goto retry;
		}
		perror("clock_gettime");
		exit(2);
	}
	*time = ts.tv_sec + ((float)ts.tv_nsec / 1000000000);
}

/*
 * Exit the program due to an error.
 *
 * If we've exhausted all the file descriptors, make sure we close all the
 * open fds in the order we received them in order to exploit a quirk of ext4
 * and xfs where the oldest unlinked inodes are at the /end/ of the unlinked
 * lists, which will make removing the unlinked files maximally painful.
 *
 * If it's some other error, just die and let the kernel sort it out.
 */
void die(void)
{
	float end_time;
	int fd;

	switch (errno) {
	case EMFILE:
	case ENFILE:
	case ENOSPC:
		clock_time(&end_time);
		printf("Opened %u files in %.2fs.\n", nr_opened,
				end_time - start_time);
		fflush(stdout);

		if (shutdown_fs) {
			/*
			 * Flush the log so that we have to process the
			 * unlinked inodes the next time we mount.
			 */
			int flag = XFS_FSOP_GOING_FLAGS_LOGFLUSH;
			int ret;

			ret = ioctl(min_fd, XFS_IOC_GOINGDOWN, &flag);
			if (ret) {
				perror("shutdown");
				exit(2);
			}
			exit(0);
		}

		clock_time(&start_time);
		for (fd = min_fd; fd <= max_fd; fd++)
			close(fd);
		clock_time(&end_time);
		printf("Closed %u files in %.2fs.\n", nr_opened,
				end_time - start_time);
		exit(0);
		break;
	default:
		perror("open?");
		exit(2);
		break;
	}
}

/* Remember how many file we open and all that. */
void remember_fd(int fd)
{
	if (min_fd == -1 || min_fd > fd)
		min_fd = fd;
	if (max_fd == -1 || max_fd < fd)
		max_fd = fd;
	nr_opened++;
}

/* Put an opened file on the unlinked list and leak the fd. */
void leak_tmpfile(void)
{
	int fd = -1;
	int ret;
#ifdef O_TMPFILE
	static int try_o_tmpfile = 1;
#endif

	/* Try to create an O_TMPFILE and leak the fd. */
#ifdef O_TMPFILE
	if (try_o_tmpfile) {
		fd = open(".", O_TMPFILE | O_RDWR, 0644);
		if (fd >= 0) {
			remember_fd(fd);
			return;
		}
		if (fd < 0) {
			if (errno == EOPNOTSUPP)
				try_o_tmpfile = 0;
			else
				die();
		}
	}
#endif

	/* Oh well, create a new file, unlink it, and leak the fd. */
	fd = open("./moo", O_CREAT | O_RDWR, 0644);
	if (fd < 0)
		die();
	ret = unlink("./moo");
	if (ret)
		die();
	remember_fd(fd);
}

/*
 * Try to put as many files on the unlinked list and then kill them.
 * The first argument is a directory to chdir into; passing any second arg
 * will shut down the fs instead of closing files.
 */
int main(int argc, char *argv[])
{
	int ret;

	if (argc > 1) {
		ret = chdir(argv[1]);
		if (ret)
			perror(argv[1]);
	}
	if (argc > 2 && !strcmp(argv[2], "shutdown"))
		shutdown_fs = 1;

	clock_time(&start_time);
	while (1)
		leak_tmpfile();
	return 0;
}
