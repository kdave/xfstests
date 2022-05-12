/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __IDMAP_UTILS_H
#define __IDMAP_UTILS_H

#include "../global.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <linux/types.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/fsuid.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#ifdef HAVE_LIBURING_H
#include <liburing.h>
#endif

#include "missing.h"

/* Flags for which functionality is required by the test */
#define T_REQUIRE_IDMAPPED_MOUNTS (1U << 0)
#define T_REQUIRE_USERNS (1U << 1)

#define T_DIR1 "idmapped_mounts_1"
#define FILE1 "file1"
#define FILE1_RENAME "file1_rename"
#define FILE2 "file2"
#define FILE3 "file3"
#define FILE2_RENAME "file2_rename"
#define DIR1 "dir1"
#define DIR2 "dir2"
#define DIR3 "dir3"
#define DIR1_RENAME "dir1_rename"
#define HARDLINK1 "hardlink1"
#define SYMLINK1 "symlink1"
#define SYMLINK_USER1 "symlink_user1"
#define SYMLINK_USER2 "symlink_user2"
#define SYMLINK_USER3 "symlink_user3"
#define CHRDEV1 "chrdev1"

/* Maximum number of nested user namespaces in the kernel. */
#define MAX_USERNS_LEVEL 32

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

#define log_stderr(format, ...)                                                         \
	fprintf(stderr, "%s: %d: %s - %m - " format "\n", __FILE__, __LINE__, __func__, \
		##__VA_ARGS__)

#ifdef DEBUG_TRACE
#define log_debug(format, ...)                                           \
	fprintf(stderr, "%s: %d: %s - " format "\n", __FILE__, __LINE__, \
		__func__, ##__VA_ARGS__)
#else
#define log_debug(format, ...)
#endif

#define log_error_errno(__ret__, __errno__, format, ...)      \
	({                                                    \
		typeof(__ret__) __internal_ret__ = (__ret__); \
		errno = (__errno__);                          \
		log_stderr(format, ##__VA_ARGS__);            \
		__internal_ret__;                             \
	})

#define log_errno(__ret__, format, ...) log_error_errno(__ret__, errno, format, ##__VA_ARGS__)

#define die_errno(__errno__, format, ...)          \
	({                                         \
		errno = (__errno__);               \
		log_stderr(format, ##__VA_ARGS__); \
		exit(EXIT_FAILURE);                \
	})

#define die(format, ...) die_errno(errno, format, ##__VA_ARGS__)

#define syserror(format, ...)                           \
	({                                              \
		fprintf(stderr, "%m - " format "\n", ##__VA_ARGS__); \
		(-errno);                               \
	})

#define syserror_set(__ret__, format, ...)                    \
	({                                                    \
		typeof(__ret__) __internal_ret__ = (__ret__); \
		errno = labs(__ret__);                        \
		fprintf(stderr, "%m - " format "\n", ##__VA_ARGS__);       \
		__internal_ret__;                             \
	})

#define safe_close(fd)      \
	if (fd >= 0) {           \
		int _e_ = errno; \
		close(fd);       \
		errno = _e_;     \
		fd = -EBADF;     \
	}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#ifndef CAP_NET_RAW
#define CAP_NET_RAW 13
#endif

#ifndef VFS_CAP_FLAGS_EFFECTIVE
#define VFS_CAP_FLAGS_EFFECTIVE 0x000001
#endif

#ifndef VFS_CAP_U32_3
#define VFS_CAP_U32_3 2
#endif

#ifndef VFS_CAP_U32
#define VFS_CAP_U32 VFS_CAP_U32_3
#endif

#ifndef VFS_CAP_REVISION_1
#define VFS_CAP_REVISION_1 0x01000000
#endif

#ifndef VFS_CAP_REVISION_2
#define VFS_CAP_REVISION_2 0x02000000
#endif

#ifndef VFS_CAP_REVISION_3
#define VFS_CAP_REVISION_3 0x03000000
struct vfs_ns_cap_data {
	__le32 magic_etc;
	struct {
		__le32 permitted;
		__le32 inheritable;
	} data[VFS_CAP_U32];
	__le32 rootid;
};
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
#define cpu_to_le16(w16) le16_to_cpu(w16)
#define le16_to_cpu(w16) ((u_int16_t)((u_int16_t)(w16) >> 8) | (u_int16_t)((u_int16_t)(w16) << 8))
#define cpu_to_le32(w32) le32_to_cpu(w32)
#define le32_to_cpu(w32)                                                                       \
	((u_int32_t)((u_int32_t)(w32) >> 24) | (u_int32_t)(((u_int32_t)(w32) >> 8) & 0xFF00) | \
	 (u_int32_t)(((u_int32_t)(w32) << 8) & 0xFF0000) | (u_int32_t)((u_int32_t)(w32) << 24))
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define cpu_to_le16(w16) ((u_int16_t)(w16))
#define le16_to_cpu(w16) ((u_int16_t)(w16))
#define cpu_to_le32(w32) ((u_int32_t)(w32))
#define le32_to_cpu(w32) ((u_int32_t)(w32))
#else
#error Expected endianess macro to be set
#endif

struct vfstest_info {
	uid_t t_overflowuid;
	gid_t t_overflowgid;
	/* path of the test device */
	const char *t_fstype;
	/* path of the test device */
	const char *t_device;
	/* path of the test scratch device */
	const char *t_device_scratch;
	/* mountpoint of the test device */
	const char *t_mountpoint;
	/* mountpoint of the test device */
	const char *t_mountpoint_scratch;
	/* fd for @t_mountpoint */
	int t_mnt_fd;
	/* fd for @t_mountpoint_scratch */
	int t_mnt_scratch_fd;
	/* fd for @T_DIR1 */
	int t_dir1_fd;
	/* whether the underlying filesystem supports idmapped mounts */
	bool t_fs_allow_idmap;
	/* whether user namespaces are supported */
	bool t_has_userns;
};

struct test_struct {
	int (*test)(const struct vfstest_info *info);
	unsigned int support_flags;
	const char *description;
};

struct test_suite {
	size_t nr_tests;
	const struct test_struct *tests;
};

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

struct list {
	void *elem;
	struct list *next;
	struct list *prev;
};

struct userns_hierarchy {
	int fd_userns;
	int fd_event;
	unsigned int level;
	struct list id_map;
};

#define list_for_each(__iterator, __list) \
	for (__iterator = (__list)->next; __iterator != __list; __iterator = __iterator->next)

#define list_for_each_safe(__iterator, __list, __next)               \
	for (__iterator = (__list)->next, __next = __iterator->next; \
	     __iterator != __list; __iterator = __next, __next = __next->next)

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

static inline void list_del(struct list *list)
{
	struct list *next, *prev;

	next = list->next;
	prev = list->prev;
	next->prev = prev;
	prev->next = next;
}

extern pid_t do_clone(int (*fn)(void *), void *arg, int flags);
extern int get_userns_fd(unsigned long nsid, unsigned long hostid,
			 unsigned long range);
extern int get_userns_fd_from_idmap(struct list *idmap);
extern ssize_t read_nointr(int fd, void *buf, size_t count);
extern int wait_for_pid(pid_t pid);
extern ssize_t write_nointr(int fd, const void *buf, size_t count);

extern int caps_down(void);
extern int caps_down_fsetid(void);
extern int caps_up(void);
static inline bool caps_supported(void)
{
	bool ret = false;

#ifdef HAVE_SYS_CAPABILITY_H
	ret = true;
#endif

	return ret;
}
extern bool expected_dummy_vfs_caps_uid(int fd, uid_t expected_uid);
extern int set_dummy_vfs_caps(int fd, int flags, int rootuid);

extern bool switch_ids(uid_t uid, gid_t gid);

extern int create_userns_hierarchy(struct userns_hierarchy *h);
extern int add_map_entry(struct list *head, __u32 id_host, __u32 id_ns,
			 __u32 range, idmap_type_t map_type);

extern bool __expected_uid_gid(int dfd, const char *path, int flags,
			       uid_t expected_uid, gid_t expected_gid, bool log);
static inline bool expected_uid_gid(int dfd, const char *path, int flags,
				    uid_t expected_uid, gid_t expected_gid)
{
	return __expected_uid_gid(dfd, path, flags, expected_uid, expected_gid, true);
}

static inline bool switch_userns(int fd, uid_t uid, gid_t gid, bool drop_caps)
{
	if (setns(fd, CLONE_NEWUSER))
		return log_errno(false, "failure: setns");

	if (!switch_ids(uid, gid))
		return log_errno(false, "failure: switch_ids");

	if (drop_caps && !caps_down())
		return log_errno(false, "failure: caps_down");

	return true;
}

extern bool switch_resids(uid_t uid, gid_t gid);

static inline bool switch_fsids(uid_t fsuid, gid_t fsgid)
{
	if (setfsgid(fsgid))
		return log_errno(false, "failure: setfsgid");

	if (setfsgid(-1) != fsgid)
		return log_errno(false, "failure: setfsgid(-1)");

	if (setfsuid(fsuid))
		return log_errno(false, "failure: setfsuid");

	if (setfsuid(-1) != fsuid)
		return log_errno(false, "failure: setfsuid(-1)");

	return true;
}

#ifdef HAVE_LIBURING_H
extern int io_uring_openat_with_creds(struct io_uring *ring, int dfd,
				      const char *path, int cred_id,
				      bool with_link, int *ret_cqe);
#endif /* HAVE_LIBURING_H */

extern int chown_r(int fd, const char *path, uid_t uid, gid_t gid);
extern int rm_r(int fd, const char *path);
extern int fd_to_fd(int from, int to);
extern bool protected_symlinks_enabled(void);
extern bool xfs_irix_sgid_inherit_enabled(const char *fstype);
extern bool expected_file_size(int dfd, const char *path, int flags,
			       off_t expected_size);
extern bool is_setid(int dfd, const char *path, int flags);
extern bool is_setgid(int dfd, const char *path, int flags);
extern bool is_sticky(int dfd, const char *path, int flags);
extern bool openat_tmpfile_supported(int dirfd);

#endif /* __IDMAP_UTILS_H */
