/*
 * Test building a SINGLE or UNIIFORM ZR table.
 */

/* 
 * For this test, I will use the RSL_...z_to_r function; simple zr.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "zr_table.h"
#include "rsl.h"

int main(int argc, char **argv)
{
  Zr_table *zr;
  float dbz;
  float k[3] = {300,300,300}, a[3] = {1.4,1.4,1.4}; /* Simple ZR */
  float r;
  struct tm tm;
	
  int i,j,n;

  /* Building the ZR table is fairly easy and it is composed of two
   * basic steps:
   *
   *    1. Fill header information: date, lat/lon, dimensions, etc.
   *    2. Fill the 3D table with numbers.  The 3D table is indexed
   *       by range, rain-type, and dBz.
   */

  /* For the PI, for building the ZR table, the job is to properly load
   * the 'Zr_table' data structure and call the output routine: write_zr.
   */

  /* The routine 'write_zr' outputs a ZR table that 2A-53 reads.  No
   * other interface should be used as the routine 'read_zr' is 
   * the counter-part to 'write_zr'.  Whatever, 'write_zr' generates
   * is acceptable to 'read_zr'.
   */

  /* This program illustrates, by example, how to properly load
   * the 'Zr_table' data structure prior to calling 'write_zr'.
   * When creating your own ZR table, please, follow the steps
   * illustrated in this example to ensure that you create a properly
   * formatted output ZR file.
   */

  putenv("ZT=UT"); /* No local time zone conversion w/ mktime. */
  /* BEGIN: Load the header section. */
  zr = (Zr_table *)calloc(1, sizeof(Zr_table));

  /* This is the site name.  It can be up to 200 characters and should
   * be representative of the location, or site, for the ZR table.
   * This can be any string.
   */
  sprintf(zr -> site_name,"KMLB, SINGLE ZR table.");

  /* Specify the lat/lon for the radar.  It is a good idea to make
   * these numbers match the lat/lon of the radar.
   */
  zr -> radar_lat  = 32.0;
  zr -> radar_lon  = -55.0;

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
  
  tm.tm_mon  = 10;    /* Month 1 - 12 */
  tm.tm_mday = 21;    /* Day   1 - 31 */
  tm.tm_year = 1996;  /* Year  19xx   */
  tm.tm_hour = 7;     /* Hour  0 - 23 */   
  tm.tm_min  = 22;    /* Min   0 - 59 */
  tm.tm_sec  = 10;    /* Sec   0 - 59 */
  
  tm.tm_year -= 1900; /* I automatically correct the century for mktime */
  tm.tm_mon --;       /* I automatically correct the month for mktime */
  tm.tm_isdst = -1;    /* Don't use daylight savings time. */
  zr->start_time = mktime(&tm);

  tm.tm_mon  = 02;    /* Month 1 - 12 */
  tm.tm_mday = 10;    /* Day   1 - 31 */
  tm.tm_year = 1997;  /* Year  19xx   */
  tm.tm_hour = 18;    /* Hour  0 - 23 */   
  tm.tm_min  = 48;    /* Min   0 - 59 */
  tm.tm_sec  = 56;    /* Sec   0 - 59 */
  
  tm.tm_year -= 1900; /* I automatically correct the century for mktime */
  tm.tm_mon --;       /* I automatically correct the month for mktime */
  tm.tm_isdst = -1;    /* Don't use daylight savings time. */
  zr -> stop_time  = mktime(&tm);

  /* Here, define the range of dBz values that are present in the 3-D
   * ZR table.  The range divided by the resolution define the number
   * of dBz indexes for the ZR table.
   */
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
  zr -> nrtype = 3; /* Uniform rain classification */

  /* 'ndbz' is computed and is the number of dBz bins.
   * Basically, given a dBz from the data you'll compute an
   * index into the ZR table.  'ndbz' is the number of possible
   * indexes.
   */
  zr -> ndbz   = (zr->dbz_hi-zr->dbz_low)/zr->dbz_res + 1 ; /* -15,84 by 0.5 */

  /* Define the range intervals.  Fill in all indexes over the range
   * 0 to zr->nrange-1.  So if you only have 1 range value, meaning
   * that there is no range dependency, then you'll only set the
   * index[0].  You should fill:
   *    zr -> range_interval[0] = 15
   *    zr -> range_interval[1] = 30
   *    zr -> range_interval[2] = 40
   *    ...
   *    zr -> range_interval[n-1] = 120
   */
  zr -> range_interval = (float *)calloc(zr->nrange, sizeof(float)); 
  zr -> range_interval[0] = 0;
  zr -> range_interval[1] = 15;
  zr -> range_interval[2] = 150;

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
  zr -> rain_type_str[2] = (char *)strdup("Uniform");

  /* Here we load the 3D table. */
  zr -> r = (float ***)calloc(zr->nrange, sizeof(float**));
  for (i=0; i<zr->nrange; i++) {
	zr -> r[i] = (float **)calloc(zr->nrtype, sizeof(float*));
	for (j=0; j<zr->nrtype; j++) {
	  zr -> r[i][j] = (float *)calloc(zr->ndbz, sizeof(float));
	  for (n=0, dbz=zr->dbz_low; dbz<=zr->dbz_hi; dbz+=zr->dbz_res, n++) {
		/* For example, I use a simple ZR relationship. */
		r = RSL_z_to_r(dbz, k[j], a[j]);

		if (j == 0) r = -99;
		if (j == 1) r = 0;
		if (zr->range_interval[i] >= 150) r = -99.0;
		if (zr->range_interval[i] < 15) r = -99.0;
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
   * is called 'write_zr'.
   */
  write_ZR (zr, "single.zr");
}


	
