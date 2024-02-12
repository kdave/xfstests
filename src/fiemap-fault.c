// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Meta Platforms, Inc.  All Rights Reserved.
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fiemap.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int prep_mmap_buffer(int fd, void **addr)
{
	struct stat st;
	int ret;

	ret = fstat(fd, &st);
	if (ret)
		err(1, "failed to stat %d", fd);

	*addr = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (*addr == MAP_FAILED)
		err(1, "failed to mmap %d", fd);

	return st.st_size;
}

int main(int argc, char *argv[])
{
	struct fiemap *fiemap;
	size_t sz, last = 0;
	void *buf = NULL;
	int ret, fd;

	if (argc != 2)
		errx(1, "no in and out file name arguments given");

	fd = open(argv[1], O_RDWR, 0666);
	if (fd == -1)
		err(1, "failed to open %s", argv[1]);

	sz = prep_mmap_buffer(fd, &buf);

	fiemap = (struct fiemap *)buf;
	fiemap->fm_flags = 0;
	fiemap->fm_extent_count = (sz - sizeof(struct fiemap)) /
				  sizeof(struct fiemap_extent);

	while (last < sz) {
		int i;

		fiemap->fm_start = last;
		fiemap->fm_length = sz - last;

		ret = ioctl(fd, FS_IOC_FIEMAP, (unsigned long)fiemap);
		if (ret < 0)
			err(1, "fiemap failed %d (%s)", errno, strerror(errno));
		for (i = 0; i < fiemap->fm_mapped_extents; i++)
		       last = fiemap->fm_extents[i].fe_logical +
			       fiemap->fm_extents[i].fe_length;
	}

	munmap(buf, sz);
	close(fd);
	return 0;
}
