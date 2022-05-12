// SPDX-License-Identifier: GPL-2.0
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "../global.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <linux/limits.h>
#include <linux/types.h>
#include <pthread.h>
#include <pwd.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#ifdef HAVE_LINUX_BTRFS_H
# ifndef HAVE_STRUCT_BTRFS_IOCTL_VOL_ARGS_V2_SUBVOLID
#  define btrfs_ioctl_vol_args_v2 override_btrfs_ioctl_vol_args_v2
# endif
#include <linux/btrfs.h>
# undef btrfs_ioctl_vol_args_v2
#endif

#ifdef HAVE_LINUX_BTRFS_TREE_H
#include <linux/btrfs_tree.h>
#endif

#include "missing.h"
#include "utils.h"

static char t_buf[PATH_MAX];

#ifndef HAVE_STRUCT_BTRFS_IOCTL_VOL_ARGS

#ifndef BTRFS_PATH_NAME_MAX
#define BTRFS_PATH_NAME_MAX 4087
#endif

struct btrfs_ioctl_vol_args {
	__s64 fd;
	char name[BTRFS_PATH_NAME_MAX + 1];
};
#endif

#ifndef HAVE_STRUCT_BTRFS_QGROUP_LIMIT
struct btrfs_qgroup_limit {
	__u64 flags;
	__u64 max_rfer;
	__u64 max_excl;
	__u64 rsv_rfer;
	__u64 rsv_excl;
};
#endif

#ifndef HAVE_STRUCT_BTRFS_QGROUP_INHERIT
struct btrfs_qgroup_inherit {
	__u64 flags;
	__u64 num_qgroups;
	__u64 num_ref_copies;
	__u64 num_excl_copies;
	struct btrfs_qgroup_limit lim;
	__u64 qgroups[0];
};
#endif

#if !defined(HAVE_STRUCT_BTRFS_IOCTL_VOL_ARGS_V2) || !defined(HAVE_STRUCT_BTRFS_IOCTL_VOL_ARGS_V2_SUBVOLID)

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

#ifndef HAVE_STRUCT_BTRFS_IOCTL_INO_LOOKUP_ARGS

#ifndef BTRFS_INO_LOOKUP_PATH_MAX
#define BTRFS_INO_LOOKUP_PATH_MAX 4080
#endif
struct btrfs_ioctl_ino_lookup_args {
	__u64 treeid;
	__u64 objectid;
	char name[BTRFS_INO_LOOKUP_PATH_MAX];
};
#endif

#ifndef HAVE_STRUCT_BTRFS_IOCTL_INO_LOOKUP_USER_ARGS

#ifndef BTRFS_VOL_NAME_MAX
#define BTRFS_VOL_NAME_MAX 255
#endif

#ifndef BTRFS_INO_LOOKUP_USER_PATH_MAX
#define BTRFS_INO_LOOKUP_USER_PATH_MAX (4080 - BTRFS_VOL_NAME_MAX - 1)
#endif

struct btrfs_ioctl_ino_lookup_user_args {
	__u64 dirid;
	__u64 treeid;
	char name[BTRFS_VOL_NAME_MAX + 1];
	char path[BTRFS_INO_LOOKUP_USER_PATH_MAX];
};
#endif

#ifndef HAVE_STRUCT_BTRFS_IOCTL_GET_SUBVOL_ROOTREF_ARGS

#ifndef BTRFS_MAX_ROOTREF_BUFFER_NUM
#define BTRFS_MAX_ROOTREF_BUFFER_NUM 255
#endif

struct btrfs_ioctl_get_subvol_rootref_args {
	__u64 min_treeid;
	struct {
		__u64 treeid;
		__u64 dirid;
	} rootref[BTRFS_MAX_ROOTREF_BUFFER_NUM];
	__u8 num_items;
	__u8 align[7];
};
#endif

#ifndef BTRFS_IOCTL_MAGIC
#define BTRFS_IOCTL_MAGIC 0x94
#endif

#ifndef BTRFS_IOC_SNAP_DESTROY
#define BTRFS_IOC_SNAP_DESTROY \
	_IOW(BTRFS_IOCTL_MAGIC, 15, struct btrfs_ioctl_vol_args)
#endif

#ifndef BTRFS_IOC_SNAP_DESTROY_V2
#define BTRFS_IOC_SNAP_DESTROY_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 63, struct btrfs_ioctl_vol_args_v2)
#endif

#ifndef BTRFS_IOC_SNAP_CREATE_V2
#define BTRFS_IOC_SNAP_CREATE_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 23, struct btrfs_ioctl_vol_args_v2)
#endif

#ifndef BTRFS_IOC_SUBVOL_CREATE_V2
#define BTRFS_IOC_SUBVOL_CREATE_V2 \
	_IOW(BTRFS_IOCTL_MAGIC, 24, struct btrfs_ioctl_vol_args_v2)
#endif

#ifndef BTRFS_IOC_SUBVOL_GETFLAGS
#define BTRFS_IOC_SUBVOL_GETFLAGS _IOR(BTRFS_IOCTL_MAGIC, 25, __u64)
#endif

#ifndef BTRFS_IOC_SUBVOL_SETFLAGS
#define BTRFS_IOC_SUBVOL_SETFLAGS _IOW(BTRFS_IOCTL_MAGIC, 26, __u64)
#endif

#ifndef BTRFS_IOC_INO_LOOKUP
#define BTRFS_IOC_INO_LOOKUP \
	_IOWR(BTRFS_IOCTL_MAGIC, 18, struct btrfs_ioctl_ino_lookup_args)
#endif

#ifndef BTRFS_IOC_INO_LOOKUP_USER
#define BTRFS_IOC_INO_LOOKUP_USER \
	_IOWR(BTRFS_IOCTL_MAGIC, 62, struct btrfs_ioctl_ino_lookup_user_args)
#endif

#ifndef BTRFS_IOC_GET_SUBVOL_ROOTREF
#define BTRFS_IOC_GET_SUBVOL_ROOTREF \
	_IOWR(BTRFS_IOCTL_MAGIC, 61, struct btrfs_ioctl_get_subvol_rootref_args)
