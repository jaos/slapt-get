/*
 * Copyright (C) 2003,2004,2005 Jason Woodward <woodwardj at jaos dot org>
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
/* analyze the pkg version hunk by hunk */
static struct pkg_version_parts *break_down_pkg_version(const char *version);
/* parse the meta lines */
static pkg_info_t *parse_meta_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,char *dep_entry);
/* called by is_required_by */
static void required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg,struct pkg_list *required_by_list);
/* free pkg_version_parts struct */
static void free_pkg_version_parts(struct pkg_version_parts *parts);
/* find dependency from "or" requirement */
static pkg_info_t *find_or_requirement(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,char *required_str);

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
		tmp_pkg->name = slapt_malloc(sizeof *tmp_pkg->name * (name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so + 1));
		strncpy(tmp_pkg->name,
			getline_buffer + name_regex.pmatch[1].rm_so,
			name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
		);
		tmp_pkg->name[
			name_regex.pmatch[1].rm_eo - name_regex.pmatch[1].rm_so
		] = '\0';

		/* pkg version */
		tmp_pkg->version = slapt_malloc(sizeof *tmp_pkg->version * (name_regex.pmatch[2].rm_eo - name_regex.pmatch[2].rm_so + 1));
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

			execute_regex(&mirror_regex,getline_buffer);

			if( mirror_regex.reg_return == 0 ){

				tmp_pkg->mirror = slapt_malloc( sizeof *tmp_pkg->mirror * (mirror_regex.pmatch[1].rm_eo - mirror_regex.pmatch[1].rm_so + 1) );
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

			execute_regex(&location_regex,getline_buffer);

			if( location_regex.reg_return == 0){

				tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location * (location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so + 1) );
				strncpy(tmp_pkg->location,
					getline_buffer + location_regex.pmatch[1].rm_so,
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				);
				tmp_pkg->location[
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				] = '\0';

				/*
					extra, testing, and pasture support
					they add in extraneous /extra/, /testing/, or /pasture/ in the
					PACKAGES.TXT location. this fixes the downloads and md5 checksum matching
				*/
				if( strstr(tmp_pkg->location,"./testing/") != NULL ){
					char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - strlen("./testing") + 2) );
					tmp_location[0] = '.';
					tmp_location[1] = '\0';
					strncat(tmp_location,&tmp_pkg->location[0] + strlen("./testing"), strlen(tmp_pkg->location) - strlen("./testing"));
					free(tmp_pkg->location);
					tmp_pkg->location = tmp_location;
				}else if( strstr(tmp_pkg->location,"./extra/") != NULL ){
					char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - strlen("./extra") + 2) );
					tmp_location[0] = '.';
					tmp_location[1] = '\0';
					strncat(tmp_location,&tmp_pkg->location[0] + strlen("./extra"), strlen(tmp_pkg->location) - strlen("./extra"));
					free(tmp_pkg->location);
					tmp_pkg->location = tmp_location;
				}else if( strstr(tmp_pkg->location,"./pasture/") != NULL ){
					char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - strlen("./pasture") + 2) );
					tmp_location[0] = '.';
					tmp_location[1] = '\0';
					strncat(tmp_location,&tmp_pkg->location[0] + strlen("./pasture"), strlen(tmp_pkg->location) - strlen("./pasture"));
					free(tmp_pkg->location);
					tmp_pkg->location = tmp_location;
				}

			}else{
				fprintf(stderr,_("regexec failed to parse location\n"));
				free_pkg(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read location\n"));
			free_pkg(tmp_pkg);
			continue;
		}

		/* size_c */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)){

			char *size_c = NULL;

			execute_regex(&size_c_regex,getline_buffer);

			if( size_c_regex.reg_return == 0 ){
				size_c = strndup(
					getline_buffer + size_c_regex.pmatch[1].rm_so,
					(size_c_regex.pmatch[1].rm_eo - size_c_regex.pmatch[1].rm_so)
				);
				tmp_pkg->size_c = strtol(size_c, (char **)NULL, 10);
				free(size_c);
			}else{
				fprintf(stderr,_("regexec failed to parse size_c\n"));
				free_pkg(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read size_c\n"));
			free_pkg(tmp_pkg);
			continue;
		}

		/* size_u */
		if( (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)){

			char *size_u = NULL;

			execute_regex(&size_u_regex,getline_buffer);

			if( size_u_regex.reg_return == 0 ){
				size_u = strndup(
					getline_buffer + size_u_regex.pmatch[1].rm_so,
					(size_u_regex.pmatch[1].rm_eo - size_u_regex.pmatch[1].rm_so)
				);
				tmp_pkg->size_u = strtol(size_u, (char **)NULL, 10);
				free(size_u);
			}else{
				fprintf(stderr,_("regexec failed to parse size_u\n"));
				free_pkg(tmp_pkg);
				continue;
			}
		}else{
			fprintf(stderr,_("getline reached EOF attempting to read size_u\n"));
			free_pkg(tmp_pkg);
			continue;
		}

		/* required, if provided */
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF) &&
			((char_pointer = strstr(getline_buffer,"PACKAGE REQUIRED")) != NULL)
		){
				char *tmp_realloc = NULL;
				size_t req_len = strlen("PACKAGE REQUIRED") + 2;
				getline_buffer[bytes_read - 1] = '\0';

				tmp_realloc = realloc(
					tmp_pkg->required,
					sizeof *tmp_pkg->required * (strlen(char_pointer + req_len) + 1)
				);
				if( tmp_realloc != NULL ){
					tmp_pkg->required = tmp_realloc;
					strncpy(tmp_pkg->required,char_pointer + req_len, strlen(char_pointer + req_len));
					tmp_pkg->required[ strlen(char_pointer + req_len) ] = '\0';
				}
		}else{
			/* required isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* conflicts, if provided */
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF) &&
			((char_pointer = strstr(getline_buffer,"PACKAGE CONFLICTS")) != NULL)
		){
				char *tmp_realloc = NULL;
				char *conflicts = (char *)strpbrk(char_pointer,":") + 3;
				getline_buffer[bytes_read - 1] = '\0';
				tmp_realloc = realloc(tmp_pkg->conflicts, sizeof *tmp_pkg->conflicts * (strlen(conflicts) + 1));
				if( tmp_realloc != NULL ){
					tmp_pkg->conflicts = tmp_realloc;
					strncat(tmp_pkg->conflicts,conflicts,strlen(conflicts));
					tmp_pkg->conflicts[ strlen(conflicts) ] = '\0';
				}
		}else{
			/* conflicts isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* suggests, if provided */
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF) &&
			((char_pointer = strstr(getline_buffer,"PACKAGE SUGGESTS")) != NULL)
		){
				char *tmp_realloc = NULL;
				char *suggests = (char *)strpbrk(char_pointer,":") + 3;
				getline_buffer[bytes_read - 1] = '\0';
				tmp_realloc = realloc(tmp_pkg->suggests, sizeof *tmp_pkg->suggests * (strlen(suggests) + 1));
				if( tmp_realloc != NULL ){
					tmp_pkg->suggests = tmp_realloc;
					strncat(tmp_pkg->suggests,suggests,strlen(suggests));
					tmp_pkg->suggests[ strlen(suggests) ] = '\0';
				}
		}else{
			/* suggests isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}
	
		/* md5 checksum */
		f_pos = ftell(pkg_list_fh);
		if(
			((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF) &&
			(strstr(getline_buffer,"PACKAGE MD5 SUM") != NULL)
		){
				char *md5sum;
				getline_buffer[bytes_read - 1] = '\0';
				md5sum = (char *)strpbrk(getline_buffer,":") + 3;
				/* don't overflow the buffer */
				if( strlen(md5sum) > MD5_STR_LEN ){
					fprintf( stderr, _("md5 sum too long\n"));
					free_pkg(tmp_pkg);
					continue;
				}

				strncpy( tmp_pkg->md5,md5sum,MD5_STR_LEN);
				tmp_pkg->md5[MD5_STR_LEN] = '\0';
		}else{
			/* md5 sum isn't provided... rewind one line */
			fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
		}

		/* description */
		if(
			(getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) &&
			(strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
		){

			while( 1 ){
				char *tmp_desc = NULL;

				if( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) == EOF ){
					break;
				}

				if( strcmp(getline_buffer,"\n") == 0){
					break;
				}

				tmp_desc = realloc(tmp_pkg->description,sizeof *tmp_pkg->description * (strlen(tmp_pkg->description) + bytes_read + 1) );
				if( tmp_desc == NULL ){
					break;
				}
				tmp_pkg->description = tmp_desc;
				strncat(tmp_pkg->description,getline_buffer,bytes_read);

			}
		}else{
			fprintf(stderr,_("error attempting to read pkg description\n"));
			free_pkg(tmp_pkg);
			continue;
		}

		/* fillin details */
		if( tmp_pkg->location == NULL ){
			tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->location[0] = '\0';
		}
		if( tmp_pkg->description == NULL ){
			tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->description[0] = '\0';
		}
		if( tmp_pkg->mirror == NULL ){
			tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->mirror[0] = '\0';
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

	list->free_pkgs = TRUE;
	return list;
}

char *gen_short_pkg_description(pkg_info_t *pkg){
	char *short_description = NULL;
	size_t string_size = 0;

	if( strchr(pkg->description,'\n') != NULL ){
		string_size = strlen(pkg->description) - (strlen(pkg->name) + 2) - strlen( strchr(pkg->description,'\n') );
	}

	/* quit now if the description is going to be empty */
	if( (int)string_size < 0 ) return NULL;

	short_description = strndup(
		pkg->description + (strlen(pkg->name) + 2),
		string_size
	);

	return short_description;
}


struct pkg_list *get_installed_pkgs(void){
	DIR *pkg_log_dir;
	char *root_env_entry = NULL;
	char *pkg_log_dirname = NULL;
	struct dirent *file;
	sg_regex ip_regex, compressed_size_reg, uncompressed_size_reg,location_regex;
	struct pkg_list *list;

	list = init_pkg_list();

	init_regex(&ip_regex,PKG_LOG_PATTERN);
	init_regex(&compressed_size_reg,PKG_LOG_SIZEC_PATTERN);
	init_regex(&uncompressed_size_reg,PKG_LOG_SIZEU_PATTERN);
	init_regex(&location_regex,PKG_LOCATION_PATTERN);

	/* Generate package log directory using ROOT env variable if set */
	if( getenv(ROOT_ENV_NAME) && strlen(getenv(ROOT_ENV_NAME)) < ROOT_ENV_LEN ){
		root_env_entry = getenv(ROOT_ENV_NAME);
	}
	pkg_log_dirname = slapt_calloc(
		strlen(PKG_LOG_DIR)+
		(root_env_entry ? strlen(root_env_entry) : 0) + 1 ,
		sizeof *pkg_log_dirname
	);
	*pkg_log_dirname = '\0';
	if(root_env_entry){
		strncpy(pkg_log_dirname, root_env_entry,strlen(root_env_entry));
		strncat(pkg_log_dirname, PKG_LOG_DIR,strlen(PKG_LOG_DIR));
	}else{
		strncat(pkg_log_dirname, PKG_LOG_DIR,strlen(PKG_LOG_DIR));
	}

	if( (pkg_log_dir = opendir(pkg_log_dirname)) == NULL ){
		if( errno ){
			perror(pkg_log_dirname);
		}
		free(pkg_log_dirname);
		return list;
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

		tmp_pkg->name = slapt_malloc(sizeof *tmp_pkg->name * (ip_regex.pmatch[1].rm_eo - ip_regex.pmatch[1].rm_so + 1) );
		strncpy(
			tmp_pkg->name,
			file->d_name + ip_regex.pmatch[1].rm_so,
			ip_regex.pmatch[1].rm_eo - ip_regex.pmatch[1].rm_so
		);
		tmp_pkg->name[ ip_regex.pmatch[1].rm_eo - ip_regex.pmatch[1].rm_so ] = '\0';

		tmp_pkg->version = slapt_malloc(sizeof *tmp_pkg->version * (ip_regex.pmatch[2].rm_eo - ip_regex.pmatch[2].rm_so + 1) );
		strncpy(
			tmp_pkg->version,
			file->d_name + ip_regex.pmatch[2].rm_so,
			ip_regex.pmatch[2].rm_eo - ip_regex.pmatch[2].rm_so
		);
		tmp_pkg->version[ ip_regex.pmatch[2].rm_eo - ip_regex.pmatch[2].rm_so ] = '\0';

		/* build the package filename including the package directory */
		pkg_f_name = slapt_malloc( sizeof *pkg_f_name * (strlen(pkg_log_dirname) + strlen(file->d_name) + 2) );
		pkg_f_name[0] = '\0';
		strncat(pkg_f_name,pkg_log_dirname,strlen(pkg_log_dirname));
		strncat(pkg_f_name,"/",strlen("/"));
		strncat(pkg_f_name,file->d_name,strlen(file->d_name));

		/* open the package log file to grok data about the package from it */
		pkg_f = open_file(pkg_f_name,"r");
		free(pkg_f_name);
		if( pkg_f == NULL ) exit(1);
		while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_f)) != EOF ){
			execute_regex(&compressed_size_reg,getline_buffer);
			execute_regex(&uncompressed_size_reg,getline_buffer);
			execute_regex(&location_regex,getline_buffer);

			/* ignore unless we matched */
			if( compressed_size_reg.reg_return == 0 ){
				char *size_c = strndup(
					getline_buffer + compressed_size_reg.pmatch[1].rm_so,
					(compressed_size_reg.pmatch[1].rm_eo - compressed_size_reg.pmatch[1].rm_so)
				);
				tmp_pkg->size_c = strtol(size_c,(char **)NULL,10);
				free(size_c);
			}else if(uncompressed_size_reg.reg_return == 0 ){
				char *size_u = strndup(
					getline_buffer + uncompressed_size_reg.pmatch[1].rm_so,
					(uncompressed_size_reg.pmatch[1].rm_eo - uncompressed_size_reg.pmatch[1].rm_so)
				);
				tmp_pkg->size_u = strtol(size_u,(char **)NULL,10);
				free(size_u);
			}else if( location_regex.reg_return == 0 ){
				tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location * (location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so + 1));
				strncpy(tmp_pkg->location,
					getline_buffer + location_regex.pmatch[1].rm_so,
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				);
				tmp_pkg->location[
					location_regex.pmatch[1].rm_eo - location_regex.pmatch[1].rm_so
				] = '\0';

			}else{
				if(strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL){
					while(1){
						char *tmp_desc = NULL;

						if((bytes_read = getline(&getline_buffer,&getline_len,pkg_f)) == EOF ) break;

						if( strstr(getline_buffer,"FILE LIST:") != NULL ) break;

						if( strcmp(getline_buffer,"\n") == 0) break;

						tmp_desc = realloc(tmp_pkg->description,sizeof *tmp_pkg->description * (strlen(tmp_pkg->description) + bytes_read + 1) );
						if( tmp_desc == NULL ){
							break;
						}
						tmp_pkg->description = tmp_desc;
						strncat(tmp_pkg->description,getline_buffer,bytes_read);

					}
				}else{
					continue;
				}
			}
		}
		if(getline_buffer) free(getline_buffer);
		fclose(pkg_f);

		/* fillin details */
		if( tmp_pkg->location == NULL ){
			tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->location[0] = '\0';
		}
		if( tmp_pkg->description == NULL ){
			tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->description[0] = '\0';
		}
		if( tmp_pkg->mirror == NULL ){
			tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->location);
			tmp_pkg->mirror[0] = '\0';
		}

		add_pkg_to_pkg_list(list,tmp_pkg);
		tmp_pkg = NULL;

	}/* end while */
	closedir(pkg_log_dir);
	free_regex(&ip_regex);
	free(pkg_log_dirname);
	free_regex(&compressed_size_reg);
	free_regex(&uncompressed_size_reg);
	free_regex(&location_regex);

	list->free_pkgs = TRUE;
	return list;
}

