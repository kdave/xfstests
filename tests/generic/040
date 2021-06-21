#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015 SUSE Linux Products GmbH. All Rights Reserved.
#
# FS QA Test No. 040
#
# This test is motivated by an fsync issue discovered in btrfs.
# The issue in btrfs was that adding a new hard link to an inode that already
# had a large number of hardlinks and fsync the inode, would make the fsync
# log replay code update the inode with a wrong link count (smaller than the
# correct value). This resulted later in dangling directory index entries,
# after removing most of the hard links (correct_value - wrong_value), that
# were visible to user space but it was impossible to delete them or do
# any other operation on them (since they pointed to an inode that didn't
# exist anymore, resulting in -ESTALE errors).
#
# The btrfs issue was fixed by the following linux kernel patch:
#
#    Btrfs: fix fsync when extend references are added to an inode
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

# Add one more link to the inode that ends up being a btrfs extref and fsync
# the inode.
ln $SCRATCH_MNT/foo $SCRATCH_MNT/foo_link_3001
$XFS_IO_PROG -c "fsync" $SCRATCH_MNT/foo

_flakey_drop_and_remount

# Now after the fsync log replay btrfs left our inode with a wrong link count N,
# which was smaller than the correct link count M (N < M).
# So after removing N hard links, the remaining M - N directory entries were
# still visible to user space but it was impossible to do anything with them
# because they pointed to an inode that didn't exist anymore. This resulted in
# stale file handle errors (-ESTALE) when accessing those dentries for example.
#
# So remove all hard links except the first one and then attempt to read the
# file, to verify we don't get an -ESTALE error when accessing the inode.
#
# The btrfs fsck tool also detected the incorrect inode link count and it
# reported an error message like the following:
#
# root 5 inode 257 errors 2001, no inode item, link count wrong
#   unresolved ref dir 256 index 2978 namelen 13 name foo_link_2976 filetype 1 errors 4, no inode ref
#
# The fstests framework automatically calls fsck after a test is run, so we
# don't need to call fsck explicitly here.

echo "Link count before rm foo_link_*: $(stat -c %h $SCRATCH_MNT/foo)"
rm -f $SCRATCH_MNT/foo_link_*
echo "Link count after rm foo_link_*: $(stat -c %h $SCRATCH_MNT/foo)"
cat $SCRATCH_MNT/foo

status=0
exit
