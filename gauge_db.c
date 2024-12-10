/*
 *
 * gauge_db.c
 *      Contains routines for creating, adding, retrieving, and deleting
 *      rain gauge database.  
 *    
 *    Requires:
 *       gdbm
 *       gv_utils
 *
 *    Note: We use tables to store the gauge data (similar to relational
 *             tables).
 *      table 1 contains: key:     1 netID gaugeID 
 *                        content: ngID etime_sec
 *      table 2 contains: key:     2 ngID time (binary) 
 *                        content: rain_rate (in string)
 *                 where,
 *                   2    = sizeof(char) byte.
 *                   ngID = sizeof(int) bytes,
 *                   time = sizeof(time_t) bytes
 *                   rain_rate = sizeof(rain rate string) bytes
 *
 *      table 3 contains: key:     3 ngID month    -- This table helps
 *                        content: year1 year2...  -- to speed up determining
 *                                                 -- whether rain rate is 
 *                                                 -- zero or missing.
 *
 *         Note: key is prefixed with the 'table #'.
 *
 *    Note: This db only keeps record of the collection period end time; thus,
 *          the collection period time is:
 *              [1/1/70 0:0:0, latest_rain_rate_time] -- in seconds.
 *
 *    Assumptions:
 *       1. Gauge data file (month worth of data) doesnot contain entry for 
 *          rain rate of value 0.
 *       2. When the gauge data file for a particular month doesnot exist,
 *          we consider all rain rates in this month missing.
 *       3. Rain rate < MISSING_RAIN_RATE for missing rain rates.
 *
 * Note: A gdbm database can be opened by at most one writer at a time. 
 *       However, many readers may open the database open simultaneously. 
 *       Readers and writers can not open the gdbm database at the same time. 
 *--------------------------------------------------------------------------
 *
 *  By:
 *
 *     Ngoc-Thuy Nguyen
 *     Science Systems and Applications, Inc. (SSAI)
 *     NASA/TRMM Office
 *     nguyen@trmm.gsfc.nasa.gov
 *     November 19, 1997
 * 
 *     Copyright (C) 1997-1998
 *
 ***************************************************************************/ 
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <time.h>


#include <gdbm.h>
#include <gv_utils.h>

#include "gauge_db.h"

#define MAX_STR_LEN 100
#define MAX_NGID_COUNT_KEY "MAX_NGID_COUNT"
#define MAX_LINE_LEN 300
#ifndef DEBUG_GAUGE_DB
static int verbose = 0;
#else
extern int verbose;
#endif

int create_table1_key(GDBM_FILE dbf, char *netID, char *gaugeID,
				datum *key);
int get_or_create_ngID(GDBM_FILE dbf, char *netID, char *gaugeID, 
			   char read_write_flag, int *ngID);
/**********************************************************************/
/*                                                                    */
/*                           gauge_db_open                            */
/*                                                                    */
/**********************************************************************/
GDBM_FILE  gauge_db_open(char *gauge_db_name, char read_write_flag)
{
  /* Open the gauge data base depending on specified 
   * read_write_flag. The database will be created if it does not exist and 
   * the flag is 'w'.
   *   flag: r, w
   */

  GDBM_FILE dbf = NULL;
  int mode = 0664;
  int block_sz = 512;
  int read_write;

  if (gauge_db_name == NULL || strlen(gauge_db_name) == 0)
	return dbf;
  if (read_write_flag == 'r')
	read_write = GDBM_READER;
  else if (read_write_flag == 'w') {
	read_write = GDBM_WRITER;
	read_write |= GDBM_FAST;       /* Need to sync the data when exit */

  }
  else
	return dbf;
  dbf = gdbm_open(gauge_db_name, block_sz, read_write, mode, 0);
  if (dbf == NULL && (read_write & GDBM_WRITER)) {
	/* Create the db; it doesnot exist.*/
	read_write = GDBM_WRCREAT;
	read_write |= GDBM_FAST;    /* Need to sync the data when exit */
	if (verbose)
	  fprintf(stderr, "Creating a new DB...\n");
	dbf = gdbm_open(gauge_db_name, block_sz, read_write, mode, 0);

  }

  return dbf;
} /* gauge_db_open */


