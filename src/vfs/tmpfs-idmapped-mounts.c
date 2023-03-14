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

#include "missing.h"
#include "utils.h"
#include "vfstest.h"
#include "idmapped-mounts.h"

static int tmpfs_nested_mount_setup(const struct vfstest_info *info, int (*test)(const struct vfstest_info *info))
{
	char path[PATH_MAX];
	int fret = -1;
	struct vfstest_info nested_test_info = *info;

	/* Create mapping for userns
	 * Make the mapping quite long, so all nested userns that are created by
	 * any test we call is contained here (otherwise userns creation fails).
	 */
	struct mount_attr attr = {
		.attr_set	= MOUNT_ATTR_IDMAP,
		.userns_fd	= -EBADF,
	};
	attr.userns_fd = get_userns_fd(0, 10000, 200000);
	if (attr.userns_fd < 0) {
		log_stderr("failure: get_userns_fd");
		goto out_close;
	}

	if (!switch_userns(attr.userns_fd, 0, 0, false)) {
		log_stderr("failure: switch_userns");
		goto out_close;
	}

	/* create separate mount namespace */
	if (unshare(CLONE_NEWNS)) {
		log_stderr("failure: create new mount namespace");
		goto out_close;
	}

	/* We don't want this mount in the parent mount ns */
	if (sys_mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, 0)) {
		log_stderr("failure: mount");
		goto out_close;
	}

	/* Create DIR0 to mount there */
	if (mkdirat(info->t_mnt_fd, DIR0, 0777)) {
		log_stderr("failure: mkdirat");
		goto out_close;
	}
	if (fchmodat(info->t_mnt_fd, DIR0, 0777, 0)) {
		log_stderr("failure: fchmodat");
		goto out_rm;
	}

	snprintf(path, sizeof(path), "%s/%s", info->t_mountpoint, DIR0);
	if (sys_mount("tmpfs", path, "tmpfs", 0, NULL)) {
		log_stderr("failure: mount");
		goto out_rm;
	}

	// Create a new info to use for the test we will call.
	nested_test_info = *info;
	nested_test_info.t_mountpoint = strdup(path);
	if (!nested_test_info.t_mountpoint) {
		log_stderr("failure: strdup");
		goto out;
	}
	nested_test_info.t_mnt_fd = openat(-EBADF, nested_test_info.t_mountpoint, O_CLOEXEC | O_DIRECTORY);
	if (nested_test_info.t_mnt_fd < 0) {
		log_stderr("failure: openat");
		goto out;
	}

	test_setup(&nested_test_info);

	// Run the test.
	if ((*test)(&nested_test_info)) {
		log_stderr("failure: calling test");
		goto out;
	}

	test_cleanup(&nested_test_info);

	fret = 0;
	log_debug("Ran test");
out:
	snprintf(path, sizeof(path), "%s/" DIR0, info->t_mountpoint);
	sys_umount2(path, MNT_DETACH);
out_rm:
	if (rm_r(info->t_mnt_fd, DIR0))
		log_stderr("failure: rm_r");
out_close:
	safe_close(attr.userns_fd);
	return fret;
}

static int tmpfs_acls(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_acls);
}
static int tmpfs_create_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_create_in_userns);
}
static int tmpfs_device_node_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_device_node_in_userns);
}
static int tmpfs_fsids_mapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_fsids_mapped);
}
static int tmpfs_fsids_unmapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_fsids_unmapped);
}
static int tmpfs_expected_uid_gid_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_expected_uid_gid_idmapped_mounts);
}
static int tmpfs_fscaps_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_fscaps_idmapped_mounts);
}
static int tmpfs_fscaps_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_fscaps_idmapped_mounts_in_userns);
}
static int tmpfs_fscaps_idmapped_mounts_in_userns_separate_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_fscaps_idmapped_mounts_in_userns_separate_userns);
}

static int tmpfs_hardlink_crossing_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_hardlink_crossing_idmapped_mounts);
}
static int tmpfs_hardlink_from_idmapped_mount(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_hardlink_from_idmapped_mount);
}
static int tmpfs_hardlink_from_idmapped_mount_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_hardlink_from_idmapped_mount_in_userns);
}

#ifdef HAVE_LIBURING_H
static int tmpfs_io_uring_idmapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_io_uring_idmapped);
}
static int tmpfs_io_uring_idmapped_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_io_uring_idmapped_userns);
}
static int tmpfs_io_uring_idmapped_unmapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_io_uring_idmapped_unmapped);
}
static int tmpfs_io_uring_idmapped_unmapped_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_io_uring_idmapped_unmapped_userns);
}
#endif /* HAVE_LIBURING_H */

