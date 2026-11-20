#!/usr/bin/env perl
# SPDX-License-Identifier: GPL-2.0
# Preserve archive order and source-counter order for ThinLTO initcalls.
# Based on Sami Tolvanen's Android initcall ordering design.
use strict;
use warnings;
my $nm = $ENV{NM} || 'llvm-nm';
my $ar = $ENV{AR} || 'llvm-ar';
my %sections;
my @levels;
sub scan {
    my ($object) = @_;
    die "missing object $object\n" unless -f $object;
    open my $fh, '-|', $nm, '--just-symbol-name', '--defined-only', $object
        or die "cannot execute $nm: $!\n";
    my %calls;
    while (<$fh>) {
        chomp;
        next unless /^__initcall_(\d+)_(\d+)_(.*)(early|rootfs|con|security|[0-7]s?)$/;
        my ($counter, $line, $fn, $level) = ($1, $2, $3, $4);
        die "duplicate initcall counter in $object\n" if exists $calls{$counter};
        $calls{$counter} = [$level, "${counter}_${line}_${fn}"];
    }
    close $fh or die "nm failed for $object\n";
    for my $counter (sort {$a <=> $b} keys %calls) {
        my ($level, $name) = @{$calls{$counter}};
        push @levels, $level unless exists $sections{$level};
        push @{$sections{$level}}, $name;
    }
}
for my $object (@ARGV) {
    if ($object =~ /\.(?:a|o)$/) {
        open my $magic, '<', $object or die "cannot open $object: $!\n";
        read $magic, my $header, 8;
        close $magic;
        if ($header eq "!<arch>\n" || $header eq "!<thin>\n") {
            open my $fh, '-|', $ar, 't', $object or die "cannot execute $ar: $!\n";
            my @members = <$fh>;
            close $fh or die "ar failed for $object\n";
            chomp @members;
            scan($_) for @members;
            next;
        }
    }
    scan($object);
}
print "SECTIONS {\n";
for my $level (@levels) {
    my $section = $level eq 'con' ? '.con_initcall.init' :
        $level eq 'security' ? '.security_initcall.init' : ".initcall${level}.init";
    print "  $section : {\n";
    print "    KEEP(*($section..$_))\n" for @{$sections{$level}};
    print "  }\n";
}
print "}\n";