/**********************************************************************/
/*                                                                    */
/*                       create_table3_key                            */
/*                                                                    */
/**********************************************************************/
int create_table3_key(GDBM_FILE dbf, char *netID, char *gaugeID, 
					  char read_write_flag, int mon, datum *key)
{
  /* Create table3's key: key: 3 ngID month
   * Return 1 for successful; -1, otherwise.
   */
  int ngID = 0;

  if (netID == NULL || gaugeID == NULL || strlen(netID) == 0 || 
	  strlen(gaugeID) == 0 || key == NULL || key->dptr == NULL ||
	  mon < 1 || mon > 12)
	return -1;
  if (get_or_create_ngID(dbf, netID, gaugeID, read_write_flag, &ngID) < 0) 
	return -1;

  sprintf(key->dptr, "3 %d %d",  ngID, mon);
  key->dsize = strlen(key->dptr) + 1;    /* Including '\0' */
  return 1;
} /* create_table3_key */

/**********************************************************************/
/*                                                                    */
/*                       add_info_to_table_3                          */
/*                                                                    */
/**********************************************************************/
int add_info_to_table_3(GDBM_FILE dbf, char *netID, 
				 char *gaugeID, time_t rr_time)
{
  /* Add/modify an entry in the 3rd table.
   *   key: 3 ngID month
   *   content: year1 year2 ...yearN
   * Return 1 for successful; -1, otherwise.
   */
  char content_str[MAX_STR_LEN];
  datum key, content;
  int rc;
  int year = 0, mon = 0;
  char year_str[MAX_NAME_LEN];
  char key_str[MAX_STR_LEN];

  if (netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0)
	return -1;
  gv_utils_get_month_year_for_time(rr_time, &mon, &year);
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (create_table3_key(dbf, netID, gaugeID, 'w', mon, &key) < 0) 
	return -1; /* Not exist*/

  memset(content_str, '\0', MAX_STR_LEN);
  content = gdbm_fetch(dbf, key);
  if (content.dptr != NULL) {
    memset(year_str, '\0', MAX_NAME_LEN);
	sprintf(year_str, "%4d", year);
	if (strstr(content.dptr, year_str) == NULL) {
	  /* Year is not in the list, append to list. */
	  sprintf(content_str, "%s %4d", content.dptr, year);
	}
	else {
	  /* Year already in the list. */
	  strcpy(content_str, content.dptr);
	}
	free(content.dptr);
  }
  else {
	sprintf(content_str, "%4d", year);
  }

  /* Reuse content */
  content.dptr = content_str;
  content.dsize = strlen(content.dptr) + 1;  /* Including '\0' */
  rc = gdbm_store(dbf, key, content, GDBM_REPLACE);

  if (rc < 0) 
	return -1;

  return 1;
  
} /* add_info_to_table_3 */

/**********************************************************************/
/*                                                                    */
/*                       gauge_db_add                                 */
/*                                                                    */
/**********************************************************************/
int gauge_db_add(GDBM_FILE dbf, char *netID, 
				 char *gaugeID, char *rate_str, time_t rr_time)
{
  /* Add a new rain rate to the database.
   *  Key: 2 netID gaugeID time 
   * Data: Rain_rate (in string)
   *  Will add/update an entry to table 3 too.
   * Return 1 for successful; -1, otherwise.
   * Note: This routine will replace the duplicated entry.
   */

  datum key, content;
  int rc;
  char key_str[MAX_STR_LEN];

  if (rate_str == NULL || strlen(rate_str) == 0 || netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0)
	return -1;


  /* Key: 2 ngID time (in binary)
   * Data: rate (in string)
   */
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (verbose)
	fprintf(stderr, "Creating key ...\n");
  if (gauge_db_create_table2_key(dbf, netID, gaugeID, rr_time, 'w', &key) < 0) {
	if (verbose) 
	  fprintf(stderr, "Failed to create key for table 2.\n");
	return -1;
  }

  if (verbose)
	fprintf(stderr, "key: <%s>, rate_str: %s, time: %d\n", key.dptr,rate_str, rr_time);

  content.dptr = rate_str;
  content.dsize = strlen(content.dptr) + 1;  /* Including '\0' */
  /*
fprintf(stderr, "NNNN = %d\n", n++);
*/
  if (verbose)
	fprintf(stderr, "Storing content, key len %d content len: %d\n", key.dsize, content.dsize);
  rc = gdbm_store(dbf, key, content, GDBM_REPLACE);

  if (rc < 0) 
	return -1;

  /* Add an entry to table 3 contained gauge's month and year.
   */
  if (add_info_to_table_3(dbf, netID, gaugeID, rr_time) < 0) 
	return -1;

  return 1;
} /* gauge_db_add */


