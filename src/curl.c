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

int download_data(FILE *fh,const char *url,size_t bytes,int use_curl_dl_stats){
	CURL *ch = NULL;
	CURLcode response;
	char curl_err_buff[1024];
	int return_code = 0;

#if DEBUG == 1
	printf(_("Fetching url:[%s]\n"),url);
#endif
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, fh);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(ch, CURLOPT_USERAGENT, PROGRAM_NAME );

	if( use_curl_dl_stats != 1 )
		curl_easy_setopt(ch, CURLOPT_PROGRESSFUNCTION, progress_callback );

	curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, curl_err_buff );

	/* resume */
	if( bytes > 0 ){
		fseek(fh,0,SEEK_END);
		curl_easy_setopt(ch, CURLOPT_RESUME_FROM, bytes);
	}

	if( (response = curl_easy_perform(ch)) != 0 ){
		fprintf(stderr,_("Failed to download: %s\n"),curl_err_buff);
		return_code = -1;
	}
	/*
   * need to use curl_easy_cleanup() so that we don't 
 	 * have tons of open connections, getting rejected
	 * by ftp servers for being naughty.
	*/
	curl_easy_cleanup(ch);
	/* can't do a curl_free() after curl_easy_cleanup() */
	/* curl_free(ch); */

	return return_code;
}

int head_request(const char *filename,const char *url){
	CURL *ch = NULL;
	CURLcode response;
	struct head_request_t head_d;
	struct stat file_stat;
	char *size_string;
	int return_code = -1;

	stat(filename,&file_stat);
	/* return false if filesize is 0 */
	if( (int)file_stat.st_size == 0 ){
		return -1;
	}

	head_d.data = malloc( sizeof *head_d.data );
	head_d.size = 0;

	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEHEADER, (void *)&head_d);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, head_request_data_callback);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
	curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
	curl_easy_setopt(ch, CURLOPT_USERAGENT, PROGRAM_NAME );

	if( (response = curl_easy_perform(ch)) != 0 )
		return_code = -1;

	if( (size_string = strstr(head_d.data,"Content-Length: ")) != NULL ){
		char *next_newline;
		if( (next_newline = strstr(size_string,"\n")) != NULL ){
			char *size;
			int len = (strlen((size_string + strlen("Content-Length: "))) - strlen(next_newline));
			size = malloc( sizeof *size * (len + 1));
			strncpy(size,size_string + strlen("Content-Length: "),len);
			if( (int)file_stat.st_size == atoi(size) ){
				return_code = 0;
			}else{
				return_code = -1;
			}
			free(size);
		}
	}

	curl_easy_cleanup(ch);
	free(head_d.data);
	return return_code;
}

int get_mirror_data_from_source(FILE *fh,int use_curl_dl_stats,const char *base_url,const char *filename){
	int return_code = 0;
	char *url = NULL;

	url = calloc(
		strlen(base_url) + strlen(filename) + 1, sizeof *url
	);
	if( url == NULL ){
		fprintf(stderr,_("Failed to calloc %s\n"),"url");
		exit(1);
	}

	strncpy(url,base_url,strlen(base_url) );
	url[ strlen(base_url) ] = '\0';
	strncat(url,filename,strlen(filename) );

	return_code = download_data(fh,url,0,use_curl_dl_stats);

	free(url);
	/* make sure we are back at the front of the file */
	/* DISABLED */
	/* rewind(fh); */
	return return_code;
}

