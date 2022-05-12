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

#include "btrfs-idmapped-mounts.h"
#include "idmapped-mounts.h"
#include "missing.h"
#include "utils.h"

static char t_buf[PATH_MAX];

static void init_vfstest_info(struct vfstest_info *info)
{
	info->t_overflowuid		= 65534;
	info->t_overflowgid		= 65534;
	info->t_fstype			= NULL;
	info->t_device			= NULL;
	info->t_device_scratch		= NULL;
	info->t_mountpoint		= NULL;
	info->t_mountpoint_scratch	= NULL;
	info->t_mnt_fd			= -EBADF;
	info->t_mnt_scratch_fd		= -EBADF;
	info->t_dir1_fd			= -EBADF;
	info->t_fs_allow_idmap		= false;
}

static void stash_overflowuid(struct vfstest_info *info)
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

	info->t_overflowuid = atoi(buf);
}

static void stash_overflowgid(struct vfstest_info *info)
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

	info->t_overflowgid = atoi(buf);
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
		fprintf(stderr, "mode(%o):uid(%d):gid(%d) -> %s\n",
			(st.st_mode & ~S_IFMT), st.st_uid, st.st_gid,
			(path && *path) ? path : "(null)");

	close(dfd_dup);
	closedir(dir);

	return ret;
}
#else
__attribute__((unused)) static int print_r(int fd, const char *path)
{
	return 0;
}
#endif

static void test_setup(struct vfstest_info *info)
{
	if (mkdirat(info->t_mnt_fd, T_DIR1, 0777))
		die("failure: mkdirat");

	info->t_dir1_fd = openat(info->t_mnt_fd, T_DIR1, O_CLOEXEC | O_DIRECTORY);
	if (info->t_dir1_fd < 0)
		die("failure: openat");

	if (fchmod(info->t_dir1_fd, 0777))
		die("failure: fchmod");
}

static void test_cleanup(struct vfstest_info *info)
{
	safe_close(info->t_dir1_fd);
	if (rm_r(info->t_mnt_fd, T_DIR1))
		die("failure: rm_r");
}

