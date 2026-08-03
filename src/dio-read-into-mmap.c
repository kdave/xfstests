// SPDX-License-Identifier: GPL-2.0
// Copyright (c) SUSE S.A.

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>

static int read_source_fd = -1;
static int mmap_dest_fd = -1;
static void *buf = NULL;
static int iosize = 4 * 1024 * 1024;

static void usage()
{
	fprintf(stderr,
	"Usage: dio-read-into-mmap <read_source> <mmap_dest>\n");
}

int main(int argc, char **argv)
{
	int ret = -EINVAL;
	const int pagesize = sysconf(_SC_PAGESIZE);
	unsigned int cur = 0;

	if (argc != 3) {
		usage();
		goto error;
	}
	if (iosize < pagesize) {
		ret = -EINVAL;
		fprintf(stderr, "blocksize smaller than pagesize\n");
		goto error;
	}

	read_source_fd = open(argv[1], O_RDONLY | O_DIRECT, 0600);
	if (read_source_fd < 0) {
		ret = -errno;
		fprintf(stderr, "failed to open '%s': %m", argv[1]);
		goto error;
	}
	mmap_dest_fd = open(argv[2], O_RDWR, 0600);
	if (mmap_dest_fd < 0) {
		ret = -errno;
		fprintf(stderr, "failed to open '%s': %m", argv[2]);
		goto error;
	}
	buf = mmap(NULL, iosize, PROT_WRITE, MAP_SHARED, mmap_dest_fd, 0);
	if (buf == MAP_FAILED) {
		buf = NULL;
		fprintf(stderr, "failed to mmap: %m");
		return -errno;
	}
	while (cur < iosize) {
		ret = pread(read_source_fd, buf + cur, iosize - cur, cur);
		if (ret == 0) {
			ret = -EINVAL;
			fprintf(stderr, "reached EOF");
			goto error;
		}
		if (ret < 0) {
			ret = -errno;
			fprintf(stderr, "failed to read: %m");
			goto error;
		}
		cur += ret;
	}
error:
	close(read_source_fd);
	close(mmap_dest_fd);
	if (buf)
		munmap(buf, iosize);
	if (ret < 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