/* lookup newest package from pkg_list */
pkg_info_t *get_newest_pkg(struct pkg_list *pkg_list,const char *pkg_name){
	unsigned int i;
	pkg_info_t *pkg = NULL;

	for(i = 0; i < pkg_list->pkg_count; i++ ){

		/* if pkg has same name as our requested pkg */
		if( (strcmp(pkg_list->pkgs[i]->name,pkg_name)) == 0 ){
			if( (pkg == NULL) || (cmp_pkg_versions(pkg->version,
			pkg_list->pkgs[i]->version) < 0) ){
				pkg = pkg_list->pkgs[i];
			}
		}

	}

	return pkg;
}

pkg_info_t *get_exact_pkg(struct pkg_list *list,const char *name,const char *version){
	unsigned int i;
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
	pkg_file_name = gen_pkg_file_name(global_config,pkg);

	/* build and execute our command */
	command = slapt_calloc( strlen(INSTALL_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strncat(command,INSTALL_CMD,strlen(INSTALL_CMD));
	command = strncat(command,pkg_file_name,strlen(pkg_file_name));

	printf(_("Preparing to install %s-%s\n"),pkg->name,pkg->version);
	if( (cmd_return = system(command)) != 0 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		free(command);
		free(pkg_file_name);
		return -1;
	}

	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int upgrade_pkg(const rc_config *global_config,pkg_info_t *installed_pkg,pkg_info_t *pkg){
	char *pkg_file_name = NULL;
	char *command = NULL;
	int cmd_return = 0;

	/* build the file name */
	pkg_file_name = gen_pkg_file_name(global_config,pkg);

	/* build and execute our command */
	command = slapt_calloc( strlen(UPGRADE_CMD) + strlen(pkg_file_name) + 1 , sizeof *command );
	command[0] = '\0';
	command = strncat(command,UPGRADE_CMD,strlen(UPGRADE_CMD));
	command = strncat(command,pkg_file_name,strlen(pkg_file_name));

	printf(_("Preparing to replace %s-%s with %s-%s\n"),pkg->name,installed_pkg->version,pkg->name,pkg->version);
	if( (cmd_return = system(command)) != 0 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		free(command);
		free(pkg_file_name);
		return -1;
	}

	free(pkg_file_name);
	free(command);
	return cmd_return;
}

int remove_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *command = NULL;
	int cmd_return = 0;

	(void)global_config;

	/* build and execute our command */
	command = slapt_calloc(
		strlen(REMOVE_CMD) + strlen(pkg->name) + strlen("-") + strlen(pkg->version) + 1,
		sizeof *command
	);
	command[0] = '\0';
	command = strncat(command,REMOVE_CMD,strlen(REMOVE_CMD));
	command = strncat(command,pkg->name,strlen(pkg->name));
	command = strncat(command,"-",strlen("-"));
	command = strncat(command,pkg->version,strlen(pkg->version));
	if( (cmd_return = system(command)) != 0 ){
		printf(_("Failed to execute command: [%s]\n"),command);
		free(command);
		return -1;
	}

	free(command);
	return cmd_return;
}

void free_pkg(pkg_info_t *pkg){
	free(pkg->required);
	free(pkg->conflicts);
	free(pkg->suggests);
	free(pkg->name);
	free(pkg->version);
	free(pkg->mirror);
	free(pkg->location);
	free(pkg->description);
	free(pkg);
}

void free_pkg_list(struct pkg_list *list){
	unsigned int i;
	if( list->free_pkgs == TRUE ){
		for(i = 0;i < list->pkg_count;i++){
			free_pkg(list->pkgs[i]);
		}
	}
	free(list->pkgs);
	free(list);
}

int is_excluded(const rc_config *global_config,pkg_info_t *pkg){
	unsigned int i,pkg_not_excluded = 0, pkg_is_excluded = 1;
	int name_reg_ret = -1,version_reg_ret = -1,location_reg_ret = -1;
	sg_regex exclude_reg;

	if( global_config->ignore_excludes == TRUE )
		return pkg_not_excluded;

	/* maybe EXCLUDE= isn't defined in our rc? */
	if( global_config->exclude_list == NULL )
		return pkg_not_excluded;

	for(i = 0; i < global_config->exclude_list->count;i++){

		/* return if its an exact match */
		if( (strncmp(global_config->exclude_list->excludes[i],pkg->name,strlen(pkg->name)) == 0))
			return pkg_is_excluded;

		/*
			this regex has to be init'd and free'd within the loop b/c the regex is pulled 
			from the exclude list
		*/
		if( init_regex(&exclude_reg,global_config->exclude_list->excludes[i]) == -1 )
			continue;

		execute_regex(&exclude_reg,pkg->name);
		name_reg_ret = exclude_reg.reg_return;
		execute_regex(&exclude_reg,pkg->version);
		version_reg_ret = exclude_reg.reg_return;
		execute_regex(&exclude_reg,pkg->location);
		location_reg_ret = exclude_reg.reg_return;

		if( name_reg_ret == 0 || version_reg_ret == 0 || location_reg_ret == 0 ){
			free_regex(&exclude_reg);
			return pkg_is_excluded;
		}
		free_regex(&exclude_reg);

	}

	return pkg_not_excluded;
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
			char *location, *name, *version;

			/* md5 sum */
			strncpy(
				sum,
				getline_buffer + md5sum_regex.pmatch[1].rm_so,
				md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so
			);
			sum[md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so] = '\0';

			/* location/directory */
			location = slapt_malloc(sizeof *location * (md5sum_regex.pmatch[2].rm_eo - md5sum_regex.pmatch[2].rm_so + 1) );
			strncpy(
				location,
				getline_buffer + md5sum_regex.pmatch[2].rm_so,
				md5sum_regex.pmatch[2].rm_eo - md5sum_regex.pmatch[2].rm_so
			);
			location[md5sum_regex.pmatch[2].rm_eo - md5sum_regex.pmatch[2].rm_so] = '\0';

			/* pkg name */
			name = slapt_malloc(sizeof *name * (md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so + 1) );
			strncpy(
				name,
				getline_buffer + md5sum_regex.pmatch[3].rm_so,
				md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so
			);
			name[md5sum_regex.pmatch[3].rm_eo - md5sum_regex.pmatch[3].rm_so] = '\0';

			/* pkg version */
			version = slapt_malloc(sizeof *version * (md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so + 1));
			strncpy(
				version,
				getline_buffer + md5sum_regex.pmatch[4].rm_so,
				md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so
			);
			version[md5sum_regex.pmatch[4].rm_eo - md5sum_regex.pmatch[4].rm_so] = '\0';

			/* see if we can match up name, version, and location */
			if( 
				(strcmp(pkg->name,name) == 0) &&
				(cmp_pkg_versions(pkg->version,version) == 0) &&
				(strcmp(pkg->location,location) == 0)
			){
				#if DEBUG == 1
				printf("%s-%s@%s, %s-%s@%s: %s\n",pkg->name,pkg->version,pkg->location,name,version,location,sum);
				#endif
				memcpy(pkg->md5,sum,md5sum_regex.pmatch[1].rm_eo - md5sum_regex.pmatch[1].rm_so + 1);
				free(name);
				free(version);
				free(location);
				break;
			}

			free(name);
			free(version);
			free(location);
		}
	}
	if( getline_buffer ) free(getline_buffer);
	#if DEBUG == 1
	printf("%s-%s@%s = %s\n",pkg->name,pkg->version,pkg->location,pkg->md5);
	#endif
	free_regex(&md5sum_regex);
	rewind(checksum_file);

	return;
}

