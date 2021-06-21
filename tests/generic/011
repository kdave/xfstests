#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 011
#
# dirstress
#
. ./common/preamble
_begin_fstest dir udf auto quick

out=""
status=0	# success is the default!

# Override the default cleanup function.
_cleanup()
{
    cd /
    rm -f $tmp.*
    [ -n "$out" ] && rm -rf $out
}

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test

out=$TEST_DIR/dirstress.$$

_test()
{
    test="$1"
    args="$2"
    count="$3"
    
    echo "*** TEST $test $args -f <count>"
    if ! $here/src/dirstress -d $out -f $count $args >$tmp.out 2>&1
    then
        echo "    dirstress failed"
        echo "*** TEST $test -d $out -f $count $args" >>$seqres.full
        cat $tmp.out >>$seqres.full
        status=1
    fi
}

# dirstress doesn't check returns - this is a crash & burn test.
if ! mkdir $out
then
    echo "!! couldn't mkdir $out"
    status=1
    exit
fi

count=1000
_test 1 "-p 1 -n 1" $count
_test 2 "-p 5 -n 1" $count
_test 3 "-p 5 -n 5" $count

# if error
exit
