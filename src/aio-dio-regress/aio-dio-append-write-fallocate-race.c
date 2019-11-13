// SPDX-License-Identifier: GPL-2.0-or-newer
/*
 * Copyright (c) 2019 Oracle.
 * All Rights Reserved.
 *
 * Race appending aio dio and fallocate to make sure we get the correct file
 * size afterwards.
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libaio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

static int fd;
static int blocksize;

static void *
falloc_thread(
	void		*p)
{
	int		ret;

	ret = fallocate(fd, 0, 0, blocksize);
	if (ret)
		perror("falloc");

	return NULL;
}

static int
test(
	const char	*fname,
	unsigned int	iteration,
	unsigned int	*passed)
{
	struct stat	sbuf;
	pthread_t	thread;
	io_context_t	ioctx = 0;
	struct iocb	iocb;
	struct iocb	*iocbp = &iocb;
	struct io_event	event;
	char		*buf;
	bool		wait_thread = false;
	int		ret;

	/* Truncate file, allocate resources for doing IO. */
	fd = open(fname, O_DIRECT | O_RDWR | O_TRUNC | O_CREAT, 0644);
	if (fd < 0) {
		perror(fname);
		return -1;
	}

	ret = fstat(fd, &sbuf);
	if (ret) {
		perror(fname);
		goto out;
	}
	blocksize = sbuf.st_blksize;

	ret = posix_memalign((void **)&buf, blocksize, blocksize);
	if (ret) {
		errno = ret;
		perror("buffer");
		goto out;
	}
	memset(buf, 'X', blocksize);
	memset(&event, 0, sizeof(event));

	ret = io_queue_init(1, &ioctx);
	if (ret) {
		errno = -ret;
		perror("io_queue_init");
		goto out_buf;
	}

	/*
	 * Set ourselves up to race fallocate(0..blocksize) with aio dio
	 * pwrite(blocksize..blocksize * 2).  This /should/ give us a file
	 * with length (2 * blocksize).
	 */
	io_prep_pwrite(&iocb, fd, buf, blocksize, blocksize);

	ret = pthread_create(&thread, NULL, falloc_thread, NULL);
	if (ret) {
		errno = ret;
		perror("pthread");
		goto out_io;
	}
	wait_thread = true;

	ret = io_submit(ioctx, 1, &iocbp);
	if (ret != 1) {
		errno = -ret;
		perror("io_submit");
		goto out_join;
	}

	ret = io_getevents(ioctx, 1, 1, &event, NULL);
	if (ret != 1) {
		errno = -ret;
		perror("io_getevents");
		goto out_join;
	}

	if (event.res < 0) {
		errno = -event.res;
		perror("io_event.res");
		goto out_join;
	}

	if (event.res2 < 0) {
		errno = -event.res2;
		perror("io_event.res2");
		goto out_join;
	}

	wait_thread = false;
	ret = pthread_join(thread, NULL);
	if (ret) {
		errno = ret;
		perror("join");
		goto out_io;
	}

	/* Make sure we actually got a file of size (2 * blocksize). */
	ret = fstat(fd, &sbuf);
	if (ret) {
		perror(fname);
		goto out_buf;
	}

	if (sbuf.st_size != 2 * blocksize) {
		fprintf(stderr, "[%u]: sbuf.st_size=%llu, expected %llu.\n",
				iteration,
				(unsigned long long)sbuf.st_size,
				(unsigned long long)2 * blocksize);
	} else {
		printf("[%u]: passed.\n", iteration);
		(*passed)++;
	}

out_join:
	if (wait_thread) {
		ret = pthread_join(thread, NULL);
		if (ret) {
			errno = ret;
			perror("join");
			goto out_io;
		}
	}
out_io:
	ret = io_queue_release(ioctx);
	if (ret) {
		errno = -ret;
		perror("io_queue_release");
	}

out_buf:
	free(buf);
out:
	ret = close(fd);
	fd = -1;
	if (ret) {
		perror("close");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int		ret;
	long		l;
	unsigned int	i;
	unsigned int	passed = 0;

	if (argc != 3) {
		printf("Usage: %s filename iterations\n", argv[0]);
		return 1;
	}

	errno = 0;
	l = strtol(argv[2], NULL, 0);
	if (errno) {
		perror(argv[2]);
		return 1;
	}
	if (l < 1 || l > UINT_MAX) {
		fprintf(stderr, "%ld: must be between 1 and %u.\n",
				l, UINT_MAX);
		return 1;
	}

	for (i = 0; i < l; i++) {
		ret = test(argv[1], i, &passed);
		if (ret)
			return 1;
	}

	printf("pass rate: %u/%u (%.2f%%)\n", passed, i, 100.0 * passed / i);

	return 0;
}
