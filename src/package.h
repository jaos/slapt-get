/*
 * Copyright (C) 2004 Jason Woodward <woodwardj at jaos dot org>
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

#define MAX_PKG_ENTRIES 3000
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

struct _pkg_info {
	char name[NAME_LEN];
	char version[VERSION_LEN];
	char mirror[MIRROR_LEN];
	char location[LOCATION_LEN];
	int size_c;
	int size_u;
	char description[DESCRIPTION_LEN];
	char required[REQUIRED_LEN];
	char conflicts[CONFLICTS_LEN];
	char md5[MD5_STR_LEN];
};
typedef struct _pkg_info pkg_info_t;
struct pkg_list {
	pkg_info_t **pkgs;
	int pkg_count;
};
struct _pkg_upgrade {
	pkg_info_t *installed;
	pkg_info_t *upgrade;
};
typedef struct _pkg_upgrade pkg_upgrade_t;
struct pkg_upgrade_list {
	pkg_upgrade_t **pkgs;
	int pkg_count;
};

struct pkg_version_parts {
	char parts[10][10];
	int count;
};



pkg_info_t *init_pkg(void);
struct pkg_list *init_pkg_list(void);
void add_pkg_to_pkg_list(struct pkg_list *list,pkg_info_t *pkg);
/* free memory allocated for pkg_list struct */
void free_pkg_list(struct pkg_list *);

/* do a head request on the mirror data to find out if it's new */
int head_mirror_data(const char *wurl,const char *file);
/* update the local package cache */
void update_pkg_cache(const rc_config *global_config);
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
/* generate the package file name */
char *gen_pkg_file_name(const rc_config *global_config,pkg_info_t *pkg);
/* generate the download url for a package */
char *gen_pkg_url(pkg_info_t *pkg);
/* exclude pkg based on pkg name */
int is_excluded(const rc_config *,pkg_info_t *);
/* lookup md5sum of file */
void get_md5sum(pkg_info_t *pkg,FILE *checksum_file);
/* package is already downloaded and cached, md5sum if applicable is ok */
int verify_downloaded_pkg(const rc_config *global_config,pkg_info_t *pkg);
/* find out the pkg file size (post download) */
size_t get_pkg_file_size(const rc_config *global_config,pkg_info_t *pkg);

/* compare package versions (returns just like strcmp) */
int cmp_pkg_versions(char *a, char *b);
/* analyze the pkg version hunk by hunk */
void break_down_pkg_version(struct pkg_version_parts *pvp,const char *version);

/* resolve dependencies */
struct pkg_list *lookup_pkg_dependencies(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg);
/* lookup package conflicts */
struct pkg_list *lookup_pkg_conflicts(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg);
/* parse the meta lines */
pkg_info_t *parse_meta_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,char *dep_entry);
/* return list of packages required by */
struct pkg_list *is_required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg);
/* parse the exclude list */
struct exclude_list *parse_exclude(char *line);

