// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Christoph Hellwig
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include "statx.h"

static int min_dio_alignment(const char *mntpnt, const char *devname)
{
	struct statx stx = { };
	struct stat st;
	int fd;

	/*
	 * If the file system supports STATX_DIOALIGN, use the dio_offset_align
	 * member, as that reports exactly the information that we are asking
	 * for.
	 *
	 * STATX_DIOALIGN is only reported on regular files, so use O_TMPFILE
	 * to create one without leaving a trace.
	 */
	fd = open(mntpnt, O_TMPFILE | O_RDWR | O_EXCL, 0600);
	if (fd >= 0 &&
	    xfstests_statx(fd, "", AT_EMPTY_PATH, STATX_DIOALIGN, &stx) == 0 &&
	    (stx.stx_mask & STATX_DIOALIGN))
		return stx.stx_dio_offset_align;

	/*
	 * If we are on a block device and no explicit aligned is reported, use
	 * the logical block size as a guestimate.
	 */
	if (stat(devname, &st) == 0 && S_ISBLK(st.st_mode)) {
		int dev_fd = open(devname, O_RDONLY);
		int logical_block_size;

		if (dev_fd > 0 &&
		    fstat(dev_fd, &st) == 0 &&
		    S_ISBLK(st.st_mode) &&
		    ioctl(dev_fd, BLKSSZGET, &logical_block_size)) {
			return logical_block_size;
		}
	}

	/*
	 * No support for STATX_DIOALIGN and not a block device:
	 * default to PAGE_SIZE.
	 */
	return getpagesize();
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s mountpoint devicename\n", argv[0]);
		exit(1);
	}

	printf("%d\n", min_dio_alignment(argv[1], argv[2]));
	exit(0);
}
