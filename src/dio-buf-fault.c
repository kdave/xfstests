// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 Meta Platforms, Inc.  All Rights Reserved.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* to get definition of O_DIRECT flag. */
#endif

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

/*
 * mmap a source file, then do a direct write of that mmapped region to a
 * destination file.
 */

int prep_mmap_buffer(char *src_filename, void **addr)
{
	struct stat st;
	int fd;
	int ret;

	fd = open(src_filename, O_RDWR, 0666);
	if (fd == -1)
		err(1, "failed to open %s", src_filename);

	ret = fstat(fd, &st);
	if (ret)
		err(1, "failed to stat %d", fd);

	*addr = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (*addr == MAP_FAILED)
		err(1, "failed to mmap %d", fd);

	return st.st_size;
}

int do_dio(char *dst_filename, void *buf, size_t sz)
{
	int fd;
	ssize_t ret;

	fd = open(dst_filename, O_CREAT | O_TRUNC | O_WRONLY | O_DIRECT, 0666);
	if (fd == -1)
		err(1, "failed to open %s", dst_filename);
	while (sz) {
		ret = write(fd, buf, sz);
		if (ret < 0) {
			if (errno == -EINTR)
				continue;
			else
				err(1, "failed to write %lu bytes to %d", sz, fd);
		} else if (ret == 0) {
			break;
		}
		buf += ret;
		sz -= ret;
	}
	return sz;
}

int main(int argc, char *argv[]) {
	size_t sz;
	void *buf = NULL;
	char c;

	if (argc != 3)
		errx(1, "no in and out file name arguments given");
	sz = prep_mmap_buffer(argv[1], &buf);

	/* touch the first page of the mapping to bring it into cache */
	c = ((char *)buf)[0];
	printf("%u\n", c);

	do_dio(argv[2], buf, sz);
}
