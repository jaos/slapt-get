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
/* parse the exclude list */
static struct exclude_list *parse_exclude(char *line);


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
	/* initialize */
	global_config->sources.count = 0;
	global_config->download_only = 0;
	global_config->simulate = 0;
	global_config->ignore_excludes = 0;
	global_config->no_md5_check = 0;
	global_config->dist_upgrade = 0;
	global_config->no_dep = 0;
	global_config->disable_dep_check = 0;
	global_config->print_uris = 0;
	global_config->dl_stats = 0;
	global_config->no_prompt = 0;
	global_config->re_install = 0;
	global_config->exclude_list = NULL;
	global_config->working_dir[0] = '\0';

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

	return global_config;
}

void working_dir_init(const rc_config *global_config){
	DIR *working_dir;

	if( (working_dir = opendir(global_config->working_dir)) == NULL ){
		if( mkdir(global_config->working_dir,
				S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1
		){
			printf(_("Failed to build working directory [%s]\n"),global_config->working_dir);
			if( errno ) perror(global_config->working_dir);
			exit(1);
		}
	}

	if( access(global_config->working_dir,W_OK) == -1 ){
		if( errno ) perror(global_config->working_dir);
		fprintf(stderr,_("Please update permissions on %s or run with appropriate privileges\n"),
			global_config->working_dir);
		exit(1);
	}

	return;
}

void clean_pkg_dir(const char *dir_name){
	DIR *dir;
	struct dirent *file;
	struct stat file_stat;

	if( (dir = opendir(dir_name)) == NULL ){
		fprintf(stderr,_("Failed to opendir %s\n"),dir_name);
		return;
	}

	if( chdir(dir_name) == -1 ){
		fprintf(stderr,_("Failed to chdir: %s\n"),dir_name);
		return;
	}

	while( (file = readdir(dir)) ){

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
	closedir(dir);

}

void free_rc_config(rc_config *global_config){
	int i;
	
	for(i = 0; i < global_config->exclude_list->count; i++){
		free(global_config->exclude_list->excludes[i]);
	}

	free(global_config->exclude_list);
	free(global_config);

}

static struct exclude_list *parse_exclude(char *line){
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
	list->excludes = malloc( sizeof *list->excludes );
	list->count = 0;

	/* skip ahead past the = */
	line = strchr(line,'=') + 1;


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