static void free_pkg_version_parts(struct pkg_version_parts *parts){
	unsigned int i;
	for(i = 0;i < parts->count;i++){
		free(parts->parts[i]);
	}
	free(parts->parts);
	free(parts);
}

int cmp_pkg_versions(char *a, char *b){
	unsigned int position = 0;
	int greater = 1,lesser = -1,equal = 0;
	struct pkg_version_parts *a_parts;
	struct pkg_version_parts *b_parts;

	/* bail out early if possible */
	if( strcmp(a,b) == 0 ) return equal;

	a_parts = break_down_pkg_version(a);
	b_parts = break_down_pkg_version(b);

	while( position < a_parts->count && position < b_parts->count ){
		if( strcmp(a_parts->parts[position],b_parts->parts[position]) != 0 ){

			/*
			 * if the integer value of the version part is the same
			 * and the # of version parts is the same (fixes 3.8.1p1-i486-1 to 3.8p1-i486-1)
			*/ 
			if( (atoi(a_parts->parts[position]) == atoi(b_parts->parts[position])) &&
				(a_parts->count == b_parts->count) ){

				if( strcmp(a_parts->parts[position],b_parts->parts[position]) < 0 ){
					free_pkg_version_parts(a_parts);
					free_pkg_version_parts(b_parts);
					return lesser;
				}
				if( strcmp(a_parts->parts[position],b_parts->parts[position]) > 0 ){
					free_pkg_version_parts(a_parts);
					free_pkg_version_parts(b_parts);
					return greater;
				}

			}

			if( atoi(a_parts->parts[position]) < atoi(b_parts->parts[position]) ){
				free_pkg_version_parts(a_parts);
				free_pkg_version_parts(b_parts);
				return lesser;
			}

			if( atoi(a_parts->parts[position]) > atoi(b_parts->parts[position]) ){
				free_pkg_version_parts(a_parts);
				free_pkg_version_parts(b_parts);
				return greater;
			}

		}
		++position;
	}

	/*
 	 * if we got this far, we know that some or all of the version
	 * parts are equal in both packages.  If pkg-a has 3 version parts
	 * and pkg-b has 2, then we assume pkg-a to be greater.
	*/
	if(a_parts->count != b_parts->count){
		if( a_parts->count > b_parts->count ){
			free_pkg_version_parts(a_parts);
			free_pkg_version_parts(b_parts);
			return greater;
		}else{
			free_pkg_version_parts(a_parts);
			free_pkg_version_parts(b_parts);
			return lesser;
		}
	}

	free_pkg_version_parts(a_parts);
	free_pkg_version_parts(b_parts);

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
			if( atoi(a_build) == atoi(b_build) ) return equal;
			if( atoi(a_build) < atoi(b_build) ) return greater;
			if( atoi(a_build) > atoi(b_build) ) return lesser;
		}

	}

	/*
	 * If both have the same # of version parts, non-standard version convention,
	 * then we fall back on strcmp.
	*/
	if( strchr(a,'-') == NULL && strchr(b,'-') == NULL )
		return strcmp(a,b);

	return equal;
}

