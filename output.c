/*
 *   output.c: Contains routines to read and write data from/to the gdbm 
 *             (first intermediate ZR file).
 *
 *    Note: We uses two tables to store the VOS entry (similar to relational
 *             tables).
 *      table 1 contains: key: 1 netID gaugeID content: ngID
 *      table 2 contains: key: 2 ngID time (in binary)  content: binary_data
 *                   2    = sizeof(char) byte.
 *                   ngID = sizeof(short) bytes,
 *                   time = sizeof(time_t) bytes
 *
 *         Note: key is prefixed with the table #.
 *
 */

#include <stdio.h>
#include <gdbm.h>
#include <gv_utils.h>
#include "get_radar_data_over_gauge.h"
#include "get_radar_data_over_gauge_db.h"
#include "zr.h"
#include "gauge_db.h"

extern int verbose;
extern char *this_prog;

#define MAX_STR_LEN 200
/*
 * Clearly, SAVE_SCALED_INT is not worth it, if
 * we save scaled int's using 4-bytes.  Why bother?
 * Might as well save the original binary.
 * But, this macro may be usefull ...
 */

#define SAVE_SCALED_INT(b, x, s)\
{\
   int i4;\
   i4 = (x) * (s);\
   memcpy((b), &i4, sizeof(i4));\
   (b) += sizeof(i4);\
}

/*
 * Simply list the keys and content. 
 */
#define GET_UNSCALED_X(b, x, s)\
{\
   int i;\
   memcpy(&i, (b), sizeof(i));\
   x = (i / (s));\
   (b) += sizeof(i);\
}

typedef struct {
  char h[HEADER_LEN];
  int  n;
} Header_t;


Header_t *construct_header(char *site,
					   float lat, float lon,
					   float gauge_win_xmax,
					   float gauge_win_ymax, 
					   float gauge_win_zmax,
					   rain_class_type_t rain_class_type);
void modify_header_time(GDBM_FILE fp, char *which_time, DATE_STR *sdate, TIME_STR *stime);

/**********************************************************************/
/*                                                                    */
/*                           open_outfile_and_write_header_info       */
/*                                                                    */
/**********************************************************************/
GDBM_FILE open_outfile_and_write_header_info(char *site,
											 DATE_STR *sdate, TIME_STR *stime,
											 DATE_STR *edate, TIME_STR *etime,
											 float lat, float lon,
											 float gauge_win_xmax,
											 float gauge_win_ymax, 
											 float gauge_win_zmax,
											 rain_class_type_t rain_class_type,
											 char *fname)
	 
{
  /* Open output file (gdbm) for appending.  If file doesnot exist, create it 
   * and store header info.
   * Else if file exists, change the end date/time info. in the header
   * section (only edate/etime and fname are required).
   *
   * This file will be left openned and its file pointer will be returned.
   * The caller should close this file.
   */
	
  GDBM_FILE fp;
  datum key, content;
  Header_t *header;

  /* Open the gdbm file 'fname' for write output the header, if file does not exist.
   */
  fp = gdbm_open(fname, 512, GDBM_WRCREAT, 0644, NULL);
  if (fp == NULL) {
	fprintf(stderr, "gdbm_open: %s\n", gdbm_strerror(gdbm_errno));
	return NULL;
  }

  /* Determine if the header needs to be changed due to a different stime
   * or etime.  These times must represent the time range.  'modify_header_time'
   * conditionally modifies the START and END times.
   */

  modify_header_time(fp, "START TIME", sdate, stime);
  modify_header_time(fp, "END TIME", edate, etime);

  /* Allocates memory for header. START and END times are stored separately; previous code. */

  header = construct_header(site, lat, lon,
							gauge_win_xmax, gauge_win_ymax, gauge_win_zmax,
							rain_class_type);
  
  key.dptr = "HEADER"; key.dsize = strlen(key.dptr) + 1;
  content.dptr = header->h;
  content.dsize = header->n;
  
  gdbm_store(fp, key, content, GDBM_REPLACE);

  free(header);
  return fp;
} /* open_outfile_and_write_header_info */

