// SPDX-License-Identifier: GPL-2.0
// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "global.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/types.h>
#ifdef HAVE_STRUCT_BTRFS_IOCTL_VOL_ARGS_V2
#include <linux/btrfs.h>
#else
#ifndef BTRFS_IOCTL_MAGIC
#define BTRFS_IOCTL_MAGIC 0x94
#endif

#ifndef BTRFS_IOC_SNAP_CREATE_V2
#define BTRFS_IOC_SNAP_CREATE_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 23, struct btrfs_ioctl_vol_args_v2)
#endif

#ifndef BTRFS_IOC_SUBVOL_CREATE_V2
#define BTRFS_IOC_SUBVOL_CREATE_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 24, struct btrfs_ioctl_vol_args_v2)
#endif

#ifndef BTRFS_SUBVOL_NAME_MAX
#define BTRFS_SUBVOL_NAME_MAX 4039
#endif

struct btrfs_ioctl_vol_args_v2 {
	__s64 fd;
	__u64 transid;
	__u64 flags;
	union {
		struct {
			__u64 size;
			struct btrfs_qgroup_inherit *qgroup_inherit;
		};
		__u64 unused[4];
	};
	union {
		char name[BTRFS_SUBVOL_NAME_MAX + 1];
		__u64 devid;
		__u64 subvolid;
	};
};
#endif

#if !HAVE_DECL_BTRFS_IOC_SNAP_DESTROY_V2
#define BTRFS_IOC_SNAP_DESTROY_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 63, struct btrfs_ioctl_vol_args_v2)
#endif

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s PATH\n", argv[0]);
		return EXIT_FAILURE;
	}

	int dirfd = open(argv[1], O_RDONLY | O_DIRECTORY);
	if (dirfd < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	struct btrfs_ioctl_vol_args_v2 subvol_args = {};
	strcpy(subvol_args.name, "subvol");
	if (ioctl(dirfd, BTRFS_IOC_SUBVOL_CREATE_V2, &subvol_args) < 0) {
		perror("BTRFS_IOC_SUBVOL_CREATE_V2");
		return EXIT_FAILURE;
	}

	int subvolfd = openat(dirfd, "subvol", O_RDONLY | O_DIRECTORY);
	if (subvolfd < 0) {
		perror("openat");
		return EXIT_FAILURE;
	}

	if (ioctl(dirfd, BTRFS_IOC_SNAP_DESTROY_V2, &subvol_args) < 0) {
		perror("BTRFS_IOC_SNAP_DESTROY_V2");
		return EXIT_FAILURE;
	}

	struct btrfs_ioctl_vol_args_v2 snap_args = { .fd = subvolfd };
	strcpy(snap_args.name, "snap");
	if (ioctl(dirfd, BTRFS_IOC_SNAP_CREATE_V2, &snap_args) < 0) {
		if (errno == ENOENT)
			return EXIT_SUCCESS;
		perror("BTRFS_IOC_SNAP_CREATE_V2");
		return EXIT_FAILURE;
	}
	fprintf(stderr, "BTRFS_IOC_SNAP_CREATE_V2 should've failed\n");
	return EXIT_FAILURE;
}
