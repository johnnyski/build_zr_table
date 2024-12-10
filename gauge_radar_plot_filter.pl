#! /usr/bin/perl

# Generate plot files of rain accumulation for radar and gauge data.
#
# Can process dBZ data (get_radar_over_gauge) or
# rain rate data (get_2A53_data_over_gauge).
#
# By: John.H.Merritt@gsfc.nasa.gov
# Space Applications Corporation
# Vienna VA.
# Copyright (C) 1998
#
# License: GNU Public License (Version 2)

require "getopts.pl";

sub usage {
	print STDERR <<"EOF";
Usage: $0 [-m matrix_index]
          [-f gauge.filter]
          [-p]
          [-R]
          NET [NUM [any_intermediate.ascii]]

Where:
  NET              = The gauge network: STJ, SFL, etc. (Required)
  NUM              = The gauge number: all, 511, 123, etc. (Optional)
  -m matrix_index  = index in 3x3 correlation matrix. Default: 4.
  -f gauge.filter  = Gauge filter file; crontab format. Default: none.
  -p               = Use positive gauge values, only, in accumulation.
                     Default: use both negative and positive values.
  -R               = Treat the data as rain rates from 2A-55.  I.e., don't
                     run the numbers through the power-law calculation;
                     don't treat as dBZ but as R.

Generates the following files:
        radar_total.plot
        radar-gauge.plot
        radar_conve.plot
        gauge_conve.plot
        radar_strat.plot
        gauge_strat.plot

Reads from stdin or from file.
EOF
}

#---------------------------------
#
#             M A I N 
#
#---------------------------------

$second_intermediate_file = "zr_second_intermediate_MELB_12-1997.asc";
$second_intermediate_file = "second.080197-081097.ascii";
$second_intermediate_file = "s.ascii";

$matrix_index = "--matrix-index 4";
$gauge_filter = "";

Getopts('m:f:pR');
$matrix_opt   = "--matrix-index $opt_m" if $opt_m ne "";
$gauge_filter = "-f $opt_f"             if $opt_f ne "";
$pos_opt      = "--positive-only"       if $opt_p;
$rrmap_opt    = "--Rainrate"             if $opt_R;
if ($#ARGV < 0) {
	usage();
	exit;
}

$gnet = $ARGV[0]; shift;
$gnum = "all";
if ($#ARGV >= 0) {
	$gnum = $ARGV[0]; shift;
}

$all_net_opt = "";
$all_net_opt = "--Network" if $gnum eq "all";

$second_intermediate_file = "";
if ($#ARGV >= 0) {
	$second_intermediate_file = $ARGV[0]; shift;
}

$grep_string = sprintf("'%s[ ]+%s'", $gnet,$gnum);
if ($grep_string eq "") {
	usage();
	exit;
}

if ($all_net_opt eq "") {
system("sort_zr_intermediate_ascii $second_intermediate_file | \
        gauge_filter.pl $gauge_filter | \
        gauge_radar_accum.pl $all_net_opt $rrmap_opt $pos_opt $matrix_opt | \
        sort +0 -2n +3 -4n | \
        egrep $grep_string | tee pre_gauge_plot.data | \
        cut -b 18-32,61-70 > radar_total.plot");
} else { # When 'net', sort by time as the last step.
system("sort_zr_intermediate_ascii_by_time $second_intermediate_file | \
        gauge_filter.pl $gauge_filter | \
        gauge_radar_accum.pl $all_net_opt $rrmap_opt $pos_opt $matrix_opt | \
        egrep $grep_string | tee pre_gauge_plot.data | \
        cut -b 18-32,61-70 > radar_total.plot");
}
$file = "gauge_total.plot";
system("cut -b 18-32,71-81 < pre_gauge_plot.data > $file");
system("gauge_radar_plot_subtract.pl radar_total.plot $file > radar-gauge.plot");

$file = "radar_conve.plot";
system("cut -b 18-32,82-92 < pre_gauge_plot.data > $file");

$file = "gauge_conve.plot";
system("cut -b 18-32,93-103 < pre_gauge_plot.data > $file");

$file = "radar_strat.plot";
system("cut -b 18-32,104-114 < pre_gauge_plot.data > $file");

$file = "gauge_strat.plot";
system("cut -b 18-32,115-125 < pre_gauge_plot.data > $file");