/**********************************************************************/
/*                                                                    */
/*                       gauge_db_entry_exists_for_this_month         */
/*                                                                    */
/**********************************************************************/ 
int gauge_db_entry_exists_for_this_month(GDBM_FILE dbf,  char *netID, char 
									 *gaugeID, time_t rr_time)
{
  /* Return 1 if there is data in the database occurred in
   * the month of rr_time; 0, otherwise.
   *  Note: This routine check data from table 3.
   */
  static int save_mon = 0, nyears = 0;
  static char save_gaugeID[MAX_NAME_LEN], save_netID[MAX_NAME_LEN];
  static int years_list[MAX_YEAR_NUM];
  int mon = 0, year = 0;
  datum key, content;
  char *tok, *tmp_str;
  int i;
  char key_str[MAX_STR_LEN];

  /* Algorithm:
   *   1. Save statically the month, gaugeID, netID..
   *   2. Fetch from the databse the list of years for rr_time's month --
   *      store this list statically. Fetch again for different month only.
   *   3. return 1 if there is an entry for this month and the year exists
   *      in the year list; 0, otherwise.
   */
  gv_utils_get_month_year_for_time(rr_time, &mon, &year);
  if (mon == 0 || year == 0) return 0;
  if (save_mon == 0 ||
	  (mon != save_mon || strcmp(save_gaugeID, gaugeID) != 0 ||
	   strcmp(save_netID, netID) != 0)) {
	memset(key_str, '\0', MAX_STR_LEN);
	key.dptr = key_str;
	key.dsize = 0;
	/* Re-fetch for new month */
	if (create_table3_key(dbf, netID, gaugeID, 'r', mon, &key) < 0)
	  return 0; /* No entry */

	content = gdbm_fetch(dbf, key);
	if (content.dptr == NULL) 
	  return 0; /* No Entry */

	/* Parse year from string and store as int in years list */
	memset(years_list, 0, sizeof(years_list)); /* Initialize*/
	tok = strtok(content.dptr, " ");
	tmp_str = content.dptr;
	i = 0;
	while (tok) {
	  years_list[i] = atoi(tok);
	  tok = strtok(NULL, " ");
	  i++;
	}
	nyears = i;
	if (tmp_str) free(tmp_str);
	/* Save */
	save_mon = mon;
	strcpy(save_netID, netID);
	strcpy(save_gaugeID, gaugeID);
  }
  
  for (i = 0; i< nyears; i++) {
	if (years_list[i] == year) 
	  return 1; /* year for this month found */
  }

  return 0;
  
}  /* gauge_db_entry_exists_for_this_month */

/**********************************************************************/
/*                                                                    */
/*                            gauge_db_fetch                          */
/*                                                                    */
/**********************************************************************/
int gauge_db_fetch(GDBM_FILE dbf, char *netID, char *gaugeID,
				   time_t rr_time, char *rr_rate_str)
{
  /* Get the rain rate from the database for the specified netID,
   * gaugeID, and rr_time.
   * return 1 for non-missing rain rate; 
   *        2 for misisng rain rate;
   *        0 for rain rate=0; 
   *        -1, otherwise.
   * Note: 
   *   The rain rate is missing when:
   *    1. There is no rate entry in the DB for rr_time's month and year, or
   *    2. There is no gauge info for this particular gauge ID & netID
   *       in the db, or
   *    3. The rate entry in the db is less than MISSING_RAIN_RATE.
   *   The rain rate = 0.0 when:
   *    There is no entry for it in the database but it's not missing 
   *    as defined above. -- Assuming that the gauge database doesnot 
   *    contain entry for rain rate of value 0.
   */
  datum key;
  datum content;
  float rr;
  char key_str[MAX_STR_LEN];

  if (netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0 ||
	  rr_rate_str == NULL)
	return -1;

  if (gauge_db_gauge_exists(dbf, netID, gaugeID) == 0) {
	/* Gauge file for this netID and gaugeID does not exist.
	 * set rain rate to MISSING_RAIN_RATE.
	 */

	sprintf(rr_rate_str, "%.2f", MISSING_RAIN_RATE);
	return 2;
  }
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (gauge_db_create_table2_key(dbf, netID, gaugeID, rr_time, 'r', &key) < 0)
	return -1;

  content = gdbm_fetch(dbf, key);
  if (content.dptr != NULL) {
	rr = atof(content.dptr);
	/* Check if rr is less than the missing rate */
	if (rr <= MISSING_RAIN_RATE) {
	  free(content.dptr);

	  goto MISSING;
	}

	strcpy(rr_rate_str, content.dptr);
	if (verbose)
	  fprintf(stderr, "netID: %s gauge ID: %s time: %s RATE: <%s>\n", netID, gaugeID, (char *)ctime(&rr_time), rr_rate_str);

	free(content.dptr);
	return 1;
  }
  if (gauge_db_entry_exists_for_this_month(dbf, netID, gaugeID, rr_time)) {
	/* Yes, there is an entry for this month. */
	sprintf(rr_rate_str, "%.2f", 0.0);
	return 0;
  }

MISSING:
  sprintf(rr_rate_str, "%.2f", MISSING_RAIN_RATE);
  return 2;
} /* gauge_db_fetch */

