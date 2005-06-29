
#define PKG_PARSE_REGEX "(.*{1,})\\-(.*[\\.\\-].*[\\.\\-].*).tgz[ ]{0,}$"
#define PKG_NAMEVER "(.*{1,})\\-(.*[\\.\\-].*[\\.\\-].*)"
#define PKG_VER "(.*)[\\.\\-](.*)[\\.\\-](.*)"
#define PKG_NAME_PATTERN "^PACKAGE NAME:[ ]{1,}(.*{1,})\\-(.*[\\.\\-].*[\\.\\-].*).tgz[ ]{0,}$"
#define PKG_MIRROR_PATTERN "^PACKAGE MIRROR:[ ]+(.*)$"
#define PKG_LOCATION_PATTERN "^PACKAGE LOCATION:[ ]+(.*)$"
#define PKG_SIZEC_PATTERN "^PACKAGE SIZE [(]+compressed[)]{1,}:[ ]{1,}([0-9]{1,}) K$"
#define PKG_SIZEU_PATTERN "^PACKAGE SIZE [(]+uncompressed[)]{1,}:[ ]{1,}([0-9]{1,}) K$"
#define PKG_LOG_SIZEC_PATTERN "^COMPRESSED PACKAGE SIZE:[ ]{1,}([0-9]{1,}) K$"
#define PKG_LOG_SIZEU_PATTERN "^UNCOMPRESSED PACKAGE SIZE:[ ]{1,}([0-9]{1,}) K$"
#define PKG_LOG_DIR "/var/log/packages"
#define ROOT_ENV_NAME "ROOT"
#define ROOT_ENV_LEN 255
#define PKG_LOG_PATTERN "^(.*{1,})\\-(.*[\\.\\-].*[\\.\\-].*)"
#define MD5SUM_REGEX "([a-zA-Z0-9]{1,})[ ]{1,}([a-zA-Z0-9\\/._\\-]{1,})\\/(.*{1,})\\-(.*[\\.\\-].*[\\.\\-].*).tgz$"
#define REQUIRED_REGEX "^[ ]{0,}([a-zA-Z0-9\\+_\\-]+)[ ]{0,}([\\<\\=\\>]+){0,}[ ]{0,}([a-zA-Z0-9\\.\\_\\-]+){0,}[ ]{0,}$"
#define MD5_STR_LEN 34
#define MD5_CHECKSUM_FAILED -100
#define PKG_LIST "PACKAGES.TXT"
#define PKG_LIST_L "package_data"
#define PATCHES_LIST "patches/PACKAGES.TXT"
#define PATCHDIR "patches/"
#define REMOVE_CMD "/sbin/removepkg "
#define INSTALL_CMD "/sbin/installpkg "
#define UPGRADE_CMD "/sbin/upgradepkg --reinstall "
#define CHECKSUM_FILE "CHECKSUMS.md5"
#define HEAD_FILE_EXT ".head"

typedef struct {
  char *name;
  char *version;
  char *mirror;
  char *location;
  unsigned int size_c;
  unsigned int size_u;
  char *description;
  char *required;
  char *conflicts;
  char *suggests;
  char md5[MD5_STR_LEN];
} pkg_info_t;

struct pkg_list {
  pkg_info_t **pkgs;
  unsigned int pkg_count;
  BOOL_T free_pkgs;
};

typedef struct {
  pkg_info_t *installed;
  pkg_info_t *upgrade;
} pkg_upgrade_t;

struct pkg_upgrade_list {
  pkg_upgrade_t **pkgs;
  unsigned int pkg_count;
};

struct pkg_version_parts {
  char **parts;
  unsigned int count;
};

typedef struct {
  char *pkg;
  char *error;
} pkg_err_t;

struct pkg_err_list {
  unsigned int err_count;
  pkg_err_t **errs;
};

/* returns an empty package structure */
__inline pkg_info_t *init_pkg(void);
/* frees the package structure */
void free_pkg(pkg_info_t *pkg);

/* create an empty package list */
struct pkg_list *init_pkg_list(void);
/* add a package to a package list */
void add_pkg_to_pkg_list(struct pkg_list *list,pkg_info_t *pkg);
/* free package list */
void free_pkg_list(struct pkg_list *);

/* update the local package cache */
int update_pkg_cache(const rc_config *global_config);
/* write pkg data to disk */
void write_pkg_data(const char *source_url,FILE *d_file,struct pkg_list *pkgs);
/* parse the PACKAGES.TXT file */
struct pkg_list *parse_packages_txt(FILE *);
/* return a list of available packages */
struct pkg_list *get_available_pkgs(void);
/* retrieve list of installed pkgs */
struct pkg_list *get_installed_pkgs(void);

