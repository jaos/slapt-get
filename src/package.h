/*
 * Copyright (C) 2003,2004,2005 Jason Woodward <woodwardj at jaos dot org>
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
#define NAME_LEN 50
#define VERSION_LEN 50
#define MIRROR_LEN 200
#define LOCATION_LEN 50
#define DESCRIPTION_LEN 1024
#define REQUIRED_LEN 1024
#define CONFLICTS_LEN 1024
#define SUGGESTS_LEN 1024
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
	char name[NAME_LEN];
	char version[VERSION_LEN];
	char mirror[MIRROR_LEN];
	char location[LOCATION_LEN];
	unsigned int size_c;
	unsigned int size_u;
	char description[DESCRIPTION_LEN];
	char required[REQUIRED_LEN];
	char conflicts[CONFLICTS_LEN];
	char suggests[SUGGESTS_LEN];
	char md5[MD5_STR_LEN];
} pkg_info_t;

struct pkg_list {
	pkg_info_t **pkgs;
	unsigned int pkg_count;
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



__inline pkg_info_t *init_pkg(void);
struct pkg_list *init_pkg_list(void);
void add_pkg_to_pkg_list(struct pkg_list *list,pkg_info_t *pkg);
/* free memory allocated for pkg_list struct */
void free_pkg_list(struct pkg_list *);

/* update the local package cache */
int update_pkg_cache(const rc_config *global_config);
/* do a head request on the mirror data to find out if it's new */
char *head_mirror_data(const char *wurl,const char *file);
/* clear head cache storage file */
void clear_head_cache(const char *cache_filename);
/* cache the head request */
void write_head_cache(const char *cache, const char *cache_filename);
/* read the cached head request */
char *read_head_cache(const char *cache_filename);
/* parse the PACKAGES.TXT file */
struct pkg_list *parse_packages_txt(FILE *);
struct pkg_list *get_available_pkgs(void);
/* retrieve list of installed pkgs */
struct pkg_list *get_installed_pkgs(void);
/* write pkg data to disk */
void write_pkg_data(const char *source_url,FILE *d_file,struct pkg_list *pkgs);


/* retrieve the newest pkg from pkg_info_t list */
pkg_info_t *get_newest_pkg(struct pkg_list *,const char *);
/* get the exact package */
pkg_info_t *get_exact_pkg(struct pkg_list *list,const char *name,const char *version);
/* lookup package by details */
pkg_info_t *get_pkg_by_details(struct pkg_list *list,char *name,char *version,char *location);
/* search package list with pattern */
struct pkg_list *search_pkg_list(struct pkg_list *available,const char *pattern);


/* install pkg */
int install_pkg(const rc_config *,pkg_info_t *);
/* upgrade pkg */
int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,pkg_info_t *pkg);
/* remove pkg */
int remove_pkg(const rc_config *,pkg_info_t *);


/* generate a short description */
char *gen_short_pkg_description(pkg_info_t *);
/* generate the filename from the url */
char *gen_filename_from_url(const char *url,const char *file);
/* generate the package file name */
char *gen_pkg_file_name(const rc_config *global_config,pkg_info_t *pkg);
/* generate the head cache filename */
char *gen_head_cache_filename(const char *filename_from_url);
/* generate the download url for a package */
char *gen_pkg_url(pkg_info_t *pkg);
/* exclude pkg based on pkg name */
int is_excluded(const rc_config *,pkg_info_t *);
/* package is already downloaded and cached, md5sum if applicable is ok */
int verify_downloaded_pkg(const rc_config *global_config,pkg_info_t *pkg);
/* lookup md5sum of file */
void get_md5sum(pkg_info_t *pkg,FILE *checksum_file);
/* find out the pkg file size (post download) */
size_t get_pkg_file_size(const rc_config *global_config,pkg_info_t *pkg);

/* compare package versions (returns just like strcmp) */
int cmp_pkg_versions(char *a, char *b);

/* resolve dependencies */
int get_pkg_dependencies(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg,struct pkg_list *deps);
/* lookup package conflicts */
struct pkg_list *get_pkg_conflicts(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg);
/* return list of packages required by */
struct pkg_list *is_required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg);

/* empty packages from cache dir */
void clean_pkg_dir(const char *dir_name);
/* clean out old outdated packages in the cache */
void purge_old_cached_pkgs(const rc_config *global_config,char *dir_name, struct pkg_list *avail_pkgs);

