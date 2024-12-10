
/******************************************************************

	 Mainline driver and assorted functions to merge ZR histograms
	 found in the input files.

	 ------------------------------------

	 Usage:

	 merge_zr_histo [options] inFile1 [inFile2...] outFile

	 Options:
	   -v:               Verbose

	 inFiles: Input ZR histogram files
	 outFile: Output ZR histogram file

	 ------------------------------------

	 Merges ZR histogram pairs from 2 or more input files into
	 one cumulative ZR histogram pair set.  The structure of all
	 input histograms (rainclasses, range intervals, resolution
	 and range of the Z and R axes) must be identical.

	 -----------------------------------------------------------------

		mike.kolander@trmm.gsfc.nasa.gov  (301) 286-1540
		31 Jul 1998
		Space Applications Corporation
		NASA/TRMM Office
		Copyright (C) 1997

*******************************************************************/



#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "zr_utils.h"



#define OK     0
#define ABORT -1

/* Global variable */
int verbose=0;


/*
 * Functions defined in this file.
 */
int *add_vectors(int *inVector, int *outVector, int nbins);
int compatibilityCheck(ZR_histo *histo1, ZR_histo *histo2);
ZR_histo *merge_zr_histograms(char **infile, int nInfiles);
void usage();
int main(int argc, char **argv);



/*************************************************************/
/*                                                           */
/*                        add_vectors                        */
/*                                                           */
/*************************************************************/
int *add_vectors(int *inVector, int *outVector, int nbins)
{
	/* Performs vector addition:
	 *    outVector = inVector + outVector
	 *
	 * Returns outVector.
	 */
	int ibin;

	for (ibin=0; ibin<nbins; ibin++)
	  outVector[ibin] = outVector[ibin] + inVector[ibin];

	return(outVector);
}

/*************************************************************/
/*                                                           */
/*                    compatibilityCheck                     */
/*                                                           */
/*************************************************************/
int compatibilityCheck(ZR_histo *histo1, ZR_histo *histo2)
{
	/*
	 * Check that all header entries in the two ZR_histo structures
	 * are identical.
	 *
	 * Returns: OK, if identical.
	 *          ABORT, else.
	 */
	int irtype, irange;

	if (strcmp(histo1->site_name, histo2->site_name) != 0) return(ABORT);
	if (histo1->radar_lat != histo2->radar_lat) return(ABORT);
	if (histo1->radar_lon != histo2->radar_lon) return(ABORT);
	if (histo1->nrange != histo2->nrange) return(ABORT);
	if (histo1->nrtype != histo2->nrtype) return(ABORT);
	if (histo1->nZbins != histo2->nZbins) return(ABORT);
	if (histo1->nRbins != histo2->nRbins) return(ABORT);
	if (histo1->z_low != histo2->z_low) return(ABORT);
	if (histo1->z_hi != histo2->z_hi) return(ABORT);
	if (histo1->z_res != histo2->z_res) return(ABORT);
	if (histo1->r_low != histo2->r_low) return(ABORT);
	if (histo1->r_hi != histo2->r_hi) return(ABORT);
	if (histo1->r_res != histo2->r_res) return(ABORT);
	for (irtype=0; irtype<histo1->nrtype; irtype++)
	  if (strcmp(histo1->rain_type_str[irtype],
							 histo2->rain_type_str[irtype]) != 0)
		  return(ABORT);
	for (irange=0; irange<histo1->nrange; irange++)
	  if (histo1->range_interval[irange] != histo2->range_interval[irange])
	    return(ABORT);

	return(OK);
}

