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

#define MIRROR_TOKEN "MIRROR="
#define WORKINGDIR_TOKEN "WORKINGDIR="
#define EXCLUDE_TOKEN "EXCLUDE="

struct exclude_list {
	char excludes[100][50];
	int count;
};

struct _configuration {
	char mirror_url[256];
	char working_dir[256];
	int download_only;
	int dist_upgrade;
	int simulate;
	int no_prompt;
	int re_install;
	struct exclude_list *exclude_list;
	int ignore_excludes;
};
typedef struct _configuration rc_config;

rc_config *read_rc_config(const char *);
void working_dir_init(const rc_config *);
FILE *open_file(const char *,const char *);
char spinner(void);
void clean_pkg_dir(const char *);
struct exclude_list *parse_exclude(char *);
void create_dir_structure(const char *);
void gen_md5_sum_of_file(FILE *,char *);
