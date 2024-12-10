#include <stdio.h>
#include <time.h>
#include <string.h>
#include "zr_table.h"

  /*
   * Format of input zr file:
   *
   * WHAT                     FORMAT    DESCRIPTION
   *----------------------    --------  --------------------------
   * site_name                char 200  one line of text.
   * mm/dd/yy hh:mm:ss start  %2.2d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d
   * mm/dd/yy hh:mm:ss stop   %2.2d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d
   * radar_lat, radar_lon     %f %f     in decimal degrees.
   * #range indexes           %d        eg. 3
   * #raintypes               %d        eg. 10  (rain classifications)
   * #dbz                     %d        eg. 161 (can be computed from range)
   * #dbz_range resolution    %f %f %f  eg. 0 80 0.5  (#dbz=range/res + 1)
   * Rain type 1              %s
   *   ...
   * Rain type n(n=#classes)  %s
   * RANGE-INTERVALS          %f...     Dimension matches number of range
   *                                    indexes.  irange=f(x).  Use
   *                                    these intevals to determine the index
   *                                    for range dependancy.
   *              NOTE: may want to make the range table a table of indexes.
   *
   * ZR_TABLE                 %f...     #classes by #strat by #dbz.  The
   *                                    dimension that increments fastest
   *                                    is #dbz, then #classes, then
   *                                    #ranges. zr[i][j][k] for
   *                                    i=#range, j=#classes, k=#dbz
   *
   *
   * For the header section, that up to the ZR_TABLE, the first 30 characters
   * of each record will be descriptive of the values in that record and
   * will serve to self-document the header fields.  The ZR_TABLE will begin
   * after the ZR-TABLE delimeter.  The appropriate number of
   * values will follow -- to the EOF.  Using a fixed layout for the header
   * makes it easy to parse.  There are section delimeters that are
   * required; order dependant.
   *
   * KEYWORDS: FILE-FORMAT-DESCRIPTION
   *           ZR-HEADER-SECTION
   *           RANGE-INTERVALS
   *           ZR-TABLE
   *
   * For simplicity, the data in the file is ASCII and may be
   * view able and modifiable via any standard text editor.
   */

  /*
   * Example ZR file:

FILE-FORMAT-DESCRIPTION
For the header section, that up to the ZR_TABLE, the first 30 characters
of each record will be descriptive of the values in that record and
will serve to self-document the header fields.  The ZR_TABLE will begin
after the ZR-TABLE delimeter.  The appropriate number of
values will follow -- to the EOF.  Using a fixed layout for the header
makes it easy to parse.  There are section delimeters that are
required; order dependant.
ZR-HEADER-SECTION
site_name                     KMLB, Melbourne FL.
mm/dd/yyyy hh:mm:ss start     12/22/1994 11:02:11
mm/dd/yyyy hh:mm:ss stop      01/17/1995 10:59:11
radar_lat, radar_lon          28.4 -72.87
#range indexes                8
#rain types                   10
#dbz                          161
#dbz_range resolution         0 80 0.5
Rain type 1                   Range
Rain type 2                   BBF
Rain type 3                   EE
Rain type 4                   RGB
Rain type 5                   HF
Rain type 6                   RGF
Rain type 7                   RGVL
Rain type 8                   RGVH
Rain type 9                   CS
Rain type 10                  RTYPE
RANGE-INTERVALS
0
10
12
25
50
100
125
150
ZR_TABLE
0.0 0.0 0.0 ...

*/

