// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 Google
 * All Rights Reserved.
 *
 * checkpoint_journal.c
 *
 * Flush journal log and checkpoint journal for ext4 file system and
 * optionally, issue discard or zeroout for the journal log blocks.
 *
 * Arguments:
 * 1) mount point for device
 * 2) flags (optional)
 *	set --erase=discard to enable discarding journal blocks
 *	set --erase=zeroout to enable zero-filling journal blocks
 *	set --dry-run flag to only perform input checking
 */

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/fs.h>
#include <getopt.h>

#if !defined(EXT4_IOC_CHECKPOINT)
#define EXT4_IOC_CHECKPOINT	_IOW('f', 43, __u32)
#endif

#if !defined(EXT4_IOC_CHECKPOINT_FLAG_DISCARD)
#define EXT4_IOC_CHECKPOINT_FLAG_DISCARD		1
#define EXT4_IOC_CHECKPOINT_FLAG_ZEROOUT		2
#define EXT4_IOC_CHECKPOINT_FLAG_DRY_RUN		4
#endif

int main(int argc, char** argv) {
	int fd, c, ret = 0, option_index = 0;
	char* rpath;
	unsigned int flags = 0;

	static struct option long_options[] =
	{
		{"dry-run", no_argument, 0, 'd'},
		{"erase", required_argument, 0, 'e'},
		{0, 0, 0, 0}
	};

	/* get optional flags */
	while ((c = getopt_long(argc, argv, "de:", long_options,
				&option_index)) != -1) {
		switch (c) {
		case 'd':
			flags |= EXT4_IOC_CHECKPOINT_FLAG_DRY_RUN;
			break;
		case 'e':
			if (strcmp(optarg, "discard") == 0) {
				flags |= EXT4_IOC_CHECKPOINT_FLAG_DISCARD;
			} else if (strcmp(optarg, "zeroout") == 0) {
				flags |= EXT4_IOC_CHECKPOINT_FLAG_ZEROOUT;
			} else {
				fprintf(stderr, "Error: invalid erase option\n");
				return 1;
			}
			break;
		default:
			return 1;
		}
	}

	if (optind != argc - 1) {
		fprintf(stderr, "Error: invalid number of arguments\n");
		return 1;
	}

	/* get fd to file system */
	rpath = realpath(argv[optind], NULL);
	fd = open(rpath, O_RDONLY);
	free(rpath);

	if (fd == -1) {
		fprintf(stderr, "Error: unable to open device %s: %s\n",
			argv[optind], strerror(errno));
		return 1;
	}

	ret = ioctl(fd, EXT4_IOC_CHECKPOINT, &flags);

	if (ret)
		fprintf(stderr, "checkpoint ioctl returned error: %s\n", strerror(errno));

	close(fd);
	return ret;
}