#endif

#ifndef BTRFS_SUBVOL_RDONLY
#define BTRFS_SUBVOL_RDONLY (1ULL << 1)
#endif

#ifndef BTRFS_SUBVOL_SPEC_BY_ID
#define BTRFS_SUBVOL_SPEC_BY_ID (1ULL << 4)
#endif

#ifndef BTRFS_FIRST_FREE_OBJECTID
#define BTRFS_FIRST_FREE_OBJECTID 256ULL
#endif

static int btrfs_delete_subvolume(int parent_fd, const char *name)
{
	struct btrfs_ioctl_vol_args args = {};
	size_t len;
	int ret;

	len = strlen(name);
	if (len >= sizeof(args.name))
		return -ENAMETOOLONG;

	memcpy(args.name, name, len);
	args.name[len] = '\0';

	ret = ioctl(parent_fd, BTRFS_IOC_SNAP_DESTROY, &args);
	if (ret < 0)
		return -1;

	return 0;
}

static int btrfs_delete_subvolume_id(int parent_fd, uint64_t subvolid)
{
	struct btrfs_ioctl_vol_args_v2 args = {};
	int ret;

	args.flags = BTRFS_SUBVOL_SPEC_BY_ID;
	args.subvolid = subvolid;

	ret = ioctl(parent_fd, BTRFS_IOC_SNAP_DESTROY_V2, &args);
	if (ret < 0)
		return -1;

	return 0;
}

static int btrfs_create_subvolume(int parent_fd, const char *name)
{
	struct btrfs_ioctl_vol_args_v2 args = {};
	size_t len;
	int ret;

	len = strlen(name);
	if (len >= sizeof(args.name))
		return -ENAMETOOLONG;

	memcpy(args.name, name, len);
	args.name[len] = '\0';

	ret = ioctl(parent_fd, BTRFS_IOC_SUBVOL_CREATE_V2, &args);
	if (ret < 0)
		return -1;

	return 0;
}

static int btrfs_create_snapshot(int fd, int parent_fd, const char *name,
				 int flags)
{
	struct btrfs_ioctl_vol_args_v2 args = {
		.fd = fd,
	};
	size_t len;
	int ret;

	if (flags & ~BTRFS_SUBVOL_RDONLY)
		return -EINVAL;

	len = strlen(name);
	if (len >= sizeof(args.name))
		return -ENAMETOOLONG;
	memcpy(args.name, name, len);
	args.name[len] = '\0';

	if (flags & BTRFS_SUBVOL_RDONLY)
		args.flags |= BTRFS_SUBVOL_RDONLY;
	ret = ioctl(parent_fd, BTRFS_IOC_SNAP_CREATE_V2, &args);
	if (ret < 0)
		return -1;

	return 0;
}

static int btrfs_get_subvolume_ro(int fd, bool *read_only_ret)
{
	uint64_t flags;
	int ret;

	ret = ioctl(fd, BTRFS_IOC_SUBVOL_GETFLAGS, &flags);
	if (ret < 0)
		return -1;

	*read_only_ret = flags & BTRFS_SUBVOL_RDONLY;
	return 0;
}

static int btrfs_set_subvolume_ro(int fd, bool read_only)
{
	uint64_t flags;
	int ret;

	ret = ioctl(fd, BTRFS_IOC_SUBVOL_GETFLAGS, &flags);
	if (ret < 0)
		return -1;

	if (read_only)
		flags |= BTRFS_SUBVOL_RDONLY;
	else
		flags &= ~BTRFS_SUBVOL_RDONLY;

	ret = ioctl(fd, BTRFS_IOC_SUBVOL_SETFLAGS, &flags);
	if (ret < 0)
		return -1;

	return 0;
}

static int btrfs_get_subvolume_id(int fd, uint64_t *id_ret)
{
	struct btrfs_ioctl_ino_lookup_args args = {
	    .treeid = 0,
	    .objectid = BTRFS_FIRST_FREE_OBJECTID,
	};
	int ret;

	ret = ioctl(fd, BTRFS_IOC_INO_LOOKUP, &args);
	if (ret < 0)
		return -1;

	*id_ret = args.treeid;

	return 0;
}

/*
 * The following helpers are adapted from the btrfsutils library. We can't use
 * the library directly since we need full control over how the subvolume
 * iteration happens. We need to be able to check whether unprivileged
 * subvolume iteration is possible, i.e. whether BTRFS_IOC_INO_LOOKUP_USER is
 * available and also ensure that it is actually used when looking up paths.
 */
struct btrfs_stack {
	uint64_t tree_id;
	struct btrfs_ioctl_get_subvol_rootref_args rootref_args;
	size_t items_pos;
	size_t path_len;
};

struct btrfs_iter {
	int fd;
	int cur_fd;

	struct btrfs_stack *search_stack;
	size_t stack_len;
	size_t stack_capacity;

	char *cur_path;
	size_t cur_path_capacity;
};

static struct btrfs_stack *top_stack_entry(struct btrfs_iter *iter)
{
	return &iter->search_stack[iter->stack_len - 1];
}

static int pop_stack(struct btrfs_iter *iter)
{
	struct btrfs_stack *top, *parent;
	int fd, parent_fd;
	size_t i;

	if (iter->stack_len == 1) {
		iter->stack_len--;
		return 0;
	}

	top = top_stack_entry(iter);
	iter->stack_len--;
	parent = top_stack_entry(iter);

	fd = iter->cur_fd;
	for (i = parent->path_len; i < top->path_len; i++) {
		if (i == 0 || iter->cur_path[i] == '/') {
			parent_fd = openat(fd, "..", O_RDONLY);
			if (fd != iter->cur_fd)
				close(fd);
			if (parent_fd == -1)
				return -1;
			fd = parent_fd;
		}
	}
	if (iter->cur_fd != iter->fd)
		close(iter->cur_fd);
	iter->cur_fd = fd;

	return 0;
}

