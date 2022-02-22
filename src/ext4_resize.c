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
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>

typedef unsigned long long __u64;

#ifndef EXT4_IOC_RESIZE_FS
#define EXT4_IOC_RESIZE_FS		_IOW('f', 16, __u64)
#endif

#define pr_error(fmt, ...) do { \
	fprintf (stderr, "ext4_resize.c: " fmt, ##__VA_ARGS__); \
} while (0)

static void usage(void)
{
	fprintf(stdout, "\nUsage: ext4_resize [mnt_path] [new_size(blocks)]\n");
}

int main(int argc, char **argv)
{
	__u64	new_size;
	int	error, fd;
	char	*mnt_dir = NULL, *tmp = NULL;

	if (argc != 3) {
		error = EINVAL;
		pr_error("insufficient arguments\n");
		usage();
		return error;
	}

	mnt_dir = argv[1];

	errno = 0;
	new_size = strtoull(argv[2], &tmp, 10);
	if ((errno) || (*tmp != '\0')) {
		error = errno;
		pr_error("invalid new size\n");
		return error;
	}

	fd = open(mnt_dir, O_RDONLY);
	if (fd < 0) {
		error = errno;
		pr_error("open() failed with error: %s\n", strerror(error));
		return error;
	}

	if(ioctl(fd, EXT4_IOC_RESIZE_FS, &new_size) < 0) {
		error = errno;
		pr_error("EXT4_IOC_RESIZE_FS ioctl() failed with error: %s\n", strerror(error));
		return error;
	}

	return 0;
}
