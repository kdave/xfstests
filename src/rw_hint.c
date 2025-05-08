// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Christoph Hellwig
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	uint64_t hint = -1;
	int fd;

	if (argc < 3) {
		fprintf(stderr,
"usage: %s file not_set|none|short|medium|long|extreme\n",
			argv[0]);
		return 1;
	}

	if (!strcmp(argv[2], "not_set"))
		hint = RWH_WRITE_LIFE_NOT_SET;
	else if (!strcmp(argv[2], "none"))
		hint = RWH_WRITE_LIFE_NONE;
	else if (!strcmp(argv[2], "short"))
		hint = RWH_WRITE_LIFE_SHORT;
	else if (!strcmp(argv[2], "medium"))
		hint = RWH_WRITE_LIFE_MEDIUM;
	else if (!strcmp(argv[2], "long"))
		hint = RWH_WRITE_LIFE_LONG;
	else if (!strcmp(argv[2], "extreme"))
		hint = RWH_WRITE_LIFE_EXTREME;

	if (hint == -1) {
		fprintf(stderr, "invalid hint %s\n", argv[2]);
		return 1;
	}

	fd = open(argv[1], O_WRONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}
	if (fcntl(fd, F_SET_RW_HINT, &hint))
		perror("fcntl");
	return 0;
}