static struct pkg_version_parts *break_down_pkg_version(const char *version){
	int pos = 0,sv_size = 0;
	char *pointer,*short_version;
	struct pkg_version_parts *pvp;

	pvp = slapt_malloc( sizeof *pvp );
	pvp->parts = slapt_malloc( sizeof *pvp->parts );
	pvp->count = 0;

	/* generate a short version, leave out arch and release */
	if( (pointer = strchr(version,'-')) == NULL ){
		sv_size = strlen(version) + 1;
		short_version = slapt_malloc( sizeof *short_version * sv_size );
		memcpy(short_version,version,sv_size);
		short_version[sv_size - 1] = '\0';
	}else{
		sv_size = ( strlen(version) - strlen(pointer) + 1);
		short_version = slapt_malloc( sizeof *short_version * sv_size );
		memcpy(short_version,version,sv_size);
		short_version[sv_size - 1] = '\0';
		pointer = NULL;
	}

	while(pos < (sv_size - 1) ){
		char **tmp;

		tmp = realloc(pvp->parts, sizeof *pvp->parts * (pvp->count + 1) );
		if( tmp == NULL ){
			fprintf(stderr,_("Failed to realloc %s\n"),"pvp->parts");
			exit(1);
		}
		pvp->parts = tmp;

		/* check for . as a seperator */
		if( (pointer = strchr(short_version + pos,'.')) != NULL ){
			int b_count = ( strlen(short_version + pos) - strlen(pointer) + 1 );
			pvp->parts[pvp->count] = slapt_malloc( sizeof *pvp->parts[pvp->count] * b_count );
			memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
			pvp->parts[pvp->count][b_count - 1] = '\0';
			++pvp->count;
			pointer = NULL;
			pos += b_count;
		/* check for _ as a seperator */
		}else if( (pointer = strchr(short_version + pos,'_')) != NULL ){
			int b_count = ( strlen(short_version + pos) - strlen(pointer) + 1 );
			pvp->parts[pvp->count] = slapt_malloc( sizeof *pvp->parts[pvp->count] * b_count );
			memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
			pvp->parts[pvp->count][b_count - 1] = '\0';
			++pvp->count;
			pointer = NULL;
			pos += b_count;
		/* must be the end of the string */
		}else{
			int b_count = ( strlen(short_version + pos) + 1 );
			pvp->parts[pvp->count] = slapt_malloc( sizeof *pvp->parts[pvp->count] * b_count );
			memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
			pvp->parts[pvp->count][b_count - 1] = '\0';
			++pvp->count;
			pos += b_count;
		}
	}

	free(short_version);
	return pvp;
}

void write_pkg_data(const char *source_url,FILE *d_file,struct pkg_list *pkgs){
	unsigned int i;

	for(i=0;i < pkgs->pkg_count;i++){

		fprintf(d_file,"PACKAGE NAME:  %s-%s.tgz\n",pkgs->pkgs[i]->name,pkgs->pkgs[i]->version);
		if( strlen(pkgs->pkgs[i]->mirror) > 0 ){
			fprintf(d_file,"PACKAGE MIRROR:  %s\n",pkgs->pkgs[i]->mirror);
		}else{
			fprintf(d_file,"PACKAGE MIRROR:  %s\n",source_url);
		}
		fprintf(d_file,"PACKAGE LOCATION:  %s\n",pkgs->pkgs[i]->location);
		fprintf(d_file,"PACKAGE SIZE (compressed):  %d K\n",pkgs->pkgs[i]->size_c);
		fprintf(d_file,"PACKAGE SIZE (uncompressed):  %d K\n",pkgs->pkgs[i]->size_u);
		fprintf(d_file,"PACKAGE REQUIRED:  %s\n",pkgs->pkgs[i]->required);
		fprintf(d_file,"PACKAGE CONFLICTS:  %s\n",pkgs->pkgs[i]->conflicts);
		fprintf(d_file,"PACKAGE SUGGESTS:  %s\n",pkgs->pkgs[i]->suggests);
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
	unsigned int i;
	int name_r = -1,desc_r = -1,loc_r = -1,version_r = -1;
	sg_regex search_regex;
	struct pkg_list *matches;

	matches = init_pkg_list();

	if( init_regex(&search_regex,pattern) == -1 )
		return matches;

	for(i = 0; i < available->pkg_count; i++ ){

		execute_regex(&search_regex,available->pkgs[i]->name);
		name_r = search_regex.reg_return;

		execute_regex(&search_regex,available->pkgs[i]->version);
		version_r = search_regex.reg_return;

		if( available->pkgs[i]->description != NULL ){
			execute_regex(&search_regex,available->pkgs[i]->description);
			desc_r = search_regex.reg_return;
		}

		if( available->pkgs[i]->location != NULL ){
			execute_regex(&search_regex,available->pkgs[i]->location);
			loc_r = search_regex.reg_return;
		}

		/* search pkg name, pkg description, pkg location */
		if( name_r == 0 || version_r == 0 || desc_r == 0 || loc_r == 0 ){
			add_pkg_to_pkg_list(matches,available->pkgs[i]);
		}
	}
	free_regex(&search_regex);

	return matches;
}

/* lookup dependencies for pkg */
int get_pkg_dependencies(const rc_config *global_config,struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg,struct pkg_list *deps){
	int position = 0, len = 0;
	char *pointer = NULL;
	char *buffer = NULL;

	/*
	 * don't go any further if the required member is empty
	 * or disable_dep_check is set
	*/
	if( global_config->disable_dep_check == TRUE ||
  	strcmp(pkg->required,"") == 0 ||
		strcmp(pkg->required," ") == 0 ||
		strcmp(pkg->required,"  ") == 0
	)
		return 1;

	#if DEBUG == 1
	printf("Resolving deps for %s, with dep data: %s\n",pkg->name,pkg->required);
	#endif

	/* parse dep line */
	len = strlen(pkg->required);
	while( position < len ){
		pkg_info_t *tmp_pkg = NULL;

		/* either the last or there was only one to begin with */
		if( strstr(pkg->required + position,",") == NULL ){
			pointer = pkg->required + position;

			/* parse the dep entry and try to lookup a package */
			if( strchr(pointer,'|') != NULL ){
				tmp_pkg = find_or_requirement(avail_pkgs,installed_pkgs,pointer);
			}else{
				tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pointer);
			}

			if( tmp_pkg == NULL ){
				/*
					if we can't find a required dep, set the dep pkg_count to -1 
					and return... the caller should check to see if its -1, and 
					act accordingly
				*/
				fprintf(stderr,_("The following packages have unmet dependencies:\n"));
				fprintf(stderr,_("  %s: Depends: %s\n"),pkg->name,pointer);
				return -1;
			}

			position += strlen(pointer);

		}else{

			/* if we have a comma, skip it */
			if( pkg->required[position] == ',' ){
				++position;
				continue;
			}

			/* build the buffer to contain the dep entry */
			pointer = strchr(pkg->required + position,',');
			buffer = strndup(
				pkg->required + position,
				strlen(pkg->required + position) - strlen(pointer)
			);

			/* parse the dep entry and try to lookup a package */
			if( strchr(buffer,'|') != NULL ){
				tmp_pkg = find_or_requirement(avail_pkgs,installed_pkgs,buffer);
			}else{
				tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,buffer);
			}

			if( tmp_pkg == NULL ){
				/* if we can't find a required dep, stop */
				fprintf(stderr,_("The following packages have unmet dependencies:\n"));
				fprintf(stderr,_("  %s: Depends: %s\n"),pkg->name,buffer);
				free(buffer);
				return -1;
			}

			position += strlen(pkg->required + position) - strlen(pointer);
			free(buffer);
		}

		/* if this pkg is excluded */
		if( (is_excluded(global_config,tmp_pkg) == 1) && (global_config->ignore_dep == FALSE) ){
			if( get_exact_pkg(installed_pkgs,tmp_pkg->name,tmp_pkg->version) == NULL ){
				printf(_("%s, which is required by %s, is excluded\n"),tmp_pkg->name,pkg->name);
				return -1;
			}
		}

		/* if tmp_pkg is not already in the deps pkg_list */
		if( (get_newest_pkg(deps,tmp_pkg->name) == NULL) ){
			int dep_check_return;

			/* add tmp_pkg to deps so that we don't needlessly recurse */
			add_pkg_to_pkg_list(deps,tmp_pkg);

			/* now check to see if tmp_pkg has dependencies */
			dep_check_return = get_pkg_dependencies(global_config,avail_pkgs,installed_pkgs,tmp_pkg,deps);
			if( dep_check_return == -1 && global_config->ignore_dep == FALSE ){
				return -1;
			}else{
				/* now move the package to the end after it's dependencies */
				pkg_info_t *tmp = NULL;
				unsigned int i = 0;
				while(i < deps->pkg_count){
					if( strcmp(deps->pkgs[i]->name,tmp_pkg->name) == 0 && tmp == NULL )
						tmp = deps->pkgs[i];
					/* move all subsequent packages up */
					if( tmp != NULL && (i+1 < deps->pkg_count) )
						deps->pkgs[i] = deps->pkgs[i + 1];
					++i;
				}
				/*
					* now put the pkg we found at the end...
					* note no resizing is necessary, we just moved the location
				*/
				if( tmp != NULL ) deps->pkgs[deps->pkg_count - 1] = tmp;
			}

		#if DEBUG == 1
		}else{
			printf("%s already exists in dep list\n",tmp_pkg->name);
		#endif
		} /* end already exists in dep check */


	}/* end while */
	return 0;
}

