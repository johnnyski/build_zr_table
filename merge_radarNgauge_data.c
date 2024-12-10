/*
 * merge_radarNgauge_data.c
 *     This program will merge the first intermediate product of the ZR table
 *     with rain gauge data to create the second intermediate
 *     product of the ZR table.
 *
 *     The file produced here will contain radar windows, rain classifications,
 *     and gauge data.
 *     The product will contain some header information followed with lines of
 *     data.
 *     A header information sample contains:
 * <<header begins>>
 *  Site_name                         MELB
 *  Start_date_time                   10/02/1992 12:00:00
 *  End_date_time                     10/02/1992 12:59:59
 *  Radar_lat                         28.113333
 *  Radar_lon                         -80.654167
 *  Gauge_window_size_in_km_xyz       6.00 6.00 3.00
 *  Rain_class_type                   DUAL
 *  Number_of_rain_types              4
 *  Rain_type_value_1                 0   No_echo
 *  Rain_type_value_2                 1   Stratiform
 *  Rain_type_value_3                 2   Convective
 *  Rain_type_value_4                 -1  Missing/bad
 *  Window_time_range_in_minutes      10
 *  Window_center_offset_in_minutes   2
 * # TABLE RECORD INFORMATION:
 * # Record Format:
 * #                                      _NC pairs__     _NC pairs__      _NR R'
 * s_
 * #                                      |         |     |         |      |     
 *  |
 * # Record:   ID Net Date Time r NH H1 NC C1 Z1 ... H2 NC C1 Z1 ... ... NR R1 ..
 * .
 * # DataType: I   S   D    T   F  I F  I  I  F      F  I  I  F           I  F  
 * #
 * #  where: 
 * #    ID   = Gauge ID         Net  = Gauge Network
 * #    Date = Vos Date         Time = Vos Time
 * #    r    = Range            NH   = Number of Heights
 * #    H    = Height in KM     NC   = Number of Cells
 * #    C    = Rain Type Value  Z    = dBz Reflectivity, -99.0 missing/bad
 * #    NR   = Number of R's    R    = Rain Rate, < 0.0 suspicion, -99.9 missing
 * #  Record Field's Data Type:
 * #    I    = Integer          S    = String         D    = mm/dd/yy
 * #    T    = hh:mm            F    = Float
 * #
 * #   Gauge Window of nCells [(x/2)*(y/2)] Centered at Gauge:
 * #       ------------------------------------------
 * #       | (CN-2,ZN-2) | (CN-1,ZN-1) |   (CN,ZN)  |
 * #       ------------------------------------------
 * #       |    ...      |      ...    |     ...    |
 * #       ------------------------------------------
 * #       |   (C1,Z1)   |   (C2,Z2)   |  (C3,Z3)   |
 * #       ------------------------------------------
 * #
 * #
 * #   Time Window of N Minutes Centered at VOS_time + Offset:
 * #       |--t1--|--t2--|...|--tN--|
 * #       |  R1  |  R2  |...|  RN  |
 * #
 * Table begins:
 * <<header ends>>
 *
 *    Note: 
 *       * If option -n is not used, the entries with one of the following 
 *         characteristics will not be written out to the result 
 *         file (Basically, removed):
 *          * Either radar or gauge data is missing OR
 *          * Both radar and gauge see no rain.
 *          * Radar shows no rain if rain_type = no_ECHO && Z <= 0 &&
 *            Z <= min_valid_Z_value.
 *          * Radar data is missing if Z = MISSING_Z.
 *
 * Program's exit code:
 *          -1 Failed.
 *           0 Successful.
 *          -2 Program was aborted by SIGINT (^C).
 * 
 * Note: 
 *   1.  This program will read gauge data from the gauge database:
 *         "$GVS_DB_PATH/gauge.gdbm".  It will build that database
 *         if it doesnot exist.
 *
 *--------------------------------------------------------------------------
 *
 *  By:
 *
 *     Ngoc-Thuy Nguyen
 *     Science Systems and Applications, Inc. (SSAI)
 *     NASA/TRMM Office
 *     nguyen@trmm.gsfc.nasa.gov
 *     July 3, 1997
 * 
 *     Copyright (C) 1997-1998
 *
 ***************************************************************************/ 

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <IO.h>
#include <gsl.h>
#include <gv_utils.h>
#include <gdbm.h>
#include "gauge_db.h"
#include "zr.h"

/************************  Definitions and data types ********************/
#define CLOSE_FILES_N_EXIT(fp1, fp2, fp3, rc) {if (fp1 != NULL) fclose(fp1);\
                                          if (fp2 != NULL) fclose(fp2); \
                                          if (fp3 != NULL) fclose(fp3); \
                                          exit(rc);}
#define MIN_VALID_Z_VALUE   0.0      /* Cell with Z < this value is bad */
#define MAX_CMD_LEN         300
#define MAX_LINE_LEN        500
#define MAX_FILENAME_LEN    256

