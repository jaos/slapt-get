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

#include <main.h>

/* parse the PACKAGES.TXT file */
struct pkg_list *get_available_pkgs(void){
	FILE *pkg_list_fh;
	struct pkg_list *list;

	/* open pkg list */
	pkg_list_fh = open_file(PKG_LIST_L,"r");
	if( pkg_list_fh == NULL ){
		fprintf(stderr,_("Perhaps you want to run --update?\n"));
		list = init_pkg_list();
		return list; /* return an empty list */
	}
	list = parse_packages_txt(pkg_list_fh);
	fclose(pkg_list_fh);

	return list;
}

struct pkg_list *parse_packages_txt(FILE *pkg_list_fh){
	sg_regex name_regex, mirror_regex,location_regex, size_c_regex, size_u_regex;
	ssize_t bytes_read;
	struct pkg_list *list;
	long f_pos = 0;
	size_t getline_len = 0;
	char *getline_buffer = NULL;
	char *char_pointer = NULL;

	list = init_pkg_list();

	/* compile our regexen */
	init_regex(&name_regex,PKG_NAME_PATTERN);
	init_regex(&mirror_regex,PKG_MIRROR_PATTERN);
	init_regex(&location_regex,PKG_LOCATION_PATTERN);
	init_regex(&size_c_regex,PKG_SIZEC_PATTERN);
	init_regex(&size_u_regex,PKG_SIZEU_PATTERN);

