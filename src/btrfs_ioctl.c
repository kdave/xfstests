// SPDX-License-Identifier: GPL-2.0
// Copyright (c) SUSE S.A.

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <uuid/uuid.h>
#include <linux/btrfs.h>

#define subvol_info_printf_u64(info, name)	\
	printf(#name ": %llu\n", (info).name)

#define subvol_info_printf_timestamp(info, name)	\
	printf(#name ": %llu.%09u\n", (info).name.sec, (info).name.nsec)

#define subvol_info_printf_uuid(info, name)	\
{						\
	char uuidbuf[UUID_STR_LEN];		\
						\
	uuid_unparse((info).name, uuidbuf);	\
	printf(#name ": %s\n", uuidbuf);	\
}

void get_subvol_info(int fd)
{
	struct btrfs_ioctl_get_subvol_info_args info = { 0 };
	int ret;

	ret = ioctl(fd, BTRFS_IOC_GET_SUBVOL_INFO, &info);
	if (ret < 0) {
		fprintf(stderr, "ioctl failed: %m\n");
		return;
	}

	subvol_info_printf_u64(info, treeid);
	printf("name: %.*s\n", BTRFS_VOL_NAME_MAX, info.name);
	subvol_info_printf_u64(info, parent_id);
	subvol_info_printf_u64(info, dirid);
	subvol_info_printf_u64(info, generation);
	printf("flags: 0x%llx\n", info.flags);
	subvol_info_printf_uuid(info, uuid);
	subvol_info_printf_uuid(info, parent_uuid);
	subvol_info_printf_uuid(info, received_uuid);
	subvol_info_printf_u64(info, ctransid);
	subvol_info_printf_u64(info, otransid);
	subvol_info_printf_u64(info, stransid);
	subvol_info_printf_u64(info, rtransid);
	subvol_info_printf_timestamp(info, ctime);
	subvol_info_printf_timestamp(info, otime);
	subvol_info_printf_timestamp(info, stime);
	subvol_info_printf_timestamp(info, rtime);
}

const struct ioctl {
	const char *name;
	void (*func)(int fd);
} supported_ioctls[] = {
	{
		.name = "get_subvol_info",
		.func = get_subvol_info,
	},
};

static void usage()
{
	fprintf(stderr, "Usage: btrfs_ioctl <ioctl_name> <path>\n");
}

int main(int argc, char **argv)
{
	const char *ioctl_name;
	int done = 0;
	int fd;
	int ret = 0;

	if (argc != 3) {
		usage();
		return 1;
	}
	fd = open(argv[2], O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %m\n", argv[2]);
		return 1;
	}
	ioctl_name = argv[1];

	for (int i = 0; i < sizeof(supported_ioctls) / sizeof(supported_ioctls[0]); i++) {
		const struct ioctl *ioctl = &supported_ioctls[i];

		if (!strncasecmp(ioctl->name, ioctl_name, strlen(ioctl->name))) {
			ioctl->func(fd);
			done = 1;
			break;
		}
	}
	if (!done) {
		fprintf(stderr, "ioctl \"%s\" is not recognized\n", ioctl_name);
		ret = 1;
	}
	close(fd);
	return ret;
}
