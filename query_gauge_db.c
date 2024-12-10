/* query_gauge_db.c
 *
 *     Program queries rain rates in the gauge DB and them to stdout.
 * 
 * Note:  The rain rate for each minute between the from\n
 *        and to times will be outputed to stdout.
 *
 *--------------------------------------------------------------------------
 *
 *  By:
 *
 *     Ngoc-Thuy Nguyen
 *     Science Systems and Applications, Inc. (SSAI)
 *     NASA/TRMM Office
 *     nguyen@trmm.gsfc.nasa.gov
 *     March 31, 1998
 * 
 *     Copyright (C) 1998
 *
 ***************************************************************************/ 

#include <stdio.h>
#include <gdbm.h>
#include <malloc.h>
#include <string.h>

#include <gv_utils.h>
#include "gauge_db.h"


int verbose = 0;

void usage(char *prog)
{

  if (prog == NULL)
	prog = "";

  fprintf(stderr, "Usage (%s): Query Rain Rates from Gauge DB.\n", PROG_VERSION);
  fprintf(stderr, "   %s [-v] [-f gauge_db_file]\n"
		          "        from_time to_time netID gaugeID\n"
                  "     where,\n"
                  "      from_time, to_time := mm/dd/yy[yy] hh:mm:ss\n", prog);
  fprintf(stderr, "     -v: Show execution messages.\n"
                  "     -f: Specify the gauge database. Default:$GVS_DB_PATH/gauge.gdbm\n");
  fprintf(stderr, "   Note: The rain rate for each minute between the from\n"
                  "         time and the to time will be outputed to stdout, separated by space.\n");
  exit(-1);
} /* usage */


/**********************************************************************/
/*                                                                    */
/*                          process_argvs                             */
/*                                                                    */
/**********************************************************************/
void process_argvs(int argc, char **argv, 
				   char *gauge_db_file, char *netID, char *gaugeID,
				   time_t *rr_stime, time_t *rr_etime)
{
  extern char *optarg;
  extern int optind, optopt;
  extern int getopt(int argc, char * const argv[],
					const char *optstring);

  char *date_str, *time_str;
  int c;
  int hr, min, sec, yr, mon, day;

  if (argc < 6) 
	usage(argv[0]);

  while ((c = getopt(argc, argv, "f:v")) != -1) {
	switch (c) {
	case 'v':
	  verbose = 1;
	  break;
	case 'f': strcpy(gauge_db_file, optarg); break;
	case '?': fprintf(stderr, "option -%c is undefined\n", optopt);
	  usage(argv[0]);
    case ':': fprintf(stderr, "option -%c requires an argument\n",optopt);
	  usage(argv[0]);
    default: break;
    }
  }
  /* must have 6 items */
  if (argc - optind != 6) usage(argv[0]);
  date_str = argv[optind++];
  time_str = argv[optind++];
  if (sscanf(date_str, "%d/%d/%d", &mon, &day, &yr) != 3) {
	fprintf(stderr, "Invalid begin date format.\n");
	exit(-1);
  }
  if (sscanf(time_str, "%d:%d:%d", &hr, &min, &sec) != 3) {
	fprintf(stderr, "Invalid begin time format.\n");
	exit(-1);
  }
  date_time_strs2seconds(date_str, time_str, rr_stime);
  date_str = argv[optind++];
  time_str = argv[optind++];
  if (sscanf(date_str, "%d/%d/%d", &mon, &day, &yr) != 3) {
	fprintf(stderr, "Invalid end date format.\n");
	exit(-1);
  }
  if (sscanf(time_str, "%d:%d:%d", &hr, &min, &sec) != 3) {
	fprintf(stderr, "Invalid end time format.\n");
	exit(-1);
  }
  date_time_strs2seconds(date_str, time_str, rr_etime);
  strcpy(netID, argv[optind++]);
  strcpy(gaugeID, argv[optind++]);

} /* process_argvs */

/**********************************************************************/
/*                                                                    */
/*                           main                                     */
/*                                                                    */
/**********************************************************************/
int main (int argc, char **argv)
{
  char gauge_db_name[MAX_FILENAME_LEN];
  time_t rr_stime, rr_etime;
  char gaugeID[MAX_NAME_LEN], netID[MAX_NAME_LEN];
  int rc = 0;
  GDBM_FILE gauge_dbf;
  char *rain_rates_str;
  int nrain_rates=0, n_non_missingNnon_zero_rain_rates=0, n_zero_rain_rates=0;

  set_signal_handlers();

  memset(gauge_db_name, '\0', MAX_FILENAME_LEN);
  gauge_construct_default_db_name(gauge_db_name); /* $GVS_DB_PATH/gauge.gdbm */
  memset(netID, '\0', MAX_NAME_LEN);
  memset(gaugeID, '\0', MAX_NAME_LEN);
  process_argvs(argc, argv, gauge_db_name, netID, gaugeID, &rr_stime, &rr_etime);
  gauge_dbf = gauge_db_open(gauge_db_name, 'r');
  if (gauge_dbf == NULL) {
	fprintf(stderr, "Failed to open %s\n", gauge_db_name);
	exit(-1);
  }
  /* Allocate enough space to store the number of rates separated with space
   * rate: [-]xxx.xx [-]xxx.xx ...
   */

  rain_rates_str = (char *) calloc(1, (int) (((rr_etime-rr_stime)/60) * 8) + 50);
  if (rain_rates_str == NULL) {
	perror("calloc str");
	rc = -1;
	goto DONE;
  }
  if (verbose) {
	fprintf(stderr, "Query for %s %s\n", netID, gaugeID);
	fprintf(stderr, "From %s", ctime(&rr_stime));
	fprintf(stderr, "To   %s", ctime(&rr_etime));
  }
  if (gauge_db_fetch_range(gauge_dbf, netID, gaugeID, rr_stime, rr_etime,
						   NULL, NULL, rain_rates_str,
						   &n_non_missingNnon_zero_rain_rates, &n_zero_rain_rates,
						   &nrain_rates) < 0) {
	fprintf(stderr, "Error querying the gauge db.\n");
	rc = -1;
	goto DONE;
  }


  fprintf(stdout, "%s\n", rain_rates_str);
DONE:
  if (rain_rates_str)
	free(rain_rates_str);
  gauge_db_close(gauge_dbf, 'r');
  gauge_dbf = NULL;
  exit(rc);
  
} /* main */
