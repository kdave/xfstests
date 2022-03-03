// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 SUSE Linux Products GmbH.  All Rights Reserved.
 */

/*
 * Create a file with 4 extents, each with a size matching the page size.
 * Then allocate a buffer to read all extents with io_uring, using O_DIRECT and
 * IOCB_NOWAIT. Before doing the read with io_uring, access the first page of
 * the read buffer to fault it in, so that during the read we only trigger page
 * faults when accessing the other pages (mapping to 2nd, 3rd and 4th extents).
 */

/* Get the O_DIRECT definition. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <liburing.h>

int main(int argc, char *argv[])
{
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	struct iovec iovec;
	int fd;
	long pagesize;
	void *write_buf;
	void *read_buf;
	ssize_t ret;
	int i;

	if (argc != 2) {
		fprintf(stderr, "Use: %s <file path>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY | O_DIRECT, 0666);
	if (fd == -1) {
		fprintf(stderr, "Failed to create file %s: %s (errno %d)\n",
			argv[1], strerror(errno), errno);
		return 1;
	}

	pagesize = sysconf(_SC_PAGE_SIZE);
	if (pagesize == -1) {
		fprintf(stderr, "Failed to get page size: %s (errno %d)\n",
			strerror(errno), errno);
		return 1;
	}

	ret = posix_memalign(&write_buf, pagesize, 4 * pagesize);
	if (ret) {
		fprintf(stderr, "Failed to allocate write buffer\n");
		return 1;
	}

	memset(write_buf, 0xab, pagesize);
	memset(write_buf + pagesize, 0xcd, pagesize);
	memset(write_buf + 2 * pagesize, 0xef, pagesize);
	memset(write_buf + 3 * pagesize, 0x73, pagesize);

	/* Create the 4 extents, each with a size matching page size. */
	for (i = 0; i < 4; i++) {
		ret = pwrite(fd, write_buf + i * pagesize, pagesize,
			     i * pagesize);
		if (ret != pagesize) {
			fprintf(stderr,
				"Write failure, ret = %ld errno %d (%s)\n",
				ret, errno, strerror(errno));
			return 1;
		}
		ret = fsync(fd);
		if (ret != 0) {
			fprintf(stderr, "Fsync failure: %s (errno %d)\n",
				strerror(errno), errno);
			return 1;
		}
	}

	close(fd);
	fd = open(argv[1], O_RDONLY | O_DIRECT);
	if (fd == -1) {
		fprintf(stderr,
			"Failed to open file %s: %s (errno %d)",
			argv[1], strerror(errno), errno);
		return 1;
	}

	ret = posix_memalign(&read_buf, pagesize, 4 * pagesize);
	if (ret) {
		fprintf(stderr, "Failed to allocate read buffer\n");
		return 1;
	}

	/*
	 * Fault in only the first page of the read buffer.
	 * We want to trigger a page fault for the 2nd page of the read buffer
	 * during the read operation with io_uring (O_DIRECT and IOCB_NOWAIT).
	 */
	memset(read_buf, 0, 1);

	ret = io_uring_queue_init(1, &ring, 0);
	if (ret != 0) {
		fprintf(stderr, "Failed to creating io_uring queue\n");
		return 1;
	}

	sqe = io_uring_get_sqe(&ring);
	if (!sqe) {
		fprintf(stderr, "Failed to get io_uring sqe\n");
		return 1;
	}

	iovec.iov_base = read_buf;
	iovec.iov_len = 4 * pagesize;
	io_uring_prep_readv(sqe, fd, &iovec, 1, 0);

	ret = io_uring_submit_and_wait(&ring, 1);
	if (ret != 1) {
		fprintf(stderr, "Failed at io_uring_submit_and_wait()\n");
		return 1;
	}

	ret = io_uring_wait_cqe(&ring, &cqe);
	if (ret < 0) {
		fprintf(stderr, "Failed at io_uring_wait_cqe()\n");
		return 1;
	}

	if (cqe->res != (4 * pagesize))
		fprintf(stderr, "Unexpected cqe->res, got %d expected %ld\n",
			cqe->res, 4 * pagesize);

	if (memcmp(read_buf, write_buf, 4 * pagesize) != 0)
		fprintf(stderr,
			"Unexpected mismatch between read and write buffers\n");

	io_uring_cqe_seen(&ring, cqe);
	io_uring_queue_exit(&ring);

	return 0;
}
