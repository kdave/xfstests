#!/usr/bin/perl -w
use strict;

use Getopt::Long;

sub help();
sub get_qa_header($);
sub get_qa_tests();

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

my @qatests = map {sprintf("%03d", $_)} @ARGV;
@qatests = get_qa_tests() unless (@qatests);

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

sub get_qa_tests() {
    my $d = shift || $ENV{'PWD'};

    opendir(my $DIR, $d) || die "can't opendir $d: $!";
    my @qa = grep {m/^\d\d\d$/ && -f "$d/$_" } readdir($DIR);
    closedir($DIR);

    return @qa;
}

