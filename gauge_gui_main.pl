#! /usr/bin/perl
#
# Demo all types of buttons
#
# Use the Xforms package!  Man, this perl interface is great!
#

use X11::Xforms;

unshift (@INC, ".", "/usr/local/trmm/GVBOX/bin");
require 'gauge_gui.pl';

sub FL_WATCH_CURSOR {150;}

sub load_second_intermediate_file {
	local ($infile) = @_;
	local (%merged);
#
# Construct an associative array:
# Keys are NET GAUGE_NUM
# Each element is an array or records; all the NET GAUGE_NUM records
#
	open(S, "gzip --stdout --decompress --force $infile |");

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

#########################################################################
#                                                                       #
#                           draw_data                                   #
#                                                                       #
#########################################################################
sub draw_data {
	local ($obj, $arg) = @_;

	set_all_forms_busy();
# These files are produced by 'gauge_radar_plot_filter.pl'
	@radarxy = load_plot_data("radar_total.plot");
	@gaugexy = load_plot_data("gauge_total.plot");
	@radar_C = load_plot_data("radar_conve.plot");
	@radar_S = load_plot_data("radar_strat.plot");
	@gauge_C = load_plot_data("gauge_conve.plot");
	@gauge_S = load_plot_data("gauge_strat.plot");

	if ($#gaugexy < 0 || $#radarxy < 0) {
		fl_show_message("No data to plot.","","");
		set_all_forms_unbusy();
		return;
	}

	$yyyy = int($radarxy[0]/1000.0);
	for($i=0; $i<=$#radarxy; $i+=2) {
		$radarxy[$i] -= 1000*$yyyy;
		$gaugexy[$i] -= 1000*$yyyy;
		$radar_C[$i] -= 1000*$yyyy;
		$radar_S[$i] -= 1000*$yyyy;
		$gauge_C[$i] -= 1000*$yyyy;
		$gauge_S[$i] -= 1000*$yyyy;
# Convert time averaged to rainrate(mm/hr) to accumulaion; divide by 60min/hr.
		$radarxy[$i+1] /= 60.0;
		$gaugexy[$i+1] /= 60.0;
		$radar_C[$i+1] /= 60.0;
		$radar_S[$i+1] /= 60.0;
		$gauge_C[$i+1] /= 60.0;
		$gauge_S[$i+1] /= 60.0;
	}

	$ymin = $gaugexy[1];
	$ymax = $gaugexy[$#gaugexy];
	$ymax = $radarxy[$#radarxy] if $ymax < $radarxy[$#radarxy];
	$ymax+= 10/60.0;

	$xmin = fl_get_input($xmin_input);
	$xmax = fl_get_input($xmax_input);
	if ("$obj" eq "Reset_x-axis") {
		$xmin = $xmax = "";
	}
	if ($xmin eq "") {
		$xmin = $gaugexy[0];
	}

	if ($xmax eq "") {
		$xmax = $gaugexy[$#gaugexy-1];
	}
	$xmax += 1 if ($xmin == $xmax);

	fl_set_input($xmin_input, $xmin=sprintf("%7.3f",$xmin));
	fl_set_input($xmax_input, $xmax=sprintf("%7.3f",$xmax));

	fl_set_xyplot_xbounds($plot, $xmin, $xmax);
	fl_set_xyplot_ybounds($plot, 0, $ymax);

	@color = (0, FL_BLUE, FL_BLACK, FL_RED, FL_GREEN, FL_CYAN, FL_MAGENTA);
	@title = ("None","FACE C&S",  "Gauge C&S", "FACE C", "Gauge C",
			  "FACE S", "Gauge S") if $do_dbz;
	@title = ("None","2A-53 C&S",  "Gauge C&S", "2A-53 C", "Gauge C",
			  "2A-53 S", "Gauge S") if ! $do_dbz;
	@drawit = (0, $do_total, $do_total, $do_convective,  $do_convective,
			   $do_stratiform, $do_stratiform);
	@the_data{$title[1]} =  join(' ',@radarxy);
	@the_data{$title[2]} =  join(' ',@gaugexy);
	@the_data{$title[3]} =  join(' ',@radar_C);
	@the_data{$title[4]} =  join(' ',@gauge_C);
	@the_data{$title[5]} =  join(' ',@radar_S);
	@the_data{$title[6]} =  join(' ',@gauge_S);

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

	fl_clear_xyplot($plot);

# Ok, now plot every thing on one graph.
	local $ngpairs = ($#gaugexy+1)/2;
	$ylabel = "millimeters";
	fl_set_xyplot_data($plot, (0,0), "Rain accumulation, Gauge $gnet $gnum. $ngpairs pairs.", "Julian Day for $yyyy", "$ylabel");
	fl_set_object_label($r_over_g_text, "R/G=$r_over_g Rc/Gc=$r_over_g_c Rs/Gs=$r_over_g_s");

# The plotting.
	for ($i=1; $i<=$#color; $i++) {
		@a = split(' ', @the_data{$title[$i]});
#		print "DATA=@a\n";
		fl_add_xyplot_overlay($plot, $i, @a, $color[$i]) if $drawit[$i];
	}
		
#	fl_set_xyplot_fontsize($plot, 14);
	$xpos = ($xmax - $xmin) * .10 + $xmin;

# Output the legend, match colors.
	for ($i=1; $i<=$#color; $i++) {
		$ypos = ($ymax-$ymin) *(1.0-$i*.05) + $ymin;
		fl_add_xyplot_text($plot, $xpos, $ypos, $title[$i], $FL_ALIGN_LEFT, $color[$i]) if $drawit[$i];
	}

	set_all_forms_unbusy();

}

#########################################################################
#                                                                       #
#                           do_plot                                     #
#                                                                       #
#########################################################################
sub do_plot {
	local ($obj, $arg) = @_;

	set_all_forms_busy();
# Now call a bunch of commands to create 'radar.plot', 'gauge.plot' and
# 'radar-gauge.plot'.
#
	if ($matrix_index < 0 || $matrix_index > 8) {
		fl_show_alert("Invalid Correlation Matrix index", "Matrix index must be 0-8", "");
		return;
	}

	if ($filter_file eq "") {
		fl_show_alert("Invalid Gauge filter file", "Specify a valid filename.", "");
		return;
	}		

	if (! -r $filter_file) {
		fl_show_alert("File not found: <$filter_file>", "Specify a valid Gauge filter filename.", "");
		return;
	}		

	fl_set_input($xmin_input,"");
	fl_set_input($xmax_input,"");
	$pos_opt = "";
	$R_opt   = "";
	$pos_opt = "-p" if $pos_gauge_values;
	$R_opt   = "-R" if ! $do_dbz;
	if ($gnet eq "net") {
		$gnet = $gnum;
		$gnum = "all";
	}
	if ("$gnet" eq "") {
		fl_show_message("No network supplied.  Select Action->Show Gauges.","","");
		set_all_forms_unbusy();
		return;
	}


	# Extract the data from %gauge_radar_merge and send
	# it create the plot.  This is a much reduced dataset and
	# makes the plotting very very responsive.
	open(MKPLOT,"| gauge_radar_plot_filter.pl $R_opt $pos_opt -f $filter_file -m $matrix_index $gnet $gnum");
	local $header = $gauge_radar_merge{"Header"};
	local $key = "$gnet $gnum";
	local $netgauge = $gauge_radar_merge{$key};
	print MKPLOT $header;
	if ($gnum eq "all") {
#		print STDERR $gauge_radar_merge{"net $gnet"},"\n";
		foreach $g (split(' ',$gauge_radar_merge{"net $gnet"})) {
			$netgauge = $gauge_radar_merge{"$gnet $g"};
#			print STDERR "gnet = $gnet, g=$g\n";
			print MKPLOT $netgauge;
		}
	} else {
		print MKPLOT $netgauge;
	}

	close (MKPLOT);
	draw_data();

	do_show_data() if ($data_form->visible);
	set_all_forms_unbusy();
}

#########################################################################
#                                                                       #
#                           do_no_net                                   #
#                                                                       #
#########################################################################

#########################################################################
#                                                                       #
#                           do_save_to_file_button                      #
#                                                                       #
#########################################################################
sub do_save_to_file_button {
	local ($obj, $arg) = @_;

	# Save the $data_form to file.
	$filename = fl_show_fselector("Enter filename", "", "", "");
	system("mv pre_gauge_plot.data $filename") if $filename;
	return;
}
	
#########################################################################
#                                                                       #
#                           do_show_data                                #
#                                                                       #
#########################################################################
sub do_show_data {
	local ($obj, $arg) = @_;
	local $s;
# Assume the values are pre-set: @radarxy, @gaugexy
	fl_show_form($data_form, FL_PLACE_CENTER, FL_FULLBORDER, "Data");
	fl_freeze_form($data_form);
	fl_set_browser_fontstyle($data_browser, FL_FIXEDBOLD_STYLE);
	fl_set_browser_fontsize($data_browser, 10);
	fl_clear_browser($data_browser);
	if (! open(F, "pre_gauge_plot.data")) {
		fl_show_alert("File not found: <pre_gauge_plot.data>", "", "");
		fl_hide_form($data_form);
		return;
	}
		
	while(<F>) {
		chop;
		fl_add_browser_line($data_browser, $_);
	}
	close(F);

	fl_unfreeze_form($data_form);
}
	
#########################################################################
#                                                                       #
#                           do_second_intermediate_browse               #
#                                                                       #
#########################################################################
sub do_second_intermediate_browse {
	local ($obj, $arg) = @_;
	local ($filename);
	$filename = $second_intermediate_text->label;
	local @parts = split('/', $filename);
	if ($filename eq "") {
		$path = ".";
		$defaultfile = "s.ascii";
	} else {
		$path = join('/', @parts[0..$#parts-1]);
		$defaultfile = $parts[$#parts];
	}
	$filename = fl_show_fselector("Enter second intermediate filename",
								  $path, "*.asc*", $defaultfile);
	if ($filename) {
		$second_intermediate_file = $filename;
		fl_hide_object($second_intermediate_text);
		fl_set_object_label($second_intermediate_text, $filename);
		fl_show_object($second_intermediate_text);
		save_config();
		do_show_gauges();
	}
}

#########################################################################
#                                                                       #
#                           do_browse_filter_file                       #
#                                                                       #
#########################################################################
sub do_browse_filter_file {
	local ($obj, $arg) = @_;
	local ($filename);
	$filename = $filter_file_text->label;
	local @parts = split('/', $filename);
	if ($filename eq "") {
		$path = ".";
		$defaultfile = "filter.file";
	} else {
		$path = join('/', @parts[0..$#parts-1]);
		$defaultfile = $parts[$#parts];
	}

	$filename = fl_show_fselector("Enter Gauge filter filename",
								  $path, "*filter*", $defaultfile);
	if ($filename) {
		$filter_file = $filename;
		fl_hide_object($filter_file_text);
		fl_set_object_label($filter_file_text, $filename);
		fl_show_object($filter_file_text);
		save_config();
	}
}

sub do_help {
	local ($obj, $arg) = @_;

	$help = <<"EOF";
PROG_VERSION = "build_zr_table-v1.14";
Help file for the Gauge QC graphical user interface.

PURPOSE
-------

   1. Display Gauge/Radar correlations.

   2. Plot Gauge and Radar rain accumulations as a function of time.

   3. Filter specific gauges for specific time ranges from the data.

INPUT
-----

   The input file to this program is the SECOND INTERMEDIATE file, currently,
   in ascii format.  This file is produced via 'merge_radarNgauge_data'.
   It is expected that 'merge_radarNgauge_data' has already been run and
   the second intermediate file is ready.  Convention, and for easy gui
   recognition, dictates that '*.ascii' be the second intermediate file
   template.

   Input may be compressed with gzip.

   Eg.  second.080197-081097.ascii

USING the GUI
-------------

   Brief
   -----

   All funtions originate from the 'Show gauges' menu selection under
   the 'Action' pulldown menu.  This menu item 
   could be called 'press-me-first'.  Then, you select a gauge by
   clicking on it.  A plot of rain accumulations will appear as
   well as the correlation window.  You can select to plot both
   convective or stratiform rain events, as well as, a combined total,
   or any combination.

   Change the x axis range to examine greater detail.  Specify a new
   'Corr. Matrix index' (range 0 - 8), if 4 (the default), is 
   insufficient.  Press 'replot', or reselect the gauge from the
   correlation window to plot the data.  Specify, different
   'Second intermediate files' to examine data for other gauges.
   New correlations will be calculated.  Specify a new 'Gauge filter file'
   or edit the current one, to add or delete gauges from the correlations
   or plot.

   Detailed
   --------

   1. Select 'Action'->'Show gauges' to display a window with Gauge/Radar
      correlations.

   2. Select a Gauge by clicking on a Gauge in the 'Gauges' window.

      Once a gauge is selected, plotting begins.  For large Second
      intermediate files, it may take some time before plot appears.

      Selecting a gauge that is 'net AAA' means plot the entire network
      for network AAA.  In this case the 'Correlations' window contains
      meaningless information.

   3. Change the 'Corr. Matrix index', valid range is 0 - 8, to
      select a column nearby.  Ideally, the gauge is centered in 
      the correlations, index 4.

      You must select 'Action'->'Replot' or reselect the Gauge as in 
      [2.] above.

   4. Changing the 'Second intermediate file' entry via
      'File'->'Open_intermediate_file', effects an immediate
      showing of gauges.

   5. Changing the 'Gauge filter file' entry via
      'File'->'Open filter file', effects nothing.  You have to 
      'Action'->'Replot' or reselect the Gauge as in [2.] above.

   6. You can change the contents of the 'Gauge filter file' by
      'File'->'Edit_filter_file'.

   7. You can change the X axis range by changing the numbers.  You
      must 'Action'->'Replot' to see the plot based on the new X axis range.

   8. You can examine the entire dataset use for plotting, the
      full X axis range, via the 'Action'->'Show Data' menu.  Both the
      fractional Julian day, that plotted on the X axis, and the
      cononical date format are displayed.  You have the option
      of saving the data to a file.

   9. You can turn off the plotting of negative gauge values in the
      accumulation plot.  Negative values are suspect, however, they
      are reasonable for total rain accumulations.  See 'Options'.

  10. You have the option of plotting rain for both convective and
      stratiform rain classifications for the radar and gauge
      measurements, or any combinination of them.  See 'Options'.

  11. Shown at the top are the Gauge/Radar (denoted G/R) ratios for
      total rain, convective only and stratiform only.  These
      ratios are computed from the last data point.  See 'Options'.

  12. You can treat the data in the ascii (intermediate) file as
      rain-rates, if you select 'Options'->'mm/hr'.  Setting this 
      option prevents passing the data through the FACE Z-R calculation --
      normally done for 'Options'->'dBz'.

      This allows you to plot 2A-53 data contained in the intermediate
      ascii files.

LIMITATIONS
-----------

   This package is simple and designed to provide you with a 
   cursory view of the Gauge and Radar correlationships. 

   Functionality is performed via several perl scripts:

   1. The 'Action'->'Show gauges' menu calls the script 'gauge_qc.pl' and
      'gauge_radar_plot_filter.pl'.

   2. The plotting is done via 'gauge_radar_plot_filter.pl'.  This
      program produces the files 'radar.plot', 'gauge.plot' and
      'radar-gauge.plot'.

      Actually, 'gauge_radar_plot_filter.pl' is composed of 
      several perl and unix scripts.

         sort_zr_intermediate_ascii
         sort_zr_intermediate_ascii_by_time
         gauge_filter.pl
         gauge_radar_accum.pl
         plot_subtract.pl

   3. 'gauge_filter.pl' can filter the second intermediate file
      to remove gauges.  A list of gauges and their times are specified
      in a filter file (default name is filter.file)
      The specification of which gauges for which times to remove is
      similiar to that of the unix crontab.
      Here is an example:

      ----------- cut filter.file ---------------
      # MM HH DD MO YR GNET GNUM
      # Ranges and wildcards are acceptable.

      * * 10 12 1997 STJ 275
      * * 1-3 8 1997 STJ 511
      * * * * * KSC 11
      ----------- cut filter.file ---------------

   4. 'gauge_radar_nonzero.pl' can be used to reduce the size of the
      second intermediate file.  It remove all entries where there
      are not all nonzero gauge measurements, usually 5 minute
      time centered about the radar time.

      This can drastically shrink the second intermediate file and
      speed up correlation calculation and plotting.

      This is the first step of gauge_radar_accum.pl and gauge_qc.pl.

   5. 'gauge_radar_accum.pl' generates the plot information.  Several
      columns of data are provide which are 'cut' and spliced 
      to produce files suitable for plotting via 'gnuplot' or
      the 'gauge_gui_main.pl' interface.


QUESTIONS
---------

help\@gsfc.nasa.gov
EOF

open(HELPDOC, "> helpdoc.$$");
print HELPDOC $help;
close(HELPDOC);

fl_load_browser($help_browser, "helpdoc.$$");
fl_show_form($help_form, FL_PLACE_CENTER, FL_FULLBORDER, "Help");
unlink("helpdoc.$$");
}

#########################################################################
#                                                                       #
#                           jday_to_mmddyy_hhmmss                       #
#                                                                       #
#########################################################################
sub jday_to_mmddyy_hhmmss {
	local ($jday, $yyyy) = @_;

	local ($HH, $MM, $SS);
	local ($i, $leap, $index, @mo_noleap, @mo_leap, @mo);
	local ($mm, $dd);
#
# Input: jjj.frac yyyy
#
# Convert fractional julian day (day count from begining of year) to
# mm/dd/yy hh:mm:ss.x format.
#
	local $frac = $jday - int($jday);
	$jday = int($jday);
#
# First tackle the hh:mm:ss.x
	$HH = 24 * $frac;
	$frac = $HH - int($HH);
	$MM = 60 * $frac;
	$frac = $MM - int($MM);
	$SS = 60 * $frac;
#
# Now tackle the julian day conversion.


	@mo_noleap = (0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365);
    @mo_leap   = (0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366);
	@mo = (@mo_noleap, @mo_leap);
    $leap = 0;
	if ( ($yyyy%4 == 0 && $yyyy%100 != 0) || $yyyy%400 == 0 ) { $leap = 1; }
	$index = $leap *12;

	for ($i=$index; $i<=$#mo; $i++) {
		last if $jday <= $mo[$i];
	}
	$mm = $i;
	$dd = $jday - $mo[$i-1];

# Fix the time.   Note: mm/dd/yyyy is unaffected, since we are starting
# with a fractional julian day.  Only the HH and MM might be affected.
	if ($SS > 59.5) {$MM++; $SS=0}
	if ($MM > 59.5) {$HH++; $MM=0}
	return sprintf("%2.2d/%2.2d/%4d %2.2d:%2.2d:%2.2d", $mm, $dd, $yyyy, $HH, $MM, $SS);
}
	
#########################################################################
#                                                                       #
#                           do_correlation                              #
#                                                                       #
#########################################################################
sub do_correlation {
	local ($obj, $data) = @_;

	if (! -r $filter_file) {
		fl_show_alert("File not found: $filter_file", "Specify a valid Gauge filter filename.", "");
		return;
	}

	if (! -r $second_intermediate_file) {
		fl_show_alert("File not found: $second_intermediate_file", "Specify a valid Second intermediate filename.", "");
		return;
	}

	fl_set_browser_fontstyle($r_browser, FL_FIXEDBOLD_STYLE);
	fl_set_browser_fontsize($r_browser, 14);
	fl_show_form($corr_form, FL_PLACE_CENTER, FL_FULLBORDER, "Correlations");
	fl_clear_browser($r_browser);
	set_all_forms_busy();

# I would have preferred to used a bidirectional pipe (pipe on both ends),
# but, this is not as easy as using a temp file.  Damn.
	$pos_opt = "-p" if $pos_gauge_values;
	open(CORR, "| gauge_filter.pl -f $filter_file | gauge_qc.pl $pos_opt > CORR.OUTPUT$$");
	print CORR $gauge_radar_merge{"Header"};
	local $key = "$gnet $gnum";
	print CORR $gauge_radar_merge{$key};
	close (CORR);
	open(CORR, "CORR.OUTPUT$$");
	@corr = <CORR>;
	close(CORR);
	unlink("CORR.OUTPUT$$");

	fl_freeze_form($corr_form);
	foreach $line (@corr) {
		chop $line;
		fl_add_browser_line($r_browser, $line);
	}
	fl_unfreeze_form($corr_form);
	set_all_forms_unbusy();
}

#########################################################################
#                                                                       #
#                          set_all_forms_busy                           #
#                          set_all_forms_unbusy                         #
#                                                                       #
#########################################################################
sub set_all_forms_busy {
	fl_deactivate_all_forms;

	if ($plot_form->visible) {
		fl_set_cursor($plot_form->window, FL_WATCH_CURSOR);
		fl_freeze_form($plot_form);
	}
	if ($corr_form->visible) {
		fl_set_cursor($corr_form->window, FL_WATCH_CURSOR);
	}
	if ($data_form->visible) {
		fl_set_cursor($data_form->window, FL_WATCH_CURSOR);
		fl_freeze_form($data_form);
	}
	if ($gauge_form->visible) {
		fl_set_cursor($gauge_form->window, FL_WATCH_CURSOR);
		fl_freeze_form($gauge_form);
	}
	fl_check_forms();
}

sub set_all_forms_unbusy {
	fl_activate_all_forms;
	if ($plot_form->visible) {
		fl_set_cursor($plot_form->window, FL_DEFAULT_CURSOR);
		fl_unfreeze_form($plot_form);
	}
	if ($corr_form->visible) {
		fl_set_cursor($corr_form->window, FL_DEFAULT_CURSOR);
	}
	if ($data_form->visible) {
		fl_set_cursor($data_form->window, FL_DEFAULT_CURSOR);
		fl_unfreeze_form($data_form);
	}
	if ($gauge_form->visible) {
		fl_set_cursor($gauge_form->window, FL_DEFAULT_CURSOR);
		fl_unfreeze_form($gauge_form);
	}
	fl_check_forms();
}

#########################################################################
#                                                                       #
#                          do_show_gauges                               #
#                                                                       #
#########################################################################
sub do_show_gauges {
	local ($obj, $data) = @_;
	local $key;
	local ($line, $range);
	local ($net, $num);

	fl_set_browser_fontstyle($gauge_browser, FL_FIXEDBOLD_STYLE);
	fl_set_browser_fontsize($gauge_browser, 14);
	fl_show_form($gauge_form, FL_PLACE_CENTER, FL_FULLBORDER, "Gauges");
	fl_clear_browser($gauge_browser);
	set_all_forms_busy();
# Suck the second intermediate file into an associative array.
# Do this, not for gauge_filter.pl, but for the plotting which
# happens due to selecting a gauge from the correlation window.
# Read time is approx. 1M/s.
	%gauge_radar_merge = load_second_intermediate_file($second_intermediate_file);

	fl_freeze_form($gauge_form);
	foreach $key (sort keys(%gauge_radar_merge)) {
		next if $key eq "Header";
		($junk, $junk, $junk, $junk, $range, $junk) = split(' ',$gauge_radar_merge{$key});
		($net, $num) = split(' ', $key);
		$range = 0 if $net eq "net";
		$line = sprintf("%-4s%5s %7.2fkm",$net, $num, $range);
		fl_add_browser_line($gauge_browser, $line);
	}
	fl_unfreeze_form($gauge_form);
	set_all_forms_unbusy();
}

#########################################################################
#                                                                       #
#                           do_select_browser                           #
#                                                                       #
#########################################################################
sub do_select_browser {
	local ($obj, $arg) = @_;

	$n = fl_get_browser($obj);
	$line = fl_get_browser_line($obj, $n);
	set_all_forms_busy();
	@line = split(' ', $line);
	$gnet = $line[0];
	$gnum = $line[1];
	do_correlation();
	do_plot();
	set_all_forms_unbusy();
}

#########################################################################
#                                                                       #
#                           do_edit_filter_file                         #
#                                                                       #
#########################################################################
sub do_edit_filter_file {
	local ($obj, $arg) = @_;

	system("emacs $filter_file &" );
}

#########################################################################
#                                                                       #
#                           MENU routines                               #
#                                                                       #
#########################################################################
sub do_file_menu {
	local ($obj, $arg) = @_;
	local $item;

	$item = fl_get_menu_text($obj);
	do_quit() if ("$item" eq "Exit");
	do_second_intermediate_browse() if ("$item" eq "Open_intermediate_file");
	do_browse_filter_file() if ("$item" eq "Open_filter_file");
	do_edit_filter_file() if ("$item" eq "Edit_filter_file");
}

sub do_action_menu {
	local ($obj, $arg) = @_;
	local $item;

	$item = fl_get_menu_text($obj);
	do_show_gauges() if ("$item" eq "Show_gauges");
	draw_data() if ("$item" eq "Replot");
	draw_data($item) if ("$item" eq "Reset_x-axis");
	do_show_data() if ("$item" eq "Show_data");
}

sub do_options_menu {
	local ($obj, $arg) = @_;
	local $item;

	$item = fl_get_menu_text($obj);
	$item_num = fl_get_menu($obj);
	if ("$item" eq "Convective") {
		$do_convective = (fl_get_menu_item_mode($obj, $item_num) == (FL_PUP_CHECK | FL_PUP_BOX));
	}
	if ("$item" eq "Stratiform") {
		$do_stratiform = (fl_get_menu_item_mode($obj, $item_num) == (FL_PUP_CHECK | FL_PUP_BOX));
	}
	if ("$item" eq "Total") {
		$do_total = (fl_get_menu_item_mode($obj, $item_num) == (FL_PUP_CHECK | FL_PUP_BOX));
	}
	if ("$item" eq "Positive_only") {
		$pos_gauge_values = (fl_get_menu_item_mode($obj, $item_num) == (FL_PUP_CHECK | FL_PUP_BOX));
	}

# These are radio buttons.
	$do_dbz = 0 if ("$item" eq "millimeters");
	$do_dbz = 1 if ("$item" eq "dBz");

	if ("$item" eq "millimeters" || "$item" eq "dBz") {
		do_plot;
	} else {
		draw_data;
	}

}

sub do_help_menu {
	local ($obj, $arg) = @_;
	local $item;
	$item = fl_get_menu_text($obj);
	do_help() if ("$item" eq "Help");
}
#########################################################################
#                                                                       #
#                           do_close                                    #
#                           do_set_network                              #
#                           do_set_number                               #
#                           do_cs                                       #
#                           do_dbz_rrmap_button                         #
#                           do_quit                                     #
#                                                                       #
#########################################################################

sub do_matrix_index_input {
	local ($obj, $arg) = @_;
	
	$matrix_index = fl_get_input($obj);
	do_plot if $matrix_index ne "";
}

sub do_close {
	local ($obj, $arg) = @_;
	
	fl_hide_form($obj->form);
}

sub do_min_max {
	local ($obj, $arg) = @_;
# Don't bother with any action.
# These xmin/xmax values are read when 'replot' is pressed. 
}

sub do_set_network {
	local ($obj, $arg) = @_;
	$gnet = fl_get_input($obj);
	do_correlation();
	do_plot();
}

sub do_set_number {
	local ($obj, $arg) = @_;
	$gnum = fl_get_input($obj);
	do_correlation();
	do_plot();
}

sub save_config {
	
	warn "Unable to save configuration." if ! open(RC, ">$ENV{HOME}/.gauge_gui_rc");
	print RC "$second_intermediate_file\n";
	print RC "$filter_file\n";
	close(RC);
}


sub do_quit {
# Save $second_intermediate_file and $filter_file in ~/.gauge_gui_rc

	save_config();

	exit;
}

#########################################################################
#                                                                       #
#                           do_plus_minus_gauge                         #
#                                                                       #
#########################################################################
sub do_plus_minus_gauge {
	local ($obj, $data) = @_;

#
# Interface that permits addition/removal of gauges for filtering.
# The program 'gague_filter.pl' uses the file this interface produces
# to exclude gauges from the second intermediate file.  The contents
# of the output file are similiar to crontab, or in gvbox, similiar
# to the runtime parameter files.
#
# Example: File filter.file
#
#     # Remove gauge STJ 302 for March 2-3 1997, all hours and minutes.
#      * * 2-3 3 1997 STJ 302
#

}

#########################################################################
#                                                                       #
#                           m a i n                                     #
#                                                                       #
#########################################################################
$display = fl_initialize("Buttform");
 create_the_forms();

# Initialize
#$second_intermediate_file = "zr_second_intermediate_MELB_12-1997.asc";
#$second_intermediate_file = "second.080197-081097.ascii";
if (! open(RC, "$ENV{HOME}/.gauge_gui_rc")) {
	$second_intermediate_file = "s.ascii";
	$filter_file = "filter.file";
} else {
	$_ = <RC>; chop;
	$second_intermediate_file = $_;
	$_ = <RC>; chop;
	$filter_file = $_;
	close(RC);
}
$do_convective = $do_stratiform = $do_total = 0;
$do_total = 1;
$do_dbz   = 1;
$pos_gauge_values = 0;

# Without searching for 'dBz' in the Option's menu, I'll just assume
# it is number 5.  Option 4 is millimeters.
fl_set_menu_item_mode($options_menu, 4, FL_PUP_BOX|FL_PUP_RADIO);
fl_set_menu_item_mode($options_menu, 5, FL_PUP_BOX|FL_PUP_CHECK|FL_PUP_RADIO);

fl_set_object_label($second_intermediate_text, $second_intermediate_file);

$matrix_index = 4;
fl_set_input($matrix_index_input, $matrix_index);
fl_set_input_return($matrix_index_input, FL_RETURN_ALWAYS);

# Gauge filter file.  Entries resemble crontab format.
fl_set_object_label($filter_file_text, $filter_file);

# show the first form */
 fl_show_form($plot_form, FL_PLACE_CENTER,FL_FULLBORDER,"Gauge gui");

 while ($done != fl_do_forms()){}

