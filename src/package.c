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
	FILE *pkg_list_fh;
	struct pkg_list *list;

	/* open pkg list */
	pkg_list_fh = open_file(PKG_LIST_L,"r");
	list = parse_packages_txt(pkg_list_fh);
	fclose(pkg_list_fh);

	return list;
}

struct pkg_list *parse_packages_txt(FILE *pkg_list_fh){
	sg_regex name_regex, mirror_regex,location_regex, size_c_regex, size_u_regex;
	ssize_t bytes_read;
	pkg_info_t **realloc_tmp;
	struct pkg_list *list;
	long f_pos = 0;
	size_t getline_len = 0;
	char *getline_buffer = NULL;
	char *size_c = NULL;
	char *size_u = NULL;
	char *char_pointer = NULL;
	pkg_info_t *tmp_pkg;

	list = malloc( sizeof *list );

	name_regex.nmatch = MAX_REGEX_PARTS;
	mirror_regex.nmatch = MAX_REGEX_PARTS;
	location_regex.nmatch = MAX_REGEX_PARTS;
	size_c_regex.nmatch = MAX_REGEX_PARTS;
	size_u_regex.nmatch = MAX_REGEX_PARTS;


	/* compile our regexen */
	regcomp(&name_regex.regex,PKG_NAME_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&mirror_regex.regex,PKG_MIRROR_PATTERN,REG_EXTENDED|REG_NEWLINE);
	regcomp(&location_regex.regex,PKG_LOCATION_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_c_regex.regex,PKG_SIZEC_PATTERN, REG_EXTENDED|REG_NEWLINE);
	regcomp(&size_u_regex.regex,PKG_SIZEU_PATTERN, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc pkgs\n"));
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

		/* skip this line if we didn't find a package name */
		if( name_regex.reg_return != 0 ){
			fprintf(stderr,_("regex failed on [%s]\n"),getline_buffer);
			continue;
		}
		/* otherwise keep going and parse out the rest of the pkg data */

		tmp_pkg = malloc( sizeof *tmp_pkg );
		if( tmp_pkg == NULL ){
			fprintf(stderr,_("Failed to malloc tmp_pkg\n"));
			exit(1);
		}

		/* pkg name base */
		/* don't overflow the buffer */
		if( (name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so) > NAME_LEN ){
			fprintf( stderr, _("pkg name too long [%s:%d]\n"),
				getline_buffer + name_regex.pmatch[1].rm_so,
				name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
			);
			free(tmp_pkg);
			continue;
		}
		strncpy(tmp_pkg->name,
			getline_buffer + name_regex.pmatch[1].rm_so,
			name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
		);
		tmp_pkg->name[
			name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
		] = '\0';

		/* pkg version */
		/* don't overflow the buffer */
		if( (name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so) > VERSION_LEN ){
			fprintf( stderr, _("pkg version too long [%s:%d]\n"),
				getline_buffer + name_regex.pmatch[2].rm_so,
				name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so
			);
			free(tmp_pkg);
			continue;
		}
		strncpy(tmp_pkg->version,
			getline_buffer + name_regex.pmatch[2].rm_so,
			name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so
		);
		tmp_pkg->version[
			name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so
		] = '\0';

		/* mirror */
		f_pos = ftell(pkg_list_fh);
		if(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF){
			/* add in support for the mirror url */
			mirror_regex.reg_return = regexec(
				&mirror_regex.regex,
				getline_buffer,
				mirror_regex.nmatch,
				mirror_regex.pmatch,
				0
			);
			if( mirror_regex.reg_return == 0 ){

				/* don't overflow the buffer */
				if( (mirror_regex.pmatch[1].rm_eo - mirror_regex.pmatch[1].rm_so) > MIRROR_LEN ){
					fprintf( stderr, _("pkg mirror too long [%s:%d]\n"),
						getline_buffer + mirror_regex.pmatch[1].rm_so,
						mirror_regex.pmatch[1].rm_eo - mirror_regex.pmatch[1].rm_so
					);
					free(tmp_pkg);
					continue;
				}

				strncpy( tmp_pkg->mirror,
					getline_buffer + mirror_regex.pmatch[1].rm_so,
					mirror_regex.pmatch[1].rm_eo - mirror_regex.pmatch[1].rm_so
				);
				tmp_pkg->mirror[
					mirror_regex.pmatch[1].rm_eo - mirror_regex.pmatch[1].rm_so
				] = '\0';

			}else{
				/* mirror isn't provided... rewind one line */
				fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
			}
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

				/* don't overflow the buffer */
				if( (location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so) > LOCATION_LEN ){
					fprintf( stderr, _("pkg location too long [%s:%d]\n"),
						getline_buffer + location_regex.pmatch[1].rm_so,
						location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
					);
					free(tmp_pkg);
					continue;
				}

				strncpy(tmp_pkg->location,
					getline_buffer + location_regex.pmatch[1].rm_so,
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				);
				tmp_pkg->location[
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				] = '\0';
			}else{
				fprintf(stderr,_("regexec failed to parse location\n"));
				free(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read location\n"));
			free(tmp_pkg);
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
					fprintf(stderr,_("Failed to calloc size_c\n"));
					exit(1);
				}
				strncpy(
					size_c,
					getline_buffer + size_c_regex.pmatch[1].rm_so,
					(size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so)
				);
				size_c[ (size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so) ] = '\0';
				tmp_pkg->size_c = strtol(size_c, (char **)NULL, 10);
				free(size_c);
			}else{
				fprintf(stderr,_("regexec failed to parse size_c\n"));
				free(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read size_c\n"));
			free(tmp_pkg);
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
					fprintf(stderr,_("Failed to calloc size_u\n"));
					exit(1);
				}
				strncpy(
					size_u,getline_buffer + size_u_regex.pmatch[1].rm_so,
					(size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so)
				);
				size_u[ (size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so) ] = '\0';
				tmp_pkg->size_u = strtol(size_u, (char **)NULL, 10);
				free(size_u);
			}else{
				fprintf(stderr,_("regexec failed to parse size_u\n"));
				free(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read size_u\n"));
			free(tmp_pkg);
			continue;
		}

		/* required, if provided */
		f_pos = ftell(pkg_list_fh);
		tmp_pkg->required[0] = '\0';
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF)
			&& ((char_pointer = strstr(getline_buffer,"PACKAGE REQUIRED")) != NULL)
		){
				/* add in support for the required data */
				size_t req_len = strlen("PACKAGE REQUIRED") + 2;
				getline_buffer[bytes_read - 1] = '\0';
				strncpy(tmp_pkg->required,char_pointer + req_len, strlen(char_pointer + req_len));
				tmp_pkg->required[ strlen(char_pointer + req_len) ] = '\0';
		}else{
			/* required isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* description */
		if(
			(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)
			&& (strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
		){

			tmp_pkg->description[0] = '\0';

			while( 1 ){
				if( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF ){

					if( strcmp(getline_buffer,"\n") != 0
						/* don't overflow the buffer */
						&& (strlen(tmp_pkg->description) + bytes_read) < DESCRIPTION_LEN
					){
						strncat(tmp_pkg->description,getline_buffer,bytes_read);
					}else{
						break;
					}

				}else{
					break;
				}
			}
		}else{
			fprintf(stderr,_("error attempting to read pkg description\n"));
			free(tmp_pkg);
			continue;
		}

		/* grow our struct array */
		realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1) );
		if( realloc_tmp == NULL ){
			fprintf(stderr,_("Failed to realloc pkgs\n"));
			exit(1);
		}