/* lookup conflicts for package */
struct pkg_list *get_pkg_conflicts(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,pkg_info_t *pkg){
	struct pkg_list *conflicts;
	int position = 0,len = 0;
	char *pointer = NULL;
	char *buffer = NULL;

	conflicts = init_pkg_list();

	/*
	 * don't go any further if the required member is empty
	*/
	if( strcmp(pkg->conflicts,"") == 0 ||
		strcmp(pkg->conflicts," ") == 0 ||
		strcmp(pkg->conflicts,"  ") == 0
	)
		return conflicts;

	/* parse conflict line */
	len = strlen(pkg->conflicts);
	while( position < len ){
		pkg_info_t *tmp_pkg = NULL;

		/* either the last or there was only one to begin with */
		if( strstr(pkg->conflicts + position,",") == NULL ){
			pointer = pkg->conflicts + position;

			/* parse the conflict entry and try to lookup a package */
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pointer);

			position += strlen(pointer);
		}else{

			/* if we have a comma, skip it */
			if( pkg->conflicts[position] == ',' ){
				++position;
				continue;
			}

			/* build the buffer to contain the conflict entry */
			pointer = strchr(pkg->conflicts + position,',');
			buffer = strndup(
				pkg->conflicts + position,
				strlen(pkg->conflicts + position) - strlen(pointer)
			);

			/* parse the conflict entry and try to lookup a package */
			tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,buffer);

			position += strlen(pkg->conflicts + position) - strlen(pointer);
			free(buffer);
		}

		if( tmp_pkg != NULL ){
			add_pkg_to_pkg_list(conflicts,tmp_pkg);
		}

	}/* end while */

	return conflicts;
}

static pkg_info_t *parse_meta_entry(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,char *dep_entry){
	unsigned int i;
	sg_regex parse_dep_regex;
	char *tmp_pkg_name = NULL,*tmp_pkg_ver = NULL;
	char tmp_pkg_cond[3];
	pkg_info_t *newest_avail_pkg;
	pkg_info_t *newest_installed_pkg;
	int tmp_name_len =0, tmp_ver_len = 0, tmp_cond_len = 0;

	init_regex(&parse_dep_regex,REQUIRED_REGEX);

	/* regex to pull out pieces */
	execute_regex(&parse_dep_regex,dep_entry);

	/* if the regex failed, just skip out */
	if( parse_dep_regex.reg_return != 0){
		#if DEBUG == 1
		printf("regex %s failed on %s\n",REQUIRED_REGEX,dep_entry);
		#endif
		free_regex(&parse_dep_regex);
		return NULL;
	}

	tmp_name_len = parse_dep_regex.pmatch[1].rm_eo - parse_dep_regex.pmatch[1].rm_so;
	tmp_cond_len = parse_dep_regex.pmatch[2].rm_eo - parse_dep_regex.pmatch[2].rm_so;
	tmp_ver_len =	 parse_dep_regex.pmatch[3].rm_eo - parse_dep_regex.pmatch[3].rm_so;

	tmp_pkg_name = slapt_malloc( sizeof *tmp_pkg_name * (tmp_name_len + 1));
	strncpy( tmp_pkg_name,
		dep_entry + parse_dep_regex.pmatch[1].rm_so,
		tmp_name_len
	);
	tmp_pkg_name[ tmp_name_len ] = '\0';

	newest_avail_pkg = get_newest_pkg(avail_pkgs,tmp_pkg_name);
	newest_installed_pkg = get_newest_pkg(installed_pkgs,tmp_pkg_name);

	/* if there is no conditional and version, return newest */
	if( tmp_cond_len == 0 ){
		#if DEBUG == 1
		printf("no conditional\n");
		#endif
		if( newest_installed_pkg != NULL ){
			free_regex(&parse_dep_regex);
			free(tmp_pkg_name);
			return newest_installed_pkg;
		}
		if( newest_avail_pkg != NULL ){
			free_regex(&parse_dep_regex);
			free(tmp_pkg_name);
			return newest_avail_pkg;
		}
	}

	if( tmp_cond_len > 3 ){
		fprintf( stderr, _("pkg conditional too long\n"));
		free_regex(&parse_dep_regex);
		free(tmp_pkg_name);
		return NULL;
	}

	strncpy( tmp_pkg_cond,
		dep_entry + parse_dep_regex.pmatch[2].rm_so,
		tmp_cond_len
	);
	tmp_pkg_cond[ tmp_cond_len ] = '\0';

	tmp_pkg_ver = slapt_malloc( sizeof *tmp_pkg_ver * (tmp_ver_len + 1));
	strncpy( tmp_pkg_ver,
		dep_entry + parse_dep_regex.pmatch[3].rm_so,
		tmp_ver_len
	);
	tmp_pkg_ver[ tmp_ver_len ] = '\0';

	free_regex(&parse_dep_regex);

	/*
		* check the newest version of tmp_pkg_name (in newest_installed_pkg)
		* before we try looping through installed_pkgs
	*/
	if( newest_installed_pkg != NULL ){

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strchr(tmp_pkg_cond,'=') != NULL) &&
		(cmp_pkg_versions(tmp_pkg_ver,newest_installed_pkg->version) == 0) ){
			free(tmp_pkg_name);
			free(tmp_pkg_ver);
			return newest_installed_pkg;
		}

		/* if "<" */
		if( strchr(tmp_pkg_cond,'<') != NULL ){
			if( cmp_pkg_versions(newest_installed_pkg->version,tmp_pkg_ver) < 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return newest_installed_pkg;
			}
		}

		/* if ">" */
		if( strchr(tmp_pkg_cond,'>') != NULL ){
			if( cmp_pkg_versions(newest_installed_pkg->version,tmp_pkg_ver) > 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return newest_installed_pkg;
			}
		}

	}

	for(i = 0; i < installed_pkgs->pkg_count; i++){

		if( strcmp(tmp_pkg_name,installed_pkgs->pkgs[i]->name) != 0 )
			continue;

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strchr(tmp_pkg_cond,'=') != NULL) &&
		(cmp_pkg_versions(tmp_pkg_ver,installed_pkgs->pkgs[i]->version) == 0) ){
			free(tmp_pkg_name);
			free(tmp_pkg_ver);
			return installed_pkgs->pkgs[i];
		}

		/* if "<" */
		if( strchr(tmp_pkg_cond,'<') != NULL ){
			if( cmp_pkg_versions(installed_pkgs->pkgs[i]->version,tmp_pkg_ver) < 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return installed_pkgs->pkgs[i];
			}
		}

		/* if ">" */
		if( strchr(tmp_pkg_cond,'>') != NULL ){
			if( cmp_pkg_versions(installed_pkgs->pkgs[i]->version,tmp_pkg_ver) > 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return installed_pkgs->pkgs[i];
			}
		}

	}

	/*
		* check the newest version of tmp_pkg_name (in newest_avail_pkg)
		* before we try looping through avail_pkgs
	*/
	if( newest_avail_pkg != NULL ){

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strchr(tmp_pkg_cond,'=') != NULL) &&
		(cmp_pkg_versions(tmp_pkg_ver,newest_avail_pkg->version) == 0) ){
			free(tmp_pkg_name);
			free(tmp_pkg_ver);
			return newest_avail_pkg;
		}

		/* if "<" */
		if( strchr(tmp_pkg_cond,'<') != NULL ){
			if( cmp_pkg_versions(newest_avail_pkg->version,tmp_pkg_ver) < 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return newest_avail_pkg;
			}
		}

		/* if ">" */
		if( strchr(tmp_pkg_cond,'>') != NULL ){
			if( cmp_pkg_versions(newest_avail_pkg->version,tmp_pkg_ver) > 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return newest_avail_pkg;
			}
		}

	}

	/* loop through avail_pkgs */
	for(i = 0; i < avail_pkgs->pkg_count; i++){

		if( strcmp(tmp_pkg_name,avail_pkgs->pkgs[i]->name) != 0 )
			continue;

		/* if condition is "=",">=", or "=<" and versions are the same */
		if( (strchr(tmp_pkg_cond,'=') != NULL) &&
		(cmp_pkg_versions(tmp_pkg_ver,avail_pkgs->pkgs[i]->version) == 0) ){
			free(tmp_pkg_name);
			free(tmp_pkg_ver);
			return avail_pkgs->pkgs[i];
		}

		/* if "<" */
		if( strchr(tmp_pkg_cond,'<') != NULL ){
			if( cmp_pkg_versions(avail_pkgs->pkgs[i]->version,tmp_pkg_ver) < 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return avail_pkgs->pkgs[i];
			}
		}

		/* if ">" */
		if( strchr(tmp_pkg_cond,'>') != NULL ){
			if( cmp_pkg_versions(avail_pkgs->pkgs[i]->version,tmp_pkg_ver) > 0 ){
				free(tmp_pkg_name);
				free(tmp_pkg_ver);
				return avail_pkgs->pkgs[i];
			}
		}

	}

	free(tmp_pkg_name);
	free(tmp_pkg_ver);

	return NULL;
}

