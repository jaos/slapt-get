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

#include "main.h"


rc_config *read_rc_config(const char *file_name){
	FILE *rc = NULL;
	rc_config *global_config;
	char *getline_buffer = NULL;
	size_t gb_length = 0;
	ssize_t g_size;

	global_config = malloc( sizeof *global_config );
	if( global_config == NULL ){
		fprintf(stderr,_("Failed to malloc %s\n"),"global_config");
		if( errno ){
			perror("global_config malloc");
		}
	}

	global_config->sources.count = 0;

	rc = open_file(file_name,"r");
	if( rc == NULL ) exit(1);

	while( (g_size = getline(&getline_buffer,&gb_length,rc) ) != EOF ){
		getline_buffer[g_size - 1] = '\0';

		/* check to see if it has our key and value seperated by our token */
		/* and extract them */

		if( getline_buffer[0] == '#' ){
			continue;
		}

		if( strstr(getline_buffer,SOURCE_TOKEN) != NULL ){ /* SOURCE URL */

			/* make sure we stay within the limits of MAX_SOURCES */
			if( global_config->sources.count == MAX_SOURCES ){
				fprintf(stderr,_("Maximum number of sources (%d) exceeded.\n"),MAX_SOURCES);
				continue;
			}

			if( strlen(getline_buffer) > strlen(SOURCE_TOKEN) ){
				if( (strlen(getline_buffer) - strlen(SOURCE_TOKEN)) >= MAX_SOURCE_URL_LEN ){
					fprintf(stderr,_("Maximum length of source (%d) exceeded.\n"),MAX_SOURCE_URL_LEN);
					continue;
				}
				strncpy(
					global_config->sources.url[global_config->sources.count],
					getline_buffer + strlen(SOURCE_TOKEN),
					(strlen(getline_buffer) - strlen(SOURCE_TOKEN))
				);
				global_config->sources.url[global_config->sources.count][
					(strlen(getline_buffer) - strlen(SOURCE_TOKEN))
				] = '\0';

				/* make sure our url has a trailing '/' */
				if( global_config->sources.url[global_config->sources.count]
					[
						strlen(global_config->sources.url[global_config->sources.count]) - 1
					] != '/'
				){
					strcat(global_config->sources.url[global_config->sources.count],"/");
				}
				++global_config->sources.count;

			}

		} else if( strstr(getline_buffer,WORKINGDIR_TOKEN) != NULL ){ /* WORKING DIR */

			if( strlen(getline_buffer) > strlen(WORKINGDIR_TOKEN) ){
				strncpy(
					global_config->working_dir,
					getline_buffer + strlen(WORKINGDIR_TOKEN),
					(strlen(getline_buffer) - strlen(WORKINGDIR_TOKEN))
				);
				global_config->working_dir[
					(strlen(getline_buffer) - strlen(WORKINGDIR_TOKEN))
				] = '\0';
			}

		}else if( strstr(getline_buffer,EXCLUDE_TOKEN) != NULL ){ /* exclude list */
			global_config->exclude_list = parse_exclude(getline_buffer);
		}/* end if/else if */

	}/* end while */
	if( getline_buffer ) free(getline_buffer);

	if( strcmp(global_config->working_dir,"") == 0 ){
		fprintf(stderr,_("WORKINGDIR directive not set within %s.\n"),file_name);
		return NULL;
	}
	if( global_config->exclude_list == NULL ){
		/* at least initialize */
		global_config->exclude_list = malloc( sizeof *global_config->exclude_list );
		global_config->exclude_list->count = 0;
	}
	if( global_config->sources.count == 0 ){
		fprintf(stderr,_("SOURCE directive not set within %s.\n"),file_name);
		return NULL;
	}

	/* initialize */
	global_config->download_only = 0;
	global_config->simulate = 0;
	global_config->ignore_excludes = 0;
	global_config->no_md5_check = 0;
	global_config->dist_upgrade = 0;
	global_config->no_dep = 0;
	global_config->disable_dep_check = 0;
	global_config->print_uris = 0;

	return global_config;
}

