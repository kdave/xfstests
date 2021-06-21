#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 075
#
# fsx (non-AIO variant)
#
. ./common/preamble
_begin_fstest rw udf auto quick

status=0	# success is the default!

# Override the default cleanup function.
_cleanup()
{
    cd /
    rm -rf $TEST_DIR/fsx.* $tmp.*
}

# Import common functions.
. ./common/filter

_do_test()
{
    _n="$1"
    _param="$2"

    out=$TEST_DIR/fsx
    rm -rf $out
    if ! mkdir $out
    then
	echo "    failed to mkdir $out"
	status=1
	exit
    fi

    _filter_param=`echo "$_param" | sed\
	-e 's/-N [0-9][0-9]*/-N numops/' \
	-e 's/-l [0-9][0-9]*/-l filelen/'`

    echo ""
    echo "-----------------------------------------------"
    echo "fsx.$_n : $_filter_param"
    echo "-----------------------------------------------"

    # Skip the prealloc runs if the file system does not support preallocation
    if [ "$_n" = "1" -o "$_n" = "3" ]; then
        if [ -n "$testio" ]; then
	    return
	fi
    fi

    # This cd and use of -P gets full debug on "$RESULT_DIR" (not TEST_DEV)
    cd $out
    if ! $here/ltp/fsx $_param -P "$RESULT_DIR" $seq.$_n $FSX_AVOID &>/dev/null
    then
	echo "    fsx ($_param) failed, $? - compare $seqres.$_n.{good,bad,fsxlog}"
	mv $out/$seq.$_n $seqres.$_n.full
	mv "$RESULT_DIR"/$seq.$_n.fsxlog $seqres.$_n.fsxlog
	od -xAx $seqres.$_n.full > $seqres.$_n.bad
	od -xAx "$RESULT_DIR"/$seq.$_n.fsxgood > $seqres.$_n.good
	rm -f "$RESULT_DIR"/$seq.$_n.fsxgood
	status=1
	exit
    fi

    cd $here
    _check_test_fs
}

_usage()
{
    echo "$0: [-l filelen] [-n numops1] [-N numops2]"
}

_process_args()
{
    OPTIND=1
    while getopts "l:n:N:?" c $@
    do
        case $c
        in
            l)
                filelen=$OPTARG
		param_type="$param_type, overidde -l"
                ;;
            N)
                numops2=$OPTARG
		param_type="$param_type, overidde -N"
                ;;
            n)
                numops1=$OPTARG
		param_type="$param_type, overidde -n"
                ;;
	    ?)
		_usage
		exit	
		;;
	esac
    done
}

# real QA test starts here
_supported_fs generic
_require_test

size10=`expr 10 \* 1024 \* 1024`	# 10 megabytes
filelen=$size10
numops1=1000
numops2=10000

# can override the params here
_process_args "$@"

echo "Params are for $param_type" >>$seqres.full
echo "Params: n = $numops1 N = $numops2 l = $filelen" >>$seqres.full

echo "brevity is wit..."

_check_test_fs

# check if preallocation is supported, xfs_io resvsp command only prints out
# messages on failure.
testfile=$TEST_DIR/$$.xfs_io
testio=`$XFS_IO_PROG -f -c "resvsp 0 1" $testfile 2>&1`
rm -f $testfile 2>&1 > /dev/null

# Options:
# -d: debug output for all operations
# -l flen: the upper bound on file size (default 262144)
# -o oplen: the upper bound on operation size
# -N numops: total # operations to do (default infinity)
# -P: save .fsxlog and .fsxgood files in dirpath (default ./)
# -S seed: for random # generator (default 1) 0 gets timestamp
# -x: pre-allocate file space, exercising unwritten extents

_do_test 0 "-d -N $numops1 -S 0"
_do_test 1 "-d -N $numops1 -S 0 -x"
_do_test 2 "-d -N $numops2 -l $filelen -S 0"
_do_test 3 "-d -N $numops2 -l $filelen -S 0 -x"

rm -f $seq.*.fsx{good,log}
exit 0
