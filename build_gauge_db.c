/*
 *
 * build_gauge_db.c:
 *      Build/update gauge database "$GVS_DB_PATH/gauge_db.gdbm". 
 *      All gauge files (2A-56 products) are store in one database.  
 *      This program appends new data to the existing database file and
 *      replaces the existing data with the new value.
 *  
 * 
 *     Exit code:
 *       0: successful
 *       1: some file(s) or dir(s) didn't get loaded correctly
 *       -1: failure -- no file or dir gets loaded into the database.
 *
 *     Requires:
 *       * gdbm
 *       * gv_utils
 *       * Set environment variable: GVS_DB_PATH to the location where the
 *           gauge database is to be stored under.
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
 *     Copyright (C) 1997-1998
 *
 ***************************************************************************/ 


#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>
#include <gdbm.h>
#include <gv_utils.h>

#include "gauge_db.h"

#define MAX_INPUTS       300     /* Number of input on the command line. */
#define MAX_FILENAME_LEN 256
#define MAX_STR_LEN      50
#define MAX_LINE_LEN     300
#define MAX_INFILE_SIZE  200000  

#define IS_WITHIN_TIME_RANGE(fstat_info, begin_time, end_time) ( \
  (fstat_info.st_mtime >= begin_time && end_time == 0) || \
	  (fstat_info.st_mtime >= begin_time && fstat_info.st_mtime <= end_time) )


static GDBM_FILE dbf = NULL;
int verbose = 0;

/********************* Function prototype ****************************/
static void handler(int sig);
void usage(char *prog);
void clean_up();
void process_argvs(int argc, char **argv, time_t *begin_timee, time_t *end_time, char *gauge_db_file, char **gauge_input_list);
int load_file_to_db(GDBM_FILE dbf, char *fname, int file_size);