void working_dir_init(const rc_config *global_config){
	if( access(global_config->working_dir,W_OK) == -1 ){
		if(errno && errno == 2 ){
			if( mkdir(global_config->working_dir,
					S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1
			){
				printf(_("Failed to build working directory [%s]\n"),global_config->working_dir);
				if( errno ){
					perror(global_config->working_dir);
					exit(1);
				}
			}else{
				/* we should now have a global_config->working_dir */
				return;
			}
		}else if( errno && errno == 13 ){
			perror(global_config->working_dir);
			fprintf(stderr,_("Please update permissions on %s or run with appropriate privileges\n"),
				global_config->working_dir);
			exit(1);
		}else{
			fprintf(stderr,_("Please update permissions on %s or run with appropriate privileges\n"),
				global_config->working_dir);
			exit(1);
		}
	}else{
		return;
	}
}

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

char spinner(void){
	static int spinner_index = 0;
	static char *spinner_parts = "\\|/-";

	if( spinner_index > 3 ){
		spinner_index = 0;
		return spinner_parts[spinner_index];
	}else{
		return spinner_parts[spinner_index++];
	}
}

void clean_pkg_dir(const char *dir_name){
	DIR *tmp;
	struct dirent *file;
	struct stat file_stat;

	if( (tmp = opendir(dir_name)) == NULL ){
		fprintf(stderr,_("Failed to opendir %s\n"),dir_name);
		return;
	}

	if( chdir(dir_name) == -1 ){
		fprintf(stderr,_("Failed to chdir: %s\n"),dir_name);
		return;
	}

	while( (file = readdir(tmp)) ){

		/* make sure we don't have . or .. */
		if( (strcmp(file->d_name,"..")) == 0 || (strcmp(file->d_name,".") == 0) )
			continue;

		/* setup file_stat struct */
		if( (stat(file->d_name,&file_stat)) == -1)
			continue;

		/* if its a directory, recurse */
		if( S_ISDIR(file_stat.st_mode) ){
			clean_pkg_dir(file->d_name);
			if( (chdir("..")) == -1 ){
				fprintf(stderr,_("Failed to chdir: %s\n"),dir_name);
				return;
			}
			continue;
		}
		if( strstr(file->d_name,".tgz") !=NULL ){
			#if DEBUG == 1
			printf(_("unlinking %s\n"),file->d_name);
			#endif
			unlink(file->d_name);
		}
	}
	closedir(tmp);

}

struct exclude_list *parse_exclude(char *line){
	char *pointer = NULL;
	char *buffer = NULL;
	int position = 0;
	char **realloc_tmp;
	struct exclude_list *list;
	list = malloc( sizeof *list );
	if( list == NULL ){
		fprintf(stderr,_("Failed to malloc %s\n"),"list");
		if( errno ){
			perror("malloc");
		}
		exit(1);
	}
	list->count = 0;

	/* skip ahead past the = */
	line = strchr(line,'=') + 1;

	list->excludes = malloc( sizeof *list->excludes );

	while( position < (int) strlen(line) ){
		if( strstr(line + position,",") == NULL ){

			pointer = line + position;

			realloc_tmp = realloc( list->excludes, sizeof *list->excludes * (list->count + 1) );
			if( realloc_tmp != NULL ){
				list->excludes = realloc_tmp;
				list->excludes[ list->count ] = calloc( strlen(pointer) + 1, sizeof *list->excludes[list->count] );
				strncpy(list->excludes[ list->count ], pointer, strlen(pointer) );
				list->excludes[ list->count ][strlen(pointer)] =  '\0';
				++list->count;
			}

			break;
		}else{

			if( line[position] == ',' ){
				++position;
				continue;
			}else{

				pointer = strchr(line + position,',');
				buffer = calloc( strlen(line + position) - strlen(pointer) + 1, sizeof *buffer );
				strncpy(buffer,line + position,strlen(line + position) - strlen(pointer) );
				buffer[ strlen(line + position) - strlen(pointer) ] = '\0';

				realloc_tmp = realloc( list->excludes, sizeof *list->excludes * (list->count + 1) );
				if( realloc_tmp != NULL ){
					list->excludes = realloc_tmp;
					list->excludes[ list->count ] = buffer;
					list->excludes[ list->count ][strlen(buffer)] =  '\0';
				}

				++list->count;
				position += (strlen(line + position) - strlen(pointer) );
			}
			continue;
		}
	}
	
	return list;
}

/* recursively create dirs */
void create_dir_structure(const char *dir_name){
	char *pointer = NULL;
	char *cwd = NULL;
	int position = 0;
	char *dir_name_buffer = NULL;

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

void gen_md5_sum_of_file(FILE *f,char *result_sum){
	EVP_MD_CTX mdctx;
	const EVP_MD *md;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	int md_len, i;
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
		char *p = malloc( 3 );

		if( snprintf(p,3,"%02x",md_value[i]) > 0 ){

			if( (result_sum_tmp = strncat(result_sum,p,3)) != NULL )
				result_sum = result_sum_tmp;

		}

		free(p);
	}

}

void free_rc_config(rc_config *global_config){
	int i;
	
	for(i = 0; i < global_config->exclude_list->count; i++){
		free(global_config->exclude_list->excludes[i]);
	}

	free(global_config->exclude_list);
	free(global_config);

}

