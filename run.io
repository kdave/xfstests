#!/bin/sh
# (very) simple file IO tests.
# TODO: make file size multiple of physmem.

filesize=1g
iosize=64k

if [ $# -gt 0 ]; then
	printf "%s,%s,%s,%s,%s\n" type bytes ops time bytes/sec ops/sec
	exit 0
fi

# buffered write
echo -n bwrite,
xfs_io iofile	-Fft	-c "pwrite -C -b $iosize 0 $filesize"

# direct write
echo -n dwrite,
xfs_io iofile	-Fftd	-c "pwrite -C -b $iosize 0 $filesize"

# buffered re-write
echo -n brwrite,
xfs_io iofile	-F	-c "pwrite -C -b $iosize 0 $filesize"

# direct re-write
echo -n drwrite,
xfs_io iofile	-Fd	-c "pwrite -C -b $iosize 0 $filesize"

# buffered read
echo -n bread,
xfs_io iofile	-F	-c "pread -C -b $iosize 0 $filesize"

# buffered re-read
echo -n brread,
xfs_io iofile	-F	-c "pread -C -b $iosize 0 $filesize"

# direct read
echo -n dread,
xfs_io iofile	-Fd	-c "pread -C -b $iosize 0 $filesize"

# direct re-read
echo -n drread,
xfs_io iofile	-Fd	-c "pread -C -b $iosize 0 $filesize"

