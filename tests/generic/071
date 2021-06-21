#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2015 SUSE Linux Products GmbH. All Rights Reserved.
#
# FS QA Test No. 071
#
# Test extent pre-allocation (using fallocate) into a region that already has a
# pre-allocated extent that ends beyond the file's size. Verify that if the fs
# is unmounted immediately after, the file's size and content are not lost.
#
. ./common/preamble
_begin_fstest auto quick prealloc

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_scratch
_require_xfs_io_command "falloc" "-k"

_scratch_mkfs >> $seqres.full 2>&1
_scratch_mount

# Create our test file with a pre-allocated extent that doesn't increase the
# file's size.
$XFS_IO_PROG -f -c "falloc -k 0 1M" $SCRATCH_MNT/foo

# Write some data to our file.
$XFS_IO_PROG -c "pwrite -S 0xaa 0 256K" $SCRATCH_MNT/foo | _filter_xfs_io

# Now call fallocate again, but allowing it to increase the file's size and
# cover a range that is entirely covered by the extent that we previously
# pre-allocated.
$XFS_IO_PROG -c "falloc 0 512K" $SCRATCH_MNT/foo

# Now ummount and mount again the fs. After this we expect the file's size to
# be 512Kb.
_scratch_cycle_mount

# Now check that all data we wrote before are available and the file size is
# 512Kb.
echo "File content after remount:"
od -t x1 $SCRATCH_MNT/foo

status=0
exit
