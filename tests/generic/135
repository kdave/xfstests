#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2006 Silicon Graphics, Inc.  All Rights Reserved.
#
# FSQA Test No. 135
#
# Concurrent I/O to same file to ensure no deadlocks
#
. ./common/preamble
_begin_fstest metadata auto quick

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic

_require_odirect
_require_scratch
_scratch_mkfs >/dev/null 2>&1
_scratch_mount

cd $SCRATCH_MNT

# create file with async I/O
$XFS_IO_PROG -f -c 'pwrite -b 4k -S 0x12 0 4k' async_file > /dev/null

# create file with sync I/O
$XFS_IO_PROG -f -s -c 'pwrite -b 4k -S 0x34 0 4k' sync_file > /dev/null

# create file with direct I/O
$XFS_IO_PROG -f -d -c 'pwrite -b 4k -S 0x56 0 4k' direct_file > /dev/null

# create file, truncate and then dirty again
$XFS_IO_PROG -f -c 'pwrite -b 4k -S 0x78 0 4k' trunc_file > /dev/null
$XFS_IO_PROG -f -c 'truncate 2k' trunc_file > /dev/null
$XFS_IO_PROG -c 'pwrite 1k 0 1k' trunc_file > /dev/null

pushd / > /dev/null
_scratch_cycle_mount
popd > /dev/null

# check file size and contents
od -Ad -x async_file
od -Ad -x sync_file
od -Ad -x direct_file
od -Ad -x trunc_file

rm -f async_file sync_file direct_file trunc_file

status=0
exit 