struct pkg_list *is_required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg){
	struct pkg_list *required_by_list;

	required_by_list = init_pkg_list();

	required_by(global_config,avail,pkg,required_by_list);

	return required_by_list;
}

static void required_by(const rc_config *global_config,struct pkg_list *avail, pkg_info_t *pkg,struct pkg_list *required_by_list){
	unsigned int i;
	sg_regex required_by_reg;
	char *escapedName = NULL, *escaped_ptr;

	/*
	 * don't go any further if disable_dep_check is set
	*/
	if( global_config->disable_dep_check == TRUE) return;

	escapedName = slapt_malloc(sizeof *escapedName * (strlen(pkg->name) + 1) );

	for(i = 0, escaped_ptr = escapedName; i < strlen(pkg->name) && pkg->name[i]; i++){
		if( pkg->name[i] == '+' ){
			*escaped_ptr++ = '\\';
			*escaped_ptr++ = pkg->name[i];
		}else{
			*escaped_ptr++ = pkg->name[i];
		}
		*escaped_ptr = '\0';
	}

	init_regex(&required_by_reg,escapedName);

	for(i = 0; i < avail->pkg_count;i++){

		if( strcmp(avail->pkgs[i]->required,"") == 0 ) continue;

		execute_regex(&required_by_reg,avail->pkgs[i]->required);
		if( required_by_reg.reg_return != 0 ) continue;

		add_pkg_to_pkg_list(required_by_list,avail->pkgs[i]);
	}

	free(escapedName);
	free_regex(&required_by_reg);
}

