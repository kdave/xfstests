#!/usr/bin/python3

# Copyright (c) 2023 Oracle.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#
# Create a bunch of xattrs in a file.

import argparse
import sys
import os

parser = argparse.ArgumentParser(description = 'Mass create xattrs in a file')
parser.add_argument(
	'--file', required = True, type = str, help = 'manipulate this file')
parser.add_argument(
	'--start', type = int, default = 0,
	help = 'create xattrs starting with this number')
parser.add_argument(
	'--incr', type = int, default = 1,
	help = 'increment attr number by this much')
parser.add_argument(
	'--end', type = int, default = 1000,
	help = 'stop at this attr number')
parser.add_argument(
	'--remove', dest = 'remove', action = 'store_true',
	help = 'remove instead of creating')
parser.add_argument(
	'--format', type = str, default = '%08d',
	help = 'printf formatting string for attr name')
parser.add_argument(
	'--verbose', dest = 'verbose', action = 'store_true',
	help = 'verbose output')

args = parser.parse_args()

fmtstring = "user.%s" % args.format

# If we are passed a regular file, open it as a proper file descriptor and
# pass that around for speed.  Otherwise, we pass the path.
fp = None
try:
	fp = open(args.file, 'r')
	fd = fp.fileno()
	os.listxattr(fd)
	if args.verbose:
		print("using fd calls")
except:
	if args.verbose:
		print("using path calls")
	fd = args.file

for i in range(args.start, args.end + 1, args.incr):
	fname = fmtstring % i

	if args.remove:
		if args.verbose:
			print("removexattr %s" % fname)
		os.removexattr(fd, fname)
	else:
		if args.verbose:
			print("setxattr %s" % fname)
		os.setxattr(fd, fname, b'abcdefgh')
