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

#include <main.h>

/* parse the PACKAGES.TXT file */
struct pkg_list *parse_pkg_list(void){
	regex_t name_regex, location_regex, size_c_regex, size_u_regex;
	ssize_t bytes_read;
	int regexec_return;
	FILE *pkg_list_fh;
	size_t nmatch = 10,getline_len = 0;
	regmatch_t pmatch[10];
	char *getline_buffer = NULL;
	char *size_c = NULL;
	char *size_u = NULL;
	struct pkg_list *list = calloc( 1 , sizeof(struct pkg_list) );


	/* open pkg list */
	pkg_list_fh = open_file(PKG_LIST_L,"r");

	/* compile our regexen */
	regcomp(&name_regex,PKG_NAME_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&location_regex,PKG_LOCATION_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_c_regex,PKG_SIZEC_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_u_regex,PKG_SIZEU_PATTERN, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = calloc(1 , sizeof(pkg_info *) );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to calloc pkgs\n");
		exit(1);
	}
	while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh) ) != EOF ){
		getline_buffer[bytes_read - 1] = '\0';

		/* pull out package data */
		if( strstr(getline_buffer,"PACKAGE NAME") == NULL ){
			continue;
		}

		regexec_return = regexec( &name_regex, getline_buffer, nmatch, pmatch, 0);
		if( regexec_return == 0 ){
			list->pkgs[list->pkg_count] = calloc( 1 , sizeof(pkg_info) );
			if( list->pkgs[list->pkg_count] == NULL ){
				fprintf(stderr,"Failed to calloc list->pkgs[list->pkg_count]\n");
				exit(1);
			}

			/* pkg name base */
			memcpy(list->pkgs[list->pkg_count]->name,
				getline_buffer + pmatch[1].rm_so,
				pmatch[1].rm_eo - pmatch[1].rm_so
			);
			list->pkgs[list->pkg_count]->name[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
			/* pkg version */
			memcpy(list->pkgs[list->pkg_count]->version,
				getline_buffer + pmatch[2].rm_so,
				pmatch[2].rm_eo - pmatch[2].rm_so
			);
			list->pkgs[list->pkg_count]->version[pmatch[2].rm_eo - pmatch[2].rm_so] = '\0';

		}else{
			fprintf(stderr,"regex failed on [%s]\n",getline_buffer);
			continue;
		}

		/* location */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) ){
			if( regexec( &location_regex, getline_buffer, nmatch, pmatch, 0) == 0 ){
				memcpy(list->pkgs[list->pkg_count]->location,
					getline_buffer + pmatch[1].rm_so,
					pmatch[1].rm_eo - pmatch[1].rm_so
				);
				list->pkgs[list->pkg_count]->location[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
			}else{
				fprintf(stderr,"regexec failed to parse location\n");
				continue;
			}
		}else{
			fprintf(stderr,"getline reached EOF attempting to read location\n");
			continue;
		}

		/* size_c */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)){
			if( (regexec_return = regexec( &size_c_regex, getline_buffer, nmatch, pmatch, 0)) == 0 ){
				size_c = calloc( (pmatch[1].rm_eo - pmatch[1].rm_so) + 1 , sizeof(char) );
				if( size_c == NULL ){
					fprintf(stderr,"Failed to calloc size_c\n");
					exit(1);
				}
				memcpy(size_c,getline_buffer + pmatch[1].rm_so,(pmatch[1].rm_eo - pmatch[1].rm_so));
				list->pkgs[list->pkg_count]->size_c = strtol(size_c, (char **)NULL, 10);
				free(size_c);
			}else{
				fprintf(stderr,"regexec failed to parse size_c\n");
				continue;
			}
		}else{
			fprintf(stderr,"getline reached EOF attempting to read size_c\n");
			continue;
		}

		/* size_u */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)){
			if( regexec( &size_u_regex, getline_buffer, nmatch, pmatch, 0) == 0 ){
				size_u = calloc( (pmatch[1].rm_eo - pmatch[1].rm_so) + 1 , sizeof(char) );
				if( size_u == NULL ){
					fprintf(stderr,"Failed to calloc size_u\n");
					exit(1);
				}
				memcpy(size_u,getline_buffer + pmatch[1].rm_so,(pmatch[1].rm_eo - pmatch[1].rm_so));
				list->pkgs[list->pkg_count]->size_u = strtol(size_u, (char **)NULL, 10);
				free(size_u);
			}else{
				fprintf(stderr,"regexec failed to parse size_u\n");
				continue;
			}
		}else{
			fprintf(stderr,"getline reached EOF attempting to read size_u\n");
			continue;
		}

		/* description */
		if(
			(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)
			&& (strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
		){

			list->pkgs[list->pkg_count]->description[0] = '\0';

			while( 1 ){
				if( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF ){
					if( strcmp(getline_buffer,"\n") != 0 ){
						strncat(list->pkgs[list->pkg_count]->description,getline_buffer,bytes_read);
						list->pkgs[list->pkg_count]->description[
							strlen(list->pkgs[list->pkg_count]->description)
						] = '\0';
					}else{
						break;
					}
				}else{
					break;
				}
			}
		}else{
			fprintf(stderr,"error attempting to read pkg description\n");
			continue;
		}

		list->pkg_count += 1;
		/* grow our struct array */
		list->pkgs = realloc(list->pkgs , sizeof(pkg_info *) * (list->pkg_count + 1) );
		if( list->pkgs == NULL ){
			fprintf(stderr,"Failed to realloc pkgs\n");
			exit(1);
		}

		/* printf("%c\b",spinner()); this interferes with --list scripting */
		continue;
	}
	if( getline_buffer) free(getline_buffer);
	fclose(pkg_list_fh);

	return list;
}

