/*
 *
 * validate_gauge_db.c: Check if the contents of the ascii gauge data 
 *     files (2A56) exist correctly in the gauge database.
 *     Output the total number of unmatched rain rates.
 *
 *     Exit code:
 *       0: successful
 *       1: some file(s) or dir(s) didn't get loaded correctly
 *       -1: failure -- no file or dir gets loaded into the database.
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
 *     May 7, 1998
 * 
 *     Copyright (C) 1998
 *
 ***************************************************************************/ 

#include <stdlib.h>
#include <stdio.h>

#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>
#include <gv_utils.h>
#include <signal.h>
#include <unistd.h>
#include <gdbm.h>
#include "gauge_db.h"

#define MAX_INPUTS 300
#define MAX_INFILE_SIZE 200000
#define MAX_FILENAME_LEN 256
#define MAX_STR_LEN      50
#define MAX_LINE_LEN     300
int verbose = 0;
static GDBM_FILE gauge_dbf = NULL;

int validate_file_against_db(GDBM_FILE dbf, char *fname, int file_size,
							 int *unmatched_count);
void clean_up();
static void handler(int sig);

void usage(char *prog)
{

  if (prog == NULL)
	prog = "";

  fprintf(stderr, "Usage (%s): Validates Gauge DB.\n", PROG_VERSION);
  fprintf(stderr, "   %s [-v] [-f gauge_db_file]\n"
		          "       input_list\n"
                  "     where,\n", prog);
  fprintf(stderr, "     -v: Show execution messages.\n"
		          "     -f: Specify the gauge database. Default:$GVS_DB_PATH/gauge.gdbm\n"
                  "     input_list: Specify a list of gauge input directori(es) \n"
                  "                   and/or gauge file(s). List is separated by space.\n");
  exit(-1);
} /* usage */


/**********************************************************************/
/*                                                                    */
/*                          process_argvs                             */
/*                                                                    */
/**********************************************************************/
void process_argvs(int argc, char **argv, 
				   char *gauge_db_file, char **gauge_input_list)

{
  extern char *optarg;
  extern int optind, optopt;
  extern int getopt(int argc, char * const argv[],
					const char *optstring);
  int i, j;
  int c;

  if (argc < 2) 
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
/*                           main                                     */
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
  struct stat fstat_info;
  int unmatched_count = 0, total_unmatched_count = 0;

  set_signal_handlers();

  /* Initialize  */
  for(i=0;i<MAX_INPUTS;i++)
	gauge_input_list[i] = NULL;

  memset(gauge_db_name, '\0', MAX_FILENAME_LEN);
  gauge_construct_default_db_name(gauge_db_name); /* $GVS_DB_PATH/gauge.gdbm */

  process_argvs(argc, argv, gauge_db_name, gauge_input_list);

  gauge_dbf = gauge_db_open(gauge_db_name, 'r');
  if (gauge_dbf == NULL) {
	fprintf(stderr, "Failed to open %s\n", gauge_db_name);
	exit(-1);
  }

  input_dir_or_fname = NULL;
  /* For each user input file or dir, load it into the database. */

  for (i=0; i<MAX_INPUTS; i++) {
	input_dir_or_fname = gauge_input_list[i];

	if (input_dir_or_fname == NULL) break;
	if (verbose)
	  fprintf(stderr, "Preparing to validate data from %s\n", input_dir_or_fname);

	memset(&fstat_info, '\0', sizeof(struct stat));
	stat(input_dir_or_fname, &fstat_info);

	if (S_ISREG(fstat_info.st_mode)) {
	  /* This is a gauge file; Validate it againt the db. */
	  if (verbose)
		fprintf(stderr, "Validating data from %s\n", input_dir_or_fname);
	  unmatched_count = 0;
	  if (validate_file_against_db(gauge_dbf, input_dir_or_fname, fstat_info.st_size,
								    &unmatched_count) < 0) {
		fprintf(stderr, "Warning:  Failed to validate %s. Ignore.\n", input_dir_or_fname);
		error_toplevel++;
		continue;
	  }
	  total_unmatched_count += unmatched_count;
	  if (verbose) 
		fprintf(stderr, "unmatched: %d; total_unmatched: %d; file: %s\n", 
				unmatched_count, total_unmatched_count, input_dir_or_fname);

	  continue;
	}
	else if (S_ISDIR(fstat_info.st_mode)) {
	  /* THis is a dir of gauge files. 
	   * Open dir, check each entry and validate it against the database if
	   * it's a file, then close dir.
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

		if (S_ISREG(fstat_info.st_mode)) {

		  if (verbose)
			fprintf(stderr, "Validating data from %s\n", fname);
		  unmatched_count = 0;
		  /* Validating file against the db. */
		  if (validate_file_against_db(gauge_dbf, fname, fstat_info.st_size, 
									   &unmatched_count) < 0) {
			fprintf(stderr, "Warning:  Failed to validate %s. Ignore.\n", 
					fname);
			error_sublevel++;
			goto NEXT_FILE;
		  }

		  total_unmatched_count += unmatched_count;
		  if (verbose) 
			fprintf(stderr, "unmatched: %d; total_unmatched: %d; file: %s\n",
					unmatched_count, total_unmatched_count, fname);
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


  fprintf(stdout, "Unmatched count: %d\n", total_unmatched_count);
  clean_up();
  exit(0);

} /* main */

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
/**********************************************************************/
/*                                                                    */
/*                               clean_up                             */
/*                                                                    */
/**********************************************************************/
void clean_up()
{
  gauge_db_close(gauge_dbf, 'r');
  gauge_dbf = NULL;

}

/**********************************************************************/
/*                                                                    */
/*                             validate_file_against_db               */
/*                                                                    */
/**********************************************************************/
int validate_file_against_db(GDBM_FILE dbf, char *fname, int file_size,
							 int *unmatched_count)
{
  /* Check the contents in gauge file against the contents in the database.
   * Set the number of unmatched entriest to unmatched_count.
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
  char ascii_file_rr_str[MAX_STR_LEN];
  char db_rr_str[MAX_STR_LEN];
  time_t rr_time = 0;
  int nbytes = 0;
  gauge_file_type_t gauge_file_type = UNKNOWN_FILE;

  if (fname == NULL || unmatched_count == NULL) return -1;

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
	memset(ascii_file_rr_str, '\0', MAX_STR_LEN);

	switch (gauge_file_type) {
	case P2A56_FILE:
	  /* Format of line: yr month day jday hr min sec rate */
	  if (sscanf(line,  "%d %*d %*d %d %d %d %d %s",
				 &yr, &jday, &hr, &min, &sec, ascii_file_rr_str) != 6) {
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
	memset(db_rr_str, '\0', MAX_STR_LEN);
	if (gauge_db_fetch(gauge_dbf, netID, gaugeID, rr_time, db_rr_str) < 0) {
	  if (verbose) {
		fprintf(stderr, "Warning: Failed to fetch from the db for line <%s> in file: %s to the DB.\n", line, fname);
	  }
	  error++;
	  if (error > 25) { /* too many errors */
		rc = -1;
		break;
	  }
	}
	if (strcmp(db_rr_str, ascii_file_rr_str) != 0) {
	  if (verbose)
		fprintf(stderr, "Unmatched entry: ascii: %s  db: %s time: %s\n", ascii_file_rr_str, db_rr_str, ctime(&rr_time));
	  (*unmatched_count)++;
	}
  }

DONE:

  if (fp)
	fclose(fp);

  return rc;

} /* validate_file_against_db */

