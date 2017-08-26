/*
 * Directly AIO re-write a file with different content again and again.
 * And check the data integrity.
 *
 * Copyright (C) 2017 Red Hat, Inc. All Rights reserved.
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
unsigned long loop_count = 0;
void *test_buf[2];
void *cmp_buf;
#define IO_PATTERN1	0x55
#define IO_PATTERN2	0xaa

void usage(char *progname)
{
	fprintf(stderr, "usage: %s [-c loop_count] [-b bufsize] filename\n"
	        "\t-c loopcount: specify how many times to test"
	        "\t-b bufsize: keep writing from offset 0 to this size",
	        progname);
	exit(1);
}

void dump_buffer(void *buf, off64_t offset, ssize_t len)
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
				fprintf(stderr, "*\n");
			new = 1;
		}

		if (!new) {
			p += 16;
			continue;
		}

		fprintf(stderr, "%08llx  ", (unsigned long long)offset + i);
		for (j = 0; j < 16 && i + j < len; j++, p++)
			fprintf(stderr, "%02x ", *p);
		fprintf(stderr, " ");
		for (j = 0; j < 16 && i + j < len; j++, s++) {
			if (isalnum((int)*s))
				fprintf(stderr, "%c", *s);
			else
				fprintf(stderr, ".");
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "%08llx\n", (unsigned long long)offset + i);
}

int init_test(char *filename)
{
	int fd;
	int err = 0;

	fd = open(filename, O_DIRECT | O_CREAT | O_RDWR, 0600);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	ftruncate(fd, buf_size);

	/* fill test_buf[0] with IO_PATTERN1 */
	err = posix_memalign(&(test_buf[0]), getpagesize(), buf_size);
	if (err) {
		fprintf(stderr, "error %s during posix_memalign\n",
		        strerror(err));
		exit(1);
	}
	memset(test_buf[0], IO_PATTERN1, buf_size);

	/* fill test_buf[1] with IO_PATTERN2 */
	err = posix_memalign(&(test_buf[1]), getpagesize(), buf_size);
	if (err) {
		fprintf(stderr, "error %s during posix_memalign\n",
		        strerror(err));
		exit(1);
	}
	memset(test_buf[1], IO_PATTERN2, buf_size);

	/* fill test file with IO_PATTERN1 */
	if (pwrite(fd, test_buf[0], buf_size, 0) != buf_size) {
		perror("pwrite");
		exit(1);
	}

	/* cmp_buf is used to compare with test_buf */
	err = posix_memalign(&cmp_buf, getpagesize(), buf_size);
	if (err) {
		fprintf(stderr, "error %s during posix_memalign\n",
		        strerror(err));
		exit(1);
	}
	memset(cmp_buf, 0, buf_size);

	fsync(fd);
	close(fd);

	return 0;
}

/*
 * Read file content back, then compare with the 'buf' which
 * point to the original buffer was written to file.
 */
int file_check(char *filename, void *buf)
{
	int fd;
	int rc = 0;

	/* check cached data with buffer read */
	fd = open(filename, O_RDONLY, 0600);
	if (fd == -1) {
		perror("open");
		exit(1);
	}

	if (pread(fd, cmp_buf, buf_size, 0) != buf_size) {
		perror("pread");
		exit(1);
	}

	if (memcmp(buf, cmp_buf, buf_size)) {
		fprintf(stderr, "get stale data from buffer read\n");
		dump_buffer(cmp_buf, 0, buf_size);
		rc++;
	}
	close(fd);

	/*
	 * check on-disk data with direct-IO read
	 */
	fd = open(filename, O_DIRECT|O_RDONLY, 0600);
	if (fd < 0) {
		perror("open with direct-IO");
		exit(1);
	}

	if (pread(fd, cmp_buf, buf_size, 0) != buf_size) {
		perror("pread");
		exit(1);
	}

	if (memcmp(buf, cmp_buf, buf_size)) {
		fprintf(stderr, "get stale data from DIO read\n");
		dump_buffer(cmp_buf, 0, buf_size);
		rc++;
	}

	close(fd);
	return rc;
}

int main(int argc, char *argv[])
{
	struct io_context *ctx = NULL;
	struct io_event evs[1];
	struct iocb iocb1;
	struct iocb *iocbs[] = { &iocb1 };
	int fd, err = 0;
	int i;
	int c;
	char *filename = NULL;

	while ((c = getopt(argc, argv, "c:b:")) != -1) {
		char *endp;

		switch (c) {
		case 'c':	/* the number of testing cycles */
			loop_count = strtol(optarg, &endp, 0);
			break;
		case 'b':	/* buffer size */
			buf_size = strtol(optarg, &endp, 0);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (loop_count == 0)
		loop_count = 1600;
	if (buf_size == 0)	/* default minimum buffer size is 65536 bytes */
		buf_size = 65536;

	if (optind == argc - 1)
		filename = argv[optind];
	else
		usage(argv[0]);

	init_test(filename);

	err = io_setup(1, &ctx);
	if (err) {
		fprintf(stderr, "error %s during io_setup\n",
		        strerror(err));
		return 1;
	}

	i = 0;
	/*
	 * Keep running until loop_count times, fill the file with IO_PATTERN1
	 * or IO_PATTERN2 one by one, then read the file data back to check if
	 * there's stale data.
	 */
	while (loop_count--) {
		i++;
		i %= 2;
		fd = open(filename, O_DIRECT | O_CREAT | O_RDWR, 0600);
		if (fd == -1) {
			perror("open");
			return 1;
		}

		io_prep_pwrite(&iocb1, fd, test_buf[i], buf_size, 0);
		err = io_submit(ctx, 1, iocbs);
		if (err != 1) {
			fprintf(stderr, "error %s during io_submit\n",
				strerror(err));
			return 1;
		}
		err = io_getevents(ctx, 1, 1, evs, NULL);
		if (err != 1) {
			fprintf(stderr, "error %s during io_getevents\n",
				strerror(err));
			return 1;
		}
		close(fd);

		err = file_check(filename, test_buf[i]);
		if (err != 0)
			break;
	}

	return 0;
}
