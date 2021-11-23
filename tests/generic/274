#! /bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2011-2012 Fujitsu, Inc.  All Rights Reserved.
#
# FS QA Test No. 274
#
# preallocation test:
# Preallocate space to a file, and fill the rest of the fs to 100%.
# Then test a write into that preallocated space, which should succeed.
#
#creator

. ./common/preamble
_begin_fstest auto rw prealloc enospc

status=0    # success is the default!

# Override the default cleanup function.
_cleanup()
{
	cd /
	rm -f $tmp.*
	_scratch_unmount
}

. ./common/filter

# real QA test starts here
_supported_fs generic
_require_scratch
_require_xfs_io_command "falloc" "-k"

# Compression can exhaust metadata space here for btrfs and cause spurious
# failurs because we hit a metadata ENOSPC, skip if we have compression enabled
_require_no_compress

echo "------------------------------"
echo "preallocation test"
echo "------------------------------"

_scratch_unmount 2>/dev/null
_scratch_mkfs_sized $((2 * 1024 * 1024 * 1024)) >>$seqres.full 2>&1
_scratch_mount

# Create a 4k file and Allocate 4M past EOF on that file
$XFS_IO_PROG -f -c "pwrite 0 4k" -c "falloc -k 4k 4m" $SCRATCH_MNT/test \
	>>$seqres.full 2>&1 || _fail "failed to create test file"

# Fill the rest of the fs completely
# Note, this will show ENOSPC errors in $seqres.full, that's ok.
echo "Fill fs with 1M IOs; ENOSPC expected" >> $seqres.full
dd if=/dev/zero of=$SCRATCH_MNT/tmp1 bs=1M >>$seqres.full 2>&1
echo "Fill fs with 4K IOs; ENOSPC expected" >> $seqres.full
dd if=/dev/zero of=$SCRATCH_MNT/tmp2 bs=4K >>$seqres.full 2>&1
sync
# Last effort, use O_SYNC
echo "Fill fs with 4K DIOs; ENOSPC expected" >> $seqres.full
dd if=/dev/zero of=$SCRATCH_MNT/tmp3 bs=4K oflag=sync >>$seqres.full 2>&1
# Save space usage info
echo "Post-fill space:" >> $seqres.full
df $SCRATCH_MNT >>$seqres.full 2>&1

# Now attempt a write into all of the preallocated space -
# in a very nasty way, badly fragmenting it and then filling it in.
echo "Fill in prealloc space; fragment at offsets:" >> $seqres.full
for i in `seq 1 2 1023`; do
	echo -n "$i " >> $seqres.full
	dd if=/dev/zero of=$SCRATCH_MNT/test seek=$i bs=4K count=1 conv=notrunc \
		>>$seqres.full 2>/dev/null || _fail "failed to write to test file"
done
sync
echo >> $seqres.full
echo "Fill in prealloc space; fill holes at offsets:" >> $seqres.full
for i in `seq 2 2 1023`; do
	echo -n "$i " >> $seqres.full
	dd if=/dev/zero of=$SCRATCH_MNT/test seek=$i bs=4K count=1 conv=notrunc \
		>>$seqres.full 2>/dev/null || _fail "failed to fill test file"
done
sync
echo >> $seqres.full

echo "done"
exit
