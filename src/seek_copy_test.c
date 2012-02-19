/*
 * Copyright (C) 2011 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#ifndef SEEK_DATA
#define SEEK_DATA	3
#define SEEK_HOLE	4
#endif

#define BUF_SIZE	4096
#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

static void
error(const char *fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	fprintf(stderr, "ERROR: [%s:%d] %s:%s\n", __func__, __LINE__,
		buf, strerror(errno));
}

static size_t
full_write(int fd, const void *buf, size_t count)
{
	size_t total = 0;
	const char *ptr = (const char *) buf;

	while (count > 0) {
		ssize_t n = write(fd, ptr, count);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			error("failed as %s", strerror(errno));
			break;
		}

		if (n == 0) {
			error("%zu bytes transferred. Aborting.",
			       total);
			break;
		}

		total += n;
		ptr += n;
		count -= n;
	}

	return total;
}

/*
 * Copy a data extent from source file to dest file.
 * @data_off: data offset
 * @hole_off: hole offset
 * The length of this extent is (hole_off - data_off).
 */
static int
do_extent_copy(int src_fd, int dest_fd, off_t data_off, off_t hole_off)
{
	uint64_t len = (uint64_t)(hole_off - data_off);
	char buf[BUF_SIZE];
	int ret;

	/* Seek to data_off for data reading */
	ret = lseek(src_fd, data_off, SEEK_SET);
	if (ret < 0) {
		error("seek source file to %llu failed as %s",
		       (uint64_t)data_off, strerror(errno));
		return ret;
	}

	/* Seek to data_off for data writing, make holes as well */
	ret = lseek(dest_fd, data_off, SEEK_SET);
	if (ret < 0) {
		error("seek dest file to %llu failed as %s",
		       (uint64_t)data_off, strerror(errno));
		return ret;
	}

	while (len > 0) {
		ssize_t nr_read = read(src_fd, buf, BUF_SIZE);
		if (nr_read < 0) {
			if (errno == EINTR)
				continue;
			error("read source file extent failed as %s",
			      strerror(errno));
			ret = -1;
			break;
		}

		if (nr_read == 0) {
			error("reached EOF");
			break;
		}

		if (full_write(dest_fd, buf, nr_read) != nr_read) {
			error("write data to dest file failed as %s",
			       strerror(errno));
			ret = -1;
			break;
		}

		len -= nr_read;
	}

	return ret;
}

/*
 * If lseek(2) failed and the errno is set to ENXIO, for
 * SEEK_DATA there are no more data regions past the supplied
 * offset.  For SEEK_HOLE, there are no more holes past the
 * supplied offset.  Set scan->hit_final_extent to true for
 * either case.
 */
static int
copy_extents(int src_fd, int dest_fd, off_t src_total_size)
{
	int ret = 0;
	off_t seek_start = 0;
	off_t dest_pos = 0;
	off_t data_pos, hole_pos;

	do {
		data_pos = lseek(src_fd, seek_start, SEEK_DATA);
		if (data_pos < 0) {
			if (errno == ENXIO)
				ret = 0;
			else {
				error("SEEK_DATA failed due to %s",
				       strerror(errno));
				ret = -1;
			}
			break;
		}

		hole_pos = lseek(src_fd, data_pos, SEEK_HOLE);
		if (hole_pos < 0) {
			if (errno == ENXIO)
				ret = 0;
			else {
				error("SEEK_HOLE failed due to %s\n",
				       strerror(errno));
				ret = -1;
			}
			break;
		}

		/* do extent copy */
		ret = do_extent_copy(src_fd, dest_fd, data_pos, hole_pos);
		if (ret < 0) {
			error("copy extent failed");
			break;
		}

		dest_pos += (hole_pos - data_pos);
		seek_start = hole_pos;
	} while (seek_start < src_total_size);

	if (dest_pos < src_total_size) {
		ret = ftruncate(dest_fd, src_total_size);
		if (ret < 0) {
			error("truncate dest file to %lld bytes failed as %s",
			      (long long)src_total_size, strerror(errno));
		}
	}

	return ret;
}

int
main(int argc, char **argv)
{
	int ret = 0;
	int src_fd;
	int dest_fd;
	struct stat st;
	size_t src_total_size;

	if (argc != 3) {
		fprintf(stdout, "Usage: %s source dest\n", argv[0]);
		return 1;
	}

	src_fd = open(argv[1], O_RDONLY, 0644);
	if (src_fd < 0) {
		error("create %s failed", argv[1]);
		return -1;
	}

	dest_fd = open(argv[2], O_RDWR|O_CREAT|O_EXCL, 0644);
	if (dest_fd < 0) {
		error("create %s failed", argv[2]);
		ret = -errno;
		goto close_src_fd;
	}

	ret = fstat(src_fd, &st);
	if (ret < 0) {
		error("get file %s staticis failed", argv[1]);
		ret = -errno;
		goto close_dest_fd;
	}

	src_total_size = st.st_size;
	ret = copy_extents(src_fd, dest_fd, src_total_size);
	if (ret < 0)
		error("extents copy failed");

close_dest_fd:
	close(dest_fd);
close_src_fd:
	close(src_fd);

	return ret;
}
