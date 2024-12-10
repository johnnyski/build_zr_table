/*
 *  get_2A53_data_over_gauge.c:
 *
 *      This program will create a file similiar to the 
 *      first intermediate product, that created by
 *      get_radar_data_over_gauge.  This program is nearly
 *      identical to that program, except I substitute
 *      the 2A-53 for 2A-55.  Thus, Z is replaced by rainrate in mm/hr (R).
 *   
 *  Execute the following to generate 2A-53/gauge plots:
 *
 *       get_2A53_data_over_gauge 2A53.980207.1.MELB.1.HDF zzzz.gdbm
 *          (Execute the above for as many 2A-53 granules you want.
 *           Data is added to zzzz.gdbm)
 *
 *       first2ascii zzzz.gdbm > zzzz.ascii
 *
 *       merge_radarNgauge_data -f /rain2/merritt/melb_gauge.gdbm zzzz.ascii zzzz_2nd.ascii
 *
 *   --<Filter out unwanted gauges; this is known from previous QC.>--
 *       gauge_filter.pl -f gauge.filter zzzz_2nd.ascii > zzzz_3rd.ascii
 *
 *   --<Now for the plotting>--
 *
 *       gauge_radar_plot_batch.pl -f gaugefilter.file -R zzzz_3rd.ascii
 *          -or-
 *       gauge_gui_main.pl     (interactive: use zzzz_3rd.ascii)
 *-------------------------------------------------------------------------
 *
 * The output of this program is a database using the GNU gdbm library.
 * Several key entries are present and are noted here (they are keys):
 *    1. START TIME
 *    2. END TIME
 *    3. HEADER  ( w/o the START and END time entries. )
 *    4. NOECHO
 *
 * All other entries in the database are keyed as:
 *    1. key <gauge_id, net_id> --->content <ngID>
 *    2. key <ngID, time_sec>   --->content <data described below>
 *
 * The content of these keys are illustrated by 2-tuple of values:
 *     r N H1 n (C1, R1) (C2, R2) ... (Cn, Rn) ... Hn  n (C1, R1) (C2, R2) ... (Cn, Rn)

 *  where:  
 *          r                = Range from radar to gauge.
 *          N                = Number of different height(s).
 *          H                = Height in km
 *          n                = Number of 2 tuples (C Z) followed
 *          C                = rain type
 *          R                = rainrate (mm/hr)
 *
 *--------------------------------------------------------------------------
 * The input to this program is:
 *    1. 2A-53
 *    (No C/S map is necessary.  These are rain rates; the raintype will
 *     be set to 1).
 *
 * The output is:
 *    1. GDBM file containing the 2A-53 data over the gauge.
 *
 *
 *--------------------------------------------------------------------------
 *  Program's exit code:
 *          -1 Failed.
 *           0 Successful.
 *          -2 Program was aborted by SIGINT (^C).
 *     
 *--------------------------------------------------------------------------
 *
 * Implemented by:
 *
 *     John H. Merritt
 *     Space Applications Corporation
 *     NASA/TRMM Office
 *     john.h.merritt@gsfc.nasa.gov
 *
 * Mostly stolen from get_radar_data_over_gauge.c written by:
 *
 *     Ngoc-Thuy Nguyen
 *     Science Systems and Applications, Inc. (SSAI)
 *     NASA/TRMM Office
 *     nguyen@trmm.gsfc.nasa.gov
 *     June 24, 1997
 *
 *       and 
 *     
 *     Mike Kolander
 *     Space Applications Corporation
 *     NASA/TRMM Office
 *     kolander@trmm.gsfc.nasa.gov
 *
 *  Copyright (C) 1997-1998
 *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <IO.h>
#include <IO_GV.h>

#include "get_2A53_data_over_gauge.h"
#include "gv_utils.h"
#include "zr.h"
#include "gauge_db.h"

#if defined (__linux)
#undef PI
#endif
#define PI 3.14159265
#define VOS_MINS      5    /* Time duration of a vos in minutes */

