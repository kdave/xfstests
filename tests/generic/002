#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 002
#
# simple inode link count test for a regular file
#
. ./common/preamble
_begin_fstest metadata udf auto quick

# Import common functions.
. ./common/filter

status=0	# success is the default!

# real QA test starts here
_supported_fs generic
_require_test
_require_hardlinks

echo "Silence is goodness ..."

# ensure target directory exists
mkdir `dirname $TEST_DIR/$tmp` 2>/dev/null

touch $TEST_DIR/$tmp.1
for l in 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
do
    ln $TEST_DIR/$tmp.1 $TEST_DIR/$tmp.$l
    x=`$here/src/lstat64 $TEST_DIR/$tmp.1 | sed -n -e '/ Links: /s/.*Links: *//p'`
    if [ "$l" -ne $x ]
    then
	echo "Arrgh, created link #$l and lstat64 looks like ..."
	$here/src/lstat64 $TEST_DIR/$tmp.1
	status=1
    fi
done

for l in 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1
do
    x=`$here/src/lstat64 $TEST_DIR/$tmp.1 | sed -n -e '/ Links: /s/.*Links: *//p'`
    if [ "$l" -ne $x ]
    then
	echo "Arrgh, about to remove link #$l and lstat64 looks like ..."
	$here/src/lstat64 $TEST_DIR/$tmp.1
	status=1
    fi
    rm -f $TEST_DIR/$tmp.$l
done

# success, all done
exit
