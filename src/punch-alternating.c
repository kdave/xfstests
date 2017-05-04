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
	printf("Usage: %s [-i interval] [-s size] file\n", cmd);
	printf("Punches every other block in the file by default,\n");
	printf("or 'size' blocks every 'interval' blocks with options.\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	struct stat	s;
	struct statfs	sf;
	off_t		offset;
	int		fd;
	blksize_t	blksz;
	off_t		sz;
	int		mode;
	int		error;
	int		c;
	int		size = 1;	/* punch $SIZE blocks ... */
	int		interval = 2;	/* every $INTERVAL blocks */

	while ((c = getopt(argc, argv, "i:s:")) != EOF) {
		switch (c) {
		case 'i':
			interval = atoi(optarg);
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
	blksz = sf.f_bsize;

	mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;
	for (offset = 0; offset < sz; offset += blksz * interval) {
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
