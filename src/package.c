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
struct pkg_list *get_available_pkgs(void){
	sg_regex name_regex, location_regex, size_c_regex, size_u_regex;
	ssize_t bytes_read;
	FILE *pkg_list_fh;
	pkg_info_t **realloc_tmp;
	struct pkg_list *list;
	long f_pos = 0;
	size_t getline_len = 0;
	char *getline_buffer = NULL;
	char *size_c = NULL;
	char *size_u = NULL;

	list = malloc( sizeof *list );

	name_regex.nmatch = MAX_REGEX_PARTS;
	location_regex.nmatch = MAX_REGEX_PARTS;
	size_c_regex.nmatch = MAX_REGEX_PARTS;
	size_u_regex.nmatch = MAX_REGEX_PARTS;


	/* open pkg list */
	pkg_list_fh = open_file(PKG_LIST_L,"r");

	/* compile our regexen */
	regcomp(&name_regex.regex,PKG_NAME_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&location_regex.regex,PKG_LOCATION_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_c_regex.regex,PKG_SIZEC_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_u_regex.regex,PKG_SIZEU_PATTERN, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to malloc pkgs\n");
		exit(1);
	}
	list->pkg_count = 0;

	while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh) ) != EOF ){
		getline_buffer[bytes_read - 1] = '\0';

		/* pull out package data */
		if( strstr(getline_buffer,"PACKAGE NAME") == NULL ){
			continue;
		}

		name_regex.reg_return = regexec(
			&name_regex.regex,
			getline_buffer,
			name_regex.nmatch,
			name_regex.pmatch,
			0
		);

		if( name_regex.reg_return == 0 ){
			list->pkgs[list->pkg_count] = malloc( sizeof *list->pkgs[list->pkg_count] );

			if( list->pkgs[list->pkg_count] == NULL ){
				fprintf(stderr,"Failed to malloc list->pkgs[list->pkg_count]\n");
				exit(1);
			}

			/* pkg name base */
			strncpy(list->pkgs[list->pkg_count]->name,
				getline_buffer + name_regex.pmatch[1].rm_so,
				name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
			);
			list->pkgs[list->pkg_count]->name[
				name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
			] = '\0';
			/* pkg version */
			strncpy(list->pkgs[list->pkg_count]->version,
				getline_buffer + name_regex.pmatch[2].rm_so,
				name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so
			);
			list->pkgs[list->pkg_count]->version[
				name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so
			] = '\0';

		}else{
			fprintf(stderr,"regex failed on [%s]\n",getline_buffer);
			continue;
		}

		/* location */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) ){

			location_regex.reg_return = regexec(
				&location_regex.regex,
				getline_buffer,
				location_regex.nmatch,
				location_regex.pmatch,
				0
			);
			if( location_regex.reg_return == 0){
				strncpy(list->pkgs[list->pkg_count]->location,
					getline_buffer + location_regex.pmatch[1].rm_so,
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				);
				list->pkgs[list->pkg_count]->location[
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				] = '\0';
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
			size_c_regex.reg_return = regexec(
				&size_c_regex.regex,
				getline_buffer,
				size_c_regex.nmatch,
				size_c_regex.pmatch,
				0
			);
			if( size_c_regex.reg_return == 0 ){
				size_c = calloc(
					(size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so) + 1 , sizeof *size_c 
				);
				if( size_c == NULL ){
					fprintf(stderr,"Failed to calloc size_c\n");
					exit(1);
				}
				strncpy(
					size_c,
					getline_buffer + size_c_regex.pmatch[1].rm_so,
					(size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so)
				);
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

			size_u_regex.reg_return = regexec(
				&size_u_regex.regex, getline_buffer, size_u_regex.nmatch, size_u_regex.pmatch, 0
			);

			if( size_u_regex.reg_return == 0 ){

				size_u = calloc(
					(size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so) + 1 ,
					sizeof *size_u
				);

				if( size_u == NULL ){
					fprintf(stderr,"Failed to calloc size_u\n");
					exit(1);
				}
				strncpy(
					size_u,getline_buffer + size_u_regex.pmatch[1].rm_so,
					(size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so)
				);
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

		/* required, if provided */
		f_pos = ftell(pkg_list_fh);
		if(
			(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)
			&& (strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
				/* add in support for the required data */
		){
			/* required isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
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
		realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1) );
		if( realloc_tmp == NULL ){
			fprintf(stderr,"Failed to realloc pkgs\n");
			exit(1);
		}else{
			list->pkgs = realloc_tmp;
		}

		/* printf("%c\b",spinner()); this interferes with --list scripting */
		continue;
	}
	if( getline_buffer) free(getline_buffer);
	fclose(pkg_list_fh);
	regfree(&name_regex.regex);
	regfree(&location_regex.regex);
	regfree(&size_c_regex.regex);
	regfree(&size_u_regex.regex);

	return list;
}

char *gen_short_pkg_description(pkg_info_t *pkg){
	char *short_description = NULL;
	size_t string_size = 0;

	string_size = strlen(pkg->description) - (strlen(pkg->name) + 2) - strlen( strchr(pkg->description,'\n') );

	/* quit now if the description is going to be empty */
	if( (int)string_size < 0 ) return NULL;

	short_description = calloc( string_size + 1 , sizeof *short_description );
	if( short_description == NULL ){
		fprintf(stderr,"Failed to calloc short_description\n");
		exit(1);
	}

	short_description = strncpy(
		short_description,
		pkg->description + (strlen(pkg->name) + 2),
		string_size
	);
	short_description[ string_size ] = '\0';

	return short_description;
}


struct pkg_list *get_installed_pkgs(void){
	DIR *pkg_log_dir;
	struct dirent *file;
	sg_regex ip_regex;
	pkg_info_t **realloc_tmp;
	struct pkg_list *list;

	list = malloc( sizeof *list );
	list->pkg_count = 0;
	ip_regex.nmatch = MAX_REGEX_PARTS;

	/* open our pkg_log_dir */
	if( (pkg_log_dir = opendir(PKG_LOG_DIR)) == NULL ){
		if( errno ){
			perror(PKG_LOG_DIR);
		}
		exit(1);
	}

	/* compile our regex */
	ip_regex.reg_return = regcomp(&ip_regex.regex, PKG_LOG_PATTERN, REG_EXTENDED|REG_NEWLINE);
	if( ip_regex.reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, "Failed to compile regex\n");

		if( (regerror_size = regerror(ip_regex.reg_return, &ip_regex.regex,errbuf,errbuf_size)) ){
			printf("Regex Error: %s\n",errbuf);
		}
		exit(1);
	}

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to malloc pkgs\n");
		exit(1);
	}

	while( (file = readdir(pkg_log_dir)) != NULL ){
		ip_regex.reg_return = regexec(&ip_regex.regex,file->d_name,ip_regex.nmatch,ip_regex.pmatch,0);
		if( ip_regex.reg_return == 0 ){
			pkg_info_t *existing_pkg = NULL;
			pkg_info_t *tmp_pkg;
			tmp_pkg = malloc( sizeof *tmp_pkg );
			if( tmp_pkg == NULL ){
				fprintf(stderr,"Failed to malloc tmp_pkg\n");
				exit(1);
			}


			strncpy(
				tmp_pkg->name,
				file->d_name + ip_regex.pmatch[1].rm_so,
				ip_regex.pmatch[1].rm_eo - ip_regex.pmatch[1].rm_so
			);
			tmp_pkg->name[ ip_regex.pmatch[1].rm_eo - ip_regex.pmatch[1].rm_so ] = '\0';
			strncpy(
				tmp_pkg->version,
				file->d_name + ip_regex.pmatch[2].rm_so,
				ip_regex.pmatch[2].rm_eo - ip_regex.pmatch[2].rm_so
			);
			tmp_pkg->version[ ip_regex.pmatch[2].rm_eo - ip_regex.pmatch[2].rm_so ] = '\0';

			/* add if no existing_pkg or tmp_pkg has greater version */
			if( ((existing_pkg = get_newest_pkg(list->pkgs,tmp_pkg->name,list->pkg_count)) == NULL)
				|| (cmp_pkg_versions(existing_pkg->version,tmp_pkg->version) < 0 )){

				list->pkgs[list->pkg_count] = malloc( sizeof *list->pkgs[list->pkg_count] );
				if( list->pkgs[list->pkg_count] == NULL ){
					fprintf(stderr,"Failed to malloc list->pkgs[list->pkg_count]\n");
					exit(1);
				}
				memcpy(list->pkgs[list->pkg_count],tmp_pkg,sizeof *tmp_pkg);
				list->pkg_count++;

				/* grow our pkgs array */
				realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1 ) );
				if( realloc_tmp == NULL ){
					fprintf(stderr,"Failed to realloc pkgs\n");
					if( errno ){
						perror("realloc");
					}
					exit(1);
				}else{
					list->pkgs = realloc_tmp;
				}

			}

			free(tmp_pkg);

		}/* end while */
	}
	closedir(pkg_log_dir);
	regfree(&ip_regex.regex);

	return list;
}

