
/******************************************************************

	 Mainline driver and assorted functions to build a Z-R table
	 for each pair of histograms (Z, R) found in the input file.

	 -----------------------------------------------------------------

	 Usage:

	 build_pmm_zr_table [options] inFile outFile

	 Options:
	     -v:               Verbose

	 inFile:   ZR_histograms
	 outFile:  ZR_tables.

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




/* Global variable */
int verbose=0;

/*
 * Functions defined in this file.
 */
float *zr_via_pmm(int zbin[], int nZbins, int rbin[], int nRbins);
Zr_table *build_pmm_zr_tables(ZR_histo *histo);
void usage();
/* int main(int argc, char **argv) */



/*************************************************************/
/*                                                           */
/*                       zr_via_pmm                          */
/*                                                           */
/*************************************************************/
float *zr_via_pmm(int zbin[], int nZbins, int rbin[], int nRbins)
{
	/* Given a histogram of radar reflectivity values Z, and a histogram
		 of raingauge rainrate values R, creates a Z-R vector using
		 the Window_Probability_Matching_Method.

		 Uses each histogram to generate a cumulative distribution
		 function (cdf).

		 For each Z value (there are 'nZbins' such values),
		 determines the corresponding gauge rainrate value R by matching
		 the two cdfs; ie,

		 if cdf(Z[j]) = cdf(R[k]) , then Z[j] is paired with R[k].

		 Returns:
		   a Z-R vector; ie, a vector containing 'nZbins' rainrate values.
			 NULL, if failure.
	*/
	int ir, iz;
	float *r;
	float *cdf_z;        /* CDF vector for Z */
	float *cdf_r;        /* CDF vector for R */
	

	/* Allocate and initialize a vector of R values. */
	r = (float *)calloc(nZbins, sizeof(float));
	for (iz=0; iz<nZbins; iz++)
	  r[iz] = -32767.0;

	/* Create a cdf(Z), using the Z_histogram. */
	cdf_z = (float *) calloc(nZbins, sizeof(float));
	cdf_z[0] = (float) zbin[0];
	for (iz=1; iz<nZbins; iz++)
		cdf_z[iz] = cdf_z[iz-1] + zbin[iz];
	/* cdf_z[nZbins-1] contains the total number of Z observations.
		 It better be non-zero. */
	if (cdf_z[nZbins-1] == 0.0)
	{
		fprintf(stderr, "zr_via_pmm(): Empty Z_histogram.\n");
		return(NULL);
	}
	if (verbose)
	  fprintf(stderr, "Total number of Z values: %d\n", (int)cdf_z[nZbins-1]);
	/* Normalize cdf(Z). */
	for (iz=0; iz<nZbins; iz++)
	  cdf_z[iz] = cdf_z[iz] / cdf_z[nZbins-1];

	/* Create a cdf(R), using the R_histogram. */
	cdf_r = (float *) calloc(nRbins, sizeof(float));
	cdf_r[0] = (float) rbin[0];
	for (ir=1; ir<nRbins; ir++)
		cdf_r[ir] = cdf_r[ir-1] + rbin[ir];
	/* cdf_r[nRbins-1] contains the total number of R observations.
	   It better be non-zero. */
	if (cdf_r[nRbins-1] == 0.0)
	{
		fprintf(stderr, "zr_via_pmm(): Empty R_histogram.\n");
		return(NULL);
	}
	if (verbose)
		fprintf(stderr, "Total number of R values: %d\n", (int)cdf_r[nRbins-1]);
	/* Normalize cdf(R). */
	for (ir=0; ir<nRbins; ir++)
	  cdf_r[ir] = cdf_r[ir] / cdf_r[nRbins-1];

	/* Now, loop to match each Z with an R... */
	ir = 0;
	for (iz=0; iz<nZbins; iz++)  /* For each Z value... */
	{
		/* Search for the minimum R such that cdf(R) >= cdf(Z) */
		while (cdf_r[ir] < cdf_z[iz])
		  ir++;
		r[iz] = (float)(ir) / RRATE_SCALE;  /* Store the R in the Z-R vector. */
		/* If no higher Z values were observed, we're finished. */
		if (cdf_z[iz] == cdf_z[nZbins-1]) break;
	} /* end for (iz=0... */
	/*
	 * Free the CDF vectors, and exit.
	 */
	free(cdf_z);
	free(cdf_r);
	return(r);
}

