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
	FILE *rc;
	rc_config *global_config;
	char *getline_buffer = NULL;
	size_t gb_length = 0;
	ssize_t g_size;
	global_config = calloc( 1,sizeof(rc_config) );

	rc = fopen(file_name,"r");
	if( errno > 1 ){
		fprintf(stderr,"Please create: %s.\n",file_name);
		perror(file_name);
		exit(1);
	}

	while( (g_size = getline(&getline_buffer,&gb_length,rc) ) != EOF ){
		getline_buffer[g_size - 1] = '\0';

		/* check to see if it has our key and value seperated by our token */
		/* and extract them */

		if( getline_buffer[0] == '#' ){
			continue;
		}

		if( strstr(getline_buffer,MIRROR_TOKEN) != NULL ){ /* MIRROR URL */

			if( strlen(getline_buffer) > strlen(MIRROR_TOKEN) ){
				strncpy(global_config->mirror_url,getline_buffer + strlen(MIRROR_TOKEN),(strlen(getline_buffer) - strlen(MIRROR_TOKEN)) );

				/* make sure our url has a trailing '/' */
				if( global_config->mirror_url[strlen(global_config->mirror_url) - 1] != '/' ){
					strcat(global_config->mirror_url,"/");
				}

			}

		} else if( strstr(getline_buffer,WORKINGDIR_TOKEN) != NULL ){ /* WORKING DIR */

			if( strlen(getline_buffer) > strlen(WORKINGDIR_TOKEN) ){
				strncpy(global_config->working_dir,getline_buffer + strlen(WORKINGDIR_TOKEN),(strlen(getline_buffer) - strlen(WORKINGDIR_TOKEN)) );
			}

		}else if( strstr(getline_buffer,EXCLUDE_TOKEN) != NULL ){ /* exclude list */
			global_config->exclude_list = parse_exclude(getline_buffer);
		}/* end if/else if */

	}/* end while */

	if( strcmp(global_config->working_dir,"") == 0 ){
		fprintf(stderr,"WORKINGDIR directive not set within %s.\n",file_name);
		exit(1);
	}
	if( strcmp(global_config->mirror_url,"") == 0 ){
		fprintf(stderr,"MIRROR directive not set within %s.\n",file_name);
		exit(1);
	}

	/* initialize */
	global_config->download_only = 0;
	global_config->dist_upgrade = 0;
	global_config->simulate = 0;
	global_config->ignore_excludes = 0;

	return global_config;
}

void working_dir_init(rc_config *global_config){
	if( access(global_config->working_dir,W_OK) == -1 ){
		if(errno && errno == 2 ){
			if( mkdir(global_config->working_dir,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1 ){
				printf("Failed to build working directory [%s]\n",global_config->working_dir);
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
			fprintf(stderr,"Please update permissions on %s or run with appropriate privileges\n",global_config->working_dir);
			exit(1);
		}else{
			fprintf(stderr,"Please update permissions on %s or run with appropriate privileges\n",global_config->working_dir);
			exit(1);
		}
	}else{
		return;
	}
}

FILE *open_file(const char *file_name,const char *mode){
	FILE *fh;
	if( (fh = fopen(file_name,mode)) == NULL ){
		fprintf(stderr,"Failed to open %s\n",file_name);
		if( errno > 1 ){
			perror(file_name);
		}
		fprintf(stderr,"Perhaps you want to run --update?\n");
		exit(1);
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

void clean_pkg_dir(char *dir_name){
	DIR *tmp;
	struct dirent *file;
	struct stat file_stat;

	if( (tmp = opendir(dir_name)) == NULL ){
		fprintf(stderr,"Failed to opendir %s\n",dir_name);
		return;
	}

	if( chdir(dir_name) == -1 ){
		fprintf(stderr,"Failed to chdir: %s\n",dir_name);
		exit(1);
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
				fprintf(stderr,"Failed to chdir: %s\n",dir_name);
				exit(1);
			}
			continue;
		}
		if( strstr(file->d_name,".tgz") !=NULL ){
#if DEBUG == 1
			printf("unlinking %s\n",file->d_name);
#endif
			unlink(file->d_name);
		}
	}
	closedir(tmp);

}

struct exclude_list *parse_exclude(char *line){
	char *pointer;
	char *buffer;
	int position = 0;
	struct exclude_list *list = calloc( 1, sizeof(struct exclude_list) );
	list->excludes = calloc( 1, sizeof( char **) );
	list->count = 0;

	/* skip ahead past the = */
	line = index(line,'=') + 1;

	while( position < (int) strlen(line) ){
		if( strstr(line + position,",") == NULL ){
			pointer = line + position;
			buffer = calloc( strlen(pointer) + 1, sizeof(char) );
			strncpy(buffer,pointer,strlen(pointer));
			buffer[ strlen(pointer) ] = '\0';
			list->excludes[ list->count ] = calloc( strlen(buffer) , sizeof(char) );
			strncpy(list->excludes[ list->count ], buffer, strlen(buffer) );
			free(buffer);
			list->count++;
			break;
		}else{
			if( line[position] == ',' ){
				position++;
				continue;
			}else{
				pointer = index(line + position,',');
				buffer = calloc( strlen(line + position) - strlen(pointer) + 1, sizeof(char) );
				strncpy(buffer,line + position,strlen(line + position) - strlen(pointer) );
				buffer[ strlen(line + position) - strlen(pointer) ] = '\0';
				list->excludes[ list->count ] = calloc( strlen(buffer) , sizeof(char) );
				strncpy(list->excludes[ list->count ], buffer, strlen(buffer) );
				list->count++;
				list->excludes = realloc(list->excludes, list->count + 1 * sizeof(char **) );
				position += (strlen(line + position) - strlen(pointer) );
				free(buffer);
			}
			continue;
		}
	}
	
	return list;
}

void free_excludes(struct exclude_list *list){
	int i;
	for(i=0;i < list->count;i++){
		free(list->excludes[i]);
	}
	free(list->excludes);
	free(list);
}

int is_excluded(rc_config *global_config,char *pkg_name){
	int i;

	if( global_config->ignore_excludes == 1 )
		return 0;

	/* maybe EXCLUDE= isn't defined in our rc? */
	if( global_config->exclude_lsit == NULL )
		return 0;

	for(i = 0; i < global_config->exclude_list->count;i++){
		/*
		 * this is kludgy... global_config->exclude_list->excludes[i] is 1 char longer
		 * than pkg_name
		*/
		if( (strncmp(global_config->exclude_list->excludes[i],pkg_name,strlen(pkg_name)) == 0)
			&& (strlen(global_config->exclude_list->excludes[i]) - strlen(pkg_name) < 2) ){
			printf("excluding %s\n",pkg_name);
			return 1;
		}
	}

	return 0;
}