char *gen_short_pkg_description(pkg_info *pkg){
	char *short_description = NULL;
	short_description = calloc(
		strlen(pkg->description) - strlen(pkg->name) + 2
			- strlen( index(pkg->description,'\n') ) + 1,
		sizeof(char)
	);
	if( short_description == NULL ){
		fprintf(stderr,"Failed to calloc short_description\n");
		exit(1);
	}
	short_description = memcpy(
		short_description,
		pkg->description + (strlen(pkg->name) + 2),
		strlen(pkg->description) - (strlen(pkg->name) + 2)
			- strlen( index(pkg->description,'\n') )
	);
	short_description[ strlen(short_description) ] = '\0';

	return short_description;
}


struct pkg_list *get_installed_pkgs(void){
	DIR *pkg_log_dir;
	struct dirent *file;
	regex_t regex;
	size_t nmatch = MAX_NMATCH;
	regmatch_t pmatch[MAX_PMATCH];
	int regexec_return;
	struct pkg_list *list = calloc( 1 , sizeof(struct pkg_list) );
	list->pkg_count = 0;

	/* open our pkg_log_dir */
	if( (pkg_log_dir = opendir(PKG_LOG_DIR)) == NULL ){
		if( errno ){
			perror(PKG_LOG_DIR);
		}
		exit(1);
	}

	/* compile our regex */
	if( (regexec_return = regcomp(&regex, PKG_LOG_PATTERN, REG_EXTENDED|REG_NEWLINE)) ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, "Failed to compile regex\n");

		if( (regerror_size = regerror(regexec_return, &regex,errbuf,errbuf_size)) ){
			printf("Regex Error: %s\n",errbuf);
		}
		exit(1);
	}

	list->pkgs = calloc( 1 , sizeof(pkg_info *) );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to calloc pkgs\n");
		exit(1);
	}

	while( (file = readdir(pkg_log_dir)) != NULL ){
		if( ( regexec(&regex,file->d_name,nmatch,pmatch,0) ) == 0 ){
			pkg_info *existing_pkg = NULL;
			pkg_info *tmp_pkg = calloc( 1 , sizeof(pkg_info) );
			if( tmp_pkg == NULL ){
				fprintf(stderr,"Failed to calloc tmp_pkg\n");
				exit(1);
			}


			memcpy(
				tmp_pkg->name,
				file->d_name + pmatch[1].rm_so,
				pmatch[1].rm_eo - pmatch[1].rm_so
			);
			tmp_pkg->name[ pmatch[1].rm_eo - pmatch[1].rm_so + 1 ] = '\0';
			memcpy(
				tmp_pkg->version,
				file->d_name + pmatch[2].rm_so,
				pmatch[2].rm_eo - pmatch[2].rm_so
			);
			tmp_pkg->version[ pmatch[2].rm_eo - pmatch[2].rm_so + 1 ] = '\0';

			/* add if no existing_pkg or tmp_pkg has greater version */
			if( ((existing_pkg = get_newest_pkg(list->pkgs,tmp_pkg->name,list->pkg_count)) == NULL)
				|| (strcmp(existing_pkg->version,tmp_pkg->version) < 0 )){

				list->pkgs[list->pkg_count] = calloc(1 , sizeof(pkg_info));
				if( list->pkgs[list->pkg_count] == NULL ){
					fprintf(stderr,"Failed to calloc list->pkgs[list->pkg_count]\n");
					exit(1);
				}
				memcpy(list->pkgs[list->pkg_count],tmp_pkg,sizeof(pkg_info));
				if( existing_pkg ) free(existing_pkg);
				list->pkg_count++;

				/* grow our pkgs array */
				list->pkgs = realloc(list->pkgs , sizeof(pkg_info *) * (list->pkg_count + 1 ) );
				if( list->pkgs == NULL ){
					fprintf(stderr,"Failed to realloc pkgs\n");
					if( errno ){
						perror("realloc");
					}
					exit(1);
				}

			}

			free(tmp_pkg);

		}/* end while */
	}
	closedir(pkg_log_dir);
	return list;
}

