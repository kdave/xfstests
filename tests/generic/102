#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Fujitsu.  All Rights Reserved.
#
# FS QA Test No. 102
#
# TEST busy loop of write and delete in a filesystem.
# Sometimes writes will failed on NO_SPACE when disk almost full
# in btrfs. It is long-term problem since very beginning for btrfs
#
# This issue was fixed by the patchset named:
#
# btrfs: Fix no_space on dd and rm loop < from zhaolei@cn.fujitsu.com >
#
. ./common/preamble
_begin_fstest auto rw

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_scratch

dev_size=$((1024 * 1024 * 1024))     # 1GB filesystem
_scratch_mkfs_sized $dev_size >>$seqres.full 2>&1
_scratch_mount

for ((i = 0; i < 10; i++)); do
	echo "loop $i" >>$seqres.full

	$XFS_IO_PROG -f -c "pwrite -b 1m 0 800m" "$SCRATCH_MNT"/file | \
_filter_xfs_io | _filter_scratch

	rm -f "$SCRATCH_MNT"/file
done

status=0
exit
