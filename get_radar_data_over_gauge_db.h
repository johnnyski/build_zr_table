/* 
 * get_radar_data_over_gauge_db.h
 */

#ifndef __GET_RADAR_DATA_OVER_GAUGE_DB_H__
#define __GET_RADAR_DATA_OVER_GAUGE_DB_H__ 1

#include "get_radar_data_over_gauge.h"

#define HEADER_LEN 15000
#define MAX_ENTRY_LINE_LEN 2000
#define MAX_NGID_ENTRIES 2000

typedef struct {
  char netID[MAX_NAME_LEN];
  int gaugeID;
  int ngID;
} ngID_entry_t;

typedef struct _ngID_list{
  ngID_entry_t ngID_array[MAX_NGID_ENTRIES];
  int nentries;
  struct _ngID_list *next;
} ngID_list_t;
int get_ngID_list_from_db(GDBM_FILE gf, ngID_list_t *ngID_list);
int read_header_from_db(GDBM_FILE gf, char *header_line);
int construct_entry_line(GDBM_FILE gf, datum *key, ngID_list_t *ngID_list,char *entry_line);

void print_ngID_list(ngID_list_t *ngID_list);

#endif