/*************************************************************/
/*                                                           */
/*                modify_header_time                         */
/*                                                           */
/*************************************************************/
void modify_header_time(GDBM_FILE fp, char *which_time, DATE_STR *sdate, TIME_STR *stime)
{
  datum key, content;
  int q_time_change;
  char date_str[MAX_NAME_LEN], time_str[MAX_NAME_LEN];
  time_t time_seconds, stime_seconds;

  key.dptr = which_time;
  key.dsize = strlen(key.dptr) + 1; /* Always include terminating \0 in the count. */
  content = gdbm_fetch(fp, key);
  q_time_change = 0;
  if (content.dptr == NULL) {
	content.dptr = (char *)calloc(MAX_NAME_LEN, sizeof(char));
	content.dsize = sprintf(content.dptr, "%.2d/%.2d/%.4d %.2d:%.2d:%.2d",
							sdate->tkmonth, sdate->tkday, sdate->tkyear,
							stime->tkhour, stime->tkminute, stime->tksecond);
	content.dsize++; /* Count the trailing \0. */
	q_time_change = 1;
	gdbm_store(fp, key, content, GDBM_REPLACE);
	return;
  }
  /* Determine if the start time is earlier than what exists. */
  memset(date_str, '\0', MAX_NAME_LEN);
  memset(time_str, '\0', MAX_NAME_LEN);
  sscanf(content.dptr, "%s %s", date_str, time_str); /* Extract what's in the file. */
  memset(&time_seconds, '\0', sizeof(time_t));
  date_time2system_time(sdate, stime, &stime_seconds);
  date_time_strs2seconds(date_str, time_str, &time_seconds);
  if (strcmp(which_time, "START TIME") == 0) {
	q_time_change = difftime(stime_seconds, time_seconds) < 0;
  } else { /* Assume END TIME :-) */
	q_time_change = difftime(stime_seconds, time_seconds) > 0;
  }
  if (q_time_change) { /* Compare w/ new stime. */
	/* New start time is earlier. Modify it. */
	sprintf(content.dptr, "%.2d/%.2d/%.4d %.2d:%.2d:%.2d",
			sdate->tkmonth, sdate->tkday, sdate->tkyear,
			stime->tkhour, stime->tkminute, stime->tksecond);
	q_time_change = 1;
  }
  if (q_time_change) gdbm_store(fp, key, content, GDBM_REPLACE);
  return;
}


/*************************************************************/
/*                                                           */
/*                   construct_header                        */
/*                                                           */
/*************************************************************/
Header_t *construct_header(char *site,
					   float lat, float lon,
					   float gauge_win_xmax,
					   float gauge_win_ymax, 
					   float gauge_win_zmax,
					   rain_class_type_t rain_class_type)

