#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2009 Eric Sandeen.  All Rights Reserved.
#
# FS QA Test No. 226
#
# Test for prealloc space leaks by rewriting the same file in a loop
#
. ./common/preamble
_begin_fstest auto enospc

# Import common functions.

_supported_fs generic
_require_scratch
_require_odirect

# real QA test starts here

_scratch_unmount 2>/dev/null
echo "--> mkfs 256m filesystem"
_scratch_mkfs_sized `expr 256 \* 1024 \* 1024` >> $seqres.full 2>&1
_scratch_mount

loops=16

# Buffer size argument supplied to xfs_io "pwrite" command
buffer="-b $(expr 512 \* 1024)"

echo "--> $loops buffered 64m writes in a loop"
for I in `seq 1 $loops`; do
	echo -n "$I "
	$XFS_IO_PROG -f \
		-c "pwrite ${buffer} 0 64m" $SCRATCH_MNT/test >> $seqres.full
	rm -f $SCRATCH_MNT/test
done

echo
_scratch_cycle_mount

echo "--> $loops direct 64m writes in a loop"
for I in `seq 1 $loops`; do
	echo -n "$I "
	$XFS_IO_PROG -f -d \
		-c "pwrite ${buffer} 0 64m" $SCRATCH_MNT/test >> $seqres.full
	rm -f $SCRATCH_MNT/test 
done

echo
_scratch_unmount

status=0
exit
