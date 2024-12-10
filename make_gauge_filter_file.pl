#! /usr/bin/perl
#
#  INPUT: List of time ranges w/ strings.  Usually the output of 'eyalqc'.
#
# OUTPUT: Gauge filter file (cron style).  Suitable for gauge_filter.pl
#
#------------------------------------------------------------------------
#

while(<>) {
	system("make_when_table $_");
}
