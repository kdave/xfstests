/*
 * Copyright (c) 2004 Silicon Graphics, Inc.
 * All Rights Reserved.
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

/*
 * Test program that uses the same interfaces as mkfs.xfs for
 * Linux, dumps out just the device size values from a driver.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

#ifndef BLKGETSIZE64
# define BLKGETSIZE64   _IOR(0x12,114,size_t)
#endif

int main(int argc, char **argv)
{
	__uint64_t	size;
	long long	sz = -1;
	int		error, fd;

	if (argc != 2) {
		fputs("insufficient arguments\n", stderr);
		return 1;
	}
	fd = open(argv[1], O_RDONLY);
	if (!fd) {
		perror(argv[1]);
		return 1;
	}

	error = ioctl(fd, BLKGETSIZE64, &size);
	if (error >= 0) {
		/* BLKGETSIZE64 returns size in bytes not 512-byte blocks */
		sz = (long long)(size >> 9);
		printf("%lld 512 byte blocks (BLKGETSIZE64)\n", sz);
	} else {
		/* If BLKGETSIZE64 fails, try BLKGETSIZE */
		unsigned long tmpsize;

		error = ioctl(fd, BLKGETSIZE, &tmpsize);
		if (error < 0) {
			fprintf(stderr, "can't determine device size");
			return 1;
		}
		sz = (long long)tmpsize;
		printf("%lld 512 byte blocks (BLKGETSIZE)\n", sz);
	}
	return 0;
}
