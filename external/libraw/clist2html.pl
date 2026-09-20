#!/usr/bin/perl
use Data::Dumper;

@makes=( "AgfaPhoto", "Canon", "Casio", "Digital Bolex", "Epson", "Fujifilm", "Imacon",
      "Mamiya", "Minolta", "Motorola", "Kodak", "Konica", "Leica", "Hasselblad",
      "Nikon", "Nokia", "Olympus", "Pentax", "Phase One", "Ricoh",
      "Samsung", "Sigma", "Sinar", "Sony" );

MAINLOOP:
while(<>)
{
  chomp;  
  $cname = $_;
  $cname=~s/^\s+//g;
  $cname=~s/\s+$//g;
  for my $camera (@makes)
  {
     if  ($cname=~/\Q$camera\E\s+(.*)/)
     {
     
 	$model = $1;
        push @{$cameralist->{$camera}},$model;
	next MAINLOOP;
     }    
  }
  if($cname=~/(\S+)\s+(.*)/)
   { 
       ($make,$model) = ($1,$2);
        push @{$cameralist->{$make}},$model;
	next MAINLOOP;
   }
   push @{$cameralist->{$make}},"NO MODEL";
   
}
my $havenx1=0;
print "<ul>\n";
for my $make (sort keys %$cameralist)
{
   if( $#{$cameralist->{$make}} < 1) 
    {
	   print "<li>$make $cameralist->{$make}->[0]</li>\n";

     }
   else
     {
	   print "<li>$make\n<ul>\n";
	   for my $model (@{$cameralist->{$make}})
	    {
	       print "  <li>$model</li>\n";
	    }
	   print "</ul>\n</li>\n";
     }
}
print "</ul>\n";
