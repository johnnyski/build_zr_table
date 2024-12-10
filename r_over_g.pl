#! /usr/bin/perl
#
$PROG_VERSION = "build_zr_table-v1.14";
sub usage {
	print STDERR <<"EOF";
 Usage: (Package $PROG_VERSION)

     gauge_radar_accum.pl  [second_intermediate_file.ascii] | r_over_g.pl [--total-only]

 Take the output of gauge_radar_accum.pl and generate a summary
 table that shows the R/G for each gauge, the net and the entire
 file for each rain type: stratiform, convective, etc.

 Options:

    --total-only        - Generate R and G sums, don't perform R/G

 The input does not need to be sorted.

 Example usage:

 gauge_radar_accum.pl radar_merged.ascii | r_over_g.pl
EOF

	exit;
}
#
# Example output:
#
#       R/G for:
#
#          Convective    Stratiform 
#   Gauge      r/g          r/g     
#-----------------------------------
#   SFL 54     .84          1.3     
#   SFL 143    .74          .91     
#   STJ 11     1.1          1.2     
#   SFL        .80          1.1     
#   STJ        1.1          1.2     
#   Total      .93          1.1     
#
# The 'Total' printed is the total for the entire input file.  Therefore,
# you can generate r/g for an entire site.
#
#---------------------------------------------------------------------
#
use Getopt::Long;

GetOptions("total-only" => \$totalonly,
           "help" => \$help) or usage();

usage() if $help;

while(<>) {
	next if /^#/;
    ($gnet, $gid, $range, $time, $dbz, $radar_rr, $gauge_rr, $SUM_dbz, $SUM_g,
	 $SUM_radar_conve, $SUM_gauge_conve, $SUM_radar_strat, $SUM_gauge_strat,
	 $Ntype) = split;

    chomp $Ntype;
    $Ntype =~ tr/[a-z]/[A-Z]/;
	next if $Ntype eq "-";
#	printf STDOUT "%s %s %s\n", $gnet, $gid, $Ntype;
	
# Record this type for the table output.
	$rain_types{$Ntype} = 1;
	$s = sprintf("%-4s%5s",$gnet, $gid);  # This is needed for proper sorting later.
	$gauge_list{$s} = 1;
	$net_list{$gnet} = 1;
	
# Record each gauge r and g.  Redoing the sum prevents the need for a sort.
	$Rtot{"$s $Ntype"}    += $radar_rr;
	$Gtot{"$s $Ntype"}    += $gauge_rr;
	$Rtot{"$gnet $Ntype"} += $radar_rr;
	$Gtot{"$gnet $Ntype"} += $gauge_rr;
	$Rtot{"$Ntype"}       += $radar_rr;
	$Gtot{"$Ntype"}       += $gauge_rr;

}

# Ok, output the table.

print STDOUT "   Gauge    ";
foreach(sort keys(%rain_types)) {
	printf STDOUT "%20s", $_;
}
print STDOUT "\n";
print STDOUT "net    id   ";
foreach(sort keys(%rain_types)) {
	if ($totalonly) {
		print STDOUT "   Total R   Total G";
	} else {
		print STDOUT "              r/g   ";
	}
}
print STDOUT "\n";

print STDOUT "------------";
foreach(sort keys(%rain_types)) {
	print STDOUT "--------------------";
}
print STDOUT "\n";

# Ok, done with the title bars, now output the numbers.

# The net and gauge r/g.
foreach $g (sort keys(%gauge_list)) {
	($gnet, $gid) = split ' ',$g;
	printf STDOUT "%-4s%5s   ", $gnet, $gid;
	foreach $type (sort keys(%rain_types)) {
		$val = 0;
		$val = $Rtot{"$g $type"}/$Gtot{"$g $type"} if $Gtot{"$g $type"} != 0;
		if ($totalonly) {
			printf STDOUT "%10.2f%10.2f", $Rtot{"$g $type"}, $Gtot{"$g $type"};
		} else {
			printf STDOUT "%20.4f", $val;
		}
	}
	print STDOUT "\n";
}

# The net r/g.
foreach $g (sort keys(%net_list)) {
	printf STDOUT "%-4s        ", $g;
	foreach $type (sort keys(%rain_types)) {
		$val = 0;
		$val = $Rtot{"$g $type"}/$Gtot{"$g $type"} if $Gtot{"$g $type"} != 0;
		if ($totalonly) {
			printf STDOUT "%10.2f%10.2f", $Rtot{"$g $type"}, $Gtot{"$g $type"};
		} else {
			printf STDOUT "%20.4f", $val;
		}
	}
	print STDOUT "\n";
}

# The total r/g.
	printf STDOUT "Total       ", $gnet, $gid;
	foreach $type (sort keys(%rain_types)) {
		$val = 0;
		$val = $Rtot{"$type"}/$Gtot{"$type"} if $Gtot{"$type"} != 0;
		if ($totalonly) {
			printf STDOUT "%10.2f%10.2f", $Rtot{"$type"}, $Gtot{"$type"};
		} else {
			printf STDOUT "%20.4f", $val;
		}
	}
	print STDOUT "\n";