{
  Header_t *header;
  char *header_p;
  int n;
  char date_str[MAX_NAME_LEN], time_str[MAX_NAME_LEN];

  memset(date_str, '\0', MAX_NAME_LEN);
  memset(time_str, '\0', MAX_NAME_LEN);
  gv_utils_time_secs2date_time_strs(time(NULL), 1, 0, date_str, time_str);

  header = (Header_t *)calloc(1,  sizeof(Header_t));
  header_p = header->h;
  /* file doesn't exist, create it and write header info.  */	
  n = sprintf(header_p, "%s This table is the first intermediate product for creating the ZR table\n"
						"%s It contains radar windows and rain classifications data for rain gauges. \n", USER_COMMENT_CHARS, USER_COMMENT_CHARS);
  header_p += n;
  n = sprintf(header_p, "%s Line starts with # is considered comment.\n", USER_COMMENT_CHARS);
  header_p += n;
  n = sprintf(header_p, "%s\n"
			            "%s File generation information:\n"
                        "%s     Created by:     %s (%s)\n"
                        "%s     Generated time: %s %s\n", 	  
			  USER_COMMENT_CHARS, USER_COMMENT_CHARS,USER_COMMENT_CHARS,
			  this_prog, PROG_VERSION,
			  USER_COMMENT_CHARS, date_str, time_str);
  header_p += n;
  n = sprintf(header_p, "%s \n", USER_COMMENT_CHARS);
  header_p += n;
  n = sprintf(header_p, " %s                         %s\n", SITE_NAME_STR, site);
  header_p += n;
  /* The START and END times will be replaced, whenever an ASCII dump of
   * the database is performed.  See 'first2ascii.c'. 
   */
  n = sprintf(header_p, " %s                   MM/DD/YYYY HH:MM:SS\n", START_DATE_TIME_STR);
  header_p += n;

  n = sprintf(header_p, " %s                     MM/DD/YYYY HH:MM:SS\n", END_DATE_TIME_STR);
  header_p += n;

  n = sprintf(header_p, " Radar_lat                         %f\n", lat);
  header_p += n;


  n = sprintf(header_p, " Radar_lon                         %f\n", lon);
  header_p += n;

  n = sprintf(header_p,   " Gauge_window_size_in_km_xyz       %.2f %.2f %.2f\n", gauge_win_xmax, gauge_win_ymax, gauge_win_zmax);
  header_p += n;

  if (rain_class_type == SINGLE) {
	n = sprintf(header_p, " Rain_class_type                   UNIFORM\n");
	header_p += n;
	n = sprintf(header_p, " Number_of_rain_types              %d\n", 1);
	header_p += n;
	n = sprintf(header_p, " Rain_type_value_1                 %-3d Uniform\n",  (int) UNIFORM_C);
	header_p += n;
  }
  else if (rain_class_type == DUAL) {
	n = sprintf(header_p, " Rain_class_type                   DUAL\n");
	header_p += n;
	n = sprintf(header_p, " Number_of_rain_types              %d\n", 4);
	header_p += n;
	n = sprintf(header_p, " Rain_type_value_1                 %-3d No_echo\n", NO_ECHO_C);
	header_p += n;
	n = sprintf(header_p, " Rain_type_value_2                 %-3d Stratiform\n",  (int) STRATIFORM_C);
	header_p += n;
	n = sprintf(header_p, " Rain_type_value_3                 %-3d Convective\n", (int) CONVECTIVE_C);
	header_p += n;
	n = sprintf(header_p, " Rain_type_value_4                 %-3d Missing/bad\n", (int) MISSING_OR_BAD_DATA_C);
	header_p += n;
  }
  /* Note: This comment section will be parsed by  'merge_radarNgauge_data'
   *       based on COMMENT_RECORD_FORMAT_LINE* characters.
   */
  n = sprintf(header_p, "%c %s\n", COMMENT_CHAR, TABLE_RECORD_INFO_STR);
  header_p += n;

  n = sprintf(header_p, "%s Record Format:\n"
			  /*1 */    "%s                                      _NC pairs__     _NC pairs__\n"
			  /* 2 */   "%s                                      |         |     |         |\n"
			  /* 3 */   "%s Record:   ID Net Date Time r NH H1 NC C1 Z1 ... H2 NC C1 Z1 ... ...\n"
			  /* 4 */   "%s DataType: I   S   D    T   F  I F  I  I  F      F  I  I  F\n"
			            "%s\n"
                        "%s  where: \n"
                        "%s    ID   = Gauge ID         Net  = Gauge Network\n"
                        "%s    Date = Vos Date         Time = Vos Time\n"
                        "%s    r    = Range            NH   = Number of Heights\n"
                        "%s    H    = Height in KM     NC   = Number of Cells\n"
			  /* 5 */   "%s    C    = Rain Type Value  Z    = dBz Reflectivity, %.2f Missing/Bad\n"
                        "%s  Record Field's Data Type:\n"
                        "%s    I    = Integer          S    = String         D    = mm/dd/yy\n"
			  /* 6 */   "%s    T    = hh:mm            F    = Float\n"
			            "%s\n", 
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE1,
			  COMMENT_RECORD_FORMAT_LINE2, COMMENT_RECORD_FORMAT_LINE3, 
			  COMMENT_RECORD_FORMAT_LINE4, COMMENT_RECORD_FORMAT_LINE,
			  /* where */
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE, 
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE,  
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE5, 
			  MISSING_Z,
			  /* Field's data type */
			  COMMENT_RECORD_FORMAT_LINE,  COMMENT_RECORD_FORMAT_LINE, 
			  COMMENT_RECORD_FORMAT_LINE6,  COMMENT_RECORD_FORMAT_LINE);
  header_p += n;

  n = sprintf(header_p, "%s   Gauge Window of nCells [(x/2)*(y/2)] Centered at Gauge:\n"
                        "%s       ------------------------------------------\n"
                        "%s       | (CN-2,ZN-2) | (CN-1,ZN-1) |   (CN,ZN)  |\n"
                        "%s       ------------------------------------------\n"
                        "%s       |    ...      |      ...    |     ...    |\n"
                        "%s       ------------------------------------------\n"
                        "%s       |   (C1,Z1)   |   (C2,Z2)   |  (C3,Z3)   |\n"
                        "%s       ------------------------------------------\n"
			  /* 7 */    "%s\n",
			  COMMENT_RECORD_FORMAT_LINE,  COMMENT_RECORD_FORMAT_LINE, 
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE,
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE, 
			  COMMENT_RECORD_FORMAT_LINE, COMMENT_RECORD_FORMAT_LINE, 
			  COMMENT_RECORD_FORMAT_LINE7);
  header_p += n;

  n = sprintf(header_p, "%s", TABLE_START_STR);
  header_p += n;
  header->n = (header_p - header->h) + 1;


  return header;
}

