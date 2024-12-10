#include <stdio.h>
#include <gdbm.h>
#include <malloc.h>
#include <string.h>
#include "zr.h"
#include "get_radar_data_over_gauge_db.h"

char *this_prog = "first2ascii";
int verbose = 0;
/**********************************************************************/
/*                                                                    */
/*                             main                                   */
/*                                                                    */
/**********************************************************************/
int main (int ac, char **av)
{
  char *infile = "test.db";
  GDBM_FILE gf;
  datum key, nextkey;
  char *key_str;
  int rc = 0;
  char header_line[HEADER_LEN];
  ngID_list_t ngID_list;
  char entry_line[MAX_ENTRY_LINE_LEN];

  this_prog = av[0];
  if (ac == 2) infile = av[1];
  else {
	fprintf(stderr, "Usage (%s): Convert the first ZR intermediate file from GDBM to ASCII.\n", PROG_VERSION);
	fprintf(stderr, "\t %s gdbm_file\n", av[0]);
	exit(-1);
  }

  gf = gdbm_open(infile, 512, GDBM_READER, 0, 0);
  if (gf == NULL) {
	perror(gdbm_strerror(gdbm_errno));
	perror(infile);
	exit(-1);
  }

  memset(header_line, '\0', HEADER_LEN);
  read_header_from_db(gf, header_line);
  printf("%s\n", header_line);
  
  /* Read all of the ngIDs from the DB once. This list will be used to
   * lookup for gaugeID and netID when constructing an entry line.
   */
  memset(&ngID_list, '\0', sizeof(ngID_list_t));
  ngID_list.next = NULL;
  ngID_list.nentries = 0;
  if (get_ngID_list_from_db(gf, &ngID_list) < 0) {
	rc = -1;
	goto DONE;
  }
  if (verbose)
	print_ngID_list(&ngID_list);
  key.dptr = NULL;

  for(nextkey = gdbm_firstkey(gf); nextkey.dptr; 
	  nextkey = gdbm_nextkey(gf, key)) {
	key_str = nextkey.dptr;
	memset(entry_line, '\0', MAX_ENTRY_LINE_LEN);
	if (construct_entry_line(gf, &nextkey, &ngID_list, entry_line) == 1) 
	  printf("%s\n", entry_line);

	if (key.dptr)
	  free(key.dptr);
	key = nextkey;
	
  }
  if (key.dptr)
	free(key.dptr);
DONE:
  gdbm_close(gf);
  exit(0);
}



