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

#define SOURCE_TOKEN "SOURCE="
#define WORKINGDIR_TOKEN "WORKINGDIR="
#define WORKINGDIR_TOKEN_LEN 256
#define EXCLUDE_TOKEN "EXCLUDE="
#define MAX_SOURCES 20
#define MAX_SOURCE_URL_LEN 400

struct exclude_list {
	char **excludes;
	int count;
};

struct source_list {
	char url[MAX_SOURCES][MAX_SOURCE_URL_LEN];
	int count;
};

typedef struct {
	struct source_list sources;
	char working_dir[WORKINGDIR_TOKEN_LEN];
	int download_only;
	int dist_upgrade;
	int simulate;
	int no_prompt;
	int re_install;
	struct exclude_list *exclude_list;
	int ignore_excludes;
	int no_md5_check;
	int ignore_dep;
	int disable_dep_check;
	int print_uris;
	int dl_stats;
} rc_config;

rc_config *read_rc_config(const char *file_name);
void working_dir_init(const rc_config *global_config);
void free_rc_config(rc_config *global_config);