/*************************************************************/
/*                                                           */
/*                       build_pmm_zr_tables                 */
/*                                                           */
/*************************************************************/
Zr_table *build_pmm_zr_tables(ZR_histo *histo)
{
	/* Builds Z-R tables, using the histograms.

		 Returns:
		     A filled 'Zr_table' structure, if success.
				 NULL, if failure.
  */
	int irange, irtype;
	Zr_table *zr;

	/*
	 * Create a Zr_table structure, and initialize it using info
	 * from the ZR_histo structure.
	 */
  zr = (Zr_table *) calloc(1, sizeof(Zr_table));
	if (zr == NULL) return(NULL);
	strncpy(zr->site_name, histo->site_name, HISTO_STR_LEN);
	memcpy(&zr->start_time, &histo->start_time, sizeof(time_t));
	memcpy(&zr->stop_time, &histo->stop_time, sizeof(time_t));
	zr->radar_lat = histo->radar_lat;
	zr->radar_lon = histo->radar_lon;
	zr->nrange = histo->nrange;
	zr->nrtype = histo->nrtype;
	zr->ndbz = histo->nZbins;
	zr->dbz_low = histo->z_low;
	zr->dbz_hi = histo->z_hi;
	zr->dbz_res = histo->z_res;
	
	/* Copy the raintype strings from the histogram to the ZR_table. */
	zr->rain_type_str = (char **)calloc(histo->nrtype, sizeof(char *));
	if (zr->rain_type_str == NULL) return(NULL);
	for (irtype=0; irtype<histo->nrtype; irtype++)
	  zr->rain_type_str[irtype] = (char *)strdup(histo->rain_type_str[irtype]);

	/* Copy range intervals from the histogram to the ZR_table. */
	zr->range_interval = (float *)calloc(histo->nrange, sizeof(float));
	if (zr->range_interval == NULL) return(NULL);
	for (irange=0; irange<histo->nrange; irange++)
	  zr->range_interval[irange] = histo->range_interval[irange];

	/* For each range interval and raintype class, construct a Z-R
		 vector from the Z_ and R_histograms using the Probability Matching
		 Method. */
	zr->r = (float ***)calloc(zr->nrange, sizeof(float **));
	if (zr->r == NULL) return(NULL);
	for (irange=0; irange<zr->nrange; irange++)
	{
		zr->r[irange] = (float **)calloc(zr->nrtype, sizeof(float *));
		if (zr->r[irange] == NULL) return(NULL);
	  for (irtype=0; irtype<zr->nrtype; irtype++)
		{
			/* Construct a Z-R vector from this pair of Z_ and R_histograms.*/
		  zr->r[irange][irtype] =
			    (float *) zr_via_pmm(histo->z[irange][irtype], histo->nZbins,
															 histo->r[irange][irtype], histo->nRbins);
			if (zr->r[irange][irtype] == NULL) return(NULL);
		} /* end for (irtype=0... */
	} /* end for (irange=0... */

	return(zr);
}

/**********************************************************************/
/*                                                                    */
/*                           handler                                  */
/*                                                                    */
/**********************************************************************/
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
	fprintf(stderr, "  build_pmm_zr_table [options] inFile outFile\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -v:               Verbose\n");

	fprintf(stderr, "\ninFile:  ZR_histogram file\n");
	fprintf(stderr, "outFile:   output Z-R tables.\n");
	fprintf(stderr, "\n------------------------------------\n\n");
	
	fprintf(stderr, "Builds a Z-R table for each pair of histograms (Z, R) found \n");
	fprintf(stderr, "in the input file.\n");
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
	int c;
	ZR_histo *histo;
	Zr_table *zr;
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

	/* Read options from command line. */
	while ((c=getopt(argc, argv, "v")) != EOF)
	{
		switch (c)
		{
		case 'v':
			verbose = 1;
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
		fprintf(stderr, "inFile:               %s\n", infile);
		fprintf(stderr, "outFile:              %s\n\n", outfile);
	}

	/*
	 * Read the Z-R histograms from infile.
	 */
	histo = (ZR_histo *) read_zr_histo(infile);
	/*
	 * Build Z-R tables using the Z-R histograms.
	 */
	zr = (Zr_table *)build_pmm_zr_tables(histo);
	if (zr == NULL) exit(-1);

  /* Write the Z-R tables to a disk file. */
	if (verbose) fprintf(stderr, "Writing Z-R tables to file: %s\n", outfile);
	write_ZR(zr, outfile);

	exit(0);
}
