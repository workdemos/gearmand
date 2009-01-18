#!/usr/bin/perl

# Gearman server and library
# Copyright (C) 2008 Brian Aker, Eric Day
# All rights reserved.
#
# Use and distribution licensed under the BSD license.  See
# the COPYING file in this directory for full text.

use strict;
use POSIX qw(strftime);

# Set some output variables.

my $path= "docs/man";
my $section= 3;
my $short= "Gearman";
my $long= "Gearman Job Queue System";
my $include= "#include <libgearman/gearman.h>";
my $website= "The Gearman homepage: http://www.gearman.org/";
my $bugs= "Bugs should be reported at https://bugs.launchpad.net/gearmand";
my $copying= "Copyright (C) 2008 Brian Aker, Eric Day. All rights reserved.\n\n" .
             "Use and distribution licensed under the BSD license. See the COPYING file in this directory for full text.";


# Don't edit below here unless you know what you're doing.

my $date= strftime("%Y-%m-%d", gmtime());

my $line;
my $proc= 0;
my $code= 0;
my $data;

print "\ndist_man_MANS=";

while ($line= <>)
{
  if ($line=~ m/^\/\*\*/)
  {
    # We have a start of a comment block.
    $proc= 1;
    $data= "";
  }
  elsif ($proc)
  {
    if ($line=~ m/^ ?\*\//)
    {
      # We've reached the end of a comment block, try to find a function.
      my $name= "";
      my $func= "";

      while ($line= <>)
      {
        if ($line=~ m/^$/)
        {
          last;
        }
        elsif ($line=~ m/#define/)
        {
          $name= "";
          last;
        }
        elsif ($name == "" && $line=~ m/([a-z_]+)\(/)
        {
          $name= $1;
        }

        chomp($line);
        $line=~ s/^ +/ /;
        $func.= $line;
      }

      if ($name)
      {
        # We have a function! Output a man page.
        print " \\\n\t$path/man.$section/$name.$section";
        $func=~ s/ ([a-z_\*]*),/ \" $1 \",/g;
        open(MANSRC, ">$path/man.$section/$name.$section");
        print MANSRC ".TH $name $section $date \"$short\" \"$long\"\n";
        print MANSRC ".SH SYNOPSIS\n";
        print MANSRC ".B $include\n";
        print MANSRC ".sp\n";
        print MANSRC ".BI \"$func\"\n";
        print MANSRC ".SH DESCRIPTION\n";
        print MANSRC "$data";
        print MANSRC ".SH \"SEE ALSO\"\n$website\n";
        print MANSRC ".SH BUGS\n$bugs\n";
        print MANSRC ".SH COPYING\n$copying\n";
        close(MANSRC);
      }

      $proc= 0;
      next;
    }

    $line=~ s/^ \* //;

    if ($code == 1)
    {
      if ($line=~ s/\@endcode/\.sp\n.nf/)
      {
        $code= 0;
      }
    }
    elsif ($line=~ s/\@code/\.sp/)
    {
      $code= 1;
    }
    else
    {
      $line=~ s/^ *//;
    }

    if ($line=~ s/\@param *([a-z_]*) */\.TP\n\.BR $1\n/ && $proc == 1)
    {
      $line= ".SH PARAMETERS\n$line";
      $proc++;
    }

    $line=~ s/\@return */\.SH \"RETURN VALUE\"\n/;

    $data.= $line;
  }
}

print "\n\n";