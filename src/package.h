/*
 * Copyright (C) 2003-2023 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define SLAPT_PKG_PARSE_REGEX "(.*{1,})\\-(.*[\\-].*[\\-].*)\\.[ti]+[gblxkz]+[ ]{0,}$"
#define SLAPT_PKG_NAMEVER "(.*{1,})\\-(.*[\\-].*[\\-].*)"
#define SLAPT_PKG_VER "(.*)[\\-](.*)[\\-](.*)"
#define SLAPT_PKG_NAME_PATTERN "^PACKAGE NAME:[ ]{1,}(.*{1,})\\-(.*[\\-].*[\\-].*)(\\.[ti]+[gblzkx]+)[ ]{0,}$"
#define SLAPT_PKG_MIRROR_PATTERN "^PACKAGE MIRROR:[ ]+(.*)$"
#define SLAPT_PKG_PRIORITY_PATTERN "^PACKAGE PRIORITY:[ ]+([0-9]{1,})$"
#define SLAPT_PKG_LOCATION_PATTERN "^PACKAGE LOCATION:[ ]+(.*)$"
#define SLAPT_PKG_SIZEC_PATTERN "^PACKAGE SIZE [(]+compressed[)]{1,}:[ ]{1,}([0-9]{1,}) K$"
#define SLAPT_PKG_SIZEU_PATTERN "^PACKAGE SIZE [(]+uncompressed[)]{1,}:[ ]{1,}([0-9]{1,}) K$"
#define SLAPT_PKG_LOG_SIZEC_PATTERN "^COMPRESSED PACKAGE SIZE:[ ]{1,}([0-9\\.]{1,})[ ]{0,}([GgMmKk])$"
#define SLAPT_PKG_LOG_SIZEU_PATTERN "^UNCOMPRESSED PACKAGE SIZE:[ ]{1,}([0-9\\.]{1,})[ ]{0,}([GgMmKk])$"
#define SLAPT_PKG_LOG_DIR "/var/log/packages"
#define SLAPT_PKG_LIB_DIR "/var/lib/pkgtools/packages"
#define SLAPT_ROOT_ENV_NAME "ROOT"
#define SLAPT_ROOT_ENV_LEN 255
#define SLAPT_PKG_LOG_PATTERN "^(.*{1,})\\-(.*[\\-].*[\\-].*)"
#define SLAPT_MD5SUM_REGEX "([a-zA-Z0-9]{1,})[ ]{1,}([a-zA-Z0-9\\/._+\\-]{1,})\\/(.*{1,})\\-(.*[\\-].*[\\-].*)\\.[ti]+[gblzkx]+$"
#define SLAPT_REQUIRED_REGEX "^[ ]{0,}([^ \\<\\=\\>]{1,})[ ]{0,}([\\<\\=\\>]+){0,}[ ]{0,}([a-zA-Z0-9\\+\\.\\_\\-]+){0,}[ ]{0,}$"
#define SLAPT_MD5_STR_LEN 32
#define SLAPT_PKG_LIST "PACKAGES.TXT"
#define SLAPT_PKG_LIST_GZ "PACKAGES.TXT.gz"
#define SLAPT_PKG_LIST_L "package_data"
#define SLAPT_PATCHES_LIST "patches/PACKAGES.TXT"
#define SLAPT_PATCHES_LIST_GZ "patches/PACKAGES.TXT.gz"
#define SLAPT_CHANGELOG_FILE "ChangeLog.txt"
#define SLAPT_CHANGELOG_FILE_GZ "ChangeLog.txt.gz"
#define SLAPT_PATCHDIR "patches/"
#define SLAPT_REMOVE_CMD "/sbin/removepkg "
#define SLAPT_INSTALL_CMD "/sbin/installpkg "
#define SLAPT_UPGRADE_CMD "/sbin/upgradepkg --reinstall "
#define SLAPT_CHECKSUM_FILE "CHECKSUMS.md5"
#define SLAPT_CHECKSUM_FILE_GZ "CHECKSUMS.md5.gz"
#define SLAPT_HEAD_FILE_EXT ".head"
#define SLAPT_MAX_MMAP_SIZE 1024
#define SLAPT_MAX_ZLIB_BUFFER 1024

typedef enum {
    DEP_OP_ANY = 0,
    DEP_OP_OR,
    DEP_OP_EQ,
    DEP_OP_GTE,
    DEP_OP_LTE,
    DEP_OP_GT,
    DEP_OP_LT,
} slapt_dependency_op;

typedef struct {
    slapt_dependency_op op;
    union {
        struct {
            char *name;
            char *version;
        };
        struct {
            slapt_vector_t* alternatives;
        };
    };
} slapt_dependency_t;
slapt_dependency_t* slapt_dependency_t_init(void);
void slapt_dependency_t_free(slapt_dependency_t *);
slapt_dependency_t *slapt_dependency_t_parse_required(const char *);

typedef struct {
    char *name;
    char *version;
    char *mirror;
    char *location;
    char *description;
    char *required;
    char *conflicts;
    char *suggests;
    char *file_ext;
    slapt_vector_t *dependencies;
    uint32_t size_c;
    uint32_t size_u;
    uint32_t priority;
    bool installed;
    char md5[SLAPT_MD5_STR_LEN + 1];
} slapt_pkg_t;

typedef struct {
    slapt_pkg_t *installed;
    slapt_pkg_t *upgrade;
} slapt_pkg_upgrade_t;

slapt_pkg_upgrade_t *slapt_pkg_upgrade_t_init(slapt_pkg_t *, slapt_pkg_t *);
void slapt_pkg_upgrade_t_free(slapt_pkg_upgrade_t *);

typedef struct {
    char *pkg;
    char *error;
} slapt_pkg_err_t;

slapt_pkg_err_t *slapt_pkg_err_t_init(char *, char *);
void slapt_pkg_err_t_free(slapt_pkg_err_t *);

/* returns an empty package structure */
slapt_pkg_t *slapt_pkg_t_init(void);
/* frees the package structure */
void slapt_pkg_t_free(slapt_pkg_t *pkg);
/* generate a short description, returns (char *) on success or NULL on error, caller responsible for freeing the returned data */
char *slapt_pkg_t_short_description(const slapt_pkg_t *);
/* clean package name from package description */
char *slapt_pkg_t_clean_description(const slapt_pkg_t *);
/* generate the download url for a package, caller responsible for freeing the returned data */
char *slapt_pkg_t_url(const slapt_pkg_t *pkg);
/* compare package versions,
  returns just like strcmp,
    > 0 if a is greater than b
    < 0 if a is less than b
    0 if a and b are equal
*/
int slapt_pkg_t_cmp_versions(const char *a, const char *b);
int slapt_pkg_t_cmp(const slapt_pkg_t *a, const slapt_pkg_t *b);
/* make a copy of a package (needs to be freed with free_pkg) */
slapt_pkg_t *slapt_pkg_t_copy(slapt_pkg_t *dst, const slapt_pkg_t *src);
/* retrieve the packages changelog entry, if any.  Returns NULL otherwise, Must be chdir'd to working_dir.  */
char *slapt_pkg_t_changelog(const slapt_pkg_t *pkg);
/* returns a string representation of the package */
char *slapt_pkg_t_string(const slapt_pkg_t *pkg);
/*
  get the package filelist, returns (char *) on success or NULL on error
  caller responsible for freeing the returned data
*/
char *slapt_pkg_t_filelist(const slapt_pkg_t *pkg);
/* used by qsort */
int slapt_pkg_t_qsort_cmp(const void *a, const void *b);