pkg_info_t *get_pkg_by_details(struct pkg_list *list,char *name,char *version,char *location){
	unsigned int i;
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

/* do a head request on the mirror data to find out if it's new */
char *head_mirror_data(const char *wurl,const char *file){
	char *request_header = NULL;
	char *request_header_ptr = NULL;
	char *delim_ptr = NULL;
	char *head_data = NULL;
	int request_header_len = 0;
	char *url;

	/* build url */
	url = slapt_calloc( strlen(wurl) + strlen(file) + 2, sizeof *url );
	url[0] = '\0';
	strncat(url,wurl,strlen(wurl));
	strncat(url,"/",1);
	strncat(url,file,strlen(file));

	/* retrieve the header info */
	head_data = head_request(url);
	free(url);
	if( head_data == NULL ){
		return NULL;
	}

	/* extract the last modified date for storage and later comparison */
	request_header_ptr = strstr(head_data,"Last-Modified");
	if( request_header_ptr == NULL ){
		/* this is ftp, in which case the Content-Length will have to do */
		request_header_ptr = strstr(head_data,"Content-Length");
		if( request_header_ptr == NULL ){
			free(head_data);
			return NULL;
		}/* give up finally */
	}
	delim_ptr = strpbrk(request_header_ptr,"\r\n");
	if( delim_ptr == NULL ){
		free(head_data);
		return NULL;
	}
	
	request_header_len = strlen(request_header_ptr) - strlen(delim_ptr);
	request_header = slapt_calloc( request_header_len + 1, sizeof *request_header );
	memcpy(request_header,request_header_ptr,request_header_len);

	free(head_data);
	return request_header;
}

void write_head_cache(const char *cache, const char *cache_filename){
	char *head_filename;
	FILE *tmp;

	head_filename = gen_head_cache_filename(cache_filename);

	/* store the last modified date */
	if( (tmp = open_file(head_filename,"w")) == NULL ) exit(1);
	fprintf(tmp,"%s",cache);
	fclose(tmp);

	free(head_filename);

}

char *read_head_cache(const char *cache_filename){
	char *head_filename;
	FILE *tmp;
	char *getline_buffer = NULL;
	size_t gl_n;
	ssize_t gl_return_size;

	head_filename = gen_head_cache_filename(cache_filename);

	tmp = open_file(head_filename,"a+");
	free(head_filename);
	if( tmp == NULL ) exit(1);
	rewind(tmp);
	gl_return_size = getline(&getline_buffer, &gl_n, tmp);
	fclose(tmp);

	if( gl_return_size == -1 ){
		free(getline_buffer);
		return NULL;
	}

	return getline_buffer;
}

char *gen_head_cache_filename(const char *filename_from_url){
	char *head_filename;

	head_filename = slapt_calloc( strlen(filename_from_url) + strlen(HEAD_FILE_EXT) + 1, sizeof *head_filename );
	strncat(head_filename,filename_from_url,strlen(filename_from_url));
	strncat(head_filename,HEAD_FILE_EXT,strlen(HEAD_FILE_EXT));

	return head_filename;
}


void clear_head_cache(const char *cache_filename){
	char *head_filename;
	FILE *tmp;

	head_filename = gen_head_cache_filename(cache_filename);

	tmp = open_file(head_filename,"w");
	if( tmp == NULL ) exit(1);
	fclose(tmp);
	free(head_filename);

}

/* update package data from mirror url */
int update_pkg_cache(const rc_config *global_config){
	unsigned int i,source_dl_failed = 0;
	FILE *pkg_list_fh_tmp = NULL;

	/* open tmp pkg list file */
	pkg_list_fh_tmp = tmpfile();
	if( pkg_list_fh_tmp == NULL ){
		if( errno ) perror("tmpfile");
		exit(1);
	}

	/* go through each package source and download the meta data */
	for(i = 0; i < global_config->sources->count; i++){
		FILE *tmp_pkg_f,*tmp_patch_f,*tmp_checksum_f;
		struct pkg_list *available_pkgs = NULL;
		struct pkg_list *patch_pkgs = NULL;
		char *pkg_filename,*patch_filename,*checksum_filename;
		char *pkg_head,*pkg_local_head;
		char *patch_head,*patch_local_head;
		char *checksum_head,*checksum_local_head;


		/* download our PKG_LIST */
		printf(_("Retrieving package data [%s]..."),global_config->sources->url[i]);
		pkg_filename = gen_filename_from_url(global_config->sources->url[i],PKG_LIST);
		pkg_head = head_mirror_data(global_config->sources->url[i],PKG_LIST);
		pkg_local_head = read_head_cache(pkg_filename);

		/* open for reading if cached, otherwise write it from the downloaded data */
		if( pkg_head != NULL && pkg_local_head != NULL && strcmp(pkg_head,pkg_local_head) == 0){
			printf(_("Cached\n"));
			if( (tmp_pkg_f = open_file(pkg_filename,"r")) == NULL ) exit(1);
			available_pkgs = parse_packages_txt(tmp_pkg_f);
		}else{
			if( global_config->dl_stats == TRUE ) printf("\n");
			if( (tmp_pkg_f = open_file(pkg_filename,"w+b")) == NULL ) exit(1);
			if( get_mirror_data_from_source(tmp_pkg_f,global_config,global_config->sources->url[i],PKG_LIST) == 0 ){
				rewind(tmp_pkg_f); /* make sure we are back at the front of the file */
				available_pkgs = parse_packages_txt(tmp_pkg_f);
				if( global_config->dl_stats == FALSE ) printf(_("Done\n"));
			}else{
				source_dl_failed = 1;
				clear_head_cache(pkg_filename);
			}
		}
		if( available_pkgs == NULL || available_pkgs->pkg_count < 1 ){
			source_dl_failed = 1;
			clear_head_cache(pkg_filename);
		}
		/* if all is good, write it */
		if( source_dl_failed != 1 && pkg_head != NULL ) write_head_cache(pkg_head,pkg_filename);
		free(pkg_head);
		free(pkg_local_head);
		free(pkg_filename);
		fclose(tmp_pkg_f);


		/* download PATCHES_LIST */
		printf(_("Retrieving patch list [%s]..."),global_config->sources->url[i]);
		patch_filename = gen_filename_from_url(global_config->sources->url[i],PATCHES_LIST);
		patch_head = head_mirror_data(global_config->sources->url[i],PATCHES_LIST);
		patch_local_head = read_head_cache(patch_filename);

		/* open for reading if cached, otherwise write it from the downloaded data */
		if( patch_head != NULL && patch_local_head != NULL && strcmp(patch_head,patch_local_head) == 0){
			printf(_("Cached\n"));
			if( (tmp_patch_f = open_file(patch_filename,"r")) == NULL ) exit(1);
			patch_pkgs = parse_packages_txt(tmp_patch_f);
		}else{
			if( global_config->dl_stats == TRUE ) printf("\n");
			if( (tmp_patch_f = open_file(patch_filename,"w+b")) == NULL ) exit (1);
			if( get_mirror_data_from_source(tmp_patch_f,global_config,global_config->sources->url[i],PATCHES_LIST) == 0 ){
				rewind(tmp_patch_f); /* make sure we are back at the front of the file */
				patch_pkgs = parse_packages_txt(tmp_patch_f);
				if( global_config->dl_stats == FALSE ) printf(_("Done\n"));
			}else{
				/* we don't care if the patch fails, for example current doesn't have patches */
				/* source_dl_failed = 1; */
				clear_head_cache(patch_filename);
			}
		}
		/* if all is good, write it */
		if( source_dl_failed != 1 && patch_head != NULL ) write_head_cache(patch_head,patch_filename);
		free(patch_head);
		free(patch_local_head);
		free(patch_filename);
		fclose(tmp_patch_f);


		/* download checksum file */
		printf(_("Retrieving checksum list [%s]..."),		global_config->sources->url[i]);
		checksum_filename = gen_filename_from_url(global_config->sources->url[i],CHECKSUM_FILE);
		checksum_head = head_mirror_data(global_config->sources->url[i],CHECKSUM_FILE);
		checksum_local_head = read_head_cache(checksum_filename);

		/* open for reading if cached, otherwise write it from the downloaded data */
		if( checksum_head != NULL && checksum_local_head != NULL && strcmp(checksum_head,checksum_local_head) == 0){
			printf(_("Cached\n"));
			if( (tmp_checksum_f = open_file(checksum_filename,"r")) == NULL ) exit(1);
		}else{
			if( global_config->dl_stats == TRUE ) printf("\n");
			if( (tmp_checksum_f = open_file(checksum_filename,"w+b")) == NULL ) exit(1);
			if( get_mirror_data_from_source(
						tmp_checksum_f,global_config,global_config->sources->url[i],CHECKSUM_FILE
					) != 0
			){
				source_dl_failed = 1;
				clear_head_cache(checksum_filename);
			}else{
				if( global_config->dl_stats == FALSE ) printf(_("Done\n"));
			}
			rewind(tmp_checksum_f); /* make sure we are back at the front of the file */
		}
		/* if all is good, write it */
		if( source_dl_failed != 1 && checksum_head != NULL ) write_head_cache(checksum_head,checksum_filename);
		free(checksum_head);
		free(checksum_local_head);

		/*
			only do this double check if we know it didn't fail
		*/
		if( source_dl_failed != 1 ){
			if( available_pkgs->pkg_count == 0 ) source_dl_failed = 1;
		}

		/* if the download failed don't do this, do it if cached or d/l was good */
		if( source_dl_failed != 1 ){
			unsigned int a;

			/* now map md5 checksums to packages */
			printf(_("Reading Package Lists..."));
			for(a = 0;a < available_pkgs->pkg_count;a++){
				get_md5sum(available_pkgs->pkgs[a],tmp_checksum_f);
				printf("%c\b",spinner());
			}
			for(a = 0;a < patch_pkgs->pkg_count;a++){
				get_md5sum(patch_pkgs->pkgs[a],tmp_checksum_f);
				printf("%c\b",spinner());
			}
			printf(_("Done\n"));

			/* write package listings to disk */
			write_pkg_data(global_config->sources->url[i],pkg_list_fh_tmp,available_pkgs);
			write_pkg_data(global_config->sources->url[i],pkg_list_fh_tmp,patch_pkgs);

		}
		if ( available_pkgs ) free_pkg_list(available_pkgs);
		if ( patch_pkgs ) free_pkg_list(patch_pkgs);
		free(checksum_filename);
		fclose(tmp_checksum_f);

	}/* end for loop */

	/* if all our downloads where a success, write to PKG_LIST_L */
	if( source_dl_failed != 1 ){
		ssize_t bytes_read;
		size_t getline_len = 0;
		char *getline_buffer = NULL;
		FILE *pkg_list_fh;

		if( (pkg_list_fh = open_file(PKG_LIST_L,"w+")) == NULL ) exit(1);
		if( pkg_list_fh == NULL ) exit(1);
		rewind(pkg_list_fh_tmp);
		while( (bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh_tmp) ) != EOF ){
			fprintf(pkg_list_fh,"%s",getline_buffer);
		}
		if( getline_buffer ) free(getline_buffer);
		fclose(pkg_list_fh);

	}else{
		printf(_("Sources failed to download, correct sources and rerun --update\n"));
	}

	/* close the tmp pkg list file */
	fclose(pkg_list_fh_tmp);

	return source_dl_failed;
}

struct pkg_list *init_pkg_list(void){
	struct pkg_list *list;

