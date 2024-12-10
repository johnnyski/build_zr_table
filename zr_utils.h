
/******************************************************************

	 -----------------------------------------------------------------

		mike.kolander@trmm.gsfc.nasa.gov  (301) 286-1540
		31 Jul 1998
		Space Applications Corporation
		NASA/TRMM Office
		Copyright (C) 1997

*******************************************************************/


#include "2A53.h"

#define START_COL 30

/* Set numerical resolution of gauge rainrate values to 1/10 mm/hr */
#define RRATE_SCALE 10.0
/* Set the no. of bins for the rainrate histograms:
	 Max rainrate (= 500 mm/hr) times RRATE_SCALE (= 10) equals 5000 */
#define NR_BINS 5000


#define HISTO_STR_LEN 200
typedef struct
{
  char site_name[HISTO_STR_LEN];/* Generic site description.     */
  time_t start_time, stop_time;/* Unix notation -- seconds.      */
  float radar_lat, radar_lon;  /* lat/lon in degrees.            */
  int nrange;                  /* # of range intervals           */
  int nrtype;                  /* # of rain classifications.     */
  int nZbins;                  /* # of Z bins.                   */
	int nRbins;                  /* # of R bins.                   */
  float z_low, z_hi, z_res;    /* range, resolution of Z (dBz)   */
	float r_low, r_hi, r_res;    /* range, resolution of R (mm/hr) */
  char **rain_type_str;        /* 1..nclasses, description of
																	rain classifications, one per
																	record.                        */
  float *range_interval;       /* 0..nrange-1                    */
  int ***z;                    /* Collection of Z histograms
																	z[nrange][nrtype][nZbins]      */
	int ***r;                    /* Collection of R histograms
																	r[nrange][nrtype][nRbins]      */
} ZR_histo;




/*
 * Functions defined in file 'zr_utils.c'.
 */
void print_zr_histo(ZR_histo *histo);
int ***new_histogram_vectors(int nrange, int nrtype, int nbin);
void free_histogram_vectors(int ***histo, int nrange, int nrtype);
void free_zr_histo(ZR_histo *histo);
ZR_histo *read_zr_histo(char *infile);
int write_zr_histo(ZR_histo *histo, char *outfile);