	while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh) ) != EOF ){

		pkg_info_t *tmp_pkg;

		getline_buffer[bytes_read - 1] = '\0';

		/* pull out package data */
		if( strstr(getline_buffer,"PACKAGE NAME") == NULL ) continue;

		execute_regex(&name_regex,getline_buffer);

		/* skip this line if we didn't find a package name */
		if( name_regex.reg_return != 0 ){
			fprintf(stderr,_("regex failed on [%s]\n"),getline_buffer);
			continue;
		}
		/* otherwise keep going and parse out the rest of the pkg data */

		tmp_pkg = init_pkg();

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
		tmp_pkg->mirror[0] = '\0';
		f_pos = ftell(pkg_list_fh);
		if(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF){

			execute_regex(&mirror_regex,getline_buffer);

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
		tmp_pkg->location[0] = '\0';
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) ){

			execute_regex(&location_regex,getline_buffer);

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

			char *size_c = NULL;

			execute_regex(&size_c_regex,getline_buffer);

			if( size_c_regex.reg_return == 0 ){
				size_c = calloc(
					(size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so) + 1 , sizeof *size_c 
				);
				if( size_c == NULL ){
					fprintf(stderr,_("Failed to calloc %s\n"),"size_c");
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

			char *size_u = NULL;

			execute_regex(&size_u_regex,getline_buffer);

			if( size_u_regex.reg_return == 0 ){

				size_u = calloc(
					(size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so) + 1 ,
					sizeof *size_u
				);

				if( size_u == NULL ){
					fprintf(stderr,_("Failed to calloc %s\n"),"size_u");
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
		tmp_pkg->required[0] = '\0';
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF)
			&& ((char_pointer = strstr(getline_buffer,"PACKAGE REQUIRED")) != NULL)
		){
				size_t req_len = strlen("PACKAGE REQUIRED") + 2;
				getline_buffer[bytes_read - 1] = '\0';
				strncpy(tmp_pkg->required,char_pointer + req_len, strlen(char_pointer + req_len));
				tmp_pkg->required[ strlen(char_pointer + req_len) ] = '\0';
		}else{
			/* required isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* conflicts, if provided */
		f_pos = ftell(pkg_list_fh);
		tmp_pkg->conflicts[0] = '\0';
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF)
			&& ((char_pointer = strstr(getline_buffer,"PACKAGE CONFLICTS")) != NULL)
		){
				char *conflicts = strpbrk(char_pointer,":") + 3;
				if( strlen(conflicts) > CONFLICTS_LEN ){
					fprintf( stderr, _("conflict too long [%s:%d]\n"),
						conflicts,
						strlen(conflicts)
					);
					free(tmp_pkg);
					continue;
				}
				getline_buffer[bytes_read - 1] = '\0';
				strncpy(tmp_pkg->conflicts,conflicts,strlen(conflicts));
		}else{
			/* conflicts isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* md5 checksum */
		tmp_pkg->md5[0] = '\0'; /* just in case the md5sum is empty */
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF)
			&& (strstr(getline_buffer,"PACKAGE MD5 SUM") != NULL)
		){
				char *md5sum;
				getline_buffer[bytes_read - 1] = '\0';
				md5sum = strpbrk(getline_buffer,":") + 3;
				/* don't overflow the buffer */
				if( strlen(md5sum) > MD5_STR_LEN ){
					fprintf( stderr, _("md5 sum too long [%s %s:%d]\n"),
						tmp_pkg->name,
						md5sum,
						strlen(md5sum)
					);
					free(tmp_pkg);
					continue;
				}

				strncpy( tmp_pkg->md5,md5sum,MD5_STR_LEN);
				tmp_pkg->md5[MD5_STR_LEN] = '\0';
		}else{
			/* md5 sum isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* description */
		tmp_pkg->description[0] = '\0';
		if(
			(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)
			&& (strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
		){

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

		add_pkg_to_pkg_list(list,tmp_pkg);
		tmp_pkg = NULL;
	}

	if( getline_buffer) free(getline_buffer);
	free_regex(&name_regex);
	free_regex(&mirror_regex);
	free_regex(&location_regex);
	free_regex(&size_c_regex);
	free_regex(&size_u_regex);

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
		fprintf(stderr,_("Failed to calloc %s\n"),"short_description");
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
	char *root_env_entry = NULL;
	char *pkg_log_dirname = NULL;
	struct dirent *file;
	sg_regex ip_regex, compressed_size_reg, uncompressed_size_reg;
	struct pkg_list *list;

	list = init_pkg_list();

	init_regex(&ip_regex,PKG_LOG_PATTERN);
	init_regex(&compressed_size_reg,PKG_LOG_SIZEC_PATTERN);
	init_regex(&uncompressed_size_reg,PKG_LOG_SIZEU_PATTERN);

	/* Generate package log directory using ROOT env variable if set */
	root_env_entry = getenv(ROOT_ENV_NAME);
	pkg_log_dirname = calloc(
		strlen(PKG_LOG_DIR)+
		(root_env_entry ? strlen(root_env_entry) : 0) + 1 ,
		sizeof *pkg_log_dirname
	);
	if(root_env_entry){
		strcpy(pkg_log_dirname, root_env_entry);
	}else{
		*pkg_log_dirname = '\0';
		strcat(pkg_log_dirname, PKG_LOG_DIR);
	}

	if( (pkg_log_dir = opendir(pkg_log_dirname)) == NULL ){
		if( errno ){
			perror(pkg_log_dirname);
		}
		free(pkg_log_dirname);
		return list;
	}

	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc %s\n"),"pkgs");
		exit(1);
	}

	while( (file = readdir(pkg_log_dir)) != NULL ){
		pkg_info_t *tmp_pkg;
		FILE *pkg_f;
		char *pkg_f_name;
		size_t getline_len;
		ssize_t bytes_read;
		char *getline_buffer = NULL;

		execute_regex(&ip_regex,file->d_name);

		/* skip if it doesn't match our regex */
		if( ip_regex.reg_return != 0 ) continue;

		tmp_pkg = init_pkg();

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

		/* build the package filename including the package directory */
		pkg_f_name = malloc( sizeof *pkg_f_name * (strlen(pkg_log_dirname) + strlen(file->d_name) + 2) );
		pkg_f_name[0] = '\0';
		strcat(pkg_f_name,pkg_log_dirname);
		strcat(pkg_f_name,"/");
		strcat(pkg_f_name,file->d_name);

		/* open the package log file to grok data about the package from it */
		pkg_f = open_file(pkg_f_name,"r");
		while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_f)) != EOF ){
			execute_regex(&compressed_size_reg,getline_buffer);
			execute_regex(&uncompressed_size_reg,getline_buffer);

			/* ignore unless we matched */
			if( compressed_size_reg.reg_return == 0 ){
				char *size_c;
				size_c = calloc( compressed_size_reg.pmatch[1].rm_eo - compressed_size_reg.pmatch[1].rm_so + 1, sizeof *size_c);
				if( size_c == NULL ){
					fprintf(stderr,_("Failed to calloc %s\n"),"size_c");
					exit(1);
				}

				strncpy(size_c,
					getline_buffer + compressed_size_reg.pmatch[1].rm_so,
					(compressed_size_reg.pmatch[1].rm_eo - compressed_size_reg.pmatch[1].rm_so)
				);

				tmp_pkg->size_c = strtol(size_c,(char **)NULL,10);
				free(size_c);
			}else if(uncompressed_size_reg.reg_return == 0 ){
				char *size_u;
				size_u = calloc( uncompressed_size_reg.pmatch[1].rm_eo - uncompressed_size_reg.pmatch[1].rm_so + 1, sizeof *size_u);
				if( size_u == NULL ){
					fprintf(stderr,_("Failed to calloc %s\n"),"size_u");
					exit(1);
				}

				strncpy(size_u,
					getline_buffer + uncompressed_size_reg.pmatch[1].rm_so,
					(uncompressed_size_reg.pmatch[1].rm_eo - uncompressed_size_reg.pmatch[1].rm_so)
				);

				tmp_pkg->size_u = strtol(size_u,(char **)NULL,10);
				free(size_u);
			}else{
				continue;
			}
		}
		fclose(pkg_f);

		add_pkg_to_pkg_list(list,tmp_pkg);
		tmp_pkg = NULL;

	}/* end while */
	closedir(pkg_log_dir);
	free_regex(&ip_regex);
	free(pkg_log_dirname);
	free_regex(&compressed_size_reg);
	free_regex(&uncompressed_size_reg);

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

