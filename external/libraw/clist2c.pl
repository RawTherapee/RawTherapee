#!/usr/bin/perl

while (<>)
  {
    chomp;
    s/^\s+//;
    s/^\s+\*\s+//;
    s/^\s+//;
    s/\"/\\\"/g;
    s/\s+$//;
    print "\"$_\",\n";
  }
