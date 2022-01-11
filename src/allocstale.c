// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 *
 * Test program to try to trip over XFS_IOC_ALLOCSP mapping stale disk blocks
 * into a file.
 */
#include <xfs/xfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#ifndef XFS_IOC_ALLOCSP
# define XFS_IOC_ALLOCSP	_IOW ('X', 10, struct xfs_flock64)
#endif

int
main(
	int		argc,
	char		*argv[])
{
	struct stat	sb;
	char		*buf, *zeroes;
	unsigned long	i;
	unsigned long	iterations;
	int		fd, ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s filename iterations\n", argv[0]);
		return 1;
	}

	errno = 0;
	iterations = strtoul(argv[2], NULL, 0);
	if (errno) {
		perror(argv[2]);
		return 1;
	}

	fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}

	ret = fstat(fd, &sb);
	if (ret) {
		perror(argv[1]);
		return 1;
	}

	buf = malloc(sb.st_blksize);
	if (!buf) {
		perror("pread buffer");
		return 1;
	}

	zeroes = calloc(1, sb.st_blksize);
	if (!zeroes) {
		perror("zeroes buffer");
		return 1;
	}

	for (i = 1; i <= iterations; i++) {
		struct xfs_flock64	arg = { };
		ssize_t			read_bytes;
		off_t			offset = sb.st_blksize * i;

		/* Ensure the last block of the file is a hole... */
		ret = ftruncate(fd, offset - 1);
		if (ret) {
			perror("truncate");
			return 1;
		}

		/*
		 * ...then use ALLOCSP to allocate the last block in the file.
		 * An unpatched kernel neglects to mark the new mapping
		 * unwritten or to zero the ondisk block, so...
		 */
		arg.l_whence = SEEK_SET;
		arg.l_start = offset;
		ret = ioctl(fd, XFS_IOC_ALLOCSP, &arg);
		if (ret < 0) {
			perror("ioctl");
			return 1;
		}

		/* ... we can read old disk contents here. */
		read_bytes = pread(fd, buf, sb.st_blksize,
						offset - sb.st_blksize);
		if (read_bytes < 0) {
			perror(argv[1]);
			return 1;
		}
		if (read_bytes != sb.st_blksize) {
			fprintf(stderr, "%s: short read of %zd bytes\n",
					argv[1], read_bytes);
			return 1;
		}

		if (memcmp(zeroes, buf, sb.st_blksize) != 0) {
			fprintf(stderr, "%s: found junk near offset %zd!\n",
					argv[1], offset - sb.st_blksize);
			return 2;
		}
	}

	return 0;
}
