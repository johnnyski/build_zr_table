#! /usr/bin/perl
#--------------------------------------------------------------------
# Coded by: John H. Merritt
#           SM&A Corporation
#           www.smacorp.com
#           John.H.Merritt@gsfc.nasa.gov
#
#---------------------------------------------------------------------
sub usage {
	print STDERR <<"EOF";
Usage: (Package $PROG_VERSION)

    gauge_radar_accum.pl [--matrix-index n]
                         [--positive-only]
                         [--Network]
                         [--tmean x]
                         [--Rainrate]
                         [--help]
                         [radar_gauge_merge.ascii]

Note: Input file must be sorted by 'sort_zr_intermediate_ascii' to
      get the correct rain accumulations.

Where:
   --matrix-index n  - Specify the matrix index (n) to plot.  The matrix
                       contains the radar to gauge correlations, it is a 3x3
                       matrix with the center being over the gauge.
                       Usually, 0 through 7 for the 3 by 3 matrix.
                       Default = 4 (this is the center).
   --positive-only   - Accumulate only positive gauge values.  Negative
                       numbers are suspect, however, they are good for
                       rain totals.  In other words, Default: INCLUDE negative
                       gauge values.
                       Default: Use both negative and positive gauge values.
   --center-only     - Only select the rain classification from the center
                       point.
                       Default: majority rules.
                       Majority rules means if the center point is not
                       classified, then take the majority classification of
                       surrounding 8 points.  If no majority, then leave
                       it unclassified.  If reclassification occurs,
                       the raintype outputted is all UPPER CASE.
   --Network    - Accumulate for entire network.
                  Default: accumulate for each gauge.
   --tmean n    - Number of gauge measurements to average.  Default: 7.
                  If number of gauge measurements (n) is <= 7, then use n instead.
                  'n' is typically [5..15] and odd.
   --Rainrate   - Data is already in mm/hr.  Default: treat data as dBZ.
   --help       - Print this usage.

   radar_gauge_merge.ascii  - Either stdin, if not specified, as sorted by
                              sort_zr_intermediate_ascii or the name of
                              a file.  The file can be any intermediate
                              radar and gauge merged file.  Common aliases
                              are second_intermediate_file.ascii and
                              third_intermediate_file.ascii.
EOF

exit;
}

# OUTPUT:
#
#     Several columns of data suitable for plotting, but, which 
#     the fields must be cut and joined.  See the program,
#     'gauge_radar_plot_filter.pl' which not only calls this program but
#     takes this output and produces *.plot files
#
#     Columns are:
#     Gauge name, Gauge number, Range, Fractional Julian day,
#     Mean radar dBz, Radar rainrate, Gauge rainrate, Radar rain accum,
#     Gauge rain accum.
#     
#####################################################################
#
# Output gauge and radar total accumulations.  Each accumulation
# has an associated fractional julian day.  A plotting program
# will take this data to plot jday vs. total rain.
#
# This is basically the second step of the Eyal Amitai algorithm that
# computes correlation coefificients.
#
# Xij - the rain rate as driven from the reflectivity at location j (1..9)
#       for window number i (1..n) assuming a nominal fixed Z-R relationship.
#
# Xij = {(10^(Zij/10)/A}^(1/B)       Where, eg., A=300, B=1.4.
#
# Yi - the average 5-minute gauge measured rain rate centered at the time
# of the scan.
# Yi = Sum(R4+R6+R7+R8+R9)/5  (start w/ t=5 minutes, but it is flexible)
#
# Constraints:
#
# a. Windows at range of 100km or less.
# b. Windows where non of the 5 1-minute gauge rain rate values is negative.
# c. Pairs of gauge and radar data where the rain type exists.  Therefore,
#    n is the actual number of pairs that were used for the j location.
#
# 
#    If the merge data is sorted by gauge and by time, then create
#    a new merge data file which includes for each rain gauge:
#    gauge ID, range from radar, scan time, Z5 X5, Yi, Sum(X5)/6,
#    Sum(Yi)/6.  Where Sum(X5)/6, Sum(Yi)/6 are the accumulation of rain
#    as obtained from the center reflectivity pixel and from the 5-minute
#    average rain rates.  These 2 data sets should be plotted as a function
#    of time.
#
#

