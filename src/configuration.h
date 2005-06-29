
#define SOURCE_TOKEN "SOURCE="
#define WORKINGDIR_TOKEN "WORKINGDIR="
#define WORKINGDIR_TOKEN_LEN 256
#define EXCLUDE_TOKEN "EXCLUDE="

struct exclude_list {
  char **excludes;
  unsigned int count;
};

struct source_list {
  char **url;
  unsigned int count;
};

typedef struct {
  struct source_list *sources;
  char working_dir[WORKINGDIR_TOKEN_LEN];
  struct exclude_list *exclude_list;
  BOOL_T download_only;
  BOOL_T dist_upgrade;
  BOOL_T simulate;
  BOOL_T no_prompt;
  BOOL_T re_install;
  BOOL_T ignore_excludes;
  BOOL_T no_md5_check;
  BOOL_T ignore_dep;
  BOOL_T disable_dep_check;
  BOOL_T print_uris;
  BOOL_T dl_stats;
  BOOL_T remove_obsolete;
  int(*progress_cb)(void *,double,double,double,double);

} rc_config;

/*
  read the configuration from file_name
  returns (rc_config *) or NULL
*/
rc_config *read_rc_config(const char *file_name);

/*
  check that working_dir exists or make it if permissions allow
*/
void working_dir_init(const rc_config *global_config);

/*
  free rc_config structure
*/
void free_rc_config(rc_config *global_config);

/*
  add an exclude expression to the exclude list.
  commonly called with global_config->exclude_list
*/
void add_exclude(struct exclude_list *list,const char *e);

/*
  add an source expression to the source list.
  commonly called with global_config->source_list
*/
void add_source(struct source_list *list,const char *s);

