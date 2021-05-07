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
#include <sched.h>
#include <stdbool.h>
#include <sys/fsuid.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

#ifdef HAVE_SYS_CAPABILITY_H
#include <sys/capability.h>
#endif

#ifdef HAVE_LIBURING_H
#include <liburing.h>
#endif

#include "missing.h"
#include "utils.h"

#define T_DIR1 "idmapped_mounts_1"
#define FILE1 "file1"
#define FILE1_RENAME "file1_rename"
#define FILE2 "file2"
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

uid_t t_overflowuid = 65534;
gid_t t_overflowgid = 65534;

/* path of the test device */
const char *t_fstype;

/* path of the test device */
const char *t_device;

/* mountpoint of the test device */
const char *t_mountpoint;

/* fd for @t_mountpoint */
int t_mnt_fd;

/* fd for @T_DIR1 */
int t_dir1_fd;

/* temporary buffer */
char t_buf[PATH_MAX];

static void stash_overflowuid(void)
{
	int fd;
	ssize_t ret;
	char buf[256];

	fd = open("/proc/sys/fs/overflowuid", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;

	ret = read(fd, buf, sizeof(buf));
	close(fd);
	if (ret < 0)
		return;

	t_overflowuid = atoi(buf);
}

static void stash_overflowgid(void)
{
	int fd;
	ssize_t ret;
	char buf[256];

	fd = open("/proc/sys/fs/overflowgid", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return;

	ret = read(fd, buf, sizeof(buf));
	close(fd);
	if (ret < 0)
		return;

	t_overflowgid = atoi(buf);
}

static bool is_xfs(void)
{
	static int enabled = -1;

	if (enabled == -1)
		enabled = !strcmp(t_fstype, "xfs");

	return enabled;
}

static bool protected_symlinks_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		int fd;
		ssize_t ret;
		char buf[256];

		enabled = 0;

		fd = open("/proc/sys/fs/protected_symlinks", O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			return false;

		ret = read(fd, buf, sizeof(buf));
		close(fd);
		if (ret < 0)
			return false;

		if (atoi(buf) >= 1)
			enabled = 1;
        }

	return enabled == 1;
}

static bool xfs_irix_sgid_inherit_enabled(void)
{
	static int enabled = -1;

	if (enabled == -1) {
		int fd;
		ssize_t ret;
		char buf[256];

		enabled = 0;

		if (is_xfs()) {
			fd = open("/proc/sys/fs/xfs/irix_sgid_inherit", O_RDONLY | O_CLOEXEC);
			if (fd < 0)
				return false;

			ret = read(fd, buf, sizeof(buf));
			close(fd);
			if (ret < 0)
				return false;

			if (atoi(buf) >= 1)
				enabled = 1;
		}
        }

	return enabled == 1;
}

static inline bool caps_supported(void)
{
	bool ret = false;

#ifdef HAVE_SYS_CAPABILITY_H
	ret = true;
#endif

	return ret;
}

/* caps_down - lower all effective caps */
static int caps_down(void)
{
	bool fret = false;
#ifdef HAVE_SYS_CAPABILITY_H
	cap_t caps = NULL;
	int ret = -1;

	caps = cap_get_proc();
	if (!caps)
		goto out;

	ret = cap_clear_flag(caps, CAP_EFFECTIVE);
	if (ret)
		goto out;

	ret = cap_set_proc(caps);
	if (ret)
		goto out;

	fret = true;

out:
	cap_free(caps);
#endif
	return fret;
}

/* caps_up - raise all permitted caps */
static int caps_up(void)
{
	bool fret = false;
#ifdef HAVE_SYS_CAPABILITY_H
	cap_t caps = NULL;
	cap_value_t cap;
	int ret = -1;

	caps = cap_get_proc();
	if (!caps)
		goto out;

	for (cap = 0; cap <= CAP_LAST_CAP; cap++) {
		cap_flag_value_t flag;

		ret = cap_get_flag(caps, cap, CAP_PERMITTED, &flag);
		if (ret) {
			if (errno == EINVAL)
				break;
			else
				goto out;
		}

		ret = cap_set_flag(caps, CAP_EFFECTIVE, 1, &cap, flag);
		if (ret)
			goto out;
	}

	ret = cap_set_proc(caps);
	if (ret)
		goto out;

	fret = true;
out:
	cap_free(caps);
#endif
	return fret;
}

/* __expected_uid_gid - check whether file is owned by the provided uid and gid */
static bool __expected_uid_gid(int dfd, const char *path, int flags,
			       uid_t expected_uid, gid_t expected_gid, bool log)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return log_errno(false, "failure: fstatat");

	if (log && st.st_uid != expected_uid)
		log_stderr("failure: uid(%d) != expected_uid(%d)", st.st_uid, expected_uid);

	if (log && st.st_gid != expected_gid)
		log_stderr("failure: gid(%d) != expected_gid(%d)", st.st_gid, expected_gid);

	errno = 0; /* Don't report misleading errno. */
	return st.st_uid == expected_uid && st.st_gid == expected_gid;
}

static bool expected_uid_gid(int dfd, const char *path, int flags,
			     uid_t expected_uid, gid_t expected_gid)
{
	return __expected_uid_gid(dfd, path, flags,
				  expected_uid, expected_gid, true);
}

static bool expected_file_size(int dfd, const char *path,
			       int flags, off_t expected_size)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return log_errno(false, "failure: fstatat");

	if (st.st_size != expected_size)
		return log_errno(false, "failure: st_size(%zu) != expected_size(%zu)",
				 (size_t)st.st_size, (size_t)expected_size);

	return true;
}

/* is_setid - check whether file is S_ISUID and S_ISGID */
static bool is_setid(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISUID) || (st.st_mode & S_ISGID);
}

/* is_setgid - check whether file or directory is S_ISGID */
static bool is_setgid(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISGID);
}

/* is_sticky - check whether file is S_ISVTX */
static bool is_sticky(int dfd, const char *path, int flags)
{
	int ret;
	struct stat st;

	ret = fstatat(dfd, path, &st, flags);
	if (ret < 0)
		return false;

	errno = 0; /* Don't report misleading errno. */
	return (st.st_mode & S_ISVTX) > 0;
}

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

static inline bool switch_ids(uid_t uid, gid_t gid)
{
	if (setgroups(0, NULL))
		return log_errno(false, "failure: setgroups");

	if (setresgid(gid, gid, gid))
		return log_errno(false, "failure: setresgid");

	if (setresuid(uid, uid, uid))
		return log_errno(false, "failure: setresuid");

	return true;
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

/* rm_r - recursively remove all files */
static int rm_r(int fd, const char *path)
{
	int dfd, ret;
	DIR *dir;
	struct dirent *direntp;

	if (!path || strcmp(path, "") == 0)
		return -1;

	dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY);
	if (dfd < 0)
		return -1;

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		return -1;
	}

	while ((direntp = readdir(dir))) {
		struct stat st;

		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		if (S_ISDIR(st.st_mode))
			ret = rm_r(dfd, direntp->d_name);
		else
			ret = unlinkat(dfd, direntp->d_name, 0);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	ret = unlinkat(fd, path, AT_REMOVEDIR);
	closedir(dir);
	return ret;
}

