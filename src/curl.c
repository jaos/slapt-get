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

void download_data(FILE *fh,char *url){
	CURL *ch;
	CURLcode response;
	char curl_err_buff[1024];

#if DEBUG == 1
	printf("Fetching url:[%s]\n",url);
#endif
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, url);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, fh);
	curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0);
#if USE_CURL_PROGRESS == 0
	curl_easy_setopt(ch, CURLOPT_PROGRESSFUNCTION, progress_callback);
#endif
	curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, curl_err_buff );

	if( (response = curl_easy_perform(ch)) != 0 ){
		fprintf(stderr,"failed to download: %s\n",curl_err_buff);
		exit(1);
	}
	/* curl_easy_cleanup(ch); */
	curl_free(ch);
}

FILE *download_pkg_list(rc_config *global_config){
	FILE *fh;
	char *url = NULL;

	fh = open_file(PKG_LIST_L,"w+");

#if USE_CURL_PROGRESS == 0
	printf("Retrieving package data...");
#else
	printf("Retrieving package data...\n");
#endif
	url = calloc( ( strlen(global_config->mirror_url) + strlen(PKG_LIST) ) + 1, sizeof(char) );
	url = strncpy(url,global_config->mirror_url,strlen(global_config->mirror_url) + 1);
	url = strncat(url,PKG_LIST,strlen(PKG_LIST) + 1);
	download_data(fh,url);
#if USE_CURL_PROGRESS == 0
	printf("Done\n");
#endif

	free(url);
	rewind(fh); /* make sure we are back at the front of the file */
	return fh;
}

FILE *download_patches_list(rc_config *global_config){
	FILE *fh;
	char *url = NULL;

	fh = open_file(PATCHES_LIST_L,"w+");

#if USE_CURL_PROGRESS == 0
	printf("Retrieving patch list...");
#else
	printf("Retrieving patch list...\n");
#endif
	url = calloc( ( strlen(global_config->mirror_url) + strlen(PATCHDIR) + strlen(PATCHES_LIST) ) + 1, sizeof(char) );
	url = strncpy(url,global_config->mirror_url,strlen(global_config->mirror_url) + 1);
	url = strncat(url,PATCHDIR,strlen(PATCHDIR) + 1);
	url = strncat(url,PATCHES_LIST,strlen(PATCHES_LIST) + 1);
	download_data(fh,url);
#if USE_CURL_PROGRESS == 0
	printf("Done\n");
#endif

	free(url);
	rewind(fh); /* make sure we are back at the front of the file */
	return fh;
}

char *download_pkg(rc_config *global_config,pkg_info *pkg){
	FILE *fh;
	char *file_name = NULL;
	char *url = NULL;

	/* build the file name */
	if((file_name = calloc((strlen(pkg->name)+strlen("-")+strlen(pkg->version)+strlen(".tgz")) + 1,sizeof(char))) == NULL){
		fprintf(stderr,"Failed to calloc file_name\n");
		exit(1);
	}
	file_name = strncpy(file_name,pkg->name,strlen(pkg->name));
	file_name = strncat(file_name,"-",strlen("-"));
	file_name = strncat(file_name,pkg->version,strlen(pkg->version));
	file_name = strncat(file_name,".tgz",strlen(".tgz"));
	/* file_name[ strlen(file_name) ] = '\0'; */

	fh = open_file(file_name,"wb");

	/*
	 * TODO: here we will use the md5sum (or some other means)
	 *	to see if the file is already present and valid
	 */


	/* build the url */
	if((url = calloc((strlen(global_config->mirror_url) + strlen(pkg->location) + strlen(file_name) + strlen("/")) + 1,sizeof(char))) == NULL){
		fprintf(stderr,"Failed to calloc url\n");
		exit(1);
	}
	url = strncpy(url,global_config->mirror_url,strlen(global_config->mirror_url));
	url = strncat(url,pkg->location,strlen(pkg->location));
	url = strncat(url,"/",strlen("/"));
	url = strncat(url,file_name,strlen(file_name));
	url[ strlen(url) ] = '\0';

#if USE_CURL_PROGRESS == 0
	printf("Downloading %s...",pkg->name);
#else
	printf("Downloading %s...\n",pkg->name);
#endif
	download_data(fh,url);
#if USE_CURL_PROGRESS == 0
	printf("Done\n");
#endif

	if( url != NULL ){
		free(url);
	}
	fclose(fh);
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