/***************************************************************************/
/*                                                                         */
/*                         create_entry_key_str                            */
/*                                                                         */
/***************************************************************************/
int create_entry_key_str(GDBM_FILE fp, char *gauge_id, char *net_name,
						 DATE_STR date, TIME_STR time, datum *entry_key)
{
  /* Construct key (ngID time, where time is in binary) for entry. 
   * To optimize space, we use two tables: 
   *            1) key: 1 netID gaugeID   content: ngID
   *            2) key: 2 ngID time (in binary)   content: data in binary.
   *                 Note: key is prefixed with the table #.
   *
   * Want to return key_str = ngID time, where time is in binary.
   * Return 1 for successful; -1 othewise.
   */
  short gauge_id_i;
  char t2_key_str[MAX_NAME_LEN];
  datum key, content;
  int rc;
  short ngID_count = 0;
  int ngID_count2 = 0;
  int len;
  time_t time_sec;
  char t1_key_str[MAX_NAME_LEN];
  char *key_str;
  int tmp_num = 0;

  if (fp == NULL || gauge_id == NULL || net_name == NULL || 
	  entry_key == NULL || entry_key->dptr == NULL)
	return -1;

  gauge_id_i = (short) atoi(gauge_id);

  memset(t1_key_str, '\0', MAX_NAME_LEN);
  sprintf(t1_key_str, "1 %s %d",  net_name, gauge_id_i);


  key.dptr = t1_key_str;
  key.dsize = strlen(t1_key_str) + 1;
  content = gdbm_fetch(fp, key);

  if (content.dptr) {
	/* Found the key for the 2nd table */
	if (sscanf(content.dptr, "%d", &tmp_num) != 1) {
	  free(content.dptr);
	  goto CREATE_NEW_KEY;
	}
	ngID_count = (short) tmp_num;
	free(content.dptr);
  }
  else {
  CREATE_NEW_KEY:

	/* Create a new key for the 2nd table (ngID for netID and gaugeID).
	 */
	if ((rc = gauge_get_max_ngid_count_from_db(fp, &ngID_count2)) == 0) {
	  /* The database is new, add an entry for max count. */
	  ngID_count = 1;
	}
	else if (rc == 1) {
	  /* max count is found, increment it */
	  ngID_count = (short) (ngID_count2+1);
	}
	else 
	  return -1;

	/* Update the db. */
	memset(t2_key_str, '\0', MAX_NAME_LEN);
	sprintf(t2_key_str, "%d ", ngID_count);
	gauge_change_max_ngid_count_in_db(fp, ngID_count);
	/* Add an entry to the net_gauge table (1st table) */

	content.dptr = t2_key_str;
	content.dsize = strlen(content.dptr) + 1;
	gdbm_store(fp, key, content, GDBM_INSERT);
  }
  
  /* Key is in binary -- can't have part string and part binary because
   * of '\0' which may be part of the binary data in the middle of key string. 
   */
  len = 0;
  key_str = entry_key->dptr;
  memcpy(key_str, "2", sizeof(char));          /* Store '2' */
  len += sizeof(char);
  memcpy(key_str + len, " ", sizeof(char));    /* append ' ' */
  len += sizeof(char);
  memcpy(key_str+len, &ngID_count, sizeof(short)); /* Append ngID */
  len +=  sizeof(short);
  memcpy(key_str+len, " ", sizeof(char));     /* append ' ' */
  len += sizeof(char);

  /* Store time in binary  -- To save space. */
  date_time2system_time(&date, &time, &time_sec);
  memcpy(key_str+len, &time_sec, sizeof(time_t)); /* append time */
  key_str[len+sizeof(time_t)] = '\0';       /* End of string char. */
  entry_key->dsize = len + sizeof(time_t) + 1;    /* Included '\0' */
  return 1;
  
} /*  create_entry_key_str */