/************************** Program ***********************************/
/**********************************************************************/
/*                                                                    */
/*                                 main                               */
/*                                                                    */
/**********************************************************************/
int main (int argc, char **argv)
{
  char gauge_db_name[MAX_FILENAME_LEN];
  char *gauge_input_list[MAX_INPUTS];
  char *input_dir_or_fname = NULL;
  int error_toplevel = 0, error_sublevel = 0;
  char fname[MAX_FILENAME_LEN];
  int i;
  struct dirent *dirent;
  char *entry;
  DIR *dir_ptr;
  time_t begin_time, end_time;
  struct stat fstat_info;

  set_signal_handlers();

  /* Initialize  */
  for(i=0;i<MAX_INPUTS;i++)
	gauge_input_list[i] = NULL;


  memset(gauge_db_name, '\0', MAX_FILENAME_LEN);
  gauge_construct_default_db_name(gauge_db_name); /* $GVS_DB_PATH/gauge.gdbm */
  memset(&begin_time, '\0', sizeof(time_t));
  memset(&end_time, '\0', sizeof(time_t));
  process_argvs(argc, argv, &begin_time, &end_time, 
				gauge_db_name, gauge_input_list);

  
  if (verbose) {
	if (begin_time > 0)
	  fprintf(stderr, "File modification time range: begin time = %s\n", ctime(&begin_time));
	if (end_time > 0)
	  fprintf(stderr, "File modification time range: end time = %s\n", ctime(&end_time));
	fprintf(stderr, "Creating gauge DB %s\n", gauge_db_name);
  }
  dbf = gauge_db_open(gauge_db_name, 'w');
  if (dbf == NULL) {
	fprintf(stderr, "Failed to open the database: %s\n", gauge_db_name);
	exit(-1);
  }

  input_dir_or_fname = NULL;
  /* For each user input file or dir, load it into the database. */
  for (i=0; i<MAX_INPUTS; i++) {
	input_dir_or_fname = gauge_input_list[i];

	if (input_dir_or_fname == NULL) break;
	if (verbose)
	  fprintf(stderr, "Preparing to load data from %s\n", input_dir_or_fname);

	memset(&fstat_info, '\0', sizeof(struct stat));
	stat(input_dir_or_fname, &fstat_info);

	if (S_ISREG(fstat_info.st_mode) &&
		IS_WITHIN_TIME_RANGE(fstat_info, begin_time, end_time)) {
	  /* This is a gauge file; load file to the DB. */
	  if (verbose)
		fprintf(stderr, "Loading data from %s\n", input_dir_or_fname);
	  if (load_file_to_db(dbf, input_dir_or_fname, fstat_info.st_size) < 0) {
		fprintf(stderr, "Warning:  Failed to load %s to the database. Ignore.\n", input_dir_or_fname);
		error_toplevel++;
		continue;
	  }
	  continue;
	}
	else if (S_ISDIR(fstat_info.st_mode)) {
	  /* THis is a dir of gauge files. 
	   * Open dir, check each entry and load to the database if it's a file,
	   * then close dir.
	   */
	  if (verbose)
		fprintf(stderr, "Opening dir %s\n", input_dir_or_fname);

	  dir_ptr = opendir(input_dir_or_fname); /* Open dir */
	  if (dir_ptr == NULL) {
		fprintf(stderr, "Warning:  Failed to access %s. Ignore.\n", input_dir_or_fname);
		error_toplevel++;
		continue;
	  }
	  /* Read the first entry from the directory */
	  dirent = readdir(dir_ptr);
	  error_sublevel = 0;
	  /* Read each entry from the directory */
	  while	(dirent != NULL) {
		entry = dirent->d_name;
		if (strlen(entry) <= 0)
		  goto NEXT_FILE;
		memset(fname, '\0', MAX_FILENAME_LEN);
		sprintf(fname, "%s/%s", input_dir_or_fname, entry);

		stat(fname, &fstat_info);

		if (S_ISREG(fstat_info.st_mode) && 
			IS_WITHIN_TIME_RANGE(fstat_info, begin_time, end_time)) {
		  if (verbose)
			fprintf(stderr, "Loading data from %s\n", fname);

		  /* Load file to the db. */
		  if (load_file_to_db(dbf, fname, fstat_info.st_size) < 0) {
			fprintf(stderr, "Warning:  Failed to load %s to the database. Ignore.\n", fname);
			error_sublevel++;
			goto NEXT_FILE;
		  }
		}
	  NEXT_FILE:
		dirent = readdir(dir_ptr);
	  } /* While */		
	  if (error_sublevel > 0) 
		error_toplevel++;
	  closedir(dir_ptr); /* Close dir */
	} /* else is a dir */
	else if (!S_ISREG(fstat_info.st_mode)) {
	  /* File does not exist. */
	  fprintf(stderr, "Warning: File <%s> doesnot exist.\n", input_dir_or_fname);
	  error_toplevel++;
	}
  } /* for each input file or dir */
  
  if (verbose)
	fprintf(stderr, "Cleaning up...\n");
  clean_up(); /* Clean up, close the db */

  if (error_toplevel > 0 && i == error_toplevel) {
	fprintf(stderr, "Failed loading dirs or files to the database.\n");
	exit (-1);
  }
  else if (error_toplevel > 0) {
	if (verbose)
	  fprintf(stderr, "Failed loading some file(s) or dir(s) to the database.\n");
	exit(1);
  }
  if (verbose)
	fprintf(stderr, "Build gauge db was successful.\n");
  exit(0);
  
} /*main */

/**********************************************************************/
/*                                                                    */
/*                               process_argvs                        */
/*                                                                    */
/**********************************************************************/
void process_argvs(int argc, char **argv,
				   time_t *begin_time, time_t *end_time,
				   char *gauge_gdbm_file,
				   char **gauge_input_list)
{
  /* Process argvs. gauge_input_list points to argv. */
  extern int getopt(int argc, char * const argv[],
					const char *optstring);
  extern int optind, optopt;
  extern char *optarg;

  int c, i, j;
  int mon1, day1, yr1, mon2, day2, yr2;

  if (argc < 2) 
	usage(argv[0]);


  while ((c = getopt(argc, argv, "f:d:v")) != -1) {
	switch (c) {
	case 'v':
	  verbose = 1;
	  break;
	case 'd':
	  if (sscanf(optarg, "%d/%d/%d-%d/%d/%d", &mon1, &day1, &yr1, &mon2, &day2, &yr2) == 6) {
		*begin_time = construct_time(yr1, mon1, day1, 0, 0, 0);
		*end_time = construct_time(yr2, mon2, day2, 23, 59, 59);
	  }
	  else if (sscanf(optarg, "-%d/%d/%d", &mon2, &day2, &yr2) == 3) {
		*end_time = construct_time(yr2, mon2, day2, 23, 59, 59);
	  }
	  else if (sscanf(optarg, "%d/%d/%d-", &mon1, &day1, &yr1) == 3) {
		*begin_time = construct_time(yr1, mon1, day1, 0, 0, 0);
	  }
	  break;
	case 'f': strcpy(gauge_gdbm_file, optarg); break;
	case '?': fprintf(stderr, "option -%c is undefined.\n", optopt);
	  usage(argv[0]);
    case ':': fprintf(stderr, "option -%c requires an argument.\n",optopt);
	  usage(argv[0]);
    default: break;
    }
  }
  /* must have at least 1 file */
  if (argc - optind < 1) usage(argv[0]);
  for (i = optind, j = 0; i < argc && i < MAX_INPUTS; i++, j++) {
	gauge_input_list[j] = argv[i];
  }
  
  if (j >= MAX_INPUTS && i < argc) {
	fprintf(stderr, "Too many input files or dirs. Limit is %d\n", MAX_INPUTS);
	exit(-1);
  }
  
} /* process_argvs */