/*
  update the local package cache. Must be chdir'd to working_dir.
*/
int slapt_update_pkg_cache(const slapt_config_t *global_config);
/* write pkg data to disk */
void slapt_write_pkg_data(const char *source_url, FILE *d_file, const slapt_vector_t *pkgs);
/* parse the PACKAGES.TXT file */
slapt_vector_t *slapt_parse_packages_txt(FILE *);
/*
  return a list of available packages.  Must be chdir'd to
  rc_config->working_dir.  Otherwise, open a filehandle to the package data
  and pass it to slapt_parse_packages_txt();
*/
slapt_vector_t *slapt_get_available_pkgs(void);
/* retrieve list of installed pkgs */
slapt_vector_t *slapt_get_installed_pkgs(void);

/* retrieve the newest package from package list */
slapt_pkg_t *slapt_get_newest_pkg(const slapt_vector_t *, const char *);
/* get the exact package */
slapt_pkg_t *slapt_get_exact_pkg(const slapt_vector_t *list, const char *name, const char *version);
/* lookup package by details */
slapt_pkg_t *slapt_get_pkg_by_details(const slapt_vector_t *list, const char *name, const char *version, const char *location);
/* search package list with pattern */
slapt_vector_t *slapt_search_pkg_list(const slapt_vector_t *list, const char *pattern);

