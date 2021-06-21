#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
# Copyright (c) 2009 Red Hat, Inc. All Rights Reserved.
#
# FS QA Test No. 213
#
# Check some unwritten extent boundary conditions, fallocate version.
#
# Based on xfs-specific test 072
#
. ./common/preamble
_begin_fstest rw auto prealloc quick enospc

tmp=$TEST_DIR/$$

# Import common functions.
. ./common/filter

# real QA test starts here
# generic, but xfs_io's fallocate must work
_supported_fs generic
# only Linux supports fallocate

[ -n "$XFS_IO_PROG" ] || _notrun "xfs_io executable not found"

_require_test
_require_xfs_io_command "falloc"

# check there's enough freespace on $TEST_DIR ... (1GiB + 1MiB)
avail=`df -P $TEST_DIR | awk 'END {print $4}'`
[ "$avail" -ge 1049600 ] || _notrun "Test device is too small ($avail KiB)"

# reserve 1GiB, truncate at 100bytes
$XFS_IO_PROG -f -c 'falloc 0 1g' -c 'truncate 100' $TEST_DIR/ouch
rm -f $TEST_DIR/ouch

# reserve 1GiB, truncate at 1GiB
$XFS_IO_PROG -f -c 'falloc 0 1g' -c 'truncate 1g' $TEST_DIR/ouch
rm -f $TEST_DIR/ouch

# reserve 1GiB, truncate at 2GiB
$XFS_IO_PROG -f -c 'falloc 0 1g' -c 'truncate 2g' $TEST_DIR/ouch
rm -f $TEST_DIR/ouch

# reserve 1GiB, 1GiB hole, reserve 1MiB, truncate at 3GiB
$XFS_IO_PROG -f -c 'falloc 0 1g' -c 'falloc 2g 1m' -c 'truncate 3g' $TEST_DIR/ouch
rm -f $TEST_DIR/ouch

# Try to reserve more space than we have
echo "We should get: fallocate: No space left on device"
echo "Strangely, xfs_io sometimes says \"Success\" when something went wrong, FYI"

let toobig=$avail*2
$XFS_IO_PROG -f -c "falloc 0 ${toobig}k" $TEST_DIR/ouch
rm -f $TEST_DIR/ouch

# success, all done
status=0
exit
