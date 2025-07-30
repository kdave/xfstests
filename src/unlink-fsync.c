// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2025 SUSE Linux Products GmbH.  All Rights Reserved.
 */

/*
 * Utility to open an existing file, unlink it while holding an open fd on it
 * and then fsync the file before closing the fd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd;
	int ret;

	if (argc != 2) {
		fprintf(stderr, "Use: %s <file path>\n", argv[0]);
		return 1;
	}

	fd = open(argv[1], O_WRONLY, 0666);
	if (fd == -1) {
		perror("failed to open file");
		exit(1);
	}

	ret = unlink(argv[1]);
	if (ret == -1) {
		perror("unlink failed");
		exit(2);
	}

	ret = fsync(fd);
	if (ret == -1) {
		perror("fsync failed");
		exit(3);
	}

	return 0;
}