int install_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	int cmd_return = 0;

	/* build the file name */
	pkg_file_name = gen_pkg_file_name(pkg);

	chdir(pkg->location);

	/* build and execute our command */
	command = calloc( strlen(INSTALL_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strcat(command,INSTALL_CMD);
	command = strcat(command,pkg_file_name);

	printf(_("Preparing to install %s-%s\n"),pkg->name,pkg->version);
	if( (cmd_return = system(command)) == -1 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		return -1;
	}

	chdir(global_config->working_dir);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	int cmd_return = 0;

	/* build the file name */
	pkg_file_name = gen_pkg_file_name(pkg);

	chdir(pkg->location);

	/* build and execute our command */
	command = calloc( strlen(UPGRADE_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strcat(command,UPGRADE_CMD);
	command = strcat(command,pkg_file_name);

	printf(_("Preparing to replace %s-%s with %s-%s\n"),pkg->name,installed_pkg->version,pkg->name,pkg->version);
	if( (cmd_return = system(command)) == -1 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		return -1;
	}

	chdir(global_config->working_dir);
	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int remove_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *command = NULL;
	int cmd_return;

	(void)global_config;

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
		return -1;
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
	int name_reg_ret,version_reg_ret;
	sg_regex exclude_reg;

	if( global_config->ignore_excludes == 1 )
		return 0;

	/* maybe EXCLUDE= isn't defined in our rc? */
	if( global_config->exclude_list == NULL )
		return 0;

	for(i = 0; i < global_config->exclude_list->count;i++){

		/* return if its an exact match */
		if( (strncmp(global_config->exclude_list->excludes[i],pkg->name,strlen(pkg->name)) == 0))
			return 1;

		/*
			this regex has to be init'd and free'd within the loop b/c the regex is pulled 
			from the exclude list
		*/
		init_regex(&exclude_reg,global_config->exclude_list->excludes[i]);

		execute_regex(&exclude_reg,pkg->name);
		name_reg_ret = exclude_reg.reg_return;
		execute_regex(&exclude_reg,pkg->version);
		version_reg_ret = exclude_reg.reg_return;

		if( name_reg_ret == 0 || version_reg_ret == 0 ){
			free_regex(&exclude_reg);
			return 1;
		}
		free_regex(&exclude_reg);

	}

	return 0;
}

void get_md5sum(pkg_info_t *pkg,FILE *checksum_file){
	sg_regex md5sum_regex;
	ssize_t getline_read;
	size_t getline_len = 0;
	char *getline_buffer = NULL;

	init_regex(&md5sum_regex,MD5SUM_REGEX);

	while( (getline_read = getline(&getline_buffer,&getline_len,checksum_file) ) != EOF ){

		/* ignore if it is not our package */
		if( strstr(getline_buffer,pkg->name) == NULL) continue;
		if( strstr(getline_buffer,pkg->version) == NULL) continue;
		if( strstr(getline_buffer,".tgz") == NULL) continue;
		if( strstr(getline_buffer,".asc") != NULL) continue;

		execute_regex(&md5sum_regex,getline_buffer);

		if( md5sum_regex.reg_return == 0 ){
			char sum[MD5_STR_LEN];
			char location[50];
			char name[50];
			char version[50];

			/* md5 sum */
			strncpy(
				sum,
				getline_buffer + md5sum_regex.pmatch[1].rm_so,
				md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so
			);
			sum[md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so] = '\0';

			/* location/directory */
			strncpy(
				location,
				getline_buffer + md5sum_regex.pmatch[2].rm_so,
				md5sum_regex.pmatch[2].rm_eo - md5sum_regex.pmatch[2].rm_so
			);
			location[md5sum_regex.pmatch[2].rm_eo - md5sum_regex.pmatch[2].rm_so] = '\0';

			/* pkg name */
			strncpy(
				name,
				getline_buffer + md5sum_regex.pmatch[3].rm_so,
				md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so
			);
			name[md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so] = '\0';

			/* pkg version */
			strncpy(
				version,
				getline_buffer + md5sum_regex.pmatch[4].rm_so,
				md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so
			);
			version[md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so] = '\0';

			/* see if we can match up name, version, and location */
			if( 
				(strcmp(pkg->name,name) == 0)
				&& (cmp_pkg_versions(pkg->version,version) == 0)
				&& (strcmp(pkg->location,location) == 0)
			){
				#if DEBUG == 1
				printf("%s-%s@%s, %s-%s@%s: %s\n",pkg->name,pkg->version,pkg->location,name,version,location,sum);
				#endif
				memcpy(pkg->md5,sum,md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so + 1);
				break;
			}

		}
	}
	#if DEBUG == 1
	printf("%s-%s@%s = %s\n",pkg->name,pkg->version,pkg->location,md5_sum);
	#endif
	free_regex(&md5sum_regex);
	rewind(checksum_file);

	return;
}


int cmp_pkg_versions(char *a, char *b){
	struct pkg_version_parts a_parts;
	struct pkg_version_parts b_parts;
	int position = 0;
	int greater = 1,lesser = -1;

	/* bail out early if possible */
	if( strcmp(a,b) == 0 ) return 0;

	a_parts.count = 0;
	b_parts.count = 0;

	break_down_pkg_version(&a_parts,a);
	break_down_pkg_version(&b_parts,b);

	while( position < a_parts.count && position < b_parts.count ){
		if( strcmp(a_parts.parts[position],b_parts.parts[position]) != 0 ){

			/*
			 * if the integer value of the version part is the same
			 * and the # of version parts is the same (fixes 3.8.1p1-i486-1 to 3.8p1-i486-1)
			*/ 
			if( (atoi(a_parts.parts[position]) == atoi(b_parts.parts[position]))
				&& (a_parts.count == b_parts.count) ){

				if( strcmp(a_parts.parts[position],b_parts.parts[position]) < 0 )
					return lesser;
				if( strcmp(a_parts.parts[position],b_parts.parts[position]) > 0 )
					return greater;

			}

			if( atoi(a_parts.parts[position]) < atoi(b_parts.parts[position]) )
				return lesser;

			if( atoi(a_parts.parts[position]) > atoi(b_parts.parts[position]) )
				return greater;

		}
		++position;
	}

	/*
 	 * if we got this far, we know that some or all of the version
	 * parts are equal in both packages.  If pkg-a has 3 version parts
	 * and pkg-b has 2, then we assume pkg-a to be greater.
	*/
	if(a_parts.count != b_parts.count)
		return ( (a_parts.count > b_parts.count) ? greater : lesser );

	/*
	 * Now we check to see that the version follows the standard slackware
	 * convention.  If it does, we will compare the build portions.
	 */
	/* make sure the packages have at least two separators */
	if( (index(a,'-') != rindex(a,'-')) && (index(b,'-') != rindex(b,'-')) ){
		char *a_build,*b_build;

		/* pointer to build portions */
		a_build = rindex(a,'-');
		b_build = rindex(b,'-');

		if( a_build != NULL && b_build != NULL ){
			/* they are equal if the integer values are equal */
			/* for instance, "1rob" and "1" will be equal */
			if( atoi(a_build) == atoi(b_build) ) return 0;
			if( atoi(a_build) < atoi(b_build) ) return 1;
			if( atoi(a_build) > atoi(b_build) ) return -1;
		}

	}

	/*
	 * If both have the same # of version parts, non-standard version convention,
	 * then we fall back on strcmp.
	*/
	return strcmp(a,b);
}

void break_down_pkg_version(struct pkg_version_parts *pvp,const char *version){
	int pos = 0,sv_size = 0;
	char *pointer,*tmp,*short_version;

	/* generate a short version, leave out arch and release */
	if( (pointer = strchr(version,'-')) == NULL ){
		return;
	}else{
		sv_size = ( strlen(version) - strlen(pointer) + 1);
		short_version = malloc( sizeof *short_version * sv_size );
		memcpy(short_version,version,sv_size);
		short_version[sv_size - 1] = '\0';
		pointer = NULL;
	}

	while(pos < (sv_size - 1) ){
		/* check for . as a seperator */
		if( (pointer = strchr(short_version + pos,'.')) != NULL ){
			int b_count = ( strlen(short_version + pos) - strlen(pointer) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			strncpy(pvp->parts[pvp->count],tmp,b_count);
			pvp->parts[pvp->count][b_count] = '\0';
			++pvp->count;
			free(tmp);
			pointer = NULL;
			pos += b_count;
		/* check for _ as a seperator */
		}else if( (pointer = strchr(short_version + pos,'_')) != NULL ){
			int b_count = ( strlen(short_version + pos) - strlen(pointer) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			strncpy(pvp->parts[pvp->count],tmp,b_count);
			pvp->parts[pvp->count][b_count] = '\0';
			++pvp->count;
			free(tmp);
			pointer = NULL;
			pos += b_count;
		/* must be the end of the string */
		}else{
			int b_count = ( strlen(short_version + pos) + 1 );
			tmp = malloc( sizeof *tmp * b_count );
			memcpy(tmp,short_version + pos,b_count);
			tmp[b_count - 1] = '\0';
			strncpy(pvp->parts[pvp->count],tmp,b_count);
			pvp->parts[pvp->count][b_count] = '\0';
			++pvp->count;
			free(tmp);
			pos += b_count;
		}
	}

	free(short_version);
	return;
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
		fprintf(d_file,"PACKAGE CONFLICTS:  %s\n",pkgs->pkgs[i]->conflicts);
		fprintf(d_file,"PACKAGE MD5 SUM:  %s\n",pkgs->pkgs[i]->md5);
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

struct pkg_list *search_pkg_list(struct pkg_list *available,const char *pattern){
	int i;
	int name_r,desc_r,loc_r;
	sg_regex search_regex;
	struct pkg_list *matches;

	init_regex(&search_regex,pattern);

	matches = init_pkg_list();

	for(i = 0; i < available->pkg_count; i++ ){

		execute_regex(&search_regex,available->pkgs[i]->name);
		name_r = search_regex.reg_return;
		execute_regex(&search_regex,available->pkgs[i]->description);
		desc_r = search_regex.reg_return;
		execute_regex(&search_regex,available->pkgs[i]->location);
		loc_r = search_regex.reg_return;

		/* search pkg name, pkg description, pkg location */
		if( name_r == 0 || desc_r == 0 || loc_r == 0 ){
			add_pkg_to_pkg_list(matches,available->pkgs[i]);
		}
	}
	free_regex(&search_regex);

	return matches;
}

/* lookup dependencies for pkg */
struct pkg_list *lookup_pkg_dependencies(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg){
	int i;
	struct pkg_list *deps;
	int position = 0;
	char *pointer = NULL;
	char *buffer = NULL;

	deps = init_pkg_list();

	/*
	 * don't go any further if the required member is empty
	 * or disable_dep_check is set
	*/
	if( global_config->disable_dep_check == 1
  	|| strcmp(pkg->required,"") == 0
		|| strcmp(pkg->required," ") == 0
		|| strcmp(pkg->required,"  ") == 0
	)
		return deps;

	#if DEBUG == 1
	printf("Resolving deps for %s, with dep data: %s\n",pkg->name,pkg->required);
	#endif

	/* parse dep line */
	while( position < (int) strlen(pkg->required) ){
		pkg_info_t *tmp_pkg = NULL;

		/* either the last or there was only one to begin with */
		if( strstr(pkg->required + position,",") == NULL ){
			pointer = pkg->required + position;

			/* parse the dep entry and try to lookup a package */
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pkg,pointer);

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
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pkg,buffer);

			position += strlen(pkg->required + position) - strlen(pointer);
			free(buffer);
		}

		/* recurse for each dep found */
		if( tmp_pkg != NULL ){

			/* if this pkg is excluded */
			if( is_excluded(global_config,tmp_pkg) == 1 && global_config->no_dep == 0 ){
				if( get_exact_pkg(installed_pkgs,tmp_pkg->name,tmp_pkg->version) == NULL ){
					printf(_("%s, which is required by %s, is excluded\n"),tmp_pkg->name,pkg->name);
					deps->pkg_count = -1;
					return deps;
				}
			}

			/* if tmp_pkg is not already in the deps pkg_list */
			if( get_newest_pkg(deps,tmp_pkg->name) == NULL ){
				struct pkg_list *tmp_pkgs_deps = NULL;

				/* add tmp_pkg to deps */
				add_pkg_to_pkg_list(deps,tmp_pkg);

				/* now check to see if tmp_pkg has dependencies */
				tmp_pkgs_deps = lookup_pkg_dependencies(global_config,avail_pkgs,installed_pkgs,tmp_pkg);
				if( tmp_pkgs_deps->pkg_count > 0 ){

					for(i = 0; i < tmp_pkgs_deps->pkg_count; i++ ){

						/* lookup package to see if it exists in dep list */
						if( get_newest_pkg(deps,tmp_pkgs_deps->pkgs[i]->name) == NULL ){
							add_pkg_to_pkg_list(deps,tmp_pkgs_deps->pkgs[i]);
						}/* end get_newest_pkg */

					}/* end for loop */

				}/* end tmp_pkgs_deps->pkg_count > 0 */

				/* don't call free_pkg_list as this list is made up of pointers */
				/* free_pkg_list(tmp_pkgs_deps); */
				free(tmp_pkgs_deps->pkgs);
				free(tmp_pkgs_deps);

			}else{
				#if DEBUG == 1
				printf("%s already exists in dep list\n",tmp_pkg->name);
				#endif
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

/* lookup conflicts for package */
struct pkg_list *lookup_pkg_conflicts(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg){
	struct pkg_list *conflicts;
	int position = 0;
	char *pointer = NULL;
	char *buffer = NULL;

	conflicts = init_pkg_list();

	/*
	 * don't go any further if the required member is empty
	*/
	if( strcmp(pkg->conflicts,"") == 0
		|| strcmp(pkg->conflicts," ") == 0
		|| strcmp(pkg->conflicts,"  ") == 0
	)
		return conflicts;

	/* parse conflict line */
	while( position < (int) strlen(pkg->conflicts) ){
		pkg_info_t *tmp_pkg = NULL;

		/* either the last or there was only one to begin with */
		if( strstr(pkg->conflicts + position,",") == NULL ){
			pointer = pkg->conflicts + position;

			/* parse the conflict entry and try to lookup a package */
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pkg,pointer);

			position += strlen(pointer);
		}else{

			/* if we have a comma, skip it */
			if( pkg->conflicts[position] == ',' ){
				++position;
				continue;
			}

			/* build the buffer to contain the conflict entry */
			pointer = strchr(pkg->conflicts + position,',');
			buffer = calloc(strlen(pkg->conflicts + position) - strlen(pointer) +1, sizeof *buffer);
			strncpy(buffer,pkg->conflicts + position, strlen(pkg->conflicts + position) - strlen(pointer));
			buffer[ strlen(pkg->conflicts + position) - strlen(pointer) ] = '\0';

			/* parse the conflict entry and try to lookup a package */
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pkg,buffer);

			position += strlen(pkg->conflicts + position) - strlen(pointer);
			free(buffer);
		}

		if( tmp_pkg != NULL ){
			add_pkg_to_pkg_list(conflicts,tmp_pkg);
		}

	}/* end while */

	return conflicts;
}

pkg_info_t *parse_meta_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg,char *dep_entry){
	int i;
	sg_regex parse_dep_regex;
	char tmp_pkg_name[NAME_LEN],tmp_pkg_cond[3],tmp_pkg_ver[VERSION_LEN];
	pkg_info_t *newest_avail_pkg;
	pkg_info_t *newest_installed_pkg;

	#if DEBUG == 1
	printf("parse_meta_entry called, %s-%s with %s\n",pkg->name,pkg->version,dep_entry);
	#endif

	init_regex(&parse_dep_regex,REQUIRED_REGEX);

	/* regex to pull out pieces */
	execute_regex(&parse_dep_regex,dep_entry);

	/* if the regex failed, just skip out */
	if( parse_dep_regex.reg_return != 0){
		#if DEBUG == 1
		printf("regex %s failed on %s\n",REQUIRED_REGEX,dep_entry);
		#endif
		return NULL;
	}

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
		#if DEBUG == 1
		printf("no conditional\n");
		#endif
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

	free_regex(&parse_dep_regex);

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
	return NULL;
}

struct pkg_list *is_required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg){
	int i;
	sg_regex required_by_reg;
	struct pkg_list *deps;

	#if DEBUG == 1
	printf("is_required_by called, %s-%s\n",pkg->name,pkg->version);
	#endif
	deps = init_pkg_list();

	/*
	 * don't go any further if disable_dep_check is set
	*/
	if( global_config->disable_dep_check == 1)
		return deps;

	init_regex(&required_by_reg,pkg->name);

	for(i = 0; i < avail->pkg_count;i++){

		if( strcmp(avail->pkgs[i]->required,"") == 0 ) continue;

		execute_regex(&required_by_reg,avail->pkgs[i]->required);
		if( required_by_reg.reg_return == 0 ){
			int c;
			struct pkg_list *deps_of_deps;

			add_pkg_to_pkg_list(deps,avail->pkgs[i]);

			deps_of_deps = is_required_by(global_config,avail,avail->pkgs[i]);
			for(c = 0; c < deps_of_deps->pkg_count;c++){
				if( get_newest_pkg(deps,deps_of_deps->pkgs[c]->name) == NULL ){
					add_pkg_to_pkg_list(deps,deps_of_deps->pkgs[c]);
				}
			}

		}

	}

	free_regex(&required_by_reg);
	return deps;
}

pkg_info_t *get_pkg_by_details(struct pkg_list *list,char *name,char *version,char *location){
	int i;
	for(i = 0; i < list->pkg_count; i++){

		if( strcmp(list->pkgs[i]->name,name) == 0 ){
			if( version != NULL ){
				if(strcmp(list->pkgs[i]->version,version) == 0){
					if( location != NULL ){
						if(strcmp(list->pkgs[i]->location,location) == 0){
							return list->pkgs[i];
						}
					}else{
						return list->pkgs[i];
					}
				}
			}
		}

	}
	return NULL;
}

/* update package data from mirror url */
void update_pkg_cache(const rc_config *global_config){
	int i,source_dl_failed = 0;
	FILE *pkg_list_fh_tmp;

	/* open tmp pkg list file */
	pkg_list_fh_tmp = tmpfile();

	/* go through each package source and download the meta data */
	for(i = 0; i < global_config->sources.count; i++){
		FILE *tmp_pkg_f,*tmp_patch_f,*tmp_checksum_f;
		struct pkg_list *available_pkgs = NULL;
		struct pkg_list *patch_pkgs = NULL;

		/* download our PKG_LIST */
		tmp_pkg_f = tmpfile();
		if( global_config->dl_stats == 1 ){
			printf(_("Retrieving package data [%s]...\n"),global_config->sources.url[i]);
		}else{
			printf(_("Retrieving package data [%s]..."),global_config->sources.url[i]);
		}
		if( get_mirror_data_from_source(tmp_pkg_f,global_config->dl_stats,global_config->sources.url[i],PKG_LIST) == 0 ){
			rewind(tmp_pkg_f); /* make sure we are back at the front of the file */
			available_pkgs = parse_packages_txt(tmp_pkg_f);
			if( global_config->dl_stats != 1 ) printf(_("Done\n"));
		}else{
			source_dl_failed = 1;
		}
		fclose(tmp_pkg_f);

		/* download PATCHES_LIST */
		tmp_patch_f = tmpfile();
		if( global_config->dl_stats == 1 ){
			printf(_("Retrieving patch list [%s]...\n"),global_config->sources.url[i]);
		}else{
			printf(_("Retrieving patch list [%s]..."),global_config->sources.url[i]);
		}
		if( get_mirror_data_from_source(tmp_patch_f,global_config->dl_stats,global_config->sources.url[i],PATCHES_LIST) == 0 ){
			rewind(tmp_patch_f); /* make sure we are back at the front of the file */
			patch_pkgs = parse_packages_txt(tmp_patch_f);
			if( global_config->dl_stats != 1 ) printf(_("Done\n"));
		}else{
			/* we don't care if the patch fails, for example current doesn't have patches */
			/* source_dl_failed = 1; */
		}
		fclose(tmp_patch_f);

		/* download checksum file */
		tmp_checksum_f = tmpfile();
		if( global_config->dl_stats == 1 ){
			printf(_("Retrieving checksum list [%s]...\n"),global_config->sources.url[i]);
		}else{
			printf(_("Retrieving checksum list [%s]..."),global_config->sources.url[i]);
		}
		if( get_mirror_data_from_source(tmp_checksum_f,global_config->dl_stats,global_config->sources.url[i],CHECKSUM_FILE) == 0 ){
			int a;
			if( global_config->dl_stats != 1 ) printf(_("Done\n"));

			/* now map md5 checksums to packages */
			printf(_("Reading Package Lists..."));
			rewind(tmp_checksum_f); /* make sure we are back at the front of the file */
			if( available_pkgs != NULL ){
				for(a = 0;a < available_pkgs->pkg_count;a++){
					get_md5sum(available_pkgs->pkgs[a],tmp_checksum_f);
					printf("%c\b",spinner());
				}
			}
			if( patch_pkgs != NULL ){
				for(a = 0;a < patch_pkgs->pkg_count;a++){
					get_md5sum(patch_pkgs->pkgs[a],tmp_checksum_f);
					printf("%c\b",spinner());
				}
			}
			printf(_("Done\n"));
		}else{
			source_dl_failed = 1;
		}
		fclose(tmp_checksum_f);

		/* write package listings to disk */
		if( available_pkgs != NULL ){
			write_pkg_data(global_config->sources.url[i],pkg_list_fh_tmp,available_pkgs);
			free_pkg_list(available_pkgs);
		}
		if( patch_pkgs != NULL ){
			write_pkg_data(global_config->sources.url[i],pkg_list_fh_tmp,patch_pkgs);
			free_pkg_list(patch_pkgs);
		}

	}

	/* if all our downloads where a success, write to PKG_LIST_L */
	if( source_dl_failed != 1 ){
		ssize_t bytes_read;
		size_t getline_len = 0;
		char *getline_buffer = NULL;
		FILE *pkg_list_fh;

		pkg_list_fh = open_file(PKG_LIST_L,"w+");
		rewind(pkg_list_fh_tmp);
		while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh_tmp) ) != EOF ){
			fprintf(pkg_list_fh,getline_buffer);
		}
		fclose(pkg_list_fh);

	}

	/* close the tmp pkg list file */
	fclose(pkg_list_fh_tmp);

	return;
}

struct pkg_list *init_pkg_list(void){
	struct pkg_list *list;

	list = malloc( sizeof *list );
	list->pkgs = malloc( sizeof *list->pkgs );
	if( list->pkgs == NULL ){
		fprintf(stderr,_("Failed to malloc %s\n"),"list");
		exit(1);
	}
	list->pkg_count = 0;

	return list;
}

void add_pkg_to_pkg_list(struct pkg_list *list,pkg_info_t *pkg){
	pkg_info_t **realloc_tmp;

	/* grow our struct array */
	realloc_tmp = realloc(list->pkgs , sizeof *list->pkgs * (list->pkg_count + 1) );
	if( realloc_tmp == NULL ){
		fprintf(stderr,_("Failed to realloc %s\n"),"pkgs");
		exit(1);
	}

	list->pkgs = realloc_tmp;
	list->pkgs[list->pkg_count] = pkg;
	++list->pkg_count;

}

pkg_info_t *init_pkg(void){
	pkg_info_t *pkg;

	pkg = malloc( sizeof *pkg );
	if( pkg == NULL ){
		fprintf(stderr,_("Failed to malloc %s\n"),"pkg");
		exit(1);
	}

	pkg->size_c = 0;
	pkg->size_u = 0;

	return pkg;
}

/* generate the package file name */
char *gen_pkg_file_name(pkg_info_t *pkg){
	char *file_name = NULL;

	/* build the file name */
	file_name = calloc(
		strlen(pkg->name)+strlen("-")+strlen(pkg->version)+strlen(".tgz") + 1 ,
		sizeof *file_name
	);
	if( file_name == NULL ){
		fprintf(stderr,_("Failed to calloc %s\n"),"file_name");
		exit(1);
	}
	file_name = strncpy(file_name,pkg->name,strlen(pkg->name));
	file_name[ strlen(pkg->name) ] = '\0';
	file_name = strncat(file_name,"-",strlen("-"));
	file_name = strncat(file_name,pkg->version,strlen(pkg->version));
	file_name = strncat(file_name,".tgz",strlen(".tgz"));

	return file_name;
}

/* generate the download url for a package */
char *gen_pkg_url(pkg_info_t *pkg){
	char *url = NULL;
	char *file_name = NULL;

	file_name = gen_pkg_file_name(pkg);

	url = calloc(
		strlen(pkg->mirror) + strlen(pkg->location)
			+ strlen(file_name) + strlen("/") + 1,
		sizeof *url
	);
	if( url == NULL ){
		fprintf(stderr,_("Failed to calloc %s\n"),"url");
		exit(1);
	}

	url = strncpy(url,pkg->mirror,strlen(pkg->mirror));
	url[ strlen(pkg->mirror) ] = '\0';
	url = strncat(url,pkg->location,strlen(pkg->location));
	url = strncat(url,"/",strlen("/"));
	url = strncat(url,file_name,strlen(file_name));

	free(file_name);
	return url;
}

