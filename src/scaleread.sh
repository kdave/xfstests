#!/bin/sh
#
# Copyright (c) 2003-2004 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of version 2 of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# Further, this software is distributed without any warranty that it is
# free of the rightful claim of any third person regarding infringement
# or the like.  Any license provided herein, whether implied or
# otherwise, applies only to this software file.  Patent licenses, if
# any, provided herein do not apply to combinations of this program with
# other software, or any other product whatsoever.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write the Free Software Foundation, Inc., 59
# Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
# Mountain View, CA  94043, or:
#
# http://www.sgi.com
#
# For further information regarding this notice, see:
#
# http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
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

