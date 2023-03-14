/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __IDMAPPED_MOUNTS_H
#define __IDMAPPED_MOUNTS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils.h"

extern const struct test_suite s_idmapped_mounts;
extern const struct test_suite s_fscaps_in_ancestor_userns;
extern const struct test_suite s_nested_userns;
extern const struct test_suite s_setattr_fix_968219708108;
extern const struct test_suite s_setxattr_fix_705191b03d50;
extern const struct test_suite s_setgid_create_umask_idmapped_mounts;
extern const struct test_suite s_setgid_create_acl_idmapped_mounts;

/* Core tests */
int tcore_acls(const struct vfstest_info *info);
int tcore_create_in_userns(const struct vfstest_info *info);
int tcore_device_node_in_userns(const struct vfstest_info *info);
int tcore_fsids_mapped(const struct vfstest_info *info);
int tcore_fsids_unmapped(const struct vfstest_info *info);
int tcore_expected_uid_gid_idmapped_mounts(const struct vfstest_info *info);
int tcore_fscaps_idmapped_mounts(const struct vfstest_info *info);
int tcore_fscaps_idmapped_mounts_in_userns(const struct vfstest_info *info);
int tcore_fscaps_idmapped_mounts_in_userns_separate_userns(const struct vfstest_info *info);
int tcore_hardlink_crossing_idmapped_mounts(const struct vfstest_info *info);
int tcore_hardlink_from_idmapped_mount(const struct vfstest_info *info);
int tcore_hardlink_from_idmapped_mount_in_userns(const struct vfstest_info *info);
#ifdef HAVE_LIBURING_H
int tcore_io_uring_idmapped(const struct vfstest_info *info);
int tcore_io_uring_idmapped_userns(const struct vfstest_info *info);
int tcore_io_uring_idmapped_unmapped(const struct vfstest_info *info);
int tcore_io_uring_idmapped_unmapped_userns(const struct vfstest_info *info);
#endif
int tcore_protected_symlinks_idmapped_mounts(const struct vfstest_info *info);
int tcore_protected_symlinks_idmapped_mounts_in_userns(const struct vfstest_info *info);
int tcore_rename_crossing_idmapped_mounts(const struct vfstest_info *info);
int tcore_rename_from_idmapped_mount(const struct vfstest_info *info);
int tcore_rename_from_idmapped_mount_in_userns(const struct vfstest_info *info);
int tcore_setattr_truncate_idmapped(const struct vfstest_info *info);
int tcore_setattr_truncate_idmapped_in_userns(const struct vfstest_info *info);
int tcore_setgid_create_idmapped(const struct vfstest_info *info);
int tcore_setgid_create_idmapped_in_userns(const struct vfstest_info *info);
int tcore_setid_binaries_idmapped_mounts(const struct vfstest_info *info);
int tcore_setid_binaries_idmapped_mounts_in_userns(const struct vfstest_info *info);
int tcore_setid_binaries_idmapped_mounts_in_userns_separate_userns(const struct vfstest_info *info);
int tcore_sticky_bit_unlink_idmapped_mounts(const struct vfstest_info *info);
int tcore_sticky_bit_unlink_idmapped_mounts_in_userns(const struct vfstest_info *info);
int tcore_sticky_bit_rename_idmapped_mounts(const struct vfstest_info *info);
int tcore_sticky_bit_rename_idmapped_mounts_in_userns(const struct vfstest_info *info);
int tcore_symlink_idmapped_mounts(const struct vfstest_info *info);
int tcore_symlink_idmapped_mounts_in_userns(const struct vfstest_info *info);

#endif /* __IDMAPPED_MOUNTS_H */
