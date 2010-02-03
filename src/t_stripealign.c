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

#define FIBMAP          _IO(0x00, 1)    /* bmap access */

/*
 * If only filename given, print first block.
 *
 * If filename & sunit (in blocks) given, print whether we are well-aligned
 */

int main(int argc, char ** argv)
{
	int	fd;
	int	ret;
	int	sunit = 0;	/* in blocks */
	char	*filename;
	unsigned int	block = 0;

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

	ret = ioctl(fd, FIBMAP, &block);
	if (ret < 0) {
		close(fd);
		perror("fibmap");
		return 1;
	}

	close(fd);

	if (block % sunit) {
		printf("%s: Start block %u not multiple of sunit %u\n",
			filename, block, sunit);
		return 1;
	} else
		printf("%s: well-aligned\n", filename);

	return 0;
}
