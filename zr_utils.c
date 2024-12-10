
/******************************************************************

	 Assorted utility functions to build, read, and write Z_R
	 histograms and tables.

	 -----------------------------------------------------------------

	  John Merritt  trmm.gsfc.nasa.gov
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
#include "zr.h"

/*************************************************************/
/*                                                           */
/*                      print_zr_histo                       */
/*                                                           */
/*************************************************************/
void print_zr_histo(ZR_histo *histo)
{
	int irange, irtype, ibin;

	for (irange=0; irange<histo->nrange; irange++)
	{
		for (irtype=0; irtype<histo->nrtype; irtype++)
		{
			fprintf(stderr, "\n\nRainType: %s\n", histo->rain_type_str[irtype]);
			if (irange == 0)
			  fprintf(stderr, "Range(km)                     0.00 %.2f\n",
								histo->range_interval[irange]);
			else
			  fprintf(stderr, "Range(km)                     %.2f %.2f\n",
								histo->range_interval[irange-1], histo->range_interval[irange]);
			fprintf(stderr, "Number of Z_histogram bins: %d\n", histo->nZbins);
			fprintf(stderr, "-------------------------\n");
			for (ibin=0; ibin<histo->nZbins; ibin++)
			  fprintf(stderr, "  zbin[%d]=%d\n", ibin, histo->z[irange][irtype][ibin]);
			fprintf(stderr, "\nNumber of R_histogram bins: %d\n", NR_BINS);
			fprintf(stderr, "-------------------------\n");
			for (ibin=0; ibin<NR_BINS; ibin++)
			  if (histo->r[irange][irtype][ibin] > 0)
				  fprintf(stderr, "  rbin[%d]=%d\n", ibin, histo->r[irange][irtype][ibin]);
		} /* end for (irtype=0... */
	} /* end for (irange=0... */
}

/*************************************************************/
/*                                                           */
/*                     new_histogram_vectors                 */
/*                                                           */
/*************************************************************/
int ***new_histogram_vectors(int nrange, int nrtype, int nbin)
{
	/* Create 'nrange x nrtype' vectors, each of length 'nbin'.
		 A vector will be used to contain a histogram for a particular
		 (range , raintype) classification.
  */
	int ***histo;
	int irange, irtype;
	
	histo = (int ***)calloc(nrange, sizeof(int **));
	for (irange=0; irange<nrange; irange++)
	{
		histo[irange] = (int **)calloc(nrtype, sizeof(int *));
	  for (irtype=0; irtype<nrtype; irtype++)
			histo[irange][irtype] = (int *)calloc(nbin, sizeof(int));
	}
	return(histo);
}

/*************************************************************/
/*                                                           */
/*                   free_histogram_vectors                  */
/*                                                           */
/*************************************************************/
void free_histogram_vectors(int ***histo, int nrange, int nrtype)
{
	/* Frees all memory allocated to the set of 'nrange' x 'nrtype'
		 histogram arrays.
  */
	int i, j;
	
	if (histo == NULL) return;
	
	for (i=0; i<nrange; i++)
	{
		if (histo[i] == NULL) continue;
	  for (j=0; j<nrtype; j++)
		{
			if (histo[i][j] == NULL) continue;
			free(histo[i][j]);
		}
		free(histo[i]);
	}
	free(histo);
}

/*************************************************************/
/*                                                           */
/*                        free_zr_histo                      */
/*                                                           */
/*************************************************************/
void free_zr_histo(ZR_histo *histo)
{
	/*
	 * Frees a ZR_histo structure.
	 */

	if (histo == NULL) return;
	/*
	 * Free the various substructures of the ZR_histo structure.
	 */
	if (histo->rain_type_str != NULL)
	  free(histo->rain_type_str);
	if (histo->range_interval != NULL)
	  free(histo->range_interval);
	/* Free up the Z histograms. There are nrange x nrtype of them. */
	free_histogram_vectors(histo->z, histo->nrange, histo->nrtype);
	/* Free up the R histograms. There are nrange x nrtype of them. */
	free_histogram_vectors(histo->r, histo->nrange, histo->nrtype);
	/*
	 * Finally, free the ZR_histo structure.
	 */
	free(histo);
}