sub mmddyyyy_to_julian {
	local ($mm, $dd, $yyyy) = @_;


	@mo_noleap = (0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365);
    @mo_leap   = (0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366);
	@mo = (@mo_noleap, @mo_leap);
    $leap = 0;
	if ( ($yyyy%4 == 0 && $yyyy%100 != 0) || $yyyy%400 == 0 ) { $leap = 1; }
	$index = $leap *12;
	return $dd + $mo[$index + $mm + $leap - 1];
}

sub day_fraction {
	local ($time) = @_;
	local ($hh, $mm, $frac);
# $time = hh:mm
	($hh, $mm) = split(':', $time);
	$hh += $mm/60.0;
	$frac = $hh/24.0;
	return $frac;
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
	return $dbz if $opt_R;
	$ln10 = log(10); # Perl uses natural log.
# k = 300, a=1.4
	$dbk  = 10 * log($k)/log(10);

	$dbr = 1/$a * ($dbz - $dbk);
	$rr = 10**($dbr/10);
	return $rr;
}

use Getopt::Long;

######################################
#              M A I N               #
######################################
# --------------------------------------------
#    RUNTIME PARAMETERS
#

$nrain_measurements = 0; # Detected later, if 0, set to max.
$matrix_index = 4;

GetOptions("matrix-index=i" => \$matrix_index,
		   "positive-only" => \$use_positive,
		   "Network"  => \$do_net,
		   "tmean=i" => \$nrain_measurements,
		   "Rainrate" => \$opt_R,
           "center-only" => \$center_only,
           "help" => \$help) or usage();

usage() if $help;
usage() if ($matrix_index < 0 || $matrix_index > 8);

 while(<>) {
	last if /^Table begins:/;
}
$second_cappi_range = 99;

printf STDOUT "#                   Date            Radar    Radar(Rr) Gauge(G)                          Convective           Stratiform\n";
printf STDOUT "#NET gauge range yyyyjjj.fday        dBz      mm/hr    mm/hr     sum(dBz)   sum(G)    sum(Rr)     sum(G)    sum(Rr)    sum(G) Type\n";