Zr_table *build_test_zr(void)
{
  Zr_table *zr;
  int xdim, ydim, zdim;
  int i,j;
  /* TEST -- these come from reading the file. */
  xdim = 8;
  ydim = 10;
  zdim = 161;

  zr = (Zr_table *)calloc(1, sizeof(Zr_table));
  zr -> nrange   = xdim;
  zr -> nrtype   = ydim;
  zr -> ndbz     = zdim;
  sprintf(zr -> site_name,"KMLB, test site");
  zr -> radar_lat  = 32.0;
  zr -> radar_lon  = -55.0;
  zr -> start_time  = time(NULL);
  zr -> stop_time  = time(NULL);
  zr -> dbz_low = 0.0;
  zr -> dbz_hi = 180.0;
  zr -> dbz_res = 0.5;
  
  zr -> rain_type_str  = (char **)calloc(zr->nrtype, sizeof(char *));
  zr -> range_interval = (float *)calloc(zr->nrange, sizeof(float)); 

  zr -> range_interval[0] = 0;
  zr -> range_interval[1] = 10;
  zr -> range_interval[2] = 12;
  zr -> range_interval[3] = 25;
  zr -> range_interval[4] = 50;
  zr -> range_interval[5] = 100;
  zr -> range_interval[6] = 125;
  zr -> range_interval[7] = 150;

  zr -> rain_type_str[0] = (char *) strdup("Range");
  zr -> rain_type_str[1] = (char *) strdup("BBF  ");
  zr -> rain_type_str[2] = (char *) strdup("EE   ");
  zr -> rain_type_str[3] = (char *) strdup("RGB  ");
  zr -> rain_type_str[4] = (char *) strdup("HF   ");
  zr -> rain_type_str[5] = (char *) strdup("RGF  ");
  zr -> rain_type_str[6] = (char *) strdup("RGVL ");
  zr -> rain_type_str[7] = (char *) strdup("RGVH ");
  zr -> rain_type_str[8] = (char *) strdup("CS   ");
  zr -> rain_type_str[9] = (char *) strdup("Rtype");

  zr -> r = (float ***)calloc(xdim, sizeof(float**));
  for (i=0; i<xdim; i++) {
	zr -> r[i] = (float **)calloc(ydim, sizeof(float*));
	for (j=0; j<ydim; j++) {
	  zr -> r[i][j] = (float *)calloc(zdim, sizeof(float));
	}
  }

  return zr;
}

/*************************************************************/
/*                                                           */
/*                         chop                              */
/*                                                           */
/*************************************************************/
char *chop(char *s)
{
  char *c;
  /* Eliminate the \n, just like perl */
  c = strstr(s, "\n");
  *c = '\0';
  return s;
}

