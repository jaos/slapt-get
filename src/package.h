/*
 * Copyright (C) 2003 Jason Woodward <woodwardj at jaos dot org>
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

/* parse the PACKAGES.TXT file */
struct pkg_list *parse_packages_txt(FILE *);
struct pkg_list *get_available_pkgs(void);

/* retrieve list of installed pkgs */
struct pkg_list *get_installed_pkgs(void);

/*
	This used to be used to parse the file list for updates,
	until I realized patches/PACKAGES.TXT existed
	Legacy, might be useful one day.
*/
struct pkg_list *parse_file_list(FILE *);

/* generate a short description */
char *gen_short_pkg_description(pkg_info_t *);

/* retrieve the newest pkg from pkg_info_t list */
pkg_info_t *get_newest_pkg(struct pkg_list *,const char *);

/* get the exact package */
pkg_info_t *get_exact_pkg(struct pkg_list *list,const char *name,const char *version);

/* lookup package by details */
pkg_info_t *get_pkg_by_details(struct pkg_list *list,char *name,char *version,char *location);

/* install pkg */
int install_pkg(const rc_config *,pkg_info_t *);

/* upgrade pkg */
int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,pkg_info_t *pkg);

/* remove pkg */
int remove_pkg(const rc_config *,pkg_info_t *);

/* free memory allocated for pkg_list struct */
void free_pkg_list(struct pkg_list *);

/* exclude pkg based on pkg name */
int is_excluded(const rc_config *,pkg_info_t *);

/* lookup md5sum of file */
void get_md5sum(pkg_info_t *pkg,FILE *checksum_file);

/* compare package versions (returns just like strcmp) */
int cmp_pkg_versions(char *a, char *b);

/* analyze the pkg version hunk by hunk */
void break_down_pkg_version(struct pkg_version_parts *pvp,const char *version);

/* write pkg data to disk */
void write_pkg_data(const char *source_url,FILE *d_file,struct pkg_list *pkgs);

/* search package list with pattern */
void search_pkg_list(struct pkg_list *available,struct pkg_list *matches,const char *pattern);

/* resolve dependencies */
struct pkg_list *lookup_pkg_dependencies(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg);

/* lookup package conflicts */
struct pkg_list *lookup_pkg_conflicts(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg);

/* parse the meta lines */
pkg_info_t *parse_meta_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg,char *dep_entry);

/* return list of packages required by */
struct pkg_list *is_required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg);

/* update the local package cache */
void update_pkg_cache(const rc_config *global_config);