static int append_stack(struct btrfs_iter *iter, uint64_t tree_id, size_t path_len)
{
	struct btrfs_stack *entry;

	if (iter->stack_len >= iter->stack_capacity) {
		size_t new_capacity = iter->stack_capacity * 2;
		struct btrfs_stack *new_search_stack;
#ifdef HAVE_REALLOCARRAY
		new_search_stack = reallocarray(iter->search_stack, new_capacity,
						sizeof(*iter->search_stack));
#else
		new_search_stack = realloc(iter->search_stack, new_capacity * sizeof(*iter->search_stack));
#endif
		if (!new_search_stack)
			return -ENOMEM;

		iter->stack_capacity = new_capacity;
		iter->search_stack = new_search_stack;
	}

	entry = &iter->search_stack[iter->stack_len];

	memset(entry, 0, sizeof(*entry));
	entry->path_len = path_len;
	entry->tree_id = tree_id;

	if (iter->stack_len) {
		struct btrfs_stack *top;
		char *path;
		int fd;

		top = top_stack_entry(iter);
		path = &iter->cur_path[top->path_len];
		if (*path == '/')
			path++;
		fd = openat(iter->cur_fd, path, O_RDONLY);
		if (fd == -1)
			return -errno;

		close(iter->cur_fd);
		iter->cur_fd = fd;
	}

	iter->stack_len++;

	return 0;
}

static int btrfs_iterator_start(int fd, uint64_t top, struct btrfs_iter **ret)
{
	struct btrfs_iter *iter;
	int err;

	iter = malloc(sizeof(*iter));
	if (!iter)
		return -ENOMEM;

	iter->fd = fd;
	iter->cur_fd = fd;

	iter->stack_len = 0;
	iter->stack_capacity = 4;
	iter->search_stack = malloc(sizeof(*iter->search_stack) *
				    iter->stack_capacity);
	if (!iter->search_stack) {
		err = -ENOMEM;
		goto out_iter;
	}

	iter->cur_path_capacity = 256;
	iter->cur_path = malloc(iter->cur_path_capacity);
	if (!iter->cur_path) {
		err = -ENOMEM;
		goto out_search_stack;
	}

	err = append_stack(iter, top, 0);
	if (err)
		goto out_cur_path;

	*ret = iter;

	return 0;

out_cur_path:
	free(iter->cur_path);
out_search_stack:
	free(iter->search_stack);
out_iter:
	free(iter);
	return err;
}

static void btrfs_iterator_end(struct btrfs_iter *iter)
{
	if (iter) {
		free(iter->cur_path);
		free(iter->search_stack);
		if (iter->cur_fd != iter->fd)
			close(iter->cur_fd);
		close(iter->fd);
		free(iter);
	}
}

static int __append_path(struct btrfs_iter *iter, const char *name,
			 size_t name_len, const char *dir, size_t dir_len,
			 size_t *path_len_ret)
{
	struct btrfs_stack *top = top_stack_entry(iter);
	size_t path_len;
	char *p;

	path_len = top->path_len;
	/*
	 * We need a joining slash if we have a current path and a subdirectory.
	 */
	if (top->path_len && dir_len)
		path_len++;
	path_len += dir_len;
	/*
	 * We need another joining slash if we have a current path and a name,
	 * but not if we have a subdirectory, because the lookup ioctl includes
	 * a trailing slash.
	 */
	if (top->path_len && !dir_len && name_len)
		path_len++;
	path_len += name_len;

	/* We need one extra character for the NUL terminator. */
	if (path_len + 1 > iter->cur_path_capacity) {
		char *tmp = realloc(iter->cur_path, path_len + 1);

		if (!tmp)
			return -ENOMEM;
		iter->cur_path = tmp;
		iter->cur_path_capacity = path_len + 1;
	}

	p = iter->cur_path + top->path_len;
	if (top->path_len && dir_len)
		*p++ = '/';
	memcpy(p, dir, dir_len);
	p += dir_len;
	if (top->path_len && !dir_len && name_len)
		*p++ = '/';
	memcpy(p, name, name_len);
	p += name_len;
	*p = '\0';

	*path_len_ret = path_len;

	return 0;
}

static int get_subvolume_path(struct btrfs_iter *iter, uint64_t treeid,
			      uint64_t dirid, size_t *path_len_ret)
{
	struct btrfs_ioctl_ino_lookup_user_args args = {
		.treeid = treeid,
		.dirid = dirid,
	};
	int ret;

	ret = ioctl(iter->cur_fd, BTRFS_IOC_INO_LOOKUP_USER, &args);
	if (ret == -1)
		return -1;

	return __append_path(iter, args.name, strlen(args.name), args.path,
			     strlen(args.path), path_len_ret);
}

static int btrfs_iterator_next(struct btrfs_iter *iter, char **path_ret,
			       uint64_t *id_ret)
{
	struct btrfs_stack *top;
	uint64_t treeid, dirid;
	size_t path_len;
	int ret, err;

	for (;;) {
		for (;;) {
			if (iter->stack_len == 0)
				return 1;

			top = top_stack_entry(iter);
			if (top->items_pos < top->rootref_args.num_items) {
				break;
			} else {
				ret = ioctl(iter->cur_fd,
					    BTRFS_IOC_GET_SUBVOL_ROOTREF,
					    &top->rootref_args);
				if (ret == -1 && errno != EOVERFLOW)
					return -1;
				top->items_pos = 0;

				if (top->rootref_args.num_items == 0) {
					err = pop_stack(iter);
					if (err)
						return err;
				}
			}
		}

		treeid = top->rootref_args.rootref[top->items_pos].treeid;
		dirid = top->rootref_args.rootref[top->items_pos].dirid;
		top->items_pos++;
		err = get_subvolume_path(iter, treeid, dirid, &path_len);
		if (err) {
			/* Skip the subvolume if we can't access it. */
			if (errno == EACCES)
				continue;
			return err;
		}

		err = append_stack(iter, treeid, path_len);
		if (err) {
			/*
			 * Skip the subvolume if it does not exist (which can
			 * happen if there is another filesystem mounted over a
			 * parent directory) or we don't have permission to
			 * access it.
			 */
			if (errno == ENOENT || errno == EACCES)
				continue;
			return err;
		}

		top = top_stack_entry(iter);
		goto out;
	}

out:
	if (path_ret) {
		*path_ret = malloc(top->path_len + 1);
		if (!*path_ret)
			return -ENOMEM;
		memcpy(*path_ret, iter->cur_path, top->path_len);
		(*path_ret)[top->path_len] = '\0';
	}
	if (id_ret)
		*id_ret = top->tree_id;
	return 0;
}

