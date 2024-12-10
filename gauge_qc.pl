#! /usr/bin/perl
#--------------------------------------------------------------------
#
# USAGE:
#
#     gauge_qc.pl -h] [-n] [second_intermediate_file.ascii] 
#
# OUTPUT:
#
#     3 by 3 matrix, by gauge, of radar/gauge correlations.
#
#####################################################################
# Compute the linear correlation between the derrived rain rate
# using a simple power-law (300R^1.5) and the rain rate that
# the gauge reports.
#
# Algorithm was submitted by Eyal Amitai / eyal@trmm.gsfc.nasa.gov
#
# Definition:
#   Window - a line in the merge data file, typically referred to as
#            the second intermediate file in the build_zr_table package,
#            which includes:
#            a. 9 reflectivity (dBZ) values Zj (j=1..9), where Z5 is at the
#               window center.
#            b. 11 one minute rain rate (mm/hr) values R1,...,R11, where
#               R6 is at the time of the scan.
#
# 1. For each rain gauge location, calculate correlation coefficient (r)
#    between each of the 9 radar pixels and the average 5-minute gauge
#    measured rain rate centered at the time of the scan.  Each statistical
#    value represents one radar pixel location for the whole period, say
#    one month.  No sorting is necessary
#
#                 SUM( (Xi - Xmean) (Yi - Ymean) )
#   rj =-----------------------------------------------------
#       /----------------------,     /----------------------,
#      V SUM( (Xi - Xmean)^2 )      V SUM( (Yi - Ymean)^2 )
#
# n - total number of windows used for a given gauge.
# Xij - the rain rate as driven from the reflectivity at location j (1..9)
#       for window number i (1..n) assuming a nominal fixed Z-R relationship.
#
# Xij = {(10^(Zij/10)/A}^(1/B)       Where, eg., A=300, B=1.5.
#
# Yi - the average 5-minute gauge measured rain rate centered at the time
# of the scan.
# Yi = Sum(R4+R6+R7+R8+R9)/5  (start w/ t=5 minutes, but it is flexible)
#
# Output file: For each gauge location, a matrix of 9 correlation values
#              corresponding to the 9 reflectivity pixels.  Each matrix
#              should come w/ a header line which includes: gauge ID number,
#              range from radar, start/end date, total amount measured by
#              the gauge (Sum(Yi)/6).
#
# Constraints:
#
# a. Windows at range of 100km or less.
# b. Windows where non of the 5 1-minute gauge rain rate values is negative.
# c. Pairs of gauge and radar data where the rain type exists.  Therefore,
#    n is the actual number of pairs that were used for the j location.
#
# 
# 2. Sorting is necessary.  This algorithm is coded in gauge_radar_accum.pl.
#    Sort by gauges and time, then create
#    a new merge data file which includes for each rain gauge:
#    gauge ID, range from radar, scan tiem, Z5 X5, Yi, Sum(X5)/6,
#    Sum(Yi)/6.  Where Sum(X5)/6, Sum(Yi)/6 are the accumulation of rain
#    as obtained from the center reflectivity pixel and from the 5-minute
#    average rain rates.  These 2 data sets should be plotted as a function
#    of time.
#
#--------------------------------------------------------------------
# Coded by: John H. Merritt
#           Space Applications Corp.
#           Vienna, VA
#           john.h.merritt@gsfc.nasa.gov
#
# 5/15/98
#---------------------------------------------------------------------
#
sub sum {
	local (*x) = @_;
	local ($s) = 0;
	foreach(@x) {$s+=$_;}
	return $s;
}

sub mean {
  my $v=ref($_[0]) ? $_[0] : \@_;
  return $#{$v}==-1 ? 0 : sum($v)/(1+$#{$v});
}

sub linear_correlation {
	local (*x, *y) = @_;
	local ($meanx, $meany);

	$meanx = &mean(@x);
	$meany = &mean(@y);
	($SUM_XY_meandiff,$SUM_X_meandiff,$SUM_Y_meandiff) = (0, 0, 0);
	for ($i=0; $i<$#x; $i++) {
		$SUM_XY_meandiff += ($x[$i] - $meanx) * ($y[$i] - $meany);
		$SUM_X_meandiff += ($x[$i] - $meanx) * ($x[$i] - $meanx);
		$SUM_Y_meandiff += ($y[$i] - $meany) * ($y[$i] - $meany);
	}
	$denom = sqrt($SUM_X_meandiff) * sqrt($SUM_Y_meandiff);

	if ($denom != 0) {
		$r = $SUM_XY_meandiff / $denom;
	} else {
		$r = 0;
	}
	return $r;
}

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


sub powerlaw {
	local ($dbz, $k, $a) = @_;
#--------
# From C
#	dbk = (float)10*log10((double)k);
#	dbr = 1./a*(dbz- dbk);
#	r   = (float)pow((double)10., (double)(dbr/10.) );
#--------
# log10(x) = ln(x)/ln(10)
	$ln10 = log(10); # Perl uses natural log.
# k = 300, a=1.4
	$dbk  = 10 * log($k)/log(10);

	$dbr = 1/$a * ($dbz - $dbk);
	$rr = 10**($dbr/10);
	return $rr;
}

require 'getopts.pl';

######################################
#              M A I N               #
######################################
# --------------------------------------------
#    RUNTIME PARAMETERS
#
		$nrain_measurements = 5;

