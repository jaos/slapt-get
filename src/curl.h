
/*
  download data to file
  returns curl code
*/
int slapt_download_data(FILE *, const char *, size_t, long *, const slapt_rc_config *);

/*
  retrieves the head data for the url, returns (char *) or NULL on error
*/
char *slapt_head_request(const char *url);

/*
  this fills FILE with data from url, used for PACKAGES.TXT and CHECKSUMS
  Returns error on failure.
*/
const char *slapt_get_mirror_data_from_source(FILE *fh,
                                              const slapt_rc_config *global_config,
                                              const char *base_url,
                                              const char *filename);

/*
  download pkg, calls download_data.  returns error on failure.
*/
const char *slapt_download_pkg(const slapt_rc_config *global_config,
                               slapt_pkg_info_t *pkg, const char *note);

/*
  this is the default progress callback if global_config->progress_cb == NULL
*/
int slapt_progress_callback(void *clientp, double dltotal, double dlnow,
                      double ultotal, double ulnow);

/*
  do a head request on the mirror data to find out if it's new
  returns (char *) or NULL
*/
char *slapt_head_mirror_data(const char *wurl,const char *file);
/*
  clear head cache storage file
*/
void slapt_clear_head_cache(const char *cache_filename);
/*
  cache the head request
*/
void slapt_write_head_cache(const char *cache, const char *cache_filename);
/*
  read the cached head request
  returns (char *) or NULL
*/
char *slapt_read_head_cache(const char *cache_filename);

struct slapt_progress_data
{
  size_t bytes;
  time_t start;
};

struct slapt_progress_data *slapt_init_progress_data(void);
void slapt_free_progress_data(struct slapt_progress_data *d);