#define BTRFS_SUBVOLUME1 "subvol1"
#define BTRFS_SUBVOLUME1_SNAPSHOT1 "subvol1_snapshot1"
#define BTRFS_SUBVOLUME1_SNAPSHOT1_RO "subvol1_snapshot1_ro"
#define BTRFS_SUBVOLUME1_RENAME "subvol1_rename"
#define BTRFS_SUBVOLUME2 "subvol2"

static int btrfs_subvolumes_fsids_mapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_up())
			die("failure: raise caps");

		/*
		 * The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must succeed.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: check ownership");

		/* remove subvolume */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: check ownership");

		if (!caps_down())
			die("failure: lower caps");

		/*
		 * The filesystem is not mounted with user_subvol_rm_allowed so
		 * subvolume deletion must fail.
		 */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
		die("failure: check ownership");

	/* remove subvolume */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_fsids_mapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: check ownership");

		/* remove subvolume */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove subvolume */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_fsids_unmapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	/* create directory for rename test */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (fchownat(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	if (!switch_fsids(0, 0)) {
		log_stderr("failure: switch_fsids");
		goto out;
	}

	/*
	 * The caller's fsids don't have a mappings in the idmapped mount so
	 * any file creation must fail.
	 */

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (!btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME2)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* try to rename a subvolume */
	if (!renameat(open_tree_fd, BTRFS_SUBVOLUME1, open_tree_fd,
		       BTRFS_SUBVOLUME1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* The caller is privileged over the inode so file deletion must work. */

	/* remove subvolume */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_fsids_unmapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF, userns_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	/* create directory for rename test */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (fchownat(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	userns_fd = get_userns_fd(0, 30000, 10000);
	if (userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		/*
		 * The caller's fsids don't have a mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (!btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME2))
			die("failure: btrfs_create_subvolume");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/* try to rename a subvolume */
		if (!renameat(open_tree_fd, BTRFS_SUBVOLUME1, open_tree_fd,
					BTRFS_SUBVOLUME1_RENAME))
			die("failure: renameat");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/*
		 * The caller is not privileged over the inode so subvolume
		 * deletion must fail.
		 */

		/* remove subvolume */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove subvolume */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);
	safe_close(userns_fd);

	return fret;
}

static int btrfs_snapshots_fsids_mapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;

		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_up())
			die("failure: raise caps");

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		/* create read-only snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					  BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		safe_close(subvolume_fd);

		/* remove subvolume */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		/* remove read-write snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1))
			die("failure: btrfs_delete_subvolume");

		/* remove read-only snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO))
			die("failure: btrfs_delete_subvolume");

		/* create directory */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		/* create read-only snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					  BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove read-write snapshot */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove read-only snapshot */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_fsids_mapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0))
			die("failure: expected_uid_gid");

		/* create read-only snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					  BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 0, 0))
			die("failure: expected_uid_gid");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
		die("failure: expected_uid_gid");

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000))
		die("failure: expected_uid_gid");

	/* remove read-write snapshot */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 10000, 10000))
		die("failure: expected_uid_gid");

	/* remove read-only snapshot */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_fsids_unmapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory for rename test */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (fchownat(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr,
			      sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;

		if (!switch_fsids(0, 0)) {
			log_stderr("failure: switch_fsids");
			goto out;
		}

		/*
		 * The caller's fsids don't have a mappings in the idmapped
		 * mount so any file creation must fail.
		 */

		/*
		 * The open_tree() syscall returns an O_PATH file descriptor
		 * which we can't use with ioctl(). So let's reopen it as a
		 * proper file descriptor.
		 */
		tree_fd = openat(open_tree_fd, ".",
				 O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (tree_fd < 0)
			die("failure: openat");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create directory */
		if (!btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME2))
			die("failure: btrfs_create_subvolume");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/* create read-write snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/* create read-only snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					   BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/* try to rename a directory */
		if (!renameat(open_tree_fd, BTRFS_SUBVOLUME1, open_tree_fd,
			       BTRFS_SUBVOLUME1_RENAME))
			die("failure: renameat");
		if (errno != EOVERFLOW)
			die("failure: errno");

		if (!caps_down())
			die("failure: caps_down");

		/* create read-write snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");
		if (errno != EPERM)
			die("failure: errno");

		/* create read-only snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					   BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");
		if (errno != EPERM)
			die("failure: errno");

		/*
		 * The caller is not privileged over the inode so subvolume
		 * deletion must fail.
		 */

		/* remove directory */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");
		if (errno != EPERM)
			die("failure: errno");

		if (!caps_up())
			die("failure: caps_down");

		/*
		 * The caller is privileged over the inode so subvolume
		 * deletion must work.
		 */

		/* remove directory */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_fsids_unmapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, subvolume_fd = -EBADF, tree_fd = -EBADF,
	    userns_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory for rename test */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (fchownat(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	userns_fd = get_userns_fd(0, 30000, 10000);
	if (userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr,
			      sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor
	 * which we can't use with ioctl(). So let's reopen it as a
	 * proper file descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".",
			O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
			O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		/*
		 * The caller's fsids don't have a mappings in the idmapped
		 * mount so any file creation must fail.
		 */

		/* create directory */
		if (!btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME2))
			die("failure: btrfs_create_subvolume");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/* create read-write snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");
		if (errno != EPERM)
			die("failure: errno");

		/* create read-only snapshot */
		if (!btrfs_create_snapshot(subvolume_fd, tree_fd,
					   BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					   BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");
		if (errno != EPERM)
			die("failure: errno");

		/* try to rename a directory */
		if (!renameat(open_tree_fd, BTRFS_SUBVOLUME1, open_tree_fd,
			       BTRFS_SUBVOLUME1_RENAME))
			die("failure: renameat");
		if (errno != EOVERFLOW)
			die("failure: errno");

		/*
		 * The caller is not privileged over the inode so subvolume
		 * deletion must fail.
		 */

		/* remove directory */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
		die("failure: btrfs_delete_subvolume");

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(subvolume_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_fsids_mapped_user_subvol_rm_allowed(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_mnt_scratch_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/*
		 * The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must succedd.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: check ownership");

		/*
		 * The scratch device is mounted with user_subvol_rm_allowed so
		 * subvolume deletion must succeed.
		 */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_fsids_mapped_userns_user_subvol_rm_allowed(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_mnt_scratch_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: check ownership");

		/*
		 * The scratch device is mounted with user_subvol_rm_allowed so
		 * subvolume deletion must succeed.
		 */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_fsids_mapped_user_subvol_rm_allowed(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_mnt_scratch_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;

		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/*
		 * The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must succeed.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		/* create read-only snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					  BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		safe_close(subvolume_fd);

		/* remove subvolume */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		/* remove read-write snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1))
			die("failure: btrfs_delete_subvolume");

		/* remove read-only snapshot */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO))
			die("failure: btrfs_delete_subvolume");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (btrfs_set_subvolume_ro(subvolume_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		safe_close(subvolume_fd);

		/* remove read-only snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_fsids_mapped_userns_user_subvol_rm_allowed(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_mnt_scratch_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0))
			die("failure: expected_uid_gid");

		/* create read-only snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
					  BTRFS_SUBVOL_RDONLY))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 0, 0))
			die("failure: expected_uid_gid");

		/* remove directory */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_delete_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0))
			die("failure: expected_uid_gid");

		/* remove read-write snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1))
			die("failure: btrfs_delete_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO, 0, 0, 0))
			die("failure: expected_uid_gid");

		/* remove read-only snapshot */
		if (!btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO))
			die("failure: btrfs_delete_subvolume");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (btrfs_set_subvolume_ro(subvolume_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		safe_close(subvolume_fd);

		/* remove read-only snapshot */
		if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1_RO))
			die("failure: btrfs_delete_subvolume");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_delete_by_spec_id(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, subvolume_fd = -EBADF, tree_fd = -EBADF;
	uint64_t subvolume_id1 = -EINVAL, subvolume_id2 = -EINVAL;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_mnt_scratch_fd, "A")) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_mnt_scratch_fd, "B")) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	subvolume_fd = openat(info->t_mnt_scratch_fd, "B", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(subvolume_fd, "C")) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	safe_close(subvolume_fd);

	subvolume_fd = openat(info->t_mnt_scratch_fd, "A", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fd, &subvolume_id1)) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}

	subvolume_fd = openat(info->t_mnt_scratch_fd, "B/C", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fd, &subvolume_id2)) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}

	if (sys_mount(info->t_device_scratch, info->t_mountpoint, "btrfs", 0, "subvol=B/C")) {
		log_stderr("failure: mount");
		goto out;
	}

	open_tree_fd = sys_open_tree(-EBADF, info->t_mountpoint,
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/*
		 * The subvolume isn't exposed in the idmapped mount so
		 * delation via spec id must fail.
		 */
		if (!btrfs_delete_subvolume_id(tree_fd, subvolume_id1))
			die("failure: btrfs_delete_subvolume_id");
		if (errno != EOPNOTSUPP)
			die("failure: errno");

		if (btrfs_delete_subvolume_id(info->t_mnt_scratch_fd, subvolume_id1))
			die("failure: btrfs_delete_subvolume_id");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);
	sys_umount2(info->t_mountpoint, MNT_DETACH);
	btrfs_delete_subvolume_id(info->t_mnt_scratch_fd, subvolume_id2);
	btrfs_delete_subvolume(info->t_mnt_scratch_fd, "B");

	return fret;
}

