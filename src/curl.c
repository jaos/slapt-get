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

int download_data(FILE *fh,const char *url){
	CURL *ch = NULL;
	CURLcode response;
	char curl_err_buff[1024];
	int return_code = 0;

#if DEBUG == 1
	printf("Fetching url:[%s]\n",url);
#endif
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, fh);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(ch, CURLOPT_USERAGENT, PROGRAM_NAME );
#if USE_CURL_PROGRESS == 0
	curl_easy_setopt(ch, CURLOPT_PROGRESSFUNCTION, progress_callback);
#endif
	curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, curl_err_buff );

	if( (response = curl_easy_perform(ch)) != 0 ){
		fprintf(stderr,"Failed to download: %s\n",curl_err_buff);
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

int get_mirror_data_from_source(FILE *fh,const char *base_url,const char *filename){
	int return_code = 0;
	char *url = NULL;

	url = calloc(
		strlen(base_url) + strlen(filename) + 1, sizeof *url
	);
	if( url == NULL ){
		fprintf(stderr,"Failed to calloc url\n");
		exit(1);
	}

	strncpy(url,base_url,strlen(base_url) );
	strncat(url,filename,strlen(filename) );
	return_code = download_data(fh,url);

	free(url);
	/* make sure we are back at the front of the file */
	/* DISABLED */
	/* rewind(fh); */
	return return_code;
}

char *download_pkg(const rc_config *global_config,pkg_info_t *pkg){
	FILE *fh = NULL;
	FILE *fh_test = NULL;
	char *file_name = NULL;
	char *url = NULL;
	char *md5_sum = NULL;
	char *md5_sum_of_file = NULL;

	md5_sum = malloc(34);
	md5_sum_of_file = malloc(34);
	get_md5sum(global_config,pkg,md5_sum);

	/* build the file name */
	file_name = calloc(
		strlen(pkg->name)+strlen("-")+strlen(pkg->version)+strlen(".tgz") + 1 ,
		sizeof *file_name
	);
	if( file_name == NULL ){
		fprintf(stderr,"Failed to calloc file_name\n");
		exit(1);
	}
	file_name = strncpy(file_name,pkg->name,strlen(pkg->name));
	file_name[ strlen(pkg->name) ] = '\0';
	file_name = strncat(file_name,"-",strlen("-"));
	file_name = strncat(file_name,pkg->version,strlen(pkg->version));
	file_name = strncat(file_name,".tgz",strlen(".tgz"));

	if( global_config->no_md5_check == 0 ){
		/*
	 	* here we will use the md5sum to see if the file is already present and valid
	 	*/
		if( ( fh_test = fopen(file_name,"r") ) != NULL){
			/* check to see if the md5sum is correct */
			gen_md5_sum_of_file(fh_test,md5_sum_of_file);
			fclose(fh_test);
			if( strcmp(md5_sum_of_file,md5_sum) == 0 ){
				printf("Using cached copy of %s\n",pkg->name);
				free(md5_sum);
				free(md5_sum_of_file);
				return file_name;
			}
		}
		/* */
	}

	/* build the url */
	url = calloc(
		strlen(pkg->mirror) + strlen(pkg->location)
			+ strlen(file_name) + strlen("/") + 1,
		sizeof *url
	);
	if( url == NULL ){
		fprintf(stderr,"Failed to calloc url\n");
		exit(1);
	}
	url = strncpy(url,pkg->mirror,strlen(pkg->mirror));
	url = strncat(url,pkg->location,strlen(pkg->location));
	url = strncat(url,"/",strlen("/"));
	url = strncat(url,file_name,strlen(file_name));
	url[ strlen(url) ] = '\0';

	#if USE_CURL_PROGRESS == 0
	printf("Downloading %s %s %s [%dK]...",pkg->mirror,pkg->name,pkg->version,pkg->size_c);
	#else
	printf("Downloading %s %s %s [%dK]...\n",pkg->mirror,pkg->name,pkg->version,pkg->size_c);
	#endif

	fh = open_file(file_name,"wb");
	if( download_data(fh,url) == 0 ){
		#if USE_CURL_PROGRESS == 0
		printf("Done\n");
		#endif
	}else{
		fclose(fh);
		#if DO_NOT_UNLINK_BAD_FILES == 0
		/* if the d/l fails, unlink the empty file */
		if( unlink(file_name) == -1 ){
			fprintf(stderr,"Failed to unlink %s\n",file_name);
			if( errno ){
				perror("unlink");
			}
		}
		#endif
		return NULL;
	}

	fclose(fh);

	fh = open_file(file_name,"r");
	gen_md5_sum_of_file(fh,md5_sum_of_file);
	fclose(fh);

	if( global_config->no_md5_check == 0 ){

		/* check to see if the md5sum is correct */
		printf("verifying %s md5 checksum...",pkg->name);
		if( strcmp(md5_sum_of_file,md5_sum) != 0 ){
			fprintf(stderr,"md5 sum for %s is not correct!\n",pkg->name);
			#if DEBUG == 1
			fprintf(stderr,"MD5 found:    [%s]\n",md5_sum_of_file);
			fprintf(stderr,"MD5 expected: [%s]\n",md5_sum);
			fprintf(stderr,"File: %s/%s\n",global_config->working_dir,file_name);
			#endif

			#if DO_NOT_UNLINK_BAD_FILES == 0
			/* if the checksum fails, unlink the bogus file */
			if( unlink(file_name) == -1 ){
				fprintf(stderr,"Failed to unlink %s\n",file_name);
				if( errno ){
					perror("unlink");
				}
			}
			#endif

			return NULL;

		}else{
			printf("Done\n");
		}
		/* end md5 */
	}

	free(md5_sum);
	free(md5_sum_of_file);

	if( url != NULL ){
		free(url);
	}
	return file_name;
}

int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow){
	/* supress unused parameter warning */
	(void) clientp;
	(void) dltotal;
	(void) dlnow;
	(void) ultotal;
	(void) ulnow;
	/* */
	printf("%c\b",spinner());
	return 0;
}