		list->pkgs = realloc_tmp;
		list->pkgs[list->pkg_count] = tmp_pkg;
		++list->pkg_count;
		tmp_pkg = NULL;

		/* printf("%c\b",spinner()); this interferes with --list scripting */
		continue;
	}
	if( getline_buffer) free(getline_buffer);
	regfree(&name_regex.regex);
	regfree(&mirror_regex.regex);
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
		fprintf(stderr,_("Failed to calloc short_description\n"));
		exit(1);
	}

	strncpy(
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
		fprintf(stderr, _("Failed to compile regex\n"));

		if( (regerror_size = regerror(ip_regex.reg_return, &ip_regex.regex,errbuf,errbuf_size)) ){
			printf(_("Regex Error: %s\n"),errbuf);
		}
		exit(1);
	}

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc pkgs\n"));
		exit(1);
	}

	while( (file = readdir(pkg_log_dir)) != NULL ){
		ip_regex.reg_return = regexec(&ip_regex.regex,file->d_name,ip_regex.nmatch,ip_regex.pmatch,0);
		if( ip_regex.reg_return == 0 ){
			pkg_info_t *existing_pkg = NULL;
			pkg_info_t *tmp_pkg;
			tmp_pkg = malloc( sizeof *tmp_pkg );
			if( tmp_pkg == NULL ){
				fprintf(stderr,_("Failed to malloc tmp_pkg\n"));
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
			if( ((existing_pkg = get_newest_pkg(list,tmp_pkg->name)) == NULL)
				|| (cmp_pkg_versions(existing_pkg->version,tmp_pkg->version) < 0 )){

				list->pkgs[list->pkg_count] = tmp_pkg;
				++list->pkg_count;
				tmp_pkg = NULL;

				/* grow our pkgs array */
				realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1 ) );
				if( realloc_tmp == NULL ){
					fprintf(stderr,_("Failed to realloc pkgs\n"));
					if( errno ){
						perror("realloc");
					}
					exit(1);
				}else{
					list->pkgs = realloc_tmp;
				}

			}

		}/* end while */
	}
	closedir(pkg_log_dir);
	regfree(&ip_regex.regex);

	return list;
}

