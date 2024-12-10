#! /usr/bin/perl

#
# Construct several gmin files by analyzing the column data
# for a radar intermediate file (one where the column data
# has been extract.
#
# The output gmin file will appear to be identical to a true gauge
# file, except that we fake the rain rate.  The new rain rate is
# derrived from the radar data via Z=300R^1.4 calculation.
#
# These output gmin files will be used to test the dataflow
# and algorithm correctness of the build_zr_table suite.
#
# By: John H. Merritt
#     Space Applications Corp.
#     Vienna, VA
#     john.h.merritt@gsfc.nasa.gov
#
#---------------------------------------------------------------------
#

sub mmddyyyy_to_julian {
	local ($mm, $dd, $yyyy) = @_;


	@mo_noleap = (0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365);
    @mo_leap   = (0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366);
	@mo = (@mo_noleap, @mo_leap);
    $leap = 0;
	if ( ($yyyy%4 == 0 && $yyyy%100 != 0) || $yyyy%400 == 0 ) { $leap = 1; }
	$index = $leap *12;
	return $dd + $mo[$index + $mm - 1];
}

while(<>) {
	chop;
	last if /^Table begins:/;
}

# log10(x) = ln(x)/ln(10)
$ln10 = log(10); # Perl uses natural log.
# k = 300, a=1.4
$k = 300;
$a = 1.4;
$dbk  = 10 * log($k)/log(10);

while(<>) {
	($gid, $gname, $date, $time, $range, $nheights, $data) = split(/[ \t\n]+/, $_, 7);
	($mm, $dd, $yyyy) = split(/\//, $date);
	if ($yyyy < 2000) { $yy = $yyyy - 1900; }
	else { $yy = $yyyy - 2000; }
	$jday = &mmddyyyy_to_julian($mm, $dd, $yyyy);
	($hh, $min) = split(/:/, $time);
	$out_dir = "GMIN_TEST";
	$gmin_out_file =  sprintf("%s%4.4d_%2d.gmin", $gname, $gid, $yy);
	if ($gaugelist{$gid} == 0) {
		$gaugelist{$gid} = 1;
		$fname = $out_dir . "/" . $gmin_out_file;
		open($gmin_out_file, "> $fname");
		# Output a dummy header.  Hey, how about the filename?
		printf($gmin_out_file "%s  -- This is synthetic data and this header is not used.\n", $fname);
	}
#	print $data,"\n";
#
# Take all the dbz values and compute an array of rainrates.  Then,
# average that up and spit it out.
	$n = 0;
	$dbz_sum = 0;
    while ($nheights-- > 0) {
		($height, $data) = split(/[ \t\n]+/, $data, 2);
		($npairs, $data) = split(/[ \t\n]+/, $data, 2);
#		print "Number of pairs is $npairs\n";
		while ($npairs-- > 0) {
			($rtype, $dbz, $data) = split(/[ \t\n]+/, $data, 3);
#			print "rtype $rtype, dbz $dbz\n";
			if ($rtype > 0) {
				if ($dbz > -90) { # really -9999.90 or -99.00
					$dbz_sum += $dbz;
					$n++;
				}
			}
		}
	}
	if ($n > 0) {
		$dbz = $dbz_sum/$n;
	} else {
		$dbz = 0;
	}
	
# Note: log10(x) = ln(x)/ln(10)

	if ($dbz <= 0) {
		$rr = 0;
	} else {
#--------
# From C
#	dbk = (float)10*log10((double)k);
#	dbr = 1./a*(dbz- dbk);
#	r   = (float)pow((double)10., (double)(dbr/10.) );
#--------
		$dbr = 1/$a * ($dbz - $dbk);
		$rr = 10**($dbr/10);
	}
	# Convert this dbz into a rainrate.  Simple Simon calculation.
	if ($rr > 0) {
		printf($gmin_out_file "%2d %3d %2d %2d 00 %8.2f\n",$yy, $jday, $hh, $min, $rr);
	}
}

