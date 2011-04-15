/*
 * Copyright (c) 2010 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
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