/***************************************************************************/
/*                                                                         */
/*                           append_column_to_file                         */
/*                                                                         */
/***************************************************************************/
void append_column_to_file(zc_column_t *column, 
						   char *net_name, GDBM_FILE fp)
{
  /* WriteAdd a data line to the database.
   * The data line format:
   *    GaugeID GaugeNetworkName mm/dd/yyyy hh:mm C0 r N H1 C1 Z1 ... CN ZN ...
   *       HN C1 Z1 ... CN ZN
   * 
   *  where:  
   *          Gauge ID         = Gauge number.
   *          GaugeNetworkName = Gauge network name, i.e., KSC, STJ,...
   *          mm/dd/yyyy       = Start date.
   *          hh:mm            = Start time.
   *          r                = Gauge to Radar range (km).
   *          N                = Number of different height(s).
   *          H1               = First height in km.
   *          n                = Number of 2 tuples (C Z) that follow.
   *          C                = Rain type.
   *          Z                = Reflectivity in dBZ.
   *          ...
   *
   *          H n C Z will be repeated for N-1 times.
   *
   *
   * Ok, what you just read above is the ideal ASCII output.  However, for
   * the database.  Gauge ID, GaugeNetworkName, Start date, and Start time
   * are stored separately.  The database stores [key, content] pairs.
   * The content is usually gauge_range, Nheights, height, n2tuple, then the 2-tuples...
   */
  int i;
  datum key, content, key_noecho, content_noecho;
  static char b200[200], b10000[10000];
  char *p10000; /* Pointer for current position of b10000. */
  char *p_noecho;
  int all_noecho;
  int h;

  if (column == NULL || fp == NULL) return;

  memset(b200, '\0', 200);
  key.dptr = b200;

  if (verbose)
	fprintf(stderr, "Get entry key ...\n");
  if (create_entry_key_str(fp, column->gauge_id, net_name, column->sdate,
					column->stime, &key) < 0) return;
  if (verbose)
	fprintf(stderr, "Adding entry to the DB...\n");
  memset(b10000, '\0', 10000);
  content.dptr = p10000 = b10000;
  
  SAVE_SCALED_INT(p10000, column->gauge_range, 1000.0);

  SAVE_SCALED_INT(p10000, column->nhinfo, 1000.0);

  for (h = 0; h < MAX_HEIGHT_LEVELS && h < column->nhinfo; h++) {
	SAVE_SCALED_INT(p10000, (column->hinfo[h])->height, 1000.0);
	/* Space saving trick:
	 *    Determine if this record contains all NOECHOS.  If so,
	 *    then point this to the NOECHO entry in the database.
	 *    If the NOECHO entry doesn't exist, create it.
	 */
	for (i = 0, all_noecho = 1; all_noecho && i < (column->hinfo[h])->nzc; i++) {
	  if ((column->hinfo[h])->zc[i].c != NO_ECHO_C) all_noecho = 0;
	}
  
	if (all_noecho) {  /* Set the 'nzc' to negative to flag this. */
	  SAVE_SCALED_INT(p10000, -1*(column->hinfo[h])->nzc, 1000.0);
	} else {
	  SAVE_SCALED_INT(p10000, (column->hinfo[h])->nzc, 1000.0);
	}
	if (all_noecho) { /* Determine if the database already has the NOECHO. */
	  key_noecho.dptr = "NOECHO"; key_noecho.dsize = strlen(key_noecho.dptr) + 1;
	  content_noecho = gdbm_fetch(fp, key_noecho);
	  if (content_noecho.dptr == NULL) { /* Create the NOECHO entry. */
		p_noecho = p10000; /* Save the start location in the P10000 buffer. */
		for (i = 0; i < (column->hinfo[h])->nzc; i++) {
		  SAVE_SCALED_INT(p10000, (column->hinfo[h])->zc[i].c, 1000.0);
		  SAVE_SCALED_INT(p10000, (column->hinfo[h])->zc[i].z, 1000.0);
		}

		content_noecho.dptr = p_noecho;
		content_noecho.dsize = p10000 - p_noecho + 1;
		gdbm_store(fp, key_noecho, content_noecho, GDBM_REPLACE);
		p10000 = p_noecho; /* For the key and content below.  
							* We're no longer storing the NOECHO numbers,
							* but, we need to write the key and ncz. 
							*/
	  } else {
		free(content_noecho.dptr);
	  }
	} else { /* The record is not all NOECHO. */
	  
	  /*
		n = sprintf(p10000, "%.2f", column->gauge_range);
		p10000 += n;
		n = sprintf(p10000, " %d", column->nzc);
		p10000 += n;
		*/
	  /* Write all zc fields. */
	  for (i = 0; i < (column->hinfo[h])->nzc; i++) {
	  
	  /*
		n = sprintf(p10000, " %d %.2f %.2f", column->zc[i].c, column->zc[i].z,
		column->zc[i].height);
		p10000 += n;
		*/
	  
		SAVE_SCALED_INT(p10000, (column->hinfo[h])->zc[i].c, 1000.0);
		SAVE_SCALED_INT(p10000, (column->hinfo[h])->zc[i].z, 1000.0);
	  }
	} /* else */
  } /* for */
  content.dsize = p10000 - b10000 + 1;
  gdbm_store(fp, key, content, GDBM_REPLACE);
} /* append_column_to_file */