/*************************************************************/
/*                                                           */
/*                         read_zr_histo                     */
/*                                                           */
/*************************************************************/
ZR_histo *read_zr_histo(char *infile)
{

	/* 
		 Read Z and R histograms from a file.
	*/
	char *s;
#define ZR_BUF_SIZE 1024
  char buf[ZR_BUF_SIZE];
  struct tm tm;
  int irange, irtype, idbz, ir;
	ZR_histo *histo;
	FILE *fp;

	putenv("TZ=UT"); /* No localtime conversion. */

	fp = fopen(infile, "r");
	if (fp == NULL) 
	{
		fprintf(stderr, "Error opening file: %s\n", infile);
		return(NULL);
	}
  /* Read down to Z_R_HISTOGRAM_HEADER_SECTION */
  while ((s = fgets(buf, sizeof(buf), fp))) {
	chop(s);
	if (strcmp(buf, "Z_R_HISTOGRAM_HEADER_SECTION") == 0) break;
  }
  /* EOF? That's an error. */
  if (s == NULL) {
	fprintf(stderr, "ZR_histo format error. Can't find Z_R_HISTOGRAM_HEADER_SECTION.\n");
	fclose(fp);
	return NULL;
  }

  histo = (ZR_histo *)calloc(1, sizeof(ZR_histo));
  
#define START_COL 30
  /* Ok, the first 30 char of each record is 
   * descriptive.  Column 31 to \n is what we parse.  Remove the \n.
   */
  s = fgets(buf, sizeof(buf), fp); chop(s); /* site_name */  /* Perl :-) */
  sprintf(histo->site_name, "%s", &s[START_COL]);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* start date/time */
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
  tm.tm_isdst = -1;
  histo->start_time = mktime(&tm);
  
  s = fgets(buf, sizeof(buf), fp); chop(s); /* stop date/time */
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
  tm.tm_isdst = -1;
  histo->stop_time = mktime(&tm);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* lat/lon */
  sscanf(&s[START_COL], "%f %f", &histo->radar_lat, &histo->radar_lon);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nrange  */
  sscanf(&s[START_COL], "%d", &histo->nrange);
	histo->range_interval = (float *)calloc(histo->nrange, sizeof(float));

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nrtype  */
  sscanf(&s[START_COL], "%d", &histo->nrtype);
  histo->rain_type_str = (char **)calloc(histo->nrtype, sizeof(char *));

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nZbins    */
  sscanf(&s[START_COL], "%d", &histo->nZbins);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nRbins    */
  sscanf(&s[START_COL], "%d", &histo->nRbins);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* Z range, res */
  sscanf(&s[START_COL], "%f %f %f", &histo->z_low, &histo->z_hi, &histo->z_res);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* R range, res */
  sscanf(&s[START_COL], "%f %f %f", &histo->r_low, &histo->r_hi, &histo->r_res);

	/* Allocate the 3D array of R_histograms. */
	histo->r = (int ***) new_histogram_vectors(histo->nrange, histo->nrtype,
																						 histo->nRbins);
	/* Allocate the 3D array of Z_histograms. */
	histo->z = (int ***) new_histogram_vectors(histo->nrange, histo->nrtype,
																						 histo->nZbins);
	

	/* Read each of the '2*nrange*nrtype' histograms from the file. */
	for (irtype=0; irtype<histo->nrtype; irtype++)
	{
		for (irange=0; irange<histo->nrange; irange++)
		{
			/*
			 * Z_HISTOGRAM
			 * Read down to Z_HISTOGRAM_HEADER
			 */
			while ((s = fgets(buf, sizeof(buf), fp))) {
				chop(s);
				if (strcmp(buf, "Z_HISTOGRAM_HEADER") == 0) break;
			}
			/* EOF? That's an error. */
			if (s == NULL) {
				fprintf(stderr, "ZR_histo format error.  Can't find Z_HISTOGRAM_HEADER.\n");
				fclose(fp);
				return NULL;
			}
		
			s = fgets(buf, sizeof(buf), fp); chop(s);   /* raintype */
			if (histo->rain_type_str[irtype] == NULL)
			  histo->rain_type_str[irtype] = (char *) strdup(&s[START_COL]);

			s = fgets(buf, sizeof(buf), fp); chop(s); /* range_interval */
			sscanf(&s[START_COL], "%*f %f", &histo->range_interval[irange]);

			/* Z_HISTOGRAM */
			fgets(buf, sizeof(buf), fp); chop(s);
			if (strcmp(s, "Z_HISTOGRAM") != 0) { /* Error. */
				fprintf(stderr, "ZR_histo format error: Expected to find Z_HISTOGRAM.\n");
				fclose(fp);
				return NULL;
			}
			for (idbz=0; idbz<histo->nZbins; idbz++)
				fscanf(fp,"%*f %d\n", &histo->z[irange][irtype][idbz]);

			/*
			 * R_HISTOGRAM
			 * Read down to R_HISTOGRAM_HEADER
			 */
			while ((s = fgets(buf, sizeof(buf), fp))) {
				chop(s);
				if (strcmp(buf, "R_HISTOGRAM_HEADER") == 0) break;
			}
			/* EOF? That's an error. */
			if (s == NULL) {
				fprintf(stderr, "ZR_histo format error. Can't find R_HISTOGRAM_HEADER.\n");
				fclose(fp);
				return NULL;
			}
		
			s = fgets(buf, sizeof(buf), fp); chop(s);   /* raintype */
			if (histo->rain_type_str[irtype] == NULL)
			  histo->rain_type_str[irtype] = (char *) strdup(&s[START_COL]);

			s = fgets(buf, sizeof(buf), fp); chop(s); /* range_interval */
			sscanf(&s[START_COL], "%*f %f", &histo->range_interval[irange]);

			/* R_HISTOGRAM */
			fgets(buf, sizeof(buf), fp); chop(s);
			if (strcmp(s, "R_HISTOGRAM") != 0) { /* Error. */
				fprintf(stderr, "ZR_histo format error: Expected to find R_HISTOGRAM.\n");
				fclose(fp);
				return NULL;
			}
			for (ir=0; ir<histo->nRbins; ir++)
				fscanf(fp,"%*f %d\n", &histo->r[irange][irtype][ir]);
		} /* end for (irange=0... */
	} /* end for (irtype=0... */
	
	fclose(fp);
	return(histo);
}

