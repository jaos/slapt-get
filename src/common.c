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

#include "main.h"


FILE *open_file(const char *file_name,const char *mode){
	FILE *fh = NULL;
	if( (fh = fopen(file_name,mode)) == NULL ){
		fprintf(stderr,_("Failed to open %s\n"),file_name);
		if( errno ){
			perror(file_name);
		}
		return NULL;
	}
	return fh;
}

/* initialize regex structure and compilie the regular expression */
void init_regex(sg_regex *regex_t, const char *regex_string){

	regex_t->nmatch = MAX_REGEX_PARTS;

	/* compile our regex */
	regex_t->reg_return = regcomp(&regex_t->regex, regex_string, REG_EXTENDED|REG_NEWLINE);
	if( regex_t->reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, _("Failed to compile regex\n"));

		if( (regerror_size = regerror(regex_t->reg_return, &regex_t->regex,errbuf,errbuf_size)) ){
			printf(_("Regex Error: %s\n"),errbuf);
		}
		exit(1);
	}

}

/*
	execute the regular expression and set the return code
	in the passed in structure
 */
void execute_regex(sg_regex *regex_t,const char *string){
	regex_t->reg_return = regexec(&regex_t->regex, string, regex_t->nmatch,regex_t->pmatch,0);
}

void free_regex(sg_regex *regex_t){
	regfree(&regex_t->regex);
}