/* the foundation function... used directly or via lazy functions */
pkg_info *get_newest_pkg(pkg_info **pkgs,const char *pkg_name,int pkg_count){
	int iterator;
	pkg_info *pkg = NULL;
	for(iterator = 0; iterator < pkg_count; iterator++ ){

		/* if pkg has same name as our requested pkg */
		if( (strcmp(pkgs[iterator]->name,pkg_name)) == 0 ){

			if( pkg == NULL ){
				if( (pkg = calloc(1 , sizeof(pkg_info))) == NULL ){
					fprintf(stderr,"Failed to calloc pkg\n");
					exit(0);
				}
			}

			if( strcmp(pkg->version,pkgs[iterator]->version) < 0 ){
				pkg = memcpy(pkg,pkgs[iterator] , sizeof(pkg_info) );
			}
		}

	}
	return pkg;
}

/* lazy func to get newest version of installed by name */
pkg_info *get_newest_installed_pkg(const char *pkg_name){
	pkg_info *pkg = NULL;
	struct pkg_list *installed_pkgs = NULL;

	installed_pkgs = get_installed_pkgs();
	pkg = get_newest_pkg(installed_pkgs->pkgs,pkg_name,installed_pkgs->pkg_count);

	free_pkg_list(installed_pkgs);
	return pkg;
}

/* lazy func to get newest version of update by name */
pkg_info *get_newest_update_pkg(const char *pkg_name){
	pkg_info *pkg = NULL;
	struct pkg_list *update_pkgs = NULL;

	update_pkgs = parse_update_pkg_list();
	pkg = get_newest_pkg(update_pkgs->pkgs,pkg_name,update_pkgs->pkg_count);

	free_pkg_list(update_pkgs);
	return pkg;
}

/* parse the update list */
struct pkg_list *parse_update_pkg_list(void){
	FILE *fh;
	size_t getline_len;
	ssize_t bytes_read;
	regex_t regex;
	size_t nmatch = MAX_NMATCH;
	regmatch_t pmatch[MAX_PMATCH];
	char *getline_buffer = NULL;
	struct pkg_list *list;

	list = malloc( sizeof *list );
	list->pkg_count = 0;

	fh = open_file(PATCHES_LIST_L,"r");

	regcomp(&regex,PKG_PARSE_REGEX, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to malloc pkgs\n");
		exit(1);
	}

	while( (bytes_read = getline(&getline_buffer,&getline_len,fh)) != EOF ){
		if( (strstr(getline_buffer,"/packages/")) != NULL ){
			getline_buffer[ bytes_read ] = '\0';

			if( (regexec( &regex, getline_buffer, nmatch, pmatch, 0)) == 0 ){

				/* find out why malloc isn't working here... mem leak somewhere */
				list->pkgs[list->pkg_count] = calloc( 1 , sizeof(pkg_info) );
				if( list->pkgs[list->pkg_count] == NULL ){
					fprintf(stderr,"Failed to calloc list->pkgs[list->pkg_count]\n");
					exit(1);
				}

				/* pkg location/dir */
				memcpy( list->pkgs[list->pkg_count]->location, PATCHDIR, strlen(PATCHDIR));
				strncat(
					list->pkgs[list->pkg_count]->location,
					getline_buffer + pmatch[1].rm_so,
					pmatch[1].rm_eo - pmatch[1].rm_so
				);
				list->pkgs[list->pkg_count]->location[
					strlen(PATCHDIR) + (pmatch[1].rm_eo - pmatch[1].rm_so)
				] = '\0';

				/* pkg base name */
				memcpy(
					list->pkgs[list->pkg_count]->name,
					getline_buffer + pmatch[2].rm_so,
					pmatch[2].rm_eo - pmatch[2].rm_so
				);
				list->pkgs[list->pkg_count]->name[
					pmatch[2].rm_eo - pmatch[2].rm_so
				] = '\0';

				/* pkg version */
				memcpy(
					list->pkgs[list->pkg_count]->version,
					getline_buffer + pmatch[3].rm_so,
					pmatch[3].rm_eo - pmatch[3].rm_so
				);
				list->pkgs[list->pkg_count]->version[
					pmatch[3].rm_eo - pmatch[3].rm_so
				] = '\0';

				/* fill these in */
				list->pkgs[list->pkg_count]->description[0] = '\0';
				list->pkgs[list->pkg_count]->size_c = 0;
				list->pkgs[list->pkg_count]->size_u = 0;
				list->pkg_count++;
				list->pkgs = realloc(list->pkgs , sizeof(pkg_info *) * (list->pkg_count + 1));
				if( list->pkgs == NULL ){
					fprintf(stderr,"Failed to realloc pkgs\n");
					exit(1);
				}
			} /* end if regexec */

		} /* end if strstr */

		printf("%c\b",spinner());
	} /* end while */

