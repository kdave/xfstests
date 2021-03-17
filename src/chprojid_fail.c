// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 *
 * Regression test for failing to undo delalloc quota reservations when
 * changing project id and we fail some other FSSETXATTR validation.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/fs.h>
#include "global.h"

static char zerobuf[65536];

int
main(
	int		argc,
	char		*argv[])
{
	struct fsxattr	fa;
	ssize_t		sz;
	int		fd, ret;

	if (argc < 2) {
		printf("Usage: %s filename\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_CREAT | O_TRUNC | O_RDWR, 0600);
	if (fd < 0) {
		perror(argv[1]);
		return 2;
	}

	/* Zero the project id and the extent size hint. */
	ret = ioctl(fd, FS_IOC_FSGETXATTR, &fa);
	if (ret) {
		perror("FSGETXATTR check file");
		return 2;
	}

	if (fa.fsx_projid != 0 || fa.fsx_extsize != 0) {
		fa.fsx_projid = 0;
		fa.fsx_extsize = 0;
		ret = ioctl(fd, FS_IOC_FSSETXATTR, &fa);
		if (ret) {
			perror("FSSETXATTR zeroing");
			return 2;
		}
	}

	/* Dirty a few kb of a file to create delalloc extents. */
	sz = write(fd, zerobuf, sizeof(zerobuf));
	if (sz != sizeof(zerobuf)) {
		perror("delalloc write");
		return 2;
	}

	/*
	 * The regression we're trying to test happens when the fsxattr input
	 * validation decides to bail out after the chown quota reservation has
	 * been made on a file containing delalloc extents.  Extent size hints
	 * can't be set on non-empty files and we can't check the value until
	 * we've reserved resources and taken the file's ILOCK, so this is a
	 * perfect vector for triggering this condition.  In this way we set up
	 * a FSSETXATTR call that will fail.
	 */
	ret = ioctl(fd, FS_IOC_FSGETXATTR, &fa);
	if (ret) {
		perror("FSGETXATTR");
		return 2;
	}

	fa.fsx_projid = 23652;
	fa.fsx_extsize = 2;
	fa.fsx_xflags |= FS_XFLAG_EXTSIZE;

	ret = ioctl(fd, FS_IOC_FSSETXATTR, &fa);
	if (ret) {
		printf("FSSETXATTRR should fail: %s\n", strerror(errno));
		return 0;
	}

	/* Uhoh, that FSSETXATTR call should have failed! */
	return 3;
}
