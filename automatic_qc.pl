#! /usr/bin/perl

#  INPUT: second intermediate file (ascii)
# OUTPUT: gauge filter file, suitable for gauge_filter.pl
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

    automatic_qc.pl [--help]
                    [--report gauges_removed.out]
                    [--positive-only]
                    [2-nd_intermediate.ascii]

	--help             = This usage message.
   
  Input: STDIN   - The merged radar and gauge file (2-nd intermediate file).
 Output: STDOUT  - Gauge filter file suitable for gauge_filter.pl.
EOF
	exit;
}
###############################################################
#                                                             #
#                         m a i n                             #
#                                                             #
###############################################################
use Getopt::Long;

GetOptions("help" => \$help,
		   "positive-only" => \$positive_only,
		   "report=s" => \$report) or usage();

usage() if $help;
$report_cmd = "| tee $report" if $report ne ""; 
$pos_opt = '-p' if $positive_only;
system("sort_zr_intermediate_ascii @ARGV | eyalqc $pos_opt $report_cmd | make_gauge_filter_file.pl");