/**********************************************************************/
/*                                                                    */
/*                         read_header_from_db                        */
/*                                                                    */
/**********************************************************************/
int read_header_from_db(GDBM_FILE gf, char *header_line)
{
  /* Read the header for the first intermediate file from the database.
   * Return 1 for successful; -1, otherwise.
   */
  datum key, content;
  char *p;
  char *start_str, *end_str;

  if (gf == NULL || header_line == NULL) return -1;
  
  /* Code was cut from first2ascii.c. */
  key.dptr = "START TIME"; key.dsize = strlen(key.dptr) + 1;
  content = gdbm_fetch(gf, key);
  if (content.dptr == NULL) return -1;
  start_str = strdup(content.dptr);
  /*
	printf("START TIME = %s\n", content.dptr); 
	*/
  free(content.dptr);  
  key.dptr = "END TIME"; key.dsize = strlen(key.dptr) + 1;
  content = gdbm_fetch(gf, key);
  if (content.dptr == NULL) return -1;
  end_str = strdup(content.dptr);

  /*
	printf("END TIME   = %s\n", content.dptr); 
	*/
  free(content.dptr);
  key.dptr = "HEADER"; key.dsize = strlen(key.dptr) + 1;
  content = gdbm_fetch(gf, key);
  if (content.dptr == NULL) return -1;
  /*
	printf("header   = <%s>\n", content.dptr); 
	*/
  if ((p = strstr(content.dptr, START_DATE_TIME_STR)) != NULL) {
	p = strstr(p, "MM/DD/YYYY");
	memcpy(p, start_str, strlen(start_str));
  }
  if ((p = strstr(content.dptr, END_DATE_TIME_STR)) != NULL) {
	p = strstr(p, "MM/DD/YYYY");
	memcpy(p, end_str, strlen(end_str));
  }

  strcpy(header_line, content.dptr);
  free(content.dptr);
  free(start_str);
  free(end_str);

  return 1;
} /* read_header_from_db */

/**********************************************************************/
/*                                                                    */
/*                        get_line_prefix                             */
/*                                                                    */
/**********************************************************************/
int get_line_prefix(char *key_str, ngID_list_t *ngID_list, char *line_prefix)
{
  /* Key_str = '2 ngID time_sec', in binary
   * Construct line_prefix: netID gaugeID mm/dd/yyyy hh:mm
   * We want to lookup netID gaugeID from ngID_list based on ngID from
   * the key_str.
   * Return 1 for successful; -1, otherwise.
   */
  short ngID;
  time_t time_sec;
  ngID_entry_t *ngID_entry = NULL;
  ngID_list_t *tmp_ngID_list;
  int found; 
  char date_str[MAX_NAME_LEN];
  char time_str[MAX_NAME_LEN];
  int i;


  if (key_str == NULL || line_prefix == NULL || ngID_list == NULL) return -1;

  if (key_str[0] != '2') return -1;
  /* Extract ngID  */
  memcpy(&ngID, key_str + 2, sizeof(short));
  /* Extract time */
  memcpy(&time_sec, key_str + 3 + sizeof(short), sizeof(time_t));
  time_secs2date_time_strs(time_sec, 1, 0, date_str, time_str);

  /* FInd ngID entry that has ngID */
  tmp_ngID_list = ngID_list;
  found = 0;
  while (tmp_ngID_list && !found) {
	for (i = 0; i < tmp_ngID_list->nentries; i++) {
	  if (tmp_ngID_list->ngID_array[i].ngID == ngID) {
		ngID_entry = &(tmp_ngID_list->ngID_array[i]);
		found = 1;
		break;
	  }
	}
	tmp_ngID_list = ngID_list->next;

  }
  if (ngID_entry == NULL) return -1;
  sprintf(line_prefix, "%d %s %s %s", ngID_entry->gaugeID, ngID_entry->netID, date_str, time_str);

  return 1;
} /* get_line_prefix */

