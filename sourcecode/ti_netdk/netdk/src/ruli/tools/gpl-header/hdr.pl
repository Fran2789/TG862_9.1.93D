#! /usr/bin/perl -w
#
# $Id: hdr.pl,v 1.1.1.1 2002/11/28 15:43:46 evertonm Exp $

my $gpl_begin = '-GNU-GPL-BEGIN-';
my $gpl_end   = '-GNU-GPL-END-';

my @header;

sub proc {
    my $source = shift;
    print "$source\n";

    local *IN;
    if (!open(IN, "<$source")) {
	warn "can't read file: $source: $!\n";
	return;
    }
    my @contents;
    SCAN: while (<IN>) {

      # remove GNU GPL
	if (/$gpl_begin/) {
	  END: while (<IN>) {
	      if (/$gpl_end/) {

		# remove brancos apos GNU GPL
		while (<IN>) {
		  if (/./) {
		    push(@contents, $_);
		    next SCAN;
		  }
		}
	      }
	  }
	}
	else {
	  push(@contents, $_);
	}
    }
    close(IN);

    local *OUT;
    if (!open(OUT, ">$source")) {
	warn "can't write file: $source: $!\n";
	return;
    }

    print OUT "/*$gpl_begin*\n";
    print OUT @header;
    print OUT "*$gpl_end*/\n";
    print OUT "\n";
    print OUT @contents;

    close(OUT);
}

sub process {
    my $f;
    foreach $f (@_) {
	&proc($f);
    }
}

sub explore {
    my $dir;
    foreach $dir (@_) {

	if (! -d $dir) {
	    warn "skipping: '$dir' is not a directory\n";
	    return;
	}
	
	if (! -r $dir) {
	    warn "skipping: can't read dir: $dir\n";
	    return;
	}
	
	local *DIR;
	if (!opendir(DIR, $dir)) {
	    warn "can't open dir: $dir: $!\n";
	    return;
	}
	my @entries = readdir DIR;
	closedir DIR;

	my $i;
	for ($i = 0; $i <= $#entries; ++$i) {
	    my $e = $entries[$i];
	    $entries[$i] = "$dir/$e";
	}

	my @sources = grep { /\.[c|h]$/ && -f "$_" } @entries;
	my @dirs = grep { ($_ !~ /\/\./) && -d "$_" } @entries;

	&process(@sources);
	&explore(@dirs);
    }
}

@header = <STDIN>;

&explore(@ARGV);