/* lookup newest package from pkg_list */
pkg_info_t *get_newest_pkg(struct pkg_list *pkg_list,const char *pkg_name){
	int i;
	pkg_info_t *pkg = NULL;
	for(i = 0; i < pkg_list->pkg_count; i++ ){

		/* if pkg has same name as our requested pkg */
		if( (strcmp(pkg_list->pkgs[i]->name,pkg_name)) == 0 ){

			if( (pkg == NULL) || cmp_pkg_versions(pkg->version,pkg_list->pkgs[i]->version) < 0 ){
				pkg = pkg_list->pkgs[i];
			}
		}

	}
	return pkg;
}

pkg_info_t *get_exact_pkg(struct pkg_list *list,const char *name,const char *version){
	int i;
	pkg_info_t *pkg = NULL;

	for(i = 0; i < list->pkg_count;i++){
		if( (strcmp(name,list->pkgs[i]->name)==0) && (strcmp(version,list->pkgs[i]->version)==0) )
			return list->pkgs[i];
	}
	return pkg;
}

/* parse the update list */
/*
 * this function is here for historic reasons
 * and in case it may ever need to be revived
 * from it's cold sleep of death.
*/
struct pkg_list *parse_file_list(FILE *fh){
	size_t getline_len;
	ssize_t bytes_read;
	sg_regex up_regex;
	char *getline_buffer = NULL;
	pkg_info_t **realloc_tmp;
	struct pkg_list *list;

	list = malloc( sizeof *list );
	list->pkg_count = 0;
	up_regex.nmatch = MAX_REGEX_PARTS;

	up_regex.reg_return = regcomp(&up_regex.regex,PKG_PARSE_REGEX, REG_EXTENDED|REG_NEWLINE);

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc pkgs\n"));
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
					fprintf(stderr,_("Failed to calloc list->pkgs[list->pkg_count]\n"));
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
				++list->pkg_count;

				realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1));
				if( realloc_tmp == NULL ){
					fprintf(stderr,_("Failed to realloc pkgs\n"));
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
	return list;
}