#define DEFAULT_VOS_WINDOW_TIME_RANGE 10   /* In minutes */

int verbose = 0;
char *this_prog = "merge_radarNgauge_data";
static GDBM_FILE gauge_dbf = NULL;
/************************ Function Prototypes ************************/
void clean_up();
int merge_gauge_and_append_to_outfile(GDBM_FILE gauge_dbf, 
									  time_t vos_window_time_interval,
									  int window_center_offset_min,
									  int keep_all_entries, 
									  float min_valid_z_value,
									  char *radar_column_data, 
									  FILE **discarded_vos_fp,
									  FILE **outfile_fp);
extern FILE *popen( const char *command, const char *type);
extern int pclose( FILE *stream);

extern int strcasecmp(const char *s1, const char *s2);
extern int getopt(int argc, char * const argv[],
                  const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;

int extract_info_from_data_line(char *line, char *gauge_id, 
								char *network_name,
								time_t *vos_stime_sec, 
								int *all_radar_data_missing,
								int *all_radar_data_no_rain,
								float min_valid_z_value);


int read_write_header_info(char *infile, char *outfile_name,
						   char *discarded_vos_file,
						   int vos_window_time_interval, 
						   int vos_window_center_offset_min, 
						   float min_valid_z_value,
						   int keep_all_entries, FILE **discarded_vos_fp,
						   FILE **outfile_fp);

static void handler(int sig);
void find_vos_window_time(time_t vos_stime_sec, int vos_window_time_interval,
						  int window_center_offset_min,
						  time_t *vos_window_stime_sec,
						  time_t *vos_window_etime_sec);
/**********************************************************************/

/**********************************************************************/
/*                                                                    */
/*                             usage                                  */
/*                                                                    */
/**********************************************************************/
void usage(char *prog)
{
  if (prog == NULL)
	prog = "";

  fprintf(stderr, "Usage (%s): Build the second ZR intermediate file.\n", PROG_VERSION);
  fprintf(stderr, "   %s [-v] \n"
		          "      [-k] [-n] [-t window_time] [-O window_center_offset]\n"
		          "      [-f gauge_db_file] [-z min_valid_Z_value]\n"
                  "      [-F discarded_vos_file]\n"
                  "      first_zr_intermediate_infile second_zr_intermediate_outfile\n", prog);
  fprintf(stderr, "\n   where,\n");
  fprintf(stderr, "     -v: Show verbose messages of program execution.\n"
		          "     -k: Tell the system to keep the partially completed out file in case of failure.\n"
                  "         Default: Remove the partially completed outfile when error occurs.\n"

                  "     -n  Not discard any VOS entry.  Choosing this option will result in very huge\n"
                  "         output file.  Default: Discard all entries having no valid gauge rain \n"
                  "         rates nor valid (rain classification, reflectivity) pair.\n"
		          "     -t: Specify the window time interval, in minutes. See option, -O,\n"
                  "         to define window centered time. Default: 10\n"
                  "     -O: Specify the number of offset minutes for defining the window centered\n"
                  "         time. It can be +/-. Centered time = [vos_time + offset]. Default: 2.\n"
		          "     -f: Specify gauge database filename.  Default is $GVS_DB_PATH/gauge.gdbm.\n"
                  "     -z: Specify the mininum valid Z value. All smaller Z values are invalid.\n"
                  "         Default: 0.0.\n"  
                  "     -F: Specify discarded vos filename. The program will write the info. of\n"
                  "         the discarded VOSes to this file. Default: \'<ouput filename>.discarded_vos.ascii\'.\n"
		          "\n"
		          "     first_zr_intermediate_infile:    File contains radar data\n"
                  "          and rain types for gauges\n"
		          "     second_zr_intermediate_outfile:  Filename for output.  File will \n"  
                  "         contain radar data, rain types, and gauge data.\n");
  exit(-1);
}


/**********************************************************************/
/*                                                                    */
/*                          process_argvs                             */
/*                                                                    */
/**********************************************************************/
void process_argvs(int argc, char **argv, int *remove_file,
				   int *keep_all_entries, int *vos_window_time_interval,
				   int *vos_window_center_offset,
				   float *min_valid_z_value, char *discarded_vos_file,
				   char *gauge_db_file, char **infile, char **outfile)
{
  extern char *optarg;
  extern int optind, opterr, optopt;

  int c;


  if (argc < 2) 
	usage(argv[0]);

  while ((c = getopt(argc, argv, "f:t:O:z:F:vkn")) != -1) {
	switch (c) {
	case 'v':
	  verbose = 1;
	  break;
	case 'n':
	  *keep_all_entries = 1;
	  break;
	case 'k':
	  *remove_file = 0;
	  break;
	case 'O':
	  *vos_window_center_offset = atoi(optarg);
	  break;
	case 'z':
	  *min_valid_z_value = atof(optarg);
	  break;
	case 'f': strcpy(gauge_db_file, optarg); break;
	case 't':
	  *vos_window_time_interval = atoi(optarg);
	  break;
	case 'F':
	  strcpy(discarded_vos_file, optarg); break;
	case '?': fprintf(stderr, "option -%c is undefined\n", optopt);
	  usage(argv[0]);
    case ':': fprintf(stderr, "option -%c requires an argument\n",optopt);
	  usage(argv[0]);
    default: break;
    }
  }
  /* must have 2 files */
  if (argc - optind != 2) usage(argv[0]);
  *infile = argv[optind++];
  *outfile = argv[optind++];
	

} /* process_argvs */


/**********************************************************************/
/*                                                                    */
/*                          main                                      */
/*                                                                    */
/**********************************************************************/
int main(int argc, char **argv)
{
  
  char *infile, *outfile;
  FILE *infile_fp=NULL, *outfile_fp=NULL, *discarded_vos_fp = NULL;
  int rc = 1;
  int remove_outfile = 1;
  int keep_all_entries = 0;
  int vos_window_time_interval = DEFAULT_VOS_WINDOW_TIME_RANGE;
  int data_flag;
  char line[MAX_LINE_LEN];
  char gauge_db_name[MAX_FILENAME_LEN], discarded_vos_file[MAX_FILENAME_LEN];
  int vos_window_center_offset_min = 2;  /* Default to 2 mins. */
  float min_valid_z_value = MIN_VALID_Z_VALUE;

  set_signal_handlers();
  this_prog = argv[0];
  memset(gauge_db_name, '\0', MAX_FILENAME_LEN);
  memset(discarded_vos_file, '\0', MAX_FILENAME_LEN);
  gauge_construct_default_db_name(gauge_db_name); /* $GVS_DB_PATH/gauge.gdbm */
  process_argvs(argc, argv,  &remove_outfile, &keep_all_entries,
				&vos_window_time_interval, &vos_window_center_offset_min,
				&min_valid_z_value, discarded_vos_file,
				gauge_db_name, &infile, &outfile);

  if (strlen(discarded_vos_file) < 1) {
	/* Set default: '<outfile>.discarded_vos.ascii' */
	sprintf(discarded_vos_file, "%s.discarded_vos.ascii", outfile);
  }
  if (verbose) {
	fprintf(stderr, "infile:    %s\n", infile);
	fprintf(stderr, "outfile:   %s\n", outfile);
	fprintf(stderr, "min_valid_z_value: %f\n", min_valid_z_value);
	fprintf(stderr, "gauge db:  %s\n", gauge_db_name);
	fprintf(stderr, "discarded_vos_file: %s\n", discarded_vos_file);
  }

  /* Open output file for writing. */
  if ((outfile_fp = fopen(outfile, "w")) == NULL) {
	fprintf(stderr, "Error: Failed to open %s\n", outfile);
	CLOSE_FILES_N_EXIT(infile_fp, outfile_fp, discarded_vos_fp, -1);
  }
  /* Open discarded_vos file for writing. */
  if ((discarded_vos_fp = fopen(discarded_vos_file, "w")) == NULL) {
	fprintf(stderr, "Error: Failed to open %s\n", discarded_vos_file);
	CLOSE_FILES_N_EXIT(infile_fp, outfile_fp, discarded_vos_fp, -1);
  }


  if (verbose)
	fprintf(stderr, "Calling read_write_header_info()...\n");
  /* Read header info from infile and write them to outfile_fp;
   */
  if (read_write_header_info(infile, outfile, discarded_vos_file,
							 vos_window_time_interval,
							 vos_window_center_offset_min,
							 min_valid_z_value,
							 keep_all_entries,
							 &discarded_vos_fp,
							 &outfile_fp) < 0) {
	fprintf(stderr, "Error: read_write_header_info() failed.\n");
	CLOSE_FILES_N_EXIT(infile_fp, outfile_fp, discarded_vos_fp, -1);
  }

  /* Open input file for reading. */
  if ((infile_fp = fopen(infile, "r")) == NULL) {
	fprintf(stderr, "Error: Failed to open %s\n", infile);
	CLOSE_FILES_N_EXIT(infile_fp, outfile_fp, discarded_vos_fp,-1);
  }
  /* Open the gauge database */
  gauge_dbf = gauge_db_open(gauge_db_name, 'r');
  if (gauge_dbf == NULL) {
	fprintf(stderr, "Error: Failed to open gauge database:%s\n", gauge_db_name);
	CLOSE_FILES_N_EXIT(infile_fp, outfile_fp, discarded_vos_fp,-1);
  }
  
  /* While not EOF (Note: we don't need to sort the input file nor remove 
   * duplicated entries since the gauge data is a database.
   * File contains header info -- so skip the header info.
   * 1. read a line 
   * 2. skip if it's not a data line.
   * 3. merge line with gauge data and append it to the outfile
   *
   */
  data_flag = 0;
  while (!feof(infile_fp)) {
	memset(line, '\0', MAX_LINE_LEN);
	if (fgets(line, MAX_LINE_LEN, infile_fp) == NULL) break;

	line[strlen(line)-1] = '\0'; /* Remove \n */
	if (strlen(line) < 1) continue;   /* Skip empty line */
	if (strstr(line, TABLE_START_STR) != NULL) {
	  data_flag = 1;
	  continue;
	}
	else if (data_flag == 0)
	  continue;
	else if (line[0] == COMMENT_CHAR)    /* Comment line */
	  continue;

	/* Merge gauge data and append to outfile */
	if (merge_gauge_and_append_to_outfile(gauge_dbf, vos_window_time_interval,
										  vos_window_center_offset_min,
										  keep_all_entries, 
										  min_valid_z_value, line, 
										  &discarded_vos_fp, &outfile_fp)
		< 0){
	  fprintf(stderr, "Error: merge_gauge_and_append_to_outfile failed.\n");
	  rc = -1;
	  break;
	}

  }  /* While not eof */
  
  if (verbose)
	fprintf(stderr, "Closing outfiles and infile...\n");
  fclose(outfile_fp);
  fclose(infile_fp);
  fclose(discarded_vos_fp);

  clean_up();
  if (rc < 0) {
	/* Error */
	if (remove_outfile)
	  unlink(outfile);    /* Remove output file. */

	if (verbose)
	  fprintf(stderr, "Failed.\n");
	exit(-1);
  }
  if (verbose)
	fprintf(stderr, "Successful.\n");
  exit(0);
} /* main */



/**********************************************************************/
/*                                                                    */
/*                       extract_info_from_data_line                  */
/*                                                                    */
/**********************************************************************/
int extract_info_from_data_line(char *line, char *gauge_id, 
								char *network_name,
								time_t *vos_stime_sec, 
								int *all_radar_data_missing,
								int *all_radar_data_no_rain,
								float min_valid_z_value)
								
{
  /* Extract gauge id, start date/time, and end date/time from the data
   * line. This routine doesnot allocate memory, so the caller must
   * allocate memory for the elements.
   * Set all_radar_data_missing = 1 if all tuples contain missing data -- 
   * missing data is determined by checking whether the tuple contains
   * either a missing rain type or Z value.
   * Set all_radar_data_no_rain = 1 if all tuples contain no rain data -- 
   * no rain data is determined by checking whether the tuple contains
   * either a no-rain rain type or Z value.
   *
   * It will return 1 upon successful; -1, otherwise.
   *
   * line starts with the following items:
   *    GaugeID GaugeNetworkName mm/dd/yyyy hh:mm r N H1 C1 Z1 ... CN ZN ...
   *       HN C1 Z1 ... CN ZN
   * 
   *  where:  
   *        -- Fields from the first zr intermediate file --
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
  char *tuple_data_ptr;
  int ntuples = 0, nheights = 0;
  int all_missing = 1, all_no_rain = 1;
  int rain_type, h,i;
  float Z;
  char smon[3], emon[3], sday[3];
  char shour[3], smin[3], ehour[3], emin[3], syr[5];
  DATE_STR vos_sdate;
  TIME_STR vos_stime;

  if (line == NULL || gauge_id == NULL || network_name == NULL || 
	  vos_stime_sec == NULL || all_radar_data_missing == NULL ||
	  all_radar_data_no_rain == NULL)
	return -1;

  /* sscanf doesnot work correctly if we extract these as integer, so 
   * we have to extract them as string and convert them to integer.
   */

  memset(smon, '\0', 3);
  memset(emon, '\0', 3);
  memset(sday, '\0', 3);
  memset(sday, '\0', 3);
  memset(shour, '\0', 3);
  memset(ehour, '\0', 3);
  memset(smin, '\0', 3);
  memset(emin, '\0', 3);
 
  if (sscanf(line, "%s %s %2s/%2s/%s %2s:%2s %*f %d %*s",
			 gauge_id, network_name, 
			 smon, sday, syr, 
			 shour, smin, &nheights) != 8) {
	fprintf(stderr, "Line's format is obsolete <%s>.\n", line);
	return -1;
  }
  vos_sdate.tkyear = atoi(syr);
  vos_stime.tksecond = 0;
  vos_stime.tkhour = atoi(shour);
  vos_stime.tkminute = atoi(smin);
  vos_sdate.tkmonth = atoi(smon);
  vos_sdate.tkday = atoi(sday);
  date_time2system_time(&vos_sdate, &vos_stime, vos_stime_sec);


  tuple_data_ptr = line;
  /* Skip to the first height */
  for (i = 0; i < 7 && tuple_data_ptr != NULL; i++) {
	tuple_data_ptr = strchr(tuple_data_ptr, ' ');
	if (tuple_data_ptr == NULL) {
	  fprintf(stderr, "Obsolete format. Expecting more entries.\n");
	  return -1;
	}
	tuple_data_ptr++;
  }

  /* Check if all tuples contain bad raintype or Z */
  for (h = 0; h < nheights && (all_no_rain == 1 || all_missing == 1) && 
		 tuple_data_ptr != NULL; h++) {
	/* Extract ntuples: heihgt ntuple ... */
	if (sscanf(tuple_data_ptr, "%*f %d %*s", &ntuples) != 1) {
	  fprintf(stderr, "Obsolete format. Expecting ntuples.\n");
	  return -1;
	}
	/* Skip to tuples */
	tuple_data_ptr = strchr(tuple_data_ptr, ' '); /* Skip height */
	tuple_data_ptr++;
	tuple_data_ptr = strchr(tuple_data_ptr, ' '); /* Skip ntuples */
	tuple_data_ptr++;
	for (i = 0; i < ntuples && tuple_data_ptr != NULL; i++) {
	  if (sscanf(tuple_data_ptr, "%d %f %*s", &rain_type, &Z) != 2) {
		fprintf(stderr, "Obsolete format. Expecting rain_type and Z tuple\n");
		return -1;
	  }

	  /* A cell is not rain if NO_ECHO_C && Z <= 0 and Z < min_valid_z_value
	   * A cell is missing if Z is MISSING. Rain type is not important in 
	   * this case.
	   */
	  if (rain_type != NO_ECHO_C &&  Z > 0.0 && Z > min_valid_z_value &&
		  Z != MISSING_Z) {
		/* A good cell is found  */
		all_no_rain = 0;
		all_missing = 0; 
	  }
	  else if (rain_type != NO_ECHO_C &&  Z > 0.0 && Z > min_valid_z_value) 
		all_no_rain = 0;

	  else if (Z != MISSING_Z) 
		all_missing = 0;



	  tuple_data_ptr = strchr(tuple_data_ptr, ' '); /* Skip raintype */
	  tuple_data_ptr++;
	  tuple_data_ptr = strchr(tuple_data_ptr, ' '); /* Skip Z */
	  tuple_data_ptr++;
	}
  }/* for h*/

  *all_radar_data_missing = all_missing;
  *all_radar_data_no_rain = all_no_rain;

  return 1;
}  /* extract_info_from_data_line */


/**********************************************************************/
/*                                                                    */
/*                     read_write_header_info                         */
/*                                                                    */
/**********************************************************************/
int read_write_header_info(char *infile, char *outfile_name,
						   char *discarded_vos_fname,
						   int vos_window_time_interval, 
						   int vos_window_center_offset_min, 
						   float min_valid_z_value,
						   int keep_all_entries, FILE **discarded_vos_fp, 
						   FILE **outfile_fp)
{
  /* Read header info from infile_fp and write them to outfile_fp.
   * Write some of that header info to discarded_vos_fp.
   * Routine will not close files when returns.
   */
  char line[MAX_LINE_LEN];
  int rc = 1;
  FILE *infile_fp;
  int i = 0;
  char date_str[MAX_NAME_LEN], time_str[MAX_NAME_LEN];

  if ( infile == NULL || outfile_name == NULL || *outfile_fp == NULL ||
	   *discarded_vos_fp == NULL || discarded_vos_fname == NULL) return -1;

  if ((infile_fp = fopen(infile, "r")) == NULL) {
	perror("fopen infile");
	return -1;
  }
  memset(date_str, '\0', MAX_NAME_LEN);
  memset(time_str, '\0', MAX_NAME_LEN);
  gv_utils_time_secs2date_time_strs(time(NULL), 1, 0, date_str, time_str);

  /* Write comments to outfile_fp first. */
  fprintf(*outfile_fp, "%s This table is the second intermediate product for creating the ZR table\n", USER_COMMENT_CHARS);
  fprintf(*outfile_fp, "%s It contains radar windows, rain classifications, and rain rates data for\n", USER_COMMENT_CHARS); 
  fprintf(*outfile_fp, "%s rain gauges.  Line starts with # is considered comment.\n", USER_COMMENT_CHARS);
  fprintf(*outfile_fp, "%s\n", USER_COMMENT_CHARS);
  fprintf(*outfile_fp, "%s File generation information:\n"
                       "%s    Created by:     %s (%s).\n"
                       "%s    Input file:     %s\n"
                       "%s    Generated time: %s %s\n", 
		  USER_COMMENT_CHARS, USER_COMMENT_CHARS, this_prog, PROG_VERSION,
		  USER_COMMENT_CHARS, infile, USER_COMMENT_CHARS,date_str, time_str);

 

  fprintf(*outfile_fp, "%s\n", USER_COMMENT_CHARS);


  /* Write comments to discarded_vos_fp.  */
  fprintf(*discarded_vos_fp, "%s This file contains information of all VOSes excluded \n"
                               "%s from the second intermediate file, '%s'.\n"
                               "%s Line starts with # is considered comment.\n"
		                       "%s\n",
		  USER_COMMENT_CHARS, USER_COMMENT_CHARS, outfile_name, USER_COMMENT_CHARS, USER_COMMENT_CHARS);
  fprintf(*discarded_vos_fp, "%s File generation information:\n"
                       "%s    Created by:               %s (%s).\n"
                       "%s    Input file:               %s\n"
                       "%s    Second intermediate file: %s\n"
                       "%s    Generated time:           %s %s\n", 
		  USER_COMMENT_CHARS, USER_COMMENT_CHARS, this_prog, PROG_VERSION,
		  USER_COMMENT_CHARS, infile, 
		  USER_COMMENT_CHARS, outfile_name, 
		  USER_COMMENT_CHARS,date_str, time_str);


  fprintf(*discarded_vos_fp, "%c\n", COMMENT_CHAR);

  /* Read from infile */
  while (!feof(infile_fp)) {

	memset(line, '\0', MAX_LINE_LEN);
	/* Read the next line from infile. */
	if (fgets(line, MAX_LINE_LEN, infile_fp) == NULL) break; 
	if (strncmp(line, USER_COMMENT_CHARS, strlen(USER_COMMENT_CHARS))==0)
		continue;  /* It's comment, skip. */

	if (strstr(line, TABLE_RECORD_INFO_STR) != NULL) {
	  fprintf(*outfile_fp, " Window_time_range_in_minutes      %d\n",vos_window_time_interval);
	  fprintf(*outfile_fp, " Window_center_offset_in_minutes   %d\n", vos_window_center_offset_min);
	  i++;
	}	
	else if (strstr(line, TABLE_START_STR) != NULL) {
	  /* Print to comment section: what is being kept in the product.
	   */
	  fprintf(*outfile_fp, "%c\n", COMMENT_CHAR);
	  fprintf(*outfile_fp, "%s Note: \n", FILE_COMMENT_CHARS);

	  if (keep_all_entries) {
		fprintf(*outfile_fp, "%s    * This file keeps all entries from the input file. \n",
				FILE_COMMENT_CHARS);
	  }
	  else {

		fprintf(*outfile_fp, "%s    * VOSes are kept based on the following criteria:\n",  FILE_COMMENT_CHARS);

		fprintf(*outfile_fp, "%s       (1) Both radar and gauge data are not missing or\n"
				             "%s       (2) Either radar or gauge sees rain. \n"
				             "%s    * Gauge data is missing if:\n"
                             "%s       (1) There is no rain rate for the interested month and year, or\n"
                             "%s       (2) There is no info for the interested gauge ID/netID, or\n"
                             "%s       (3) R <= %.2f\n"
				             "%s    * Gauge data shows no rain if R = 0.0\n"
                             "%s    * Radar data is missing if Z = %.2f.\n"
                             "%s    * Radar data shows no rain if C = %d (NO_ECHO) && Z <= 0.0 && Z <= %.2f (min_valid_Z_value)\n",
				FILE_COMMENT_CHARS,FILE_COMMENT_CHARS,FILE_COMMENT_CHARS,FILE_COMMENT_CHARS,FILE_COMMENT_CHARS,FILE_COMMENT_CHARS, MISSING_RAIN_RATE, FILE_COMMENT_CHARS,FILE_COMMENT_CHARS,MISSING_Z, FILE_COMMENT_CHARS, NO_ECHO_C, min_valid_z_value);

		fprintf(*outfile_fp, "%s    * See file, '%s', for information on discarded VOSes.\n", FILE_COMMENT_CHARS, discarded_vos_fname);
	  }
	  fprintf(*outfile_fp, "%c\n", COMMENT_CHAR);

	  i = 0;
	  fprintf(*outfile_fp, "%s", line); /* Write header line to outfile. */

	  fprintf(*discarded_vos_fp, "%c\n", COMMENT_CHAR); 
	  fprintf(*discarded_vos_fp, "%c TABLE RECORD INFORMATION:\n"
			  "%c Record Format:\n"
			  "%c    Gauge_ID Net_ID VOS_Date VOS_Time\n",
			  COMMENT_CHAR, COMMENT_CHAR, COMMENT_CHAR);

	  fprintf(*discarded_vos_fp, "%c\n", COMMENT_CHAR); 
	  fprintf(*discarded_vos_fp, "%s", line); /* Write header line to discarded_vos_fp. */
	  break;                         /* Done, end of header info. is reached. */
	}

	else if (i > 0) {
	  /* Assumming this line is part of the table record's format. */
	  if (line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';  /* Remove \n */


	  if (strstr(line, COMMENT_RECORD_FORMAT_LINE) != NULL)
		fprintf(*outfile_fp, "%c%s\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE));

	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE1) != NULL)
		fprintf(*outfile_fp, "%c%s      _NR R's_\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE1));
	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE2) != NULL)
		fprintf(*outfile_fp, "%c%s      |      |\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE2));
	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE3) != NULL)
		fprintf(*outfile_fp, "%c%s NR R1 ...\n", COMMENT_CHAR,
				line+strlen(COMMENT_RECORD_FORMAT_LINE3));
	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE4) != NULL)
		fprintf(*outfile_fp, "%c%s           I  F  \n", COMMENT_CHAR,
				line+strlen(COMMENT_RECORD_FORMAT_LINE4));

	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE5) != NULL) {
		fprintf(*outfile_fp, "%c%s\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE5));
		fprintf(*outfile_fp, "%c    NR   = Number of R's    R    = Rain Rate, < 0.0 suspicion, %.2f missing\n", COMMENT_CHAR,  MISSING_RAIN_RATE);
	  }
	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE6) != NULL)
		fprintf(*outfile_fp, "%c%s\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE6));
	  else if (strstr(line, COMMENT_RECORD_FORMAT_LINE7) != NULL) {
		fprintf(*outfile_fp, "%c%s\n", COMMENT_CHAR, 
				line+strlen(COMMENT_RECORD_FORMAT_LINE7));
		/* Add the time window here */
        fprintf(*outfile_fp, "%c\n"
                             "%c   Time Window of N Minutes Centered at VOS_time + Offset:\n"
				             "%c       |--t1--|--t2--|...|--tN--|\n"
                             "%c       |  R1  |  R2  |...|  RN  |\n"
                             "%c\n",

				COMMENT_CHAR, COMMENT_CHAR, COMMENT_CHAR, COMMENT_CHAR, COMMENT_CHAR);

	  }

	  else {
		
		fprintf(*outfile_fp, "%s\n", line); /* Write comment info to outfile. */
	  }
	  i++;

	  continue;
	}
	  
	fprintf(*outfile_fp, "%s", line); /* Write header info to outfile. */

	if (strstr(line, SITE_NAME_STR) != NULL) {
	  /* Write this line to discarded_time_fp. */
	  fprintf(*discarded_vos_fp, "%c %s", COMMENT_CHAR, line);
	}
	else if (strstr(line, END_DATE_TIME_STR) != NULL ||
			 strstr(line, START_DATE_TIME_STR) != NULL) {
	  /* Write this line to discarded_time_fp. */
	  fprintf(*discarded_vos_fp, "%c %s", COMMENT_CHAR, line);
	}

  }

  fclose(infile_fp);
  return rc;
} /* read_write_header_info */

