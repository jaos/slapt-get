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
	BOOL_T download_only;
	BOOL_T dist_upgrade;
	BOOL_T simulate;
	BOOL_T no_prompt;
	BOOL_T re_install;
	struct exclude_list *exclude_list;
	BOOL_T ignore_excludes;
	BOOL_T no_md5_check;
	BOOL_T ignore_dep;
	BOOL_T disable_dep_check;
	BOOL_T print_uris;
	BOOL_T dl_stats;
	BOOL_T remove_obsolete;
	int(*progress_cb)(void *,double,double,double,double);

} rc_config;

rc_config *read_rc_config(const char *file_name);
void working_dir_init(const rc_config *global_config);
void free_rc_config(rc_config *global_config);
void add_exclude(struct exclude_list *list,const char *e);
void add_source(struct source_list *list,const char *s);
