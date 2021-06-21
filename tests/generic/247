#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2010 Red Hat, Inc.  All Rights Reserved.
#
# FS QA Test No. 247
#
# Test for race between direct I/O and mmap
#
. ./common/preamble
_begin_fstest auto quick rw

_register_cleanup "_cleanup" BUS

# Override the default cleanup function.
_cleanup()
{
    cd /
    rm -f $tmp.* $testfile
    wait
}

# Import common functions.
. ./common/filter

# real QA test starts here

# Modify as appropriate.
_supported_fs generic
_require_test

# this test leaves a 512MB file around if we abort the test during the run via a
# reboot or kernel panic. Hence just name the file $seq so that we can always
# clean up on the next run and not leave large stale files around on the testdir
# that can lead to ENOSPC issues over time.
testfile=$TEST_DIR/$seq
rm -f $testfile

loops=500
iosize=1048576

# Initialise file
dd if=/dev/zero of=$testfile bs=$iosize count=$loops &> /dev/null
[ $? -ne 0 ] && exit
sync
	
# Direct I/O overwriter
dd if=/dev/zero of=$testfile oflag=direct bs=$iosize count=$loops conv=notrunc &> /dev/null &
[ $? -ne 0 ] && exit

# Mmap writer
start=`expr $loops - 1`
for i in `seq $start -1 0`
do
	offset=`expr $i \* $iosize`
	$XFS_IO_PROG -f -c "mmap -w $offset $iosize" -c "mwrite $offset $iosize" $testfile
	[ $? -ne 0 ] && exit
done

wait

echo "Silence is golden."

# unmount and check dmesg, filtering out expected warnings about mixed
# mmap/dio
_test_unmount
_check_dmesg _filter_aiodio_dmesg

status=$?
exit
