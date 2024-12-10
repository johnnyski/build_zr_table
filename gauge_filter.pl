#! /usr/bin/perl
#
# Read time specifications, similiar to cron time, from a file
# and eliminate matching entries showing up from the stdin; filter
# them out.  There are two more items present for each time specification:
# gauge network and gauge id.  Eliminate only those that match.
# Gauge network and gauge id must match exactly.
# The file containing the times may contain comments deliminated
# via '#' in col 1, or at the end of a valid line.
#
# While this program allows general time specifications, it really only
# makes sense to eliminate ranges of times.  Exact times will likely not
# occur.  Maybe they will.
#
# The format of each line is:
# MIN HOUR DAY MONTH YEAR GAUGE_NET GAUGE_ID
#
# where:
# MIN      : 0 - 59
# HOUR     : 0 - 23
# DAY      : 1 - 31
# MONTH    : 1 - 12
# YEAR     : 1997...
# GAUGE_NET: SFL, STJ, ...
# GAUGE_ID : any integer.
# 
# You can specify:
#    *              --  Wild match.  This matches all possibilities.
#    n-m            --  Range.  n=begin, m=end, inclusive.
#    Exact values   --  Exact time specification.
#
# Example:
#
#       # at 6:10 a.m. every day
#       10 6 * * * STJ 511 # Comment ...
#
#       # at 11:00-11:59 a.m. on the 4th for 1997 and 1998. 
#       * 11 4 * 1997-1998 SFL 101
#
#       # 4:00 a.m. on january 1st
#       0 4 1 1 * STJ 511
#
#       # once an hour.
#       0 * * * * STJ 511
#
#######################################################################
sub rangematch {
	local($item, $range) = @_;

	if ($range eq "*") { return 1; }
	$pos = index($range, "-");
	if ($pos > -1) { # Dealing w/ a range. 
		@range = split(/-/,$range);
		if ($range[0] <= $item && $item <= $range[1]) {
			return 1;
		} else {
			return 0;
		}
	} elsif ($item == $range) {
#		printf("<%s> range=<%s>\n", $item, $range);
		return 1;
	} else {
		return 0;
	}
}

sub datematch {
	local($a_month, $a_day, $a_year, $a_hour, $a_min,
		  $mon,     $day,   $year,   $hour,   $min) = @_;


#	printf("rangematch(%s, %s) is %d\n", $a_year, $year, &rangematch($a_year, $year));
#	printf("rangematch(%s, %s) is %d\n", $a_month, $mon, &rangematch($a_month, $mon));
#	printf("rangematch(%s, %s) is %d\n", $a_day, $day, &rangematch($a_day, $day));
#	printf("rangematch(%s, %s) is %d\n", $a_hour, $hour, &rangematch($a_hour, $hour));
#	printf("rangematch(%s, %s) is %d\n", $a_min, $min, &rangematch($a_min, $min));
	if (&rangematch($a_year, $year) == 1) { 
		if (&rangematch($a_month, $mon) == 1) { 
			if (&rangematch($a_day, $day)   == 1) {
				if (&rangematch($a_hour, $hour) == 1) {
					if (&rangematch($a_min, $min)   == 1) {
						return 1; }
				}
			}
		}
	}
# No match.
	return 0;
}

sub load_filter_file {
	local ($infile) = @_;
	local (%filter);
	local ($n);
#
# Construct an associative array:
# Keys are NET GAUGE_NUM
# Each element is an array or records; all the NET GAUGE_NUM records
#
	die "Unable to open <$infile>.\n" if ! open(S, "$infile");

	while (<S>) {
		next if /^#/;
		$line = $_; # Save
		split;
		next if $#_ < 6;
		$key = "$_[5] $_[6]";
		$count_key = "$key count";
#		print STDERR "--KEY=<$key>, --N=<$n,$filter{$count_key}>, --COUNT_KEY=<$count_key> LINE=<$line>\n";
		$n = $filter{$count_key} + 0;
		$filter[$n]{$key} = $line;
		$filter{$count_key}++;
	}
	close(S);
	return %filter;
}

sub usage {
	print STDERR "Usage: $0 [-h] [-f filter.file]\n";
	print STDERR "\n";
	print STDERR "Where: -f filter.file - lines of time and gauge specification that\n";
	print STDERR "                        will be removed from stdin.  Default: none.\n";
	print STDERR "       -v - Verbose.  Show how much was filtered.\n";
	print STDERR "       -h - Show usage.\n";
}

require 'getopts.pl';
#
# Output, to stdout, the last entry that matches the time pattern.
#

Getopts('f:hv');
if ($opt_h) {
	usage();
	exit;
}

if ($opt_f eq "") {
	# Perform cats (live).
	while(<>) {
		print;
	}
	exit;
}

$filter_file = $opt_f;

%filter = load_filter_file($filter_file);

while (<>) {
	print;
	last if /Table begins:/;
}

while (<>) {
	$line = $_; # Save for possible output.
	split;
	($gid, $gnet, $date, $time) = @_;
	($a_month, $a_day, $a_year)  = split(/\//, $date);
	($a_hour,  $a_min)   = split(/:/, $time);
# Now, determine if this entry matches any entry from the FILTER.
	$found = 0;
	$key = "$gnet $gid";
	$num = $filter{"$key count"};

	$total_pairs{$key}++;

#	print STDERR "KEY=<$key>, NUM=<$num>\n";
	for($n=0; $n < $num; $n++) {
		$_ = $filter[$n]{$key};
		split;
		next if $#_ < 6;
		($min, $hour, $day, $month, $year, $f_gnet, $f_gid, $comment) = @_;
# Save the match; overwrite any previous time match.
		if ($f_gnet eq $gnet && $f_gid eq $gid &&
			&datematch($a_month, $a_day, $a_year, $a_hour, $a_min,
					   $month,   $day,   $year,   $hour,   $min)) {
			$found = 1;
		}
	}
	$pairs_removed{$key}++ if $found;
	next if $found;
	print $line;
}

foreach $key (sort keys(%total_pairs)) {
	if ($opt_v) {
		printf STDERR "%8s removed %5d out of %5d. %6.2f%% removed.\n",
	}
	$key, $pairs_removed{$key}, $total_pairs{$key},
	100*$pairs_removed{$key}/$total_pairs{$key};
}
