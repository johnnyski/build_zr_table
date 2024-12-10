#! /usr/bin/perl
#--------------------------------------------------------------------
#
# Filter the second intermediate file and print only those
# entries where there is both radar and gauge measurements.
#
# This is essentially what 'gauge_qc.pl' and 'gauge_radar_accum.pl'
# do in their first step.
#
# Run this program first to reduce the size of the input second 
# intermediate file.   This is useful for quicker feedback when
# using the GUI 'gui_main.pl'.
#
# USAGE:
#
#     gauge_radar_nonzero.pl [second_intermediate_file.ascii] 
#
# OUTPUT:
#
# Looks identical to the second intermediate file.
# Constraints:
#
# a. Windows at range of 100km or less.
# b. Windows where non of the 5 1-minute gauge rain rate values is negative.
# c. Pairs of gauge and radar data where the rain type exists.  Therefore,
#    n is the actual number of pairs that were used for the j location.
#
#--------------------------------------------------------------------
# Coded by: John H. Merritt
#           Space Applications Corp.
#           Vienna, VA
#           john.h.merritt@gsfc.nasa.gov
#
# 5/26/1998
#---------------------------------------------------------------------
#
# --------------------------------------------
#    RUNTIME PARAMETERS
#
		$nrain_measurements = 5;

#
#---------------------------------------------

while(<>) {
	print;
	last if /^Table begins:/;
}
$second_cappi_range = 99;

while(<>) {
	$save_line = $_;
	($gid, $gname, $date, $time, $range, $nheights, $data) = split(/[ \t\n]+/, $_, 7);

    while ($nheights-- > 0) {
		($height, $data) = split(/[ \t\n]+/, $data, 2);
		($npairs, $data) = split(/[ \t\n]+/, $data, 2);
#		print "Number of pairs is $npairs\n";
		
		$j = 0;
		for ($j=0; $j < $npairs; $j++) {
			($rtype, $dbz, $data) = split(/[ \t\n]+/, $data, 3);
#			print "rtype $rtype, dbz $dbz, data $data";
            if ($range > $second_cappi_range) {
                next if $height != 3.0;
            }
            if ($range <= $second_cappi_range) {
		        next if $height > 1.5; # Only 1.5 km cappi when <= 99 km.
            }

			if ($rtype > 0) {
				if ($dbz > -90) { # really -9999.90 or -99.00
					$N[$j]++;
				}
				
			}
		}
	}

# $N[$j] contains number of valid radar measurements.
# Now, check the Gauge data

	($ngpairs, $data) = split(/[ \t\n]+/, $data, 2);
	@R = split(/[ \t\n]+/, $data);
#
# Determine if we use this window.  If any R for the gauge is negative,
# then, don't use this window.
#
	$nrain = $nrain_measurements; # From runtime parameters
	$mid = $ngpairs / 2;
	$index1 = $mid - $nrain/2;
	$nr = 0;
	while($nrain-- > 0) {
		if ($R[$index1] > 0) {
			$nr++;
		}
		$index1++;
	}

# If there are no valid gauge measurements, ie. greater than 0, then
# go on to the next record.
	next if $nr < $nrain_measurements;
	
# Ok, output the record.  It's a keeper.

	print $save_line;
}
