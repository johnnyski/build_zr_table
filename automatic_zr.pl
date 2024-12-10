#! /usr/bin/perl
###############################################################
# Process the 2-nd intermediate file through to the ZR table.
#
# Features:
#
#   Automatic gauge quality control. (optional)
#   Gauge filtering. (optional)
#   Generate a report of which gauges were removed. (optional)
#   Computing R/G.
#   Adjusting FACE ZR w/ R/G.
#   Generating the ZR table.
#
###############################################################
#
# Author: John H. Merritt
#         SM&A Corp.
#         NASA/Goddard Space Flight Center
#         February 10, 1999
#
# Copyright (c) 1999 GPL
###############################################################
#                                                             #
#                        u s a g e                            #
#                                                             #
###############################################################
$PROG_VERSION = "build_zr_table-v1.14";
sub usage {
	print STDERR <<"EOF";
 Usage: (Package $PROG_VERSION)

    automatic_zr.pl [--no-qc]
                    [--filter-file file]
                    [--gauges-removed]
                    [--help]
                    [2-nd_intermediate.ascii]

    --no-qc            - Implies no filtering of gauges.  Ie., the 2-nd 
                         intermediate file is the final version.
    --filter-file file - No QC step is performed, but, gauge filtering
                         still happens.  'file' contains filtering info.
                         Ie., 'file' is the output of
                         make_gauge_filter_file.pl.
    --gauges-removed   - Report gauges removed during QC to file
                         'gauges-removed.fileNNNNN', where NNNNN is
                         a 1 to 5 digit number automatically generated. 
    --help             - This usage message.
   
  Input: STDIN   - The merged radar and gauge file (2-nd intermediate file).
 Output: zr_table.zrNNNNN  - The ZR table, Gauge QC-d and adjusted.
         STDERR  - Filter file, Site, start/stop times, lat/lon and
                   the build_zr_table command.
EOF
	exit;
}
###############################################################
#                                                             #
#                         m a i n                             #
#                                                             #
###############################################################
use Getopt::Long;
use POSIX;
GetOptions("no-qc" => \$noqc,
		   "filter-file=s" => \$filter_file,
		   "gauges-removed" => \$gauges_removed,
           "help" => \$help) or usage();

usage() if $help;

$second_intermediate = $ARGV[0];
$noqc = 1 if $filter_file ne "";
$removed_file = "gauges-removed.file$$" if $gauges_removed;
$report_opt = "-report $removed_file" if $gauges_removed;
if (!$noqc) {
	$filter_file = "filter.file$$";
	system("automatic_qc.pl $report_opt $second_intermediate > $filter_file");
}
$filter_opt = "-f $filter_file" if $filter_file;
$r_over_g_line = `gauge_filter.pl $filter_opt $second_intermediate |
        gauge_radar_accum.pl | r_over_g.pl | grep 'Total'`;

print STDERR " Gauge_filter_file                 $filter_file\n";
print STDERR " Gauges_removed_file               $removed_file\n";
#unlink "$filter_file";

# $r_over_g_line contains the R/G for both convective and stratiform,
# in that order.  The default 1.4 exponent will not change.
# The new coefficient (A) is computed as:
#          +-    -+1.4
#          |Rtotal|
#       A =|------|  *  300.0
#          |Gtotal|
#          +-    -+

chomp $r_over_g_line;

@rg = split ' ', $r_over_g_line;

$convective_adjustment = pow($rg[1],1.4) * 300.0;
$stratiform_adjustment = pow($rg[2],1.4) * 300.0;

# Determine the site and date/time range for the ZR table.
#------------------  cut 2-nd intermediate header NEW ---------------
# Site_name                         MELB
# Start_date_time                   02/01/1998 00:00:00
# End_date_time                     02/28/1998 23:59:59
# Radar_lat                         28.113333
# Radar_lon                         -80.654167
#------------------  cut 2-nd intermediate header NEW ---------------
# The old format looks like
#------------------  cut 2-nd intermediate header OLD ---------------
# site_name                     MELB
# mm/dd/yyyy hh:mm:ss start     12/31/1969 18:59:59
# mm/dd/yyyy hh:mm:ss stop      12/31/1969 18:59:59
# radar_lat radar_lon           28.113333 -80.654167
#------------------  cut 2-nd intermediate header OLD ---------------

# The header ends at 'Table begins:'
#

$header = `perl -ne 'print; last if /Table begins:/' $second_intermediate`;
@h = split /\n/,$header;
# Handle both new and old format.  New format is the left side of ||.
foreach (@h) {
	$site = $_ if /^ Site_name/ || /^ Site/;
	$sdate = $_ if /^ Start_date_time/ || /^ mm\/dd\/yyyy.*start/;
	$edate = $_ if /^ End_date_time/ || /^ mm\/dd\/yyyy.*end/;
	$lat   = $_ if /^ Radar_lat/ || /^ radar_lat.*radar_lon/;
	$lon   = $_ if /^ Radar_lon/ || /^ radar_lat.*radar_lon/;
}

@site = split ' ', $site;    $site = $site[$#site];
@sdate = split ' ', $sdate;
$sdate = $sdate[$#sdate-1];
$stime = $sdate[$#sdate];

@edate = split ' ', $edate;
$edate = $edate[$#edate-1];
$etime = $edate[$#edate];

@lat = split ' ', $lat;
if ($#lat > 1) {
	$lat = $lat[$#lat-1];
} else { # New format
	$lat = $lat[$#lat];
}
@lon = split ' ', $lon;
$lon = $lon[$#lon]; # Same place for new and old format.

print STDERR " Site_name                         $site\n";
print STDERR " Start_date_time                   $sdate $stime\n";
print STDERR " End_date_time                     $edate $etime\n";
print STDERR " Radar_lat                         $lat\n";
print STDERR " Radar_lon                         $lon\n";

# NOTE:
# It is not a typo that the stratiform adjustment comes before the convective
# adjustment.  The build_zr_table progam orders it this way.
$cmd = "build_zr_table --site $site -k $stratiform_adjustment -k $convective_adjustment --sdate $sdate --stime $stime --edate $edate --etime $etime > zr_table.zr$$";

print STDERR $cmd, "\n";
system($cmd);
