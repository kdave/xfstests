#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 006
#
# permname
#
. ./common/preamble
_begin_fstest dir udf auto quick

status=0	# success is the default!

# Override the default cleanup function.
_cleanup()
{
    cd /
    rm -f $tmp.*
    rm -rf $TEST_DIR/permname.$$
}

_count()
{
    $AWK_PROG '
        BEGIN   { count = 0 }
                { count ++ }
        END     { print count " files created" }
    '
}

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test

mkdir $TEST_DIR/permname.$$

echo ""
echo "single thread permname"
echo "----------------------"
mkdir $TEST_DIR/permname.$$/a
cd $TEST_DIR/permname.$$/a
$here/src/permname -c 4 -l 6 -p 1 || echo "permname returned $?"
find . | _count

echo ""
echo "multi thread permname"
echo "----------------------"
mkdir $TEST_DIR/permname.$$/b
cd $TEST_DIR/permname.$$/b
$here/src/permname -c 4 -l 6 -p 4 || echo "permname returned $?"
find . | _count

exit
