#! /usr/bin/perl
#
# Usage:
#    gauge_radar_plot_batch.pl radargauge_merged.ascii
#

require "getopts.pl";

sub load_second_intermediate_file {
	local ($infile) = @_;
	local (%merged);
#
# Construct an associative array:
# Keys are NET GAUGE_NUM
# Each element is an array or records; all the NET GAUGE_NUM records
#
	open(S, "gzip --stdout --decompress --force $infile  |");

	while (<S>) {
		$merged{"Header"} .= $_;
		last if /^Table begins:/;
	}
	while (<S>) {
		next if /^#/;
		$line = $_; # Save
		split;
		$key = "$_[1] $_[0]";
		$merged{$key} .= $line;
		$merged{"net $_[1]"} .= " $_[0]" if index($merged{"net $_[1]"},$_[0]) < 0;
	}
	close(S);
	return %merged;
}

sub output_file {
	local (*xy, $filename) = @_;

	warn "Can't open $filename\n" if ! open(OUT, ">$filename");
	for ($i=0; $i<=$#xy; $i+=2) {
		print OUT "$xy[$i] $xy[$i+1]\n";
	}
	close(OUT);
}

#########################################################################
#                                                                       #
#                           load_plot_data                              #
#                                                                       #
#########################################################################
sub load_plot_data {
	local ($infile) = @_;
	local @in = () x @in;
	local @out = () x @out;

	open(F, $infile);
	@in = grep( ! /^#/, <F>);
    close(F);
	for ($i=0, $j=0; $i<=$#in; $i++) {
		$_ = $in[$i];
		chop;
		split;
		$out[$j++] = $_[0];
		$out[$j++] = $_[1];
#		print "OUT=<@out>\n";
	}
			   
   return @out;
}

#################################################################
#                                                               #
#                  massage_plot_files                           #
#                                                               #
#################################################################

sub massage_plot_files {
	@radarxy = load_plot_data("radar_total.plot");
	@gaugexy = load_plot_data("gauge_total.plot");
	@diffxy  = load_plot_data("radar-gauge.plot");
	@radar_C = load_plot_data("radar_conve.plot");
	@radar_S = load_plot_data("radar_strat.plot");
	@gauge_C = load_plot_data("gauge_conve.plot");
	@gauge_S = load_plot_data("gauge_strat.plot");

	$yyyy = int($radarxy[0]/1000.0);
	for($i=0; $i<=$#radarxy; $i+=2) {
		$radarxy[$i] -= 1000*$yyyy;
		$gaugexy[$i] -= 1000*$yyyy;
		$diffxy[$i] -= 1000*$yyyy;
		$radar_C[$i] -= 1000*$yyyy;
		$radar_S[$i] -= 1000*$yyyy;
		$gauge_C[$i] -= 1000*$yyyy;
		$gauge_S[$i] -= 1000*$yyyy;
# Convert time averaged to rainrate; divide by 60 to get mm/hr.
		$radarxy[$i+1] /= 60.0;
		$gaugexy[$i+1] /= 60.0;
		$radar_C[$i+1] /= 60.0;
		$radar_S[$i+1] /= 60.0;
		$gauge_C[$i+1] /= 60.0;
		$gauge_S[$i+1] /= 60.0;
		$diffxy[$i+1] /= 60.0;

	}

	$ngpairs = ($#radarxy+1)/2;
	$ymin = $gaugexy[1];
	$ymax = $gaugexy[$#gaugexy];
	$ymax = $radarxy[$#radarxy] if $ymax < $radarxy[$#radarxy];
	$ymax+= 10/60.0;

# All x-axis ranges are same.
	$xmin = $gaugexy[0];
	$xmax = $gaugexy[$#gaugexy-1];
	$xmax += 1 if ($xmin == $xmax);
	output_file(*radarxy, "radar_total.plot");
	output_file(*gaugexy, "gauge_total.plot");
	output_file(*diffxy,  "radar-gauge.plot");
}
#################################################################
#                                                               #
#                         usage                                 #
#                                                               #
#################################################################

sub usage {
	print STDERR <<"EOF";
Usage: $0 [-m matrix_index]
          [-n | -N]
          [-p]
          [-f filter.file] radargauge_merged.ascii

  Where:
    -m matrix_index - Correlation index (0-8). Default 4.
    -p              - Use positive gauge measurements only.
                      Default: use both negative and positive measurements.
    -N or -n        - Generate plots for entire network.
    -f filter.file  - Specify a filter file.  See gauge_filter.pl.
                      Default: No filtering.
    -R              - Treat the data as rain rates from 2A-55.  I.e., don't
                      run the numbers through the power-law calculation;
                      don't treat as dBZ but as R.
    radargauge_merged.ascii - The merged radar and gauge ascii file.
                              Also known as the second or third intermediate
                              file.
EOF

}
#################################################################
#                                                               #
#                  generate_the_plot                            #
#                                                               #
#################################################################

sub generate_the_plot {
	local($gnet, $gnum) = @_;
# Massage it a bit more.  I.e., divide the year out of the x column.
	massage_plot_files();

# Generate the Correlations.  Extract $matrix_index number.
	open(CORR,"| gauge_filter.pl -f $filter_file | gauge_qc.pl > gauge_corr.ascii$$");
	print CORR $header;
	print CORR $netgauge;
	close (CORR);

    open(CORR, "gauge_corr.ascii$$");
	<CORR>; #skip rec 1
	$corr = "";
	while(<CORR>) {
		$corr .= $_;
	}
	close(CORR);
	unlink("gauge_corr.ascii$$");

	@corr = split(' ', $corr);
	$correlation = $corr[$matrix_index];

# What is R/G?
	$r_over_g = 0.0;
	$r_over_g = $radarxy[$#gaugexy]/$gaugexy[$#gaugexy] if $gaugexy[$#gaugexy] != 0;
	$r_over_g = sprintf("%7.3f", $r_over_g);
	$r_over_g_c = 0.0;
	$r_over_g_c = $radar_C[$#gauge_C]/$gauge_C[$#gauge_C] if $gauge_C[$#gauge_C] != 0;
	$r_over_g_c = sprintf("%7.3f", $r_over_g_c);
	$r_over_g_s = 0.0;
	$r_over_g_s = $radar_S[$#gauge_S]/$gauge_S[$#gauge_S] if $gauge_S[$#gauge_S] != 0;
	$r_over_g_s = sprintf("%7.3f", $r_over_g_s);
#	print "radarxy=@radarxy\n";

	$ratio_str = "R/G=$r_over_g Rc/Gc=$r_over_g_c Rs/Gs=$r_over_g_s";


# Generate the gnuplot script.

	open(GNUPLOT, "| gnuplot");
	print GNUPLOT "set terminal postscript landscape color \"Times-Roman\" 14\n";
	print GNUPLOT sprintf("set output 'Gauge_%s_%s.ps'\n", $gnet, $gnum);
	print GNUPLOT "set title 'Rain accumulation, Gauge $gnet $gnum.  $ngpairs pairs. ($filter_opt $pos_opt $matrix_opt) Range $range km.'\n";

	print GNUPLOT "set xrange[$xmin:$xmax]\n";
	print GNUPLOT "set yrange[$ymin:$ymax]\n";
	$xpos = ($xmax - $xmin) * .10 + $xmin;
	$ypos = ($ymax-$ymin) *.95 + $ymin;
	print GNUPLOT "set key ", $xpos,", ", $ypos, "\n";

	$xpos = ($xmax - $xmin) * .20 + $xmin;
	$ypos = ($ymax-$ymin) *.95 + $ymin;
	print GNUPLOT "set label 'Correlation $correlation' at $xpos,$ypos\n" if $gnum ne "all";
	$xpos = ($xmax - $xmin) * .20 + $xmin;
	$ypos = ($ymax-$ymin) *.92 + $ymin;
    $rmax = sprintf("%7.3f", $radarxy[$#radarxy]);
	print GNUPLOT "set label '2A-53 max = $rmax' at $xpos,$ypos\n" if $opt_R;
	print GNUPLOT "set label 'Radar max = $rmax' at $xpos,$ypos\n" if ! $opt_R;

	$xpos = ($xmax - $xmin) * .20 + $xmin;
	$ypos = ($ymax-$ymin) *.89 + $ymin;
    $gmax = sprintf("%7.3f", $gaugexy[$#gaugexy]);
   	print GNUPLOT "set label 'Gauge max = $gmax' at $xpos,$ypos\n";

	$xpos = ($xmax - $xmin) * .05 + $xmin;
	$ypos = ($ymax-$ymin) *.85 + $ymin;
   	print GNUPLOT "set label 'Ratios: $ratio_str' at $xpos,$ypos\n";

	print GNUPLOT "set xlabel 'Julian Day for $yyyy'\n";
	print GNUPLOT "set ylabel 'millimeters'\n";

	print GNUPLOT "plot 'gauge_total.plot' title 'gauge' with linespoints,'radar_total.plot' title '2A-53' with linespoints,'radar-gauge.plot' title 'diff' with linespoints\n" if $opt_R;
	print GNUPLOT "plot 'gauge_total.plot' title 'gauge' with linespoints,'radar_total.plot' title 'radar' with linespoints,'radar-gauge.plot' title 'diff' with linespoints\n" if ! $opt_R;

	close (GNUPLOT);
}

############## M A I N ###############
#
$pos_opt = "";
$R_opt   = "";
$filter_file = "filter.file";
$matrix_index = 4;

Getopts("f:nNRm:");
$filter_opt = "-f $opt_f" if $opt_f;
$filter_file = "$opt_f" if $opt_f;
$pos_opt = "-p"       if $opt_p;
$R_opt   = "-R"       if $opt_R;
$do_net  = $opt_N || $opt_n;
$matrix_index = $opt_m if $opt_m;

if ($#ARGV != 0 || 0>$matrix_index || $matrix_index>8) {
	usage;
	exit;
}
$matrix_opt = "-m $matrix_index";
$second_intermediate_file = $ARGV[0];
shift;

%gauge_radar_merge = load_second_intermediate_file($second_intermediate_file);

# Generate a gnuplot script and execut it.
# The script makes plots similiar to that from gauge_gui_main.pl.
#

if ($do_net) {
	foreach (sort keys(%gauge_radar_merge)) {
		($gnet, $gnum) = split;
		next if /Header/;
		next if ! /net/;
		$gnet = $gnum;
		$gnum = "all";
	
		print STDERR "Working on $gnet $gnum\n";
		open(MKPLOT,"| gauge_radar_plot_filter.pl $R_opt $pos_opt $filter_opt $matrix_opt $gnet $gnum");
		$header = $gauge_radar_merge{"Header"};
		print MKPLOT $header;
#		print STDERR $gauge_radar_merge{"net $gnet"};
		foreach $g (split(' ',$gauge_radar_merge{"net $gnet"})) {
			$netgauge = $gauge_radar_merge{"$gnet $g"};
#			print STDERR "gnet = $gnet, g=$g\n";
			print MKPLOT $netgauge;
		}
		close MKPLOT;
		generate_the_plot($gnet, $gnum);
	}
} else {
   
	foreach (sort keys(%gauge_radar_merge)) {
		($gnet, $gnum) = split;
		next if /Header/;
		next if /net/;
		
# Generate the plot data.  This creates three files:
#    radar.plot, gauge.plot, radar-gauge.plot.
#
		print STDERR "Working on $_\n";
		open(MKPLOT,"| gauge_radar_plot_filter.pl $R_opt $pos_opt $filter_opt $matrix_opt $gnet $gnum");
		local $header = $gauge_radar_merge{"Header"};
		print MKPLOT $header;
		$key = "$gnet $gnum";
		($j, $j, $j, $j, $range, $j) = split(' ', $gauge_radar_merge{$_});
		$netgauge = $gauge_radar_merge{$key};
		print MKPLOT $netgauge;
		close (MKPLOT);
		
		generate_the_plot($gnet, $gnum);
	}
}