static int btrfs_subvolumes_setflags_fsids_mapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;
		bool read_only = false;

		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (btrfs_set_subvolume_ro(subvolume_fd, true))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (!read_only)
			die("failure: not read_only");

		if (btrfs_set_subvolume_ro(subvolume_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_setflags_fsids_mapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;
		bool read_only = false;

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (btrfs_set_subvolume_ro(subvolume_fd, true))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (!read_only)
			die("failure: not read_only");

		if (btrfs_set_subvolume_ro(subvolume_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_setflags_fsids_unmapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;
		bool read_only = false;

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (!switch_fsids(0, 0))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/*
		 * The caller's fsids don't have mappings in the idmapped mount
		 * so any file creation must fail.
		 */

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (!btrfs_set_subvolume_ro(subvolume_fd, true))
			die("failure: btrfs_set_subvolume_ro");
		if (errno != EPERM)
			die("failure: errno");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_subvolumes_setflags_fsids_unmapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF, userns_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	userns_fd = get_userns_fd(0, 30000, 10000);
	if (userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int subvolume_fd = -EBADF;
		bool read_only = false;

		/*
		 * The caller's fsids don't have mappings in the idmapped mount
		 * so any file creation must fail.
		 */

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		if (btrfs_get_subvolume_ro(subvolume_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (!btrfs_set_subvolume_ro(subvolume_fd, true))
			die("failure: btrfs_set_subvolume_ro");
		if (errno != EPERM)
			die("failure: errno");

		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);
	safe_close(userns_fd);

	return fret;
}

static int btrfs_snapshots_setflags_fsids_mapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int snapshot_fd = -EBADF, subvolume_fd = -EBADF;
		bool read_only = false;

		if (!switch_fsids(10000, 10000))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/*
		 * The caller's fsids now have mappings in the idmapped mount
		 * so any file creation must succeed.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000))
			die("failure: expected_uid_gid");

		snapshot_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1,
				     O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (snapshot_fd < 0)
			die("failure: openat");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (btrfs_set_subvolume_ro(snapshot_fd, true))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (!read_only)
			die("failure: not read_only");

		if (btrfs_set_subvolume_ro(snapshot_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		safe_close(snapshot_fd);
		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_setflags_fsids_mapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int snapshot_fd = -EBADF, subvolume_fd = -EBADF;
		bool read_only = false;

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/*
		 * The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must succeed.
		 */

		/* create subvolume */
		if (btrfs_create_subvolume(tree_fd, BTRFS_SUBVOLUME1))
			die("failure: btrfs_create_subvolume");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 0, 0))
			die("failure: expected_uid_gid");

		subvolume_fd = openat(tree_fd, BTRFS_SUBVOLUME1,
				      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (subvolume_fd < 0)
			die("failure: openat");

		/* create read-write snapshot */
		if (btrfs_create_snapshot(subvolume_fd, tree_fd,
					  BTRFS_SUBVOLUME1_SNAPSHOT1, 0))
			die("failure: btrfs_create_snapshot");

		if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0))
			die("failure: expected_uid_gid");

		snapshot_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1,
				     O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (snapshot_fd < 0)
			die("failure: openat");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (btrfs_set_subvolume_ro(snapshot_fd, true))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (!read_only)
			die("failure: not read_only");

		if (btrfs_set_subvolume_ro(snapshot_fd, false))
			die("failure: btrfs_set_subvolume_ro");

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		safe_close(snapshot_fd);
		safe_close(subvolume_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_setflags_fsids_unmapped(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, subvolume_fd = -EBADF, tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	subvolume_fd = openat(info->t_dir1_fd, BTRFS_SUBVOLUME1,
			      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create read-write snapshot */
	if (btrfs_create_snapshot(subvolume_fd, info->t_dir1_fd,
				  BTRFS_SUBVOLUME1_SNAPSHOT1, 0)) {
		log_stderr("failure: btrfs_create_snapshot");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int snapshot_fd = -EBADF;
		bool read_only = false;

		snapshot_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1,
				     O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (snapshot_fd < 0)
			die("failure: openat");

		if (!switch_fsids(0, 0))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: raise caps");

		/*
		 * The caller's fsids don't have mappings in the idmapped mount
		 * so any file creation must fail.
		 */

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (!btrfs_set_subvolume_ro(snapshot_fd, true))
			die("failure: btrfs_set_subvolume_ro");
		if (errno != EPERM)
			die("failure: errno");

		safe_close(snapshot_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(subvolume_fd);
	safe_close(tree_fd);

	return fret;
}

static int btrfs_snapshots_setflags_fsids_unmapped_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF, subvolume_fd = -EBADF, tree_fd = -EBADF,
	    userns_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	userns_fd = get_userns_fd(0, 30000, 10000);
	if (userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(info->t_dir1_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	subvolume_fd = openat(info->t_dir1_fd, BTRFS_SUBVOLUME1,
			      O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create read-write snapshot */
	if (btrfs_create_snapshot(subvolume_fd, info->t_dir1_fd,
				  BTRFS_SUBVOLUME1_SNAPSHOT1, 0)) {
		log_stderr("failure: btrfs_create_snapshot");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int snapshot_fd = -EBADF;
		bool read_only = false;

		/*
		 * The caller's fsids don't have mappings in the idmapped mount
		 * so any file creation must fail.
		 */

		snapshot_fd = openat(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1,
				     O_RDONLY | O_CLOEXEC | O_DIRECTORY);
		if (snapshot_fd < 0)
			die("failure: openat");


		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(info->t_dir1_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd, BTRFS_SUBVOLUME1, 0,
				      info->t_overflowuid, info->t_overflowgid))
			die("failure: expected_uid_gid");

		/*
		 * The caller's fsids don't have mappings in the idmapped mount
		 * so any file creation must fail.
		 */

		if (btrfs_get_subvolume_ro(snapshot_fd, &read_only))
			die("failure: btrfs_get_subvolume_ro");

		if (read_only)
			die("failure: read_only");

		if (!btrfs_set_subvolume_ro(snapshot_fd, true))
			die("failure: btrfs_set_subvolume_ro");
		if (errno != EPERM)
			die("failure: errno");

		safe_close(snapshot_fd);

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	/* remove directory */
	if (btrfs_delete_subvolume(tree_fd, BTRFS_SUBVOLUME1_SNAPSHOT1)) {
		log_stderr("failure: btrfs_delete_subvolume");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);
	safe_close(subvolume_fd);
	safe_close(tree_fd);
	safe_close(userns_fd);

	return fret;
}

#define BTRFS_SUBVOLUME_SUBVOL1 "subvol1"
#define BTRFS_SUBVOLUME_SUBVOL2 "subvol2"
#define BTRFS_SUBVOLUME_SUBVOL3 "subvol3"
#define BTRFS_SUBVOLUME_SUBVOL4 "subvol4"

#define BTRFS_SUBVOLUME_SUBVOL1_ID 0
#define BTRFS_SUBVOLUME_SUBVOL2_ID 1
#define BTRFS_SUBVOLUME_SUBVOL3_ID 2
#define BTRFS_SUBVOLUME_SUBVOL4_ID 3

#define BTRFS_SUBVOLUME_DIR1 "dir1"
#define BTRFS_SUBVOLUME_DIR2 "dir2"

#define BTRFS_SUBVOLUME_MNT "mnt_subvolume1"

#define BTRFS_SUBVOLUME_SUBVOL1xSUBVOL3 "subvol1/subvol3"
#define BTRFS_SUBVOLUME_SUBVOL1xDIR1xDIR2 "subvol1/dir1/dir2"
#define BTRFS_SUBVOLUME_SUBVOL1xDIR1xDIR2xSUBVOL4 "subvol1/dir1/dir2/subvol4"

/*
 * We create the following mount layout to test lookup:
 *
 * |-/mnt/test                    /dev/loop0                   btrfs       rw,relatime,space_cache,subvolid=5,subvol=/
 * | |-/mnt/test/mnt1             /dev/loop1[/subvol1]         btrfs       rw,relatime,space_cache,user_subvol_rm_allowed,subvolid=268,subvol=/subvol1
 * '-/mnt/scratch                 /dev/loop1                   btrfs       rw,relatime,space_cache,user_subvol_rm_allowed,subvolid=5,subvol=/
 */
static int btrfs_subvolume_lookup_user(const struct vfstest_info *info)
{
	int fret = -1, i;
	int dir1_fd = -EBADF, dir2_fd = -EBADF, mnt_fd = -EBADF,
	    open_tree_fd = -EBADF, tree_fd = -EBADF, userns_fd = -EBADF;
	int subvolume_fds[BTRFS_SUBVOLUME_SUBVOL4_ID + 1];
	uint64_t subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID + 1];
	uint64_t subvolid = -EINVAL;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;
	struct btrfs_iter *iter;

	if (!caps_supported())
		return 0;

	for (i = 0; i < ARRAY_SIZE(subvolume_fds); i++)
		subvolume_fds[i] = -EBADF;

	for (i = 0; i < ARRAY_SIZE(subvolume_ids); i++)
		subvolume_ids[i] = -EINVAL;

	if (btrfs_create_subvolume(info->t_mnt_scratch_fd, BTRFS_SUBVOLUME_SUBVOL1)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (btrfs_create_subvolume(info->t_mnt_scratch_fd, BTRFS_SUBVOLUME_SUBVOL2)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID] = openat(info->t_mnt_scratch_fd,
							   BTRFS_SUBVOLUME_SUBVOL1,
							   O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID] < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create subvolume */
	if (btrfs_create_subvolume(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID], BTRFS_SUBVOLUME_SUBVOL3)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (mkdirat(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID], BTRFS_SUBVOLUME_DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir1_fd = openat(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID], BTRFS_SUBVOLUME_DIR1,
			 O_CLOEXEC | O_DIRECTORY);
	if (dir1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (mkdirat(dir1_fd, BTRFS_SUBVOLUME_DIR2, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir2_fd = openat(dir1_fd, BTRFS_SUBVOLUME_DIR2, O_CLOEXEC | O_DIRECTORY);
	if (dir2_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_create_subvolume(dir2_fd, BTRFS_SUBVOLUME_SUBVOL4)) {
		log_stderr("failure: btrfs_create_subvolume");
		goto out;
	}

	if (mkdirat(info->t_mnt_fd, BTRFS_SUBVOLUME_MNT, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	snprintf(t_buf, sizeof(t_buf), "%s/%s", info->t_mountpoint, BTRFS_SUBVOLUME_MNT);
	if (sys_mount(info->t_device_scratch, t_buf, "btrfs", 0,
		      "subvol=" BTRFS_SUBVOLUME_SUBVOL1)) {
		log_stderr("failure: mount");
		goto out;
	}

	mnt_fd = openat(info->t_mnt_fd, BTRFS_SUBVOLUME_MNT, O_CLOEXEC | O_DIRECTORY);
	if (mnt_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (chown_r(info->t_mnt_scratch_fd, ".", 1000, 1000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	subvolume_fds[BTRFS_SUBVOLUME_SUBVOL2_ID] = openat(info->t_mnt_scratch_fd,
							   BTRFS_SUBVOLUME_SUBVOL2,
							   O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fds[BTRFS_SUBVOLUME_SUBVOL2_ID] < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL1_ID],
				   &subvolume_ids[BTRFS_SUBVOLUME_SUBVOL1_ID])) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL2_ID],
				   &subvolume_ids[BTRFS_SUBVOLUME_SUBVOL2_ID])) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}

	subvolume_fds[BTRFS_SUBVOLUME_SUBVOL3_ID] = openat(info->t_mnt_scratch_fd,
							   BTRFS_SUBVOLUME_SUBVOL1xSUBVOL3,
							   O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fds[BTRFS_SUBVOLUME_SUBVOL3_ID] < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL3_ID],
				   &subvolume_ids[BTRFS_SUBVOLUME_SUBVOL3_ID])) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}

	subvolume_fds[BTRFS_SUBVOLUME_SUBVOL4_ID] = openat(info->t_mnt_scratch_fd,
							   BTRFS_SUBVOLUME_SUBVOL1xDIR1xDIR2xSUBVOL4,
							   O_CLOEXEC | O_DIRECTORY);
	if (subvolume_fds[BTRFS_SUBVOLUME_SUBVOL4_ID] < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (btrfs_get_subvolume_id(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL4_ID],
				   &subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID])) {
		log_stderr("failure: btrfs_get_subvolume_id");
		goto out;
	}


	if (fchmod(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL3_ID], S_IRUSR | S_IWUSR | S_IXUSR), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	if (fchmod(subvolume_fds[BTRFS_SUBVOLUME_SUBVOL4_ID], S_IRUSR | S_IWUSR | S_IXUSR), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(mnt_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/*
	 * The open_tree() syscall returns an O_PATH file descriptor which we
	 * can't use with ioctl(). So let's reopen it as a proper file
	 * descriptor.
	 */
	tree_fd = openat(open_tree_fd, ".", O_RDONLY | O_CLOEXEC | O_DIRECTORY);
	if (tree_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		bool subvolume3_found = false, subvolume4_found = false;

		if (!switch_fsids(11000, 11000))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: lower caps");

		if (btrfs_iterator_start(tree_fd, 0, &iter))
			die("failure: btrfs_iterator_start");

		for (;;) {
			char *subvol_path = NULL;
			int ret;

			ret = btrfs_iterator_next(iter, &subvol_path, &subvolid);
			if (ret == 1)
				break;
			else if (ret)
				die("failure: btrfs_iterator_next");

			if (subvolid != subvolume_ids[BTRFS_SUBVOLUME_SUBVOL3_ID] &&
			    subvolid != subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID])
				die("failure: subvolume id %llu->%s",
				    (long long unsigned)subvolid, subvol_path);

			if (subvolid == subvolume_ids[BTRFS_SUBVOLUME_SUBVOL3_ID])
				subvolume3_found = true;

			if (subvolid == subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID])
				subvolume4_found = true;

			free(subvol_path);
		}
		btrfs_iterator_end(iter);

		if (!subvolume3_found || !subvolume4_found)
			die("failure: subvolume id");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		bool subvolume3_found = false, subvolume4_found = false;

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (btrfs_iterator_start(tree_fd, 0, &iter))
			die("failure: btrfs_iterator_start");

		for (;;) {
			char *subvol_path = NULL;
			int ret;

			ret = btrfs_iterator_next(iter, &subvol_path, &subvolid);
			if (ret == 1)
				break;
			else if (ret)
				die("failure: btrfs_iterator_next");

			if (subvolid != subvolume_ids[BTRFS_SUBVOLUME_SUBVOL3_ID] &&
			    subvolid != subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID])
				die("failure: subvolume id %llu->%s",
				    (long long unsigned)subvolid, subvol_path);

			if (subvolid == subvolume_ids[BTRFS_SUBVOLUME_SUBVOL3_ID])
				subvolume3_found = true;

			if (subvolid == subvolume_ids[BTRFS_SUBVOLUME_SUBVOL4_ID])
				subvolume4_found = true;

			free(subvol_path);
		}
		btrfs_iterator_end(iter);

		if (!subvolume3_found || !subvolume4_found)
			die("failure: subvolume id");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		bool subvolume_found = false;

		if (!switch_fsids(0, 0))
			die("failure: switch fsids");

		if (!caps_down())
			die("failure: lower caps");

		if (btrfs_iterator_start(tree_fd, 0, &iter))
			die("failure: btrfs_iterator_start");

		for (;;) {
			char *subvol_path = NULL;
			int ret;

			ret = btrfs_iterator_next(iter, &subvol_path, &subvolid);
			if (ret == 1)
				break;
			else if (ret)
				die("failure: btrfs_iterator_next");

			free(subvol_path);

			subvolume_found = true;
			break;
		}
		btrfs_iterator_end(iter);

		if (subvolume_found)
			die("failure: subvolume id");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	userns_fd = get_userns_fd(0, 30000, 10000);
	if (userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		bool subvolume_found = false;

		if (!switch_userns(userns_fd, 0, 0, true))
			die("failure: switch_userns");

		if (btrfs_iterator_start(tree_fd, 0, &iter))
			die("failure: btrfs_iterator_start");

		for (;;) {
			char *subvol_path = NULL;
			int ret;

			ret = btrfs_iterator_next(iter, &subvol_path, &subvolid);
			if (ret == 1)
				break;
			else if (ret)
				die("failure: btrfs_iterator_next");

			free(subvol_path);

			subvolume_found = true;
			break;
		}
		btrfs_iterator_end(iter);

		if (subvolume_found)
			die("failure: subvolume id");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(dir1_fd);
	safe_close(dir2_fd);
	safe_close(open_tree_fd);
	safe_close(tree_fd);
	safe_close(userns_fd);
	for (i = 0; i < ARRAY_SIZE(subvolume_fds); i++)
		safe_close(subvolume_fds[i]);
	snprintf(t_buf, sizeof(t_buf), "%s/%s", info->t_mountpoint, BTRFS_SUBVOLUME_MNT);
	sys_umount2(t_buf, MNT_DETACH);
	unlinkat(info->t_mnt_fd, BTRFS_SUBVOLUME_MNT, AT_REMOVEDIR);

	return fret;
}

static const struct test_struct t_btrfs[] = {
	{ btrfs_subvolumes_fsids_mapped,				T_REQUIRE_IDMAPPED_MOUNTS,	"test subvolumes with mapped fsids",								},
	{ btrfs_subvolumes_fsids_mapped_userns,				T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolumes with mapped fsids inside user namespace",					},
	{ btrfs_subvolumes_fsids_mapped_user_subvol_rm_allowed,		T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume deletion with user_subvol_rm_allowed mount option",				},
	{ btrfs_subvolumes_fsids_mapped_userns_user_subvol_rm_allowed,	T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume deletion with user_subvol_rm_allowed mount option inside user namespace",	},
	{ btrfs_subvolumes_fsids_unmapped,				T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolumes with unmapped fsids",								},
	{ btrfs_subvolumes_fsids_unmapped_userns,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolumes with unmapped fsids inside user namespace",					},
	{ btrfs_snapshots_fsids_mapped,					T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots with mapped fsids",								},
	{ btrfs_snapshots_fsids_mapped_userns,				T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots with mapped fsids inside user namespace",					},
	{ btrfs_snapshots_fsids_mapped_user_subvol_rm_allowed,		T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots deletion with user_subvol_rm_allowed mount option",				},
	{ btrfs_snapshots_fsids_mapped_userns_user_subvol_rm_allowed,	T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots deletion with user_subvol_rm_allowed mount option inside user namespace",	},
	{ btrfs_snapshots_fsids_unmapped,				T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots with unmapped fsids",								},
	{ btrfs_snapshots_fsids_unmapped_userns,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots with unmapped fsids inside user namespace",					},
	{ btrfs_delete_by_spec_id,					T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume deletion by spec id",								},
	{ btrfs_subvolumes_setflags_fsids_mapped,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume flags with mapped fsids",							},
	{ btrfs_subvolumes_setflags_fsids_mapped_userns,		T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume flags with mapped fsids inside user namespace",					},
	{ btrfs_subvolumes_setflags_fsids_unmapped,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume flags with unmapped fsids",							},
	{ btrfs_subvolumes_setflags_fsids_unmapped_userns,		T_REQUIRE_IDMAPPED_MOUNTS, 	"test subvolume flags with unmapped fsids inside user namespace",				},
	{ btrfs_snapshots_setflags_fsids_mapped,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots flags with mapped fsids",							},
	{ btrfs_snapshots_setflags_fsids_mapped_userns,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots flags with mapped fsids inside user namespace",					},
	{ btrfs_snapshots_setflags_fsids_unmapped,			T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots flags with unmapped fsids",							},
	{ btrfs_snapshots_setflags_fsids_unmapped_userns,		T_REQUIRE_IDMAPPED_MOUNTS, 	"test snapshots flags with unmapped fsids inside user namespace",				},
	{ btrfs_subvolume_lookup_user,					T_REQUIRE_IDMAPPED_MOUNTS, 	"test unprivileged subvolume lookup",								},
};

const struct test_suite s_btrfs_idmapped_mounts = {
	.tests = t_btrfs,
	.nr_tests = ARRAY_SIZE(t_btrfs),
};