/**********************************************************************/
/*                                                                    */
/*                             gauge_db_delete                        */
/*                                                                    */
/**********************************************************************/
int gauge_db_delete(GDBM_FILE dbf, char *netID, char *gaugeID,
				   time_t rr_time)
{
  /* Delete the entry in the database for the specified netID,
   * gaugeID, and rr_time.
   * return 1 for successful; -1, otherwise.
   */
  datum key;
  int rc;
  char key_str[MAX_STR_LEN];

  if (netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0)
	return -1;
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (gauge_db_create_table2_key(dbf, netID, gaugeID, rr_time, 'r', &key) < 0)
	return -1;
  rc = gdbm_delete(dbf, key);

  if (rc == 0) return 1; /* Successfully deleted. */
  return -1;
} /* gauge_db_delete */

/**********************************************************************/
/*                                                                    */
/*                         gauge_change_max_ngid_count_in_db                */
/*                                                                    */
/**********************************************************************/
int gauge_change_max_ngid_count_in_db(GDBM_FILE dbf, int count)
{
  /*  Write the max count of the Net gauge ID to the db.
   * Return 1 for successful; -1, otherwise.
   */
  datum key, content;
  char ngID_str[MAX_STR_LEN];

  if (dbf == NULL) return -1;

  key.dptr = MAX_NGID_COUNT_KEY;
  key.dsize = strlen(key.dptr) + 1;
  memset(ngID_str, '\0', MAX_STR_LEN);
  sprintf(ngID_str, "%d", count);
  content.dptr = ngID_str;
  content.dsize = strlen(content.dptr) + 1;
  gdbm_store(dbf, key, content, GDBM_REPLACE);
  return 1;
} /* gauge_change_max_ngid_count_in_db */

/**********************************************************************/
/*                                                                    */
/*                         gauge_get_max_ngid_count_from_db                 */
/*                                                                    */
/**********************************************************************/
int gauge_get_max_ngid_count_from_db(GDBM_FILE dbf, int *count)
{
  /*  Get the max count of net gauge ID from the db. Return 1 for successful, 0 for no entry,
   * -1 for failure.
   */
  datum key, content;
  int rc = -1;
  int count_i=0;

  if (dbf == NULL) return -1;

  key.dptr = MAX_NGID_COUNT_KEY;
  key.dsize = strlen(key.dptr) + 1;
  content = gdbm_fetch(dbf, key);
  if (content.dptr) {

	if (sscanf(content.dptr, "%d", &count_i) != 1) {
	  fprintf(stderr, "Max count entry is obsolete.\n");
	}
	else {
	  *count = count_i;
	  rc = 1;
	}
	free(content.dptr);
  }
  else
	rc = 0;

  return rc;
} /* gauge_get_max_ngid_count_from_db */

