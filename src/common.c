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

void gen_md5_sum_of_file(FILE *f,char *result_sum){
	EVP_MD_CTX mdctx;
	const EVP_MD *md;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	int md_len = 0, i;
	ssize_t getline_read;
	size_t getline_size;
	char *result_sum_tmp = NULL;
	char *getline_buffer = NULL;

	md = EVP_md5();
 
	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, md, NULL);

	rewind(f);

	while( (getline_read = getline(&getline_buffer, &getline_size, f)) != EOF )
		EVP_DigestUpdate(&mdctx, getline_buffer, getline_read);

	free(getline_buffer);

	EVP_DigestFinal_ex(&mdctx, md_value, (unsigned int*)&md_len);
	EVP_MD_CTX_cleanup(&mdctx);

	result_sum[0] = '\0';
	
	for(i = 0; i < md_len; i++){
		char *p = malloc( sizeof *p * 3 );

		if( snprintf(p,3,"%02x",md_value[i]) > 0 ){

			if( (result_sum_tmp = strncat(result_sum,p,3)) != NULL )
				result_sum = result_sum_tmp;

		}

		free(p);
	}

}

/* recursively create dirs */
void create_dir_structure(const char *dir_name){
	char *cwd = NULL;
	int position = 0;

	cwd = getcwd(NULL,0);
	if( cwd == NULL ){
		fprintf(stderr,_("Failed to get cwd\n"));
		return;
	}else{
		#if DEBUG == 1
		fprintf(stderr,_("\tCurrent working directory: %s\n"),cwd);
		#endif
	}

	while( position < (int) strlen(dir_name) ){

		char *pointer = NULL;
		char *dir_name_buffer = NULL;

		/* if no more directory delim, then this must be last dir */
		if( strstr(dir_name + position,"/" ) == NULL ){

			dir_name_buffer = calloc( strlen(dir_name + position) + 1 , sizeof *dir_name_buffer );
			strncpy(dir_name_buffer,dir_name + position,strlen(dir_name + position));
			dir_name_buffer[ strlen(dir_name + position) ] = '\0';

			if( strcmp(dir_name_buffer,".") != 0 ){
				if( (mkdir(dir_name_buffer,0755)) == -1){
					#if DEBUG == 1
					fprintf(stderr,_("Failed to mkdir: %s\n"),dir_name_buffer);
					#endif
				}else{
					#if DEBUG == 1
					fprintf(stderr,_("\tCreated directory: %s\n"),dir_name_buffer);
					#endif
				}
				if( (chdir(dir_name_buffer)) == -1 ){
					fprintf(stderr,_("Failed to chdir to %s\n"),dir_name_buffer);
					return;
				}else{
					#if DEBUG == 1
					fprintf(stderr,_("\tchdir into %s\n"),dir_name_buffer);
					#endif
				}
			}/* don't create . */

			free(dir_name_buffer);
			break;
		}else{
			if( dir_name[position] == '/' ){
				/* move on ahead */
				++position;
			}else{

				/* figure our dir name and mk it */
				pointer = strchr(dir_name + position,'/');
				dir_name_buffer = calloc(
					strlen(dir_name + position) - strlen(pointer) + 1 , sizeof *dir_name_buffer
				);
				strncpy(dir_name_buffer,dir_name + position, strlen(dir_name + position) - strlen(pointer));
				dir_name_buffer[ (strlen(dir_name + position) - strlen(pointer)) ] = '\0';

				if( strcmp(dir_name_buffer,".") != 0 ){
					if( (mkdir(dir_name_buffer,0755)) == -1 ){
						#if DEBUG == 1
						fprintf(stderr,_("Failed to mkdir: %s\n"),dir_name_buffer);
						#endif
					}else{
						#if DEBUG == 1
						fprintf(stderr,_("\tCreated directory: %s\n"),dir_name_buffer);
						#endif
					}
					if( (chdir(dir_name_buffer)) == -1 ){
						fprintf(stderr,_("Failed to chdir to %s\n"),dir_name_buffer);
						return;
					}else{
						#if DEBUG == 1
						fprintf(stderr,_("\tchdir into %s\n"),dir_name_buffer);
						#endif
					}
				} /* don't create . */

				free(dir_name_buffer);
				position += (strlen(dir_name + position) - strlen(pointer));
			}
		}
	}/* end while */

	if( (chdir(cwd)) == -1 ){
		fprintf(stderr,_("Failed to chdir to %s\n"),cwd);
		return;
	}

	free(cwd);
}