/**********************************************************************/
/*                                                                    */
/*                       get_new_ngID_list                            */
/*                                                                    */
/**********************************************************************/
ngID_list_t *get_new_ngID_list()
{
  ngID_list_t *ngID_list;

  ngID_list = (ngID_list_t *) calloc(1, sizeof(ngID_list_t));
  if (ngID_list) {
	ngID_list->next = NULL;
	ngID_list->nentries = 0;
  }
  return ngID_list;
}

/**********************************************************************/
/*                                                                    */
/*                     get_ngID_list_from_db                          */
/*                                                                    */
/**********************************************************************/
int get_ngID_list_from_db(GDBM_FILE gf, ngID_list_t *ngID_list) 
{
  /* Load netID, gaugeID, and ngID from the database to memory.
   * Return 1 for successful; -1, otherwise.
   */
  datum nextkey, key, content;
  ngID_entry_t *ngID_entry;
  ngID_list_t *new_ngID_list;
  char *key_str;
  char netID[MAX_NAME_LEN];
  int rc = 1;
  int gaugeID;

  if (gf == NULL || ngID_list == NULL) return -1;
  key.dptr = NULL;
  content.dptr = NULL;
  for(nextkey = gdbm_firstkey(gf); nextkey.dptr; 
	  nextkey = gdbm_nextkey(gf, key)) {

	key_str = nextkey.dptr;
    /* key_str: netID gaugeID */
	memset(netID, '\0', MAX_NAME_LEN);

	if (sscanf(key_str, "1 %s %d", netID, &gaugeID) == 2) {
	  /* Get the content (ngID) */
	  content = gdbm_fetch(gf, nextkey);
	  if (content.dptr != NULL) {
		if (ngID_list->nentries >= MAX_NGID_ENTRIES) {
		  /* Get a new block of array. */
		  new_ngID_list = get_new_ngID_list();
		  if (new_ngID_list == NULL) {
			free (content.dptr);
			free (key_str);
			if (key.dptr)
			  free(key.dptr);
			rc = -1;
			break;
		  }
		  /* Add new block to the beginning of list. */
		  new_ngID_list->next = ngID_list;
		  ngID_list = new_ngID_list;

		}

		ngID_entry = &(ngID_list->ngID_array[ngID_list->nentries]);
		strcpy(ngID_entry->netID, netID);
		ngID_entry->gaugeID = gaugeID;
		if (sscanf(content.dptr, "%d", &(ngID_entry->ngID)) == 1)
		  ngID_list->nentries++;
	  }
	  if (content.dptr)
		free(content.dptr);
	}
	if (key.dptr)
	  free(key.dptr);
	key = nextkey;
	
  } /* for */
  if (key.dptr)
	free(key.dptr);
  return 1;
		
} /* get_ngID_list_from_db */

