#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 Red Hat, Inc.  All Rights Reserved.
#
# FS QA Test No. 241
#
# Run parallel dbench & check for filesystem corruption
# This corrupted ext4 inode bitmaps due to races at one point
#
#
. ./common/preamble
_begin_fstest auto

# Import common functions.
. ./common/filter

_supported_fs generic
_require_test

[ "$DBENCH_PROG" = "" ] && _notrun "dbench not found"

echo "Silence is golden."

# real QA test starts here

rm -rf $TEST_DIR/dbench
mkdir $TEST_DIR/dbench

dbench -t 60 -D $TEST_DIR/dbench 4 >> $seqres.full

status=$?
exit
