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


#define MAX_REGEX_PARTS 10
#define SLACK_BASE_SET_REGEX "^./slackware/a$"


typedef struct {
	regex_t regex;
	size_t nmatch;
	regmatch_t pmatch[MAX_REGEX_PARTS];
	int reg_return;
} sg_regex;


FILE *open_file(const char *file_name,const char *mode);
void init_regex(sg_regex *regex_t, const char *regex_string);
void execute_regex(sg_regex *regex_t,const char *string);
void free_regex(sg_regex *regex_t);
void create_dir_structure(const char *dir_name);
/* generate an md5sum of filehandle */
void gen_md5_sum_of_file(FILE *f,char *result_sum);

/* Ask the user to answer yes or no.
 * return 1 on yes, 0 on no, else -1.
 */
int ask_yes_no(const char *format, ...);
char *str_replace_chr(const char *string,const char find, const char replace);