/**********************************************************************/
/*                                                                    */
/*                         get_or_create_ngID                         */
/*                                                                    */
/**********************************************************************/
int get_or_create_ngID(GDBM_FILE dbf, char *netID, char *gaugeID, 
			   char read_write_flag, int *ngID)
{
  /* Get or create ngID for netID and gaugeID_i. Return 1 if sucessful; -1, 
   * otherwise (i.e., gauge doesn't exist).
   */
  char key_str[MAX_STR_LEN], tmp_str[MAX_STR_LEN];
  datum key, content;
  int rc;
  int ngID_count = 0;

  if (dbf == NULL || netID == NULL || gaugeID == NULL || ngID == NULL) 
	return -1;

  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  /*
   *      table 1: content: ngID etime_sec
   */
  if (create_table1_key(dbf, netID, gaugeID, &key) < 0)  return -1;
  content = gdbm_fetch(dbf, key);
  if (content.dptr) {
	/* Found the key for the rate table */
	if (sscanf(content.dptr, "%d %*d", &ngID_count) != 1) {
	  free(content.dptr);
	  return -1;
	}
	free(content.dptr);
  }
  else if (read_write_flag == 'w') {
	/* Create a new key for the rate table (ngID for netID and gaugeID)
	 * Only if write is specified. 
	 */
	if ((rc = gauge_get_max_ngid_count_from_db(dbf, &ngID_count)) == 0) {
	  /* The database is new, add an entry for max count. */
	  ngID_count = 1;
	}
	else if (rc == 1) {
	  /* max count is found, increment it */
	  ngID_count++;
	}
	else 
	  return -1;
	if (verbose)
	  fprintf(stderr, "NEW ngID_count: %d\n", ngID_count);

	/* Update the db. */
	gauge_change_max_ngid_count_in_db(dbf, ngID_count);
	/* Add an entry to the net_gauge table: just store ngID_count and 0 for 
	 * time--will modify time later. 
	 */
	memset(tmp_str, '\0', MAX_STR_LEN);
	sprintf(tmp_str, "%d %d", ngID_count, 0);
	content.dptr = tmp_str;
	content.dsize = strlen(content.dptr) + 1;
	if (verbose)
	  fprintf(stderr, "Calling gdbm_store...\n");
	gdbm_store(dbf, key, content, GDBM_INSERT);
  }
  else
	return -1;
  if (verbose)
	fprintf(stderr, "ngID = %d\n", ngID_count);

  *ngID = ngID_count;
   return 1;
} /* get_or_create_ngID */


/**********************************************************************/
/*                                                                    */
/*                          create_table1_key                         */
/*                                                                    */
/**********************************************************************/
int create_table1_key(GDBM_FILE dbf, char *netID, char *gaugeID,
				datum *key)
{
  /* Set table1's key. key_str: 1 netID gaugeID. Return 1 for successful; -1,
   * otherwise.
   */
  if (dbf == NULL || netID == NULL || gaugeID == NULL || key == NULL ||
	  key->dptr == NULL) 
	return -1;

  /* Convert gaugeID to int to save space--doesnot contain leading zeros. */
  sprintf(key->dptr, "1 %s %d",  netID, atoi(gaugeID));
  key->dsize = strlen(key->dptr) + 1;    /* Including '\0' */
  return 1;

} /* create_table1_key */

/**********************************************************************/
/*                                                                    */
/*                    gauge_db_create_table2_key                      */
/*                                                                    */
/**********************************************************************/
int gauge_db_create_table2_key(GDBM_FILE dbf, char *netID, char *gaugeID,
				time_t rr_time, char read_write_flag, datum *key)
{
  /* Construct Key: netID gaugeID time. the subsequent call will use the
   * same memory space of key->dptr.
   * Return 1 for successful; -1, otherwise.
   */
  int len;
  int ngID = 0;

  if (key == NULL || key->dptr == NULL || netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0)
	return -1;
  /* Find a key to the rain rate table for this netID and gaugeID.
   * Create a new key if it doesnt exist.
   *   There are two tables
   *   table 1 contains: key: 1 netID gaugeID content: ngID etime_sec
   *   table 2 contains: key: 2 ngID time (in binary)  content: rain_rate (in string)
   *    Note: key is prefixed with the table #.
   */

  if (get_or_create_ngID(dbf, netID, gaugeID, read_write_flag, &ngID) < 0) 
	return -1;
  /* key: 2 ngID time */
  /* Use memcpy instead of strcpy since we don't want '\0' in the middle of
   *  key->dptr
   */
  memcpy(key->dptr, "2 ", sizeof(char)*2);       /* '2 '*/
  len = sizeof(char)*2;                            
  memcpy(key->dptr+len, &ngID, sizeof(int));         /* Append 'ngID' */
  len += sizeof(int);

  memcpy(key->dptr+len, " ", sizeof(char));      /* Append ' ' */
  len += sizeof(char);                       

  /* Store time in binary  -- To save space. */
  memcpy(key->dptr+len, &rr_time, sizeof(time_t)); /* Append time */
  len += sizeof(time_t);
  key->dptr[len] = '\0';       /* End of string char. */

  key->dsize = len + 1;    /* Including '\0' */

  return 1;
  
} /* gauge_db_create_table2_key_str */

/**********************************************************************/
/*                                                                    */
/*                            gauge_db_close                          */
/*                                                                    */
/**********************************************************************/
void gauge_db_close(GDBM_FILE dbf, char read_write_flag)
{
  /* Close the database. */

  if (dbf == NULL) return;

  if (verbose)
	fprintf(stderr, "Closing gauge db...\n");
  if (read_write_flag == 'w') 
	gdbm_sync(dbf);   /* synchronize the data on disk since it used GDBM_FAST 
					   * option in open.
					   */
  gdbm_close(dbf);
} /* gauge_db_close */

