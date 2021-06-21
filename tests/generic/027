#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2014 Red Hat Inc.  All Rights Reserved.
#
# FS QA Test No. generic/027
#
# Run 8 processes writing 1k files to seperate files in seperate dirs to
# hit ENOSPC on small fs with little free space. Loop for 100 iterations.
#
# Regression test for
# 34cf865 ext4: fix deadlock when writing in ENOSPC conditions
#
. ./common/preamble
_begin_fstest auto enospc

create_file()
{
	local dir=$1
	local direct=$2
	local i=0

	mkdir -p $dir >/dev/null 2>&1
	while $XFS_IO_PROG -f $direct -c "pwrite 0 1k" $dir/file_$i >/dev/null 2>&1; do
		let i=$i+1
	done
}

# Import common functions.
. ./common/filter

# real QA test starts here
_supported_fs generic

_require_scratch

echo "Silence is golden"

_scratch_mkfs_sized $((256 * 1024 * 1024)) >>$seqres.full 2>&1
_scratch_mount

echo "Reserve 2M space" >>$seqres.full
$XFS_IO_PROG -f -c "pwrite 0 2m" $SCRATCH_MNT/testfile >>$seqres.full 2>&1
echo "Fulfill the fs" >>$seqres.full
$XFS_IO_PROG -f -c "pwrite 0 254m" $SCRATCH_MNT/bigfile >>$seqres.full 2>&1
echo "Remove reserved file" >>$seqres.full
rm -f $SCRATCH_MNT/testfile

loop=100
# btrfs takes much longer time, reduce the loop count
if [ "$FSTYP" == "btrfs" ]; then
	loop=10
fi

dir=$SCRATCH_MNT/testdir
echo -n "iteration" >>$seqres.full
i=1
while [ $i -le $loop ]; do
	echo -n " $i" >>$seqres.full
	nr_worker=8
	while [ $nr_worker -gt 0 ]; do
		# half buffered I/O half direct I/O
		if [ `expr $nr_worker % 2` -eq 0 ]; then
			create_file $dir/$nr_worker -d >>$seqres.full &
		else
			create_file $dir/$nr_worker >>$seqres.full &
		fi
		let nr_worker=$nr_worker-1
	done
	wait
	rm -rf $dir
	let i=$i+1
done
_scratch_unmount

status=0
exit
