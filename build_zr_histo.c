
/******************************************************************

	 Mainline driver and assorted functions to generate Z-R histograms
	 based upon:
	   1) Rain classification at the raingauge.
	   2) Range of the raingauge from the radar site.

	 -----------------------------------------------------------------

	 Usage:

	   build_zr_histo [options] inFile outFile

	 Options:
	  -v:               verbose
	  -c:               Use only the cell over gauge from spatial gauge window.
	                    Note: A cell has area 4km^2 (2km x 2km)
                      (Default: use 9(=3x3) cells from one height)
	  -h <1.5 or 3.0>:  Use window cells from specified height(km) from 2A-55.
                      (Default: range-dependant)
    -t <mins>:        temporal gauge window interval. (Default: 10 minutes)
    -r <r1,r2,.,150>: range intervals (km) for Z-R tables

	 inFile:  2nd_zr_intermediate_file from 'merge_radarNgauge_data'
	 outFile: output Z-R histograms.

  -----------------------------------------------------------------

		mike.kolander@trmm.gsfc.nasa.gov  (301) 286-1540
		31 Jul 1998
		Space Applications Corporation
		NASA/TRMM Office
		Copyright (C) 1997

*******************************************************************/


#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "zr_utils.h"


#define MAX_RANGE_INTERVALS 10
#define MAX_RANGE 150.0         /* km */




/* Raingauge window cell descriptor */
typedef struct
{
	int rtype;
	float z;
	float height;
} WindowCell;


/* Raingauge window (spatial and temporal) descriptor */
typedef struct
{
	/* Spatial window over raingauge */
	int cell_over_gauge_only;  /* Spatial window size: Either 1 or 9 cells */
	float height;              /* height = 1.5 or 3.0 km */
	/* Temporal window */
	int duration;              /* minutes */
	float offset;              /* window center offset ;ie,
															  bias the window center away from the
																center in 2nd int_file.*/
	int unclassified;          /* 0: Use rainclasses as found in 2nd int_file
																1: Lump all rainclasses together into one */
} Window;



/* Global variable */
int verbose=0;

/*
 * Functions defined in this file.
 */
int index_of_range(float range_interval[], int nrange, float gauge_range);
ZR_histo *read_intermediate_file(ZR_histo *histo, char *infile,
																 Window *window);
ZR_histo *build_ZR_histo(char *infile, float *r, int nrange, Window *window);
void usage();
/* int main(int argc, char **argv) */



/*************************************************************/
/*                                                           */
/*                      index_of_range                       */
/*                                                           */
/*************************************************************/
int index_of_range(float range_interval[], int nrange, float gauge_range)
{
	/* Given a gauge_range, returns the corresponding index into
		 the array of range_intervals.
		 Returns -1 if error.
  */
	int j;

	if (gauge_range < 0.0)
	{
		fprintf(stderr, "gauge_range < 0 ???\n");
		return(-1);
	}

	for (j=0; j<nrange; j++)
	  if (gauge_range <= range_interval[j]) return(j);
	
	fprintf(stderr, "gauge_range: %.3fkm > MAX_RANGE: %.1fkm\n",
					gauge_range, MAX_RANGE);
	return(-1);
}

