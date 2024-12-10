#include <stdio.h>
#include <gdbm.h>
#include <malloc.h>
#include <string.h>

#include <gv_utils.h>

/*
 * Simply list the keys and content. 
 */

int main (int ac, char **av)
{
  char *infile = "/usr/local/trmm/GVBOX/data/db/gdbm_gauge.db";
  GDBM_FILE gf;
  datum key, content;
  char *key_str, *content_str;
  char date_str[20], time_str[20];
  int ngID;
  time_t time_sec;
  char file_type;
  int len;

  if (ac == 3) {
	file_type = av[1][0];
	infile = av[2];
	if (file_type != 'g' && file_type != 'f')
	  goto USAGE;
  }
  else  {
  USAGE:
	fprintf(stderr, "Usage: %s g|f gdbm_file\n", av[0]);
	exit(-1);
  }

  gf = gdbm_open(infile, 512, GDBM_READER, 0, 0);
  if (gf == NULL) {
	perror(gdbm_strerror(gdbm_errno));
	perror(infile);
	exit(-1);
  }
  for(key = gdbm_firstkey(gf); key.dptr; key = gdbm_nextkey(gf, key)) {

	key_str = (char *)calloc(key.dsize+1, sizeof(char));
	memmove(key_str, key.dptr, key.dsize);
	content = gdbm_fetch(gf, key);
	content_str = (char *)calloc(content.dsize+1, sizeof(char));
	memmove(content_str, content.dptr, content.dsize);
	
	if (key_str[0] == '2') {
	  /* key_str = '2 ngID time', in binary -- both in the first 
	   * inter. file and gauge db.
	   */
	  len = sizeof(int);
	  if (file_type == 'f') len = sizeof(int);

	  /* Extract ngID  */
	  memcpy(&ngID, key_str + 2, len);
	  /* Extract time */
	  memcpy(&time_sec, key_str + 3 + len, sizeof(time_t));
	  strcpy(time_str, "");
	  strcpy(date_str, "");
	  time_secs2date_time_strs(time_sec, 1, 0, date_str, time_str);
	  printf("Key<<%4.4d %s %s>>, ", ngID, date_str, time_str);
	  printf(" Content<<%s>>\n", content_str);
	}
	else {
	  printf("Key<<%s>>, Content<<%s>>\n", key_str, content_str);
	  
	}

	free(key_str); free(content_str);

  }

  gdbm_close(gf);
  exit(0);
}
