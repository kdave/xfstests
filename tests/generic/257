#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2011 Gražvydas Ignotas
#
#
# Check that no duplicate d_off values are returned and that those
# values are seekable. Most work is done by the C program here.
#
. ./common/preamble
_begin_fstest dir auto quick

# Override the default cleanup function.
_cleanup()
{
	rm -rf $TEST_DIR/ttt
}

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic
_require_test

mkdir $TEST_DIR/ttt
for n in {1..168}; do
    touch $TEST_DIR/ttt/$n;
done
$here/src/t_dir_offset2 $TEST_DIR/ttt

# success, all done
echo "*** done"
status=0
