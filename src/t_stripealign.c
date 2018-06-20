/*
 * t_stripealign.c
 *
 * Print whether the file start block is stripe-aligned.
 *
 * Copyright (c) 2010 Eric Sandeen <sandeen@sandeen.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/fiemap.h>
#include <linux/fs.h>

#ifndef FIEMAP_EXTENT_SHARED
# define FIEMAP_EXTENT_SHARED	0x00002000
#endif

#define FIEMAP_EXTENT_ACCEPTABLE	(FIEMAP_EXTENT_LAST | \
		FIEMAP_EXTENT_DATA_ENCRYPTED | FIEMAP_EXTENT_ENCODED | \
		FIEMAP_EXTENT_UNWRITTEN | FIEMAP_EXTENT_MERGED | \
		FIEMAP_EXTENT_SHARED)

/*
 * If only filename given, print first block.
 *
 * If filename & sunit (in blocks) given, print whether we are well-aligned
 */

int main(int argc, char ** argv)
{
	struct stat		sb;
	struct fiemap		*fie;
	struct fiemap_extent	*fe;
	int			fd;
	int			ret;
	int			sunit = 0;	/* in blocks */
	char			*filename;
	unsigned long long	block;

        if (argc < 3) {
                printf("Usage: %s <filename> <sunit in blocks>\n", argv[0]);
                return 1;
        }

        filename = argv[1];
	sunit = atoi(argv[2]);

        fd = open(filename, O_RDONLY);
        if (fd < 0) {
                perror("can't open file\n");
                return 1;
        }

	ret = fstat(fd, &sb);
	if (ret) {
		perror(filename);
		close(fd);
		return 1;
	}

	fie = calloc(1, sizeof(struct fiemap) + sizeof(struct fiemap_extent));
	if (!fie) {
		close(fd);
		perror("malloc");
		return 1;
	}
	fie->fm_length = 1;
	fie->fm_flags = FIEMAP_FLAG_SYNC;
	fie->fm_extent_count = 1;

	ret = ioctl(fd, FS_IOC_FIEMAP, fie);
	if (ret < 0) {
		unsigned int	bmap = 0;

		ret = ioctl(fd, FIBMAP, &bmap);
		if (ret < 0) {
			perror("fibmap");
			free(fie);
			close(fd);
			return 1;
		}
		block = bmap;
		goto check;
	}


	if (fie->fm_mapped_extents != 1) {
		printf("%s: no extents?\n", filename);
		free(fie);
		close(fd);
		return 1;
	}
	fe = &fie->fm_extents[0];
	if (fe->fe_flags & ~FIEMAP_EXTENT_ACCEPTABLE) {
		printf("%s: bad flags 0x%x\n", filename, fe->fe_flags);
		free(fie);
		close(fd);
		return 1;
	}

	block = fie->fm_extents[0].fe_physical / sb.st_blksize;
check:
	if (block % sunit) {
		printf("%s: Start block %llu not multiple of sunit %u\n",
			filename, block, sunit);
		return 1;
	} else
		printf("%s: well-aligned\n", filename);
	free(fie);
	close(fd);

	return 0;
}
