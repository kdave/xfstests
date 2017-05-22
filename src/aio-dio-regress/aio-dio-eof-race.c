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

unsigned long buf_size = 0;
unsigned long size_MB = 0;
#define IO_PATTERN	0xab

void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-s filesize] [-b bufsize] filename\n"
	        "\t-s filesize: specify the minimum file size"
	        "\t-b bufsize: buffer size",
	        progname);
	exit(1);
}

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
	int fd, err = 0;
	off_t eof;
	int c;
	char *cmp_buf = NULL;
	char *filename = NULL;

	while ((c = getopt(argc, argv, "s:b:")) != -1) {
		char *endp;

		switch (c) {
		case 's':	/* XXX MB size will be extended */
			size_MB = strtol(optarg, &endp, 0);
			break;
		case 'b':	/* buffer size */
			buf_size = strtol(optarg, &endp, 0);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (size_MB == 0)	/* default size is 8MB */
		size_MB = 8;
	if (buf_size < 2048)	/* default minimum buffer size is 2048 bytes */
		buf_size = 2048;

	if (optind == argc - 1)
		filename = argv[optind];
	else
		usage(argv[0]);



	fd = open(filename, O_DIRECT | O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		return 1;
	}

	err = posix_memalign(&buf, getpagesize(), buf_size);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(err),
			"posix_memalign");
		return 1;
	}
	cmp_buf = malloc(buf_size);
	memset(cmp_buf, IO_PATTERN, buf_size);

	err = io_setup(4, &ctx);
	if (err) {
		fprintf(stderr, "error %s during %s\n",
			strerror(err),
			"io_setup");
		return 1;
	}

	eof = 0;

	/* Keep extending until size_MB */
	while (eof < size_MB * 1024 * 1024) {
		memset(buf, IO_PATTERN, buf_size);
		fstat(fd, &statbuf);
		eof = statbuf.st_size;

		/*
		 * Split the buffer into multiple I/Os to send a mix of block
		 * aligned/unaligned writes as well as writes that start beyond
		 * the current EOF. This stresses things like inode size
		 * management and stale block zeroing for races and can lead to
		 * data corruption when not handled properly.
		 */
		io_prep_pwrite(&iocb1, fd, buf, buf_size/4, eof + 0*buf_size/4);
		io_prep_pwrite(&iocb2, fd, buf, buf_size/4, eof + 1*buf_size/4);
		io_prep_pwrite(&iocb3, fd, buf, buf_size/4, eof + 2*buf_size/4);
		io_prep_pwrite(&iocb4, fd, buf, buf_size/4, eof + 3*buf_size/4);

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
		 * eof is the prior eof; we just wrote buf_size more.
		 */
		if (pread(fd, buf, buf_size, eof) != buf_size) {
			perror("pread");
			return 1;
		}

		/*
		 * We launched 4 AIOs which, stitched together, should write
		 * a seamless buf_size worth of IO_PATTERN to the last block.
		 */
		if (memcmp(buf, cmp_buf, buf_size)) {
			printf("corruption while extending from %lld\n",
			       (long long) eof);
			dump_buffer(buf, 0, buf_size);
			return 1;
		}
	}

	printf("Success, all done.\n");
	return 0;
}
