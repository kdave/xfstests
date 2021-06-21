#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2014 SUSE Linux Products GmbH. All Rights Reserved.
#
# FS QA Test No. 034
#
# This test is motivated by a bug found in btrfs when replaying a directory
# from the fsync log. The issue was that if a directory entry is both found
# in the persisted metadata and in the fsync log, at log replay time the
# directory got set with a wrong i_size. This had the consequence of not being
# able to rmdir empty directories (failed with errno ENOTEMPTY).
# This was fixed in btrfs with the following linux kernel patch:
#
#     Btrfs: fix directory recovery from fsync log
#
. ./common/preamble
_begin_fstest auto quick metadata log

# Override the default cleanup function.
_cleanup()
{
	_cleanup_flakey
}

# Import common functions.
. ./common/filter
. ./common/dmflakey

# real QA test starts here
_supported_fs generic
_require_scratch
_require_dm_target flakey

_scratch_mkfs >> $seqres.full 2>&1
_require_metadata_journaling $SCRATCH_DEV
_init_flakey
_mount_flakey

mkdir $SCRATCH_MNT/test_dir
touch $SCRATCH_MNT/test_dir/foo

# Invoke sync here because it's necessary to trigger the original bug in btrfs.
# The intention is that at log recovery time we have a dir entry for 'foo' both
# in the fs/subvol tree and in the log tree - this is necessary to trigger the
# bug on btrfs.
sync

touch $SCRATCH_MNT/test_dir/bar
$XFS_IO_PROG -c "fsync" $SCRATCH_MNT/test_dir
$XFS_IO_PROG -c "fsync" $SCRATCH_MNT/test_dir/bar

# In the original btrfs bug, log replay would update the directory's inode
# i_size incorrectly - it would sum again the size of dentry 'foo' (3) to
# the inode's i_size, which is incorrect because the dentry was already
# persisted before (in the fs/subvol tree).
_flakey_drop_and_remount

[ -f $SCRATCH_MNT/test_dir/foo ] || echo "file foo is missing"
[ -f $SCRATCH_MNT/test_dir/bar ] || echo "file bar is missing"

rm -f $SCRATCH_MNT/test_dir/foo
rm -f $SCRATCH_MNT/test_dir/bar

# In btrfs removing all entries from a directory should set the directory's
# inode i_size to 0, but with this bug that didn't happen and this made
# an rmdir fail with errno ENOTEMPTY (even though the directory had no more
# entries in it).
rmdir $SCRATCH_MNT/test_dir
[ -d $SCRATCH_MNT/test_dir ] && echo "rmdir didn't succeed"

_unmount_flakey

echo "Silence is golden"

status=0
exit