	list = slapt_malloc( sizeof *list );
	list->pkgs = slapt_malloc( sizeof *list->pkgs );
	list->pkg_count = 0;
	list->free_pkgs = FALSE;

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

__inline pkg_info_t *init_pkg(void){
	pkg_info_t *pkg;

	pkg = slapt_malloc( sizeof *pkg );

	pkg->size_c = 0;
	pkg->size_u = 0;

	pkg->name = NULL;
	pkg->version = NULL;
	pkg->mirror = NULL;
	pkg->location = NULL;

	pkg->description = slapt_malloc(sizeof *pkg->description); 
	pkg->description[0] = '\0';

	pkg->required = slapt_malloc(sizeof *pkg->required);
	pkg->required[0] = '\0';

	pkg->conflicts = slapt_malloc(sizeof *pkg->conflicts);
	pkg->conflicts[0] = '\0';

	pkg->suggests = slapt_malloc(sizeof *pkg->suggests);
	pkg->suggests[0] = '\0';


	pkg->md5[0] = '\0';

	return pkg;
}

/* generate the package file name */
char *gen_pkg_file_name(const rc_config *global_config,pkg_info_t *pkg){
	char *file_name = NULL;

	/* build the file name */
	file_name = slapt_calloc(
		strlen(global_config->working_dir)+strlen("/")
			+strlen(pkg->location)+strlen("/")
			+strlen(pkg->name)+strlen("-")+strlen(pkg->version)+strlen(".tgz") + 1 ,
		sizeof *file_name
	);
	file_name = strncpy(file_name,
		global_config->working_dir,strlen(global_config->working_dir)
	);
	file_name[ strlen(global_config->working_dir) ] = '\0';
	file_name = strncat(file_name,"/",strlen("/"));
	file_name = strncat(file_name,pkg->location,strlen(pkg->location));
	file_name = strncat(file_name,"/",strlen("/"));
	file_name = strncat(file_name,pkg->name,strlen(pkg->name));
	file_name = strncat(file_name,"-",strlen("-"));
	file_name = strncat(file_name,pkg->version,strlen(pkg->version));
	file_name = strncat(file_name,".tgz",strlen(".tgz"));

	return file_name;
}

/* generate the download url for a package */
char *gen_pkg_url(pkg_info_t *pkg){
	char *url = NULL;
	char *file_name = NULL;

	/* build the file name */
	file_name = slapt_calloc(
		strlen(pkg->name)+strlen("-")+strlen(pkg->version)+strlen(".tgz") + 1 ,
		sizeof *file_name
	);
	file_name = strncpy(file_name,pkg->name,strlen(pkg->name));
	file_name = strncat(file_name,"-",strlen("-"));
	file_name = strncat(file_name,pkg->version,strlen(pkg->version));
	file_name = strncat(file_name,".tgz",strlen(".tgz"));

	url = slapt_calloc(
		strlen(pkg->mirror) + strlen(pkg->location)
			+ strlen(file_name) + strlen("/") + 1,
		sizeof *url
	);

	url = strncpy(url,pkg->mirror,strlen(pkg->mirror));
	url[ strlen(pkg->mirror) ] = '\0';
	url = strncat(url,pkg->location,strlen(pkg->location));
	url = strncat(url,"/",strlen("/"));
	url = strncat(url,file_name,strlen(file_name));

	free(file_name);
	return url;
}

/* find out the pkg file size (post download) */
size_t get_pkg_file_size(const rc_config *global_config,pkg_info_t *pkg){
	char *file_name = NULL;
	struct stat file_stat;
	size_t file_size = 0;

	/* build the file name */
	file_name = gen_pkg_file_name(global_config,pkg);

	if( stat(file_name,&file_stat) == 0 ){
		file_size = file_stat.st_size;
	}
	free(file_name);

	return file_size;
}

/* package is already downloaded and cached, md5sum if applicable is ok */
int verify_downloaded_pkg(const rc_config *global_config,pkg_info_t *pkg){
	char *file_name = NULL;
	FILE *fh_test = NULL;
	size_t file_size = 0;
	char md5sum_f[MD5_STR_LEN];
	int is_verified = 0,not_verified = -1;

	/*
		check the file size first so we don't run an md5 checksum
		on an incomplete file
	*/
	file_size = get_pkg_file_size(global_config,pkg);
	if( (unsigned int)(file_size/1024) != pkg->size_c){
		return not_verified;
	}
	/* if not checking the md5 checksum and the sizes match, assume its good */
	if( global_config->no_md5_check == TRUE ) return is_verified;

	/* check to see that we actually have an md5 checksum */
	if( strcmp(pkg->md5,"") == 0){
		printf(_("Could not find MD5 checksum for %s, override with --no-md5\n"),pkg->name);
		return not_verified;
	}

	/* build the file name */
	file_name = gen_pkg_file_name(global_config,pkg);

	/* return if we can't open the file */
	if( ( fh_test = open_file(file_name,"r") ) == NULL ){
		free(file_name);
		return not_verified;
	}
	free(file_name);

	/* generate the md5 checksum */
	gen_md5_sum_of_file(fh_test,md5sum_f);
	fclose(fh_test);

	/* check to see if the md5sum is correct */
	if( strcmp(md5sum_f,pkg->md5) == 0 ) return is_verified;

	return MD5_CHECKSUM_FAILED;

}

char *gen_filename_from_url(const char *url,const char *file){
	char *filename,*cleaned;

	filename = slapt_calloc( strlen(url) + strlen(file) + 2 , sizeof *filename );
	filename[0] = '.';
	strncat(filename,url,strlen(url));
	strncat(filename,file,strlen(file));

	cleaned = str_replace_chr(filename,'/','#');

	free(filename);

	return cleaned;
}

void purge_old_cached_pkgs(const rc_config *global_config,char *dir_name,struct pkg_list *avail_pkgs){
	DIR *dir;
	struct dirent *file;
	struct stat file_stat;
	sg_regex cached_pkgs_regex;
	int local_pkg_list = 0;

	if( avail_pkgs == NULL ){
		avail_pkgs = get_available_pkgs();
		local_pkg_list = 1;
	}
	if( dir_name == NULL ) dir_name = (char *)global_config->working_dir;
	init_regex(&cached_pkgs_regex,PKG_PARSE_REGEX);

	if( (dir = opendir(dir_name)) == NULL ){
		if( errno ) perror(dir_name);
		fprintf(stderr,_("Failed to opendir %s\n"),dir_name);
		return;
	}

	if( chdir(dir_name) == -1 ){
		if( errno ) perror(dir_name);
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
			purge_old_cached_pkgs(global_config,file->d_name,avail_pkgs);
			if( (chdir("..")) == -1 ){
				fprintf(stderr,_("Failed to chdir: %s\n"),dir_name);
				return;
			}
			continue;
		}

		/* if its a package */
		if( strstr(file->d_name,".tgz") !=NULL ){

			execute_regex(&cached_pkgs_regex,file->d_name);

			/* if our regex matches */
			if( cached_pkgs_regex.reg_return == 0 ){
				char *tmp_pkg_name,*tmp_pkg_version;
				pkg_info_t *tmp_pkg;

				tmp_pkg_name = strndup(
					file->d_name + cached_pkgs_regex.pmatch[1].rm_so,
					cached_pkgs_regex.pmatch[1].rm_eo - cached_pkgs_regex.pmatch[1].rm_so
				);
				tmp_pkg_version = strndup(
					file->d_name + cached_pkgs_regex.pmatch[2].rm_so,
					cached_pkgs_regex.pmatch[2].rm_eo - cached_pkgs_regex.pmatch[2].rm_so
				);

				tmp_pkg = get_exact_pkg(avail_pkgs,tmp_pkg_name,tmp_pkg_version);
				free(tmp_pkg_name);
				free(tmp_pkg_version);

				if( tmp_pkg == NULL ){

					if(global_config->no_prompt == TRUE ){
							unlink(file->d_name);
					}else{
						if( ask_yes_no(_("Delete %s ? [y/N]"), file->d_name) == 1 )
							unlink(file->d_name);
					}

				}
				tmp_pkg = NULL;

			}

		}

	}
	closedir(dir);

	free_regex(&cached_pkgs_regex);
	if( local_pkg_list == 1 ){
		free_pkg_list(avail_pkgs);
	}

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

/* find dependency from "or" requirement */
static pkg_info_t *find_or_requirement(struct pkg_list *avail_pkgs,struct pkg_list *installed_pkgs,char *required_str){
	pkg_info_t *pkg = NULL;
	int position = 0, len = 0;

	len = strlen(required_str);
	while( position < len ){

		if( strchr(required_str + position,'|') == NULL ){
			char *string = required_str + position;

			pkg = parse_meta_entry(avail_pkgs,installed_pkgs,string);

			if( pkg != NULL ) break;

			position += strlen(string);
		}else{
			char *string = NULL;
			int str_len = 0;
			char *next_token = NULL;

			if( required_str[position] == '|' ){ ++position; continue; }

			next_token = index(required_str + position,'|');
			str_len = strlen(required_str + position) - strlen(next_token);

			string = strndup(
				required_str + position,
				str_len
			);

			pkg = parse_meta_entry(avail_pkgs,installed_pkgs,string);
			free(string);

			if( pkg != NULL ) break;

			position += str_len;
		}

		++position;
	}

	return pkg;
}

pkg_info_t *copy_pkg(pkg_info_t *dst,pkg_info_t *src){
	dst = memcpy(dst,src, sizeof *src);
	dst->name = strndup(src->name,strlen(src->name));
	dst->version = strndup(src->version,strlen(src->version));
	dst->mirror = strndup(src->mirror,strlen(src->mirror));
	dst->location = strndup(src->location,strlen(src->location));
	dst->description = strndup(src->description,strlen(src->description));
	dst->suggests = strndup(src->suggests, strlen(src->suggests));
	dst->conflicts = strndup(src->conflicts, strlen(src->conflicts));
	dst->required = strndup(src->required, strlen(src->required));
	return dst;
}