/* chown_r - recursively change ownership of all files */
static int chown_r(int fd, const char *path, uid_t uid, gid_t gid)
{
	int dfd, ret;
	DIR *dir;
	struct dirent *direntp;

	dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY);
	if (dfd < 0)
		return -1;

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		return -1;
	}

	while ((direntp = readdir(dir))) {
		struct stat st;

		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		if (S_ISDIR(st.st_mode))
			ret = chown_r(dfd, direntp->d_name, uid, gid);
		else
			ret = fchownat(dfd, direntp->d_name, uid, gid, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	ret = fchownat(fd, path, uid, gid, AT_SYMLINK_NOFOLLOW);
	closedir(dir);
	return ret;
}

/*
 * There'll be scenarios where you'll want to see the attributes associated with
 * a directory tree during debugging or just to make sure things look correct.
 * Simply uncomment and place the print_r() helper where you need it.
 */
#ifdef DEBUG_TRACE
static int fd_cloexec(int fd, bool cloexec)
{
	int oflags, nflags;

	oflags = fcntl(fd, F_GETFD, 0);
	if (oflags < 0)
		return -errno;

	if (cloexec)
		nflags = oflags | FD_CLOEXEC;
	else
		nflags = oflags & ~FD_CLOEXEC;

	if (nflags == oflags)
		return 0;

	if (fcntl(fd, F_SETFD, nflags) < 0)
		return -errno;

	return 0;
}

static inline int dup_cloexec(int fd)
{
	int fd_dup;

	fd_dup = dup(fd);
	if (fd_dup < 0)
		return -errno;

	if (fd_cloexec(fd_dup, true)) {
		close(fd_dup);
		return -errno;
	}

	return fd_dup;
}

__attribute__((unused)) static int print_r(int fd, const char *path)
{
	int ret = 0;
	int dfd, dfd_dup;
	DIR *dir;
	struct dirent *direntp;
	struct stat st;

	if (!path || *path == '\0') {
		char buf[sizeof("/proc/self/fd/") + 30];

		ret = snprintf(buf, sizeof(buf), "/proc/self/fd/%d", fd);
		if (ret < 0 || (size_t)ret >= sizeof(buf))
			return -1;

		/*
		 * O_PATH file descriptors can't be used so we need to re-open
		 * just in case.
		 */
		dfd = openat(-EBADF, buf, O_CLOEXEC | O_DIRECTORY, 0);
	} else {
		dfd = openat(fd, path, O_CLOEXEC | O_DIRECTORY, 0);
	}
	if (dfd < 0)
		return -1;

	/*
	 * When fdopendir() below succeeds it assumes ownership of the fd so we
	 * to make sure we always have an fd that fdopendir() can own which is
	 * why we dup() in the case where the caller wants us to operate on the
	 * fd directly.
	 */
	dfd_dup = dup_cloexec(dfd);
	if (dfd_dup < 0) {
		close(dfd);
		return -1;
	}

	dir = fdopendir(dfd);
	if (!dir) {
		close(dfd);
		close(dfd_dup);
		return -1;
	}
	/* Transfer ownership to fdopendir(). */
	dfd = -EBADF;

	while ((direntp = readdir(dir))) {
		if (!strcmp(direntp->d_name, ".") ||
		    !strcmp(direntp->d_name, ".."))
			continue;

		ret = fstatat(dfd_dup, direntp->d_name, &st, AT_SYMLINK_NOFOLLOW);
		if (ret < 0 && errno != ENOENT)
			break;

		ret = 0;
		if (S_ISDIR(st.st_mode))
			ret = print_r(dfd_dup, direntp->d_name);
		else
			fprintf(stderr, "mode(%o):uid(%d):gid(%d) -> %d/%s\n",
				(st.st_mode & ~S_IFMT), st.st_uid, st.st_gid,
				dfd_dup, direntp->d_name);
		if (ret < 0 && errno != ENOENT)
			break;
	}

	if (!path || *path == '\0')
		ret = fstatat(fd, "", &st,
			      AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW |
			      AT_EMPTY_PATH);
	else
		ret = fstatat(fd, path, &st,
			      AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW);
	if (!ret)
		fprintf(stderr, "mode(%o):uid(%d):gid(%d) -> %s",
			(st.st_mode & ~S_IFMT), st.st_uid, st.st_gid,
			(path && *path) ? path : "(null)");

	close(dfd_dup);
	closedir(dir);

	return ret;
}
#endif

/* fd_to_fd - transfer data from one fd to another */
static int fd_to_fd(int from, int to)
{
	for (;;) {
		uint8_t buf[PATH_MAX];
		uint8_t *p = buf;
		ssize_t bytes_to_write;
		ssize_t bytes_read;

		bytes_read = read_nointr(from, buf, sizeof buf);
		if (bytes_read < 0)
			return -1;
		if (bytes_read == 0)
			break;

		bytes_to_write = (size_t)bytes_read;
		do {
			ssize_t bytes_written;

			bytes_written = write_nointr(to, p, bytes_to_write);
			if (bytes_written < 0)
				return -1;

			bytes_to_write -= bytes_written;
			p += bytes_written;
		} while (bytes_to_write > 0);
	}

	return 0;
}

static int sys_execveat(int fd, const char *path, char **argv, char **envp,
			int flags)
{
#ifdef __NR_execveat
	return syscall(__NR_execveat, fd, path, argv, envp, flags);
#else
	errno = ENOSYS;
	return -1;
#endif
}

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

/* expected_dummy_vfs_caps_uid - check vfs caps are stored with the provided uid */
static bool expected_dummy_vfs_caps_uid(int fd, uid_t expected_uid)
{
#define __cap_raised_permitted(x, ns_cap_data)                                 \
	((ns_cap_data.data[(x) >> 5].permitted) & (1 << ((x)&31)))
	struct vfs_ns_cap_data ns_xattr = {};
	ssize_t ret;

	ret = fgetxattr(fd, "security.capability", &ns_xattr, sizeof(ns_xattr));
	if (ret < 0 || ret == 0)
		return false;

	if (ns_xattr.magic_etc & VFS_CAP_REVISION_3) {

		if (le32_to_cpu(ns_xattr.rootid) != expected_uid) {
			errno = EINVAL;
			log_stderr("failure: rootid(%d) != expected_rootid(%d)", le32_to_cpu(ns_xattr.rootid), expected_uid);
		}

		return (le32_to_cpu(ns_xattr.rootid) == expected_uid) &&
		       (__cap_raised_permitted(CAP_NET_RAW, ns_xattr) > 0);
	} else {
		log_stderr("failure: fscaps version");
	}

	return false;
}

/* set_dummy_vfs_caps - set dummy vfs caps for the provided uid */
static int set_dummy_vfs_caps(int fd, int flags, int rootuid)
{
#define __raise_cap_permitted(x, ns_cap_data)                                  \
	ns_cap_data.data[(x) >> 5].permitted |= (1 << ((x)&31))

	struct vfs_ns_cap_data ns_xattr;

	memset(&ns_xattr, 0, sizeof(ns_xattr));
	__raise_cap_permitted(CAP_NET_RAW, ns_xattr);
	ns_xattr.magic_etc |= VFS_CAP_REVISION_3 | VFS_CAP_FLAGS_EFFECTIVE;
	ns_xattr.rootid = cpu_to_le32(rootuid);

	return fsetxattr(fd, "security.capability",
			 &ns_xattr, sizeof(ns_xattr), flags);
}

#define safe_close(fd)      \
	if (fd >= 0) {           \
		int _e_ = errno; \
		close(fd);       \
		errno = _e_;     \
		fd = -EBADF;     \
	}

static void test_setup(void)
{
	if (mkdirat(t_mnt_fd, T_DIR1, 0777))
		die("failure: mkdirat");

	t_dir1_fd = openat(t_mnt_fd, T_DIR1, O_CLOEXEC | O_DIRECTORY);
	if (t_dir1_fd < 0)
		die("failure: openat");

	if (fchmod(t_dir1_fd, 0777))
		die("failure: fchmod");
}

static void test_cleanup(void)
{
	safe_close(t_dir1_fd);
	if (rm_r(t_mnt_fd, T_DIR1))
		die("failure: rm_r");
}

/* Validate that basic file operations on idmapped mounts. */
static int fsids_unmapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, hardlink_target_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	/* create hardlink target */
	hardlink_target_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (hardlink_target_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create directory for rename test */
	if (mkdirat(t_dir1_fd, DIR1, 0700)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	/* The caller's fsids don't have a mappings in the idmapped mount so any
	 * file creation must fail.
	 */

	/* create hardlink */
	if (!linkat(open_tree_fd, FILE1, open_tree_fd, HARDLINK1, 0)) {
		log_stderr("failure: linkat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* try to rename a file */
	if (!renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* try to rename a directory */
	if (!renameat(open_tree_fd, DIR1, open_tree_fd, DIR1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* The caller is privileged over the inode so file deletion must work. */

	/* remove file */
	if (unlinkat(open_tree_fd, FILE1, 0)) {
		log_stderr("failure: unlinkat");
		goto out;
	}

	/* remove directory */
	if (unlinkat(open_tree_fd, DIR1, AT_REMOVEDIR)) {
		log_stderr("failure: unlinkat");
		goto out;
	}

	/* The caller's fsids don't have a mappings in the idmapped mount so
	 * any file creation must fail.
	 */

	/* create regular file via open() */
	file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd >= 0) {
		log_stderr("failure: create");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* create regular file via mknod */
	if (!mknodat(open_tree_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* create character device */
	if (!mknodat(open_tree_fd, CHRDEV1, S_IFCHR | 0644, makedev(5, 1))) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* create symlink */
	if (!symlinkat(FILE2, open_tree_fd, SYMLINK1)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	/* create directory */
	if (!mkdirat(open_tree_fd, DIR1, 0700)) {
		log_stderr("failure: mkdirat");
		goto out;
	}
	if (errno != EOVERFLOW) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(hardlink_target_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int fsids_mapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, hardlink_target_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create hardlink target */
	hardlink_target_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (hardlink_target_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create directory for rename test */
	if (mkdirat(t_dir1_fd, DIR1, 0700)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

		/* The caller's fsids now have mappings in the idmapped mount so
		 * any file creation must fail.
		 */

		/* create hardlink */
		if (linkat(open_tree_fd, FILE1, open_tree_fd, HARDLINK1, 0))
			die("failure: create hardlink");

		/* try to rename a file */
		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: rename");

		/* try to rename a directory */
		if (renameat(open_tree_fd, DIR1, open_tree_fd, DIR1_RENAME))
			die("failure: rename");

		/* remove file */
		if (unlinkat(open_tree_fd, FILE1_RENAME, 0))
			die("failure: delete");

		/* remove directory */
		if (unlinkat(open_tree_fd, DIR1_RENAME, AT_REMOVEDIR))
			die("failure: delete");

		/* The caller's fsids have mappings in the idmapped mount so any
		 * file creation must fail.
		 */

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (file1_fd < 0)
			die("failure: create");

		/* create regular file via mknod */
		if (mknodat(open_tree_fd, FILE2, S_IFREG | 0000, 0))
			die("failure: create");

		/* create character device */
		if (mknodat(open_tree_fd, CHRDEV1, S_IFCHR | 0644, makedev(5, 1)))
			die("failure: create");

		/* create symlink */
		if (symlinkat(FILE2, open_tree_fd, SYMLINK1))
			die("failure: create");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0700))
			die("failure: create");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(hardlink_target_fd);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that basic file operations on idmapped mounts from a user namespace. */
static int create_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	/* change ownership of all files to uid 0 */
	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (file1_fd < 0)
			die("failure: open file");
		safe_close(file1_fd);

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		/* create regular file via mknod */
		if (mknodat(open_tree_fd, FILE2, S_IFREG | 0000, 0))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, FILE2, 0, 0, 0))
			die("failure: check ownership");

		/* create symlink */
		if (symlinkat(FILE2, open_tree_fd, SYMLINK1))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, 0, 0))
			die("failure: check ownership");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0700))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, DIR1, 0, 0, 0))
			die("failure: check ownership");

		/* try to rename a file */
		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, FILE1_RENAME, 0, 0, 0))
			die("failure: check ownership");

		/* try to rename a file */
		if (renameat(open_tree_fd, DIR1, open_tree_fd, DIR1_RENAME))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, DIR1_RENAME, 0, 0, 0))
			die("failure: check ownership");

		/* remove file */
		if (unlinkat(open_tree_fd, FILE1_RENAME, 0))
			die("failure: remove");

		/* remove directory */
		if (unlinkat(open_tree_fd, DIR1_RENAME, AT_REMOVEDIR))
			die("failure: remove");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int hardlink_crossing_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;

        if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (mkdirat(open_tree_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* We're crossing a mountpoint so this must fail.
	 *
	 * Note that this must also fail for non-idmapped mounts but here we're
	 * interested in making sure we're not introducing an accidental way to
	 * violate that restriction or that suddenly this becomes possible.
	 */
	if (!linkat(open_tree_fd, FILE1, t_dir1_fd, HARDLINK1, 0)) {
		log_stderr("failure: linkat");
		goto out;
	}
	if (errno != EXDEV) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int hardlink_crossing_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd1 = -EBADF, open_tree_fd2 = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd1 = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd1 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd1, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	file1_fd = openat(open_tree_fd1, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	safe_close(file1_fd);

	if (mkdirat(open_tree_fd1, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	open_tree_fd2 = sys_open_tree(t_dir1_fd, DIR1,
				      AT_NO_AUTOMOUNT |
				      AT_SYMLINK_NOFOLLOW |
				      OPEN_TREE_CLOEXEC |
				      OPEN_TREE_CLONE |
				      AT_RECURSIVE);
	if (open_tree_fd2 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd2, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* We're crossing a mountpoint so this must fail.
	 *
	 * Note that this must also fail for non-idmapped mounts but here we're
	 * interested in making sure we're not introducing an accidental way to
	 * violate that restriction or that suddenly this becomes possible.
	 */
	if (!linkat(open_tree_fd1, FILE1, open_tree_fd2, HARDLINK1, 0)) {
		log_stderr("failure: linkat");
		goto out;
	}
	if (errno != EXDEV) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd1);
	safe_close(open_tree_fd2);

	return fret;
}

static int hardlink_from_idmapped_mount(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(file1_fd);

	if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* We're not crossing a mountpoint so this must succeed. */
	if (linkat(open_tree_fd, FILE1, open_tree_fd, HARDLINK1, 0)) {
		log_stderr("failure: linkat");
		goto out;
	}


	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int hardlink_from_idmapped_mount_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (file1_fd < 0)
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		/* We're not crossing a mountpoint so this must succeed. */
		if (linkat(open_tree_fd, FILE1, open_tree_fd, HARDLINK1, 0))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, HARDLINK1, 0, 0, 0))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int rename_crossing_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (mkdirat(open_tree_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* We're crossing a mountpoint so this must fail.
	 *
	 * Note that this must also fail for non-idmapped mounts but here we're
	 * interested in making sure we're not introducing an accidental way to
	 * violate that restriction or that suddenly this becomes possible.
	 */
	if (!renameat(open_tree_fd, FILE1, t_dir1_fd, FILE1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}
	if (errno != EXDEV) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int rename_crossing_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd1 = -EBADF, open_tree_fd2 = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd1 = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd1 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd1, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	file1_fd = openat(open_tree_fd1, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (mkdirat(open_tree_fd1, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	open_tree_fd2 = sys_open_tree(t_dir1_fd, DIR1,
				      AT_NO_AUTOMOUNT |
				      AT_SYMLINK_NOFOLLOW |
				      OPEN_TREE_CLOEXEC |
				      OPEN_TREE_CLONE |
				      AT_RECURSIVE);
	if (open_tree_fd2 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd2, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* We're crossing a mountpoint so this must fail.
	 *
	 * Note that this must also fail for non-idmapped mounts but here we're
	 * interested in making sure we're not introducing an accidental way to
	 * violate that restriction or that suddenly this becomes possible.
	 */
	if (!renameat(open_tree_fd1, FILE1, open_tree_fd2, FILE1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}
	if (errno != EXDEV) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd1);
	safe_close(open_tree_fd2);

	return fret;
}

static int rename_from_idmapped_mount(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* We're not crossing a mountpoint so this must succeed. */
	if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME)) {
		log_stderr("failure: renameat");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int rename_from_idmapped_mount_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	pid_t pid;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (file1_fd < 0)
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		/* We're not crossing a mountpoint so this must succeed. */
		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: create");

		if (!expected_uid_gid(open_tree_fd, FILE1_RENAME, 0, 0, 0))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int symlink_regular_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct stat st;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (chown_r(t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (symlinkat(FILE1, open_tree_fd, FILE2)) {
		log_stderr("failure: symlinkat");
		goto out;
	}

	if (fchownat(open_tree_fd, FILE2, 15000, 15000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	if (fstatat(open_tree_fd, FILE2, &st, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fstatat");
		goto out;
	}

	if (st.st_uid != 15000 || st.st_gid != 15000) {
		log_stderr("failure: compare ids");
		goto out;
	}

	if (fstatat(open_tree_fd, FILE1, &st, 0)) {
		log_stderr("failure: fstatat");
		goto out;
	}

	if (st.st_uid != 10000 || st.st_gid != 10000) {
		log_stderr("failure: compare ids");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int symlink_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

		if (symlinkat(FILE1, open_tree_fd, FILE2))
			die("failure: create");

		if (fchownat(open_tree_fd, FILE2, 15000, 15000, AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");

		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW, 15000, 15000))
			die("failure: check ownership");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int symlink_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (chown_r(t_mnt_fd, T_DIR1, 0, 0)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
		if (file1_fd < 0)
			die("failure: create");
		safe_close(file1_fd);

		if (symlinkat(FILE1, open_tree_fd, FILE2))
			die("failure: create");

		if (fchownat(open_tree_fd, FILE2, 5000, 5000, AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");

		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW, 5000, 5000))
			die("failure: check ownership");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (!expected_uid_gid(t_dir1_fd, FILE2, AT_SYMLINK_NOFOLLOW, 5000, 5000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that a caller whose fsids map into the idmapped mount within it's
 * user namespace cannot create any device nodes.
 */
static int device_node_in_userns(void)
{
	int fret = -1;
	int open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create character device */
		if (!mknodat(open_tree_fd, CHRDEV1, S_IFCHR | 0644, makedev(5, 1)))
			die("failure: create");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);

	return fret;
}


/* Validate that changing file ownership works correctly on idmapped mounts. */
static int expected_uid_gid_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd1 = -EBADF, open_tree_fd2 = -EBADF;
	struct mount_attr attr1 = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	struct mount_attr attr2 = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!switch_fsids(0, 0)) {
		log_stderr("failure: switch_fsids");
		goto out;
	}

	/* create regular file via open() */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(t_dir1_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}

	/* create character device */
	if (mknodat(t_dir1_fd, CHRDEV1, S_IFCHR | 0644, makedev(5, 1))) {
		log_stderr("failure: mknodat");
		goto out;
	}

	/* create hardlink */
	if (linkat(t_dir1_fd, FILE1, t_dir1_fd, HARDLINK1, 0)) {
		log_stderr("failure: linkat");
		goto out;
	}

	/* create symlink */
	if (symlinkat(FILE2, t_dir1_fd, SYMLINK1)) {
		log_stderr("failure: symlinkat");
		goto out;
	}

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0700)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr1.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr1.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd1 = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd1 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd1, "", AT_EMPTY_PATH, &attr1, sizeof(attr1))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* Validate that all files created through the image mountpoint are
	 * owned by the callers fsuid and fsgid.
	 */
	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, FILE2, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, DIR1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Validate that all files are owned by the uid and gid specified in
	 * the idmapping of the mount they are accessed from.
	 */
	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, FILE2, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, DIR1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr2.userns_fd	= get_userns_fd(0, 30000, 2001);
	if (attr2.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd2 = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd2 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd2, "", AT_EMPTY_PATH, &attr2, sizeof(attr2))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* Validate that all files are owned by the uid and gid specified in
	 * the idmapping of the mount they are accessed from.
	 */
	if (!expected_uid_gid(open_tree_fd2, FILE1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, FILE2, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, DIR1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Change ownership throught original image mountpoint. */
	if (fchownat(t_dir1_fd, FILE1, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, HARDLINK1, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, CHRDEV1, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, SYMLINK1, 3000, 3000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, SYMLINK1, 2000, 2000, AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchownat(t_dir1_fd, DIR1, 2000, 2000, AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	/* Check ownership through original mount. */
	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, FILE2, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, 3000, 3000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, DIR1, 0, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through first idmapped mount. */
	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 12000, 12000)) {
		log_stderr("failure:expected_uid_gid ");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, FILE2, 0, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, 13000, 13000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, 12000, 12000)) {
		log_stderr("failure:expected_uid_gid ");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, DIR1, 0, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through second idmapped mount. */
	if (!expected_uid_gid(open_tree_fd2, FILE1, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, FILE2, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, t_overflowuid, t_overflowgid)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, DIR1, 0, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr1.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!fchownat(t_dir1_fd, FILE1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, FILE2, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, HARDLINK1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, CHRDEV1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, SYMLINK1, 2000, 2000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, SYMLINK1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, DIR1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (!fchownat(open_tree_fd2, FILE1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, FILE2, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, HARDLINK1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, CHRDEV1, 1000, 1000, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, SYMLINK1, 2000, 2000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, SYMLINK1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, DIR1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (fchownat(open_tree_fd1, FILE1, 1000, 1000, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, FILE2, 1000, 1000, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, HARDLINK1, 1000, 1000, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, CHRDEV1, 1000, 1000, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, SYMLINK1, 2000, 2000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, SYMLINK1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (fchownat(open_tree_fd1, DIR1, 1000, 1000, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (!expected_uid_gid(t_dir1_fd, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, FILE2, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, DIR1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd2, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, FILE2, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, DIR1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 1000, 1000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, FILE2, 0, 1000, 1000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, 1000, 1000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, 1000, 1000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, 2000, 2000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, 1000, 1000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, DIR1, 0, 1000, 1000))
			die("failure: expected_uid_gid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	/* Check ownership through original mount. */
	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, FILE2, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, DIR1, 0, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through first idmapped mount. */
	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, FILE2, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, DIR1, 0, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through second idmapped mount. */
	if (!expected_uid_gid(open_tree_fd2, FILE1, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, FILE2, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, DIR1, 0, 31000, 31000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr2.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!fchownat(t_dir1_fd, FILE1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, FILE2, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, HARDLINK1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, CHRDEV1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, SYMLINK1, 3000, 3000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, SYMLINK1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (!fchownat(t_dir1_fd, DIR1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (!fchownat(open_tree_fd1, FILE1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, FILE2, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, HARDLINK1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, CHRDEV1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, SYMLINK1, 3000, 3000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, SYMLINK1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd1, DIR1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (fchownat(open_tree_fd2, FILE1, 0, 0, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd2, FILE2, 0, 0, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd2, HARDLINK1, 0, 0, 0))
			die("failure: fchownat");
		if (fchownat(open_tree_fd2, CHRDEV1, 0, 0, 0))
			die("failure: fchownat");
		if (!fchownat(open_tree_fd2, SYMLINK1, 3000, 3000, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW))
			die("failure: fchownat");
		if (fchownat(open_tree_fd2, SYMLINK1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");
		if (fchownat(open_tree_fd2, DIR1, 0, 0, AT_EMPTY_PATH))
			die("failure: fchownat");

		if (!expected_uid_gid(t_dir1_fd, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, FILE2, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(t_dir1_fd, DIR1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd1, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, FILE2, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd1, DIR1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		if (!expected_uid_gid(open_tree_fd2, FILE1, 0, 0, 0))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, FILE2, 0, 0, 0))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, 0, 0))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, 0, 0))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, 2000, 2000))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, 0, 0))
			die("failure: expected_uid_gid");
		if (!expected_uid_gid(open_tree_fd2, DIR1, 0, 0, 0))
			die("failure: expected_uid_gid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	/* Check ownership through original mount. */
	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, FILE2, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, HARDLINK1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, CHRDEV1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, AT_SYMLINK_NOFOLLOW, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, SYMLINK1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(t_dir1_fd, DIR1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through first idmapped mount. */
	if (!expected_uid_gid(open_tree_fd1, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, FILE2, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, HARDLINK1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, CHRDEV1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, AT_SYMLINK_NOFOLLOW, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, SYMLINK1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd1, DIR1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Check ownership through second idmapped mount. */
	if (!expected_uid_gid(open_tree_fd2, FILE1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, FILE2, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, HARDLINK1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, CHRDEV1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, AT_SYMLINK_NOFOLLOW, 32000, 32000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, SYMLINK1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(open_tree_fd2, DIR1, 0, 30000, 30000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr1.userns_fd);
	safe_close(attr2.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd1);
	safe_close(open_tree_fd2);

	return fret;
}

static int fscaps(void)
{
	int fret = -1;
	int file1_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* Skip if vfs caps are unsupported. */
	if (set_dummy_vfs_caps(file1_fd, 0, 1000))
		return 0;

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd, 1000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
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

		/*
		 * On kernels before 5.12 this would succeed and return the
		 * unconverted caps. Then - for whatever reason - this behavior
		 * got changed and since 5.12 EOVERFLOW is returned when the
		 * rootid stored alongside the vfs caps does not map to uid 0 in
		 * the caller's user namespace.
		 */
		if (!expected_dummy_vfs_caps_uid(file1_fd, 1000) && errno != EOVERFLOW)
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (fremovexattr(file1_fd, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}
	if (expected_dummy_vfs_caps_uid(file1_fd, -1)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}
	if (errno != ENODATA) {
		log_stderr("failure: errno");
		goto out;
	}

	if (set_dummy_vfs_caps(file1_fd, 0, 10000)) {
		log_stderr("failure: set_dummy_vfs_caps");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd, 10000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
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

		if (!expected_dummy_vfs_caps_uid(file1_fd, 0))
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (fremovexattr(file1_fd, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}
	if (expected_dummy_vfs_caps_uid(file1_fd, -1)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}
	if (errno != ENODATA) {
		log_stderr("failure: errno");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);

	return fret;
}

static int fscaps_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, file1_fd2 = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* Skip if vfs caps are unsupported. */
	if (set_dummy_vfs_caps(file1_fd, 0, 1000))
		return 0;

	if (fremovexattr(file1_fd, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	file1_fd2 = openat(open_tree_fd, FILE1, O_RDWR | O_CLOEXEC, 0);
	if (file1_fd2 < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (!set_dummy_vfs_caps(file1_fd2, 0, 1000)) {
		log_stderr("failure: set_dummy_vfs_caps");
		goto out;
	}

	if (set_dummy_vfs_caps(file1_fd2, 0, 10000)) {
		log_stderr("failure: set_dummy_vfs_caps");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd2, 10000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd, 0)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
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

		if (!expected_dummy_vfs_caps_uid(file1_fd2, 0))
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (fremovexattr(file1_fd2, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}
	if (expected_dummy_vfs_caps_uid(file1_fd2, -1)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}
	if (errno != ENODATA) {
		log_stderr("failure: errno");
		goto out;
	}

	if (set_dummy_vfs_caps(file1_fd2, 0, 12000)) {
		log_stderr("failure: set_dummy_vfs_caps");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd2, 12000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}

	if (!expected_dummy_vfs_caps_uid(file1_fd, 2000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
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

		if (!expected_dummy_vfs_caps_uid(file1_fd2, 2000))
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(file1_fd2);
	safe_close(open_tree_fd);

	return fret;
}

static int fscaps_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, file1_fd2 = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* Skip if vfs caps are unsupported. */
	if (set_dummy_vfs_caps(file1_fd, 0, 1000))
		return 0;

	if (fremovexattr(file1_fd, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	file1_fd2 = openat(open_tree_fd, FILE1, O_RDWR | O_CLOEXEC, 0);
	if (file1_fd2 < 0) {
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

		if (expected_dummy_vfs_caps_uid(file1_fd2, -1))
			die("failure: expected_dummy_vfs_caps_uid");
		if (errno != ENODATA)
			die("failure: errno");

		if (set_dummy_vfs_caps(file1_fd2, 0, 1000))
			die("failure: set_dummy_vfs_caps");

		if (!expected_dummy_vfs_caps_uid(file1_fd2, 1000))
			die("failure: expected_dummy_vfs_caps_uid");

		if (!expected_dummy_vfs_caps_uid(file1_fd, 1000) && errno != EOVERFLOW)
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (!expected_dummy_vfs_caps_uid(file1_fd, 1000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(file1_fd2);
	safe_close(open_tree_fd);

	return fret;
}

static int fscaps_idmapped_mounts_in_userns_separate_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, file1_fd2 = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* Skip if vfs caps are unsupported. */
	if (set_dummy_vfs_caps(file1_fd, 0, 1000)) {
		log_stderr("failure: set_dummy_vfs_caps");
		goto out;
	}

	if (fremovexattr(file1_fd, "security.capability")) {
		log_stderr("failure: fremovexattr");
		goto out;
	}

	/* change ownership of all files to uid 0 */
	if (chown_r(t_mnt_fd, T_DIR1, 20000, 20000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(20000, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	file1_fd2 = openat(open_tree_fd, FILE1, O_RDWR | O_CLOEXEC, 0);
	if (file1_fd2 < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int userns_fd;

		userns_fd = get_userns_fd(0, 10000, 10000);
		if (userns_fd < 0)
			die("failure: get_userns_fd");

		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (set_dummy_vfs_caps(file1_fd2, 0, 0))
			die("failure: set fscaps");

		if (!expected_dummy_vfs_caps_uid(file1_fd2, 0))
			die("failure: expected_dummy_vfs_caps_uid");

		if (!expected_dummy_vfs_caps_uid(file1_fd, 20000) && errno != EOVERFLOW)
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (!expected_dummy_vfs_caps_uid(file1_fd, 20000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int userns_fd;

		userns_fd = get_userns_fd(0, 10000, 10000);
		if (userns_fd < 0)
			die("failure: get_userns_fd");

		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (fremovexattr(file1_fd2, "security.capability"))
			die("failure: fremovexattr");
		if (expected_dummy_vfs_caps_uid(file1_fd2, -1))
			die("failure: expected_dummy_vfs_caps_uid");
		if (errno != ENODATA)
			die("failure: errno");

		if (set_dummy_vfs_caps(file1_fd2, 0, 1000))
			die("failure: set_dummy_vfs_caps");

		if (!expected_dummy_vfs_caps_uid(file1_fd2, 1000))
			die("failure: expected_dummy_vfs_caps_uid");

		if (!expected_dummy_vfs_caps_uid(file1_fd, 21000) && errno != EOVERFLOW)
			die("failure: expected_dummy_vfs_caps_uid");

		exit(EXIT_SUCCESS);
	}

	if (wait_for_pid(pid))
		goto out;

	if (!expected_dummy_vfs_caps_uid(file1_fd, 21000)) {
		log_stderr("failure: expected_dummy_vfs_caps_uid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(file1_fd2);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that when the IDMAP_MOUNT_TEST_RUN_SETID environment variable is set
 * to 1 that we are executed with setid privileges and if set to 0 we are not.
 * If the env variable isn't set the tests are not run.
 */
static void __attribute__((constructor)) setuid_rexec(void)
{
	const char *expected_euid_str, *expected_egid_str, *rexec;

	rexec = getenv("IDMAP_MOUNT_TEST_RUN_SETID");
	/* This is a regular test-suite run. */
	if (!rexec)
		return;

	expected_euid_str = getenv("EXPECTED_EUID");
	expected_egid_str = getenv("EXPECTED_EGID");

	if (expected_euid_str && expected_egid_str) {
		uid_t expected_euid;
		gid_t expected_egid;

		expected_euid = atoi(expected_euid_str);
		expected_egid = atoi(expected_egid_str);

		if (strcmp(rexec, "1") == 0) {
			/* we're expecting to run setid */
			if ((getuid() != geteuid()) && (expected_euid == geteuid()) &&
			    (getgid() != getegid()) && (expected_egid == getegid()))
				exit(EXIT_SUCCESS);
		} else if (strcmp(rexec, "0") == 0) {
			/* we're expecting to not run setid */
			if ((getuid() == geteuid()) && (expected_euid == geteuid()) &&
			    (getgid() == getegid()) && (expected_egid == getegid()))
				exit(EXIT_SUCCESS);
			else
				die("failure: non-setid");
		}
	}

	exit(EXIT_FAILURE);
}

/* Validate that setid transitions are handled correctly. */
static int setid_binaries(void)
{
	int fret = -1;
	int file1_fd = -EBADF, exec_fd = -EBADF;
	pid_t pid;

	/* create a file to be used as setuid binary */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* open our own executable */
	exec_fd = openat(-EBADF, "/proc/self/exe", O_RDONLY | O_CLOEXEC, 0000);
	if (exec_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* copy our own executable into the file we created */
	if (fd_to_fd(exec_fd, file1_fd)) {
		log_stderr("failure: fd_to_fd");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 5000, 5000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(exec_fd);
	safe_close(file1_fd);

	/* Verify we run setid binary as uid and gid 5000 from the original
	 * mount.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=5000",
			"EXPECTED_EGID=5000",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 5000, 5000))
			die("failure: expected_uid_gid");

		sys_execveat(t_dir1_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:

	return fret;
}

/* Validate that setid transitions are handled correctly on idmapped mounts. */
static int setid_binaries_idmapped_mounts(void)
{
	int fret = -1;
	int file1_fd = -EBADF, exec_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (mkdirat(t_mnt_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* create a file to be used as setuid binary */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* open our own executable */
	exec_fd = openat(-EBADF, "/proc/self/exe", O_RDONLY | O_CLOEXEC, 0000);
	if (exec_fd < 0) {
		log_stderr("failure:openat ");
		goto out;
	}

	/* copy our own executable into the file we created */
	if (fd_to_fd(exec_fd, file1_fd)) {
		log_stderr("failure: fd_to_fd");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 5000, 5000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(exec_fd);
	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	/* A detached mount will have an anonymous mount namespace attached to
	 * it. This means that we can't execute setid binaries on a detached
	 * mount because the mnt_may_suid() helper will fail the check_mount()
	 * part of its check which compares the caller's mount namespace to the
	 * detached mount's mount namespace. Since by definition an anonymous
	 * mount namespace is not equale to any mount namespace currently in
	 * use this can't work. So attach the mount to the filesystem first
	 * before performing this check.
	 */
	if (sys_move_mount(open_tree_fd, "", t_mnt_fd, DIR1, MOVE_MOUNT_F_EMPTY_PATH)) {
		log_stderr("failure: sys_move_mount");
		goto out;
	}

	/* Verify we run setid binary as uid and gid 10000 from idmapped mount mount. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=15000",
			"EXPECTED_EGID=15000",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 15000, 15000))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}

	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(exec_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	snprintf(t_buf, sizeof(t_buf), "%s/" DIR1, t_mountpoint);
	sys_umount2(t_buf, MNT_DETACH);
	rm_r(t_mnt_fd, DIR1);

	return fret;
}

/* Validate that setid transitions are handled correctly on idmapped mounts
 * running in a user namespace where the uid and gid of the setid binary have no
 * mapping.
 */
static int setid_binaries_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, exec_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (mkdirat(t_mnt_fd, DIR1, 0777)) {
		log_stderr("failure: ");
		goto out;
	}

	/* create a file to be used as setuid binary */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* open our own executable */
	exec_fd = openat(-EBADF, "/proc/self/exe", O_RDONLY | O_CLOEXEC, 0000);
	if (exec_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* copy our own executable into the file we created */
	if (fd_to_fd(exec_fd, file1_fd)) {
		log_stderr("failure: fd_to_fd");
		goto out;
	}

	safe_close(exec_fd);

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 5000, 5000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	/* A detached mount will have an anonymous mount namespace attached to
	 * it. This means that we can't execute setid binaries on a detached
	 * mount because the mnt_may_suid() helper will fail the check_mount()
	 * part of its check which compares the caller's mount namespace to the
	 * detached mount's mount namespace. Since by definition an anonymous
	 * mount namespace is not equale to any mount namespace currently in
	 * use this can't work. So attach the mount to the filesystem first
	 * before performing this check.
	 */
	if (sys_move_mount(open_tree_fd, "", t_mnt_fd, DIR1, MOVE_MOUNT_F_EMPTY_PATH)) {
		log_stderr("failure: sys_move_mount");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=5000",
			"EXPECTED_EGID=5000",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 5000, 5000))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}

	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	file1_fd = openat(t_dir1_fd, FILE1, O_RDWR | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXOTH | S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=0",
			"EXPECTED_EGID=0",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 5000, 5000, true))
			die("failure: switch_userns");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}

	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	file1_fd = openat(t_dir1_fd, FILE1, O_RDWR | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 30000, 30000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	if (fchmod(file1_fd, S_IXOTH | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	/* Verify that we can't assume a uid and gid of a setid binary for which
	 * we have no mapping in our user namespace.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		char expected_euid[100];
		char expected_egid[100];
		static char *envp[4] = {
			NULL,
			NULL,
			NULL,
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		envp[0] = "IDMAP_MOUNT_TEST_RUN_SETID=0";
		snprintf(expected_euid, sizeof(expected_euid), "EXPECTED_EUID=%d", geteuid());
		envp[1] = expected_euid;
		snprintf(expected_egid, sizeof(expected_egid), "EXPECTED_EGID=%d", getegid());
		envp[2] = expected_egid;

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}

	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(exec_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	snprintf(t_buf, sizeof(t_buf), "%s/" DIR1, t_mountpoint);
	sys_umount2(t_buf, MNT_DETACH);
	rm_r(t_mnt_fd, DIR1);

	return fret;
}

/* Validate that setid transitions are handled correctly on idmapped mounts
 * running in a user namespace where the uid and gid of the setid binary have no
 * mapping.
 */
static int setid_binaries_idmapped_mounts_in_userns_separate_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, exec_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (mkdirat(t_mnt_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	/* create a file to be used as setuid binary */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* open our own executable */
	exec_fd = openat(-EBADF, "/proc/self/exe", O_RDONLY | O_CLOEXEC, 0000);
	if (exec_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* copy our own executable into the file we created */
	if (fd_to_fd(exec_fd, file1_fd)) {
		log_stderr("failure: fd_to_fd");
		goto out;
	}

	safe_close(exec_fd);

	/* change ownership of all files to uid 0 */
	if (chown_r(t_mnt_fd, T_DIR1, 20000, 20000)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 25000, 25000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(20000, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	/* A detached mount will have an anonymous mount namespace attached to
	 * it. This means that we can't execute setid binaries on a detached
	 * mount because the mnt_may_suid() helper will fail the check_mount()
	 * part of its check which compares the caller's mount namespace to the
	 * detached mount's mount namespace. Since by definition an anonymous
	 * mount namespace is not equale to any mount namespace currently in
	 * use this can't work. So attach the mount to the filesystem first
	 * before performing this check.
	 */
	if (sys_move_mount(open_tree_fd, "", t_mnt_fd, DIR1, MOVE_MOUNT_F_EMPTY_PATH)) {
		log_stderr("failure: sys_move_mount");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int userns_fd;
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=5000",
			"EXPECTED_EGID=5000",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		userns_fd = get_userns_fd(0, 10000, 10000);
		if (userns_fd < 0)
			die("failure: get_userns_fd");

		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 5000, 5000))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}

	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	file1_fd = openat(t_dir1_fd, FILE1, O_RDWR | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 20000, 20000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	/* set the setid bits and grant execute permissions to the group */
	if (fchmod(file1_fd, S_IXOTH | S_IXGRP | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int userns_fd;
		static char *envp[] = {
			"IDMAP_MOUNT_TEST_RUN_SETID=1",
			"EXPECTED_EUID=0",
			"EXPECTED_EGID=0",
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		userns_fd = get_userns_fd(0, 10000, 10000);
		if (userns_fd < 0)
			die("failure: get_userns_fd");

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	file1_fd = openat(t_dir1_fd, FILE1, O_RDWR | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* chown the file to the uid and gid we want to assume */
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}

	if (fchmod(file1_fd, S_IXOTH | S_IEXEC | S_ISUID | S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setid(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: is_setid");
		goto out;
	}

	safe_close(file1_fd);

	/* Verify that we can't assume a uid and gid of a setid binary for
	 * which we have no mapping in our user namespace.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int userns_fd;
		char expected_euid[100];
		char expected_egid[100];
		static char *envp[4] = {
			NULL,
			NULL,
			NULL,
			NULL,
		};
		static char *argv[] = {
			NULL,
		};

		userns_fd = get_userns_fd(0, 10000, 10000);
		if (userns_fd < 0)
			die("failure: get_userns_fd");

		if (!switch_userns(userns_fd, 0, 0, false))
			die("failure: switch_userns");

		envp[0] = "IDMAP_MOUNT_TEST_RUN_SETID=0";
		snprintf(expected_euid, sizeof(expected_euid), "EXPECTED_EUID=%d", geteuid());
		envp[1] = expected_euid;
		snprintf(expected_egid, sizeof(expected_egid), "EXPECTED_EGID=%d", getegid());
		envp[2] = expected_egid;

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, t_overflowuid, t_overflowgid))
			die("failure: expected_uid_gid");

		sys_execveat(open_tree_fd, FILE1, argv, envp, 0);
		die("failure: sys_execveat");

		exit(EXIT_FAILURE);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(exec_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	snprintf(t_buf, sizeof(t_buf), "%s/" DIR1, t_mountpoint);
	sys_umount2(t_buf, MNT_DETACH);
	rm_r(t_mnt_fd, DIR1);

	return fret;
}

static int sticky_bit_unlink(void)
{
	int fret = -1;
	int dir_fd = -EBADF;
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}

	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (!unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 1000, 0))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 1000, 2000))
			die("failure: expected_uid_gid");

		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 1000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(dir_fd);

	return fret;
}

static int sticky_bit_unlink_idmapped_mounts(void)
{
	int fret = -1;
	int dir_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 10000, 10000)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 10000, 10000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 12000, 12000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(dir_fd, "",
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

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 10000, 10000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 12000, 12000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (!unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 11000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 11000, 10000))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 11000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 11000, 12000))
			die("failure: expected_uid_gid");

		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 11000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 10000, 10000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 12000, 12000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(dir_fd);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that the sticky bit behaves correctly on idmapped mounts for unlink
 * operations in a user namespace.
 */
static int sticky_bit_unlink_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int dir_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(dir_fd, "",
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

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (!unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 1000, 0))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 1000, 2000))
			die("failure: expected_uid_gid");

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (!unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");

		if (unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 1000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		/* we don't own the directory from the original mount */
		if (!unlinkat(dir_fd, FILE1, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		if (!unlinkat(dir_fd, FILE2, 0))
			die("failure: unlinkat");
		if (errno != EPERM)
			die("failure: errno");

		/* we own the file from the idmapped mount */
		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: unlinkat");
		if (unlinkat(open_tree_fd, FILE2, 0))
			die("failure: unlinkat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(dir_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int sticky_bit_rename(void)
{
	int fret = -1;
	int dir_fd = -EBADF;
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE1_RENAME, dir_fd, FILE1))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2_RENAME, dir_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (!renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 1000, 0))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 1000, 2000))
			die("failure: expected_uid_gid");

		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE1_RENAME, dir_fd, FILE1))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2_RENAME, dir_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 1000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}


	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE1_RENAME, dir_fd, FILE1))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2_RENAME, dir_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(dir_fd);

	return fret;
}

static int sticky_bit_rename_idmapped_mounts(void)
{
	int fret = -1;
	int dir_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (fchown(dir_fd, 10000, 10000)) {
		log_stderr("failure: fchown");
		goto out;
	}

	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 10000, 10000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 12000, 12000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(dir_fd, "",
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

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE1_RENAME, open_tree_fd, FILE1))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2_RENAME, open_tree_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (!renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 11000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 11000, 10000))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 11000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 11000, 12000))
			die("failure: expected_uid_gid");

		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE1_RENAME, open_tree_fd, FILE1))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2_RENAME, open_tree_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 11000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE1_RENAME, open_tree_fd, FILE1))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2_RENAME, open_tree_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(dir_fd);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that the sticky bit behaves correctly on idmapped mounts for unlink
 * operations in a user namespace.
 */
static int sticky_bit_rename_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int dir_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE2, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE2, 2000, 2000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE2, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(dir_fd, "",
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

	/* The sticky bit is not set so we must be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(dir_fd, FILE1_RENAME, dir_fd, FILE1))
			die("failure: renameat");

		if (renameat(dir_fd, FILE2_RENAME, dir_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* set sticky bit */
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* The sticky bit is set so we must not be able to delete files not
	 * owned by us.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (!renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* The sticky bit is set and we own the files so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* change ownership */
		if (fchownat(dir_fd, FILE1, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE1, 0, 1000, 0))
			die("failure: expected_uid_gid");
		if (fchownat(dir_fd, FILE2, 1000, -1, 0))
			die("failure: fchownat");
		if (!expected_uid_gid(dir_fd, FILE2, 0, 1000, 2000))
			die("failure: expected_uid_gid");

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		if (!renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE1_RENAME, open_tree_fd, FILE1))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2_RENAME, open_tree_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* change uid to unprivileged user */
	if (fchown(dir_fd, 1000, -1)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* The sticky bit is set and we own the directory so we must be able to
	 * delete the files now.
	 */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		/* we don't own the directory from the original mount */
		if (!renameat(dir_fd, FILE1, dir_fd, FILE1_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		if (!renameat(dir_fd, FILE2, dir_fd, FILE2_RENAME))
			die("failure: renameat");
		if (errno != EPERM)
			die("failure: errno");

		/* we own the file from the idmapped mount */
		if (renameat(open_tree_fd, FILE1, open_tree_fd, FILE1_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2, open_tree_fd, FILE2_RENAME))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE1_RENAME, open_tree_fd, FILE1))
			die("failure: renameat");

		if (renameat(open_tree_fd, FILE2_RENAME, open_tree_fd, FILE2))
			die("failure: renameat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(open_tree_fd);
	safe_close(attr.userns_fd);
	safe_close(dir_fd);

	return fret;
}

/* Validate that protected symlinks work correctly. */
static int protected_symlinks(void)
{
	int fret = -1;
	int dir_fd = -EBADF, fd = -EBADF;
	pid_t pid;

	if (!protected_symlinks_enabled())
		return 0;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create symlinks */
	if (symlinkat(FILE1, dir_fd, SYMLINK_USER1)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER1, 0, 0, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER1, AT_SYMLINK_NOFOLLOW, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER2)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER2, 1000, 1000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER2, AT_SYMLINK_NOFOLLOW, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER3)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER3, 2000, 2000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER3, AT_SYMLINK_NOFOLLOW, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* validate file can be directly read */
	fd = openat(dir_fd, FILE1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	/* validate file can be read through own symlink */
	fd = openat(dir_fd, SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		/* validate file can be directly read */
		fd = openat(dir_fd, FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(dir_fd, SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(dir_fd, SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(dir_fd, SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(2000, 2000))
			die("failure: switch_ids");

		/* validate file can be directly read */
		fd = openat(dir_fd, FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(dir_fd, SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(dir_fd, SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(dir_fd, SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(fd);
	safe_close(dir_fd);

	return fret;
}

/* Validate that protected symlinks work correctly on idmapped mounts. */
static int protected_symlinks_idmapped_mounts(void)
{
	int fret = -1;
	int dir_fd = -EBADF, fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!protected_symlinks_enabled())
		return 0;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 10000, 10000)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 10000, 10000, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create symlinks */
	if (symlinkat(FILE1, dir_fd, SYMLINK_USER1)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER1, 10000, 10000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER1, AT_SYMLINK_NOFOLLOW, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER2)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER2, 11000, 11000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER2, AT_SYMLINK_NOFOLLOW, 11000, 11000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER3)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER3, 12000, 12000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER3, AT_SYMLINK_NOFOLLOW, 12000, 12000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 10000, 10000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(10000, 0, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0) {
		log_stderr("failure: open_tree_fd");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* validate file can be directly read */
	fd = openat(open_tree_fd, DIR1 "/"  FILE1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	/* validate file can be read through own symlink */
	fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		/* validate file can be directly read */
		fd = openat(open_tree_fd, DIR1 "/" FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(2000, 2000))
			die("failure: switch_ids");

		/* validate file can be directly read */
		fd = openat(open_tree_fd, DIR1 "/" FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(fd);
	safe_close(dir_fd);
	safe_close(open_tree_fd);

	return fret;
}

/* Validate that protected symlinks work correctly on idmapped mounts inside a
 * user namespace.
 */
static int protected_symlinks_idmapped_mounts_in_userns(void)
{
	int fret = -1;
	int dir_fd = -EBADF, fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!protected_symlinks_enabled())
		return 0;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(dir_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir_fd, 0777 | S_ISVTX)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	/* validate sticky bit is set */
	if (!is_sticky(t_dir1_fd, DIR1, 0)) {
		log_stderr("failure: is_sticky");
		goto out;
	}

	/* create regular file via mknod */
	if (mknodat(dir_fd, FILE1, S_IFREG | 0000, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}
	if (fchownat(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (fchmodat(dir_fd, FILE1, 0644, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* create symlinks */
	if (symlinkat(FILE1, dir_fd, SYMLINK_USER1)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER1, 0, 0, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER1, AT_SYMLINK_NOFOLLOW, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER2)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER2, 1000, 1000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER2, AT_SYMLINK_NOFOLLOW, 1000, 1000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (symlinkat(FILE1, dir_fd, SYMLINK_USER3)) {
		log_stderr("failure: symlinkat");
		goto out;
	}
	if (fchownat(dir_fd, SYMLINK_USER3, 2000, 2000, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, SYMLINK_USER3, AT_SYMLINK_NOFOLLOW, 2000, 2000)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}
	if (!expected_uid_gid(dir_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	/* validate file can be directly read */
	fd = openat(open_tree_fd, DIR1 "/" FILE1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	/* validate file can be read through own symlink */
	fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
	if (fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	safe_close(fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		/* validate file can be directly read */
		fd = openat(open_tree_fd, DIR1 "/" FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 2000, 2000, true))
			die("failure: switch_userns");

		/* validate file can be directly read */
		fd = openat(open_tree_fd, DIR1 "/" FILE1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through own symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER3, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can be read through root symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER1, O_RDONLY | O_CLOEXEC, 0);
		if (fd < 0)
			die("failure: openat");
		safe_close(fd);

		/* validate file can't be read through other users symlink */
		fd = openat(open_tree_fd, DIR1 "/" SYMLINK_USER2, O_RDONLY | O_CLOEXEC, 0);
		if (fd >= 0)
			die("failure: openat");
		if (errno != EACCES)
			die("failure: errno");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(dir_fd);
	safe_close(open_tree_fd);
	safe_close(attr.userns_fd);

	return fret;
}

static int acls(void)
{
	int fret = -1;
	int dir1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (mkdirat(t_dir1_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}
	if (fchmodat(t_dir1_fd, DIR1, 0777, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	if (mkdirat(t_dir1_fd, DIR2, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}
	if (fchmodat(t_dir1_fd, DIR2, 0777, 0)) {
		log_stderr("failure: fchmodat");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd = get_userns_fd(100010, 100020, 5);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, DIR1,
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

	if (sys_move_mount(open_tree_fd, "", t_dir1_fd, DIR2, MOVE_MOUNT_F_EMPTY_PATH)) {
		log_stderr("failure: sys_move_mount");
		goto out;
	}

	dir1_fd = openat(t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (dir1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (mkdirat(dir1_fd, DIR3, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}
	if (fchown(dir1_fd, 100010, 100010)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(dir1_fd, 0777)) {
		log_stderr("failure: fchmod");
		goto out;
	}

	snprintf(t_buf, sizeof(t_buf), "setfacl -m u:100010:rwx %s/%s/%s/%s", t_mountpoint, T_DIR1, DIR1, DIR3);
	if (system(t_buf)) {
		log_stderr("failure: system");
		goto out;
	}

	snprintf(t_buf, sizeof(t_buf), "getfacl -p %s/%s/%s/%s | grep -q user:100010:rwx", t_mountpoint, T_DIR1, DIR1, DIR3);
	if (system(t_buf)) {
		log_stderr("failure: system");
		goto out;
	}

	snprintf(t_buf, sizeof(t_buf), "getfacl -p %s/%s/%s/%s | grep -q user:100020:rwx", t_mountpoint, T_DIR1, DIR2, DIR3);
	if (system(t_buf)) {
		log_stderr("failure: system");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 100010, 100010, true))
			die("failure: switch_userns");

		snprintf(t_buf, sizeof(t_buf), "getfacl -p %s/%s/%s/%s | grep -q user:%lu:rwx",
			 t_mountpoint, T_DIR1, DIR1, DIR3, 4294967295LU);
		if (system(t_buf))
			die("failure: system");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 100010, 100010, true))
			die("failure: switch_userns");

		snprintf(t_buf, sizeof(t_buf), "getfacl -p %s/%s/%s/%s | grep -q user:%lu:rwx",
			 t_mountpoint, T_DIR1, DIR2, DIR3, 100010LU);
		if (system(t_buf))
			die("failure: system");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* Now, dir is owned by someone else in the user namespace, but we can
	 * still read it because of acls.
	 */
	if (fchown(dir1_fd, 100012, 100012)) {
		log_stderr("failure: fchown");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int fd;

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 100010, 100010, true))
			die("failure: switch_userns");

		fd = openat(open_tree_fd, DIR3, O_CLOEXEC | O_DIRECTORY);
		if (fd < 0)
			die("failure: openat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	/* if we delete the acls, the ls should fail because it's 700. */
	snprintf(t_buf, sizeof(t_buf), "%s/%s/%s/%s", t_mountpoint, T_DIR1, DIR1, DIR3);
	if (removexattr(t_buf, "system.posix_acl_access")) {
		log_stderr("failure: removexattr");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		int fd;

		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 100010, 100010, true))
			die("failure: switch_userns");

		fd = openat(open_tree_fd, DIR3, O_CLOEXEC | O_DIRECTORY);
		if (fd >= 0)
			die("failure: openat");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	snprintf(t_buf, sizeof(t_buf), "%s/" T_DIR1 "/" DIR2, t_mountpoint);
	sys_umount2(t_buf, MNT_DETACH);

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(dir1_fd);
	safe_close(open_tree_fd);

	return fret;
}

#ifdef HAVE_LIBURING_H
static int io_uring_openat_with_creds(struct io_uring *ring, int dfd, const char *path, int cred_id,
				      bool with_link, int *ret_cqe)
{
	struct io_uring_cqe *cqe;
	struct io_uring_sqe *sqe;
	int ret, i, to_submit = 1;

	if (with_link) {
		sqe = io_uring_get_sqe(ring);
		if (!sqe)
			return log_error_errno(-EINVAL, EINVAL, "failure: io_uring_sqe");
		io_uring_prep_nop(sqe);
		sqe->flags |= IOSQE_IO_LINK;
		sqe->user_data = 1;
		to_submit++;
	}

	sqe = io_uring_get_sqe(ring);
	if (!sqe)
		return log_error_errno(-EINVAL, EINVAL, "failure: io_uring_sqe");
	io_uring_prep_openat(sqe, dfd, path, O_RDONLY | O_CLOEXEC, 0);
	sqe->user_data = 2;

	if (cred_id != -1)
		sqe->personality = cred_id;

	ret = io_uring_submit(ring);
	if (ret != to_submit) {
		log_stderr("failure: io_uring_submit");
		goto out;
	}

	for (i = 0; i < to_submit; i++) {
		ret = io_uring_wait_cqe(ring, &cqe);
		if (ret < 0) {
			log_stderr("failure: io_uring_wait_cqe");
			goto out;
		}

		ret = cqe->res;
		/*
		 * Make sure caller can identify that this is a proper io_uring
		 * failure and not some earlier error.
		 */
		if (ret_cqe)
			*ret_cqe = ret;
		io_uring_cqe_seen(ring, cqe);
	}
	log_debug("Ran test");
out:
	return ret;
}

static int io_uring(void)
{
	int fret = -1;
	int file1_fd = -EBADF;
	struct io_uring *ring;
	int cred_id, ret, ret_cqe;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* Verify we can open it with our current credentials. */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		/* Verify we can't open it with our current credentials. */
		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure %d", ret_cqe);
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(1000, 1000))
			die("failure: switch_ids");

		/* Verify we can open it with the registered credentials. */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		/* Verify we can open it with the registered credentials and as
		 * a link.
		 */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      cred_id, true, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(file1_fd);

	return fret;
}

static int io_uring_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, userns_fd = -EBADF;
	struct io_uring *ring;
	int cred_id, ret, ret_cqe;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	userns_fd = get_userns_fd(0, 10000, 10000);
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
		/* Verify we can open it with our current credentials. */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
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

		/* Verify we can't open it with our current credentials. */
		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
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

		/* Verify we can open it with the registered credentials. */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		/* Verify we can open it with the registered credentials and as
		 * a link.
		 */
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      cred_id, true, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(file1_fd);
	safe_close(userns_fd);

	return fret;
}

static int io_uring_idmapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct io_uring *ring;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	int cred_id, ret;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0)
		return log_errno(-1, "failure: create user namespace");

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0)
		return log_errno(-1, "failure: create detached mount");

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr)))
		return log_errno(-1, "failure: set mount attributes");

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(10000, 10000))
			die("failure: switch_ids");

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      -1, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(10001, 10001))
			die("failure: switch_ids");

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, true, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

/*
 * Create an idmapped mount where the we leave the owner of the file unmapped.
 * In no circumstances, even with recorded credentials can it be allowed to
 * open the file.
 */
static int io_uring_idmapped_unmapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct io_uring *ring;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	int cred_id, ret, ret_cqe;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(1, 10000, 10000);
	if (attr.userns_fd < 0)
		return log_errno(-1, "failure: create user namespace");

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0)
		return log_errno(-1, "failure: create detached mount");

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr)))
		return log_errno(-1, "failure: set mount attributes");

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(10000, 10000))
			die("failure: switch_ids");

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, true, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int io_uring_idmapped_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct io_uring *ring;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	int cred_id, ret, ret_cqe;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0)
		return log_errno(-1, "failure: create user namespace");

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0)
		return log_errno(-1, "failure: create detached mount");

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr)))
		return log_errno(-1, "failure: set mount attributes");

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      -1, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 1000, 1000, true))
			die("failure: switch_userns");

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, t_dir1_fd, FILE1,
						      -1, true, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      -1, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      -1, true, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, true, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int io_uring_idmapped_unmapped_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct io_uring *ring;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	int cred_id, ret, ret_cqe;
	pid_t pid;

	ring = mmap(0, sizeof(struct io_uring), PROT_READ|PROT_WRITE,
		   MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	if (!ring)
		return log_errno(-1, "failure: io_uring_queue_init");

	ret = io_uring_queue_init(8, ring, 0);
	if (ret) {
		log_stderr("failure: io_uring_queue_init");
		goto out_unmap;
	}

	ret = io_uring_register_personality(ring);
	if (ret < 0) {
		fret = 0;
		goto out_unmap; /* personalities not supported */
	}
	cred_id = ret;

	/* create file only owner can open */
	file1_fd = openat(t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}
	if (fchown(file1_fd, 0, 0)) {
		log_stderr("failure: fchown");
		goto out;
	}
	if (fchmod(file1_fd, 0600)) {
		log_stderr("failure: fchmod");
		goto out;
	}
	safe_close(file1_fd);

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(1, 10000, 10000);
	if (attr.userns_fd < 0)
		return log_errno(-1, "failure: create user namespace");

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
				     AT_EMPTY_PATH |
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0)
		return log_errno(-1, "failure: create detached mount");

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr)))
		return log_errno(-1, "failure: set mount attributes");

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 10000, 10000, true))
			die("failure: switch_ids");

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, false, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		ret_cqe = 0;
		file1_fd = io_uring_openat_with_creds(ring, open_tree_fd, FILE1,
						      cred_id, true, &ret_cqe);
		if (file1_fd >= 0)
			die("failure: io_uring_open_file");
		if (ret_cqe == 0)
			die("failure: non-open() related io_uring_open_file failure");
		if (ret_cqe != -EACCES)
			die("failure: errno(%d)", abs(ret_cqe));

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid)) {
		log_stderr("failure: wait_for_pid");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	ret = io_uring_unregister_personality(ring, cred_id);
	if (ret)
		log_stderr("failure: io_uring_unregister_personality");

out_unmap:
	munmap(ring, sizeof(struct io_uring));

	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}
#endif /* HAVE_LIBURING_H */

/* The following tests are concerned with setgid inheritance. These can be
 * filesystem type specific. For xfs, if a new file or directory is created
 * within a setgid directory and irix_sgid_inhiert is set then inherit the
 * setgid bit if the caller is in the group of the directory.
 */
static int setgid_create(void)
{
	int fret = -1;
	int file1_fd = -EBADF;
	pid_t pid;

	if (!caps_supported())
		return 0;

	if (fchmod(t_dir1_fd, S_IRUSR |
			      S_IWUSR |
			      S_IRGRP |
			      S_IWGRP |
			      S_IROTH |
			      S_IWOTH |
			      S_IXUSR |
			      S_IXGRP |
			      S_IXOTH |
			      S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the setgid bit got raised. */
	if (!is_setgid(t_dir1_fd, "", AT_EMPTY_PATH)) {
		log_stderr("failure: is_setgid");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* create regular file via open() */
		file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* We're capable_wrt_inode_uidgid() and also our fsgid matches
		 * the directories gid.
		 */
		if (!is_setgid(t_dir1_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(t_dir1_fd, DIR1, 0000))
			die("failure: create");

		/* Directories always inherit the setgid bit. */
		if (!is_setgid(t_dir1_fd, DIR1, 0))
			die("failure: is_setgid");

		if (unlinkat(t_dir1_fd, FILE1, 0))
			die("failure: delete");

		if (unlinkat(t_dir1_fd, DIR1, AT_REMOVEDIR))
			die("failure: delete");

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
		if (!switch_ids(0, 10000))
			die("failure: switch_ids");

		if (!caps_down())
			die("failure: caps_down");

		/* create regular file via open() */
		file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* Neither in_group_p() nor capable_wrt_inode_uidgid() so setgid
		 * bit needs to be stripped.
		 */
		if (is_setgid(t_dir1_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(t_dir1_fd, DIR1, 0000))
			die("failure: create");

		if (xfs_irix_sgid_inherit_enabled()) {
			/* We're not in_group_p(). */
			if (is_setgid(t_dir1_fd, DIR1, 0))
				die("failure: is_setgid");
		} else {
			/* Directories always inherit the setgid bit. */
			if (!is_setgid(t_dir1_fd, DIR1, 0))
				die("failure: is_setgid");
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);

	return fret;
}

static int setgid_create_idmapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	if (fchmod(t_dir1_fd, S_IRUSR |
			      S_IWUSR |
			      S_IRGRP |
			      S_IWGRP |
			      S_IROTH |
			      S_IWOTH |
			      S_IXUSR |
			      S_IXGRP |
			      S_IXOTH |
			      S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setgid(t_dir1_fd, "", AT_EMPTY_PATH)) {
		log_stderr("failure: is_setgid");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(10000, 11000))
			die("failure: switch fsids");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* Neither in_group_p() nor capable_wrt_inode_uidgid() so setgid
		 * bit needs to be stripped.
		 */
		if (is_setgid(open_tree_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0000))
			die("failure: create");

		if (xfs_irix_sgid_inherit_enabled()) {
			/* We're not in_group_p(). */
			if (is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		} else {
			/* Directories always inherit the setgid bit. */
			if (!is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int setgid_create_idmapped_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};
	pid_t pid;

	if (!caps_supported())
		return 0;

	if (fchmod(t_dir1_fd, S_IRUSR |
			      S_IWUSR |
			      S_IRGRP |
			      S_IWGRP |
			      S_IROTH |
			      S_IWOTH |
			      S_IXUSR |
			      S_IXGRP |
			      S_IXOTH |
			      S_ISGID), 0) {
		log_stderr("failure: fchmod");
		goto out;
	}

	/* Verify that the sid bits got raised. */
	if (!is_setgid(t_dir1_fd, "", AT_EMPTY_PATH)) {
		log_stderr("failure: is_setgid");
		goto out;
	}

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* We're in_group_p() and capable_wrt_inode_uidgid() so setgid
		 * bit needs to be set.
		 */
		if (!is_setgid(open_tree_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0000))
			die("failure: create");

		/* Directories always inherit the setgid bit. */
		if (!is_setgid(open_tree_fd, DIR1, 0))
			die("failure: is_setgid");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(open_tree_fd, DIR1, 0, 0, 0))
			die("failure: check ownership");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: delete");

		if (unlinkat(open_tree_fd, DIR1, AT_REMOVEDIR))
			die("failure: delete");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (fchownat(t_dir1_fd, "", -1, 1000, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	if (fchownat(t_dir1_fd, "", -1, 1000, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 0, 0, true))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* Neither in_group_p() nor capable_wrt_inode_uidgid() so setgid
		 * bit needs to be stripped.
		 */
		if (is_setgid(open_tree_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0000))
			die("failure: create");

		if (xfs_irix_sgid_inherit_enabled()) {
			/* We're not in_group_p(). */
			if (is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		} else {
			/* Directories always inherit the setgid bit. */
			if (!is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		}

		/* Files and directories created in setgid directories inherit
		 * the i_gid of the parent directory.
		 */
		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 1000))
			die("failure: check ownership");

		if (!expected_uid_gid(open_tree_fd, DIR1, 0, 0, 1000))
			die("failure: check ownership");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: delete");

		if (unlinkat(open_tree_fd, DIR1, AT_REMOVEDIR))
			die("failure: delete");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (fchownat(t_dir1_fd, "", -1, 0, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	if (fchownat(t_dir1_fd, "", -1, 0, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 0, 1000, true))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* Neither in_group_p() nor capable_wrt_inode_uidgid() so setgid
		 * bit needs to be stripped.
		 */
		if (is_setgid(open_tree_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(open_tree_fd, DIR1, 0000))
			die("failure: create");

		/* Directories always inherit the setgid bit. */
		if (xfs_irix_sgid_inherit_enabled()) {
			/* We're not in_group_p(). */
			if (is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		} else {
			/* Directories always inherit the setgid bit. */
			if (!is_setgid(open_tree_fd, DIR1, 0))
				die("failure: is_setgid");
		}

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(open_tree_fd, DIR1, 0, 0, 0))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

#define PTR_TO_INT(p) ((int)((intptr_t)(p)))
#define INT_TO_PTR(u) ((void *)((intptr_t)(u)))

static void *idmapped_mount_create_cb(void *data)
{
	int fret = EXIT_FAILURE, open_tree_fd = PTR_TO_INT(data);
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	fret = EXIT_SUCCESS;

out:
	safe_close(attr.userns_fd);
	pthread_exit(INT_TO_PTR(fret));
}

/* This tries to verify that we never see an inconistent ownership on-disk and
 * can't write invalid ids to disk. To do this we create a race between
 * idmapping a mount and creating files on it.
 * Note, while it is perfectly fine to see overflowuid and overflowgid as owner
 * if we create files through the open_tree_fd before the mount is idmapped but
 * look at the files after the mount has been idmapped in this test it can never
 * be the case that we see overflowuid and overflowgid when we access the file
 * through a non-idmapped mount (in the initial user namespace).
 */
static void *idmapped_mount_operations_cb(void *data)
{
	int file1_fd = -EBADF, file2_fd = -EBADF, dir1_fd = -EBADF,
	    dir1_fd2 = -EBADF, fret = EXIT_FAILURE,
	    open_tree_fd = PTR_TO_INT(data);

	if (!switch_fsids(10000, 10000)) {
		log_stderr("failure: switch fsids");
		goto out;
	}

	file1_fd = openat(open_tree_fd, FILE1,
			  O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	file2_fd = openat(open_tree_fd, FILE2,
			  O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file2_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (mkdirat(open_tree_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir1_fd = openat(open_tree_fd, DIR1,
			 O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dir1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (!__expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0, false) &&
	    !__expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000, false) &&
	    !__expected_uid_gid(open_tree_fd, FILE1, 0, t_overflowuid, t_overflowgid, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(open_tree_fd, FILE2, 0, 0, 0, false) &&
	    !__expected_uid_gid(open_tree_fd, FILE2, 0, 10000, 10000, false) &&
	    !__expected_uid_gid(open_tree_fd, FILE2, 0, t_overflowuid, t_overflowgid, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(open_tree_fd, DIR1, 0, 0, 0, false) &&
	    !__expected_uid_gid(open_tree_fd, DIR1, 0, 10000, 10000, false) &&
	    !__expected_uid_gid(open_tree_fd, DIR1, 0, t_overflowuid, t_overflowgid, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(dir1_fd, "", AT_EMPTY_PATH, 0, 0, false) &&
	    !__expected_uid_gid(dir1_fd, "", AT_EMPTY_PATH, 10000, 10000, false) &&
	    !__expected_uid_gid(dir1_fd, "", AT_EMPTY_PATH, t_overflowuid, t_overflowgid, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	dir1_fd2 = openat(t_dir1_fd, DIR1,
			 O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (dir1_fd2 < 0) {
		log_stderr("failure: openat");
		goto out;
	}

        if (!__expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0, false) &&
	    !__expected_uid_gid(t_dir1_fd, FILE1, 0, 10000, 10000, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(t_dir1_fd, FILE2, 0, 0, 0, false) &&
	    !__expected_uid_gid(t_dir1_fd, FILE2, 0, 10000, 10000, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(t_dir1_fd, DIR1, 0, 0, 0, false) &&
	    !__expected_uid_gid(t_dir1_fd, DIR1, 0, 10000, 10000, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(t_dir1_fd, DIR1, 0, 0, 0, false) &&
	    !__expected_uid_gid(t_dir1_fd, DIR1, 0, 10000, 10000, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	if (!__expected_uid_gid(dir1_fd2, "", AT_EMPTY_PATH, 0, 0, false) &&
	    !__expected_uid_gid(dir1_fd2, "", AT_EMPTY_PATH, 10000, 10000, false)) {
		log_stderr("failure: expected_uid_gid");
		goto out;
	}

	fret = EXIT_SUCCESS;

out:
	safe_close(file1_fd);
	safe_close(file2_fd);
	safe_close(dir1_fd);
	safe_close(dir1_fd2);

	pthread_exit(INT_TO_PTR(fret));
}

static int threaded_idmapped_mount_interactions(void)
{
	int i;
	int fret = -1;
	pid_t pid;
	pthread_attr_t thread_attr;
	pthread_t threads[2];

	pthread_attr_init(&thread_attr);

	for (i = 0; i < 1000; i++) {
		int ret1 = 0, ret2 = 0, tret1 = 0, tret2 = 0;

		pid = fork();
		if (pid < 0) {
			log_stderr("failure: fork");
			goto out;
		}
		if (pid == 0) {
			int open_tree_fd = -EBADF;

			open_tree_fd = sys_open_tree(t_dir1_fd, "",
						     AT_EMPTY_PATH |
						     AT_NO_AUTOMOUNT |
						     AT_SYMLINK_NOFOLLOW |
						     OPEN_TREE_CLOEXEC |
						     OPEN_TREE_CLONE);
			if (open_tree_fd < 0)
				die("failure: sys_open_tree");

			if (pthread_create(&threads[0], &thread_attr,
					   idmapped_mount_create_cb,
					   INT_TO_PTR(open_tree_fd)))
				die("failure: pthread_create");

			if (pthread_create(&threads[1], &thread_attr,
					   idmapped_mount_operations_cb,
					   INT_TO_PTR(open_tree_fd)))
				die("failure: pthread_create");

			ret1 = pthread_join(threads[0], INT_TO_PTR(tret1));
			ret2 = pthread_join(threads[1], INT_TO_PTR(tret2));

			if (ret1) {
				errno = ret1;
				die("failure: pthread_join");
			}

			if (ret2) {
				errno = ret2;
				die("failure: pthread_join");
			}

			if (tret1 || tret2)
				exit(EXIT_FAILURE);

			exit(EXIT_SUCCESS);

		}

		if (wait_for_pid(pid)) {
			log_stderr("failure: iteration %d", i);
			goto out;
		}

		rm_r(t_dir1_fd, ".");

	}

	fret = 0;
	log_debug("Ran test");

out:
	return fret;
}

static int setattr_truncate(void)
{
	int fret = -1;
	int file1_fd = -EBADF;

	/* create regular file via open() */
	file1_fd = openat(t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
	if (file1_fd < 0) {
		log_stderr("failure: create");
		goto out;
	}

	if (ftruncate(file1_fd, 10000)) {
		log_stderr("failure: ftruncate");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: check ownership");
		goto out;
	}

	if (!expected_file_size(file1_fd, "", AT_EMPTY_PATH, 10000)) {
		log_stderr("failure: expected_file_size");
		goto out;
	}

	if (ftruncate(file1_fd, 0)) {
		log_stderr("failure: ftruncate");
		goto out;
	}

	if (!expected_uid_gid(t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: check ownership");
		goto out;
	}

	if (!expected_file_size(file1_fd, "", AT_EMPTY_PATH, 0)) {
		log_stderr("failure: expected_file_size");
		goto out;
	}

	if (unlinkat(t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: remove");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);

	return fret;
}

static int setattr_truncate_idmapped(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	pid_t pid;
	struct mount_attr attr = {
		.attr_set = MOUNT_ATTR_IDMAP,
	};

	/* Changing mount properties on a detached mount. */
	attr.userns_fd	= get_userns_fd(0, 10000, 10000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_ids(10000, 10000))
			die("failure: switch_ids");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		if (ftruncate(file1_fd, 10000))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 10000))
			die("failure: expected_file_size");

		if (ftruncate(file1_fd, 0))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 0))
			die("failure: expected_file_size");

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
		int file1_fd2 = -EBADF;

		/* create regular file via open() */
		file1_fd2 = openat(open_tree_fd, FILE1, O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd2 < 0)
			die("failure: create");

		if (ftruncate(file1_fd2, 10000))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 10000))
			die("failure: expected_file_size");

		if (ftruncate(file1_fd2, 0))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 10000, 10000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 0))
			die("failure: expected_file_size");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static int setattr_truncate_idmapped_in_userns(void)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
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

	open_tree_fd = sys_open_tree(t_dir1_fd, "",
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

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		if (ftruncate(file1_fd, 10000))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 10000))
			die("failure: expected_file_size");

		if (ftruncate(file1_fd, 0))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 0))
			die("failure: expected_file_size");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: delete");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (fchownat(t_dir1_fd, "", -1, 1000, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	if (fchownat(t_dir1_fd, "", -1, 1000, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 0, 0, true))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		if (ftruncate(file1_fd, 10000))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 10000))
			die("failure: expected_file_size");

		if (ftruncate(file1_fd, 0))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 0))
			die("failure: expected_file_size");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: delete");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	if (fchownat(t_dir1_fd, "", -1, 0, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	if (fchownat(t_dir1_fd, "", -1, 0, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!caps_supported()) {
			log_debug("skip: capability library not installed");
			exit(EXIT_SUCCESS);
		}

		if (!switch_userns(attr.userns_fd, 0, 1000, true))
			die("failure: switch_userns");

		/* create regular file via open() */
		file1_fd = openat(open_tree_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		if (ftruncate(file1_fd, 10000))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 1000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 10000))
			die("failure: expected_file_size");

		if (ftruncate(file1_fd, 0))
			die("failure: ftruncate");

		if (!expected_uid_gid(open_tree_fd, FILE1, 0, 0, 1000))
			die("failure: check ownership");

		if (!expected_file_size(open_tree_fd, FILE1, 0, 0))
			die("failure: expected_file_size");

		if (unlinkat(open_tree_fd, FILE1, 0))
			die("failure: delete");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(file1_fd);
	safe_close(open_tree_fd);

	return fret;
}

static void usage(void)
{
	fprintf(stderr, "Description:\n");
	fprintf(stderr, "    Run idmapped mount tests\n\n");

	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "-d --device        Device used in the tests\n");
	fprintf(stderr, "-m --mountpoint    Mountpoint of device\n");

	_exit(EXIT_SUCCESS);
}

static const struct option longopts[] = {
	{"device",	required_argument,	0,	'd'},
	{"fstype",	required_argument,	0,	'f'},
	{"mountpoint",	required_argument,	0,	'm'},
	{"supported",	no_argument,		0,	's'},
	{"help",	no_argument,		0,	'h'},
	{NULL,		0,			0,	0  },
};

struct t_idmapped_mounts {
	int (*test)(void);
	const char *description;
} t_idmapped_mounts[] = {
	{ acls,								"posix acls on regular mounts",									},
	{ create_in_userns,						"create operations in user namespace",								},
	{ device_node_in_userns,					"device node in user namespace",								},
	{ expected_uid_gid_idmapped_mounts,				"expected ownership on idmapped mounts",							},
	{ fscaps,							"fscaps on regular mounts",									},
	{ fscaps_idmapped_mounts,					"fscaps on idmapped mounts",									},
	{ fscaps_idmapped_mounts_in_userns,				"fscaps on idmapped mounts in user namespace",							},
	{ fscaps_idmapped_mounts_in_userns_separate_userns,		"fscaps on idmapped mounts in user namespace with different id mappings ",			},
	{ fsids_mapped,							"mapped fsids",											},
	{ fsids_unmapped,						"unmapped fsids",										},
	{ hardlink_crossing_mounts,					"cross mount hardlink",										},
	{ hardlink_crossing_idmapped_mounts,				"cross idmapped mount hardlink",								},
	{ hardlink_from_idmapped_mount,					"hardlinks from idmapped mounts",								},
	{ hardlink_from_idmapped_mount_in_userns,			"hardlinks from idmapped mounts in user namespace",						},
#ifdef HAVE_LIBURING_H
	{ io_uring,							"io_uring",											},
	{ io_uring_userns,						"io_uring in user namespace",									},
	{ io_uring_idmapped,						"io_uring from idmapped mounts",								},
	{ io_uring_idmapped_userns,					"io_uring from idmapped mounts in user namespace",						},
	{ io_uring_idmapped_unmapped,					"io_uring from idmapped mounts with unmapped ids",						},
	{ io_uring_idmapped_unmapped_userns,				"io_uring from idmapped mounts with unmapped ids in user namespace",				},
#endif
	{ protected_symlinks,						"following protected symlinks on regular mounts",						},
	{ protected_symlinks_idmapped_mounts,				"following protected symlinks on idmapped mounts",						},
	{ protected_symlinks_idmapped_mounts_in_userns,			"following protected symlinks on idmapped mounts in user namespace",				},
	{ rename_crossing_mounts,					"cross mount rename",										},
	{ rename_crossing_idmapped_mounts,				"cross idmapped mount rename",									},
	{ rename_from_idmapped_mount,					"rename from idmapped mounts",									},
	{ rename_from_idmapped_mount_in_userns,				"rename from idmapped mounts in user namespace",						},
	{ setattr_truncate,						"setattr truncate",										},
	{ setattr_truncate_idmapped,					"setattr truncate on idmapped mounts",								},
	{ setattr_truncate_idmapped_in_userns,				"setattr truncate on idmapped mounts in user namespace",					},
	{ setgid_create,						"create operations in directories with setgid bit set",						},
	{ setgid_create_idmapped,					"create operations in directories with setgid bit set on idmapped mounts",			},
	{ setgid_create_idmapped_in_userns,				"create operations in directories with setgid bit set on idmapped mounts in user namespace",	},
	{ setid_binaries,						"setid binaries on regular mounts",								},
	{ setid_binaries_idmapped_mounts,				"setid binaries on idmapped mounts",								},
	{ setid_binaries_idmapped_mounts_in_userns,			"setid binaries on idmapped mounts in user namespace",						},
	{ setid_binaries_idmapped_mounts_in_userns_separate_userns,	"setid binaries on idmapped mounts in user namespace with different id mappings",		},
	{ sticky_bit_unlink,						"sticky bit unlink operations on regular mounts",						},
	{ sticky_bit_unlink_idmapped_mounts,				"sticky bit unlink operations on idmapped mounts",						},
	{ sticky_bit_unlink_idmapped_mounts_in_userns,			"sticky bit unlink operations on idmapped mounts in user namespace",				},
	{ sticky_bit_rename,						"sticky bit rename operations on regular mounts",						},
	{ sticky_bit_rename_idmapped_mounts,				"sticky bit rename operations on idmapped mounts",						},
	{ sticky_bit_rename_idmapped_mounts_in_userns,			"sticky bit rename operations on idmapped mounts in user namespace",				},
	{ symlink_regular_mounts,					"symlink from regular mounts",									},
	{ symlink_idmapped_mounts,					"symlink from idmapped mounts",									},
	{ symlink_idmapped_mounts_in_userns,				"symlink from idmapped mounts in user namespace",						},
	{ threaded_idmapped_mount_interactions,				"threaded operations on idmapped mounts",							},
};

int main(int argc, char *argv[])
{
	int i, fret, ret;
	int index = 0;
	bool supported = false;

	while ((ret = getopt_long(argc, argv, "", longopts, &index)) != -1) {
		switch (ret) {
		case 'd':
			t_device = optarg;
			break;
		case 'f':
			t_fstype = optarg;
			break;
		case 'm':
			t_mountpoint = optarg;
			break;
		case 's':
			supported = true;
			break;
		case 'h':
			/* fallthrough */
		default:
			usage();
		}
	}

	if (!t_device)
		die_errno(EINVAL, "test device missing");

	if (!t_fstype)
		die_errno(EINVAL, "test filesystem type missing");

	if (!t_mountpoint)
		die_errno(EINVAL, "mountpoint of test device missing");

	/* create separate mount namespace */
	if (unshare(CLONE_NEWNS))
		die("failure: create new mount namespace");

	/* turn off mount propagation */
	if (sys_mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0))
		die("failure: turn mount propagation off");

	t_mnt_fd = openat(-EBADF, t_mountpoint, O_CLOEXEC | O_DIRECTORY);
	if (t_mnt_fd < 0)
		die("failed to open %s", t_mountpoint);

	/*
	 * Caller just wants to know whether the filesystem we're on supports
	 * idmapped mounts.
	 */
	if (supported) {
		int open_tree_fd = -EBADF;
		struct mount_attr attr = {
			.attr_set	= MOUNT_ATTR_IDMAP,
			.userns_fd	= -EBADF,
		};

		/* Changing mount properties on a detached mount. */
		attr.userns_fd	= get_userns_fd(0, 1000, 1);
		if (attr.userns_fd < 0)
			exit(EXIT_FAILURE);

		open_tree_fd = sys_open_tree(t_mnt_fd, "",
					     AT_EMPTY_PATH |
					     AT_NO_AUTOMOUNT |
					     AT_SYMLINK_NOFOLLOW |
					     OPEN_TREE_CLOEXEC |
					     OPEN_TREE_CLONE);
		if (open_tree_fd < 0)
			ret = -1;
		else
			ret = sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr));

		close(open_tree_fd);
		close(attr.userns_fd);

		if (ret)
			exit(EXIT_FAILURE);

		exit(EXIT_SUCCESS);
	}

	stash_overflowuid();
	stash_overflowgid();

	fret = EXIT_FAILURE;

	/* Proper test suite run. */
	for (i = 0; i < (sizeof(t_idmapped_mounts) / sizeof(t_idmapped_mounts[0])); i++) {
		struct t_idmapped_mounts *t = &t_idmapped_mounts[i];
		pid_t pid;

		test_setup();

		pid = fork();
		if (pid < 0)
			goto out;

		if (pid == 0) {
			ret = t->test();
			if (ret) {
				fprintf(stderr, "failure: %s\n", t->description);
				exit(EXIT_FAILURE);
			}

			exit(EXIT_SUCCESS);
		}

		ret = wait_for_pid(pid);
		test_cleanup();

		if (ret)
			goto out;
	}

	fret = EXIT_SUCCESS;

out:
	exit(fret);
}
