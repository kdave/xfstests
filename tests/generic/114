#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015 Red Hat, Inc.  All Rights Reserved.
#
# FS QA Test No. 114
#
# Test races while extending past EOF via sub-block AIO writes
#
. ./common/preamble
_begin_fstest rw aio auto quick

# Override the default cleanup function.
_cleanup()
{
    cd /
    rm -f $TEST_DIR/tst-aio-dio-eof-race
}

# Import common functions.
. ./common/filter

_supported_fs generic

_require_test
_require_sparse_files
_require_aiodio aio-dio-eof-race

# Test does 512 byte DIO, so make sure that'll work
logical_block_size=`_min_dio_alignment $TEST_DEV`

if [ "$logical_block_size" -gt "512" ]; then
	_notrun "device block size: $logical_block_size greater than 512"
fi

# We don't mind 512-byte fs blocks; the IOs won't be sub-block,
# but the test should still pass, even if it doesn't stress the code
# we're targeting.

# Note, this test does several extending loops internally
$AIO_TEST $TEST_DIR/tst-aio-dio-eof-race

status=$?
exit