/* retrieve the newest package from package list */
pkg_info_t *get_newest_pkg(struct pkg_list *,const char *);
/* get the exact package */
pkg_info_t *get_exact_pkg(struct pkg_list *list,const char *name,
                          const char *version);
/* lookup package by details */
pkg_info_t *get_pkg_by_details(struct pkg_list *list,char *name,
                               char *version,char *location);
/* search package list with pattern */
struct pkg_list *search_pkg_list(struct pkg_list *available,
                                 const char *pattern);


/*
  install package by calling installpkg
  returns 0 on success, -1 on error
*/
int install_pkg(const rc_config *,pkg_info_t *);
/*
  upgrade package by calling upgradepkg
  returns 0 on success, -1 on error
*/
int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,
                pkg_info_t *pkg);
/*
  remove package by calling removepkg
  returns 0 on success, -1 on error
*/
int remove_pkg(const rc_config *,pkg_info_t *);


/*
  generate a short description, returns (char *) on success or NULLon error
  caller responsible for freeing the returned data
*/
char *gen_short_pkg_description(pkg_info_t *);
/*
  generate the filename from the url
  caller responsible for freeing the returned data
*/
char *gen_filename_from_url(const char *url,const char *file);
/*
  generate the package file name
  caller responsible for freeing the returned data
*/
char *gen_pkg_file_name(const rc_config *global_config,pkg_info_t *pkg);
/*
  generate the head cache filename
  caller responsible for freeing the returned data
*/
char *gen_head_cache_filename(const char *filename_from_url);
/*
  generate the download url for a package
  caller responsible for freeing the returned data
*/
char *gen_pkg_url(pkg_info_t *pkg);
/*
  exclude pkg based on pkg name
  returns 1 if package is present in the exclude list, 0 if not present
*/
int is_excluded(const rc_config *,pkg_info_t *);
/*
  package is already downloaded and cached, md5sum if applicable is ok
  returns 0 if download is cached, -1 if not
*/
int verify_downloaded_pkg(const rc_config *global_config,pkg_info_t *pkg);
/*
  fill in the md5sum of the package
*/
void get_md5sum(pkg_info_t *pkg,FILE *checksum_file);
/*
  find out the pkg file size (post download)
*/
size_t get_pkg_file_size(const rc_config *global_config,pkg_info_t *pkg);

/*
  compare package versions
  returns just like strcmp,
    > 0 if a is greater than b
    < 0 if a is less than b
    0 if a and b are equal
*/
int cmp_pkg_versions(char *a, char *b);

/*
  resolve dependencies
  returns 0 on success, -1 on error setting conflict_err and missing_err
  (usually called with transaction->conflict_err and transaction->missing_err)
*/
int get_pkg_dependencies(const rc_config *global_config,
                         struct pkg_list *avail_pkgs,
                         struct pkg_list *installed_pkgs,pkg_info_t *pkg,
                         struct pkg_list *deps,
                         struct pkg_err_list *conflict_err,
                         struct pkg_err_list *missing_err);
/*
  return list of package conflicts
*/
struct pkg_list *get_pkg_conflicts(struct pkg_list *avail_pkgs,
                                   struct pkg_list *installed_pkgs,
                                   pkg_info_t *pkg);
/*
  return list of packages required by
*/
struct pkg_list *is_required_by(const rc_config *global_config,
                                struct pkg_list *avail, pkg_info_t *pkg);

/*
  empty packages from cache dir
*/
void clean_pkg_dir(const char *dir_name);
/*
  clean out old outdated packages in the cache that are no longer available
  in the current source lists (ie are not downloadable)
*/
void purge_old_cached_pkgs(const rc_config *global_config,char *dir_name,
                           struct pkg_list *avail_pkgs);

/*
  make a copy of a package (needs to be freed with free_pkg)
*/
pkg_info_t *copy_pkg(pkg_info_t *dst,pkg_info_t *src);

/*
  package error handling api to handle errors within core functions
*/
struct pkg_err_list *init_pkg_err_list(void);
void add_pkg_err_to_list(struct pkg_err_list *l,
                         const char *pkg,const char *err);
int search_pkg_err_list(struct pkg_err_list *l,
                        const char *pkg, const char *err);
void free_pkg_err_list(struct pkg_err_list *l);