/* lookup newest package from pkg_list */
pkg_info_t *get_newest_pkg(pkg_info_t **pkgs,const char *pkg_name,int pkg_count){
	int iterator;
	pkg_info_t *pkg = NULL;
	for(iterator = 0; iterator < pkg_count; iterator++ ){

		/* if pkg has same name as our requested pkg */
		if( (strcmp(pkgs[iterator]->name,pkg_name)) == 0 ){

			if( (pkg == NULL) || cmp_pkg_versions(pkg->version,pkgs[iterator]->version) < 0 ){
				pkg = pkgs[iterator];
			}
		}

	}
	return pkg;
}

/* parse the update list */
struct pkg_list *get_update_pkgs(void){
	FILE *fh;
	size_t getline_len;
	ssize_t bytes_read;
	sg_regex up_regex;
	char *getline_buffer = NULL;
	pkg_info_t **realloc_tmp;
	struct pkg_list *list;

	list = malloc( sizeof *list );
	list->pkg_count = 0;
	up_regex.nmatch = MAX_REGEX_PARTS;

	fh = open_file(PATCHES_LIST_L,"r");

	up_regex.reg_return = regcomp(&up_regex.regex,PKG_PARSE_REGEX, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,"Failed to malloc pkgs\n");
		exit(1);
	}

	while( (bytes_read = getline(&getline_buffer,&getline_len,fh)) != EOF ){
		if( (strstr(getline_buffer,"/packages/")) != NULL ){

			up_regex.reg_return = regexec(
				&up_regex.regex,
				getline_buffer,
				up_regex.nmatch,
				up_regex.pmatch,
				0
			);
			if( up_regex.reg_return == 0 ){

				/* find out why malloc isn't working here... mem leak somewhere */
				list->pkgs[list->pkg_count] = calloc( 1, sizeof *list->pkgs[list->pkg_count] );
				if( list->pkgs[list->pkg_count] == NULL ){
					fprintf(stderr,"Failed to calloc list->pkgs[list->pkg_count]\n");
					exit(1);
				}

				/* pkg location/dir */
				strncpy( list->pkgs[list->pkg_count]->location, PATCHDIR, strlen(PATCHDIR));
				strncat(
					list->pkgs[list->pkg_count]->location,
					getline_buffer + up_regex.pmatch[1].rm_so,
					up_regex.pmatch[1].rm_eo - up_regex.pmatch[1].rm_so
				);
				list->pkgs[list->pkg_count]->location[
					strlen(PATCHDIR) + (up_regex.pmatch[1].rm_eo - up_regex.pmatch[1].rm_so)
				] = '\0';

				/* pkg base name */
				strncpy(
					list->pkgs[list->pkg_count]->name,
					getline_buffer + up_regex.pmatch[2].rm_so,
					up_regex.pmatch[2].rm_eo - up_regex.pmatch[2].rm_so
				);
				list->pkgs[list->pkg_count]->name[
					up_regex.pmatch[2].rm_eo - up_regex.pmatch[2].rm_so
				] = '\0';

				/* pkg version */
				strncpy(
					list->pkgs[list->pkg_count]->version,
					getline_buffer + up_regex.pmatch[3].rm_so,
					up_regex.pmatch[3].rm_eo - up_regex.pmatch[3].rm_so
				);
				list->pkgs[list->pkg_count]->version[
					up_regex.pmatch[3].rm_eo - up_regex.pmatch[3].rm_so
				] = '\0';

				/* fill these in */
				list->pkgs[list->pkg_count]->description[0] = '\0';
				list->pkgs[list->pkg_count]->size_c = 0;
				list->pkgs[list->pkg_count]->size_u = 0;
				list->pkg_count++;

				realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1));
				if( realloc_tmp == NULL ){
					fprintf(stderr,"Failed to realloc pkgs\n");
					exit(1);
				}else{
					list->pkgs = realloc_tmp;
				}

			} /* end if regexec */

		} /* end if strstr */

		printf("%c\b",spinner());
	} /* end while */

	if( getline_buffer ) free(getline_buffer);
	regfree(&up_regex.regex);
	fclose(fh);
	return list;
}


