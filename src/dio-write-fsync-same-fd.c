// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 SUSE Linux Products GmbH.  All Rights Reserved.
 */

/*
 * Test two threads working with the same file descriptor, one doing direct IO
 * writes into the file and the other just doing fsync calls. We want to verify
 * that there are no crashes or deadlocks.
 *
 * This program never finishes, it starts two infinite loops to write and fsync
 * the file. It's meant to be called with the 'timeout' program from coreutils.
 */

/* Get the O_DIRECT definition. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

static int fd;

static ssize_t do_write(int fd, const void *buf, size_t count, off_t offset)
{
        while (count > 0) {
		ssize_t ret;

		ret = pwrite(fd, buf, count, offset);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return ret;
		}
		count -= ret;
		buf += ret;
	}
	return 0;
}

static void *fsync_loop(void *arg)
{
	while (1) {
		int ret;

		ret = fsync(fd);
		if (ret != 0) {
			perror("Fsync failed");
			exit(6);
		}
	}
}

int main(int argc, char *argv[])
{
	long pagesize;
	void *write_buf;
	pthread_t fsyncer;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Use: %s <file path>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0666);
	if (fd == -1) {
		perror("Failed to open/create file");
		return 1;
	}

	pagesize = sysconf(_SC_PAGE_SIZE);
	if (pagesize == -1) {
		perror("Failed to get page size");
		return 2;
	}

	ret = posix_memalign(&write_buf, pagesize, pagesize);
	if (ret) {
		perror("Failed to allocate buffer");
		return 3;
	}

	ret = pthread_create(&fsyncer, NULL, fsync_loop, NULL);
	if (ret != 0) {
		fprintf(stderr, "Failed to create writer thread: %d\n", ret);
		return 4;
	}

	while (1) {
		ret = do_write(fd, write_buf, pagesize, 0);
		if (ret != 0) {
			perror("Write failed");
			exit(5);
		}
	}

	return 0;
}
