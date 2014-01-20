#!/usr/bin/perl -w
#
# Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it would be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write the Free Software Foundation,
# Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

# Print headers of given tests
# Accepted parameter types:
# - nothing - list all tests from all subdirectories in tests/*
# - tests/DIR - list all tests from DIR
# - tests/DIR/123 - show header from single test

use strict;

use Getopt::Long;

sub help();
sub get_qa_header($);
sub get_qa_tests;

my %opt;

my @oa = (
    ['--help|?',     "Show this help message.",
     \$opt{'help'}],
    ['--head|h',     "Shows only the head of the QA test",
    \$opt{'head'}],
    ['--body|b',     "Shows only the body of the QA test.",
    \$opt{'body'}],
    ['--one-line|1', "Output everything on a single line.",
    \$opt{'oneline'}],
    );

# black magic
GetOptions(map { @{$_}[0] => @{$_}[2] } @oa);

if ($opt{'help'}) {
    die help();
}

my @qatests;

if (!@ARGV) {
    my $d="tests";
    opendir(DIR, $d);
    map { push @qatests,get_qa_tests("$d/$_") if -d "$d/$_" } readdir(DIR);
    closedir(DIR);
}

foreach (@ARGV) {
    push @qatests,$_ if -f && /\d{3}$/;
    push @qatests,get_qa_tests($_) if -d;
}

foreach (@qatests) {
    my @h = get_qa_header($_);

    if ($opt{'head'}) {
	@h = shift @h;
    } elsif ($opt{'body'}) {
	shift @h;
	shift @h
    }

    if ($opt{'oneline'}) {
	print map {s/\n/ /; $_} @h;
	print "\n";
    } else {
	print @h;
    }

    print "--------------------------------------------------\n" unless (@qatests < 2);
}

sub help() {
    my $sa = '';
    foreach (@oa) {
	#	local $_ = @{$_}[0];
	@{$_}[0] =~ s/=(.*)$//;
	@{$_}[0] =~ s/\|/ \| -/;
	@{$_}[0] =~ s/^/\[ /;
	@{$_}[0] =~ s/$/ \] /;
	$sa .= @{$_}[0];
    }

    print "Usage: $0\t$sa\n";
    foreach (@oa) {
	$$_[0] =~ s/\|/\t\|/;
	print "\t$$_[0]\t$$_[1]\n";
    }
}

sub get_qa_header($) {
    my $f = shift || die "need an argument";
    my @l;

    open(my $FH, $f) || die "couldn't open '$f': $!";
    while (<$FH>) {
	#ignore.
	m/^#\!/    		and next; #shebang
	m/^#\s*\-{10}/		and last; #dashed lines
	m/^#\s*copyright/i 	and last; #copyright lines

	s/^# *//;

	push @l, $_;
    }
    close($FH);
    return @l;
}

sub get_qa_tests {
    my $d = shift || $ENV{'PWD'};

    opendir(my $DIR, $d) || die "can't opendir $d: $!";
    my @qa = sort grep { m/^\d\d\d$/ && -f "$d/$_" } readdir($DIR);
    closedir($DIR);
    return map { $_ = "$d/$_" } @qa;
}
