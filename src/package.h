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

#define MAX_PKG_ENTRIES 1300
#define PKG_PARSE_REGEX "([a-zA-Z0-9._\\-\\/]+/)([a-zA-Z0-9\\+_\\-]+)-([a-zA-Z0-9._\\-]+).tgz$"
#define PKG_NAME_PATTERN "^PACKAGE NAME:[ ]+([a-zA-Z0-9\\+_\\-]+)-([a-zA-Z0-9._\\-]+).tgz$"
#define PKG_LOCATION_PATTERN "^PACKAGE LOCATION:[ ]+(.*)$"
#define PKG_SIZEC_PATTERN "^PACKAGE SIZE [(]+compressed[)]+:[ ]+([0-9]+) K$"
#define PKG_SIZEU_PATTERN "^PACKAGE SIZE [(]+uncompressed[)]+:[ ]+([0-9]+) K$"
#define PKG_LOG_DIR "/var/log/packages"
#define PKG_LOG_PATTERN "^([a-zA-Z0-9\\+_\\-]+)-([a-zA-Z0-9._\\-]+)$"
#define MD5SUM_REGEX "([a-zA-Z0-9]+)[ ]+([a-zA-Z0-9._\\-\\/]+/)([a-zA-Z0-9\\+_\\-]+)-([a-zA-Z0-9._\\-]+).tgz$"

/*
 * VARIABLE DEFINITIONS
 */
struct _pkg_info {
	char name[50];
	char version[50];
	char location[50];
	int size_c;
	int size_u;
	char description[1024];
};
typedef struct _pkg_info pkg_info_t;
struct pkg_list {
	pkg_info_t **pkgs;
	int pkg_count;
};

/*
 * FUNCTION DEFINITIONS
 */
/* parse the PACKAGES.TXT file */
struct pkg_list *get_available_pkgs(void);
/* retrieve list of installed pkgs */
struct pkg_list *get_installed_pkgs(void);
/* return list of update pkgs */
struct pkg_list *get_update_pkgs(void);
/* generate a short description */
char *gen_short_pkg_description(pkg_info_t *);
/* retrieve the newest pkg from pkg_info_t list */
pkg_info_t *get_newest_pkg(pkg_info_t **,const char *,int);
/* install pkg */
int install_pkg(const rc_config *,pkg_info_t *);
/* upgrade pkg */
int upgrade_pkg(const rc_config *,pkg_info_t *,pkg_info_t *);
/* remove pkg */
int remove_pkg(pkg_info_t *);
/* free memory allocated for pkg_list struct */
void free_pkg_list(struct pkg_list *);
/* exclude pkg based on pkg name */
int is_excluded(const rc_config *,const char *);
/* lookup md5sum of file */
void get_md5sum(const rc_config *,pkg_info_t *,char *);
/* compare package versions */
int cmp_pkg_versions(char *, char *);
/* analyze the pkg version hunk by hunk */
int break_down_pkg_version(int *,char *);
/* get available, installed, and update pkgs all in one */
struct pkg_list *get_available_and_update_pkgs(void);