/**********************************************************************/
/*                                                                    */
/*                           gauge_db_write_to_disk                   */
/*                                                                    */
/**********************************************************************/
void gauge_db_write_to_disk(GDBM_FILE dbf)
{
  gdbm_sync(dbf);   /* synchronize the data on disk since it used GDBM_FAST 
					 * option in open.
					 */
}
/**********************************************************************/
/*                                                                    */
/*                        gauge_db_entry_exists                       */
/*                                                                    */
/**********************************************************************/
int gauge_db_entry_exists(GDBM_FILE dbf,  char *netID, char *gaugeID,
				   time_t rr_time)
{
  /* Return 1 if there is an entry in the database for the specified netID,
   * gaugeID, and rr_time; 0, otherwise.
   */
  datum key;
  int rc;
  char key_str[MAX_STR_LEN];

  if (netID == NULL ||
	  gaugeID == NULL || strlen(netID) == 0 || strlen(gaugeID) == 0)
	return 0;
  if (verbose)
	fprintf(stderr, "Checking if entry exist...\n");
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (gauge_db_create_table2_key(dbf, netID, gaugeID, rr_time, 'r', &key) < 0)
	return 0;
  rc = gdbm_exists(dbf, key);

  return rc;
} /* gauge_db_entry_exists */


/**********************************************************************/
/*                                                                    */
/*                       gauge_db_gauge_exists                        */
/*                                                                    */
/**********************************************************************/
int gauge_db_gauge_exists(GDBM_FILE dbf,  char *netID, char *gaugeID)
{
  /* Return 1 if the gauge for the specified netID and gaugeID exist; 0,
   * otherwise.
   */
  datum key, content;
  int ngID_count=0, exist = 0;
  char key_str[MAX_STR_LEN];

  if (dbf == NULL || netID == NULL || gaugeID == NULL) return 0;
  if (verbose)
	fprintf(stderr, "Checking if gauge netID<%s> gaugeID <%s> exists...\n",
			netID, gaugeID);
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (create_table1_key(dbf, netID, gaugeID, &key) < 0) return 0; /* Not exist*/
  content = gdbm_fetch(dbf, key);
  if (content.dptr) {
	if (sscanf(content.dptr, "%d", &ngID_count) == 1) {
	  exist = 1;
	}
	/* Gauge exist. */
	free(content.dptr);
  }
  return exist;

} /* gauge_db_gauge_exists */

/**********************************************************************/
/*                                                                    */
/*                  gauge_construct_default_db_name                   */
/*                                                                    */
/**********************************************************************/
void gauge_construct_default_db_name(char *gauge_db_name)
{
   /* Construct the gauge database name: $GVS_DB_PATH/gauge.gdbm.
	* $GVS_DB_PATH="/usr/local/trmm/GVBOX/data/db" if it's not defined.
    */
   char *path, *db_name;
 
   if (gauge_db_name == NULL) return;
   path = getenv("GVS_DB_PATH");
   if (path == NULL)
       path = "/usr/local/trmm/GVBOX/data/db";
   db_name = "gauge.gdbm";
 
   sprintf(gauge_db_name, "%s/%s", path, db_name);
}