/* install package by calling installpkg, returns 0 on success, -1 on error */
int slapt_install_pkg(const slapt_config_t *, const slapt_pkg_t *);
/* upgrade package by calling upgradepkg, returns 0 on success, -1 on error */
int slapt_upgrade_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg);
/* remove package by calling removepkg, returns 0 on success, -1 on error */
int slapt_remove_pkg(const slapt_config_t *, const slapt_pkg_t *);

/* get a list of obsolete packages */
slapt_vector_t *slapt_get_obsolete_pkgs(const slapt_config_t *global_config, const slapt_vector_t *avail_pkgs, const slapt_vector_t *installed_pkgs);

/* generate the filename from the url, caller responsible for freeing the returned data */
char *slapt_gen_filename_from_url(const char *url, const char *file);
/* generate the package file name, caller responsible for freeing the returned data */
char *slapt_gen_pkg_file_name(const slapt_config_t *global_config, const slapt_pkg_t *pkg);
/* exclude pkg based on pkg name, returns 1 if package is present in the exclude list, 0 if not present */
bool slapt_is_excluded(const slapt_config_t *, const slapt_pkg_t *);
/* package is already downloaded and cached, md5sum if applicable is ok, returns slapt_code_t.  */
slapt_code_t slapt_verify_downloaded_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg);
/* fill in the md5sum of the package */
void slapt_get_md5sums(slapt_vector_t *pkgs, FILE *checksum_file);
/* find out the pkg file size (post download) */
size_t slapt_get_pkg_file_size(const slapt_config_t *global_config, const slapt_pkg_t *pkg);

/*
  resolve dependencies
  returns 0 on success, -1 on error setting conflict_err and missing_err
  (usually called with transaction->conflict_err and transaction->missing_err)
*/
int slapt_get_pkg_dependencies(const slapt_config_t *global_config,
                               const slapt_vector_t *avail_pkgs,
                               const slapt_vector_t *installed_pkgs,
                               const slapt_pkg_t *pkg,
                               slapt_vector_t *deps,
                               slapt_vector_t *conflict_err,
                               slapt_vector_t *missing_err);
/* return list of package conflicts */
slapt_vector_t *slapt_get_pkg_conflicts(const slapt_vector_t *avail_pkgs, const slapt_vector_t *installed_pkgs, const slapt_pkg_t *pkg);
/* return list of packages required by */
slapt_vector_t *slapt_is_required_by(const slapt_config_t *global_config,
                                     const slapt_vector_t *avail,
                                     const slapt_vector_t *installed_pkgs,
                                     slapt_vector_t *pkgs_to_install,
                                     slapt_vector_t *pkgs_to_remove,
                                     const slapt_pkg_t *pkg);

/*
  clean out old outdated packages in the cache that are no longer available
  in the current source lists (ie are not downloadable)
*/
void slapt_purge_old_cached_pkgs(const slapt_config_t *global_config, const char *dir_name, slapt_vector_t *avail_pkgs);

/*
  download the PACKAGES.TXT and CHECKSUMS.md5 files
  compressed is set if the compressed version was downloaded.
*/
slapt_vector_t *slapt_get_pkg_source_packages(const slapt_config_t *global_config, const char *url, bool *compressed);
slapt_vector_t *slapt_get_pkg_source_patches(const slapt_config_t *global_config, const char *url, bool *compressed);
FILE *slapt_get_pkg_source_checksums(const slapt_config_t *global_config, const char *url, bool *compressed);
bool slapt_get_pkg_source_changelog(const slapt_config_t *global_config, const char *url, bool *compressed);
