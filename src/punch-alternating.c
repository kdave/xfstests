/*
 * Punch out every other block in a file.
 * Copyright (C) 2016 Oracle.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"

void usage(char *cmd)
{
	printf("Usage: %s [-o offset] [-i interval] [-s size] file\n", cmd);
	printf("Punches every other block in the file by default,\n");
	printf("or 'size' blocks every 'interval' blocks starting at\n");
	printf("'offset'.  Units are in fstatfs blocks.\n");
	exit(1);
}

/* Compute the file allocation unit size for an XFS file. */
static int detect_xfs_alloc_unit(int fd)
{
	struct fsxattr fsx;
	struct xfs_fsop_geom fsgeom;
	int ret;

	ret = ioctl(fd, XFS_IOC_FSGEOMETRY, &fsgeom);
	if (ret)
		return -1;

	ret = ioctl(fd, XFS_IOC_FSGETXATTR, &fsx);
	if (ret)
		return -1;

	ret = fsgeom.blocksize;
	if (fsx.fsx_xflags & XFS_XFLAG_REALTIME)
		ret *= fsgeom.rtextsize;

	return ret;
}

int main(int argc, char *argv[])
{
	struct stat	s;
	struct statfs	sf;
	off_t		offset;
	off_t		start_offset = 0;
	int		fd;
	blksize_t	blksz;
	off_t		sz;
	int		mode;
	int		error;
	int		c;
	int		size = 1;	/* punch $SIZE blocks ... */
	int		interval = 2;	/* every $INTERVAL blocks */

	while ((c = getopt(argc, argv, "i:o:s:")) != EOF) {
		switch (c) {
		case 'i':
			interval = atoi(optarg);
			break;
		case 'o':
			errno = 0;
			start_offset = strtoull(optarg, NULL, 0);
			if (errno) {
				fprintf(stderr, "invalid offset '%s'\n", optarg);
				return 1;
			}
			break;
		case 's':
			size = atoi(optarg);
			break;
		default:
			usage(argv[0]);
		}
	}

	if (interval <= 0) {
		printf("interval must be > 0\n");
		usage(argv[0]);
	}

	if (size <= 0) {
		printf("size must be > 0\n");
		usage(argv[0]);
	}

	if (optind != argc - 1)
		usage(argv[0]);

	fd = open(argv[optind], O_WRONLY);
	if (fd < 0)
		goto err;

	error = fstat(fd, &s);
	if (error)
		goto err;

	error = fstatfs(fd, &sf);
	if (error)
		goto err;

	sz = s.st_size;
	c = detect_xfs_alloc_unit(fd);
	if (c > 0)
		blksz = c;
	else
		blksz = sf.f_bsize;

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	for (offset = start_offset * blksz;
	     offset < sz;
	     offset += blksz * interval) {
		error = fallocate(fd, mode, offset, blksz * size);
		if (error)
			goto err;
	}

	error = fsync(fd);
	if (error)
		goto err;

	error = close(fd);
	if (error)
		goto err;
	return 0;
err:
	perror(argv[optind]);
	return 2;
}