int install_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	int cmd_return = 0;
	char prompt_answer[10];

	if( global_config->simulate == 1 ){
		printf(_("%s-%s is to be installed\n"),pkg->name,pkg->version);
		return 0;
	}

	if( global_config->interactive == 1 ){
		printf(_("Install %s-%s [Y|n] "),pkg->name,pkg->version);
		fgets(prompt_answer,10,stdin);
		if( tolower(prompt_answer[0]) == 'n' ){
			chdir(global_config->working_dir);
			return cmd_return;
		}
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
		printf(_("Preparing to install %s-%s\n"),pkg->name,pkg->version);
		if( (cmd_return = system(command)) == -1 ){
			printf(_("Failed to execute command: [%s]\n"),command);
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
	if( is_excluded(global_config,pkg) == 1 ){
		printf(_("excluding %s\n"),pkg->name);
		return 0;
	}

	if( global_config->simulate == 1 ){
		printf(_("%s-%s is to be upgraded to version %s\n"),pkg->name,installed_pkg->version,pkg->version);
		return 0;
	}

	/*
		only give double check notice if in interactive,
		w/o no_prompt and download_only
	*/
	if( global_config->no_prompt == 0 && global_config->download_only == 0 && global_config->interactive == 1 ){
		printf(_("Replace %s-%s with %s-%s? [y|N] "),pkg->name,installed_pkg->version,pkg->name,pkg->version);
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
		printf(_("Preparing to replace %s-%s with %s-%s\n"),pkg->name,installed_pkg->version,pkg->name,pkg->version);
		if( (cmd_return = system(command)) == -1 ){
			printf(_("Failed to execute command: [%s]\n"),command);
			exit(1);
		}
	}

	chdir(global_config->working_dir);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int remove_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *command = NULL;
	int cmd_return;

	if( global_config->simulate == 1 ){
		printf(_("%s-%s is to be removed\n"),pkg->name,pkg->version);
		return 0;
	}

	/* build and execute our command */
	command = calloc(
		strlen(REMOVE_CMD) + strlen(pkg->name) + strlen("-") + strlen(pkg->version) + 1,
		sizeof *command
	);
	command[0] = '\0';
	command = strcat(command,REMOVE_CMD);
	command = strcat(command,pkg->name);
	command = strcat(command,"-");
	command = strcat(command,pkg->version);
	if( (cmd_return = system(command)) == -1 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		exit(1);
	}

	free(command);
	return cmd_return;
}

void free_pkg_list(struct pkg_list *list){
	int i;
	for(i=0;i < list->pkg_count;i++){
		free(list->pkgs[i]);
	}
	free(list->pkgs);
	free(list);
}

int is_excluded(const rc_config *global_config,pkg_info_t *pkg){
	int i;
	sg_regex exclude_reg;

	if( global_config->ignore_excludes == 1 )
		return 0;

	/* maybe EXCLUDE= isn't defined in our rc? */
	if( global_config->exclude_list == NULL )
		return 0;

	exclude_reg.nmatch = MAX_REGEX_PARTS;

	for(i = 0; i < global_config->exclude_list->count;i++){
		if( (strncmp(global_config->exclude_list->excludes[i],pkg->name,strlen(pkg->name)) == 0))
			return 1;
		regcomp(&exclude_reg.regex,global_config->exclude_list->excludes[i],REG_EXTENDED|REG_NEWLINE);
		if(
			(regexec( &exclude_reg.regex, pkg->name, exclude_reg.nmatch, exclude_reg.pmatch, 0) == 0)
			|| (regexec( &exclude_reg.regex, pkg->version, exclude_reg.nmatch, exclude_reg.pmatch, 0) == 0)
		){
			regfree(&exclude_reg.regex);
			return 1;
		}
		regfree(&exclude_reg.regex);
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
		fprintf(stderr,_("Failed to compile regex [%s]\n"),MD5SUM_REGEX);
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
	int pos = 0,count = 0,sv_size = 0;
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
			++count;
			free(tmp);
			pointer = NULL;
			pos += b_count;
		}else{
			int b_count = ( strlen(short_version + pos) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			v[count] = atoi(tmp);
			++count;
			free(tmp);
			pos += b_count;
		}
	}

	free(short_version);
	return count;
}

void write_pkg_data(const char *source_url,FILE *d_file,struct pkg_list *pkgs){
	int i;

	for(i=0;i < pkgs->pkg_count;i++){

		fprintf(d_file,"PACKAGE NAME:  %s-%s.tgz\n",pkgs->pkgs[i]->name,pkgs->pkgs[i]->version);
		fprintf(d_file,"PACKAGE MIRROR:  %s\n",source_url);
		fprintf(d_file,"PACKAGE LOCATION:  %s\n",pkgs->pkgs[i]->location);
		fprintf(d_file,"PACKAGE SIZE (compressed):  %d K\n",pkgs->pkgs[i]->size_c);
		fprintf(d_file,"PACKAGE SIZE (uncompressed):  %d K\n",pkgs->pkgs[i]->size_u);
		fprintf(d_file,"PACKAGE REQUIRED:  %s\n",pkgs->pkgs[i]->required);
		fprintf(d_file,"PACKAGE DESCRIPTION:\n");
		/* do we have to make up an empty description? */
		if( strlen(pkgs->pkgs[i]->description) < strlen(pkgs->pkgs[i]->name) ){
			fprintf(d_file,"%s: no description\n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
			fprintf(d_file,"%s: \n\n",pkgs->pkgs[i]->name);
		}else{
			fprintf(d_file,"%s\n",pkgs->pkgs[i]->description);
		}

	}
}

void search_pkg_list(struct pkg_list *available,struct pkg_list *matches,const char *pattern){
	int i;
	sg_regex search_regex;
	search_regex.nmatch = MAX_REGEX_PARTS;

	/* compile our regex */
	search_regex.reg_return = regcomp(&search_regex.regex, pattern, REG_EXTENDED|REG_NEWLINE);
	if( search_regex.reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, _("Failed to compile regex\n"));

		regerror_size = regerror(search_regex.reg_return, &search_regex.regex,errbuf,errbuf_size);
		if( regerror_size != 0 ){
			printf(_("Regex Error: %s\n"),errbuf);
		}
		exit(1);
	}

	for(i = 0; i < available->pkg_count; i++ ){
		if(
			/* search pkg name */
			( regexec(
				&search_regex.regex,
				available->pkgs[i]->name,
				search_regex.nmatch,
				search_regex.pmatch,
				0
			) == 0)
			||
			/* search pkg description */
			( regexec(
				&search_regex.regex,
				available->pkgs[i]->description,
				search_regex.nmatch,
				search_regex.pmatch,
				0
			) == 0)
			||
			/* search pkg location */
			( regexec(
				&search_regex.regex,
				available->pkgs[i]->location,
				search_regex.nmatch,
				search_regex.pmatch,
				0
			) == 0)
		){
			/* make alias here */
			matches->pkgs[matches->pkg_count] = available->pkgs[i];
			++matches->pkg_count;
		}
	}
	regfree(&search_regex.regex);

}

/* resolve dependencies for packages already in the current transaction */
struct pkg_list *lookup_pkg_dependencies(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg){
	int i;
	struct pkg_list *deps;
	int position = 0;
	char *pointer = NULL;
	char *buffer = NULL;
	pkg_info_t **realloc_tmp = NULL;

	deps = malloc( sizeof *deps);
	if( deps == NULL ){
		fprintf(stderr,_("Failed to malloc deps\n"));
		exit(1);
	}
	deps->pkgs = malloc( sizeof *deps->pkgs);
	if( deps->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc deps->pkgs\n"));
		exit(1);
	}
	deps->pkg_count = 0;

	/* don't go any further if the required member is empty */
	if( strcmp(pkg->required,"") == 0 || strcmp(pkg->required," ") == 0 )
		return deps;

	/* parse dep line */
	while( position < (int) strlen(pkg->required) ){
		pkg_info_t *tmp_pkg = NULL;

		/* either the last or there was only one to begin with */
		if( strstr(pkg->required + position,",") == NULL ){
			pointer = pkg->required + position;

			/* parse the dep entry and try to lookup a package */
			tmp_pkg = parse_dep_entry(avail_pkgs,installed_pkgs,pkg,pointer);

			position += strlen(pointer);
		}else{

			/* if we have a comma, skip it */
			if( pkg->required[position] == ',' ){
				++position;
				continue;
			}

			/* build the buffer to contain the dep entry */
			pointer = strchr(pkg->required + position,',');
			buffer = calloc(strlen(pkg->required + position) - strlen(pointer) +1, sizeof *buffer);
			strncpy(buffer,pkg->required + position, strlen(pkg->required + position) - strlen(pointer));
			buffer[ strlen(pkg->required + position) - strlen(pointer) ] = '\0';

			/* parse the dep entry and try to lookup a package */
			tmp_pkg = parse_dep_entry(avail_pkgs,installed_pkgs,pkg,buffer);

			position += strlen(pkg->required + position) - strlen(pointer);
			free(buffer);
		}

		/* recurse for each dep found */
		if( tmp_pkg != NULL ){
			/* if tmp_pkg is not already in the deps pkg_list */
			if( get_newest_pkg(deps,tmp_pkg->name) == NULL ){
				struct pkg_list *tmp_pkgs_deps = NULL;

				/* add tmp_pkg to deps */
				deps->pkgs[deps->pkg_count] = tmp_pkg;
				++deps->pkg_count;
				realloc_tmp = realloc(
					deps->pkgs,
					sizeof *deps->pkgs * (deps->pkg_count + 1)
				);
				if( realloc_tmp != NULL ){
					realloc_tmp = NULL;
				}/* end realloc check */

				/* now check to see if tmp_pkg has dependencies */
				tmp_pkgs_deps = lookup_pkg_dependencies(avail_pkgs,installed_pkgs,tmp_pkg);
				if( tmp_pkgs_deps->pkg_count > 0 ){

					for(i = 0; i < tmp_pkgs_deps->pkg_count; i++ ){

						/* lookup package to see if it exists in dep list */
						if( get_newest_pkg(deps,tmp_pkgs_deps->pkgs[i]->name) == NULL ){
							deps->pkgs[deps->pkg_count] = tmp_pkgs_deps->pkgs[i];
							++deps->pkg_count;
							realloc_tmp = realloc( deps->pkgs, sizeof *deps->pkgs * (deps->pkg_count + 1) );
							if( realloc_tmp != NULL ){
								deps->pkgs = realloc_tmp;
								realloc_tmp = NULL;
							}/* end realloc check */
						}/* end get_newest_pkg */

					}/* end for loop */

				}/* end tmp_pkgs_deps->pkg_count > 0 */
				free(tmp_pkgs_deps->pkgs);
				free(tmp_pkgs_deps);
			} /* end already exists in dep check */

		}else{
			/*
				if we can't find a required dep, set the dep pkg_count to -1 
				and return... the caller should check to see if its -1, and 
				act accordingly
			*/
			deps->pkg_count = -1;
			return deps;
		}/* end tmp_pkg != NULL */

	}/* end while */
	return deps;
}

pkg_info_t *parse_dep_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg,char *dep_entry){
	int i;
	sg_regex parse_dep_regex;
	char tmp_pkg_name[NAME_LEN],tmp_pkg_cond[3],tmp_pkg_ver[VERSION_LEN];
	pkg_info_t *newest_avail_pkg;
	pkg_info_t *newest_installed_pkg;

	parse_dep_regex.nmatch = MAX_REGEX_PARTS;
	regcomp(&parse_dep_regex.regex,REQUIRED_REGEX,REG_EXTENDED|REG_NEWLINE);

	/* regex to pull out pieces */
	parse_dep_regex.reg_return = regexec(
		&parse_dep_regex.regex,
		dep_entry,
		parse_dep_regex.nmatch,
		parse_dep_regex.pmatch,
		0
	);
	/* skip this line if we didn't find a package name */
	if( parse_dep_regex.reg_return != 0 ) return NULL;

	if( (parse_dep_regex.pmatch[1].rm_eo - parse_dep_regex.pmatch[1].rm_so) > NAME_LEN ){
		fprintf( stderr, _("pkg name too long [%s:%d]\n"),
			dep_entry + parse_dep_regex.pmatch[1].rm_so,
			parse_dep_regex.pmatch[1].rm_eo - parse_dep_regex.pmatch[1].rm_so
		);
		exit(1);
	}
	strncpy( tmp_pkg_name,
		dep_entry + parse_dep_regex.pmatch[1].rm_so,
		parse_dep_regex.pmatch[1].rm_eo - parse_dep_regex.pmatch[1].rm_so
	);
	tmp_pkg_name[ parse_dep_regex.pmatch[1].rm_eo - parse_dep_regex.pmatch[1].rm_so ] = '\0';

	newest_avail_pkg = get_newest_pkg(avail_pkgs,tmp_pkg_name);
	newest_installed_pkg = get_newest_pkg(installed_pkgs,tmp_pkg_name);

	/* if there is no conditional and version, return newest */
	if( (parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so) == 0 ){
		if( newest_avail_pkg != NULL ) return newest_avail_pkg;
		if( newest_installed_pkg != NULL ) return newest_installed_pkg;
	}

	if( (parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so) > 3 ){
		fprintf( stderr, _("pkg conditional too long [%s:%d]\n"),
			dep_entry + parse_dep_regex.pmatch[2].rm_so,
			parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so
		);
		exit(1);
	}
	if( (parse_dep_regex.pmatch[3].rm_eo - parse_dep_regex.pmatch[3].rm_so) > VERSION_LEN ){
		fprintf( stderr, _("pkg version too long [%s:%d]\n"),
			dep_entry + parse_dep_regex.pmatch[3].rm_so,
			parse_dep_regex.pmatch[3].rm_eo - parse_dep_regex.pmatch[3].rm_so
		);
		exit(1);
	}
	strncpy( tmp_pkg_cond,
		dep_entry + parse_dep_regex.pmatch[2].rm_so,
		parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so
	);
	tmp_pkg_cond[ parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so ] = '\0';
	strncpy( tmp_pkg_ver,
		dep_entry + parse_dep_regex.pmatch[3].rm_so,
		parse_dep_regex.pmatch[3].rm_eo - parse_dep_regex.pmatch[3].rm_so
	);
	tmp_pkg_ver[ parse_dep_regex.pmatch[3].rm_eo - parse_dep_regex.pmatch[3].rm_so ] = '\0';
	regfree(&parse_dep_regex.regex);

	/*
		* check the newest version of tmp_pkg_name (in newest_installed_pkg)
		* before we try looping through installed_pkgs
	*/
	if( newest_installed_pkg != NULL ){
		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strstr(tmp_pkg_cond,"=") != NULL)
		&& (strcmp(tmp_pkg_ver,newest_installed_pkg->version) == 0) )
			return newest_installed_pkg;
		/* if "<" */
		if( strstr(tmp_pkg_cond,"<") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,newest_installed_pkg->version) > 0 )
				return newest_installed_pkg;
		/* if ">" */
		if( strstr(tmp_pkg_cond,">") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,newest_installed_pkg->version) < 0 )
				return newest_installed_pkg;
	}
	for(i = 0; i < installed_pkgs->pkg_count; i++){
		if( strcmp(tmp_pkg_name,installed_pkgs->pkgs[i]->name) != 0 )
			continue;

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strstr(tmp_pkg_cond,"=") != NULL)
		&& (strcmp(tmp_pkg_ver,installed_pkgs->pkgs[i]->version) == 0) )
			return installed_pkgs->pkgs[i];
		/* if "<" */
		if( strstr(tmp_pkg_cond,"<") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,installed_pkgs->pkgs[i]->version) > 0 )
				return installed_pkgs->pkgs[i];
		/* if ">" */
		if( strstr(tmp_pkg_cond,">") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,installed_pkgs->pkgs[i]->version) < 0 )
				return installed_pkgs->pkgs[i];
	}

	/*
		* check the newest version of tmp_pkg_name (in newest_avail_pkg)
		* before we try looping through avail_pkgs
	*/
	if( newest_avail_pkg != NULL ){
		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strstr(tmp_pkg_cond,"=") != NULL)
		&& (strcmp(tmp_pkg_ver,newest_avail_pkg->version) == 0) )
			return newest_avail_pkg;
		/* if "<" */
		if( strstr(tmp_pkg_cond,"<") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,newest_avail_pkg->version) > 0 )
				return newest_avail_pkg;
		/* if ">" */
		if( strstr(tmp_pkg_cond,">") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,newest_avail_pkg->version) < 0 )
				return newest_avail_pkg;
	}

	/* loop through avail_pkgs */
	for(i = 0; i < avail_pkgs->pkg_count; i++){
		if( strcmp(tmp_pkg_name,avail_pkgs->pkgs[i]->name) != 0 )
			continue;

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strstr(tmp_pkg_cond,"=") != NULL)
		&& (strcmp(tmp_pkg_ver,avail_pkgs->pkgs[i]->version) == 0) )
			return avail_pkgs->pkgs[i];
		/* if "<" */
		if( strstr(tmp_pkg_cond,"<") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,avail_pkgs->pkgs[i]->version) > 0 )
				return avail_pkgs->pkgs[i];
		/* if ">" */
		if( strstr(tmp_pkg_cond,">") != NULL )
			if( cmp_pkg_versions(tmp_pkg_ver,avail_pkgs->pkgs[i]->version) < 0 )
				return avail_pkgs->pkgs[i];
	}

	fprintf(stderr,_("The following packages have unmet dependencies:\n"));
	fprintf(stderr,_("  %s: Depends: %s\n"),pkg->name,dep_entry);
	/* exit(1); */
	return NULL;
}