static int hardlink_crossing_mounts(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;

        if (chown_r(info->t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
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
	if (!linkat(open_tree_fd, FILE1, info->t_dir1_fd, HARDLINK1, 0)) {
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

static int rename_crossing_mounts(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;

	if (chown_r(info->t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
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
	if (!renameat(open_tree_fd, FILE1, info->t_dir1_fd, FILE1_RENAME)) {
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

static int symlink_regular_mounts(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF, open_tree_fd = -EBADF;
	struct stat st;

	file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	if (chown_r(info->t_mnt_fd, T_DIR1, 10000, 10000)) {
		log_stderr("failure: chown_r");
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

static int fscaps(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF, fd_userns = -EBADF;
	pid_t pid;

	file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (file1_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	/* Skip if vfs caps are unsupported. */
	if (set_dummy_vfs_caps(file1_fd, 0, 1000))
		return 0;

	/* Changing mount properties on a detached mount. */
	fd_userns = get_userns_fd(0, 10000, 10000);
	if (fd_userns < 0) {
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
		if (!switch_userns(fd_userns, 0, 0, false))
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
		if (!switch_userns(fd_userns, 0, 0, false))
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
	safe_close(file1_fd);
	safe_close(fd_userns);

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
static int setid_binaries(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF, exec_fd = -EBADF;
	pid_t pid;

	/* create a file to be used as setuid binary */
	file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC | O_RDWR, 0644);
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
	if (!is_setid(info->t_dir1_fd, FILE1, 0)) {
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

		if (!expected_uid_gid(info->t_dir1_fd, FILE1, 0, 5000, 5000))
			die("failure: expected_uid_gid");

		sys_execveat(info->t_dir1_fd, FILE1, argv, envp, 0);
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

static int sticky_bit_unlink(const struct vfstest_info *info)
{
	int fret = -1;
	int dir_fd = -EBADF;
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(info->t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(info->t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
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
	if (!is_sticky(info->t_dir1_fd, DIR1, 0)) {
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
	if (!is_sticky(info->t_dir1_fd, DIR1, 0)) {
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

static int sticky_bit_rename(const struct vfstest_info *info)
{
	int fret = -1;
	int dir_fd = -EBADF;
	pid_t pid;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(info->t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(info->t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
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
	if (!is_sticky(info->t_dir1_fd, DIR1, 0)) {
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
	if (!is_sticky(info->t_dir1_fd, DIR1, 0)) {
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

/* Validate that protected symlinks work correctly. */
static int protected_symlinks(const struct vfstest_info *info)
{
	int fret = -1;
	int dir_fd = -EBADF, fd = -EBADF;
	pid_t pid;

	if (!protected_symlinks_enabled())
		return 0;

	if (!caps_supported())
		return 0;

	/* create directory */
	if (mkdirat(info->t_dir1_fd, DIR1, 0000)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	dir_fd = openat(info->t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
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
	if (!is_sticky(info->t_dir1_fd, DIR1, 0)) {
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

#ifdef HAVE_LIBURING_H
static int io_uring(const struct vfstest_info *info)
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
	file1_fd = openat(info->t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		/* Verify we can open it with the registered credentials and as
		 * a link.
		 */
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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

static int io_uring_userns(const struct vfstest_info *info)
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
	file1_fd = openat(info->t_dir1_fd, FILE1, O_RDONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0000);
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
						      cred_id, false, NULL);
		if (file1_fd < 0)
			die("failure: io_uring_open_file");

		/* Verify we can open it with the registered credentials and as
		 * a link.
		 */
		file1_fd = io_uring_openat_with_creds(ring, info->t_dir1_fd, FILE1,
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
#endif /* HAVE_LIBURING_H */

/* The following tests are concerned with setgid inheritance. These can be
 * filesystem type specific. For xfs, if a new file or directory or node is
 * created within a setgid directory and irix_sgid_inhiert is set then inherit
 * the setgid bit if the caller is in the group of the directory.
 */
static int setgid_create(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF;
	int tmpfile_fd = -EBADF;
	pid_t pid;
	bool supported = false;

	if (!caps_supported())
		return 0;

	if (fchmod(info->t_dir1_fd, S_IRUSR |
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
	if (!is_setgid(info->t_dir1_fd, "", AT_EMPTY_PATH)) {
		log_stderr("failure: is_setgid");
		goto out;
	}

	supported = openat_tmpfile_supported(info->t_dir1_fd);

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* create regular file via open() */
		file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* We're capable_wrt_inode_uidgid() and also our fsgid matches
		 * the directories gid.
		 */
		if (!is_setgid(info->t_dir1_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(info->t_dir1_fd, DIR1, 0000))
			die("failure: create");

		/* Directories always inherit the setgid bit. */
		if (!is_setgid(info->t_dir1_fd, DIR1, 0))
			die("failure: is_setgid");

		/* create a special file via mknodat() vfs_create */
		if (mknodat(info->t_dir1_fd, FILE2, S_IFREG | S_ISGID | S_IXGRP, 0))
			die("failure: mknodat");

		if (!is_setgid(info->t_dir1_fd, FILE2, 0))
			die("failure: is_setgid");

		/* create a character device via mknodat() vfs_mknod */
		if (mknodat(info->t_dir1_fd, CHRDEV1, S_IFCHR | S_ISGID | S_IXGRP, makedev(5, 1)))
			die("failure: mknodat");

		if (!is_setgid(info->t_dir1_fd, CHRDEV1, 0))
			die("failure: is_setgid");

		if (!expected_uid_gid(info->t_dir1_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(info->t_dir1_fd, DIR1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(info->t_dir1_fd, FILE2, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(info->t_dir1_fd, CHRDEV1, 0, 0, 0))
			die("failure: check ownership");

		if (unlinkat(info->t_dir1_fd, FILE1, 0))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, DIR1, AT_REMOVEDIR))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, FILE2, 0))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, CHRDEV1, 0))
			die("failure: delete");

		/* create tmpfile via filesystem tmpfile api */
		if (supported) {
			tmpfile_fd = openat(info->t_dir1_fd, ".", O_TMPFILE | O_RDWR, S_IXGRP | S_ISGID);
			if (tmpfile_fd < 0)
				die("failure: create");
			/* link the temporary file into the filesystem, making it permanent */
			if (linkat(tmpfile_fd, "", info->t_dir1_fd, FILE3, AT_EMPTY_PATH))
				die("failure: linkat");
			if (close(tmpfile_fd))
				die("failure: close");
			if (!is_setgid(info->t_dir1_fd, FILE3, 0))
				die("failure: is_setgid");
			if (!expected_uid_gid(info->t_dir1_fd, FILE3, 0, 0, 0))
				die("failure: check ownership");
			if (unlinkat(info->t_dir1_fd, FILE3, 0))
				die("failure: delete");
		}

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

		if (!caps_down_fsetid())
			die("failure: caps_down_fsetid");

		/* create regular file via open() */
		file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_CLOEXEC, S_IXGRP | S_ISGID);
		if (file1_fd < 0)
			die("failure: create");

		/* Neither in_group_p() nor capable_wrt_inode_uidgid() so setgid
		 * bit needs to be stripped.
		 */
		if (is_setgid(info->t_dir1_fd, FILE1, 0))
			die("failure: is_setgid");

		/* create directory */
		if (mkdirat(info->t_dir1_fd, DIR1, 0000))
			die("failure: create");

		if (xfs_irix_sgid_inherit_enabled(info->t_fstype)) {
			/* We're not in_group_p(). */
			if (is_setgid(info->t_dir1_fd, DIR1, 0))
				die("failure: is_setgid");
		} else {
			/* Directories always inherit the setgid bit. */
			if (!is_setgid(info->t_dir1_fd, DIR1, 0))
				die("failure: is_setgid");
		}

		/* create a special file via mknodat() vfs_create */
		if (mknodat(info->t_dir1_fd, FILE2, S_IFREG | S_ISGID | S_IXGRP, 0))
			die("failure: mknodat");

		if (is_setgid(info->t_dir1_fd, FILE2, 0))
			die("failure: is_setgid");

		/* create a character device via mknodat() vfs_mknod */
		if (mknodat(info->t_dir1_fd, CHRDEV1, S_IFCHR | S_ISGID | S_IXGRP, makedev(5, 1)))
			die("failure: mknodat");

		if (is_setgid(info->t_dir1_fd, CHRDEV1, 0))
			die("failure: is_setgid");
		/*
		 * In setgid directories newly created files always inherit the
		 * gid from the parent directory. Verify that the file is owned
		 * by gid 0, not by gid 10000.
		 */
		if (!expected_uid_gid(info->t_dir1_fd, FILE1, 0, 0, 0))
			die("failure: check ownership");

		/*
		 * In setgid directories newly created directories always
		 * inherit the gid from the parent directory. Verify that the
		 * directory is owned by gid 0, not by gid 10000.
		 */
		if (!expected_uid_gid(info->t_dir1_fd, DIR1, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(info->t_dir1_fd, FILE2, 0, 0, 0))
			die("failure: check ownership");

		if (!expected_uid_gid(info->t_dir1_fd, CHRDEV1, 0, 0, 0))
			die("failure: check ownership");

		if (unlinkat(info->t_dir1_fd, FILE1, 0))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, DIR1, AT_REMOVEDIR))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, FILE2, 0))
			die("failure: delete");

		if (unlinkat(info->t_dir1_fd, CHRDEV1, 0))
			die("failure: delete");

		/* create tmpfile via filesystem tmpfile api */
		if (supported) {
			tmpfile_fd = openat(info->t_dir1_fd, ".", O_TMPFILE | O_RDWR, S_IXGRP | S_ISGID);
			if (tmpfile_fd < 0)
				die("failure: create");
			/* link the temporary file into the filesystem, making it permanent */
			if (linkat(tmpfile_fd, "", info->t_dir1_fd, FILE3, AT_EMPTY_PATH))
				die("failure: linkat");
			if (close(tmpfile_fd))
				die("failure: close");
			if (is_setgid(info->t_dir1_fd, FILE3, 0))
				die("failure: is_setgid");
			if (!expected_uid_gid(info->t_dir1_fd, FILE3, 0, 0, 0))
				die("failure: check ownership");
			if (unlinkat(info->t_dir1_fd, FILE3, 0))
				die("failure: delete");
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

static int setattr_truncate(const struct vfstest_info *info)
{
	int fret = -1;
	int file1_fd = -EBADF;

	/* create regular file via open() */
	file1_fd = openat(info->t_dir1_fd, FILE1, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, S_IXGRP | S_ISGID);
	if (file1_fd < 0) {
		log_stderr("failure: create");
		goto out;
	}

	if (ftruncate(file1_fd, 10000)) {
		log_stderr("failure: ftruncate");
		goto out;
	}

	if (!expected_uid_gid(info->t_dir1_fd, FILE1, 0, 0, 0)) {
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

	if (!expected_uid_gid(info->t_dir1_fd, FILE1, 0, 0, 0)) {
		log_stderr("failure: check ownership");
		goto out;
	}

	if (!expected_file_size(file1_fd, "", AT_EMPTY_PATH, 0)) {
		log_stderr("failure: expected_file_size");
		goto out;
	}

	if (unlinkat(info->t_dir1_fd, FILE1, 0)) {
		log_stderr("failure: remove");
		goto out;
	}

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(file1_fd);

	return fret;
}

static int nested_userns(const struct vfstest_info *info)
{
	int fret = -1;
	int ret;
	pid_t pid;
	unsigned int id;
	struct list *it, *next;
	struct userns_hierarchy hierarchy[] = {
		{ .level = 1, .fd_userns = -EBADF, },
		{ .level = 2, .fd_userns = -EBADF, },
		{ .level = 3, .fd_userns = -EBADF, },
		{ .level = 4, .fd_userns = -EBADF, },
		/* Dummy entry that marks the end. */
		{ .level = MAX_USERNS_LEVEL, .fd_userns = -EBADF, },
	};
	struct mount_attr attr_level1 = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	struct mount_attr attr_level2 = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	struct mount_attr attr_level3 = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	struct mount_attr attr_level4 = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	int fd_dir1 = -EBADF,
	    fd_open_tree_level1 = -EBADF,
	    fd_open_tree_level2 = -EBADF,
	    fd_open_tree_level3 = -EBADF,
	    fd_open_tree_level4 = -EBADF;
	const unsigned int id_file_range = 10000;

	list_init(&hierarchy[0].id_map);
	list_init(&hierarchy[1].id_map);
	list_init(&hierarchy[2].id_map);
	list_init(&hierarchy[3].id_map);

	/*
	 * Give a large map to the outermost user namespace so we can create
	 * comfortable nested maps.
	 */
	ret = add_map_entry(&hierarchy[0].id_map, 1000000, 0, 1000000000, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: adding uidmap for userns at level 1");
		goto out;
	}

	ret = add_map_entry(&hierarchy[0].id_map, 1000000, 0, 1000000000, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: adding gidmap for userns at level 1");
		goto out;
	}

	/* This is uid:0->2000000:100000000 in init userns. */
	ret = add_map_entry(&hierarchy[1].id_map, 1000000, 0, 100000000, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: adding uidmap for userns at level 2");
		goto out;
	}

	/* This is gid:0->2000000:100000000 in init userns. */
	ret = add_map_entry(&hierarchy[1].id_map, 1000000, 0, 100000000, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: adding gidmap for userns at level 2");
		goto out;
	}

	/* This is uid:0->3000000:999 in init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1000000, 0, 999, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: adding uidmap for userns at level 3");
		goto out;
	}

	/* This is gid:0->3000000:999 in the init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1000000, 0, 999, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: adding gidmap for userns at level 3");
		goto out;
	}

	/* id 999 will remain unmapped. */

	/* This is uid:1000->2001000:1 in init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1000, 1000, 1, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: adding uidmap for userns at level 3");
		goto out;
	}

	/* This is gid:1000->2001000:1 in init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1000, 1000, 1, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: adding gidmap for userns at level 3");
		goto out;
	}

	/* This is uid:1001->3001001:10000 in init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1001001, 1001, 10000000, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: adding uidmap for userns at level 3");
		goto out;
	}

	/* This is gid:1001->3001001:10000 in init userns. */
	ret = add_map_entry(&hierarchy[2].id_map, 1001001, 1001, 10000000, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: adding gidmap for userns at level 3");
		goto out;
	}

	/* Don't write a mapping in the 4th userns. */
	list_empty(&hierarchy[4].id_map);

	/* Create the actual userns hierarchy. */
	ret = create_userns_hierarchy(hierarchy);
	if (ret) {
		log_stderr("failure: create userns hierarchy");
		goto out;
	}

	attr_level1.userns_fd = hierarchy[0].fd_userns;
	attr_level2.userns_fd = hierarchy[1].fd_userns;
	attr_level3.userns_fd = hierarchy[2].fd_userns;
	attr_level4.userns_fd = hierarchy[3].fd_userns;

	/*
	 * Create one directory where we create files for each uid/gid within
	 * the first userns.
	 */
	if (mkdirat(info->t_dir1_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	fd_dir1 = openat(info->t_dir1_fd, DIR1, O_DIRECTORY | O_CLOEXEC);
	if (fd_dir1 < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	for (id = 0; id <= id_file_range; id++) {
		char file[256];

		snprintf(file, sizeof(file), DIR1 "/" FILE1 "_%u", id);

		if (mknodat(info->t_dir1_fd, file, S_IFREG | 0644, 0)) {
			log_stderr("failure: create %s", file);
			goto out;
		}

		if (fchownat(info->t_dir1_fd, file, id, id, AT_SYMLINK_NOFOLLOW)) {
			log_stderr("failure: fchownat %s", file);
			goto out;
		}

		if (!expected_uid_gid(info->t_dir1_fd, file, 0, id, id)) {
			log_stderr("failure: check ownership %s", file);
			goto out;
		}
	}

	/* Create detached mounts for all the user namespaces. */
	fd_open_tree_level1 = sys_open_tree(info->t_dir1_fd, DIR1,
					    AT_NO_AUTOMOUNT |
					    AT_SYMLINK_NOFOLLOW |
					    OPEN_TREE_CLOEXEC |
					    OPEN_TREE_CLONE);
	if (fd_open_tree_level1 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	fd_open_tree_level2 = sys_open_tree(info->t_dir1_fd, DIR1,
					    AT_NO_AUTOMOUNT |
					    AT_SYMLINK_NOFOLLOW |
					    OPEN_TREE_CLOEXEC |
					    OPEN_TREE_CLONE);
	if (fd_open_tree_level2 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	fd_open_tree_level3 = sys_open_tree(info->t_dir1_fd, DIR1,
					    AT_NO_AUTOMOUNT |
					    AT_SYMLINK_NOFOLLOW |
					    OPEN_TREE_CLOEXEC |
					    OPEN_TREE_CLONE);
	if (fd_open_tree_level3 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	fd_open_tree_level4 = sys_open_tree(info->t_dir1_fd, DIR1,
					    AT_NO_AUTOMOUNT |
					    AT_SYMLINK_NOFOLLOW |
					    OPEN_TREE_CLOEXEC |
					    OPEN_TREE_CLONE);
	if (fd_open_tree_level4 < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	/* Turn detached mounts into detached idmapped mounts. */
	if (sys_mount_setattr(fd_open_tree_level1, "", AT_EMPTY_PATH,
			      &attr_level1, sizeof(attr_level1))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	if (sys_mount_setattr(fd_open_tree_level2, "", AT_EMPTY_PATH,
			      &attr_level2, sizeof(attr_level2))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	if (sys_mount_setattr(fd_open_tree_level3, "", AT_EMPTY_PATH,
			      &attr_level3, sizeof(attr_level3))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	if (sys_mount_setattr(fd_open_tree_level4, "", AT_EMPTY_PATH,
			      &attr_level4, sizeof(attr_level4))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	/* Verify that ownership looks correct for callers in the init userns. */
	for (id = 0; id <= id_file_range; id++) {
		bool bret;
		unsigned int id_level1, id_level2, id_level3;
		char file[256];

		snprintf(file, sizeof(file), FILE1 "_%u", id);

		id_level1 = id + 1000000;
		if (!expected_uid_gid(fd_open_tree_level1, file, 0, id_level1, id_level1)) {
			log_stderr("failure: check ownership %s", file);
			goto out;
		}

		id_level2 = id + 2000000;
		if (!expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2)) {
			log_stderr("failure: check ownership %s", file);
			goto out;
		}

		if (id == 999) {
			/* This id is unmapped. */
			bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
		} else if (id == 1000) {
			id_level3 = id + 2000000; /* We punched a hole in the map at 1000. */
			bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
		} else {
			id_level3 = id + 3000000; /* Rest is business as usual. */
			bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
		}
		if (!bret) {
			log_stderr("failure: check ownership %s", file);
			goto out;
		}

		if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid)) {
			log_stderr("failure: check ownership %s", file);
			goto out;
		}
	}

	/* Verify that ownership looks correct for callers in the first userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level1.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			bool bret;
			unsigned int id_level1, id_level2, id_level3;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			id_level1 = id;
			if (!expected_uid_gid(fd_open_tree_level1, file, 0, id_level1, id_level1))
				die("failure: check ownership %s", file);

			id_level2 = id + 1000000;
			if (!expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2))
				die("failure: check ownership %s", file);

			if (id == 999) {
				/* This id is unmapped. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
			} else if (id == 1000) {
				id_level3 = id + 1000000; /* We punched a hole in the map at 1000. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			} else {
				id_level3 = id + 2000000; /* Rest is business as usual. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			}
			if (!bret)
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that ownership looks correct for callers in the second userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level2.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			bool bret;
			unsigned int id_level2, id_level3;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			id_level2 = id;
			if (!expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2))
				die("failure: check ownership %s", file);

			if (id == 999) {
				/* This id is unmapped. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
			} else if (id == 1000) {
				id_level3 = id; /* We punched a hole in the map at 1000. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			} else {
				id_level3 = id + 1000000; /* Rest is business as usual. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			}
			if (!bret)
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that ownership looks correct for callers in the third userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level3.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			bool bret;
			unsigned int id_level2, id_level3;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (id == 1000) {
				/*
				 * The idmapping of the third userns has a hole
				 * at uid/gid 1000. That means:
				 * - 1000->userns_0(2000000) // init userns
				 * - 1000->userns_1(2000000) // level 1
				 * - 1000->userns_2(1000000) // level 2
				 * - 1000->userns_3(1000)    // level 3 (because level 3 has a hole)
				 */
				id_level2 = id;
				bret = expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2);
			} else {
				bret = expected_uid_gid(fd_open_tree_level2, file, 0, info->t_overflowuid, info->t_overflowgid);
			}
			if (!bret)
				die("failure: check ownership %s", file);


			if (id == 999) {
				/* This id is unmapped. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
			} else {
				id_level3 = id; /* Rest is business as usual. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			}
			if (!bret)
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that ownership looks correct for callers in the fourth userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (setns(attr_level4.userns_fd, CLONE_NEWUSER))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level2, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that chown works correctly for callers in the first userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level1.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			bool bret;
			unsigned int id_level1, id_level2, id_level3, id_new;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			id_new = id + 1;
			if (fchownat(fd_open_tree_level1, file, id_new, id_new, AT_SYMLINK_NOFOLLOW))
				die("failure: fchownat %s", file);

			id_level1 = id_new;
			if (!expected_uid_gid(fd_open_tree_level1, file, 0, id_level1, id_level1))
				die("failure: check ownership %s", file);

			id_level2 = id_new + 1000000;
			if (!expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2))
				die("failure: check ownership %s", file);

			if (id_new == 999) {
				/* This id is unmapped. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
			} else if (id_new == 1000) {
				id_level3 = id_new + 1000000; /* We punched a hole in the map at 1000. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			} else {
				id_level3 = id_new + 2000000; /* Rest is business as usual. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			}
			if (!bret)
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			/* Revert ownership. */
			if (fchownat(fd_open_tree_level1, file, id, id, AT_SYMLINK_NOFOLLOW))
				die("failure: fchownat %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that chown works correctly for callers in the second userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level2.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			bool bret;
			unsigned int id_level2, id_level3, id_new;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			id_new = id + 1;
			if (fchownat(fd_open_tree_level2, file, id_new, id_new, AT_SYMLINK_NOFOLLOW))
				die("failure: fchownat %s", file);

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			id_level2 = id_new;
			if (!expected_uid_gid(fd_open_tree_level2, file, 0, id_level2, id_level2))
				die("failure: check ownership %s", file);

			if (id_new == 999) {
				/* This id is unmapped. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid);
			} else if (id_new == 1000) {
				id_level3 = id_new; /* We punched a hole in the map at 1000. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			} else {
				id_level3 = id_new + 1000000; /* Rest is business as usual. */
				bret = expected_uid_gid(fd_open_tree_level3, file, 0, id_level3, id_level3);
			}
			if (!bret)
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			/* Revert ownership. */
			if (fchownat(fd_open_tree_level2, file, id, id, AT_SYMLINK_NOFOLLOW))
				die("failure: fchownat %s", file);
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that chown works correctly for callers in the third userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(attr_level3.userns_fd, 0, 0, false))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			unsigned int id_new;
			char file[256];

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			id_new = id + 1;
			if (id_new == 999 || id_new == 1000) {
				/*
				 * We can't change ownership as we can't
				 * chown from or to an unmapped id.
				 */
				if (!fchownat(fd_open_tree_level3, file, id_new, id_new, AT_SYMLINK_NOFOLLOW))
					die("failure: fchownat %s", file);
			} else {
				if (fchownat(fd_open_tree_level3, file, id_new, id_new, AT_SYMLINK_NOFOLLOW))
					die("failure: fchownat %s", file);
			}

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			/* There's no id 1000 anymore as we changed ownership for id 1000 to 1001 above. */
			if (!expected_uid_gid(fd_open_tree_level2, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (id_new == 999) {
				/*
				 * We did not change ownership as we can't
				 * chown to an unmapped id.
				 */
				if (!expected_uid_gid(fd_open_tree_level3, file, 0, id, id))
					die("failure: check ownership %s", file);
			} else if (id_new == 1000) {
				/*
				 * We did not change ownership as we can't
				 * chown from an unmapped id.
				 */
				if (!expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid))
					die("failure: check ownership %s", file);
			} else {
				if (!expected_uid_gid(fd_open_tree_level3, file, 0, id_new, id_new))
					die("failure: check ownership %s", file);
			}

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			/* Revert ownership. */
			if (id_new != 999 && id_new != 1000) {
				if (fchownat(fd_open_tree_level3, file, id, id, AT_SYMLINK_NOFOLLOW))
					die("failure: fchownat %s", file);
			}
		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	/* Verify that chown works correctly for callers in the fourth userns. */
	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (setns(attr_level4.userns_fd, CLONE_NEWUSER))
			die("failure: switch_userns");

		for (id = 0; id <= id_file_range; id++) {
			char file[256];
			unsigned long id_new;

			snprintf(file, sizeof(file), FILE1 "_%u", id);

			id_new = id + 1;
			if (!fchownat(fd_open_tree_level4, file, id_new, id_new, AT_SYMLINK_NOFOLLOW))
				die("failure: fchownat %s", file);

			if (!expected_uid_gid(fd_open_tree_level1, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level2, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level3, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

			if (!expected_uid_gid(fd_open_tree_level4, file, 0, info->t_overflowuid, info->t_overflowgid))
				die("failure: check ownership %s", file);

		}

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");

out:
	list_for_each_safe(it, &hierarchy[0].id_map, next) {
		list_del(it);
		free(it->elem);
		free(it);
	}

	list_for_each_safe(it, &hierarchy[1].id_map, next) {
		list_del(it);
		free(it->elem);
		free(it);
	}

	list_for_each_safe(it, &hierarchy[2].id_map, next) {
		list_del(it);
		free(it->elem);
		free(it);
	}

	safe_close(hierarchy[0].fd_userns);
	safe_close(hierarchy[1].fd_userns);
	safe_close(hierarchy[2].fd_userns);
	safe_close(fd_dir1);
	safe_close(fd_open_tree_level1);
	safe_close(fd_open_tree_level2);
	safe_close(fd_open_tree_level3);
	safe_close(fd_open_tree_level4);
	return fret;
}

#define USER1 "fsgqa"
#define USER2 "fsgqa2"

/**
 * lookup_ids - lookup uid and gid for a username
 * @name: [in]  name of the user
 * @uid:  [out] pointer to the user-ID
 * @gid:  [out] pointer to the group-ID
 *
 * Lookup the uid and gid of a user.
 *
 * Return: On success, true is returned.
 *         On error, false is returned.
 */
static bool lookup_ids(const char *name, uid_t *uid, gid_t *gid)
{
	bool bret = false;
	struct passwd *pwentp = NULL;
	struct passwd pwent;
	char *buf;
	ssize_t bufsize;
	int ret;

	bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize < 0)
		bufsize = 1024;

	buf = malloc(bufsize);
	if (!buf)
		return bret;

	ret = getpwnam_r(name, &pwent, buf, bufsize, &pwentp);
	if (!ret && pwentp) {
		*uid = pwent.pw_uid;
		*gid = pwent.pw_gid;
		bret = true;
	}

	free(buf);
	return bret;
}

/**
 * setattr_fix_968219708108 - test for commit 968219708108 ("fs: handle circular mappings correctly")
 *
 * Test that ->setattr() works correctly for idmapped mounts with circular
 * idmappings such as:
 *
 * b:1000:1001:1
 * b:1001:1000:1
 *
 * Assume a directory /source with two files:
 *
 * /source/file1 | 1000:1000
 * /source/file2 | 1001:1001
 *
 * and we create an idmapped mount of /source at /target with an idmapped of:
 *
 * mnt_userns:        1000:1001:1
 *                    1001:1000:1
 *
 * In the idmapped mount file1 will be owned by uid 1001 and file2 by uid 1000:
 *
 * /target/file1 | 1001:1001
 * /target/file2 | 1000:1000
 *
 * Because in essence the idmapped mount switches ownership for {g,u}id 1000
 * and {g,u}id 1001.
 *
 * 1. A user with fs{g,u}id 1000 must be allowed to setattr /target/file2 from
 *    {g,u}id 1000 in the idmapped mount to {g,u}id 1000.
 * 2. A user with fs{g,u}id 1001 must be allowed to setattr /target/file1 from
 *    {g,u}id 1001 in the idmapped mount to {g,u}id 1001.
 * 3. A user with fs{g,u}id 1000 must fail to setattr /target/file1 from
 *    {g,u}id 1001 in the idmapped mount to {g,u}id 1000.
 *    This must fail with EPERM. The caller's fs{g,u}id doesn't match the
 *    {g,u}id of the file.
 * 4. A user with fs{g,u}id 1001 must fail to setattr /target/file2 from
 *    {g,u}id 1000 in the idmapped mount to {g,u}id 1000.
 *    This must fail with EPERM. The caller's fs{g,u}id doesn't match the
 *    {g,u}id of the file.
 * 5. Both, a user with fs{g,u}id 1000 and a user with fs{g,u}id 1001, must
 *    fail to setattr /target/file1 owned by {g,u}id 1001 in the idmapped mount
 *    and /target/file2 owned by {g,u}id 1000 in the idmapped mount to any
 *    {g,u}id apart from {g,u}id 1000 or 1001 with EINVAL.
 *    Only {g,u}id 1000 and 1001 have a mapping in the idmapped mount. Other
 *    {g,u}id are unmapped.
 */
static int setattr_fix_968219708108(const struct vfstest_info *info)
{
	int fret = -1;
	int open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	int ret;
	uid_t user1_uid, user2_uid;
	gid_t user1_gid, user2_gid;
	pid_t pid;
	struct list idmap;
	struct list *it_cur, *it_next;

	if (!caps_supported())
		return 0;

	list_init(&idmap);

	if (!lookup_ids(USER1, &user1_uid, &user1_gid)) {
		log_stderr("failure: lookup_user");
		goto out;
	}

	if (!lookup_ids(USER2, &user2_uid, &user2_gid)) {
		log_stderr("failure: lookup_user");
		goto out;
	}

	log_debug("Found " USER1 " with uid(%d) and gid(%d) and " USER2 " with uid(%d) and gid(%d)",
		  user1_uid, user1_gid, user2_uid, user2_gid);

	if (mkdirat(info->t_dir1_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	if (mknodat(info->t_dir1_fd, DIR1 "/" FILE1, S_IFREG | 0644, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}

	if (chown_r(info->t_mnt_fd, T_DIR1, user1_uid, user1_gid)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	if (mknodat(info->t_dir1_fd, DIR1 "/" FILE2, S_IFREG | 0644, 0)) {
		log_stderr("failure: mknodat");
		goto out;
	}

	if (fchownat(info->t_dir1_fd, DIR1 "/" FILE2, user2_uid, user2_gid, AT_SYMLINK_NOFOLLOW)) {
		log_stderr("failure: fchownat");
		goto out;
	}

	print_r(info->t_mnt_fd, T_DIR1);

	/* u:1000:1001:1 */
	ret = add_map_entry(&idmap, user1_uid, user2_uid, 1, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* u:1001:1000:1 */
	ret = add_map_entry(&idmap, user2_uid, user1_uid, 1, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* g:1000:1001:1 */
	ret = add_map_entry(&idmap, user1_gid, user2_gid, 1, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* g:1001:1000:1 */
	ret = add_map_entry(&idmap, user2_gid, user1_gid, 1, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	attr.userns_fd = get_userns_fd_from_idmap(&idmap);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	open_tree_fd = sys_open_tree(info->t_dir1_fd, DIR1,
				     AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW |
				     OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE |
				     AT_RECURSIVE);
	if (open_tree_fd < 0) {
		log_stderr("failure: sys_open_tree");
		goto out;
	}

	if (sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr, sizeof(attr))) {
		log_stderr("failure: sys_mount_setattr");
		goto out;
	}

	print_r(open_tree_fd, "");

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		/* switch to {g,u}id 1001 */
		if (!switch_resids(user2_uid, user2_gid))
			die("failure: switch_resids");

		/* drop all capabilities */
		if (!caps_down())
			die("failure: caps_down");

		/*
		 * The {g,u}id 0 is not mapped in this idmapped mount so this
		 * needs to fail with EINVAL.
		 */
		if (!fchownat(open_tree_fd, FILE1, 0, 0, AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EINVAL)
			die("failure: errno");

		/*
		 * A user with fs{g,u}id 1001 must be allowed to change
		 * ownership of /target/file1 owned by {g,u}id 1001 in this
		 * idmapped mount to {g,u}id 1001.
		 */
		if (fchownat(open_tree_fd, FILE1, user2_uid, user2_gid,
			     AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");

		/* Verify that the ownership is still {g,u}id 1001. */
		if (!expected_uid_gid(open_tree_fd, FILE1, AT_SYMLINK_NOFOLLOW,
				      user2_uid, user2_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1001 must not be allowed to change
		 * ownership of /target/file1 owned by {g,u}id 1001 in this
		 * idmapped mount to {g,u}id 1000.
		 */
		if (!fchownat(open_tree_fd, FILE1, user1_uid, user1_gid,
			      AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1001. */
		if (!expected_uid_gid(open_tree_fd, FILE1, AT_SYMLINK_NOFOLLOW,
				      user2_uid, user2_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1001 must not be allowed to change
		 * ownership of /target/file2 owned by {g,u}id 1000 in this
		 * idmapped mount to {g,u}id 1000.
		 */
		if (!fchownat(open_tree_fd, FILE2, user1_uid, user1_gid,
			      AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1000. */
		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW,
				      user1_uid, user1_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1001 must not be allowed to change
		 * ownership of /target/file2 owned by {g,u}id 1000 in this
		 * idmapped mount to {g,u}id 1001.
		 */
		if (!fchownat(open_tree_fd, FILE2, user2_uid, user2_gid,
			      AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1000. */
		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW,
				      user1_uid, user1_gid))
			die("failure: check ownership");

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
		/* switch to {g,u}id 1000 */
		if (!switch_resids(user1_uid, user1_gid))
			die("failure: switch_resids");

		/* drop all capabilities */
		if (!caps_down())
			die("failure: caps_down");

		/*
		 * The {g,u}id 0 is not mapped in this idmapped mount so this
		 * needs to fail with EINVAL.
		 */
		if (!fchownat(open_tree_fd, FILE1, 0, 0, AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EINVAL)
			die("failure: errno");

		/*
		 * A user with fs{g,u}id 1000 must be allowed to change
		 * ownership of /target/file2 owned by {g,u}id 1000 in this
		 * idmapped mount to {g,u}id 1000.
		 */
		if (fchownat(open_tree_fd, FILE2, user1_uid, user1_gid,
			     AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");

		/* Verify that the ownership is still {g,u}id 1000. */
		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW,
				      user1_uid, user1_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1000 must not be allowed to change
		 * ownership of /target/file2 owned by {g,u}id 1000 in this
		 * idmapped mount to {g,u}id 1001.
		 */
		if (!fchownat(open_tree_fd, FILE2, user2_uid, user2_gid,
			      AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1000. */
		if (!expected_uid_gid(open_tree_fd, FILE2, AT_SYMLINK_NOFOLLOW,
				      user1_uid, user1_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1000 must not be allowed to change
		 * ownership of /target/file1 owned by {g,u}id 1001 in this
		 * idmapped mount to {g,u}id 1000.
		 */
		if (!fchownat(open_tree_fd, FILE1, user1_uid, user1_gid,
			     AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1001. */
		if (!expected_uid_gid(open_tree_fd, FILE1, AT_SYMLINK_NOFOLLOW,
				      user2_uid, user2_gid))
			die("failure: check ownership");

		/*
		 * A user with fs{g,u}id 1000 must not be allowed to change
		 * ownership of /target/file1 owned by {g,u}id 1001 in this
		 * idmapped mount to {g,u}id 1001.
		 */
		if (!fchownat(open_tree_fd, FILE1, user2_uid, user2_gid,
			      AT_SYMLINK_NOFOLLOW))
			die("failure: change ownership");
		if (errno != EPERM)
			die("failure: errno");

		/* Verify that the ownership is still {g,u}id 1001. */
		if (!expected_uid_gid(open_tree_fd, FILE1, AT_SYMLINK_NOFOLLOW,
				      user2_uid, user2_gid))
			die("failure: check ownership");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(attr.userns_fd);
	safe_close(open_tree_fd);

	list_for_each_safe(it_cur, &idmap, it_next) {
		list_del(it_cur);
		free(it_cur->elem);
		free(it_cur);
	}

	return fret;
}

/**
 * setxattr_fix_705191b03d50 - test for commit 705191b03d50 ("fs: fix acl translation").
 */
static int setxattr_fix_705191b03d50(const struct vfstest_info *info)
{
	int fret = -1;
	int fd_userns = -EBADF;
	int ret;
	uid_t user1_uid;
	gid_t user1_gid;
	pid_t pid;
	struct list idmap;
	struct list *it_cur, *it_next;

	list_init(&idmap);

	if (!lookup_ids(USER1, &user1_uid, &user1_gid)) {
		log_stderr("failure: lookup_user");
		goto out;
	}

	log_debug("Found " USER1 " with uid(%d) and gid(%d)", user1_uid, user1_gid);

	if (mkdirat(info->t_dir1_fd, DIR1, 0777)) {
		log_stderr("failure: mkdirat");
		goto out;
	}

	if (chown_r(info->t_mnt_fd, T_DIR1, user1_uid, user1_gid)) {
		log_stderr("failure: chown_r");
		goto out;
	}

	print_r(info->t_mnt_fd, T_DIR1);

	/* u:0:user1_uid:1 */
	ret = add_map_entry(&idmap, user1_uid, 0, 1, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* g:0:user1_gid:1 */
	ret = add_map_entry(&idmap, user1_gid, 0, 1, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* u:100:10000:100 */
	ret = add_map_entry(&idmap, 10000, 100, 100, ID_TYPE_UID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	/* g:100:10000:100 */
	ret = add_map_entry(&idmap, 10000, 100, 100, ID_TYPE_GID);
	if (ret) {
		log_stderr("failure: add_map_entry");
		goto out;
	}

	fd_userns = get_userns_fd_from_idmap(&idmap);
	if (fd_userns < 0) {
		log_stderr("failure: get_userns_fd");
		goto out;
	}

	pid = fork();
	if (pid < 0) {
		log_stderr("failure: fork");
		goto out;
	}
	if (pid == 0) {
		if (!switch_userns(fd_userns, 0, 0, false))
			die("failure: switch_userns");

		/* create separate mount namespace */
		if (unshare(CLONE_NEWNS))
			die("failure: create new mount namespace");

		/* turn off mount propagation */
		if (sys_mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0))
			die("failure: turn mount propagation off");

		snprintf(t_buf, sizeof(t_buf), "%s/%s/%s", info->t_mountpoint, T_DIR1, DIR1);

		if (sys_mount("none", t_buf, "tmpfs", 0, "mode=0755"))
			die("failure: mount");

		snprintf(t_buf, sizeof(t_buf), "%s/%s/%s/%s", info->t_mountpoint, T_DIR1, DIR1, DIR3);
		if (mkdir(t_buf, 0700))
			die("failure: mkdir");

		snprintf(t_buf, sizeof(t_buf), "setfacl -m u:100:rwx %s/%s/%s/%s", info->t_mountpoint, T_DIR1, DIR1, DIR3);
		if (system(t_buf))
			die("failure: system");

		snprintf(t_buf, sizeof(t_buf), "getfacl -n -p %s/%s/%s/%s | grep -q user:100:rwx", info->t_mountpoint, T_DIR1, DIR1, DIR3);
		if (system(t_buf))
			die("failure: system");

		exit(EXIT_SUCCESS);
	}
	if (wait_for_pid(pid))
		goto out;

	fret = 0;
	log_debug("Ran test");
out:
	safe_close(fd_userns);

	list_for_each_safe(it_cur, &idmap, it_next) {
		list_del(it_cur);
		free(it_cur->elem);
		free(it_cur);
	}

	return fret;
}

static void usage(void)
{
	fprintf(stderr, "Description:\n");
	fprintf(stderr, "    Run idmapped mount tests\n\n");

	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "--device                            Device used in the tests\n");
	fprintf(stderr, "--fstype                            Filesystem type used in the tests\n");
	fprintf(stderr, "--help                              Print help\n");
	fprintf(stderr, "--mountpoint                        Mountpoint of device\n");
	fprintf(stderr, "--idmapped-mounts-supported	     Test whether idmapped mounts are supported on this filesystem\n");
	fprintf(stderr, "--scratch-mountpoint                Mountpoint of scratch device used in the tests\n");
	fprintf(stderr, "--scratch-device                    Scratch device used in the tests\n");
	fprintf(stderr, "--test-core                         Run core idmapped mount testsuite\n");
	fprintf(stderr, "--test-fscaps-regression            Run fscap regression tests\n");
	fprintf(stderr, "--test-nested-userns                Run nested userns idmapped mount testsuite\n");
	fprintf(stderr, "--test-btrfs                        Run btrfs specific idmapped mount testsuite\n");
	fprintf(stderr, "--test-setattr-fix-968219708108     Run setattr regression tests\n");
	fprintf(stderr, "--test-setxattr-fix-705191b03d50    Run setxattr regression tests\n");

	_exit(EXIT_SUCCESS);
}

static const struct option longopts[] = {
	{"device",				required_argument,	0,	'd'},
	{"fstype",				required_argument,	0,	'f'},
	{"mountpoint",				required_argument,	0,	'm'},
	{"scratch-mountpoint",			required_argument,	0,	'a'},
	{"scratch-device",			required_argument,	0,	'e'},
	{"idmapped-mounts-supported",		no_argument,		0,	's'},
	{"help",				no_argument,		0,	'h'},
	{"test-core",				no_argument,		0,	'c'},
	{"test-fscaps-regression",		no_argument,		0,	'g'},
	{"test-nested-userns",			no_argument,		0,	'n'},
	{"test-btrfs",				no_argument,		0,	'b'},
	{"test-setattr-fix-968219708108",	no_argument,		0,	'i'},
	{"test-setxattr-fix-705191b03d50",	no_argument,		0,	'j'},
	{NULL,					0,			0,	  0},
};

static const struct test_struct t_basic[] = {
	{ fscaps,							T_REQUIRE_USERNS,	"fscaps on regular mounts",									},
	{ hardlink_crossing_mounts,					0,			"cross mount hardlink",										},
#ifdef HAVE_LIBURING_H
	{ io_uring,							0,			"io_uring",											},
	{ io_uring_userns,						T_REQUIRE_USERNS,	"io_uring in user namespace",									},
#endif
	{ protected_symlinks,						0,			"following protected symlinks on regular mounts",						},
	{ rename_crossing_mounts,					0,			"cross mount rename",										},
	{ setattr_truncate,						0,			"setattr truncate",										},
	{ setgid_create,						0,			"create operations in directories with setgid bit set",						},
	{ setid_binaries,						0,			"setid binaries on regular mounts",								},
	{ sticky_bit_unlink,						0,			"sticky bit unlink operations on regular mounts",						},
	{ sticky_bit_rename,						0,			"sticky bit rename operations on regular mounts",						},
	{ symlink_regular_mounts,					0,			"symlink from regular mounts",									},
};

static const struct test_suite s_basic = {
	.tests = t_basic,
	.nr_tests = ARRAY_SIZE(t_basic),
};

static const struct test_struct t_nested_userns[] = {
	{ nested_userns,						T_REQUIRE_IDMAPPED_MOUNTS,	"test that nested user namespaces behave correctly when attached to idmapped mounts",		},
};

static const struct test_suite s_nested_userns = {
	.tests = t_nested_userns,
	.nr_tests = ARRAY_SIZE(t_nested_userns),
};

/* Test for commit 968219708108 ("fs: handle circular mappings correctly"). */
static const struct test_struct t_setattr_fix_968219708108[] = {
	{ setattr_fix_968219708108,					T_REQUIRE_IDMAPPED_MOUNTS,	"test that setattr works correctly",								},
};

static const struct test_suite s_setattr_fix_968219708108 = {
	.tests = t_setattr_fix_968219708108,
	.nr_tests = ARRAY_SIZE(t_setattr_fix_968219708108),
};

/* Test for commit 705191b03d50 ("fs: fix acl translation"). */
static const struct test_struct t_setxattr_fix_705191b03d50[] = {
	{ setxattr_fix_705191b03d50,					T_REQUIRE_USERNS,	"test that setxattr works correctly for userns mountable filesystems",				},
};

static const struct test_suite s_setxattr_fix_705191b03d50 = {
	.tests = t_setxattr_fix_705191b03d50,
	.nr_tests = ARRAY_SIZE(t_setxattr_fix_705191b03d50),
};

static bool run_test(struct vfstest_info *info, const struct test_struct suite[], size_t suite_size)
{
	int i;

	for (i = 0; i < suite_size; i++) {
		const struct test_struct *t = &suite[i];
		int ret;
		pid_t pid;

		/*
		 * If the underlying filesystems does not support idmapped
		 * mounts only run vfs generic tests.
		 */
		if ((t->support_flags & T_REQUIRE_IDMAPPED_MOUNTS &&
		     !info->t_fs_allow_idmap) ||
		    (t->support_flags & T_REQUIRE_USERNS && !info->t_has_userns)) {
			log_debug("Skipping test %s", t->description);
			continue;
		}

		test_setup(info);

		pid = fork();
		if (pid < 0)
			return false;

		if (pid == 0) {
			ret = t->test(info);
			if (ret)
				die("failure: %s", t->description);

			exit(EXIT_SUCCESS);
		}

		ret = wait_for_pid(pid);
		test_cleanup(info);

		if (ret)
			return false;
	}

	return true;
}

static inline bool run_suite(struct vfstest_info *info,
			     const struct test_suite *suite)
{
	return run_test(info, suite->tests, suite->nr_tests);
}

static bool fs_allow_idmap(const struct vfstest_info *info)
{
	int ret;
	int open_tree_fd = -EBADF;
	struct mount_attr attr = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};

	/* Changing mount properties on a detached mount. */
	attr.userns_fd = get_userns_fd(0, 1000, 1);
	if (attr.userns_fd < 0)
		return false;

	open_tree_fd = sys_open_tree(info->t_mnt_fd, "",
				     AT_EMPTY_PATH | AT_NO_AUTOMOUNT |
				     AT_SYMLINK_NOFOLLOW | OPEN_TREE_CLOEXEC |
				     OPEN_TREE_CLONE);
	if (open_tree_fd < 0)
		ret = -1;
	else
		ret = sys_mount_setattr(open_tree_fd, "", AT_EMPTY_PATH, &attr,
					sizeof(attr));
	close(open_tree_fd);
	close(attr.userns_fd);

	return ret == 0;
}

static bool sys_has_userns(void)
{
	int fd = get_userns_fd(0, 1000, 1);

	if (fd < 0)
		return false;
	close(fd);
	return true;
}

int main(int argc, char *argv[])
{
	struct vfstest_info info;
	int fret, ret;
	int index = 0;
	bool idmapped_mounts_supported = false, test_btrfs = false,
	     test_core = false, test_fscaps_regression = false,
	     test_nested_userns = false, test_setattr_fix_968219708108 = false,
	     test_setxattr_fix_705191b03d50 = false;

	init_vfstest_info(&info);

	while ((ret = getopt_long_only(argc, argv, "", longopts, &index)) != -1) {
		switch (ret) {
		case 'd':
			info.t_device = optarg;
			break;
		case 'f':
			info.t_fstype = optarg;
			break;
		case 'm':
			info.t_mountpoint = optarg;
			break;
		case 's':
			idmapped_mounts_supported = true;
			break;
		case 'c':
			test_core = true;
			break;
		case 'g':
			test_fscaps_regression = true;
			break;
		case 'n':
			test_nested_userns = true;
			break;
		case 'b':
			test_btrfs = true;
			break;
		case 'a':
			info.t_mountpoint_scratch = optarg;
			break;
		case 'e':
			info.t_device_scratch = optarg;
			break;
		case 'i':
			test_setattr_fix_968219708108 = true;
			break;
		case 'j':
			test_setxattr_fix_705191b03d50 = true;
			break;
		case 'h':
			/* fallthrough */
		default:
			usage();
		}
	}

	if (!info.t_device)
		die_errno(EINVAL, "test device missing");

	if (!info.t_fstype)
		die_errno(EINVAL, "test filesystem type missing");

	if (!info.t_mountpoint)
		die_errno(EINVAL, "mountpoint of test device missing");

	/* create separate mount namespace */
	if (unshare(CLONE_NEWNS))
		die("failure: create new mount namespace");

	/* turn off mount propagation */
	if (sys_mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0))
		die("failure: turn mount propagation off");

	info.t_mnt_fd = openat(-EBADF, info.t_mountpoint, O_CLOEXEC | O_DIRECTORY);
	if (info.t_mnt_fd < 0)
		die("failed to open %s", info.t_mountpoint);

	info.t_mnt_scratch_fd = openat(-EBADF, info.t_mountpoint_scratch, O_CLOEXEC | O_DIRECTORY);
	if (info.t_mnt_fd < 0)
		die("failed to open %s", info.t_mountpoint_scratch);

	info.t_fs_allow_idmap = fs_allow_idmap(&info);
	if (idmapped_mounts_supported) {
		/*
		 * Caller just wants to know whether the filesystem we're on
		 * supports idmapped mounts.
		 */
		if (!info.t_fs_allow_idmap)
			exit(EXIT_FAILURE);

		exit(EXIT_SUCCESS);
	}
	info.t_has_userns = sys_has_userns();
	/* don't copy ENOSYS errno to child process on older kernel */
	errno = 0;

	stash_overflowuid(&info);
	stash_overflowgid(&info);

	fret = EXIT_FAILURE;

	if (test_core) {
		if (!run_suite(&info, &s_basic))
			goto out;

		if (!run_suite(&info, &s_idmapped_mounts))
			goto out;
	}

	if (test_fscaps_regression && !run_suite(&info, &s_fscaps_in_ancestor_userns))
		goto out;

	if (test_nested_userns && !run_suite(&info, &s_nested_userns))
		goto out;

	if (test_btrfs && !run_suite(&info, &s_btrfs_idmapped_mounts))
		goto out;

	if (test_setattr_fix_968219708108 &&
	    !run_suite(&info, &s_setattr_fix_968219708108))
		goto out;

	if (test_setxattr_fix_705191b03d50 &&
	    !run_suite(&info, &s_setxattr_fix_705191b03d50))
		goto out;

	fret = EXIT_SUCCESS;

out:
	exit(fret);
}