/*************************************************************/
/*                                                           */
/*                   read_intermediate_file                  */
/*                                                           */
/*************************************************************/
ZR_histo *read_intermediate_file(ZR_histo *histo, char *infile,
																 Window *window)
{
	/* 
		 Reads the 'windowed' radar Z values and the raingauge R values
		 from 'infile'. Uses these values to create histograms for Z and R.

		 Fills header fields in the structure 'ZR_histo' based on header
		 values found in 'infile'.

		 Returns:
		     Arrays of histograms for both Z and R, and a partially filled
				 ZR_histo, if success.
				 NULL, if failure.
  */
	struct tm tm;
	FILE *fp;          /* intermediate_file */
	char *s;
	float temp;
#define LINE_BUF_SIZE 256
	char buf[32], line[LINE_BUF_SIZE];
	int i, j, irange;
	/* Array of raintype indices; ie, irtype[raintype] gives the index
	   into the histograms for a specific raintype, where raintypes are
		 defined in the intermediate_file. */
	int irtype[32];
	/*
	 * Radar affiliated variables
	 */
	int radar_says_rain;  /* 1, if a valid Z in spatial window,
													 0, else. */
	int ncells, nvalid_cells, nrtype1, nrtype2, rtype, window_rtype=0;
	float cell_height, z;
#define ARRAY_SIZE 64
	WindowCell cell[ARRAY_SIZE];  /* Array of triplets (rtype, z, height) */
	/*
	 * Raingauge affiliated variables
	 */
	int gauge_says_rain;   /* 1, if a positive rainrate in temporal window,
														0, else. */
	int ngauge_rates, nvalid_gauge_rates;
	int fileWindow;    /* Length (minutes) of the gauge window used to
												create the 2nd_intermediate file. */
	int temporalWindow_start; /* Starting index of the temporal gauge
															 window used to build the R_histogram. */
	int temporalWindow_end;   /* Ending index of temporal gauge window */
	float gauge_range, g_rate, gauge_rate[ARRAY_SIZE], gauge_avg;
	/*
	 * Z_ and R_histograms
	 */
	int ***zbin, ***rbin;


	putenv("ZT=UT"); /* No local time zone conversion w/ mktime. */
	/*
	 * Open the intermediate_file for input.
	 */
	fp = fopen(infile, "r");
	if (fp == NULL)
	{
	  fprintf(stderr, "Error opening file: %s\n", infile);
		return(NULL);
	}
	/*
	 * Find the 1st header line in the intermediate_file.
	 */
	while ((s = fgets(line, sizeof(line), fp)))
	{
		chop(s);
		if (strncmp(line, " Site_name", 10) == 0) break;
	}
	if (s == NULL)
	{
		fprintf(stderr, "Intermediate table format error.\n");
		fclose(fp);
		return(NULL);
	}
	/*
	 * Read the various header lines from the intermediate_file.
	 */
#define START_COLUMN 35
  sprintf(histo->site_name, "%s", &s[START_COLUMN]);  /* Site Name */

  s = fgets(line, sizeof(line), fp); chop(s); /* Start date/time */
  memset(&tm, 0, sizeof(tm));
  sscanf(&s[START_COL], "%d/%d/%d %d:%d:%d",
		 &tm.tm_mon,
		 &tm.tm_mday,
		 &tm.tm_year,
		 &tm.tm_hour,
		 &tm.tm_min,
		 &tm.tm_sec);
  tm.tm_year -= 1900;
  tm.tm_mon --;
  tm.tm_isdst = -1;    /* Don't use daylight savings time. */
  histo->start_time = mktime(&tm);
  
  s = fgets(line, sizeof(line), fp); chop(s); /* Stop date/time */
  memset(&tm, 0, sizeof(tm));
  sscanf(&s[START_COL], "%d/%d/%d %d:%d:%d",
		 &tm.tm_mon,
		 &tm.tm_mday,
		 &tm.tm_year,
		 &tm.tm_hour,
		 &tm.tm_min,
		 &tm.tm_sec);
  tm.tm_year -= 1900;
  tm.tm_mon --;
  tm.tm_isdst = -1;    /* Don't use daylight savings time. */
  histo->stop_time = mktime(&tm);
	
  s = fgets(line, sizeof(line), fp); chop(s); /* Radar lat */
  sscanf(&s[START_COLUMN], "%f", &histo->radar_lat);

  s = fgets(line, sizeof(line), fp); chop(s); /* Radar lon */
  sscanf(&s[START_COLUMN], "%f", &histo->radar_lon);

  s = fgets(line, sizeof(line), fp); chop(s); /* Gauge_window_size */

  s = fgets(line, sizeof(line), fp); chop(s); /* Rain class type */
	if (window->unclassified) histo->nrtype = 1;
	else if (strcmp(&s[START_COLUMN], "DUAL") == 0) histo->nrtype = 2;
	else if (strcmp(&s[START_COLUMN], "UNIFORM") == 0) histo->nrtype = 1;
	else
	{
		fprintf(stderr, "Unknown Rain Class Type\n");
		goto error_exit;
	}

	s = fgets(line, sizeof(line), fp);  /* Number of rain types */
	
	histo->rain_type_str = (char **)calloc(histo->nrtype, sizeof(char *));
	/*
	 * Read the rain types.
	 */
	j = 0;
	while ((s = fgets(line, sizeof(line), fp)))
	{
		chop(s);
		if (strncmp(line, " Rain_type_value", 6) != 0) break; /* End rain_types */
		sscanf(&s[START_COLUMN], "%d %s", &rtype, buf);
		if (rtype < 1) continue;  /* Ignore 0 and -1 */
		if (window->unclassified) continue; /* Will handle this case below. */
		histo->rain_type_str[j] = (char *) strdup(buf); /* Raintype descriptive string */
		irtype[rtype] = j;              /* Raintype index */
		j++;
	}
	if (window->unclassified)
	{
		histo->rain_type_str[0] = (char *) strdup("UNIFORM");
		irtype[1] = 0;             /* Raintype index */
	}
	histo->z_low = -15.0;
  histo->z_hi  =  70.0;
  histo->z_res =  0.1;
  histo->nZbins = (histo->z_hi - histo->z_low)/histo->z_res + 1 ;		
	/*
	 * Allocate space for 'nrange x nrtype' vectors of length 'nZbins'.
	 * Each vector will contain a histogram of radar Z values.
	 */
	zbin = (int ***) new_histogram_vectors(histo->nrange, histo->nrtype,
																				 histo->nZbins);
	histo->z = zbin;
	/*
	 * Allocate space for 'nrange x nrtype' vectors of length NR_BINS.
	 * (Max rainrate = 900 mm/hr)
	 * Each vector will contain a histogram of gauge rainrate values.
	 */
	rbin = (int ***) new_histogram_vectors(histo->nrange, histo->nrtype,
																				 NR_BINS);
	histo->r = rbin;
	/*
	 * window duration (minutes) used to generate the
	 * intermediate file.
	 */
	sscanf(&s[START_COLUMN], "%d", &fileWindow);  /* Window_time_range */
	/*
	 * Compute the lo and hi indices of the temporal window used to
	 * select which of the gauge readings from the file we'll use
	 * for the R-histograms.
	 */
	/* Lower bound */
	temp = (fileWindow / 2.0) - (window->duration / 2.0) + window->offset;
	if (temp < 0.0)	/* Check for valid temporal window lower bound. */
	{
		fprintf(stderr, "Lower index of temporal window < 0\n");
		fprintf(stderr, "0 <= Valid temporal window indices <= %d\n\n",
						fileWindow-1);
		fclose(fp);
		return(NULL);
	}
	temporalWindow_start = (int)temp;

	/* Upper bound */
	temp = (fileWindow / 2.0) + (window->duration / 2.0) + window->offset;
	if (temp > fileWindow)	/* Check for valid temporal upper window bound. */
	{
		fprintf(stderr, "Upper index of temporal window > %d\n", fileWindow-1);
		fprintf(stderr, "0 <= Valid temporal window indices <= %d\n\n",
						fileWindow-1);
		fclose(fp);
		return(NULL);
	}
	temporalWindow_end = (int)temp;

	if (verbose)
	  fprintf(stderr, "temporalWindow_lowerIndex:%d temporalWindow_upperIndex:%d\n",
						temporalWindow_start, temporalWindow_end);

	/*
	 * Find the data table in the file.
	 */
	while ((s = fgets(line, sizeof(line), fp)))
	{
		chop(s);
		if (strncmp(line, "Table begins", 10) == 0) break;
	}
	if (s == NULL)
	{
		fprintf(stderr, "Intermediate table format error.\n");
		fclose(fp);
		return(NULL);
	}

	/********************
		 Read each line of VOS_gauge window data values from infile. Write
		 the valid values into temporary storage arrays, and discard the
		 invalid/irrelevant values.
	1. If valid radar Z values exist, they are added to the
		 appropriate Z_histogram, determined by gauge_range and raintype,
		 and the gauge R values are added to the appropriate R_histogram.
	2. If valid gauge rates exist, they are averaged over a temporal window,
	   and the average added to the appropriate R_histogram.
	*********************/
	while (fscanf(fp, "%*d %*s %*s %*s %f %*d", &gauge_range) != EOF)
	{
		/* Read from the file the pair of radar-derived values
			 (rtype, z) for each of the 'ncells' window cells from the
			 bottom two carpis over this raingauge.
			 If the values 'rtype' and 'z' are valid,
			 save the values in the array cell[], otherwise discard. */
		radar_says_rain = 0;
		nvalid_cells = 0;
		for (i=0; i<2; i++)  /* for the bottom two carpis... */
		{
			/* Read carpi height, and number of cells which follow. */
			fscanf(fp, "%f %d", &cell_height, &ncells);
			for (j=0; j<ncells; j++)
			{
				/* Read the pair of values (raintype, Z) for one window cell. */
				fscanf(fp, "%d %f", &rtype, &z);
				/*
				 * If only the center window cell is desired, discard all
				 * other cells.
				 */
				if (window->cell_over_gauge_only)
					if (j != (int)ncells/2) continue;
				/*
				 * If this cell isn't from the desired window_height, discard it.
				 */
				if ( (window->height == 1.5) && (cell_height != 1.5) ) continue;
				else if ( (window->height == 3.0) && (cell_height != 3.0) ) continue;
				else if (window->height == 0.0) /* Range-dependant window height. */
				{
					/* If gauge_range < 100.0 , ignore data from 2nd carpi. */
					if ((gauge_range < 100.0) && (cell_height == 3.0)) continue;
				}
				/* If gauge_range >= 100.0 , no valid data in lowest carpi. */
				if ( (gauge_range >= 100.0) && (cell_height == 1.5) ) continue;

				/*
				 * Check for valid raintype.
				 */
				if (rtype == -1) continue;  /* Discard missing/bad data */
				if (rtype == 0) continue;   /* No echo ??? */
				if ( (rtype > 0) && (z > -15.0) ) radar_says_rain = 1;
				
				/*
				 * Check for valid Z value.
				 */
				if (z < histo->z_low) z = histo->z_low;
				if (z > histo->z_hi)  z = histo->z_hi;
				/*
				 * Save the triplet (rtype, z, height) in 'cell[]' array.
				 */
				cell[nvalid_cells].rtype = rtype;
				cell[nvalid_cells].z = z;
				cell[nvalid_cells].height = cell_height;
				nvalid_cells++;
				if (nvalid_cells >= ARRAY_SIZE)
				{
					fprintf(stderr, "Too many window cells (> %d)\n", ARRAY_SIZE);
					goto error_exit;
				}
			} /* end for (j=0; j<ncells... */
		} /* end for (i=0... */
		

		fscanf(fp, "%d", &ngauge_rates);   /* number of gauge observations */
		/*
		 * Read all 'ngauge_rates' gauge rainrate values from the file line.
		 * Store only the non-negative values, recorded within the temporal
		 * gauge window, into array 'gauge_rate[]'.
		 */
		gauge_says_rain = 0;     /* 1 if a positive gauge rate */
		nvalid_gauge_rates = 0;  /* Number of positive gauge rates */
		for (j=0; j<ngauge_rates; j++)
		{
			fscanf(fp, "%f", &g_rate);
			/* Discard gauge reading if outside temporal window. */
			if ( (j < temporalWindow_start) || (j > temporalWindow_end) )
			  continue;
			if (g_rate >= 0.0)  /* Discard negative gauge readings. */
			{
				gauge_rate[nvalid_gauge_rates] = g_rate;
				if (g_rate > 0.0) gauge_says_rain = 1;
				nvalid_gauge_rates++;
				if (nvalid_gauge_rates >= ARRAY_SIZE)
				{
					fprintf(stderr, "Too many gauge_rates (> %d)\n", ARRAY_SIZE);
					goto error_exit;
				}
			} /* end if if (g_rate >= 0.0) */
		} /* end for (j=0... */

/*	
fprintf(stderr, "nvalid_cells:%d  nvalid_gauge_rates:%d\n", nvalid_cells,
						nvalid_gauge_rates);
*/
		/* If both radar and gauge report no rain, discard this input line. */
		if ( (!radar_says_rain) && (!gauge_says_rain) )
		  continue;
		

		if (nvalid_cells > 0)  /* Any valid Z values? */
		{

			/*
			 The radar-derived raintypes are not in general the same for all the
			 cells in a VOS_gauge window. Do a majority-rules-decision to
			 establish the overall raintype 'window_rtype' to characterize this
			 window.
			 */
			if (histo->nrtype == 1)  /* UNIFORM Regime */
			{
				window_rtype = 1;
			}
			else if (histo->nrtype == 2)  /* DUAL Regime: Stratiform and Convective */
			{
				nrtype1 = nrtype2 = 0;
				for (j=0; j<nvalid_cells; j++)
				{
					if (cell[j].rtype == 1) nrtype1++;
					else if (cell[j].rtype == 2) nrtype2++;
					else fprintf(stderr, "Unknown raintype:%d in cell.\n", cell[j].rtype);
				}
				if (nrtype1 > nrtype2) window_rtype = 1;
				else window_rtype = 2;
			} /* end else if (histo->nrtype == 2) */
			else
			{
				fprintf(stderr, "Can't accomodate %d raintypes.\n", histo->nrtype);
				goto error_exit;
			}

			/* Add the radar Z values from array 'cell[]' to the appropriate 
				 Z_histogram. */
			irange = index_of_range(histo->range_interval, histo->nrange, gauge_range);
			for (j=0; j<nvalid_cells; j++)
		    zbin[irange][irtype[window_rtype]][(int)((cell[j].z - histo->z_low)/histo->z_res)]++;
		} /* end if (nvalid_cells > 0) */


		if (nvalid_gauge_rates > 0)  /* Any valid gauge rates? */
		{
			/* Average the gauge readings from array 'gauge_rate[]' */
			gauge_avg = 0.0;
			for (j=0; j<nvalid_gauge_rates; j++)
		    gauge_avg = gauge_avg + gauge_rate[j];
			gauge_avg = gauge_avg / nvalid_gauge_rates;
			/* Add the averaged gauge reading to the appropriate R_histogram. */
			irange = index_of_range(histo->range_interval, histo->nrange, gauge_range);
			rbin[irange][irtype[window_rtype]][(int)(gauge_avg*RRATE_SCALE)]++;
		} /* end if (nvalid_gauge_rates > 0) */

	} /* end while (fscanf(... */

	/* Success... */
	fclose(fp);
	return(histo);

 error_exit:
	fprintf(stderr, "Error reading file: %s\n", infile);
	fclose(fp);
	return(NULL);
}

