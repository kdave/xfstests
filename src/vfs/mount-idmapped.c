// SPDX-License-Identifier: GPL-2.0
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../global.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/sched.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "missing.h"
#include "utils.h"

static struct list active_map;

static int parse_map(char *map)
{
	char types[2] = {'u', 'g'};
	int ret, i;
	__u32 id_host, id_ns, range;
	char which;

	if (!map)
		return -1;

	ret = sscanf(map, "%c:%u:%u:%u", &which, &id_ns, &id_host, &range);
	if (ret != 4)
		return -1;

	if (which != 'b' && which != 'u' && which != 'g')
		return -1;

	for (i = 0; i < 2; i++) {
		idmap_type_t map_type;

		if (which != types[i] && which != 'b')
			continue;

		if (types[i] == 'u')
			map_type = ID_TYPE_UID;
		else
			map_type = ID_TYPE_GID;

		ret = add_map_entry(&active_map, id_host, id_ns, range, map_type);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static inline bool strnequal(const char *str, const char *eq, size_t len)
{
	return strncmp(str, eq, len) == 0;
}

static void usage(void)
{
	const char *text = "\
mount-idmapped --map-mount=<idmap> <source> <target>\n\
\n\
Create an idmapped mount of <source> at <target>\n\
Options:\n\
  --map-mount=<idmap>\n\
	Specify an idmap for the <target> mount in the format\n\
	<idmap-type>:<id-from>:<id-to>:<id-range>\n\
	The <idmap-type> can be:\n\
	\"b\" or \"both\"	-> map both uids and gids\n\
	\"u\" or \"uid\"	-> map uids\n\
	\"g\" or \"gid\"	-> map gids\n\
	For example, specifying:\n\
	both:1000:1001:1	-> map uid and gid 1000 to uid and gid 1001 in <target> and no other ids\n\
	uid:20000:100000:1000	-> map uid 20000 to uid 100000, uid 20001 to uid 100001 [...] in <target>\n\
	Currently up to 340 separate idmappings may be specified.\n\n\
  --map-mount=/proc/<pid>/ns/user\n\
	Specify a path to a user namespace whose idmap is to be used.\n\n\
  --recursive\n\
	Copy the whole mount tree from <source> and apply the idmap to everyone at <target>.\n\n\
Examples:\n\
  - Create an idmapped mount of /source on /target with both ('b') uids and gids mapped:\n\
	mount-idmapped --map-mount b:0:10000:10000 /source /target\n\n\
  - Create an idmapped mount of /source on /target with uids ('u') and gids ('g') mapped separately:\n\
	mount-idmapped --map-mount u:0:10000:10000 g:0:20000:20000 /source /target\n\n\
";
	fprintf(stderr, "%s", text);
	_exit(EXIT_SUCCESS);
}

#define exit_usage(format, ...)                         \
	({                                              \
		fprintf(stderr, format, ##__VA_ARGS__); \
		usage();                                \
	})

#define exit_log(format, ...)                           \
	({                                              \
		fprintf(stderr, format, ##__VA_ARGS__); \
		exit(EXIT_FAILURE);                     \
	})

static const struct option longopts[] = {
	{"map-mount",	required_argument,	0,	'a'},
	{"help",	no_argument,		0,	'c'},
	{"recursive",	no_argument,		0,	'd'},
	{ NULL,		0,			0,	0  },
};

int main(int argc, char *argv[])
{
	int fd_userns = -EBADF;
	int index = 0;
	const char *source = NULL, *target = NULL;
	bool recursive = false;
	int fd_tree, new_argc, ret;
	char *const *new_argv;

	list_init(&active_map);
	while ((ret = getopt_long_only(argc, argv, "", longopts, &index)) != -1) {
		switch (ret) {
		case 'a':
			if (strnequal(optarg, "/proc/", STRLITERALLEN("/proc/"))) {
				fd_userns = open(optarg, O_RDONLY | O_CLOEXEC);
				if (fd_userns < 0)
					exit_log("%m - Failed top open user namespace path %s\n", optarg);
				break;
			}

			ret = parse_map(optarg);
			if (ret < 0)
				exit_log("Failed to parse idmaps for mount\n");
			break;
		case 'd':
			recursive = true;
			break;
		case 'c':
			/* fallthrough */
		default:
			usage();
		}
	}

	new_argv = &argv[optind];
	new_argc = argc - optind;
	if (new_argc < 2)
		exit_usage("Missing source or target mountpoint\n\n");
	source = new_argv[0];
	target = new_argv[1];

	fd_tree = sys_open_tree(-EBADF, source,
			        OPEN_TREE_CLONE |
			        OPEN_TREE_CLOEXEC |
			        AT_EMPTY_PATH |
			        (recursive ? AT_RECURSIVE : 0));
	if (fd_tree < 0) {
		exit_log("%m - Failed to open %s\n", source);
		exit(EXIT_FAILURE);
	}

	if (!list_empty(&active_map) || fd_userns >= 0) {
		struct mount_attr attr = {
			.attr_set = MOUNT_ATTR_IDMAP,
		};

		if (fd_userns >= 0)
			attr.userns_fd = fd_userns;
		else
			attr.userns_fd = get_userns_fd_from_idmap(&active_map);
		if (attr.userns_fd < 0)
			exit_log("%m - Failed to create user namespace\n");

		ret = sys_mount_setattr(fd_tree, "", AT_EMPTY_PATH | AT_RECURSIVE,
					&attr, sizeof(attr));
		if (ret < 0)
			exit_log("%m - Failed to change mount attributes\n");
		close(attr.userns_fd);
	}

	ret = sys_move_mount(fd_tree, "", -EBADF, target,
			     MOVE_MOUNT_F_EMPTY_PATH);
	if (ret < 0)
		exit_log("%m - Failed to attach mount to %s\n", target);
	close(fd_tree);

	exit(EXIT_SUCCESS);
}