int install_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	int cmd_return = 0;

	if( global_config->simulate == 1 ){
		printf("%s-%s is to be installed\n",pkg->name,pkg->version);
		return 0;
	}

	create_dir_structure(pkg->location);
	chdir(pkg->location);

	pkg_file_name = download_pkg(global_config,pkg);
	if( pkg_file_name == NULL ){
		chdir(global_config->working_dir);
		return -1;
	}

	/* build and execute our command */
	command = calloc( strlen(INSTALL_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strcat(command,INSTALL_CMD);
	command = strcat(command,pkg_file_name);

	if( global_config->download_only == 0 ){
		printf("Preparing to install %s-%s\n",pkg->name,pkg->version);
		if( (cmd_return = system(command)) == -1 ){
			printf("Failed to execute command: [%s]\n",command);
			exit(1);
		}
	}

	chdir(global_config->working_dir);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	char prompt_answer[10];
	int cmd_return = 0;

	/* skip if excluded */
	if( is_excluded(global_config,pkg->name) == 1 ){
		printf("excluding %s\n",pkg->name);
		return 0;
	}

	if( global_config->simulate == 1 ){
		printf("%s-%s is to be upgraded to version %s\n",pkg->name,installed_pkg->version,pkg->version);
		return 0;
	}

	if( global_config->no_prompt == 0 ){
		printf("Replace %s-%s with %s-%s? [y|n] ",pkg->name,installed_pkg->version,pkg->name,pkg->version);
		fgets(prompt_answer,10,stdin);
		if( tolower(prompt_answer[0]) != 'y' ){
			chdir(global_config->working_dir);
			return cmd_return;
		}
	}

	create_dir_structure(pkg->location);
	chdir(pkg->location);

	/* download it */
	pkg_file_name = download_pkg(global_config,pkg);
	if( pkg_file_name == NULL ){
		chdir(global_config->working_dir);
		return -1;
	}

	/* build and execute our command */
	command = calloc( strlen(UPGRADE_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strcat(command,UPGRADE_CMD);
	command = strcat(command,pkg_file_name);

	if( global_config->download_only == 0 ){
		printf("Preparing to replace %s-%s with %s-%s\n",pkg->name,installed_pkg->version,pkg->name,pkg->version);
		if( (cmd_return = system(command)) == -1 ){
			printf("Failed to execute command: [%s]\n",command);
			exit(1);
		}
	}

	chdir(global_config->working_dir);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int remove_pkg(pkg_info_t *pkg){
	char *command = NULL;
	int cmd_return;

	/* build and execute our command */
	command = calloc( strlen(REMOVE_CMD) + strlen(pkg->name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strcat(command,REMOVE_CMD);
	command = strcat(command,pkg->name);
	if( (cmd_return = system(command)) == -1 ){
		printf("Failed to execute command: [%s]\n",command);
		exit(1);
	}

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

int is_excluded(const rc_config *global_config,const char *pkg_name){
	int i;

	if( global_config->ignore_excludes == 1 )
		return 0;

	/* maybe EXCLUDE= isn't defined in our rc? */
	if( global_config->exclude_list == NULL )
		return 0;

	for(i = 0; i < global_config->exclude_list->count;i++){
		/*
		 * this is kludgy... global_config->exclude_list->excludes[i] is 1 char longer
		 * than pkg_name
		*/
		if( (strncmp(global_config->exclude_list->excludes[i],pkg_name,strlen(pkg_name)) == 0)
			&& (strlen(global_config->exclude_list->excludes[i]) - strlen(pkg_name) < 2) ){
			return 1;
		}
	}

	return 0;
}

void get_md5sum(const rc_config *global_config,pkg_info_t *pkg,char *md5_sum){
	FILE *checksum_file;
	sg_regex md5sum_regex;
	ssize_t getline_read;
	size_t getline_len = 0;
	char *getline_buffer = NULL;
	char *cwd = NULL;

	md5sum_regex.nmatch = MAX_REGEX_PARTS;

	cwd = getcwd(NULL,0);

	chdir(global_config->working_dir);

	checksum_file = open_file(CHECKSUM_FILE,"r");

	md5sum_regex.reg_return = regcomp(&md5sum_regex.regex,MD5SUM_REGEX, REG_EXTENDED|REG_NEWLINE);
	if( md5sum_regex.reg_return != 0 ){
		fprintf(stderr,"Failed to compile regex [%s]\n",MD5SUM_REGEX);
		exit(1);
	}

	while( (getline_read = getline(&getline_buffer,&getline_len,checksum_file) ) != EOF ){

		if( strstr(getline_buffer,".tgz") == NULL) continue;

		md5sum_regex.reg_return = regexec( &md5sum_regex.regex, getline_buffer, md5sum_regex.nmatch, md5sum_regex.pmatch, 0);
		if( md5sum_regex.reg_return == 0 ){
			char name[50];
			char version[50];
			char sum[34];

			strncpy(
				name,
				getline_buffer + md5sum_regex.pmatch[3].rm_so,
				md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so
			);
			name[md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so] = '\0';

			strncpy(
				version,
				getline_buffer + md5sum_regex.pmatch[4].rm_so,
				md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so
			);
			version[md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so] = '\0';

			strncpy(
				sum,
				getline_buffer + md5sum_regex.pmatch[1].rm_so,
				md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so
			);
			sum[md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so] = '\0';

			if( (strcmp(pkg->name,name) == 0) && (cmp_pkg_versions(pkg->version,version) == 0) ){
				memcpy(md5_sum,sum,md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so + 1);
				break;
			}

		}
	}
	fclose(checksum_file);
	chdir(cwd);
	free(cwd);
	regfree(&md5sum_regex.regex);

	return;
}

int cmp_pkg_versions(char *a, char *b){
	int ver_breakdown_1[] = { 0, 0, 0, 0, 0, 0 };
	int ver_breakdown_2[] = { 0, 0, 0, 0, 0, 0 };
	int version_part_count1,version_part_count2,position = 0;
	int greater = 1,lesser = -1;

	version_part_count1 = break_down_pkg_version(ver_breakdown_1,a);
	version_part_count2 = break_down_pkg_version(ver_breakdown_2,b);

	while( position < version_part_count1 && position < version_part_count2 ){
		if( ver_breakdown_1[position] != ver_breakdown_2[position] ){

			if( ver_breakdown_1[position] < ver_breakdown_2[position] )
				return lesser;

			if( ver_breakdown_1[position] > ver_breakdown_2[position] )
				return greater;

		}
		++position;
	}

	/*
 	 * if we got this far, we know that some or all of the version
	 * parts are equal in both packages.  If pkg-a has 3 version parts
	 * and pkg-b has 2, then we assume pkg-a to be greater.  If both
	 * have the same # of version parts, then we fall back on strcmp.
	*/
	return (version_part_count1 != version_part_count2)
		? ( (version_part_count1 > version_part_count2) ? greater : lesser )
		: strcmp(a,b);
}

int break_down_pkg_version(int *v,char *version){
	int pos = 0,count = 0,sv_size;
	char *pointer,*tmp,*short_version;


	/* generate a short version, leave out arch and release */
	if( (pointer = strchr(version,'-')) == NULL ){
		return 0;
	}else{
		sv_size = ( strlen(version) - strlen(pointer) + 1);
		short_version = malloc( sizeof *short_version * sv_size );
		memcpy(short_version,version,sv_size);
		short_version[sv_size - 1] = '\0';
		pointer = NULL;
	}

	while(pos < (sv_size - 1) ){
		if( (pointer = strchr(short_version + pos,'.')) != NULL ){
			int b_count = ( strlen(short_version + pos) - strlen(pointer) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			v[count] = atoi(tmp);
			count++;
			free(tmp);
			pointer = NULL;
			pos += b_count;
		}else{
			int b_count = ( strlen(short_version + pos) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			v[count] = atoi(tmp);
			count++;
			free(tmp);
			pos += b_count;
		}
	}

	free(short_version);
	return count;
}

