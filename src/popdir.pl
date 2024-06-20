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
	   "file-pct=i" => \$file_pct,
	   "incr=i" => \$incr,
	   "format=s" => \$format,
	   "dir=s" => \$dir,
	   "remove!" => \$remove,
	   "help!" => \$help,
	   "hardlink!" => \$hardlink,
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
  --file-pct        create this percentage of regular files (90 percent)
  --remove          remove instead of creating
  --format=str      printf formatting string for file name ("%08d")
  --verbose         verbose output
  --help            this help screen
  --hardlink        hardlink subsequent files to the first one created
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
$file_pct = 90 if (!defined $file_pct);
$format = "%08d" if (!defined $format);
$incr = 1 if (!defined $incr);

if ($file_pct < 0) {
	$file_pct = 0;
} elsif ($file_pct > 100) {
	$file_pct = 100;
}

if ($hardlink) {
	$file_pct = 100;
	$link_fname = sprintf($format, $start);
}

for ($i = $start; $i <= $end; $i += $incr) {
	$fname = sprintf($format, $i);

	if ($remove) {
		$verbose && print "rm $fname\n";
		unlink($fname) or rmdir($fname) or die("unlink $fname");
	} elsif ($hardlink && $i > $start) {
		# hardlink everything after the first file
		$verbose && print "ln $link_fname $fname\n";
		link $link_fname, $fname;
	} elsif (($i % 100) < $file_pct) {
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