GDBM_FILE zr_rr_fp;
char *this_prog = "get_2A53_data_over_gauge";
extern int getopt(int argc, char * const argv[],
                  const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;
extern int strcasecmp(const char *s1, const char *s2);

int verbose = 0;
static void handler(int sig);
static void process_argv(int argc, char ** argv, float *gauge_win_xmax,
						 float *gauge_win_ymax, float *gauge_win_zmax,
						 char **gauge_top_dir, char *site,
						 rain_class_type_t *rain_class_type,
						 char **rrmap_file, 
						 char ** zr_rr_file);
void free_zc_column(zc_column_t *column);

/**********************************************************************/
/*                                                                    */
/*                              main                                  */
/*                                                                    */
/**********************************************************************/
int main (int argc, char **argv)
{
  float gauge_win_xmax, gauge_win_ymax, gauge_win_zmax;
  rain_class_type_t rain_class_type;
  char *rrmap_file = NULL,
	*zr_rr_file = NULL;
  static Raintype_map *rain_class = NULL;
  int rc = 0;
  IO_HANDLE grrmap_fh;
  L2A_53_SINGLE_RADARGRID rrmap_grid;
  static zc_column_t data_column;
  DATE_STR sdate, edate;
  TIME_STR stime, etime;
  gauge_network_t *gnet_list = NULL, *save_gnet_list, *gnet;
  Gauge_info *gauge;
  Gauge_list *gauges;
  int status;
  char site[MAX_NAME_LEN];
  char site1[MAX_NAME_LEN];
  int nvos;
  int i, g;
  float lat, lon;
  char *gauge_top_dir = NULL;

  signal(SIGINT, handler);
  signal(SIGFPE, handler);
  signal(SIGKILL, handler);
  signal(SIGILL, handler);
  signal(SIGSTOP, handler);
  signal(SIGSEGV, handler);
	
  /* Set default. */
  gauge_win_xmax = 6.0;
  gauge_win_ymax = 6.0;
  gauge_win_zmax = 3.0;
  rain_class_type = SINGLE;
  memset(site, '\0', MAX_NAME_LEN);
  this_prog = argv[0];
  process_argv(argc, argv, &gauge_win_xmax, &gauge_win_ymax, &gauge_win_zmax,
			   &gauge_top_dir, site, &rain_class_type,
			   &rrmap_file, &zr_rr_file);
	
  if (verbose) {
	    fprintf(stderr, "gauge win size x,y,z: %f,%f,%f\n", gauge_win_xmax,
						gauge_win_ymax, gauge_win_zmax);
		fprintf(stderr, "rain class type: %d\n", rain_class_type);
		fprintf(stderr, "2A-53 file: %s, outfile: %s\n", 
						rrmap_file, zr_rr_file);

		
  }
	
  /* Open the 2A-55 HDF file and leave it open untill program exits.
   * Note: We want to open this file in the main line since we need to read
   *      granule info from it and do TKclose on it at the end of the program.
   */
  memset(&grrmap_fh, '\0', sizeof(IO_HANDLE));
  status = TKopen(rrmap_file, TK_L2A_53S, TK_READ_ONLY, &grrmap_fh); 
  if (status != TK_SUCCESS) {
	fprintf(stderr, "TKopen() failed.\n");
	exit(-1);
  }
  memset(&sdate, '\0', sizeof(DATE_STR));
  memset(&edate, '\0', sizeof(DATE_STR));
  memset(&stime, '\0', sizeof(TIME_STR));
  memset(&etime, '\0', sizeof(TIME_STR));
  memset(site1, '\0', MAX_NAME_LEN);
  /* Read some of granule info. (metadata fields) from the hdf file */
  if (read_granule_info_from_hdf(&grrmap_fh, &nvos, &lat, &lon, &sdate,
								 &stime, &edate, &etime, site1) < 0) {
	rc = -1;
	CLOSE_FILES_AND_EXIT(&grrmap_fh, NULL, rc);
  }
	
  if (strlen(site) == 0) strcpy(site, site1);

  /* Open out file for write and write header info there if file doesnot 
   * exist yet; else modify the start/end data/time rows if appropriate.
   */

  zr_rr_fp = open_outfile_and_write_header_info(site, &sdate, &stime,
												&edate, &etime, lat, lon,
												gauge_win_xmax,
												gauge_win_ymax, 
												gauge_win_zmax,
												rain_class_type,
												zr_rr_file);
  if (verbose) {
		fprintf(stderr, "Will extract columns of data for %d-VOS granule.\n", nvos);
  }
  /* Get gauge list for data's site. */
  gnet_list = get_gauge_network_list(gauge_top_dir, site);
  if (gnet_list == NULL) {
		if (verbose) {
			fprintf(stderr, "Error retrieving gauges for site: %s\n", site);
		}
		rc = -1;
		CLOSE_FILES_AND_EXIT(&grrmap_fh, NULL, rc);
  }
  save_gnet_list = gnet_list;
	
  memset(&data_column, '\0', sizeof(zc_column_t));
  rc = initialize_zc_column(&data_column, gauge_win_xmax, gauge_win_ymax,
								   gauge_win_zmax);
  if (rc < 0) {
	CLOSE_FILES_AND_EXIT(&grrmap_fh, NULL, -1);
  }
  else {
	rc  = 0;
  }
  
  memcpy(&data_column.sdate, &sdate, sizeof(DATE_STR));

  
  /* This loop will extract and write columns of radar and rain gauge info, 
   * based on 3D reflectivities and rain gauges, for each VOS from the HDF 
   * file.
   */
  if (verbose)
	  fprintf(stderr, "nvos: %d\n", nvos);
  for (i = 0; i < nvos; i++) {
	    if (verbose) 
	       fprintf(stderr, "Processing vos <%d>\n", i);

		/* Get the next 3D reflectivity grid from the HDF file. */
		if (get_next_3D_field(&grrmap_fh, &rrmap_grid) < 0) {
	    if (verbose) 
	       fprintf(stderr, "Failed to get next 3D field for vos <%d>. Ignore.\n", i);
		  continue;
		}
		rain_class = NULL;
		/* Get rain classifications for this 3D refl grid. */
		rain_class = set_single_raintype();; /* Set explicitly since we're using 2A-53. */
		
		/* For each network, 
		 * extract a column of data over each gauge and write it to file.
		 */
		if (verbose)
		  fprintf(stderr, "Extracting a column of data over gauges...\n");
		gnet = gnet_list;
		while (gnet != NULL) {
			gauges = gnet->gauges;    /* Gauges in network. */
			if (verbose)
			  fprintf(stderr, "Extracting a column of data over gauge net <%s>...\n", gnet->net_name);
			for (g = 0; g < gauges->ngauges; g++) {
				gauge = &(gauges->g[g]);
				if (verbose) {
					fprintf(stderr, "Extracting data column for VOS#:%d gnet:%s gauge#:%s range:%.2f\n",
									i+1, gnet->net_name, gauge->site_id, gauge->range);
				}

				status = extract_column(&data_column, gauge, &rrmap_grid, rain_class);
				if (status < 0)  {
				  if (verbose)
					fprintf(stderr, "extract_column() returns < 0\n");
				  continue;  /* Ignore this column, since gauge range > 150km */
				}
				memcpy(&data_column.stime, &(rrmap_grid.tktime), sizeof(TIME_STR));

				if (verbose) 
					fprintf(stderr, "Appending data column to file\n");
				append_column_to_file(&data_column, gnet->net_name, zr_rr_fp);

			} /* end for (g = 0...*/
			gnet = gnet->next;             /* Go to the next network. */
		} /* while gnet != null */
		

  } /* for each vos */

  free_zc_column(&data_column);
  free_gauge_network_list(save_gnet_list);
  if (rc < 0) {
	if (verbose) {
	  fprintf(stderr, "Failed.\n");
	}
	rc = -1;
  }
  else {
	rc = 0;
	if (verbose) {
	  fprintf(stderr, "Successful.\n");
	}
  }
  CLOSE_FILES_AND_EXIT(&grrmap_fh, NULL, rc); /* Will close the db's */

} /* main */


/**********************************************************************/
/*                                                                    */
/*                             process_argv                           */
/*                                                                    */
/**********************************************************************/
void process_argv(int argc, char ** argv, float *gauge_win_xmax,
				  float *gauge_win_ymax, float *gauge_win_zmax,
				  char **gauge_top_dir, char *site,
				  rain_class_type_t *rain_class_type,
				  char **rrmap_file, 
				  char ** zr_rr_file)

{
  extern char *optarg;
  extern int optind, opterr, optopt;
	
  int c;
  
  if (argc < 3) {
  USAGE:	
		fprintf(stderr,
"Usage (%s): Build 2A-53 first intermediate file.\n"
"     %s  [-v] [-S site_name] \n"
"\t   [-x gauge_win_xmax] [-y gauge_win_ymax] \n"
"\t   [-c rain_class_type] [-g gauge_locations_top_dir] \n"
"\t   2A-53_granule_hdf first_zr_intermediate_outfile\n"
"\n   where:\n"
"     -S     Specify site name. Default: get from 2A-53 file.\n"
"     -x     Specify the X length of the gauge window in km. Default: 6.0\n"
"     -y     Specify the Y length of the gauge window in km. Default: 6.0\n"
"     -c     Specify rain classifications number (Uniform; conv/strat).  \n"
"            Default: conv/strat\n"
"     -g     Default: $GVS_DATA_PATH.  The site locations data files are actually in a\n"
"            subdirectory called 'sitelist', but don't specify that here.\n"
"            Specify the directory where 'sitelist/' resides.\n"
"            The default value for $GVS_DATA_PATH is /usr/local/trmm/GVBOX/data.\n"
"\n"
"     2A-53_granule_hdf             2A-53 rain rate map granule in HDF.\n"
"     first_zr_intermediate_outfile Output filename. This file will be \n"
"                                   appended or created if not exists with radar \n"
"                                   data and rain types for gauges.\n",  PROG_VERSION, argv[0]);
		
		exit(-1);
  }
	
  while ((c = getopt(argc, argv,  ":x:y:c:g:S:v")) != -1) {
		switch (c) {
		case 'S':
		  if (site) strcpy(site, optarg);
		  break;
		case 'x':
			if (sscanf(optarg, "%f", gauge_win_xmax) != 1) {
				fprintf(stderr, "Error: Invalid maximum X length for gauge window.\n");
				exit(-1);
			}
			break;
		case 'y':
			if (sscanf(optarg, "%f", gauge_win_ymax) != 1) {
				fprintf(stderr, "Error: Invalid maximum Y length for gauge window.\n");
				exit(-1);
			}
			break;
		case 'c':
			if (strcasecmp(optarg, "conv/strat") == 0) {
				*rain_class_type = DUAL;
			}
			else if (strcasecmp(optarg, "uniform") == 0) {
				*rain_class_type = SINGLE;
			}
			else {
				fprintf(stderr, "Error: Invalid rain classification type.\n");
				exit(-1);
			}
			break;
		case 'v':
			verbose = 1;
			break;
		case 'g':
			if (optarg[0] == '-') goto USAGE;
			*gauge_top_dir = (char *) strdup(optarg);
			break;
		case '?': fprintf(stderr, "option -%c is undefined\n", optopt);
			goto USAGE;
    case ':': fprintf(stderr, "option -%c requires an argument\n",optopt);
			goto USAGE;
    default: break;
    }
  }
	
  /* Must have 2 files */
  if (argc - optind != 2) goto USAGE;
  *rrmap_file = argv[optind++];
  *zr_rr_file = argv[optind++];
	
  /* Get from environment, if option wasn't specified. */
  if (*gauge_top_dir == NULL) 
	*gauge_top_dir = (char *) strdup(getenv("GVS_DATA_PATH"));

  if (*gauge_top_dir == NULL) 
	*gauge_top_dir = "/usr/local/trmm/GVBOX/data";

} /* process_argv */
/***************************************************************************/
/*                                                                         */
/*                             get_next_3D_field                           */
/*                                                                         */
/***************************************************************************/
int get_next_3D_field(IO_HANDLE *fh, L2A_53_SINGLE_RADARGRID *field)
{
  /* Read the next field (grid) from the openned HDF file (2A-53 product)
   * Return 1 for successful; -1, otherwise.
   */
	int status, rc = 1;
	
  if (fh == NULL || field == NULL)
	return -1;
  
  memset(field, '\0', sizeof(L2A_53_SINGLE_RADARGRID));
  /* Read each grid from HDF file */
  status = TKreadGrid(fh, field);
  if (status != TK_SUCCESS) {
		fprintf(stderr, "TKreadGrid failed\n");
		rc = -1;
  }	
  return rc;
} /* get_next_3D_field */


/***********************************************************************/
/*                                                                     */
/*                      set_single_raintype                            */
/*                                                                     */
/***********************************************************************/
Raintype_map *set_single_raintype(void)
{
  /*
   * Uniform: For single ZR.
   * Copy from '2A53.c'
   */
  Raintype_map *rtype;
  int r, c;
	
  rtype = (Raintype_map *)calloc(1, sizeof(Raintype_map));
  rtype->ix = (int **)calloc(MAX_NROWS, sizeof(int *));
  rtype->xdim = MAX_NROWS;
  rtype->ydim = MAX_NCOLS;
  /* access grid */
  for (r = 0; r < MAX_NROWS; r++) {
	/* All 1's is a good rain-type index, because, the lookup
     * in applyZR subtracts 1. */
	rtype->ix[r] = (int *)calloc(MAX_NCOLS, sizeof(int));
	for (c = 0; c < MAX_NCOLS; c++)
	  rtype->ix[r][c] = 1;
  }
  return rtype;
}


/***************************************************************************/
/*                                                                         */
/*                               csmap2raintype_map                        */
/*                                                                         */
/***************************************************************************/
int csmap2raintype_map(L2A_54_SINGLE_RADARGRID *csmap_grid, 
												Raintype_map *rtmap)
{
  /* Convert csmap in grid format to raintype map.
   * Return 1 for successful; -1, otherwise. 
   */
  int r,c, i;
	
  if (csmap_grid == NULL || rtmap == NULL) return -1;
  
  memcpy(&(rtmap->map_time), &(csmap_grid->tktime), sizeof(TIME_STR));
  rtmap->xdim = MAX_NCOLS;
  rtmap->ydim = MAX_NROWS;
  rtmap->ix = (int **)calloc(rtmap->ydim, sizeof(int));
  if (rtmap->ix == NULL) return -1;
  for (r = 0; r < rtmap->ydim; r++) {
	rtmap->ix[r] = (int *)calloc(rtmap->xdim, sizeof(int));
	if (rtmap->ix[r] == NULL) {
	  /* CLean memory */
	  for (i = 0; i < r; i++) {
		if (rtmap->ix[r])
		  free(rtmap->ix[r]);
	  }
	  return -1;
	}
	for (c = 0; c < rtmap->xdim; c++) {
	  /* convStartFlag = 0 (no rain)
	   *                 1 (Stratiform)
	   *                 2 (Convective)
	   *               MISSING_CS (Missing or bad data)
	   */
	  if (csmap_grid->convStratFlag[r][c] <= -99)
		rtmap->ix[r][c] = MISSING_OR_BAD_DATA_C; /* Change to something smaller to save space */
	  else
		rtmap->ix[r][c] =	csmap_grid->convStratFlag[r][c];
	  
	}
  }
	
  return 1;
} /* csmap2raintype_map */

/***************************************************************************/
/*                                                                         */
/*                        free_gauge_network_list                          */
/*                                                                         */
/***************************************************************************/
void free_gauge_network_list(gauge_network_t *gnet_list)
{
  gauge_network_t *gnet, *tgnet;
	
  if (gnet_list == NULL) return;
  gnet = gnet_list;
  while (gnet) {
		tgnet = gnet;
		gnet = gnet->next;
		free_gauge_list(tgnet->gauges);
		free(tgnet);
  }
} /* free_gauge_list */


/***************************************************************************/
/*                                                                         */
/*                              free_zc_column                             */
/*                                                                         */
/***************************************************************************/
void free_zc_column(zc_column_t *column)
{
  /* Free the space in column structure only--not the column itself.
   */
  int i;

  if (column == NULL) return;

  for (i = 0; i < MAX_HEIGHT_LEVELS && i < column->nhinfo; i++) {
	if (column->hinfo[i] != NULL) {

	  if ((column->hinfo[i])->zc != NULL) {
		free((column->hinfo[i])->zc);
	  }

	  free(column->hinfo[i]);

	}
  }

} /* free_zc_column */

/***************************************************************************/
/*                                                                         */
/*                             free_raintype_map                           */
/*                                                                         */
/***************************************************************************/
void free_raintype_map(Raintype_map *map)
{
  int r;
	
  if (map == NULL) return;
  for (r = 0; r < map->ydim; r++) {
                if (map->ix == NULL) break;
                if (map->ix[r]) free(map->ix[r]);
  }
  if (map->ix) free(map->ix);
  free(map);
  
} /* free_raintype_map */

/***************************************************************************/
/*                                                                         */
/*                           read_granule_info_from_hdf                    */
/*                                                                         */
/***************************************************************************/
int read_granule_info_from_hdf(IO_HANDLE *hdf_fh, int *nvos, float *lat, 
							   float *lon, DATE_STR *sdate, TIME_STR *stime, 
							   DATE_STR *edate, TIME_STR *etime, 
							   char *site)
{ /* Read selected granule info from openned hdf file.
   * Return 1 upon successful; -1, otherwise.
   */
	
  if (TKreadMetadataInt(hdf_fh, TK_NUM_VOS, nvos) == TK_FAIL) 
	return -1;
	
  if (TKreadMetadataChar(hdf_fh, TK_RADAR_NAME, site) == TK_FAIL) 
	return -1;
	
  if (TKreadMetadataFloat(hdf_fh, TK_RADAR_ORIGIN_LAT, lat) == TK_FAIL) 
	return -1;
	
  if (TKreadMetadataFloat(hdf_fh, TK_RADAR_ORIGIN_LON, lon) == TK_FAIL) 
	return -1;
	
  if (TKreadMetadataInt(hdf_fh, TK_BEGIN_DATE, sdate) == TK_FAIL)
	return -1;
	
  if (TKreadMetadataInt(hdf_fh, TK_END_DATE, edate) == TK_FAIL)
	return -1;
	
  if (TKreadMetadataInt(hdf_fh, TK_BEGIN_TIME, stime) == TK_FAIL)
	return -1;
	
  if (TKreadMetadataInt(hdf_fh, TK_END_TIME, etime) == TK_FAIL)
	return -1;
	
  if (TKreadMetadataInt(hdf_fh, TK_NUM_VOS, nvos) == TK_FAIL)
	return -1;
  if ((edate->tkyear == 0 && edate->tkmonth == 0 && edate->tkday == 0) ||
	  (memcmp(edate, sdate, sizeof(DATE_STR)) == 0 && 
	   memcmp(etime, stime, sizeof(TIME_STR)) == 0)) {
	/* Use begin date/time +as end date/time. */
	memcpy(edate, sdate, sizeof(DATE_STR));
	memcpy(etime, stime, sizeof(TIME_STR));
	/* End time of vos = start time + VOS_MINS mins. */
	adjust_dateNtime(edate, etime, VOS_MINS*60);
  }

  return 1;
} /* read_granule_info_from_hdf */

/**********************************************************************/
/*                                                                    */
/*                          initialize_zc_column                      */
/*                                                                    */
/**********************************************************************/
int initialize_zc_column(zc_column_t *column, 
						 float gauge_win_xmax, float gauge_win_ymax,
						 float gauge_win_zmax)
{
	/*
		 Initialize one 'zc_column_t' structure.
		 Assumes 3D grid resolution (in km) dx=2.0 , dy=2.0 , dz=1.5
		 A column is 2 carpis high.
         Return 1 for successful; -1, otherwise.
	*/
	/* Grid resolution */
	float xres=2.0;
	float yres=2.0;
	int i;

	if (column == NULL) return -1;
	column->nx1 = ceil(gauge_win_xmax / xres);
	column->nx2 = ceil(gauge_win_ymax / yres);
	column->nx3 = 1;  /* Column is 1 unit high; 2A-53. */
	column->nhinfo = column->nx3;
	for (i = 0; i < MAX_HEIGHT_LEVELS; i++) {
	  column->hinfo[i] = NULL;
	}
	for (i = 0; i < column->nx3; i++) {
	  column->hinfo[i] = (height_info_t *) calloc(1, sizeof(height_info_t));
	  if (column->hinfo[i] == NULL)	{
		free_zc_column(column);
		return -1;
	  }
	  (column->hinfo[i])->nzc = column->nx1 * column->nx2;
	  (column->hinfo[i])->zc = (zc_t *) calloc((column->hinfo[i])->nzc, sizeof(zc_t));

	  if ((column->hinfo[i])->zc == NULL)	{
		free_zc_column(column);
		return -1;
	  }
	}

	return 1;
}

/**********************************************************************/
/*                                                                    */
/*                           extract_column                           */
/*                                                                    */
/**********************************************************************/
int extract_column(zc_column_t *column, Gauge_info *gauge,
									 L2A_53_SINGLE_RADARGRID *rrmap_grid, 
									 Raintype_map *rain_class)
{
	/*
		 Assumes 3D grid resolution (in km) dx=2.0 , dy=2.0 , dz=1.5

		 No column is generated if gauge range > 150 km

		 Returns: 0, if gauge range < 150 km.
		         -1, otherwise.
	*/
  static int i, ix, iy, iz;
  static int ix_low,   iy_low;
  static int gauge_ix, gauge_iy;  /* 2A55 grid indices of the gauge */

	if (gauge->range >= 150.0) return(-1); /* Ignore gauges past 150 km */
	strncpy(column->gauge_id, gauge->site_id, MAX_NAME_LEN-1);
	column->gauge_range = gauge->range;
	
	/* Find the indices (gauge_ix, gauge_iy) of the 2A55 grid cell
		 corresponding to the gauge location. */
/* Exactly where in the 151x151 grid does SPRINT place the radar??
	 It should be in the middle of the center grid cell (75.5, 75.5), but
	 we're seeing systematic radar_gauge correlation problems.
	 Note: the grid indices in C range from 0 to 150
	gauge_ix = 75.5 + (gauge->range/2.0 * sin(gauge->azimuth*PI/180.0));
	gauge_iy = 75.5 + (gauge->range/2.0 * cos(gauge->azimuth*PI/180.0));
*/
	gauge_ix = 75.0 + (gauge->range/2.0 * sin(gauge->azimuth*PI/180.0));
	gauge_iy = 75.0 + (gauge->range/2.0 * cos(gauge->azimuth*PI/180.0));
	column->c = rain_class->ix[gauge_iy][gauge_ix];


	/* Get the remaining column entries, which surround the gauge. */
	for (iz=0; iz<column->nx3; iz++){
		iy_low = gauge_iy - column->nx2/2.0 + 0.5;
		(column->hinfo[iz])->height = 1.5 * (iz + 1);
		i = 0;
		for (iy=iy_low; iy<(iy_low + column->nx2); iy++){
			ix_low = gauge_ix - column->nx1/2.0 + 0.5;
			for (ix=ix_low; ix<(ix_low + column->nx1); ix++){
				(column->hinfo[iz])->zc[i].c = rain_class->ix[iy][ix];
				if (rrmap_grid->rainRate[iy][ix] <= TK_DEFAULT)
				  (column->hinfo[iz])->zc[i].r = MISSING_Z;
				else
				  (column->hinfo[iz])->zc[i].r = rrmap_grid->rainRate[iy][ix];
				i++;
			} /* end for (ix=... */
		} /* end for (iy=... */
	} /* end for (iz=0... */
	column->gauge_range = gauge->range;

	return(0);
}

/**********************************************************************/
/*                                                                    */
/*                           get_gauge_network_list                   */
/*                                                                    */
/**********************************************************************/
gauge_network_t *get_gauge_network_list(char *gauge_top_dir, char *site)
{
  /* Get gauge network list for the specified site name.
   * Return a pointer to list for successful; NULL, otherwise.
   * The caller needs to deallocate memory for the returned list when
   * finished using it.
   */
  gauge_network_t *gnet_list, *gnet;
  int i, status;
  char *net_id[MAX_GAUGE_NETWORKS];
  int nnet_ids;
  char *radar_id = NULL;
	float radarLat, radarLon;
	
  if (gauge_top_dir == NULL || site == NULL) return NULL;
	
  /* Convert the TSDIS radar site name to the GSL radar site name. */
  radar_id = tsdis_site2gsl_site(site);
  if (radar_id == NULL) {
		fprintf(stderr, "Warning: Site <%s> not found.\n",site);
		return NULL;
  }
  gnet_list = NULL;
  memset(net_id, '\0', MAX_GAUGE_NETWORKS);
  /* Get network IDs for radar site. */
  status = get_gauge_networks_for_radar_site(gauge_top_dir, radar_id,
											 net_id, &nnet_ids, &radarLat, 
											 &radarLon);
  free(radar_id);
  if (status < 0) return(NULL);
	
  /* For each network ID, do:
   *  - allocate new network,
   *  - get a list of gauges
   *  - assign gauges to network.
   *  - add new network to top of gnet_list.
   */
  for (i = 0; i < nnet_ids; i++) {
		gnet = (gauge_network_t *) calloc(1, sizeof(gauge_network_t));
		if (gnet == NULL) {
			perror("Allocate gnet");
			goto ERROR;
		}
		gnet->gauges = get_gauge_sites_info(gauge_top_dir, net_id[i],
																				radarLat, radarLon);
		if (gnet->gauges == NULL) {
			fprintf(stderr, "Warning: There is no gauge for network <%s>\n", net_id[i]);
		}
		if (verbose)
		  fprintf(stderr, "gnet: %s   ngauges: %d\n", net_id[i],
				  gnet->gauges->ngauges);
		
		strncpy(gnet->net_name, net_id[i], MAX_NAME_LEN-1);
		/* Add to top of list */
		gnet->next = gnet_list;
		gnet_list = gnet;
  } /* end for (i = 0... */
  
  return gnet_list;
	
 ERROR:
  free_gauge_network_list(gnet_list);
  return NULL;
} /* get_gauge_network_list */


/**********************************************************************/
/*                                                                    */
/*                          handler                                   */
/*                                                                    */
/**********************************************************************/
void handler(int sig)
{
  fprintf(stderr, "Got signal %d. Abort.\n", sig);
  kill(0, sig);
  if (sig == SIGINT || sig == SIGKILL || sig == SIGSTOP) 
	exit (-2);
  exit(-1);
}

/**********************************************************************/
/*                                                                    */
/*                         clean_up                                   */
/*                                                                    */
/**********************************************************************/
void clean_up()
{
  if (zr_rr_fp)
    gdbm_close(zr_rr_fp);
}