/*************************************************************/
/*                                                           */
/*                     merge_zr_histograms                   */
/*                                                           */
/*************************************************************/
ZR_histo *merge_zr_histograms(char **infile, int nInfiles)
{
	int j, irtype, irange;
	ZR_histo *inHisto, *outHisto;
	
	/*
	 * Read in the first set of input histograms.
	 */
	outHisto = (ZR_histo *)read_zr_histo(infile[0]);

	for (j=1; j<nInfiles; j++)
	{
		inHisto = read_zr_histo(infile[j]);
		/*
		 * Check that the input Z_R histogram header entries are identical
		 * with those already encountered in the previous infiles.
		 */
		if (compatibilityCheck(inHisto, outHisto) != OK)
		{
			fprintf(stderr, "Input Z_R histograms are not compatible!!\n");
			return(NULL);
		}

		/*
		 * For each raintype and range interval, add the input R vector to
		 * the output R vector.  Similiarly, add the input Z vector to the
		 * output Z vector.
		 */
		for (irtype=0; irtype<outHisto->nrtype; irtype++)
		{
			for (irange=0; irange<outHisto->nrange; irange++)
			{
				/* Add the R vectors, place the result in outHisto->r */
				outHisto->r[irange][irtype] = add_vectors(inHisto->r[irange][irtype],
																									outHisto->r[irange][irtype],
																									outHisto->nRbins);
				/* Add the Z vectors, place the result in outHisto->z */
				outHisto->z[irange][irtype] = add_vectors(inHisto->z[irange][irtype],
																									outHisto->z[irange][irtype],
																									outHisto->nZbins);
			}
		}
		
		free_zr_histo(inHisto);
	} /* end for (j=1; j<nInfiles; j++) */
	
	return(outHisto);
}

/*************************************************************/
/*                                                           */
/*                          usage                            */
/*                                                           */
/*************************************************************/
void usage()
{
	fprintf(stderr, "\nUsage (merge_zr_histo-v0.0):\n\n");
	fprintf(stderr, "  merge_zr_histo [options] inFile1 [inFile2...] outFile\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -v:               Verbose\n");

	fprintf(stderr, "\ninFiles: Input ZR histogram files\n");
	fprintf(stderr, "outFile: Output ZR histogram file\n");
	fprintf(stderr, "\n------------------------------------\n\n");

	fprintf(stderr, "Merges ZR histogram pairs from 2 or more input files into\n");
	fprintf(stderr, "one cumulative ZR histogram pair set.  The structure of all\n");
	fprintf(stderr, "input histograms (rainclasses, range intervals, resolution\n");
	fprintf(stderr, "and range of the Z and R axes) must be identical.\n");
	fprintf(stderr, "\n\n\n");
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
	char *infile[16];     /* Maximum of 16 input files */
	int nInfiles;         /* Number of user-supplied input files. */
	char *outfile;	
	int c, j;
	
	ZR_histo *histo;
	extern char *optarg;
	extern int optind;

	if (argc < 3) usage();

	/* Read options from command line. */
	while ((c=getopt(argc, argv, "v")) != EOF)
	{
		switch (c)
		{
		case 'v':
			verbose = 1;
			break;
		}	/* end switch (c) */
	} /* end while ((c=getopt... */

	/* Check for 2 or more command line items: infiles and outfile. */
	if ((argc - optind) < 2) 
	{
		fprintf(stderr, "\nUnspecified 'inFile' and/or 'outFile'\n\n"); 
		usage();
	}

	nInfiles = argc - optind - 1;
	/* Load the array of ptrs to the input files. */
	for (j=0; j<nInfiles; j++)
	  infile[j] = argv[optind++];
	/* Output file */
	outfile = argv[optind];

	if (verbose)
	{
		fprintf(stderr, "\nCommand Summary\n");
		fprintf(stderr, "---------------------------\n");
		fprintf(stderr, "%d InFiles:\n", nInfiles);
		for (j=0; j<nInfiles; j++)
		  fprintf(stderr, "  %s\n", infile[j]);
		fprintf(stderr, "OutFile: %s\n\n", outfile);
	} /* end if (verbose) */
	
	/*
	 * Read the ZR histogram pairs from each of the infiles, and merge
	 * them into one output ZR histogram pair set.
	 */
	histo = (ZR_histo *)merge_zr_histograms(infile, nInfiles);
	if (histo == NULL) exit(-1);
	/*
	 * Write the output ZR histogram pairs to a disk file.
	 */
	write_zr_histo(histo, outfile);

	exit(0);
}