struct pkg_list *is_required_by(struct pkg_list *avail, pkg_info_t *pkg){
	int i;
	sg_regex required_by_reg;
	struct pkg_list *deps;
	pkg_info_t **realloc_tmp;

	deps = malloc( sizeof *deps );
	if( deps == NULL ){
		fprintf(stderr,_("Failed to malloc deps\n"));
		exit(1);
	}
	deps->pkgs = malloc( sizeof *deps->pkgs );
	if( deps->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc deps->pkgs\n"));
		exit(1);
	}
	deps->pkg_count = 0;
	required_by_reg.nmatch = MAX_REGEX_PARTS;

	regcomp(&required_by_reg.regex,pkg->name, REG_EXTENDED|REG_NEWLINE);

	for(i = 0; i < avail->pkg_count;i++){

		if( strcmp(avail->pkgs[i]->required,"") == 0 ) continue;

		if( regexec( &required_by_reg.regex, avail->pkgs[i]->required,
		required_by_reg.nmatch, required_by_reg.pmatch, 0) == 0){
			int c;
			struct pkg_list *deps_of_deps;

			deps->pkgs[deps->pkg_count] = avail->pkgs[i];
			++deps->pkg_count;
			realloc_tmp = realloc( deps->pkgs, sizeof *deps->pkgs * (deps->pkg_count + 1) );
			if( realloc_tmp != NULL ){
				deps->pkgs = realloc_tmp;
				realloc_tmp = NULL;
			}

			deps_of_deps = is_required_by(avail,avail->pkgs[i]);
			for(c = 0; c < deps_of_deps->pkg_count;c++){
				if( get_newest_pkg(deps,deps_of_deps->pkgs[c]->name) == NULL ){
					deps->pkgs[deps->pkg_count] = deps_of_deps->pkgs[c];
					++deps->pkg_count;
					realloc_tmp = realloc( deps->pkgs, sizeof *deps->pkgs * (deps->pkg_count + 1) );
					if( realloc_tmp != NULL ){
						deps->pkgs = realloc_tmp;
						realloc_tmp = NULL;
					}
				}
			}

		}
	}

	return deps;
}