/*************************************************************/
/*                                                           */
/*                       write_zr_histo                      */
/*                                                           */
/*************************************************************/
int write_zr_histo(ZR_histo *histo, char *outfile)
{
	/* 
		 Write the Z and R histograms to a file.
		 There exists a total of (histo->nrtype x histo->nrange) Z-R tables
		 in the structure 'ZR_histo'.
	*/
	int irange, irtype, idbz, irainrate;
	FILE *fp;
  struct tm *sec_time;
	char time_str[100];

	fp = fopen(outfile, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error opening file: %s\n", outfile);
		return(-1);
	}
  fprintf(fp,"FILE_FORMAT_DESCRIPTION\n");
  fprintf(fp,"For all header sections, the first 30 characters\n");
  fprintf(fp,"of each record are descriptive of the values in that record and\n");
  fprintf(fp,"serve to self-document the header fields.  Each Z_HISTOGRAM begins\n");
  fprintf(fp,"after the Z_HISTOGRAM delimiter.  The appropriate number of pairs\n");
  fprintf(fp,"(dBZ, count) follow.  The R_HISTOGRAMs are analogous, having pairs\n");
	fprintf(fp,"(rainrate, count).\n");


  fprintf(fp,"\nZ_R_HISTOGRAM_HEADER_SECTION\n");
  fprintf(fp,"site_name                     %s\n", histo->site_name);
  sec_time = localtime(&histo->start_time);
  strftime(time_str, sizeof(time_str), "%m/%d/%Y %H:%M:%S", sec_time);
  fprintf(fp,"mm/dd/yyyy hh:mm:ss start     %s\n", time_str);
  sec_time = localtime(&histo->stop_time);
  strftime(time_str, sizeof(time_str), "%m/%d/%Y %H:%M:%S", sec_time);
  fprintf(fp,"mm/dd/yyyy hh:mm:ss stop      %s\n", time_str);
  fprintf(fp,"radar_lat, radar_lon          %f %f\n", histo->radar_lat,
					histo->radar_lon);
  fprintf(fp,"range intervals               %d\n", histo->nrange);
  fprintf(fp,"rain types                    %d\n", histo->nrtype);
  fprintf(fp,"nZbins                        %d\n", histo->nZbins);
  fprintf(fp,"nRbins                        %d\n", histo->nRbins);
  fprintf(fp,"Z_range, resolution (dBZ)     %.2f %.2f %.2f\n", histo->z_low,
					histo->z_hi, histo->z_res);
  fprintf(fp,"R_range, resolution (mm/hr)   %.2f %.2f %.2f\n", histo->r_low,
					histo->r_hi, histo->r_res);

	for (irtype=0; irtype<histo->nrtype; irtype++)
	{
		for (irange=0; irange<histo->nrange; irange++)
		{
			/*
			 * Print out the Z_Histogram.
			 */
			fprintf(fp,"\nZ_HISTOGRAM_HEADER\n");
			fprintf(fp, "raintype                      %s\n",
							histo->rain_type_str[irtype]);
			if (irange == 0)
		    fprintf(fp, "range(km)                     0.00 %.2f\n",
								histo->range_interval[irange]);
			else
		    fprintf(fp, "range(km)                     %.2f %.2f\n",
								histo->range_interval[irange-1], histo->range_interval[irange]);
			fprintf(fp, "Z_HISTOGRAM\n");
			for (idbz=0; idbz<histo->nZbins; idbz++)
			  fprintf(fp, "%5.1f  %d\n", (histo->z_low + idbz*histo->z_res), 
								histo->z[irange][irtype][idbz]);
			fprintf(fp, "\n");

			/*
			 * Print out the R_Histogram.
			 */
			fprintf(fp,"\nR_HISTOGRAM_HEADER\n");
			fprintf(fp, "raintype                      %s\n",
							histo->rain_type_str[irtype]);
			if (irange == 0)
		    fprintf(fp, "range(km)                     0.00 %.2f\n",
								histo->range_interval[irange]);
			else
		    fprintf(fp, "range(km)                     %.2f %.2f\n",
								histo->range_interval[irange-1], histo->range_interval[irange]);
			fprintf(fp, "R_HISTOGRAM\n");
			for (irainrate=0; irainrate<histo->nRbins; irainrate++)
			  fprintf(fp, "%5.1f  %d\n", (histo->r_low + irainrate * histo->r_res), 
								histo->r[irange][irtype][irainrate]);
			fprintf(fp, "\n");
		}
	} /* end for (irtype=0... */
	fclose(fp);
	return(0);
}