int download_pkg(const rc_config *global_config,pkg_info_t *pkg){
	FILE *fh = NULL;
	FILE *fh_test = NULL;
	char *file_name = NULL;
	char *url = NULL;
	char md5_sum_of_file[MD5_STR_LEN];
	struct stat file_stat;
	size_t f_size = 0;

	create_dir_structure(pkg->location);
	chdir(global_config->working_dir); /* just in case */
	chdir(pkg->location);

	/* build the file name */
	file_name = gen_pkg_file_name(pkg);

	/* if we can open the file, gen the md5sum and stat for the file size */
	if( ( fh_test = fopen(file_name,"r") ) != NULL){
		/* check to see if the md5sum is correct */
		gen_md5_sum_of_file(fh_test,md5_sum_of_file);
		if( stat(file_name,&file_stat) == -1 ){
			if ( errno ) perror("stat");
		}else{
			f_size = file_stat.st_size;
		}
		fclose(fh_test);
	}

	if( global_config->no_md5_check == 0 ){
		/*
	 	* here we will use the md5sum to see if the file is already present and valid
	 	*/
		if( strcmp(md5_sum_of_file,pkg->md5) == 0 ){
			printf(_("Using cached copy of %s\n"),pkg->name);
			chdir(global_config->working_dir);
			free(file_name);
			return 0;
		}
	}else{
		/* if no md5 check is performed, and the sizes match, assume its good */
		if( (int)(f_size/1024) == pkg->size_c){
			printf(_("Using cached copy of %s\n"),pkg->name);
			chdir(global_config->working_dir);
			free(file_name);
			return 0;
		}
	}

	/* build the url */
	url = gen_pkg_url(pkg);

	if( global_config->dl_stats == 1 ){
		printf(_("Downloading %s %s %s [%dK]...\n"),pkg->mirror,pkg->name,pkg->version,pkg->size_c - (f_size/1024));
	}else{
		printf(_("Downloading %s %s %s [%dK]..."),pkg->mirror,pkg->name,pkg->version,pkg->size_c - (f_size/1024));
	}

	fh = open_file(file_name,"a+b");
	if( fh == NULL ){
		chdir(global_config->working_dir);
		free(file_name);
		return -1;
	}

	if( download_data(fh,url,f_size,global_config->dl_stats) == 0 ){
		if( global_config->dl_stats != 1 ) printf(_("Done\n"));
	}else{
		fclose(fh);
		#if DEBUG == 1
		printf("FAilure: %s-%s from %s %s\n",pkg->name,pkg->version,pkg->mirror,pkg->location);
		#endif
		#if DO_NOT_UNLINK_BAD_FILES == 0
		/* if the d/l fails, unlink the empty file */
		if( unlink(file_name) == -1 ){
			fprintf(stderr,_("Failed to unlink %s\n"),file_name);
			if( errno ){
				perror("unlink");
			}
		}
		#endif
		free(file_name);
		return -1;
	}

	fclose(fh);

	fh = open_file(file_name,"r");
	if( fh == NULL ){
		chdir(global_config->working_dir);
		free(file_name);
		return -1;
	}

	if( stat(file_name,&file_stat) == -1 ){
		chdir(global_config->working_dir);
		free(file_name);
		return -1;
	}

	/* do an initial check... so we don't run md5 checksum on an imcomplete file */
	f_size = file_stat.st_size;
	if( (int)(f_size/1024) != pkg->size_c){
		printf(_("Download of %s incomplete\n"),pkg->name);
		chdir(global_config->working_dir);
		free(file_name);
		return -1;
	}

	gen_md5_sum_of_file(fh,md5_sum_of_file);
	fclose(fh);

	if( global_config->no_md5_check == 0 ){

		/* check to see that we actually have an md5 checksum */
		if( strcmp(pkg->md5,"") == 0){
			printf(_("Could not find MD5 checksum for %s, override with --no-md5\n"),pkg->name);
			chdir(global_config->working_dir);
			free(file_name);
			return -1;
		}

		/* check to see if the md5sum is correct */
		if( strcmp(md5_sum_of_file,pkg->md5) != 0 ){
			fprintf(stderr,_("md5 checksum for %s is not correct!\n"),pkg->name);
			#if DEBUG == 1
			fprintf(stderr,_("MD5 found:    [%s]\n"),md5_sum_of_file);
			fprintf(stderr,_("MD5 expected: [%s]\n"),pkg->md5);
			fprintf(stderr,_("File: %s/%s\n"),global_config->working_dir,file_name);
			#endif

			#if DO_NOT_UNLINK_BAD_FILES == 0
			/* if the checksum fails, unlink the bogus file */
			if( unlink(file_name) == -1 ){
				fprintf(stderr,_("Failed to unlink %s\n"),file_name);
				if( errno ){
					perror("unlink");
				}
			}
			#endif

			chdir(global_config->working_dir);
			free(file_name);
			return -1;

		}
		/* end md5 */
	}

	if( url != NULL ){
		free(url);
	}
	chdir(global_config->working_dir);
	free(file_name);
	return 0;
}

