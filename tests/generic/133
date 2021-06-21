#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2006 Silicon Graphics, Inc.  All Rights Reserved.
#
# FSQA Test No. 133
#
# Concurrent I/O to same file to ensure no deadlocks
#
. ./common/preamble
_begin_fstest rw auto

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test
_require_odirect

echo "Buffered writer, buffered reader"
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
$XFS_IO_PROG -f -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test >/dev/null &
$XFS_IO_PROG -c 'pread -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
wait
rm $TEST_DIR/io_test

echo "Direct writer, buffered reader"
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test >/dev/null &
$XFS_IO_PROG -c 'pread -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
wait
rm $TEST_DIR/io_test

echo "Buffered writer, direct reader"
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
$XFS_IO_PROG -f -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test >/dev/null &
$XFS_IO_PROG -d -c 'pread -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
wait
rm $TEST_DIR/io_test

echo "Direct writer, direct reader"
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
$XFS_IO_PROG -f -d -c 'pwrite -b 64k 0 512m' $TEST_DIR/io_test >/dev/null &
$XFS_IO_PROG -d -c 'pread -b 64k 0 512m' $TEST_DIR/io_test > /dev/null
wait
rm $TEST_DIR/io_test

status=0
exit 
