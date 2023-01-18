#!/usr/bin/perl -w

# Copyright (c) 2023 Oracle.  All rights reserved.
# SPDX-License-Identifier: GPL-2.0
#
# Create a bunch of files and subdirs in a directory.

use Getopt::Long;
use File::Basename;

$progname=$0;
GetOptions("start=i" => \$start,
	   "end=i" => \$end,
	   "file-mult=i" => \$file_mult,
	   "incr=i" => \$incr,
	   "format=s" => \$format,
	   "dir=s" => \$dir,
	   "remove!" => \$remove,
	   "help!" => \$help,
	   "verbose!" => \$verbose);


# check/remove output directory, get filesystem info
if (defined $help) {
  # newline at end of die message suppresses line number
  print STDERR <<"EOF";
Usage: $progname [options]
Options:
  --dir             chdir here before starting
  --start=num       create names starting with this number (0)
  --incr=num        increment file number by this much (1)
  --end=num         stop at this file number (100)
  --file-mult       create a regular file when file number is a multiple
                    of this quantity (20)
  --remove          remove instead of creating
  --format=str      printf formatting string for file name ("%08d")
  --verbose         verbose output
  --help            this help screen
EOF
  exit(1) unless defined $help;
  # otherwise...
  exit(0);
}

if (defined $dir) {
	chdir($dir) or die("chdir $dir");
}
$start = 0 if (!defined $start);
$end = 100 if (!defined $end);
$file_mult = 20 if (!defined $file_mult);
$format = "%08d" if (!defined $format);
$incr = 1 if (!defined $incr);

for ($i = $start; $i <= $end; $i += $incr) {
	$fname = sprintf($format, $i);

	if ($remove) {
		$verbose && print "rm $fname\n";
		unlink($fname) or rmdir($fname) or die("unlink $fname");
	} elsif ($file_mult == 0 or ($i % $file_mult) == 0) {
		# create a file
		$verbose && print "touch $fname\n";
		open(DONTCARE, ">$fname") or die("touch $fname");
		close(DONTCARE);
	} else {
		# create a subdir
		$verbose && print "mkdir $fname\n";
		mkdir($fname, 0755) or die("mkdir $fname");
	}
}

exit(0);
