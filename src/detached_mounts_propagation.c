/* SPDX-License-Identifier: LGPL-2.1+ */
/*
 * Copyright (c) 2021 Christian Brauner <christian.brauner@ubuntu.com>
 * All Rights Reserved.
 *
 * Regression test to verify that creating a series of detached mounts,
 * attaching them to the filesystem, and unmounting them does not trigger an
 * integer overflow in ns->mounts causing the kernel to block any new mounts in
 * count_mounts() and returning ENOSPC because it falsely assumes that the
 * maximum number of mounts in the mount namespace has been reached, i.e. it
 * thinks it can't fit the new mounts into the mount namespace anymore.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "vfs/missing.h"

static bool is_shared_mountpoint(const char *path)
{
	bool shared = false;
	FILE *f = NULL;
	char *line = NULL;
	int i;
	size_t len = 0;

	f = fopen("/proc/self/mountinfo", "re");
	if (!f)
		return 0;

	while (getline(&line, &len, f) > 0) {
		char *slider1, *slider2;

		for (slider1 = line, i = 0; slider1 && i < 4; i++)
			slider1 = strchr(slider1 + 1, ' ');

		if (!slider1)
			continue;

		slider2 = strchr(slider1 + 1, ' ');
		if (!slider2)
			continue;

		*slider2 = '\0';
		if (strcmp(slider1 + 1, path) == 0) {
			/* This is the path. Is it shared? */
			slider1 = strchr(slider2 + 1, ' ');
			if (slider1 && strstr(slider1, "shared:")) {
				shared = true;
				break;
			}
		}
	}
	fclose(f);
	free(line);

	return shared;
}

static void usage(void)
{
	const char *text = "mount-new [--recursive] <base-dir>\n";
	fprintf(stderr, "%s", text);
	_exit(EXIT_SUCCESS);
}

#define exit_usage(format, ...)                              \
	({                                                   \
		fprintf(stderr, format "\n", ##__VA_ARGS__); \
		usage();                                     \
	})

#define exit_log(format, ...)                                \
	({                                                   \
		fprintf(stderr, format "\n", ##__VA_ARGS__); \
		exit(EXIT_FAILURE);                          \
	})

static const struct option longopts[] = {
	{"help",	no_argument,		0,	'a'},
	{ NULL,		no_argument,		0,	 0 },
};

int main(int argc, char *argv[])
{
	int exit_code = EXIT_SUCCESS, index = 0;
	int dfd, fd_tree, new_argc, ret, i;
	char *base_dir;
	char *const *new_argv;
	char target[PATH_MAX];

	while ((ret = getopt_long_only(argc, argv, "", longopts, &index)) != -1) {
		switch (ret) {
		case 'a':
			/* fallthrough */
		default:
			usage();
		}
	}

	new_argv = &argv[optind];
	new_argc = argc - optind;
	if (new_argc < 1)
		exit_usage("Missing base directory\n");
	base_dir = new_argv[0];

	if (*base_dir != '/')
		exit_log("Please specify an absolute path");

	/* Ensure that target is a shared mountpoint. */
	if (!is_shared_mountpoint(base_dir))
		exit_log("Please ensure that \"%s\" is a shared mountpoint", base_dir);

	ret = unshare(CLONE_NEWNS);
	if (ret < 0)
		exit_log("%m - Failed to create new mount namespace");

	ret = mount(NULL, base_dir, NULL, MS_REC | MS_SHARED, NULL);
	if (ret < 0)
		exit_log("%m - Failed to make base_dir shared mountpoint");

	dfd = open(base_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dfd < 0)
		exit_log("%m - Failed to open base directory \"%s\"", base_dir);

	ret = mkdirat(dfd, "detached-move-mount", 0755);
	if (ret < 0)
		exit_log("%m - Failed to create required temporary directories");

	ret = snprintf(target, sizeof(target), "%s/detached-move-mount", base_dir);
	if (ret < 0 || (size_t)ret >= sizeof(target))
		exit_log("%m - Failed to assemble target path");

	/*
	 * Having a mount table with 10000 mounts is already quite excessive
	 * and shoult account even for weird test systems.
	 */
	for (i = 0; i < 10000; i++) {
		fd_tree = sys_open_tree(dfd, "detached-move-mount",
					OPEN_TREE_CLONE |
					OPEN_TREE_CLOEXEC |
					AT_EMPTY_PATH);
		if (fd_tree < 0) {
			if (errno == ENOSYS) /* New mount API not (fully) supported. */
				break;

			fprintf(stderr, "%m - Failed to open %d(detached-move-mount)", dfd);
			exit_code = EXIT_FAILURE;
			break;
		}

		ret = sys_move_mount(fd_tree, "", dfd, "detached-move-mount", MOVE_MOUNT_F_EMPTY_PATH);
		if (ret < 0) {
			if (errno == ENOSPC)
				fprintf(stderr, "%m - Buggy mount counting");
			else if (errno == ENOSYS) /* New mount API not (fully) supported. */
				break;
			else
				fprintf(stderr, "%m - Failed to attach mount to %d(detached-move-mount)", dfd);
			exit_code = EXIT_FAILURE;
			break;
		}
		close(fd_tree);

		ret = umount2(target, MNT_DETACH);
		if (ret < 0) {
			fprintf(stderr, "%m - Failed to unmount %s", target);
			exit_code = EXIT_FAILURE;
			break;
		}
	}

	(void)unlinkat(dfd, "detached-move-mount", AT_REMOVEDIR);
	close(dfd);

	exit(exit_code);
}
