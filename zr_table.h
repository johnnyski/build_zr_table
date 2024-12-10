#ifndef __zr_table__
#define  __zr_table__ 1

enum RAIN_classification {NOCLASS, SINGLE=3, DUAL=4, MULTI=5};

/* NOECHO, MISSING ... defined in terms of class. */
enum RAIN_type {NOECHO, UNIFORM, STRATIFORM, CONVECTIVE, MISSING=-99};

#define ZR_STR_LEN 200
typedef struct {
  char site_name[ZR_STR_LEN];  /* Generic site description.      */
  time_t start_time, stop_time;/* Unix notation -- seconds.      */
  float radar_lat, radar_lon;  /* lat/lon in degrees.            */
  int nrange;                  /* # of range intervals           */
  int nrtype;                  /* # of rain classifications.     */
  int ndbz;                    /* # of dbz bins.  This can be    */
							   /*   computed from dbz range and  */
                               /*   resolution.                  */
  float dbz_low, dbz_hi, dbz_res;  /* dbz range and resolution (in dbz) */
  char **rain_type_str;        /* 1..nclasses, description of    */
                               /* rain classifications  one per  */
                               /* record.                        */
  float *range_interval;       /* 0..nrange-1                    */
  float ***r;                  /* Dynamic allocate nstrat x nclasses x ndbz */

  /* PRIVATE -- DO NOT TOUCH THE MEMBERS BELOW THIS LINE -- */
  /* They are automatically set by the routine read_zr      */
  int *range_hash_table;
} Zr_table;
  
/* Function prototypes. */
Zr_table *read_ZR(char *file);
int write_ZR(Zr_table *zr, char *file);
float applyZRtable(int rain_type,
					float pix_range,
					float dbz,
					Zr_table *zr_table);

#endif








