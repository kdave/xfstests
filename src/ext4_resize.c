// SPDX-License-Identifier: GPL-2.0

/*
 * Test program which uses the raw ext4 resize_fs ioctl directly.
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

typedef unsigned long long __u64;

#ifndef EXT4_IOC_RESIZE_FS
#define EXT4_IOC_RESIZE_FS		_IOW('f', 16, __u64)
#endif

int main(int argc, char **argv)
{
	__u64	new_size;
	int	error, fd;
	char	*tmp = NULL;

	if (argc != 3) {
		fputs("insufficient arguments\n", stderr);
		return 1;
	}
	fd = open(argv[1], O_RDONLY);
	if (!fd) {
		perror(argv[1]);
		return 1;
	}

	errno = 0;
	new_size = strtoull(argv[2], &tmp, 10);
	if ((errno) || (*tmp != '\0')) {
		fprintf(stderr, "%s: invalid new size\n", argv[0]);
		return 1;
	}

	error = ioctl(fd, EXT4_IOC_RESIZE_FS, &new_size);
	if (error < 0) {
		perror("EXT4_IOC_RESIZE_FS");
		return 1;
	}
	return 0;
}
