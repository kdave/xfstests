/*
 * Check ctime updates when calling futimens without UTIME_OMIT for the
 * mtime entry.
 *
 * Copyright (c) 2009 Eric Blake <ebb9@byu.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

int
main(int argc, char **argv)
{
	int fd = creat ("file", 0600);
	struct stat st1, st2;
	struct timespec t[2] = { { 1000000000, 0 }, { 0, UTIME_OMIT } };

	fstat(fd, &st1);
	sleep(1);
	futimens(fd, t);
	fstat(fd, &st2);

	if (st1.st_ctime == st2.st_ctime)
		printf("failed to update ctime!\n");
	return 0;
}
