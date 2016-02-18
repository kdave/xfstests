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

	if (argc != 2) {
		printf("Usage: %s file\n", argv[0]);
		printf("Punches every other block in the file.\n");
		return 1;
	}

	fd = open(argv[1], O_WRONLY);
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
	for (offset = 0; offset < sz; offset += blksz * 2) {
		error = fallocate(fd, mode, offset, blksz);
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
	perror(argv[1]);
	return 2;
}