/*************************************************************/
/*                                                           */
/*                         read_ZR                           */
/*                                                           */
/*************************************************************/
Zr_table *read_ZR(char *infile)
{
	/* 
		 Read Z-R tables from a file.
	*/
	char *s;
#define ZR_BUF_SIZE 1024
  char buf[ZR_BUF_SIZE];
  struct tm tm;
  int i, j, k, irange, irtype, idbz;
  float range1, range2;
	Zr_table *zr;
	FILE *fp;

	fp = fopen(infile, "r");
	if (fp == NULL) 
	{
		fprintf(stderr, "Error opening file: %s\n", infile);
		return(NULL);
	}
  /* Read down to ZR_HEADER_SECTION */
  while ((s = fgets(buf, sizeof(buf), fp))) {
	chop(s);
	if (strcmp(buf, "ZR_HEADER_SECTION") == 0) break;
  }
  /* EOF? That's an error. */
  if (s == NULL) {
	fprintf(stderr, "ZR_table format error.  Can't find ZR_HEADER_SECTION.\n");
	fclose(fp);
	return NULL;
  }

  zr = (Zr_table *)calloc(1, sizeof(Zr_table));
  
#define START_COL 30
  /* Ok, the first 30 char of each record, down to ZR_TABLE_HEADER, is 
   * descriptive.  Column 31 to \n is what we parse.  Remove the \n.
   */
  s = fgets(buf, sizeof(buf), fp); chop(s); /* site_name */  /* Perl :-) */
  sprintf(zr->site_name, "%s", &s[START_COL]);

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
  zr->start_time = mktime(&tm);
  
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
  zr->stop_time = mktime(&tm);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* lat/lon */
  sscanf(&s[START_COL], "%f %f", &zr->radar_lat, &zr->radar_lon);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nrange  */
  sscanf(&s[START_COL], "%d", &zr->nrange);
	zr->range_interval = (float *)calloc(zr->nrange, sizeof(float));

  s = fgets(buf, sizeof(buf), fp); chop(s); /* nrtype  */
  sscanf(&s[START_COL], "%d", &zr->nrtype);
  zr->rain_type_str = (char **)calloc(zr->nrtype, sizeof(char *));

  s = fgets(buf, sizeof(buf), fp); chop(s); /* ndbz    */
  sscanf(&s[START_COL], "%d", &zr->ndbz);

  s = fgets(buf, sizeof(buf), fp); chop(s); /* dbz range, res */
  sscanf(&s[START_COL], "%f %f %f", &zr->dbz_low, &zr->dbz_hi, &zr->dbz_res);

	/* Allocate the 3D array of rainrate values. */
	zr ->r = (float ***)calloc(zr->nrange, sizeof(float**));
	for (irange=0; irange<zr->nrange; irange++) {
		zr->r[irange] = (float **)calloc(zr->nrtype, sizeof(float*));
		for (irtype=0; irtype<zr->nrtype; irtype++) {
			zr->r[irange][irtype] = (float *)calloc(zr->ndbz, sizeof(float));
		}
	}

	/* Read each of the 'nrange*nrtype' zr_tables from the file. */
	for (irtype=0; irtype<zr->nrtype; irtype++)
	{
		for (irange=0; irange<zr->nrange; irange++)
		{
			/* Read down to ZR_TABLE_HEADER */
			while ((s = fgets(buf, sizeof(buf), fp))) {
				chop(s);
				if (strcmp(buf, "ZR_TABLE_HEADER") == 0) break;
			}
			/* EOF? That's an error. */
			if (s == NULL) {
				fprintf(stderr, "ZR_table format error.  Can't find ZR_TABLE_HEADER.\n");
				fclose(fp);
				return NULL;
			}
		
			s = fgets(buf, sizeof(buf), fp); chop(s);   /* raintype */
			if (zr->rain_type_str[irtype] == NULL)
			  zr->rain_type_str[irtype] = (char *) strdup(&s[START_COL]);

			s = fgets(buf, sizeof(buf), fp); chop(s); /* range_interval */
			sscanf(&s[START_COL], "%f", &zr->range_interval[irange]);

			/* ZR-TABLE */
			fgets(buf, sizeof(buf), fp); chop(s);
			if (strcmp(s, "ZR_TABLE") != 0) { /* Error. */
				fprintf(stderr, "ZR_table format error: Expected to find ZR_TABLE.\n");
				fclose(fp);
				return NULL;
			}
			for (idbz=0; idbz<zr->ndbz; idbz++)
				fscanf(fp,"%*f %f\n", &zr->r[irange][irtype][idbz]);
		} /* end for (irange=0... */
	} /* end for (irtype=0... */
	

  /* Construct the range hash table from the intervals.
   * We'll start with a range resolution of 1/100 km.  That should be
   * plenty.
   */
#define R_HASH_RANGE 2000   /* km */
#define R_HASH_RES   100    /* 1/x (resolution) eg. 1/100 km. */
  zr->range_hash_table = (int *)calloc(R_HASH_RANGE*R_HASH_RES,
									   sizeof(int));  /* Out to 200 km. */
  range1 = range2 = 0;
  for (i=0, k=0; i<zr->nrange-1; i++, k++) {
	/* Over the range interval [i, i+1] */
    /* 'k' is the hash index. */
	range1 = zr->range_interval[i];
	range2 = zr->range_interval[i+1];
	for (j=range1*R_HASH_RES; j<range2*R_HASH_RES; j++)
	  zr->range_hash_table[j] = k;
  }

  /* Still not done with the range hash table, I must supply indexes for the
   * implied last interval.  [last, infinity].  Basically, just use the last 'k'.
   */
  for (j=range2*R_HASH_RES; j<R_HASH_RANGE*R_HASH_RES; j++)
	zr->range_hash_table[j] = k;
  /*
  for (j=0; j<R_HASH_RANGE*R_HASH_RES; j++)
    fprintf(stderr,"zr->range_hash_table[%d] = %d\n", j, 	zr->range_hash_table[j]);
	*/
  fclose(fp);
  return(zr);
}

