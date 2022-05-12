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

static void usage(void)
{
	fprintf(stderr, "Description:\n");
	fprintf(stderr, "    Run idmapped mount tests\n\n");

	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "--device                            Device used in the tests\n");
	fprintf(stderr, "--fstype                            Filesystem type used in the tests\n");
	fprintf(stderr, "--help                              Print help\n");
	fprintf(stderr, "--mountpoint                        Mountpoint of device\n");
	fprintf(stderr, "--idmapped-mounts-supported         Test whether idmapped mounts are supported on this filesystem\n");
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
