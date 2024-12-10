
/******************************************************************

	 Mainline driver and assorted functions to scale Z-R tables
	 using scale factor(s) supplied as input.

	 -----------------------------------------------------------------

	 Usage:

  scale_zr_table [options] inFile outFile

	Options:
    -v:               Verbose
    -c <conv_scale>   Convective scale factor
    -s <strat_scale>  Stratiform scale factor
    -u <unif_scale>   Uniform scale factor

  inFile:  input Z_R tables.
  outFile: output Z_R tables.

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

#include "zr_table.h"


#define NO_VAL -9999.0


/* Global variable */
int verbose=0;

/*
 * Functions defined in this file.
 */
Zr_table *scale_zr_tables(Zr_table *zr, float strat_scale,
													float conv_scale, float unif_scale);
void usage();
/* int main(int argc, char **argv) */




/*************************************************************/
/*                                                           */
/*                       scale_zr_tables                     */
/*                                                           */
/*************************************************************/
Zr_table *scale_zr_tables(Zr_table *zr, float strat_scale,
													float conv_scale, float unif_scale)
{
	/* Scale Z-R tables by the given scale factors.

		 Returns:
		     A scaled 'Zr_table' structure, if success.
				 NULL, if failure.
  */
	int irange, irtype, ibin;
	float scale;

	for (irtype=0; irtype<zr->nrtype; irtype++)
	  for (irange=0; irange<zr->nrange; irange++)
		{
			scale = NO_VAL;
			if (strcmp(zr->rain_type_str[irtype], "Stratiform") == 0)
				scale = strat_scale;
			else if (strcmp(zr->rain_type_str[irtype], "Convective") == 0)
			  scale = conv_scale;
			else if (strcmp(zr->rain_type_str[irtype], "Uniform") == 0)
			  scale = unif_scale;
			/*
			 * If the user didn't supply a scale factor for this rtype, abort.
			 */
			if (scale == NO_VAL)
			{
				fprintf(stderr, "Given scale factor type(s) don't match input Z_R table(s)\n");
				fprintf(stderr, "A scale factor must be supplied for each rainclass\n");
				fprintf(stderr, "found in the input Z_R tables.\n\n");
				return(NULL);
			}
			/*
			 * Want R negative if ZR table range > 100km
			 */
			if (zr->range_interval[irange] > 100.0)
			  if (scale > 0.0)
				  scale = -scale;
			/*
			 * Scale each valid R by the scale factor.
			 */
			for (ibin=0; ibin<zr->ndbz; ibin++)
			  if (abs(zr->r[irange][irtype][ibin]) < 9000)
			    zr->r[irange][irtype][ibin] = zr->r[irange][irtype][ibin] * scale;
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
	fprintf(stderr, "  scale_zr_table [options] inFile outFile\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -v:               Verbose\n");
	fprintf(stderr, "  -c <conv_scale>   Convective scale factor\n");
	fprintf(stderr, "  -s <strat_scale>  Stratiform scale factor\n");
	fprintf(stderr, "  -u <unif_scale>   Uniform (1 rainclass) scale factor\n");
	
	fprintf(stderr, "\ninFile:  input Z_R tables.\n");
	fprintf(stderr, "outFile: output Z_R tables.\n\n");
	fprintf(stderr, "Scales the input Z_R tables by the user-supplied scale\n");
	fprintf(stderr, "factors. A scale factor must be supplied for each\n");
	fprintf(stderr, "rainclass found in the input Z_R tables.\n");
	fprintf(stderr, "\n------------------------------------\n\n");
	
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
	Zr_table *zr;
	float conv_scale = NO_VAL;
	float strat_scale = NO_VAL;
	float unif_scale = NO_VAL;
	extern char *optarg;
	extern int optind;

	signal(SIGINT, handler);
	signal(SIGFPE, handler);
	signal(SIGKILL, handler);
	signal(SIGILL, handler);
	signal(SIGSTOP, handler);
	signal(SIGSEGV, handler);
	
	if (argc < 4) usage();

	memset(infile, '\0', sizeof(infile));
	memset(outfile, '\0', sizeof(outfile));

	/* Read options from command line. */
	while ((c=getopt(argc, argv, "vc:s:u:")) != EOF)
	{
		switch (c)
		{
		case 'v':
			verbose = 1;
			break;
		case 'c':
			conv_scale = atof(optarg);
			break;
		case 's':
			strat_scale = atof(optarg);
			break;
		case 'u':
			unif_scale = atof(optarg);
			break;
		default:  /* Unknown/illegal option */
			fprintf(stderr, "\n");
			exit(-1);
			break;
		}	/* end switch (c) */
	} /* end while ((c=getopt... */

	/* User must have specified at least one scale factor. */
	if (strat_scale == NO_VAL)
	  if (conv_scale == NO_VAL)
	    if (unif_scale == NO_VAL)
			{
				fprintf(stderr, "Must specify a scale factor.\n");
				usage();
			}

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
		if (strat_scale != NO_VAL)
		  fprintf(stderr, "strat_scale:  %.2f\n", strat_scale);
		if (conv_scale != NO_VAL)
		  fprintf(stderr, "conv_scale:   %.2f\n", conv_scale);
		if (unif_scale != NO_VAL)
		  fprintf(stderr, "unif_scale:   %.2f\n", unif_scale);
		fprintf(stderr, "inFile:       %s\n", infile);
		fprintf(stderr, "outFile:      %s\n\n", outfile);
	}		

	if (verbose)
	  fprintf(stderr, "Reading input Z-R tables from file: %s\n", infile);
	zr = (Zr_table *) read_ZR(infile);
	if (zr == NULL) exit(-1);
	/*
	 * Scale the Z-R tables using the input scale factor(s).
	 */
	zr = (Zr_table *)scale_zr_tables(zr, strat_scale, conv_scale, unif_scale);
	if (zr == NULL) exit(-1);
  /*
	 * Write the scaled Z-R tables to a disk file.
	 */
	if (verbose)
	  fprintf(stderr, "Writing scaled Z-R tables to file: %s\n", outfile);
	write_ZR(zr, outfile);

	exit(0);
}