/**********************************************************************/
/*                                                                    */
/*                                usage                               */
/*                                                                    */
/**********************************************************************/
void usage(char *prog)
{
  if (prog == NULL)
	prog = "";
  fprintf(stderr, "Usage (%s): Create/Update Gauge Database.\n", PROG_VERSION);
  fprintf(stderr, "  %s [-v] [-f output_gauge_database] \n"
                  "          [-d infile_modification_date_range] input_list \n", prog);
  fprintf(stderr, "  where,\n");
  fprintf(stderr, "      -v         - Show verbose messages of program execution.\n");
  fprintf(stderr, "      -f         - Specify the filename for the output database.  \n"
                  "                   Default is $GVS_DB_PATH/gauge.gdbm.\n"
                  "                   Default of $GVS_DB_PATH is /usr/local/trmm/GVBOX/data/db\n");
  fprintf(stderr, "      -d         - Specify the file modification date range. \n"
                  "                   Only add gauge files that has the file modification date\n"
                  "                   within the specified date range. \n"
                  "                   Syntax: mm/dd/yy[yy]- | -mm/dd/yy[yy] | \n"
                  "                           mm/dd/yy[yy]-mm/dd/yy[yy].\n"
                  "                   Note: The file modification date is the date stamp when \n"
                  "                   the file was last modified. Type 'ls -l 2A56*' to see\n"
                  "                   the file modification dates. Default: Add all gauge files.\n");
  fprintf(stderr, "      input_list - Specify a list of gauge input directori(es) \n"
                  "                   and/or gauge file(s). List is separated by space.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "      NOTE: \n"
                  "        1. Gauge files are 2A-56 products.\n"
		          "        2. This program appends new data to the existing database file but\n"
                  "           update the existing entri(es) as appropriate.\n");

  
  exit(-1);
}


