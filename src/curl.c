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
	char *file_name = NULL;
	char *url = NULL;
	size_t f_size = 0;
	int pkg_verify_return = -1;

	if( verify_downloaded_pkg(global_config,pkg) == 0 ) return 0;

	chdir(global_config->working_dir); /* just in case */
	create_dir_structure(pkg->location);

	/* build the url, file name, and get the file size if the file is present */
	url = gen_pkg_url(pkg);
	file_name = gen_pkg_file_name(global_config,pkg);
	f_size = get_pkg_file_size(global_config,pkg);

	if( global_config->dl_stats == 1 ){
		printf(_("Downloading %s %s %s [%dK]...\n"),pkg->mirror,pkg->name,pkg->version,pkg->size_c - (f_size/1024));
	}else{
		printf(_("Downloading %s %s %s [%dK]..."),pkg->mirror,pkg->name,pkg->version,pkg->size_c - (f_size/1024));
	}

	/* open the file to write, append if already present */
	fh = open_file(file_name,"a+b");
	if( fh == NULL ){
		free(file_name);
		free(url);
		return -1;
	}

	/* download the file to our file handle */
	if( download_data(fh,url,f_size,global_config->dl_stats) == 0 ){
		if( global_config->dl_stats != 1 ) printf(_("Done\n"));
	}else{
		fclose(fh);
		#if DEBUG == 1
		printf("Failure: %s-%s from %s %s\n",pkg->name,pkg->version,pkg->mirror,pkg->location);
		#endif
		#if DO_NOT_UNLINK_BAD_FILES == 0
		/* if the d/l fails, unlink the empty file */
		if( unlink(file_name) == -1 ){
			fprintf(stderr,_("Failed to unlink %s\n"),file_name);
			if( errno ) perror("unlink");
		}
		#endif
		free(url);
		free(file_name);
		return -1;
	}

	fclose(fh);
	free(url);

	/* check to make sure we have the complete file */
	pkg_verify_return = verify_downloaded_pkg(global_config,pkg);
	if( pkg_verify_return == 0 ){
		free(file_name);
		return 0;
	}else if( pkg_verify_return == MD5_CHECKSUM_FAILED ){
		fprintf(stderr,_("md5 sum for %s is not correct, override with --no-md5!\n"),pkg->name);
		#if DO_NOT_UNLINK_BAD_FILES == 0
		/* if the checksum fails, unlink the bogus file */
		if( unlink(file_name) == -1 ){
			fprintf(stderr,_("Failed to unlink %s\n"),file_name);
			if( errno ) perror("unlink");
		}
		#endif
		free(file_name);
		return -1;
	}else{
		printf(_("Download of %s incomplete\n"),pkg->name);
		free(file_name);
		return -1;
	}

}

