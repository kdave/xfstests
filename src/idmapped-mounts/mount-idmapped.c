// SPDX-License-Identifier: GPL-2.0
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../global.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <linux/bpf.h>
#include <linux/sched.h>
#include <linux/seccomp.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "missing.h"
#include "utils.h"

/* A few helpful macros. */
#define STRLITERALLEN(x) (sizeof(""x"") - 1)

#define INTTYPE_TO_STRLEN(type)             \
	(2 + (sizeof(type) <= 1             \
		  ? 3                       \
		  : sizeof(type) <= 2       \
			? 5                 \
			: sizeof(type) <= 4 \
			      ? 10          \
			      : sizeof(type) <= 8 ? 20 : sizeof(int[-2 * (sizeof(type) > 8)])))

#define syserror(format, ...)                           \
	({                                              \
		fprintf(stderr, format, ##__VA_ARGS__); \
		(-errno);                               \
	})

#define syserror_set(__ret__, format, ...)                    \
	({                                                    \
		typeof(__ret__) __internal_ret__ = (__ret__); \
		errno = labs(__ret__);                        \
		fprintf(stderr, format, ##__VA_ARGS__);       \
		__internal_ret__;                             \
	})

struct list {
	void *elem;
	struct list *next;
	struct list *prev;
};

#define list_for_each(__iterator, __list) \
	for (__iterator = (__list)->next; __iterator != __list; __iterator = __iterator->next)

static inline void list_init(struct list *list)
{
	list->elem = NULL;
	list->next = list->prev = list;
}

static inline int list_empty(const struct list *list)
{
	return list == list->next;
}

static inline void __list_add(struct list *new, struct list *prev, struct list *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add_tail(struct list *head, struct list *list)
{
	__list_add(list, head->prev, head);
}

typedef enum idmap_type_t {
	ID_TYPE_UID,
	ID_TYPE_GID
} idmap_type_t;

struct id_map {
	idmap_type_t map_type;
	__u32 nsid;
	__u32 hostid;
	__u32 range;
};

static struct list active_map;

static int add_map_entry(__u32 id_host,
			 __u32 id_ns,
			 __u32 range,
			 idmap_type_t map_type)
{
	struct list *new_list = NULL;
	struct id_map *newmap = NULL;

	newmap = malloc(sizeof(*newmap));
	if (!newmap)
		return -ENOMEM;

	new_list = malloc(sizeof(struct list));
	if (!new_list) {
		free(newmap);
		return -ENOMEM;
	}

	*newmap = (struct id_map){
		.hostid		= id_host,
		.nsid		= id_ns,
		.range		= range,
		.map_type	= map_type,
	};

	new_list->elem = newmap;
	list_add_tail(&active_map, new_list);
	return 0;
}

static int parse_map(char *map)
{
	char types[2] = {'u', 'g'};
	int ret;
	__u32 id_host, id_ns, range;
	char which;

	if (!map)
		return -1;

	ret = sscanf(map, "%c:%u:%u:%u", &which, &id_ns, &id_host, &range);
	if (ret != 4)
		return -1;

	if (which != 'b' && which != 'u' && which != 'g')
		return -1;

	for (int i = 0; i < 2; i++) {
		idmap_type_t map_type;

		if (which != types[i] && which != 'b')
			continue;

		if (types[i] == 'u')
			map_type = ID_TYPE_UID;
		else
			map_type = ID_TYPE_GID;

		ret = add_map_entry(id_host, id_ns, range, map_type);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int write_id_mapping(idmap_type_t map_type, pid_t pid, const char *buf, size_t buf_size)
{
	int fd = -EBADF, setgroups_fd = -EBADF;
	int fret = -1;
	int ret;
	char path[STRLITERALLEN("/proc/") + INTTYPE_TO_STRLEN(pid_t) +
		  STRLITERALLEN("/setgroups") + 1];

	if (geteuid() != 0 && map_type == ID_TYPE_GID) {
		ret = snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
		if (ret < 0 || ret >= sizeof(path))
			goto out;

		setgroups_fd = open(path, O_WRONLY | O_CLOEXEC);
		if (setgroups_fd < 0 && errno != ENOENT) {
			syserror("Failed to open \"%s\"", path);
			goto out;
		}

		if (setgroups_fd >= 0) {
			ret = write_nointr(setgroups_fd, "deny\n", STRLITERALLEN("deny\n"));
			if (ret != STRLITERALLEN("deny\n")) {
				syserror("Failed to write \"deny\" to \"/proc/%d/setgroups\"", pid);
				goto out;
			}
		}
	}

	ret = snprintf(path, sizeof(path), "/proc/%d/%cid_map", pid, map_type == ID_TYPE_UID ? 'u' : 'g');
	if (ret < 0 || ret >= sizeof(path))
		goto out;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0) {
		syserror("Failed to open \"%s\"", path);
		goto out;
	}

	ret = write_nointr(fd, buf, buf_size);
	if (ret != buf_size) {
		syserror("Failed to write %cid mapping to \"%s\"",
			 map_type == ID_TYPE_UID ? 'u' : 'g', path);
		goto out;
	}

	fret = 0;
out:
	if (fd >= 0)
		close(fd);
	if (setgroups_fd >= 0)
		close(setgroups_fd);

	return fret;
}

static int map_ids_from_idmap(struct list *idmap, pid_t pid)
{
	int fill, left;
	char mapbuf[4096] = {};
	bool had_entry = false;

	for (idmap_type_t map_type = ID_TYPE_UID, u_or_g = 'u';
	     map_type <= ID_TYPE_GID; map_type++, u_or_g = 'g') {
		char *pos = mapbuf;
		int ret;
		struct list *iterator;


		list_for_each(iterator, idmap) {
			struct id_map *map = iterator->elem;
			if (map->map_type != map_type)
				continue;

			had_entry = true;

			left = 4096 - (pos - mapbuf);
			fill = snprintf(pos, left, "%u %u %u\n", map->nsid, map->hostid, map->range);
			/*
			 * The kernel only takes <= 4k for writes to
			 * /proc/<pid>/{g,u}id_map
			 */
			if (fill <= 0 || fill >= left)
				return syserror_set(-E2BIG, "Too many %cid mappings defined", u_or_g);

			pos += fill;
		}
		if (!had_entry)
			continue;

		ret = write_id_mapping(map_type, pid, mapbuf, pos - mapbuf);
		if (ret < 0)
			return syserror("Failed to write mapping: %s", mapbuf);

		memset(mapbuf, 0, sizeof(mapbuf));
	}

	return 0;
}

static int get_userns_fd_from_idmap(struct list *idmap)
{
	int ret;
	pid_t pid;
	char path_ns[STRLITERALLEN("/proc/") + INTTYPE_TO_STRLEN(pid_t) +
		  STRLITERALLEN("/ns/user") + 1];

	pid = do_clone(get_userns_fd_cb, NULL, CLONE_NEWUSER | CLONE_NEWNS);
	if (pid < 0)
		return -errno;

	ret = map_ids_from_idmap(idmap, pid);
	if (ret < 0)
		return ret;

	ret = snprintf(path_ns, sizeof(path_ns), "/proc/%d/ns/user", pid);
	if (ret < 0 || (size_t)ret >= sizeof(path_ns))
		ret = -EIO;
	else
		ret = open(path_ns, O_RDONLY | O_CLOEXEC | O_NOCTTY);

	(void)kill(pid, SIGKILL);
	(void)wait_for_pid(pid);
	return ret;
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