/**********************************************************************/
/*                                                                    */
/*                        gauge_db_fetch_range                        */
/*                                                                    */
/**********************************************************************/
int gauge_db_fetch_range(GDBM_FILE dbf, char *netID, char *gaugeID, 
						 time_t stime_sec,
						 time_t etime_sec, 
						 char *non_missingNnon_zero_rain_rates_str, 
						 char *zero_rain_rates_str, 
						 char *rain_rates_str, 
						 int *n_non_missingNnon_zero_rain_rates, 
						 int *n_zero_rain_rates, int *nrain_rates)
{
  /* Get gauge rain rates for the given network ID, gauge_id, from the 
   * start time to end time from the gauge database. 
   * Set n_non_missingNnon_zero_rain_rates to the number of rain rates 
   * not missing nor zero.
   * Set n_zero_rain_rates to the number of rain rates of zero.
   * Set nrain_rates to the total number of rain rates including the
   * missing and zero ones.
   * non_missingNnon_zero_rain_rates_str contains only non missing and non zero
   * rain rates.
   * zero_rain_rates_str contains only non zero rain rates.
   * rain_rates_str contains each rain rate per minute for the time range 
   * interval (It includes MISSING_RAIN_RATE_STR for missing rain rate and
   * zero).
   * non_missingNnon_zero_rain_rates_str, zero_rain_rates_str, or 
   * rain_rates_str may be NULL.
   * Return 1 upon successful; -1 otherwise.
   */
  time_t time_sec, rounded_time_sec = 0;
  int rc = 1;
  char rate_str[MAX_STR_LEN];

  if (dbf == NULL || gaugeID == NULL || netID == NULL ||
	  n_non_missingNnon_zero_rain_rates == NULL ||
	  n_zero_rain_rates == NULL || nrain_rates == NULL)
	return -1;

  if (verbose)
	fprintf(stderr, "Fetching range netID <%s> gaugeID <%s>\n", netID, gaugeID);
  round_time_to_the_minute(stime_sec, &rounded_time_sec);

  for (time_sec = rounded_time_sec; time_sec <= etime_sec; time_sec += 60) {

	/* Check for rain rate in every minute from the rounded start time to
	 * end time
	 */
	memset(rate_str, '\0', MAX_STR_LEN);
	if ((rc = gauge_db_fetch(dbf, netID, gaugeID, time_sec, rate_str)) < 0) 
	  /* Failure occurred. */
	  return -1;
	else if (rc == 1 || rc == 0 || rc == 2) {

	  (*nrain_rates)++;
	  if (rain_rates_str != NULL) {
		strcat(rain_rates_str, rate_str);
		strcat(rain_rates_str, " ");
	  }

	  if (rc == 1) {
		/* Rain rate is not mising nor zero */
		(*n_non_missingNnon_zero_rain_rates)++;

		if (non_missingNnon_zero_rain_rates_str != NULL) {
		  strcat(non_missingNnon_zero_rain_rates_str, rate_str);
		  strcat(non_missingNnon_zero_rain_rates_str, " ");
		}
	  }
	  else if (rc == 0) {
		/* Rain rate is zero.  */
		(*n_zero_rain_rates)++;
		if (zero_rain_rates_str != NULL) {
		  strcat(zero_rain_rates_str, rate_str);
		  strcat(zero_rain_rates_str, " ");
		}
	  }

	}

	
  } /* for */

  return 1;
} /* gauge_db_fetch_range */



/**********************************************************************/
/*                                                                    */
/*                       gauge_db_get_collection_end_time             */
/*                                                                    */
/**********************************************************************/
time_t gauge_db_get_collection_end_time(GDBM_FILE dbf, 
										 char *gaugeID, char *netID)
{
  /* Get the collection end time in seconds for the specified gauge.
   * Return 0 for failure; etime for successful.
   */
  time_t etime = 0;
  char key_str[MAX_STR_LEN];
  datum key, content;

  if (dbf == NULL || gaugeID == NULL || netID == NULL) return 0;

  if (verbose)
	fprintf(stderr, "Getting db collection end time netID <%s>, gaugeID <%s>\n", netID, gaugeID);
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  /* Get the time in the content of table 1 */
  if (create_table1_key(dbf, netID, gaugeID, &key) < 0)  return 0;
  content = gdbm_fetch(dbf, key);
  if (content.dptr) {
	if (sscanf(content.dptr, "%*d %d", &etime) != 1) {
	  free(content.dptr);
	  return 0;
	}
	free(content.dptr);
  }
  return etime;
  
} /* gauge_db_get_collection_end_time */


/**********************************************************************/
/*                                                                    */
/*                        gauge_db_is_within_collection_period        */
/*                                                                    */
/**********************************************************************/
int gauge_db_is_within_collection_period(GDBM_FILE dbf, time_t time_sec, 
										 char *gaugeID, char *netID)
{
  /* Return 1 if the specified time_sec is <= the end time of the collection 
   * period for this gauge; 0, otherwise.
   */
  time_t etime;

  if (dbf == NULL || gaugeID == NULL || netID == NULL) return 0;
  
  if (verbose)
	fprintf(stderr, "Checking if netID <%s> gaugeID <%s> within collection period.\n", netID, gaugeID);
  etime = gauge_db_get_collection_end_time(dbf, gaugeID, netID);

  if (time_sec <= etime) return 1;
  return 0;
  
} /* gauge_db_is_within_collection_period */


