/* utils.c
 *     Contains utilities routines for building the zr table.
 */
#include <stdio.h>
#include <stdlib.h>

extern int strcasecmp(const char *s1, const char *s2);
char *tsdis_site2gsl_site(char *tsdis_site)
{
  /* Convert TSDIS site name to GSL site name.
   * Return a pointer to GSL site name or NULL. 
   *  The caller needs to deallocate its space.
   */
  struct _site_list {
		char *tsdis_radar_site;
		char *gsl_radar_site;
  } site_list[] = {
		{"DARW", "DARW"},
		{"MELB", "KMLB"},
		{"KMLB", "KMLB"},     /* Some old uf files use KMLB. */
		{"KWAJ", "KWAJ"},
		{"HSTN", "KHGX"},
		{"THOM", "TOMK"},
		{"GUAM", "GUAM"},
		{"ISBN", "ISBN"},
		{"SAOP", "SAOP"},
		{"TWWF", "TWWF"},
		{"TAMP", "KTBW"},
		{"MIAM", "KMIA"},
		{"KEYW", "KEYW"},
		{"LKCH", "KLCH"},
		{"NWBR", "NWBR"},
		{"CORC", "CORC"},
		{NULL, NULL}
  };
  int i;
  char *gsl_site = NULL;

  if (tsdis_site == NULL) return NULL;
  i = 0;
  while (site_list[i].tsdis_radar_site != NULL) {
		if (strcasecmp(site_list[i].tsdis_radar_site, tsdis_site) == 0) {
			gsl_site = (char *) strdup(site_list[i].gsl_radar_site);
			break;
		}
		i++;
  }
  return gsl_site;
}
