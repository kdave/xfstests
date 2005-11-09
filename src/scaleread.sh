#!/bin/sh
#
# Copyright (c) 2003-2004 Silicon Graphics, Inc.  All Rights Reserved.
#

help() {
cat <<END
Measure scaling of multiple cpus readin the same set of files.
(NASA testcase).
	Usage:  $0 [-b <bytes>] [-f <files>] [-s] [-B] [-v] cpus ...
			or
		$0 -i [-b <bytes>] [-f <files>] 

	  -b file size in bytes
	  -f number of files
	  -s keep processes synchronized when reading files
	  -B use bcfree to free buffer cache pages before each run
END
exit 1
}

err () {
	echo "ERROR - $*"
	exit 1
}

BYTES=8192
FILES=10
SYNC=""
VERBOSE=""
STRIDED=""
BCFREE=0
INIT=0
OPTS="f:b:vsiSBH"
while getopts "$OPTS" c ; do
	case $c in
		H)  help;;
		f)  FILES=${OPTARG};;
		b)  BYTES=${OPTARG};;
		i)  INIT=1;;
		B)  BCFREE=1;;
		S)  STRIDED="-S";;
		s)  SYNC="-s";;
		v)  VERBOSE="-v";;
		\?) help;;
	esac

done
shift `expr $OPTIND - 1`

if [ $INIT -gt 0 ] ; then
	echo "Initializing $BYTES bytes, $FILES files"
	./scaleread $VERBOSE -i -b $BYTES -f $FILES 
	sync
else
	[ $# -gt 0 ] || help
	echo "Testing $BYTES bytes, $FILES files"
	for CPUS in $* ; do
		[ $BCFREE -eq 0 ] || bcfree -a
		/usr/bin/time -f "$CPUS:  %e wall,    %S sys,   %U user" ./scaleread \
			$SYNC $STRIDED $VERBOSE -b $BYTES -f $FILES -c $CPUS
	done
fi

