#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015 SUSE Linux Products GmbH. All Rights Reserved.
#
# FS QA Test No. 041
#
# This test is motivated by an fsync issue discovered in btrfs.
# The steps to trigger the issue were:
#
# 1) remove an hard link from an inode with a large number of hard links;
# 2) add a new hard link;
# 3) add another hard link with the same name as the one removed in step 1;
# 4) fsync the inode.
#
# These steps made the btrfs fsync log replay fail (with the -EOVERFLOW error),
# making the filesystem unmountable, requiring the use of btrfs-zero-log (it
# wipes the fsync log) in order to make the filesystem mountable again (but
# losing some data/metadata).
#
# The btrfs issue was fixed by the following linux kernel patches:
#
#  Btrfs: fix fsync when extend references are added to an inode
#  Btrfs: fix fsync log replay for inodes with a mix of regular refs and extrefs
#
# This issue was present in btrfs since the extrefs (extend references)
# feature was added (2012).
#
. ./common/preamble
_begin_fstest metadata auto quick log

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
_require_hardlinks
_require_dm_target flakey

# If the test filesystem is btrfs, make sure we create a filesystem with
# the extend references (extrefs) feature enabled (it's enabled by default
# in recent versions of btrfs-progs).
if [ "$FSTYP" = "btrfs" ]; then
	_scratch_mkfs "-O extref" >> $seqres.full 2>&1
else
	_scratch_mkfs >> $seqres.full 2>&1
fi

_require_metadata_journaling $SCRATCH_DEV
_init_flakey
_mount_flakey

# Create a test file with 3001 hard links. This number is large enough to
# make btrfs start using extrefs at some point even if the fs has the maximum
# possible leaf/node size (64Kb).
echo "hello world" > $SCRATCH_MNT/foo
for i in `seq 1 3000`; do
	ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_`printf "%04d" $i`
done

# Make sure all metadata and data are durably persisted.
sync

# Now remove one link, add a new one with a new name, add another new one with
# the same name as the one we just removed and fsync the inode.
rm -f $SCRATCH_MNT/foo_link_0001
ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_3001
ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_0001
rm -f $SCRATCH_MNT/foo_link_0002
ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_3002
ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_3003
$XFS_IO_PROG -c "fsync" $SCRATCH_MNT/foo

_flakey_drop_and_remount

# Check that the number of hard links is correct, we are able to remove all
# the hard links and read the file's data. This is just to verify we don't
# get stale file handle errors (due to dangling directory index entries that
# point to inodes that no longer exist).
echo "Link count: $(stat -c %h $SCRATCH_MNT/foo)"
[ -f $SCRATCH_MNT/foo ] || echo "Link foo is missing"
for ((i = 1; i <= 3003; i++)); do
	name=foo_link_`printf "%04d" $i`
	if [ $i -eq 2 ]; then
		[ -f $SCRATCH_MNT/$name ] && echo "Link $name found"
	else
		[ -f $SCRATCH_MNT/$name ] || echo "Link $name is missing"
	fi
done
rm -f $SCRATCH_MNT/foo_link_*
cat $SCRATCH_MNT/foo
rm -f $SCRATCH_MNT/foo

status=0
exit