while(<>) {
	next if /^#/;
	($gid, $gname, $date, $time, $range, $nheights, $data) = split(' ', $_, 7);
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

	$gid = "all" if $do_net;
	@dbz_sum = (0) x @dbz_sum;
	@strat_sum = (0) x @strat_sum;
	@conve_sum = (0) x @conve_sum;
	@N = (0) x @N;
	@Nstrat = (0) x @Nstrat;
	@Nconve = (0) x @Nconve;
	$gauge_index = join(' ',($gname,$gid));
	while ($nheights-- > 0) {
		($height, $data) = split(' ', $data, 2);
		($npairs, $data) = split(' ', $data, 2);
#		print "Number of pairs is $npairs\n";
		
		$j = 0;
		for ($j=0; $j < $npairs; $j++) {
			($rtype, $dbz, $data) = split(' ', $data, 3);
#		print "rtype $rtype, rr $rr\n";
            if ($range > $second_cappi_range) {
                next if $height != 3.0;
            }
            if ($range <= $second_cappi_range) {
		        next if $height > 1.5; # Only 1.5 km cappi when <= 99 km.
            }
			$NPAIRS{$gauge_index} = $npairs;

			# Zij = dbz
			# Xij = f(Zij, 300.0, 1.4), where f(x) = [(10^(Zij/10))/A]^(1/B)
            #                           for A=300.0 and B=1.4
			
			$Ntype[$j] = "-";
			# Catch unclassified $dbz, also.
			if ($dbz > -90) { # really -9999.90 or -99.00
				$dbz_sum[$j] += $dbz;
				$strat_sum[$j] += $dbz if $rtype == 1;
				$conve_sum[$j] += $dbz if $rtype == 2;
				$N[$j]++;
				$Nstrat[$j]++ if $rtype == 1;
				$Nconve[$j]++ if $rtype == 2;
				$Ntype[$j] = "Stratiform" if $rtype == 1;
				$Ntype[$j] = "Convective" if $rtype == 2;
			}
		}
	}

	# -------------------------------------------------
    #   Still not done with this gauge ($gauge_index)
    #
	# Have SUM(Zij) and n
	# $data contains the rain rates from the gauge.  Typically 11 pairs.
	#  Eg. $data = (11 r1 r2 ... r11)
	($ngpairs, $data) = split(' ', $data, 2);
	@R = split(' ', $data);
#
# Determine if we use this window.  If any R for the gauge is negative,
# then, don't use this window.
#
	if ($nrain_measurements == 0) {
		$nrain = 7;
		$nrain = $ngpairs if ($ngpairs < 7); # From file.
	} else {
		$nrain = $nrain_measurements; # From runtime parameters
	}
	$nrain_save = $nrain;
	$mid = $ngpairs / 2;
	$index1 = $mid - $nrain/2;
	$nr = 0;
	$Yi = 0;
	while($nrain-- > 0) {
#		print STDOUT "index1=$index1 R=$R[$index1]  ... ";
		next if $R[$index1] == -501.00;  # <= 501.00 is missing.
		$R[$index1] *= -1 if $R[$index1] < 0 && ! $use_positive;
		if ($R[$index1] >= 0) {
			$Yi += $R[$index1]; # Units are mm/hr.
#			print STDOUT "Yi = $Yi\n";
			$nr++;
		}
		$index1++;

	}
	$Yi /= $nrain_save if $nrain_save > 1;
#	next if $nr == 0;
#	print STDOUT "Yi/nr = $Yi\n";
	
#		print "------- ------------ N[$matrix_index] == $N[$matrix_index]\n";
#		print "------- ------ dbz_sum[$matrix_index] == $dbz_sum[$matrix_index]\n";
	if ($N[$matrix_index] > 0) {
		$dbz = $dbz_sum[$matrix_index]/$N[$matrix_index];
		$Z5 = $dbz;
		$X5 = powerlaw($dbz, 300.0, 1.4); # Strat  FACE
		# Convert X5 is in mm/hr.
	} else {
		$Z5 = 0;
		$X5 = 0;
	}
	
#	print STDOUT "GAUGE=$gauge_index\n";
#	print STDOUT "R=", join(' ',@R),"\n";

	$SUM_X5{$gauge_index} += $X5;
	$SUM_Yi{$gauge_index} += $Yi;
	
# Determine majority rules and reclassify.
	if (! $center_only) {
		if ($Ntype[$matrix_index] eq "-") {
			for($j=$nconv=$nstra=0; $j<$npairs; $j++) {      # $npairs from radar data
				$nconv += 1 if $Ntype[$j] eq "Convective";
				$nstra += 1 if $Ntype[$j] eq "Stratiform";
			}
			$Ntype[$matrix_index] = "CONVECTIVE" if ($nconv > $nstra);
			$Ntype[$matrix_index] = "STRATIFORM" if ($nconv < $nstra);
		}
	}

	$SUM_Yi_conve{$gauge_index} += $Yi if $Ntype[$matrix_index] =~ /Convective/i;
	$SUM_Yi_strat{$gauge_index} += $Yi if $Ntype[$matrix_index] =~ /Stratiform/i;
	$SUM_conve{$gauge_index} += $X5    if $Ntype[$matrix_index] =~ /Convective/i;
	$SUM_strat{$gauge_index} += $X5    if $Ntype[$matrix_index] =~ /Stratiform/i;
	
	$dfrac = day_fraction($time);
	$jday = mmddyyyy_to_julian(split('/',$date));
#	print STDOUT "dfrac $dfrac, $time, $date\n";
	$dfrac += $jday + $yyyy*1000;
	printf STDOUT "%3s %4s %7s %15.7f %8.2f %8.2f %8.2f %10.2f %10.2f %10.2f %10.2f %10.2f %10.2f %s\n",
 $gname,
 $gid,
 $range,
 $dfrac,
 $Z5,
 $X5,
 $Yi,
 $SUM_X5{$gauge_index}, $SUM_Yi{$gauge_index},
 $SUM_conve{$gauge_index}, $SUM_Yi_conve{$gauge_index},
 $SUM_strat{$gauge_index}, $SUM_Yi_strat{$gauge_index},
 $Ntype[$matrix_index];
}

