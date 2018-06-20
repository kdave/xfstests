// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Christoph Hellwig.  All Rights Reserved.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <libaio.h>

#ifndef FICLONE
#define FICLONE		_IOW(0x94, 9, int)
#endif

#define IO_PATTERN	0xab

int main(int argc, char *argv[])
{
	struct io_context *ctx = NULL;
	struct io_event evs[4];
	struct iocb iocb1, iocb2, iocb3, iocb4;
	struct iocb *iocbs[] = { &iocb1, &iocb2, &iocb3, &iocb4 };
	void *buf;
	int fd, clone, err = 0;
	unsigned long buf_size = getpagesize() * 2;
	char *filename, *clonename;

	if (argc != 3) {
		fprintf(stderr, "usage: %s filename clonename\n", argv[0]);
		exit(1);
	}

	err = posix_memalign(&buf, getpagesize(), buf_size);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(err), "posix_memalign");
		return 1;
	}
	memset(buf, IO_PATTERN, buf_size);

	filename = argv[1];
	fd = open(filename, O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	if (write(fd, buf, buf_size) != buf_size) {
		perror("write");
		return 1;
	}

	clonename = argv[2];
	clone = open(clonename, O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (clone == -1) {
		perror("open clone");
		return 1;
	}

	if (ioctl(clone, FICLONE, fd)) {
		perror("FICLONE");
		return 1;
	}

	err = io_setup(4, &ctx);
	if (err) {
		fprintf(stderr, "io_setup error: %s\n", strerror(err));
		return 1;
	}

	/*
	 * Test overlapping aio writes, where an earlier one clears the whole
	 * range of a later aio, thus leaving nothing to do for the I/O
	 * completion to do.  To make things harder also make sure there is
	 * other outstanding COW I/O.
	 */
	io_prep_pwrite(&iocb1, fd, buf, buf_size, 0);
	io_prep_pwrite(&iocb2, fd, buf, buf_size / 2, 0);
	io_prep_pwrite(&iocb3, fd, buf, buf_size / 2, buf_size);
	io_prep_pwrite(&iocb4, fd, buf, buf_size, 0);

	err = io_submit(ctx, 4, iocbs);
	if (err != 4) {
		fprintf(stderr, "io_submit error: %s\n", strerror(err));
		return 1;
	}

	err = io_getevents(ctx, 4, 4, evs, NULL);
	if (err != 4) {
		fprintf(stderr, "io_getevents error: %s\n", strerror(err));
		return 1;
	}

	return 0;
}
