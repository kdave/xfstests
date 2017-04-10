/* Create an AF_UNIX socket.
 *
 * Copyright (C) 2017 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#define offsetof(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)

int main(int argc, char *argv[])
{
	struct sockaddr_un sun;
	struct stat st;
	size_t len, max;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "Format: %s <socketpath>\n", argv[0]);
		exit(2);
	}

	max = sizeof(sun.sun_path);
	len = strlen(argv[1]);
	if (len >= max) {
		fprintf(stderr, "Filename too long (max %zu)\n", max);
		exit(2);
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, argv[1]);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		perror("bind");
		exit(1);
	}

	if (stat(argv[1], &st)) {
		fprintf(stderr, "Couldn't stat socket after creation: %m\n");
		exit(1);
	}

	exit(0);
}