Getopts('ph');
$use_positive = $opt_p;
if ($opt_h) {
	print STDERR <<"EOF";
Usage: $0 [-p] [-h] [gauge-radar-intermediate.ascii]

Where:
   -p         - Only use positve gauge values.  Default: Use both negative
                and positive gauge values.
   -h         - Print this usage.
EOF

exit;
}


#
#---------------------------------------------

while(<>) {
	chop;
	last if /^Table begins:/;
}

$second_cappi_range = 99;

while(<>) {
	($gid, $gname, $date, $time, $range, $nheights, $data) = split(/[ \t\n]+/, $_, 7);
	($mm, $dd, $yyyy) = split(/\//, $date);
	if ($yyyy < 2000) { $yy = $yyyy - 1900; }
	else { $yy = $yyyy - 2000; }
	$jday = &mmddyyyy_to_julian($mm, $dd, $yyyy);
	($hh, $min) = split(/:/, $time);
	# For each rain gauge location, calculate correlation coefficient (r)
    # between each of the 9 radar pixels and the average 5-minute gauge
    # measured rain rate centered at the time of the scan.  Each  statistic
    # value represents one radar pixel location for the whole period 
    # (say, 1 month).

	@dbz_sum = (0) x @dbz_sum;
	@N = (0) x @N;
	$gauge_index = $gname . $gid;
	next if $gauge_index eq "";
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
			$NPAIRS{$gauge_index} = $npairs;

			# Zij = dbz
			# Xij = f(Zij, 300.0, 1.5), where f(x) = [(10^(Zij/10))/A]^(1/B)
            #                           for A=300.0 and B=1.5
			
			if ($rtype > 0) {
				if ($dbz > -90) { # really -9999.90 or -99.00
					$dbz_sum[$j] += $dbz;
#					print "dbz_sum[$j] = $dbz_sum[$j]\n";
					$N[$j]++;
				}
				
			}
		}
	}

	# -------------------------------------------------
    #   Still not done with this gauge ($gauge_index)
    #
	$GAUGE_LIST{$gauge_index} = $gauge_index;
	$GAUGE_RANGE{$gauge_index} = $range;
#	print STDOUT "npairs = $npairs\n";

	# Have SUM(Zij) and n
	# $data contains the rain rates from the gauge.  Typically 11 pairs.
	#  Eg. $data = (11 r1 r2 ... r11)
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
	$Yi = 0;
	while($nrain-- > 0) {
#		print STDOUT "index1=$index1 R=$R[$index1]  ... ";
		$R[$index1] *= -1 if $R[$index1] < 0 && ! $use_positive;
		if ($R[$index1] > 0) {
			$Yi += $R[$index1];
#			print STDOUT "Yi = $Yi\n";
			$nr++;
		}
		$index1++;
	}
	$Yi /= $nrain_measurements if $nrain_measurements > 1;
# If there are no valid gauge measurements, ie. greater than 0, then
# go on to the next record.
#	next if $nr == 0;
	
#	print STDOUT "GAUGE=$gauge_index\n";
#	print STDOUT "R=", join(' ',@R),"\n";

	for($j=0; $j<$npairs; $j++) {
#
# One more check.  We want pairs of gauge and radar data where the rain
# type value is not missing nor bad. Therefore, n ($N) is the actual
# number of pairs that were used for j.
#		next if $N[$j] <= 0;
#
# Ok, we have a good window.  Count it.  Statistics come later.
#
#  float k[2] = {238, 68}, a[2] = {1.6, 1.68}; /* Simple ZR: strat, conv*/
		if ($N[$j] != 0) {
			$dbz = $dbz_sum[$j]/$N[$j];
			$Xij = powerlaw($dbz, 300, 1.4);
		} else {
			$dbz = 0;
		    $Xij = 0;
		}
		$N_WINDOWS[$j]{$gauge_index}++;
		$n = $N_WINDOWS[$j]{$gauge_index};
		$V_X[$n][$j]{$gauge_index} = $Xij;
#		print "Xij = $Xij, V_X[$n][$j]{$gauge_index} = $V_X[$n][$j]{$gauge_index}\n";
		$V_Y[$n][$j]{$gauge_index} = $Yi;

	}
#	print "Yi = $Yi\n";

}
# Each record represents a j.
# Each column represents an i.
# The number of columns on each record varies.
foreach $g (sort keys(%GAUGE_LIST)) {

	for ($j=0; $j<$NPAIRS{$g}; $j++) {

		@X = () x @X;
		@Y = () x @Y;
#		print STDERR "N_WINDOWS[$j]{$g} = $N_WINDOWS[$j]{$g}\n";
		for ($i=0; $i<=$N_WINDOWS[$j]{$g}; $i++) {

			@X = ($V_X[$i][$j]{$g}, @X);
			@Y = ($V_Y[$i][$j]{$g}, @Y);

		}
		
#		print "X=", join(' ', @X),"\n";
#		print "Y=", join(' ', @Y),"\n";
		$r[$j] = sprintf("%10.5f(%d)", linear_correlation(*X, *Y), $#X);
	}

	printf STDOUT "Gauge %s (pairs).\n", $g;
	printf STDOUT "%s %s %s\n", @r[0..2];
	printf STDOUT "%s %s %s\n", @r[3..5];
	printf STDOUT "%s %s %s\n", @r[6..8];
	printf STDOUT "\n";

}
