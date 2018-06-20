// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2010 Silicon Graphics, Inc.  All Rights Reserved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

int main(int argc, char **argv)
{
	int buf[1024];
	int fd, i, j;

	fd = open(argv[1], O_RDWR|O_CREAT, 0666);
        for (i = 1; i < 9100; i++) {
		for (j = 0; j < 1024; j++)
			buf[j]  = i | i << 5;

		if (write(fd,buf,253*4*sizeof(int))!= 253*4*sizeof(int)) {
			printf("Write did not return correct amount\n");
			exit(EXIT_FAILURE);
		}

		if ((i % 9) == 0 && i < 9001)
			lseek(fd, 4096 * 110,SEEK_CUR);
	}

	close(fd);
	return 0;
}
