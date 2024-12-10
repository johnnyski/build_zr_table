
/*
 *
 * zr.h
 *     Common definitions for zr programs.
 */


#ifndef __ZR_H__
#define __ZR_H__ 1


/* Header section of the table. */
#define COMMENT_CHAR        '#'
#define FILE_COMMENT_CHARS  "###"
#define USER_COMMENT_CHARS   "##"
#define TABLE_START_STR     "Table begins:"
#define SITE_NAME_STR       "Site_name"
#define START_DATE_TIME_STR "Start_date_time"
#define END_DATE_TIME_STR   "End_date_time"
#define TABLE_RECORD_INFO_STR "TABLE RECORD INFORMATION:"
#define COMMENT_RECORD_FORMAT_LINE   "#* "
#define COMMENT_RECORD_FORMAT_LINE1  "#1 "
#define COMMENT_RECORD_FORMAT_LINE2  "#2 "
#define COMMENT_RECORD_FORMAT_LINE3  "#3 "
#define COMMENT_RECORD_FORMAT_LINE4  "#4 "

#define COMMENT_RECORD_FORMAT_LINE5  "#5 "
#define COMMENT_RECORD_FORMAT_LINE6  "#6 "
#define COMMENT_RECORD_FORMAT_LINE7  "#7 "
#define COMMENT_RECORD_FORMAT_LINE8  "#8 "

#define COMMENT_RECORD_FORMAT_LINE9  "#9 "
#define COMMENT_RECORD_FORMAT_LINE10 "#10 "


#define MISSING_Z    -99.0 /* For 3 D reflectivity. */

typedef enum  {NO_ECHO_C=0, UNIFORM_C=1, STRATIFORM_C = 1, CONVECTIVE_C = 2, 
			   MISSING_OR_BAD_DATA_C=-1} rain_type_t;

char *tsdis_site2gsl_site(char *tsdis_site);

#endif