/*************************************************************/
/*                                                           */
/*                      build_zr_histograms                  */
/*                                                           */
/*************************************************************/
ZR_histo *build_zr_histograms(char *infile, float *r, int nrange,
															Window *window)
{
	/* Builds Z-R histograms, using the radar and raingauge data
		 found in 'infile'.

		 Returns:
		     A filled 'ZR_histo' structure, if success.
				 NULL, if failure.
  */
	int irange;
	ZR_histo *histo;

	/*
	 * Create a ZR_histo structure, and fill some slots with the
	 * info that we currently know.
	 */
  histo = (ZR_histo *) calloc(1, sizeof(ZR_histo));
	histo->range_interval = (float *)calloc(nrange, sizeof(float));
	/* Move range values from the array 'r' to the ZR_histo structure. */
	for (irange=0; irange<nrange; irange++)
	  histo->range_interval[irange] = r[irange];
	histo->nrange = nrange;

	histo->nRbins = NR_BINS;
	histo->r_low = 0.0;
	histo->r_hi = NR_BINS / RRATE_SCALE;
	histo->r_res = 1.0 / RRATE_SCALE;

	/*
	 * Read the radar and raingauge values from the intermediate
	 * file 'infile', and build Z_ and R_ histograms.
	 */
	if (read_intermediate_file(histo, infile, window) == NULL)
	{
		free_zr_histo(histo);
		return(NULL);
	}

	return(histo);
}