/*************************************************************/
/*                                                           */
/*                         write_ZR                          */
/*                                                           */
/*************************************************************/
int write_ZR(Zr_table *zr, char *outfile)
{
	/* 
		 Write the Z-R tables to a file.
		 There exists a total of (zr->nrtype x zr->nrange) Z-R tables
		 in the structure 'Zr_table'.
	*/
	int irange, irtype, idbz;
	FILE *fp;
  struct tm *sec_time;
	char time_str[100];

	if (outfile == NULL) fp = stdout;
    else
	  fp = fopen(outfile, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "Error opening file: %s\n", outfile);
		return(-1);
	}
  fprintf(fp,"FILE_FORMAT_DESCRIPTION\n");
  fprintf(fp,"Version: GVS 4.9 and higher\n");
  fprintf(fp,"For all header sections, the first 30 characters\n");
  fprintf(fp,"of each record are descriptive of the values in that record and\n");
  fprintf(fp,"serve to self-document the header fields.  Each ZR_TABLE begins\n");
  fprintf(fp,"after the ZR_TABLE delimeter.  The appropriate number of pairs\n");
  fprintf(fp,"(dbz rainrate) follow.  Using a fixed layout for the headers\n");
  fprintf(fp,"makes them easy to parse.  There are section delimeters that are\n");
  fprintf(fp,"required; order dependant.\n");
  
  fprintf(fp,"\nZR_HEADER_SECTION\n");
  fprintf(fp,"site_name                     %s\n", zr->site_name);
  sec_time = localtime(&zr->start_time);
  strftime(time_str, sizeof(time_str), "%m/%d/%Y %H:%M:%S", sec_time);
  fprintf(fp,"mm/dd/yyyy hh:mm:ss start     %s\n", time_str);
  sec_time = localtime(&zr->stop_time);
  strftime(time_str, sizeof(time_str), "%m/%d/%Y %H:%M:%S", sec_time);
  fprintf(fp,"mm/dd/yyyy hh:mm:ss stop      %s\n", time_str);
  fprintf(fp,"radar_lat, radar_lon          %f %f\n", zr->radar_lat, zr->radar_lon);
  fprintf(fp,"range intervals               %d\n", zr->nrange);
  fprintf(fp,"rain types                    %d\n", zr->nrtype);
  fprintf(fp,"ndbz                          %d\n", zr->ndbz);
  fprintf(fp,"dbz_range resolution          %.2f %.2f %.2f\n", zr->dbz_low, zr->dbz_hi,
					zr->dbz_res);
	
	for (irtype=0; irtype<zr->nrtype; irtype++)
	{
		for (irange=0; irange<zr->nrange; irange++)
		{
			fprintf(fp,"\nZR_TABLE_HEADER\n");
			fprintf(fp, "raintype                      %s\n",    zr->rain_type_str[irtype]);
		    fprintf(fp, "range(km)                     %.2f\n",
								zr->range_interval[irange]);
			fprintf(fp, "ZR_TABLE\n");
			for (idbz=0; idbz<zr->ndbz; idbz++)
			  fprintf(fp, "%5.1f  %.1f\n", (zr->dbz_low + idbz*zr->dbz_res), 
								zr->r[irange][irtype][idbz]);
			fprintf(fp, "\n");
		}
	} /* end for (irtype=0... */
	fclose(fp);
	return(0);
}


/***********************************************************************/
/*                                                                     */
/*                      applyZRtable                                   */
/*                                                                     */
/***********************************************************************/
float applyZRtable(int rain_type,
					float pix_range,
					float dbz,
					Zr_table *zr_table)
{
  int idbz, irange, itype;

  /* Raintype 1,2 */
  /* pix_range = 1 */
  /* dbz (range/res +1) */

  /*  fprintf(stderr, " dbz %f ", dbz); */
  irange = zr_table->range_hash_table[(int)(pix_range*R_HASH_RES)];
  itype  = rain_type;
  if (dbz < zr_table->dbz_low) dbz = zr_table->dbz_low;
  /* accurate zr table index calculation */
  idbz = (dbz - zr_table->dbz_low)*2.0+0.5;
  /*
  fprintf(stderr, " range %f, itype %d,  dbz %f\n", pix_range, itype, dbz);
  fprintf(stderr, "irange %d, itype %d, idbz %d\n", irange, itype, idbz);
  fprintf(stderr, "zr_table->r[%d][%d][%d] = %f\n", irange, itype, idbz,
		 zr_table->r[irange][itype][idbz]);
		 */
  return zr_table->r[irange][itype][idbz];
}