/**********************************************************************/
/*                                                                    */
/*                           clean_up                                 */
/*                                                                    */
/**********************************************************************/
void clean_up()
{
  gauge_db_close(gauge_dbf, 'r'); /* Close the gauge database */
}


/**********************************************************************/
/*                                                                    */
/*                           handler                                  */
/*                                                                    */
/**********************************************************************/
void handler(int sig)
{
  if (verbose)
	fprintf(stderr, "Got signal %d. Abort.\n", sig);
  clean_up();
  kill(0, sig);
  if (sig == SIGINT || sig == SIGKILL || sig == SIGSTOP) 
	exit (-2);
  exit(-1);
}


/**********************************************************************/
/*                                                                    */
/*                    merge_gauge_and_append_to_outfile               */
/*                                                                    */
/**********************************************************************/
int merge_gauge_and_append_to_outfile(GDBM_FILE gauge_dbf, 
									  time_t vos_window_time_interval,
									  int window_center_offset_min,
									  int keep_all_entries, 
									  float min_valid_z_value,
									  char *radar_column_data, 
									  FILE **discarded_vos_fp,
									  FILE **outfile_fp)
{
  /* Merge radar, rain class, and rain rates data and append the results
   * to outfile_fp.  The rain gauge data is from the database.
   * if keep_all_entries != 1, do not write vos having no rain rates and
   * no radar data. 
   * Write to discarded_vos_fp any vos not included in outfile_fp.
   * Return 1 for successful; -1, otherwise.
   *
   * Output entry to file based on the following criteria:
   *   * Both radar and gauge data are not missing.
   *   * Or either radar or gauge sees rain. 
   *   * Or keep_all_entries is specified.
   *
   */
  char rain_rates_str[MAX_LINE_LEN];
  int nrain_rates = 0, n_non_missingNnon_zero_rain_rates = 0, 
	n_zero_rain_rates = 0;
  time_t vos_window_stime_sec, vos_window_etime_sec;
  char gauge_id[MAX_NAME_LEN], net_id[MAX_NAME_LEN];
  time_t vos_time_sec;
  int all_radar_data_missing = 0;
  int all_radar_data_no_rain = 0;
  char date_str[MAX_NAME_LEN], time_str[MAX_NAME_LEN];
  int all_gauge_data_missing = 0, all_gauge_data_no_rain = 0;

  if (radar_column_data == NULL || *outfile_fp == NULL || gauge_dbf == NULL ||
	  *discarded_vos_fp == NULL)
	return -1;

  extract_info_from_data_line(radar_column_data, gauge_id, net_id, 
							  &vos_time_sec, &all_radar_data_missing, 
							  &all_radar_data_no_rain,
							  min_valid_z_value);

  find_vos_window_time(vos_time_sec, vos_window_time_interval,
					   window_center_offset_min,
					   &vos_window_stime_sec, &vos_window_etime_sec);


  if (verbose) {
	fprintf(stderr, "net <%s> gauge <%s>: vos window start time %s\n",
			net_id, gauge_id, ctime(&vos_window_stime_sec));
	fprintf(stderr, "net <%s> gauge <%s>: vos window start time %s\n",
			net_id, gauge_id, ctime(&vos_window_etime_sec));
  }
  /* rain_rates_str will contain rain rate or each minute for the 
   * specified time period.
   */
  memset(rain_rates_str, '\0', MAX_LINE_LEN);
  gauge_db_fetch_range(gauge_dbf, net_id, gauge_id, vos_window_stime_sec,
					   vos_window_etime_sec, NULL, NULL, rain_rates_str, 
					   &n_non_missingNnon_zero_rain_rates, &n_zero_rain_rates,
					   &nrain_rates);

  /* Output entry to file based on the criteria defined in Brad Fisher's 
   * message:
   *   For radar gauge QC it is important to know whether either
   *   instrument records rainfall.  It is an OR condition.  So if the Rain 
   *   Gauge sees OR the Radar we should write that data to the second 
   *   intermediate file. 
   *
   *   It is probably less critical to write out records 
   *   if the radar OR gauge data is missing OR both instruments see no rain.
   *
   *   In the interest of smaller files we will not write the data out if 
   *   one or the other is missing, but if for instance the radar sees no 
   *   rain, but the gauge does, write it out. 
   */

  all_gauge_data_missing = (n_non_missingNnon_zero_rain_rates == 0 &&
							n_zero_rain_rates == 0);
  all_gauge_data_no_rain = (n_zero_rain_rates == nrain_rates);


  if (!keep_all_entries &&
	  /* Either radar or gauge data is missing */
	  ((all_gauge_data_missing == 1 || all_radar_data_missing == 1) ||
	  /* Or both radar and gauge see no rain. */
	   (all_gauge_data_no_rain == 1 && all_radar_data_no_rain))) {
	/* Don't keep this VOS */

	memset(date_str, '\0', MAX_NAME_LEN);
	memset(time_str, '\0', MAX_NAME_LEN);
	gv_utils_time_secs2date_time_strs(vos_time_sec, 1, 0, date_str, time_str);
	fprintf(*discarded_vos_fp, "%s %s %s %s\n", gauge_id, net_id, date_str, time_str);
	if (verbose) {
	  fprintf(stderr, "Ignored: radar data: %s\n", radar_column_data);
	  fprintf(stderr, "Ignored: rain rate count: %d\n", nrain_rates);
	  fprintf(stderr, "Ignored: rain rates: %s\n", rain_rates_str);
	}
  }
  else {
	/* Keep this VOS */
	/*   * Both radar and gauge data are not missing.
	 *   * Or either radar or gauge sees rain. 
	 *   * Or keep_all_entries is specified.
	 */
	if (verbose) {
	  fprintf(stderr, "Kept: radar data: %s\n", radar_column_data);
	  fprintf(stderr, "Kept: rain rate count: %d\n", nrain_rates);
	  fprintf(stderr, "Kept:rain rates: %s\n", rain_rates_str);
	}

	fprintf(*outfile_fp, "%s %d %s\n", radar_column_data, nrain_rates, rain_rates_str);
  }

  return 1;
} /*merge_gauge_and_append_to_outfile */



/**********************************************************************/
/*                                                                    */
/*                      find_vos_window_time                          */
/*                                                                    */
/**********************************************************************/
void find_vos_window_time(time_t vos_stime_sec, int vos_window_time_interval,
						  int window_center_offset_min,
						  time_t *vos_window_stime_sec,
						  time_t *vos_window_etime_sec)
{
	/* Find the vos window start/end times based on the vos start time and
	 * thw window time interval as followed:
	 * vos_window_stime = vos_center_time - 
	 *                              (vos_window_time_interval/2 * 60) 
	 * vos_window_etime = vos_center_time + 
	 *                              (vos_window_time_interval/2 * 60) 
	 *  where vos_center_time = vos_stime_sec + window_center_offset_min*60 
	 */
  time_t vos_center_time_sec;

  vos_center_time_sec = vos_stime_sec + window_center_offset_min*60;

  *vos_window_stime_sec = vos_center_time_sec - 
	((int) (vos_window_time_interval/2) * 60);

  *vos_window_etime_sec = vos_center_time_sec + 
	((int) (vos_window_time_interval/2) * 60);

} /* find_vos_window_time */
