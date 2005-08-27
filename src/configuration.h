
#define SOURCE_TOKEN "SOURCE="
#define WORKINGDIR_TOKEN "WORKINGDIR="
#define WORKINGDIR_TOKEN_LEN 256
#define EXCLUDE_TOKEN "EXCLUDE="

struct slapt_exclude_list {
  char **excludes;
  unsigned int count;
};

struct slapt_source_list {
  char **url;
  unsigned int count;
};

typedef struct {
  struct slapt_source_list *sources;
  char working_dir[WORKINGDIR_TOKEN_LEN];
  struct slapt_exclude_list *exclude_list;
  SLAPT_BOOL_T download_only;
  SLAPT_BOOL_T dist_upgrade;
  SLAPT_BOOL_T simulate;
  SLAPT_BOOL_T no_prompt;
  SLAPT_BOOL_T re_install;
  SLAPT_BOOL_T ignore_excludes;
  SLAPT_BOOL_T no_md5_check;
  SLAPT_BOOL_T ignore_dep;
  SLAPT_BOOL_T disable_dep_check;
  SLAPT_BOOL_T print_uris;
  SLAPT_BOOL_T dl_stats;
  SLAPT_BOOL_T remove_obsolete;
  SLAPT_BOOL_T no_upgrade;
  unsigned int retry;
  int(*progress_cb)(void *,double,double,double,double);

} slapt_rc_config;

/*
  read the configuration from file_name
  returns (rc_config *) or NULL
*/
slapt_rc_config *slapt_read_rc_config(const char *file_name);

/*
  check that working_dir exists or make it if permissions allow
*/
void slapt_working_dir_init(const slapt_rc_config *global_config);

/*
  free rc_config structure
*/
void slapt_free_rc_config(slapt_rc_config *global_config);

/*
  add an exclude expression to the exclude list.
  commonly called with global_config->exclude_list
*/
void slapt_add_exclude(struct slapt_exclude_list *list,const char *e);

/*
  add or remove a package source url to the source list.
  commonly called with global_config->source_list
*/
void slapt_add_source(struct slapt_source_list *list,const char *s);
void slapt_remove_source (struct slapt_source_list *list, const char *s);