/**********************************************************************/
/*                                                                    */
/*                               load_file_to_db                      */
/*                                                                    */
/**********************************************************************/
int load_file_to_db(GDBM_FILE dbf, char *fname, int file_size)
{
  /* Load gauge file to the database.
   * Return 1 for successful; -1, otherwise
   * 
   * Note: fname's format is is not relevant.
   */
  FILE *fp = NULL;
  char *line = NULL;
  static char save_buf[MAX_INFILE_SIZE];
  static char *save_buf2 = NULL;
  char *buf = NULL;
  char netID[MAX_STR_LEN], gaugeID[MAX_STR_LEN];
  int error = 0;
  int rc = 1;
  int yr = 0;
  int jday, hr, min, sec;
  char rate_str[MAX_NAME_LEN];
  time_t rr_time = 0, latest_rr_time = 0;
  int nbytes = 0;
  gauge_file_type_t gauge_file_type = UNKNOWN_FILE;

  if (fname == NULL) return -1;

  memset(netID, '0', MAX_STR_LEN);
  memset(gaugeID, '0', MAX_STR_LEN);
  
  gauge_db_get_info_from_ascii_gauge_file(fname, &gauge_file_type, netID, gaugeID, NULL);

  if (gauge_file_type == UNKNOWN_FILE ||
	  strlen(netID) == 0 || strlen(gaugeID) == 0) {
	fprintf(stderr, "Warning: File <%s> is not a recognized gauge file. Ignore.\n", fname);
	return 1;
  }
  
  if (verbose)
	fprintf(stderr, "Got netID: %s, gaugeID: %s, from filename: %s\n", netID, gaugeID, fname);

  /* Load the whole file to memory -- this eliminates reading each line 
   * to improve performance.
   */
  nbytes = file_size;
  if (nbytes <= 0) 
	return 1;

  if (nbytes >= MAX_INFILE_SIZE) {
	/* File is big, can't use buf. Use the previous allocated bigger buf
	 * if exist and sufficient; else remove that allocated buf and get 
	 * a bigger buf. 
	 */
	if (save_buf2 == NULL || nbytes >= sizeof(save_buf2)) {
	  /* File is too big--bigger than the current buffer. 
	   * Allocate a bigger buffer. 
	   */

	  if (save_buf2 != NULL) free(save_buf2); /* free buf--it's small. */
	  if (verbose)
		fprintf(stderr, "Allocating %d bytes...\n", nbytes+1);
	  save_buf2 = (char *) calloc(nbytes+1, sizeof(char));
	  if (save_buf2 == NULL) {
		perror("allocate save_buf2");
		return -1;
	  }

	}
	else
	  memset(save_buf2, '\0', sizeof(save_buf2));
	buf = save_buf2;
  }
  else {
	memset(save_buf, '\0', sizeof(save_buf));
	buf = save_buf;
  }
	

  if ((fp = fopen(fname, "r")) == NULL) return -1;
  if (fread(buf, 1, nbytes, fp) != nbytes) {
	fprintf(stderr, "Error reading from file %s\n", fname);
	rc = -1;
	goto DONE;

  }
  fclose(fp);
  fp = NULL;
  /* Skip the header line */
  line = strtok(buf, "\n");
  if (line == NULL) {
	rc =1;   
	goto DONE;
  }

  while ((line = strtok(NULL, "\n")) != NULL) {
	if (strlen(line) < 1) continue;
	memset(rate_str, '\0', MAX_NAME_LEN);

	switch (gauge_file_type) {
	case P2A56_FILE:
	  /* Format of line: yr month day jday hr min sec rate */
	  if (sscanf(line,  "%d %*d %*d %d %d %d %d %s",
				 &yr, &jday, &hr, &min, &sec, rate_str) != 6) {
		fprintf(stderr, "Warning: Gauge record's format is obsolete. Ignore record.\n");
		fprintf(stderr, "Gauge Record for netID: %s gaugeID: %s = %s\n",
				netID, gaugeID, line);

		error++;
		if (error > 25) { /* too many errors */
		  rc = -1;
		  break;
		}

	  }
	  break;
	default:
	  break;
	}
	if (hr < 0 || hr > 23) {
	  error++;
	  if (error > 25) { /* too many errors */
		rc = -1;
		break;
	  }
	}
	construct_time_from_jday(yr, jday, hr, min, sec, &rr_time);
#define DEBUG
#ifdef DEBUG
	fprintf(stderr, "Calling gauge_db_add() for line <%s>\n", line);
	fprintf(stderr, "        rr_time = %d\n", rr_time);
	fprintf(stderr, "         c_time = %s\n", ctime(&rr_time));
#endif
	/*
	if (verbose)
	  fprintf(stderr, "Calling gauge_db_add() for line <%s>...\n", line);
	  */
	if (gauge_db_add(dbf, netID, gaugeID, rate_str, rr_time) < 0) {
	  if (verbose) {
		fprintf(stderr, "Warning: Failed to add <%s> in file: %s to the DB.\n", line, fname);
	  }
	  error++;
	  if (error > 25) { /* too many errors */
		rc = -1;
		break;
	  }
	}
	if (rr_time > latest_rr_time)
	  latest_rr_time = rr_time;
  }

  /* Update end time */
  if (verbose)
	fprintf(stderr, "Calling gauge_db_update_collection_end_time()...\n");
  if (gauge_db_update_collection_end_time(dbf, netID, gaugeID, latest_rr_time) < 0) {
	fprintf(stderr, "Warning: Failed to set the collection end time for netID <%s> gaugeID <%s>\n", netID, gaugeID);
	rc = -1;
  }

DONE:

  if (verbose)
	fprintf(stderr, "Synchonize the db to disk after loading <%s>...\n", fname);
  gauge_db_write_to_disk(dbf); /* Sync to disk every 400 entries. */

  if (fp)
	fclose(fp);

  return rc;

} /* load_file_to_db */

/**********************************************************************/
/*                                                                    */
/*                               clean_up                             */
/*                                                                    */
/**********************************************************************/
void clean_up()
{
  gauge_db_close(dbf, 'w');
  dbf = NULL;

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
  if (sig == SIGINT || sig == SIGKILL || sig == SIGSTOP) 
	exit (-2);
  exit(-1);
}
