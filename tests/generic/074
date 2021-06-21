#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# FS QA Test No. 074
#
# fstest
#
. ./common/preamble
_begin_fstest rw udf auto

fstest_dir=$TEST_DIR/fstest
status=0	# success is the default!

# Override the default cleanup function.
_cleanup()
{
    rm -rf $fstest_dir.* $tmp.*
}

# Import common functions.
. ./common/filter

_supported_fs generic
_require_test

_do_test()
{
    _n="$1"
    _param="$2"

    out=$fstest_dir.$_n
    rm -rf $out
    if ! mkdir $out
    then
        echo "    failed to mkdir $out"
        status=1
        exit
    fi

    _filter_param=`echo "$_param" | sed\
	-e 's/-n [0-9][0-9]*/-n children/' \
	-e 's/-l [0-9][0-9]*/-l loops/' \
	-e 's/-f [0-9][0-9]*/-f files/'`

    echo ""
    echo "-----------------------------------------------"
    echo "fstest.$_n : $_filter_param"
    echo "-----------------------------------------------"
    if ! $here/src/fstest $_param -p $out >>$seqres.full
    then
        echo "    fstest ($_param) returned $? - see $seqres.full"
        status=1
	exit
    fi

    _check_test_fs
}

_usage()
{
    echo "$0: [-f files] [-l loops] [-n children]"
}

_process_args()
{
    OPTIND=1
    while getopts "f:l:n:?" c $@
    do
        case $c
        in
            f)
                numfiles=$OPTARG
		param_type="$param_type, overidde -f"
                ;;
            l)
                numloops=$OPTARG
		param_type="$param_type, overidde -l"
                ;;
            n)
                numchildren=$OPTARG
		param_type="$param_type, overidde -n"
                ;;
	    ?)
		_usage
		exit	
		;;
	esac
    done
}

#
# set params
# These params can take a while on different CPUs/OSs
# so we set them differently for different machines
#
if uname -a | grep -q SMP; then
	numloops=10
	numfiles=5
	numchildren=3
	param_type="Linux SMP"
else
	numloops=2
	numfiles=3
	numchildren=3
	param_type="Linux UP"
fi

# can override the params here
_process_args "$@"

echo "Params are for $param_type" >>$seqres.full
echo "Params: n = $numchildren l = $numloops f = $numfiles" >>$seqres.full

_check_test_fs

# Options:
# -F                    generate files with holes  (default=no)
# -n num_children       set number of child processes (default=1)
# -f num_files          set number of files (default=1)
# -s file_size          set file sizes (default=1MiB)
# -b block_size         set block (IO) size (default=1024)
# -l loops              set loop count (default=100)
# -m                    use mmap (default=no)
# -S                    use synchronous IO (default=no)
# -P                    preallocate space (default=no)

size10=`expr 10 \* 1024 \* 1024`	# 10 megabytes
size30=`expr 30 \* 1024 \* 1024`	# 30 megabytes

_do_test 0 "-l $numloops"

_do_test 1 "-l $numloops -s $size10 -b 8192 -m"

_do_test 2 "-n $numchildren -F -l $numloops -f $numfiles -s $size30 -b 512"

_do_test 3 "-n $numchildren -F -l $numloops -f $numfiles -s $size30 -b 512 -m"

_do_test 4 "-n $numchildren -F -l $numloops -f $numfiles -s $size10 -b 512 -mS"