static int tmpfs_protected_symlinks_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_protected_symlinks_idmapped_mounts);
}
static int tmpfs_protected_symlinks_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_protected_symlinks_idmapped_mounts_in_userns);
}
static int tmpfs_rename_crossing_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_rename_crossing_idmapped_mounts);
}
static int tmpfs_rename_from_idmapped_mount(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_rename_from_idmapped_mount);
}
static int tmpfs_rename_from_idmapped_mount_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_rename_from_idmapped_mount_in_userns);
}
static int tmpfs_setattr_truncate_idmapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setattr_truncate_idmapped);
}
static int tmpfs_setattr_truncate_idmapped_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setattr_truncate_idmapped_in_userns);
}
static int tmpfs_setgid_create_idmapped(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setgid_create_idmapped);
}
static int tmpfs_setgid_create_idmapped_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setgid_create_idmapped_in_userns);
}
static int tmpfs_setid_binaries_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setid_binaries_idmapped_mounts);
}
static int tmpfs_setid_binaries_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setid_binaries_idmapped_mounts_in_userns);
}
static int tmpfs_setid_binaries_idmapped_mounts_in_userns_separate_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_setid_binaries_idmapped_mounts_in_userns_separate_userns);
}
static int tmpfs_sticky_bit_unlink_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_sticky_bit_unlink_idmapped_mounts);
}
static int tmpfs_sticky_bit_unlink_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_sticky_bit_unlink_idmapped_mounts_in_userns);
}
static int tmpfs_sticky_bit_rename_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_sticky_bit_rename_idmapped_mounts);
}
static int tmpfs_sticky_bit_rename_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_sticky_bit_rename_idmapped_mounts_in_userns);
}
static int tmpfs_symlink_idmapped_mounts(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_symlink_idmapped_mounts);
}
static int tmpfs_symlink_idmapped_mounts_in_userns(const struct vfstest_info *info)
{
	return tmpfs_nested_mount_setup(info, tcore_symlink_idmapped_mounts_in_userns);
}

static const struct test_struct t_tmpfs[] = {
	{ tmpfs_acls,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs create operations in user namespace",							      },
	{ tmpfs_create_in_userns,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs create operations in user namespace",							      },
	{ tmpfs_device_node_in_userns,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs device node in user namespace",								      },
	{ tmpfs_expected_uid_gid_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs expected ownership on idmapped mounts",							},
	{ tmpfs_fscaps_idmapped_mounts,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs fscaps on idmapped mounts",									},
	{ tmpfs_fscaps_idmapped_mounts_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs fscaps on idmapped mounts in user namespace",							},
	{ tmpfs_fscaps_idmapped_mounts_in_userns_separate_userns,		T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs fscaps on idmapped mounts in user namespace with different id mappings",			},
	{ tmpfs_fsids_mapped,							T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs mapped fsids",										      },
	{ tmpfs_fsids_unmapped,							T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs unmapped fsids",										      },
	{ tmpfs_hardlink_crossing_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs cross idmapped mount hardlink",								},
	{ tmpfs_hardlink_from_idmapped_mount,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs hardlinks from idmapped mounts",								},
	{ tmpfs_hardlink_from_idmapped_mount_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs hardlinks from idmapped mounts in user namespace",						},
#ifdef HAVE_LIBURING_H
	{ tmpfs_io_uring_idmapped,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs io_uring from idmapped mounts",								      },
	{ tmpfs_io_uring_idmapped_userns,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs io_uring from idmapped mounts in user namespace",					      },
	{ tmpfs_io_uring_idmapped_unmapped,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs io_uring from idmapped mounts with unmapped ids",					      },
	{ tmpfs_io_uring_idmapped_unmapped_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs io_uring from idmapped mounts with unmapped ids in user namespace",			      },
#endif
	{ tmpfs_protected_symlinks_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs following protected symlinks on idmapped mounts",						},
	{ tmpfs_protected_symlinks_idmapped_mounts_in_userns,			T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs following protected symlinks on idmapped mounts in user namespace",				},
	{ tmpfs_rename_crossing_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs cross idmapped mount rename",									},
	{ tmpfs_rename_from_idmapped_mount,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs rename from idmapped mounts",									},
	{ tmpfs_rename_from_idmapped_mount_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs rename from idmapped mounts in user namespace",						},
	{ tmpfs_setattr_truncate_idmapped,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs setattr truncate on idmapped mounts",								},
	{ tmpfs_setattr_truncate_idmapped_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs setattr truncate on idmapped mounts in user namespace",					},
	{ tmpfs_setgid_create_idmapped,						T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs create operations in directories with setgid bit set on idmapped mounts",			},
	{ tmpfs_setgid_create_idmapped_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs create operations in directories with setgid bit set on idmapped mounts in user namespace",	},
	{ tmpfs_setid_binaries_idmapped_mounts,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs setid binaries on idmapped mounts",								},
	{ tmpfs_setid_binaries_idmapped_mounts_in_userns,			T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs setid binaries on idmapped mounts in user namespace",						},
	{ tmpfs_setid_binaries_idmapped_mounts_in_userns_separate_userns,	T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs setid binaries on idmapped mounts in user namespace with different id mappings",		},
	{ tmpfs_sticky_bit_unlink_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs sticky bit unlink operations on idmapped mounts",						},
	{ tmpfs_sticky_bit_unlink_idmapped_mounts_in_userns,			T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs sticky bit unlink operations on idmapped mounts in user namespace",				},
	{ tmpfs_sticky_bit_rename_idmapped_mounts,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs sticky bit rename operations on idmapped mounts",						},
	{ tmpfs_sticky_bit_rename_idmapped_mounts_in_userns,			T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs sticky bit rename operations on idmapped mounts in user namespace",				},
	{ tmpfs_symlink_idmapped_mounts,					T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs symlink from idmapped mounts",									},
	{ tmpfs_symlink_idmapped_mounts_in_userns,				T_REQUIRE_USERNS | T_REQUIRE_IDMAPPED_MOUNTS,	"tmpfs symlink from idmapped mounts in user namespace",						},
};


const struct test_suite s_tmpfs_idmapped_mounts = {
	.tests = t_tmpfs,
	.nr_tests = ARRAY_SIZE(t_tmpfs),
};
