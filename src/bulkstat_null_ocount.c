// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 *
 * Ensure the kernel returns the new lastip even when ocount is null.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <xfs/xfs.h>

static void die(const char *tag)
{
	perror(tag);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct xfs_bstat	bstat;
	__u64			last;
	struct xfs_fsop_bulkreq bulkreq = {
		.lastip		= &last,
		.icount		= 1,
		.ubuffer	= &bstat,
		.ocount		= NULL,
	};
	int ret;
	int fd;

	fd = open(argv[1], O_RDONLY);
	if (fd < 0)
		die(argv[1]);

	last = 0;
	ret = ioctl(fd, XFS_IOC_FSINUMBERS, &bulkreq);
	if (ret)
		die("inumbers");

	if (last == 0)
		printf("inumbers last = %llu\n", (unsigned long long)last);

	last = 0;
	ret = ioctl(fd, XFS_IOC_FSBULKSTAT, &bulkreq);
	if (ret)
		die("bulkstat");

	if (last == 0)
		printf("bulkstat last = %llu\n", (unsigned long long)last);

	ret = close(fd);
	if (ret)
		die("close");

	return 0;
}
