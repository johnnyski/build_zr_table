/*
 *
 * gauge_db.h
 *      Contains routines for creating, adding, retrieving, and deleting
 *      rain gauge database.  All rain gauge files will be stored
 *      in the same database.
 *    Requires:
 *       gdbm
 *       gv_utils
 *
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
 *     Copyright (C) 1997
 *
 ***************************************************************************/ 


#ifndef __GAUGE_DB_H__
#define __GAUGE_DB_H__ 1

#include <gdbm.h>
#ifdef MAX_NAME_LEN
#undef MAX_NAME_LEN
#endif
#define MAX_NAME_LEN        51
#define MAX_YEAR_NUM        50
#define MISSING_RAIN_RATE   -501.0

typedef enum { P2A56_FILE, UNKNOWN_FILE} gauge_file_type_t;

/*  gauge_db_open: 
 * Open the gauge data base depending on specified 
 * read_write_flag. The database will be created if it does not exist and 
 * the flag is 'w'.
 *   flag: r, w
 */
GDBM_FILE gauge_db_open(char *gauge_db_name, char read_write_flag);

/* gauge_db_sync: synchronize the data on disk since it used GDBM_FAST 
 * option in open.
 */
void gauge_db_sync (GDBM_FILE dbf);

/* gauge_db_add: Add a new rain rate to the database.
 *  Key: netID gaugeID time 
 * Data: Rain_rate (in string)
 * Return 1 for successful; -1, otherwise.
 * Note: This routine will replace the duplicated entry.
 */
int gauge_db_add(GDBM_FILE dbf, char *netID, 
				 char *gaugeID, char *rate_str, time_t rr_time);

/* gauge_db_fetch: 
 * Get the rain rate from the database for the specified netID,
 * gaugeID, and rr_time.
 * return 1 for non-missing rain rate; 2 for misisng rain rate;
 * 0 for rain rate=0; -1, otherwise.
 * Note: 
 *   The rain rate is missing when:
 *    1. rr_time is > the end time of the collection period or
 *    2. There is no guage in the db.
 *   The rain rate = 0 when:
 *    There is no entry for it in the database but it's not missing 
 *    as defined above. -- Assuming that the gauge data file doesnot 
 *    contain entry for rain rate of value 0.
  */
int gauge_db_fetch(GDBM_FILE dbf, char *netID, char *gaugeID,
				   time_t rr_time, char *rr_rate_str);


/* gauge_db_delete: Delete the entry in the database for the specified netID,
 * gaugeID, and rr_time.
 * return 1 for successful; -1, otherwise.
 */
int gauge_db_delete(GDBM_FILE dbf, char *netID, char *gaugeID,
				   time_t rr_time);

/* gauge_db_create_table2_key: 
 * Construct table2's Key: netID gaugeID time. the subsequent 
 * call will use the same memory space of key->dptr.
 * return 1 for successful; -1, otherwise.
 */
int gauge_db_create_table2_key(GDBM_FILE dbf, char *netID, char *gaugeID,
				time_t rr_time, char read_write_flag, datum *key);

/* gauge_db_close: Close the database. */
void gauge_db_close(GDBM_FILE dbf, char read_write_flag);

/* gauge_db_entry_exists: 
 * Return 1 if there is an entry in the database for the specified netID,
 * gaugeID, and rr_time; 0, otherwise.
 */
int gauge_db_entry_exists(GDBM_FILE dbf,  char *netID, char *gaugeID,
				   time_t rr_time);

/* gauge_db_gauge_exists: 
 * Return 1 if the gauge for the specified netID and gaugeID exist; 0,
 * otherwise.
 */
int gauge_db_gauge_exists(GDBM_FILE dbf,  char *netID, char *gaugeID);

/* gauge_construct_default_db_name: 
 *  Construct the gauge database name: $GVS_DB_PATH/gauge.gdbm.
 * $GVS_DB_PATH="/user/local/trmm/GVBOX/data/db" if it's not defined.
 */
void gauge_construct_default_db_name(char *gauge_db_name);

/* gauge_db_fetch_range: 
 * Get gauge rain rates for the given network ID, gauge_id, from the 
   * start time to end time from the gauge database. 
   * Set n_non_missingNzero_rain_rates to the number of rain rates 
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
int gauge_db_fetch_range(GDBM_FILE gauge_dbf, char *netID, char *gaugeID, 
						 time_t stime_sec, time_t etime_sec, 
						 char *non_missingNnon_zero_rain_rates_str, 
						 char *zero_rain_rates_str, 
						 char *rain_rates_str, 
						 int *n_non_missingNnon_zero_rain_rates, 
						 int *n_zero_rain_rates, int *nrain_rates);


/* gauge_change_max_ngid_count_in_db:
 * Write the max count of the Net gauge ID to the db.
 * Return 1 for successful; -1, otherwise. 
 **/
int gauge_change_max_ngid_count_in_db(GDBM_FILE dbf, int count);
/* gauge_get_max_ngid_count_from_db:
 * Get the max count of net gauge ID from the db. Return 1 for successful, 0 
 * for no entry, -1 for failure.
 */
int gauge_get_max_ngid_count_from_db(GDBM_FILE dbf, int *count);

/* gauge_db_update_collection_end_time:
 * Set or update the collection end time for this gauge. 
 * Return 1 for successful; -1 otherwise.
 */
int gauge_db_update_collection_end_time(GDBM_FILE dbf, char *netID, 
									 char *gaugeID, time_t time_sec);

/* gauge_db_is_within_collection_period:
 * Return 1 if the specified time_sec is <= the end time of the collection 
 * period for this gauge; 0, otherwise.
 */
int gauge_db_is_within_collection_period(GDBM_FILE dbf, time_t time_sec, 
										 char *gaugeID, char *netID);

/* gauge_db_write_to_disk:
 * synchronize the data on disk since it used GDBM_FAST 
 * option in open.
 */
void gauge_db_write_to_disk(GDBM_FILE dbf);

/* gauge_db_get_info_from_ascii_gauge_file: Get info from 2A-56's header info.
 * Set netID, gaugeID, and/or site if they are not NULL.
 */
void gauge_db_get_info_from_ascii_gauge_file(char *fname, gauge_file_type_t *gauge_file_type,
							  char *netID, char *gaugeID, 
							  char *site);


#endif
