/*
 * Build ZR tables using command line parameters.  This program
 * is an extension of the programs: build_dual_zr.c and build_single_zr.c.
 * This program eliminates the need to manually edit the C program
 * to adjust the ZR parameters.
 *
 * OUTPUT (stdout): zr table compatable w/ gvs 4.12, product 2A-53.
 *
 * The output is really a 4 rain type (class) zr table.  The order
 * of the classes is important because of 2A-53 interpretation.
 * The classes are: MISSING, NOECHO, Stratiform, Convective.
 *
 */

/*
 * Current implementation:
 *
 * 1. Handles Convective and Stratiform classes only.
 * 2. Uses gv_radar_site_info.data for lat/lon.
 * 3. R = kZ^a
 * 
 * Soon to be implemented:
 *
 * 1. -k x1, ..., xn
 *    -a y1, ..., yn
 *    -t t1, ..., tn
 *
 * 2. Range dependancy specification.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include "getopt.h"
#include <string.h>
#include <time.h>
#include "zr_table.h"
#include "rsl.h"

/*---------------------------------------------------------------*/
/*                                                               */
/*                         u s a g e                             */
/*                                                               */
/*---------------------------------------------------------------*/
void usage(int argc, char **argv)
{
  fprintf(stderr,"\n");
  fprintf(stderr,"Usage: (Package: %s)\n", PROG_VERSION);
  fprintf(stderr,"  build_zr_table [--site {KMLB, KWAJ, ...}]  -> Default: MELB\n");
  fprintf(stderr,"     (See /usr/local/trmm/GVBOX/data/gv_radar_site_info.data)\n");
  fprintf(stderr,"                 [--sdate mm/dd/yyyy]  -> Start date. 00/00/0000\n");
  fprintf(stderr,"                 [--stime hh:mm:ss]    -> Start time. 00:00:00\n");
  fprintf(stderr,"                 [--edate mm/dd/yyyy]  -> End   date. 00/00/0000\n");
  fprintf(stderr,"                 [--etime hh:mm:ss]    -> End   time. 00:00:00\n");
  fprintf(stderr,"                 [--maxdbz maxdbz]     -> Maximum allowable dbz.  Default: 60.0\n");
  fprintf(stderr,"                 [--badval_above_max]  -> Use BADVAL for rainrate above\n");
  fprintf(stderr,"                                          maxdbz.  Default: plateau the rainrate\n");
  fprintf(stderr,"                                          corresponding to maxdbz for all dbz\n");
  fprintf(stderr,"                                          values above maxdbz.\n");
  fprintf(stderr,"                 [--minrange minrange] -> Minimum range (KM) from radar for valid\n");
  fprintf(stderr,"                                          data. MISSING for anything closer.\n");
  fprintf(stderr,"                                          Default: 15.");
  fprintf(stderr,"                 [--maxrange maxrange] -> Maximum range (KM) from radar for valid\n");
  fprintf(stderr,"                                          data. MISSING for anything farther.\n");
  fprintf(stderr,"                                          Default: 150.");
  fprintf(stderr,"                 [-k x1 [-k x2 ...]]   -> Default: -k 300.0 -k 300.0\n");
  fprintf(stderr,"                 [-a y1 [-a y2 ...]]   -> Default: -a 1.4 -a 1.4\n");
  fprintf(stderr,"                 [-t t1 [-t t2 ...]]   -> Default: -t Stratiform -t Convective\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"                 [-h | --help]        -> HELP.  Print this usage.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"           -k, -a, and -t should be entered together.  And the number\n");
  fprintf(stderr,"           of parameters should match.  (xn, yn, tn) defines a triplet\n");
  fprintf(stderr,"           of (k, a, and raintype) that specify the function: R=kZ^a.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"           Currently, specify both x1 and x2, or y1 and y2 or t1 and t2.\n");
  fprintf(stderr,"           Currently, -t is not implemented, but, the rain types Stratiform\n");
  fprintf(stderr,"                      and Convective are known (hard coded) and the order\n");
  fprintf(stderr,"                      shown as the default is accurate: t1 = Stratiform\n");
  fprintf(stderr,"                      and t2 = Convective.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"           Soon to be implemented:\n");
  fprintf(stderr,"               -k x1 -k x2 ... -k xn\n");
  fprintf(stderr,"               -a y1 -a y2 ... -a yn\n");
  fprintf(stderr,"               -t t1 -t t2 ... -t tn\n");
  exit(-1);
}

  /* Any changes to the defaults? */
void  process_args(int argc, char **argv,
				   char **site,
				   char **sdate, char **stime,
				   char **edate, char **etime,
				   float *maxdbz, int *badval_above_max,
				   float *minrange, float *maxrange,
				   float k[], float a[], char *t[], int *ntype)
{

  int ki, ai, ti; /* Index for multi-arg parameters. */
  int c;
  int this_option_optind = optind ? optind : 1;
  int long_index = 0;
  static struct option long_options[] =
  {
	{"site",  1, 0, 0},
	{"sdate", 1, 0, 0},
	{"stime", 1, 0, 0},
	{"edate", 1, 0, 0},
	{"etime", 1, 0, 0},
	{"maxdbz", 1, 0, 0},
	{"badval_above_max", 0, 0, 0},
	{"minrange", 1, 0, 0},
	{"maxrange", 1, 0, 0},
	{"help", 0, 0, 0},
	{NULL, 0, 0, 0}
  };

  /* DON'T do the MISSING or NOECHO indexes. */
  ki = ai = ti = 2;
  
  while ((c = getopt_long(argc, argv, "hk:a:t:", long_options, &long_index)) != -1)
	switch (c) {
	case 0:
	  if (strcmp(long_options[long_index].name, "site") == 0)
		*site = (char *)strdup(optarg);
	  else if (strcmp(long_options[long_index].name, "sdate") == 0)
		*sdate = (char *)strdup(optarg);
	  else if(strcmp(long_options[long_index].name, "stime") == 0)
		*stime = (char *)strdup(optarg);
	  else if(strcmp(long_options[long_index].name, "edate") == 0)
		*edate = (char *)strdup(optarg);
	  else if(strcmp(long_options[long_index].name, "etime") == 0)
		*etime = (char *)strdup(optarg);
	  else if(strcmp(long_options[long_index].name, "maxdbz") == 0)
		sscanf(optarg, "%f", maxdbz);
	  else if(strcmp(long_options[long_index].name, "badval_above_max") == 0)
		*badval_above_max = 1;
	  else if(strcmp(long_options[long_index].name, "minrange") == 0)
		sscanf(optarg, "%f", minrange);
	  else if(strcmp(long_options[long_index].name, "maxrange") == 0)
		sscanf(optarg, "%f", maxrange);
	  else if(strcmp(long_options[long_index].name, "help") == 0)
		usage(argc, argv);
	  break;
	case 'k': sscanf(optarg, "%f", &k[ki++]);  break;
	case 'a': sscanf(optarg, "%f", &a[ai++]); break;
	case 't': t[ti++] = (char *)strdup(optarg); break;
	case 'h': usage(argc, argv); break;
	case '?': usage(argc, argv); break;
	default:  break;
	}

  *ntype = 4; /* MISSING, NOECHO, Stratiform, Convective. */

  if (ti != 2) {
	fprintf(stderr, "-t not implemented yet.\n");
	usage(argc, argv);
  }
  
  if (ki != 2 && ki != 4) {
	fprintf(stderr, "Number of -k options must be 2.\n");
	usage(argc, argv);
  }
  
  if (ai != 2 && ai != 4) {
	fprintf(stderr, "Number of -a options must be 2.\n");
	usage(argc, argv);
  }
  
}

/*---------------------------------------------------------------*/
/*                                                               */
/*                          m a i n                              */
/*                                                               */
/*---------------------------------------------------------------*/
int main(int argc, char **argv)
{
  Zr_table *zr;
  float dbz;
  float k[4], a[4];
  char  *t[4]; /* Default: "MISSING", "NOECHO", "Stratiform", "Convective" */
 
 float r;
 struct tm stm, etm; /* Start = stm, End = etm */
 
 char *the_site,
	*sdate, *stime,
	*edate, *etime;
 float maxdbz;
 int badval_above_max;
 float minrange, maxrange;
  int ntype;
	
  int i,j,n;

  /* Index for 'k' is the raintype: MISSING, NOECHO, Stratiform, Convective. */

  /* Defaults: -k, -a */
  the_site = sdate = stime = edate = etime = NULL;
  maxdbz = 60.0;
  badval_above_max = 0;
  minrange = 15.0;
  maxrange = 150.0;
  ntype = 0;

  k[0] = 0; /* MISSING -- This won't change from the command line. */
  k[1] = 0; /* NOECHO  -- This won't change from the command line. */
  k[2] = 300;     /* Stratiform FACE*/
  k[3] = 300;     /* Convective FACE*/

  a[0] = 0.0;
  a[1] = 0.0;
  a[2] = 1.4;     /* Stratiform FACE*/
  a[3] = 1.4;     /* Convective FACE*/

  /* Building the ZR table is fairly easy and it is composed of two
   * basic steps:
   *
   *    1. Fill header information: date, lat/lon, dimensions, etc.
   *    2. Fill the 3D table with numbers.  The 3D table is indexed
   *       by range, rain-type, and dBz.
   */

  /* The routine 'write_ZR' outputs a ZR table that 2A-53 reads.  No
   * other interface should be used as the routine 'read_ZR' is 
   * the counter-part to 'write_ZR'.  Whatever, 'write_ZR' generates
   * is acceptable to 'read_ZR'.
   */


  /* BEGIN: Load the header section. */
  zr = (Zr_table *)calloc(1, sizeof(Zr_table));

  /* This is the site name.  It can be up to 200 characters and should
   * be representative of the location, or site, for the ZR table.
   * This can be any string.
   */
  sprintf(zr -> site_name,"MELB"); /* Default -site */

  /* Get this from gv_radar_site_info.data in /usr/local/trmm/GVBOX/data,
   * if it exists.
   */
  
  zr -> radar_lat  = 28.113333;   /* MELB */
  zr -> radar_lon  = -80.654167;  /* MELB */

  /* Enter the time range for the ZR table.  You should specify
   * the range of time, of the data, used to construct the ZR table.
   * The time is in Unix 'time' format which is the number of seconds
   * from Jan 1, 1970.  To properly define these members, use mktime();
   *
   * Unix provides functions to do a conversion of the usual mm/dd/yy
   * hh:mm:ss format.  Just load the 'tm' structure as shown in the
   * example below.  For the time functions to work '#include <time.h>'
   * and the proper definition of 'tm' is required.
   */
  
  stm.tm_mon  = 0;    /* Month 1 - 12 */
  stm.tm_mday = 0;    /* Day   1 - 31 */
  stm.tm_year = 0;    /* Year  19xx   */
  stm.tm_hour = 0;    /* Hour  0 - 23 */   
  stm.tm_min  = 0;    /* Min   0 - 59 */
  stm.tm_sec  = 0;    /* Sec   0 - 59 */
  
  stm.tm_year -= 1900; /* I automatically correct the century for mktime */
  stm.tm_mon --;       /* I automatically correct the month for mktime */
  stm.tm_isdst = -1;    /* Don't use daylight savings time. */

  etm = stm;
  zr -> start_time = mktime(&stm);
  zr -> stop_time  = mktime(&etm);

  /* Here, define the range of dBz values that are present in the 3-D
   * ZR table.  The range divided by the resolution define the number
   * of dBz indexes for the ZR table.
   */
  /* NOT YET IMPLEMENTED via the command line arguments. */
  zr -> dbz_low = -15.0;
  zr -> dbz_hi  =  84.0;
  zr -> dbz_res =   0.5;
  
  /* The dimensions of the ZR table are defined with the following 
   * three members.  You'll notice that the member 'ndbz' is calculated
   * from the dBz range and resolution above. 
   *
   * 'nrange' defines the number of range indexes.  This is used to
   * store any range dependency for the ZR table.  Each index covers
   * some predefined range interval.  For instance, if the range intervals
   * for the ZR table are 0, 10, 25, 40.  Then, index=0 covers the range
   * interval 0 through 10.  index=1 is for 10 to 25.  index=2 is for
   * 25 to 40.  And, index=3 is for 40 to infinity.  Range intervals
   * are placed in the 'range_interval' member which is explained later.
   *
   * A 'nrange == 1' means that there is no range dependency.
   */
  zr -> nrange = 3; /* Range dependency. */

  /* 'nrtype' defines the number of rain-types or number of rain
   * classifications.  A single ZR would set this to 3.  A dual ZR, one
   * suitable for a convective/stratiform rain classification, would set
   * this to 4.  Later, you will be setting 'rain_type_str' to 
   * a string representing the rain-type.  You ALWAYS need the classes:
   * MISSING and NOECHO.
   */
  zr -> nrtype = 4; /* MISSING, NOECHO, S, C */

  /* 'ndbz' is computed and is the number of dBz bins.
   * Basically, given a dBz from the data you'll compute an
   * index into the ZR table.  'ndbz' is the number of possible
   * indexes.
   */
  zr -> ndbz   = (zr->dbz_hi-zr->dbz_low)/zr->dbz_res + 1 ; /* -15,84 by 0.5 */

  /* Define the range intervals.  Fill in all indexes over the range
   * 0 to zr->nrange-1.  So if you only have 1 range value, meaning
   * that there is no range dependency, then you'll only set the
   * index[0].  For range dependency, you should fill similiar to:
   *    zr -> range_interval[0] = 0
   *    zr -> range_interval[1] = 30
   *    zr -> range_interval[2] = 40
   *    ...
   *    zr -> range_interval[n-1] = 120
   */
  zr -> range_interval = (float *)calloc(zr->nrange, sizeof(float)); 
  zr -> range_interval[0] = 0;
  zr -> range_interval[1] = minrange;
  zr -> range_interval[2] = maxrange;

  /* For documentation of the rain-types, in the order they appear
   * in the ZR table, specify some descriptive string for the 
   * rain-type.  Load all indexes from 0 to zr->nrtype-1.
   *
   *    zr -> rain_type_str[0] = strdup("Stratiform");
   *    zr -> rain_type_str[1] = strdup("Convective");
   *    ...
   *    zr -> rain_type_str[n-1] = strdup("TYPE N");
   */
  zr -> rain_type_str  = (char **)calloc(zr->nrtype, sizeof(char *));
  zr -> rain_type_str[0] = (char *)strdup("MISSING");
  zr -> rain_type_str[1] = (char *)strdup("NOECHO");
  zr -> rain_type_str[2] = (char *)strdup("Stratiform");
  zr -> rain_type_str[3] = (char *)strdup("Convective");


  /* Any changes to the defaults? */
  process_args(argc, argv,
			   &the_site,
			   &sdate, &stime,
			   &edate, &etime,
			   &maxdbz,
			   &badval_above_max,
			   &minrange, &maxrange,
			   k, a, zr->rain_type_str, &ntype);

  if (sdate) {
	sscanf(sdate, "%d/%d/%d", &stm.tm_mon, &stm.tm_mday, &stm.tm_year);
	stm.tm_year -= 1900; /* I automatically correct the century for mktime */
	stm.tm_mon --;       /* I automatically correct the month for mktime */
	stm.tm_isdst = -1;    /* Don't use daylight savings time. */
  }
  if (stime) sscanf(stime, "%d:%d:%d", &stm.tm_hour, &stm.tm_min, &stm.tm_sec);
  if (sdate || stime) zr -> start_time = mktime(&stm);

  if (edate) {
	sscanf(edate, "%d/%d/%d", &etm.tm_mon, &etm.tm_mday, &etm.tm_year);
	etm.tm_year -= 1900; /* I automatically correct the century for mktime */
	etm.tm_mon --;       /* I automatically correct the month for mktime */
	etm.tm_isdst = -1;    /* Don't use daylight savings time. */
  }
  if (etime) sscanf(etime, "%d:%d:%d", &etm.tm_hour, &etm.tm_min, &etm.tm_sec);

  if (edate || etime) zr -> stop_time = mktime(&etm);


  /* Do we need to lookup the site? */
  if (the_site) {
	FILE *gp;
	char *cmd = (char *)calloc(500, sizeof(char));
	char *data_path = (char *)calloc(100, sizeof(char));
	char *site_file = (char *)calloc(200, sizeof(char));

	for (i=0; *(the_site+i) = toupper(*(the_site+i)); i++)  /* UPPER */;

	sprintf(zr->site_name, "%s", the_site);
	data_path = getenv("GVS_DATA_PATH");
	if (data_path == NULL) data_path = "/usr/local/trmm/GVBOX/data";
	sprintf(site_file, "%s/gv_radar_site_info.data", data_path);
	sprintf(cmd,"grep %s %s", the_site, site_file);
	gp = (FILE *)popen(cmd, "r");
	fgets(cmd, 500, gp);

	/* Count ','.  Count 6 of them.  Then, parse for lat/lon. 
     * Note: it is not possible to scanf directly on the pipe because
     * %s stops on white-space and some comma delimited fields contain
     * white-space.
     */
	if (strchr(cmd, ',') != NULL) {
	  for (i=0;i<6;i++) {
		cmd = (char *)strchr(cmd, ',');
		cmd++;
	  }
	  if (sscanf(cmd, "%f%*s%f", &zr->radar_lat, &zr->radar_lon) != 2) {
		zr->radar_lat = zr->radar_lon = 0.0;
	  }
	} else {
		zr->radar_lat = zr->radar_lon = 0.0;
	}
	if (zr->radar_lat == 0 || zr->radar_lon == 0) 
	  fprintf(stderr,"Unable to determine lat/lon for site <%s> in file <%s>.\n", zr->site_name, site_file);

	pclose(gp);
  }
  /* Here we load the 3D table. */
  zr -> r = (float ***)calloc(zr->nrange, sizeof(float**));
  for (i=0; i<zr->nrange; i++) {
	zr -> r[i] = (float **)calloc(zr->nrtype, sizeof(float*));
	for (j=0; j<zr->nrtype; j++) {
	  zr -> r[i][j] = (float *)calloc(zr->ndbz, sizeof(float));
	  for (n=0, dbz=zr->dbz_low; dbz<=zr->dbz_hi; dbz+=zr->dbz_res, n++) {
		/* RSL_z_to_r is a simple ZR relationship. */
		/* If the dbz is above the maximum allowable, then check if we flag
         * those rainrates as BADVAL or we plateau at the maximum allowable dbz.
         * -99 is MISSING.  MISSING data exists closer than minrange and farther
         * than maxrange from the radar.
		 */
		if (dbz > maxdbz)
		  if (badval_above_max) r = BADVAL;
		  else r = RSL_z_to_r(maxdbz, k[j], a[j]);
		else
		  r = RSL_z_to_r(dbz, k[j], a[j]);

		if (j == 0) r = -99;
		if (j == 1) r = 0;
		if (zr->range_interval[i] >= maxrange) r = -99.0;
		if (zr->range_interval[i] < minrange) r = -99.0;
		/*
         * i indexes the ranges.
         * j indexes the rain-types.
         * n indexes the dBz index.
         */
		zr->r[i][j][n] = r;
	  }
	}
  }
  
  /* Once the 3D table contains all the information, use the proper
   * interface routine to correctly write the ZR table.  The routine
   * is called 'write_ZR'.
   */
  write_ZR (zr, NULL);  /* The NULL means, write to stdout. */
}