/*************************************************************/
/*                                                           */
/*                           handler                         */
/*                                                           */
/*************************************************************/
void handler(int sig)
{
  if (verbose)
	fprintf(stderr, "Got signal %d. Abort.\n", sig);
  if (sig == SIGINT || sig == SIGKILL || sig == SIGSTOP) 
	exit (-2);
  exit(-1);
}

/*************************************************************/
/*                                                           */
/*                          usage                            */
/*                                                           */
/*************************************************************/
void usage()
{
	fprintf(stderr, "\nUsage (%s):\n\n", PROG_VERSION);
	fprintf(stderr, "  build_zr_histo [options] inFile outFile\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -v:               Verbose\n");
	fprintf(stderr, "  -c:               Use only the cell over gauge from spatial gauge window.\n");
	fprintf(stderr, "                    Note: A cell has area 4km^2 (2km x 2km)\n");
	fprintf(stderr, "                    (Default: use 9(=3x3) cells from one height)\n");
	fprintf(stderr, "  -h <1.5 or 3.0>:  Use window cells from specified height(km) from 2A-55.\n");
	fprintf(stderr, "                    (Default: range-dependent)\n");
	
	fprintf(stderr, "  -t <mins>:        Temporal gauge window duration. (Default: 6 minutes)\n");
	fprintf(stderr, "  -o <mins>:        Offset temporal gauge window from the input file's\n");
	fprintf(stderr, "                    temporal window center. (Default: 0 minutes)\n");
	
	fprintf(stderr, "  -r <r1,r2,.,150>: Range intervals (km) for Z-R tables\n");
	fprintf(stderr, "  -d:               Disregard rain classification. Lump all classes together\n");
	fprintf(stderr, "\ninFile:  2nd_zr_intermediate_file from 'merge_radarNgauge_data'\n");
	fprintf(stderr, "outFile: output Z-R histograms.\n");
	fprintf(stderr, "\n------------------------------------\n\n");
	
	fprintf(stderr, "Builds a pair of Z-R histograms for each range_interval & rain_type.\n");
	fprintf(stderr, "Produces Z-R histograms based on:\n");
	fprintf(stderr, "  1) Rain classification at the raingauge.\n");
	fprintf(stderr, "  2) Range of the raingauge from the radar site.\n\n");
	fprintf(stderr, "For example, given 2 rain types; say, convective & stratiform,\n");
	fprintf(stderr, "and 3 range intervals; say, [0,50], [50,100], [100,150]\n");
	fprintf(stderr, "then 6 (=3x2) pairs of Z-R histograms will be produced.\n\n");
	exit(-1);
}


