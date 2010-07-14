/*
 * Read from a sparse file immedialy after filling a hole to test for races
 * in unwritten extent conversion.
 *
 * Copyright (C) 2010 Red Hat, Inc. All Rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libaio.h>

#define BUF_SIZE	4096
#define IO_PATTERN	0xab

int main(int argc, char *argv[])
{
	struct io_context *ctx = NULL;
	struct io_event ev;
	struct iocb iocb, *iocbs[] = { &iocb };
	void *buf;
	char cmp_buf[BUF_SIZE];
	int fd, err = 0;

	fd = open(argv[1], O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	err = posix_memalign(&buf, BUF_SIZE, BUF_SIZE);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(-err),
			"posix_memalign");
		return 1;
	}
	memset(buf, IO_PATTERN, BUF_SIZE);
	memset(cmp_buf, IO_PATTERN, BUF_SIZE);

	/*
	 * Truncate to some random large file size.  Just make sure
	 * it's not smaller than our I/O size.
	 */
	if (ftruncate(fd, 1024 * 1024 * 1024) < 0) {
		perror("ftruncate");
		return 1;
	}


	/*
	 * Do a simple 4k write into a hole using aio.
	 */
	err = io_setup(1, &ctx);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(-err),
			"io_setup");
		return 1;
	}

	io_prep_pwrite(&iocb, fd, buf, BUF_SIZE, 0);

	err = io_submit(ctx, 1, iocbs);
	if (err != 1) {
		fprintf(stderr, "error %s during %s\n",
			strerror(-err),
			"io_submit");
		return 1;
	}

	err = io_getevents(ctx, 1, 1, &ev, NULL);
	if (err != 1) {
		fprintf(stderr, "error %s during %s\n",
			strerror(-err),
			"io_getevents");
		return 1;
	}

	/*
	 * And then read it back.
	 *
	 * Using pread to keep it simple, but AIO has the same effect.
	 */
	if (pread(fd, buf, BUF_SIZE, 0) != BUF_SIZE) {
		perror("pread");
		return 1;
	}

	/*
	 * And depending on the machine we'll just get zeroes back quite
	 * often here.  That's because the unwritten extent conversion
	 * hasn't finished.
	 */
	if (memcmp(buf, cmp_buf, BUF_SIZE)) {
		unsigned long long *ubuf = (unsigned long long *)buf;
		int i;

		for (i = 0; i < BUF_SIZE / sizeof(unsigned long long); i++)
			printf("%d: 0x%llx\n", i, ubuf[i]);
		return 1;
	}

	return 0;
}
