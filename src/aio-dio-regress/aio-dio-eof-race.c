/*
 * Launch 4 sub-block AIOs past EOF and ensure that we don't see
 * corruption from racing sub-block zeroing when they're complete.
 *
 * Copyright (C) 2015 Red Hat, Inc. All Rights reserved.
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
#include <ctype.h>

#include <libaio.h>

/* Sized to allow 4 x 512 AIOs */
#define BUF_SIZE	2048
#define IO_PATTERN	0xab

void
dump_buffer(
	void	*buf,
	off64_t	offset,
	ssize_t	len)
{
	int	i, j;
	char	*p;
	int	new;

	for (i = 0, p = (char *)buf; i < len; i += 16) {
		char    *s = p;

		if (i && !memcmp(p, p - 16, 16)) {
			new = 0;
		} else {
			if (i)
				printf("*\n");
			new = 1;
		}

		if (!new) {
			p += 16;
			continue;
		}

		printf("%08llx  ", (unsigned long long)offset + i);
		for (j = 0; j < 16 && i + j < len; j++, p++)
			printf("%02x ", *p);
		printf(" ");
		for (j = 0; j < 16 && i + j < len; j++, s++) {
			if (isalnum((int)*s))
				printf("%c", *s);
			else
				printf(".");
		}
		printf("\n");

	}
	printf("%08llx\n", (unsigned long long)offset + i);
}

int main(int argc, char *argv[])
{
	struct io_context *ctx = NULL;
	struct io_event evs[4];
	struct iocb iocb1, iocb2, iocb3, iocb4;
	struct iocb *iocbs[] = { &iocb1, &iocb2, &iocb3, &iocb4 };
	void *buf;
	struct stat statbuf;
	char cmp_buf[BUF_SIZE];
	int fd, err = 0;
	off_t eof;

	fd = open(argv[1], O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	err = posix_memalign(&buf, getpagesize(), BUF_SIZE);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(err),
			"posix_memalign");
		return 1;
	}
	memset(cmp_buf, IO_PATTERN, BUF_SIZE);

	err = io_setup(4, &ctx);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(err),
			"io_setup");
		return 1;
	}

	eof = 0;

	/* Keep extending until 8MB (fairly arbitrary) */
	while (eof < 8 * 1024 * 1024) {
		memset(buf, IO_PATTERN, BUF_SIZE);
		fstat(fd, &statbuf);
		eof = statbuf.st_size;

		/*
		 * Split the buffer into multiple I/Os to send a mix of block
		 * aligned/unaligned writes as well as writes that start beyond
		 * the current EOF. This stresses things like inode size
		 * management and stale block zeroing for races and can lead to
		 * data corruption when not handled properly.
		 */
		io_prep_pwrite(&iocb1, fd, buf, BUF_SIZE/4, eof + 0*BUF_SIZE/4);
		io_prep_pwrite(&iocb2, fd, buf, BUF_SIZE/4, eof + 1*BUF_SIZE/4);
		io_prep_pwrite(&iocb3, fd, buf, BUF_SIZE/4, eof + 2*BUF_SIZE/4);
		io_prep_pwrite(&iocb4, fd, buf, BUF_SIZE/4, eof + 3*BUF_SIZE/4);

		err = io_submit(ctx, 4, iocbs);
		if (err != 4) {
			fprintf(stderr, "error %s during %s\n",
				strerror(err),
				"io_submit");
			return 1;
		}

		err = io_getevents(ctx, 4, 4, evs, NULL);
		if (err != 4) {
			fprintf(stderr, "error %s during %s\n",
				strerror(err),
				"io_getevents");
			return 1;
		}

		/*
		 * And then read it back.
		 *
		 * Using pread to keep it simple, but AIO has the same effect.
		 * eof is the prior eof; we just wrote BUF_SIZE more.
		 */
		if (pread(fd, buf, BUF_SIZE, eof) != BUF_SIZE) {
			perror("pread");
			return 1;
		}

		/*
		 * We launched 4 AIOs which, stitched together, should write
		 * a seamless BUF_SIZE worth of IO_PATTERN to the last block.
		 */
		if (memcmp(buf, cmp_buf, BUF_SIZE)) {
			printf("corruption while extending from %ld\n", eof);
			dump_buffer(buf, 0, BUF_SIZE);
			return 1;
		}
	}

	printf("Success, all done.\n");
	return 0;
}