/**********************************************************************/
/*                                                                    */
/*                      gauge_db_update_collection_end_time           */
/*                                                                    */
/**********************************************************************/ 
int gauge_db_update_collection_end_time(GDBM_FILE dbf, char *netID, 
									 char *gaugeID, time_t time_sec)
{
  /* Set or update the collection end time for this gauge. 
   * Return 1 for successful; -1 otherwise.
   */
  datum content, key;
  char key_str[MAX_STR_LEN], content_str[MAX_STR_LEN];
  int rc, ngID_count=0;
  time_t old_time = 0;

  if (dbf == NULL ||  gaugeID == NULL || netID == NULL) return -1;
  if (verbose)
	fprintf(stderr, "Updating collection end time for netID <%s> gaugeID <%s>.\n", netID, gaugeID);
  /* Update the time in the content of table 1*/
  memset(key_str, '\0', MAX_STR_LEN);
  key.dptr = key_str;
  key.dsize = 0;
  if (create_table1_key(dbf, netID, gaugeID, &key) < 0) return -1; /* Not exist*/
  content = gdbm_fetch(dbf, key);
  if (content.dptr) {
	/* Found the key for the rate table */
	if (sscanf(content.dptr, "%d %d", &ngID_count, &old_time) != 2) {
	  free(content.dptr);
	  return -1;
	}
	free(content.dptr);
	if (old_time >= time_sec) 
	  /* Don't update if time_sec is earlier than the existing time */
	  return 1;

  }
  else {
	/* New entry 
	 * Create a new key for the rate table (ngID for netID and gaugeID)
	 * Only if write is specified. 
	 */
	if (verbose)
	  fprintf(stderr, "New key for the rate table netID <%s> gaugeID <%s>\n", netID, gaugeID);
	if ((rc = gauge_get_max_ngid_count_from_db(dbf, &ngID_count)) == 0) {
	  /* The database is new, add an entry for max count. */
	  ngID_count = 1;
	}
	else if (rc == 1) {
	  /* max count is found, increment it */
	  ngID_count++;
	}
	else 
	  return -1;
	/* Update the db. */
	gauge_change_max_ngid_count_in_db(dbf, ngID_count);
  }
  memset(content_str, '\0', MAX_STR_LEN);
  sprintf(content_str, "%d %d", ngID_count, time_sec);
  content.dptr = content_str;
  content.dsize = strlen(content.dptr) + 1;
  gdbm_store(dbf, key, content, GDBM_REPLACE);

  return 1;

}/* gauge_db_set_collection_end_time */









/**********************************************************************/
/*                                                                    */
/*                    gauge_db_get_info_from_ascii_gauge_file         */
/*                                                                    */
/**********************************************************************/ 
void gauge_db_get_info_from_ascii_gauge_file(char *fname, gauge_file_type_t *gauge_file_type,
							  char *netID, char *gaugeID, 
							  char *site)
{
  /* Get info from 2A-56's header info.
   * Set netID, gaugeID, and/or site if they are not NULL.
   */
  extern FILE *popen( const char *command, const char *type);
  extern int pclose( FILE *stream);
  extern int strcasecmp(const char *s1, const char *s2);

  char cmd[MAX_LINE_LEN];
  char tmp_site[MAX_NAME_LEN], tmp_netID[MAX_NAME_LEN], 
	tmp_gaugeID[MAX_NAME_LEN], ftype[MAX_NAME_LEN];
  FILE *fp;

  if (fname == NULL || gauge_file_type == NULL) return;

  memset(cmd, '\0', MAX_LINE_LEN);
  sprintf(cmd, "head -1 %s", fname);
  fp = (FILE *) popen(cmd, "r");
  if (fp == NULL) return;
  memset(tmp_site, '\0', MAX_NAME_LEN);
  memset(tmp_gaugeID, '\0', MAX_NAME_LEN);
  memset(tmp_netID, '\0', MAX_NAME_LEN);
  memset(ftype, '\0', MAX_NAME_LEN);

  if (fscanf(fp, "%s %s %s %s %*s", ftype, tmp_site, tmp_netID, tmp_gaugeID) != 4) {
	pclose(fp);
	return;
  }
  pclose(fp);
  if (strcasecmp(ftype, "2A-56") == 0) 
	*gauge_file_type = P2A56_FILE;
  else
	return;  /* Not a recognized gauge file. */
  if (netID)
	strcpy(netID, tmp_netID);
  if (site)
	strcpy(site, tmp_site);
  if (gaugeID)
	strcpy(gaugeID, tmp_gaugeID);

} /* gauge_db_get_info_from_ascii_gauge_file */