	if( getline_buffer ) free(getline_buffer);
	regfree(&regex);
	fclose(fh);
	return list;
}


/* lazy func to get newest version of pkg by name */
pkg_info *lookup_pkg(const char *pkg_name){
	pkg_info *pkg = NULL;
	struct pkg_list *pkgs = NULL;
	int iterator;

	pkgs = parse_pkg_list();

	/* printf("About to show the details for %s\n",pkg_name); */
	for(iterator = 0; iterator < pkgs->pkg_count; iterator++ ){
		if( strcmp(pkgs->pkgs[iterator]->name,pkg_name) == 0 ){
			if( pkg == NULL ){
				pkg = malloc( sizeof *pkg );
				if( pkg == NULL ){
					fprintf(stderr,"Failed to malloc pkg\n");
					exit(0);
				}
			}
			memcpy(pkg,pkgs->pkgs[iterator] , sizeof(pkg_info) );
		}
	}

	free_pkg_list(pkgs);
	return pkg;
}

int install_pkg(const rc_config *global_config,pkg_info *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	char *cwd = NULL;
	int cmd_return = 0;

	if( global_config->simulate == 1 ){
		printf("%s-%s is to be installed\n",pkg->name,pkg->version);
		return 0;
	}

	cwd = getcwd(NULL,0);
	create_dir_structure(pkg->location);
	chdir(pkg->location);

	pkg_file_name = download_pkg(global_config,pkg);

	/* build and execute our command */
	command = calloc( strlen(INSTALL_CMD) + strlen(pkg_file_name) + 1 , sizeof(char) );
	command[0] = '\0';
	command = strcat(command,INSTALL_CMD);
	command = strcat(command,pkg_file_name);

	if( global_config->download_only == 0 ){
		printf("Preparing to install %s - %s\n",pkg->name,pkg->version);
		cmd_return = system(command);
	}

	chdir(cwd);
	free(cwd);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int upgrade_pkg(const rc_config *global_config,pkg_info *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	char *cwd = NULL;
	char prompt_answer[10];
	int cmd_return = 0;

	/* skip if excluded */
	if( is_excluded(global_config,pkg->name) == 1 ){
		printf("excluding %s\n",pkg->name);
		return 0;
	}

	if( global_config->simulate == 1 ){
		printf("%s is to be upgraded to version %s\n",pkg->name,pkg->version);
		return 0;
	}

	cwd = getcwd(NULL,0);
	create_dir_structure(pkg->location);
	chdir(pkg->location);

	/* download it */
	pkg_file_name = download_pkg(global_config,pkg);

	/* build and execute our command */
	command = calloc( strlen(UPGRADE_CMD) + strlen(pkg_file_name) + 1 , sizeof(char) );
	command[0] = '\0';
	command = strcat(command,UPGRADE_CMD);
	command = strcat(command,pkg_file_name);

	if( global_config->download_only == 0 ){
		if( global_config->no_prompt == 0 ){
			printf("Replace %s with %s-%s? [y|n] ",pkg->name,pkg->name,pkg->version);
			fgets(prompt_answer,10,stdin);
			if( tolower(prompt_answer[0]) != 'y' ){
				return cmd_return;
			}
		}
		printf("Preparing to replace %s with %s-%s\n",pkg->name,pkg->name,pkg->version);
		cmd_return = system(command);
	}

	chdir(cwd);
	free(cwd);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int remove_pkg(pkg_info *pkg){
	char *command = NULL;
	int cmd_return;

	/* build and execute our command */
	command = calloc( strlen(REMOVE_CMD) + strlen(pkg->name) + 1 , sizeof(char) );
	command[0] = '\0';
	command = strcat(command,REMOVE_CMD);
	command = strcat(command,pkg->name);
	cmd_return = system(command);

	free(command);
	return cmd_return;
}

void free_pkg_list(struct pkg_list *list){
	int iterator;
	for(iterator=0;iterator < list->pkg_count;iterator++){
		free(list->pkgs[iterator]);
	}
	free(list->pkgs);
	free(list);
}