extern int getopt(int argc, char * const argv[], const char *optstring);
/*************************************************************/
/*                                                           */
/*                          main                             */
/*                                                           */
/*************************************************************/
int main(int argc, char **argv)
{
	char infile[128], outfile[128];
	int c, i;
	Window window;
	int nrange;         /* Number of gauge range intervals */
	float r[10];        /* gauge range intervals */
	ZR_histo *histo;
	extern char *optarg;
	extern int optind;

	signal(SIGINT, handler);
	signal(SIGFPE, handler);
	signal(SIGKILL, handler);
	signal(SIGILL, handler);
	signal(SIGSTOP, handler);
	signal(SIGSEGV, handler);
	
	if (argc < 3) usage();

	memset(infile, '\0', sizeof(infile));
	memset(outfile, '\0', sizeof(outfile));
	/* 
	 * Defaults
	 */
	nrange = 1;                /* one range interval: 0 -> MAX_RANGE km */
	r[0] = MAX_RANGE;
	window.cell_over_gauge_only = 0;  /* Use all 9 cells */
	window.height = 0.0;              /* range-dependant spatial window height */
	window.duration = 6;              /* Temporal window duration : 6 minutes */
	window.offset = 0.0;              /* temporal window offset */
	window.unclassified = 0;          /* Use rainclasses as found in 2nd int_file */
	

	/* Read options from command line. */
	while ((c=getopt(argc, argv, "vch:t:o:r:d")) != EOF)
	{
		switch (c)
		{
		case 'v':
			verbose = 1;
			break;
		case 'c':
			window.cell_over_gauge_only = 1;
			break;
		case 'h':
			window.height = atof(optarg);
			if ( (window.height != 1.5) && (window.height != 3.0) )
			{
				fprintf(stderr, "\n-h: Invalid window height arg:%f\n\n",
								window.height);
				exit(-1);
			}
			break;
		case 't':
			window.duration = (int) atof(optarg);
			if (window.duration < 1)
			{
				fprintf(stderr, "\n-t: Invalid temporal window:%d\n\n",
								window.duration);
				exit(-1);
			}
			break;
		case 'o':
			window.offset = atof(optarg);
			break;
		case 'r':
			if ((nrange = sscanf(optarg, "%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",
													 r, r+1, r+2, r+3, r+4, r+5, r+6,
													 r+7, r+8, r+9)) == EOF)
			  usage();
			/* If first range value is 0, remove it from the list. */
			if (r[0] == 0.0)
			{
				for (i=1; i<nrange; i++)
				  r[i-1] = r[i];
				nrange--;
			}
			/* Check that range values are in ascending order, and less than
			   MAX_RANGE. */
			for (i=1; i<nrange; i++)
			{
				if (r[i-1] >= r[i]) usage();
				if (r[i-1] > MAX_RANGE) usage();
			}
			/* If last range value is not MAX_RANGE, add MAX_RANGE to the list.*/
			if (r[nrange-1] < MAX_RANGE)
			{
				r[nrange] = MAX_RANGE;
				nrange++;
			}
			break;
		case 'd':    /* Unclassified rain regime */
			window.unclassified = 1;
			break;
		default:  /* Unknown/illegal option */
			fprintf(stderr, "\n");
			exit(-1);
			break;
		}	/* end switch (c) */
	} /* end while ((c=getopt... */

	/* Check for 2 more command line items: infile and outfile. */
	if ((argc - optind) != 2) 
	{
		fprintf(stderr, "\nUnspecified 'inFile' and/or 'outFile'\n\n"); 
		usage();
	}
	strncpy(infile, argv[argc-2], 127);
	strncpy(outfile, argv[argc-1], 127);

	if (verbose)
	{
		fprintf(stderr, "\nCommand Summary\n");
		fprintf(stderr, "---------------------------\n");
		fprintf(stderr, "Spatial window size: ");
		if (window.cell_over_gauge_only)
		  fprintf(stderr, " 1 cell\n");
		else
		  fprintf(stderr, " 9 cells\n");
		if (window.height == 0.0)
		{
			fprintf(stderr, "Window height: 1.5km for   0 < range < 100km\n");
			fprintf(stderr, "               3.0km for 100 < range < 150km\n");
		}
		else
		{
		  fprintf(stderr, "Window height:        %.1f km\n", window.height);
		}
		fprintf(stderr, "Temporal window duration: %3d minutes\n", window.duration);
		fprintf(stderr, "Temporal window offset: %6.2f minutes\n", window.offset);
		fprintf(stderr, "Range intervals (km): ");
		for (i=0; i<nrange; i++)
		  fprintf(stderr, "%.2f  ", r[i]);
		fprintf(stderr, "\n");
		if (window.unclassified)
		  fprintf(stderr, "Disregard rainclasses in input file\n");
		else
		  fprintf(stderr, "Use rainclasses from 2nd interm file\n");
		fprintf(stderr, "inFile:               %s\n", infile);
		fprintf(stderr, "outFile:              %s\n\n", outfile);
	}

	/* Build Z-R tables using the radar & gauge data from infile. */
	histo = (ZR_histo *)build_zr_histograms(infile, r, nrange, &window);
	if (histo == NULL) exit(-1);

  /* Write the Z-R tables to a disk file. */
	if (verbose) fprintf(stderr, "Writing Z-R histograms to file: %s\n", outfile);
	write_zr_histo(histo, outfile);
	free_zr_histo(histo);
	
	/*------ Testing ------- */
/*
	{
		ZR_histo *histo2;
		
		histo2 = (ZR_histo *)read_zr_histo("out1");
		write_zr_histo(histo2, "test");
		free_zr_histo(histo2);
	}
*/
	exit(0);
}
