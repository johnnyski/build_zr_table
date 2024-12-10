/* get_2A53_data_over_gauge.h
 */


#ifndef __GET_2A53_DATA_OVER_GAUGE_H__
#define __GET_2A53_DATA_OVER_GAUGE_H__ 1

#include <IO.h>

#include <2A53.h>

#include <gsl.h>
#include <gdbm.h>
#include "zr.h"

#define MAX_NAME_LEN     51
#define MAX_FILENAME_LEN 256
#define MAX_LINE_LEN     256
#define MAX_HEIGHT_LEVELS 4

typedef enum RAIN_classification rain_class_type_t;


typedef struct _zrc {
  rain_type_t   c;            /* Rain type. */
  float         r;            /* R: rainrate in mm/hr */
} zc_t;

typedef struct _height_info {
  float         height;   /* Height in km. */
  zc_t          *zc;      /* nzc entries of 2 tuples(R, raintype).
						   * Note: The first tuple is closest to the gauge. */
  int           nzc;      /* Number of zc entries. (= nx1 * nx2) */
} height_info_t;

typedef struct _zc_column {
  char          gauge_id[MAX_NAME_LEN];
  float         gauge_range; /* Distance from radar to gauge in km. */
  rain_type_t   c;           /* Rain type at gauge. */
  DATE_STR      sdate;    /* Start date of VOS */
  TIME_STR      stime;    /* Start time of VOS */
  height_info_t *hinfo[MAX_HEIGHT_LEVELS];   /* List of height info. */
  int           nhinfo;   /* Number of hinfo entries */
  int           nx1;  /* # 1st coord_cells.  Cartesian: x  Polar: range */
  int           nx2;  /* # 2nd coord_cells.  Cartesian: y  Polar: azimuth*/
  int           nx3;  /* # 3rd coord cells.  Cartesian: z  Polar: z */
} zc_column_t;


typedef struct _gauge {
  char          net_name[MAX_NAME_LEN]; /* i.e., KSC, STJ, ... */
  Gauge_list    *gauges;                    /* Gauges for this network. */
  struct _gauge *next;
} gauge_network_t;


#define CLOSE_FILES_AND_EXIT(fh, gauge_dbf, rc)     \
      {if (fh != NULL) TKclose(fh); \
       clean_up(); \
       exit(rc); \
      }

void free_gauge_network_list(gauge_network_t *gnet_list);
void free_zc_column(zc_column_t *column);
int initialize_zc_column(zc_column_t *column, 
						 float gauge_win_xmax, float gauge_win_ymax,
						 float gauge_win_zmax);
int extract_column(zc_column_t *column, Gauge_info *gauge,
									 L2A_53_SINGLE_RADARGRID *d3Drefl_grid, 
									 Raintype_map *rain_class);
void append_column_to_file(zc_column_t *column,
						   char *net_name, GDBM_FILE fp);
int csmap2raintype_map(L2A_54_SINGLE_RADARGRID *csmap, Raintype_map *rtmap);
Raintype_map **create_rain_class_list(char *csmap_file);
Raintype_map *get_rain_class(TIME_STR *class_time, char *csmap_file);
int get_next_3D_field(IO_HANDLE *fh, L2A_53_SINGLE_RADARGRID *field);
gauge_network_t *get_gauge_network_list(char *gauge_top_dir, char *site);
Raintype_map *set_single_raintype(void);
void free_raintype_map(Raintype_map *map);
GDBM_FILE open_outfile_and_write_header_info(char *site,
											 DATE_STR *sdate, TIME_STR *stime,
											 DATE_STR *edate, TIME_STR *etime,
											 float lat, float lon,
											 float gauge_win_xmax,
											 float gauge_win_ymax, 
											 float gauge_win_zmax,
											 rain_class_type_t rain_class_type,
											 char *fname);
int read_granule_info_from_hdf(IO_HANDLE *hdf_fh, int *nvos, float *lat, 
							   float *lon, DATE_STR *sdate, TIME_STR *stime, 
							   DATE_STR *edate, TIME_STR *etime, 
							   char *site);
void clean_up();
#endif