/**********************************************************************/
/*                                                                    */
/*                          construct_entry_line                      */
/*                                                                    */
/**********************************************************************/
int construct_entry_line(GDBM_FILE gf, datum *key, ngID_list_t *ngID_list,
						 char *entry_line)
{
  /* Construct the first intermediate's entry line based on key.
   * Return 1 for successful; -1, otherwise.
   * Note: Code was cut from first2ascii.c. 
   *  entry_line's format:
   *    GaugeID GaugeNetworkName mm/dd/yyyy hh:mm r N H1 C1 Z1 ... CN ZN ...
   *       HN C1 Z1 ... CN ZN
   * 
   *  where:  
   *          Gauge ID         = Gauge number.
   *          GaugeNetworkName = Gauge network name, i.e., KSC, STJ,...
   *          mm/dd/yyyy       = Start date.
   *          hh:mm            = Start time.
   *          r                = Gauge to Radar range (km).
   *          N                = Number of different height(s).
   *          H1               = First height in km.
   *          n                = Number of 2 tuples (C Z) that follow.
   *          C                = Rain type.
   *          Z                = Reflectivity in dBZ.
   *          ...
   *
   *          H n C Z will be repeated for N-1 times.
   *
   */
  char *p=NULL, *save_p = NULL, *save_content=NULL, *save_content_noecho = NULL;
  datum content, key_noecho, content_noecho;
  char tmp_s[MAX_NAME_LEN];
  int tmp_i;
  char line_prefix[MAX_STR_LEN];
  int c, ncz, i;
  float z, height, range;
  int q_noecho;
  char *content_str=NULL;
  char *key_str;
  int nhinfo, h;

  if (gf == NULL || key == NULL || key->dptr == NULL ||
	  entry_line == NULL) return -1;
  key_str = key->dptr;


  if (strcmp("HEADER", key_str) == 0 ||
		strcmp("START TIME", key_str) == 0 ||
		strcmp("NOECHO", key_str) == 0 ||
		strcmp("END TIME", key_str) == 0 ||
		/* 1 netID gaugeID **/
		sscanf(key_str, "1 %s %d", tmp_s, &tmp_i) == 2) return -1;
	memset(line_prefix, '\0', MAX_STR_LEN);
	/* key_str = '2 ngID time' in binary */
	if (get_line_prefix(key_str, ngID_list, line_prefix) < 0 || strlen(line_prefix) == 0) {
	  return -1;
	}

	sprintf(entry_line, "%s ", line_prefix);
	content = gdbm_fetch(gf, *key);
	if (content.dptr) {
	  content_str = content.dptr;
	  p = content_str;
	  save_p = NULL;
	  save_content = p;
	  GET_UNSCALED_X(p, range, 1000.0);
	  sprintf(entry_line, "%s %.2f", entry_line, range);
	  GET_UNSCALED_X(p, nhinfo, 1000.0);

	  sprintf(entry_line, "%s %d", entry_line, nhinfo);
	  for (h = 0; h < nhinfo; h++) {
		save_content_noecho = NULL;
		GET_UNSCALED_X(p, height, 1000.0);
		sprintf(entry_line, "%s %.2f", entry_line, height);
		GET_UNSCALED_X(p, ncz, 1000.0);

		q_noecho = 0;
		if (ncz < 0) { /* Use the NOECHO entry for the record. */
		  q_noecho = 1;
		  ncz *= -1;
		}

		sprintf(entry_line, "%s %d", entry_line, ncz);
		if (q_noecho) {
		  key_noecho.dptr = "NOECHO"; key_noecho.dsize = strlen(key_noecho.dptr) + 1;
		  content_noecho = gdbm_fetch(gf, key_noecho);
		  save_p = p;
		  p = content_noecho.dptr;
		  save_content_noecho = p;
		}

		for (i=0; i<ncz && p; i++) {
		  GET_UNSCALED_X(p, c, 1000.0);
		  GET_UNSCALED_X(p, z, 1000.0);
		  sprintf(entry_line, "%s %d %.2f", entry_line, c, z);
		}
		if (save_content_noecho) free(save_content_noecho);
		if (save_p != NULL)
		  p = save_p;
	  } /* for */

	  if (save_content) free(save_content);
	}
	return 1;
} /* construct_entry_line */


/**********************************************************************/
/*                                                                    */
/*                         print_ngID_list                            */
/*                                                                    */
/**********************************************************************/
void print_ngID_list(ngID_list_t *ngID_list)
{
  ngID_entry_t *ngID_entry = NULL;
  ngID_list_t *tmp_ngID_list;
  int  i;

  if (ngID_list == NULL) return;

  /* FInd ngID entry that has ngID */
  tmp_ngID_list = ngID_list;
  while (tmp_ngID_list != NULL) {

	fprintf(stderr, "--> nentries = %d\n", tmp_ngID_list->nentries);
	for (i = 0; i < tmp_ngID_list->nentries; i++) {
	  ngID_entry = &(tmp_ngID_list->ngID_array[i]);
	  fprintf(stderr, "gauge ID: %4.4d netID: %s ngID: %d\n", ngID_entry->gaugeID, ngID_entry->netID, ngID_entry->ngID);
	}
	tmp_ngID_list = ngID_list->next;

  }

}
