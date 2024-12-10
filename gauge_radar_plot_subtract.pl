#! /usr/bin/perl
#
# Subtract two plot files.  The difference is high# - low#.  
# All I'm interested is trends.
#
# Use this to determine if the radar and gauge don't change together.
#
if ($#ARGV != 1) {
	printf STDERR "Usage: %s radar.plot gauge.plot\n", $0;
	printf STDERR "\n";
	printf STDERR "  Output to stdout.\n";
	exit;
}

$in1 = $ARGV[0];
$in2 = $ARGV[1];
shift; shift;
#
# Don't worry, the files should be the same length.
#
# Each file has column 1 as fractional julian day.
#               column 2 as the rain accumulation.
#

open(IN1, $in1);
open(IN2, $in2);

while ($line1 = <IN1>) {
	$line2 = <IN2>;
	chop($line1);
	chop($line2);

	($jday, $radar) = split(' ', $line1);
	($jday, $gauge) = split(' ', $line2);

	$val = ($radar + $gauge)/2.0;
	$val = abs($radar - $gauge);

	print "$jday $val\n";
}
