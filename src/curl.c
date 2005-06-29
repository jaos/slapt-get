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
static size_t write_header_callback(void *buffer,
                                    size_t size, size_t nmemb, void *userp);

int download_data(FILE *fh,const char *url,size_t bytes,
                  const rc_config *global_config)
{
  CURL *ch = NULL;
  CURLcode response;
  char curl_err_buff[1024];
  int return_code = 0;
  struct curl_slist *headers = NULL;

#if DEBUG == 1
  printf(_("Fetching url:[%s]\n"),url);
#endif
  ch = curl_easy_init();
  curl_easy_setopt(ch, CURLOPT_URL, url);
  curl_easy_setopt(ch, CURLOPT_WRITEDATA, fh);
  curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0);
  curl_easy_setopt(ch, CURLOPT_USERAGENT, PROGRAM_NAME );
  curl_easy_setopt(ch, CURLOPT_FTP_USE_EPSV, 0);
/* this is a check for slack 9.0's curl libs */
#ifdef CURLOPT_FTP_USE_EPRT
  curl_easy_setopt(ch, CURLOPT_FTP_USE_EPRT, 0);
#endif
  curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
#ifdef CURLOPT_HTTPAUTH
  curl_easy_setopt(ch, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
#endif
  curl_easy_setopt(ch, CURLOPT_FILETIME, 1 );
  curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1 );

  headers = curl_slist_append(headers, "Pragma: "); /* override no-cache */

  if ( global_config->dl_stats != 1 ) {
    if ( global_config->progress_cb == NULL ) {
      curl_easy_setopt(ch, CURLOPT_PROGRESSFUNCTION, progress_callback );
    } else {
      curl_easy_setopt(ch, CURLOPT_PROGRESSFUNCTION,
                       global_config->progress_cb );
    }
    curl_easy_setopt(ch, CURLOPT_PROGRESSDATA, &bytes);
  }

  curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, curl_err_buff );

  /* resume */
  if ( bytes > 0 ) {
    fseek(fh,0,SEEK_END);
    curl_easy_setopt(ch, CURLOPT_RESUME_FROM, bytes);
  }

  if ( (response = curl_easy_perform(ch)) != CURLE_OK ) {
    /*
      * this is for proxy servers that can't resume 
    */
    if ( response == CURLE_HTTP_RANGE_ERROR ) {
      return_code = CURLE_HTTP_RANGE_ERROR;
    /*
      * this is a simple hack for all ftp sources that won't have a patches dir
      * we don't want an ugly error to confuse the user
    */
    }else if ( strstr(url,PATCHES_LIST) != NULL ) {
      return_code = 0;
    } else {
      fprintf(stderr,_("Failed to download: %s\n"),curl_err_buff);
      return_code = -1;
    }
  }
  /*
   * need to use curl_easy_cleanup() so that we don't 
    * have tons of open connections, getting rejected
   * by ftp servers for being naughty.
  */
  curl_easy_cleanup(ch);
  /* can't do a curl_free() after curl_easy_cleanup() */
  /* curl_free(ch); */
  curl_slist_free_all(headers);

  return return_code;
}

static size_t write_header_callback(void *buffer, size_t size,
                                    size_t nmemb, void *userp)
{
  char *tmp;
  register int a_size = size * nmemb;
  struct head_data_t *head_t = (struct head_data_t *)userp;
  
  tmp = (char *)realloc( head_t->data, head_t->size + a_size + 1);
  if ( tmp != NULL ) {
    head_t->data = tmp;
    memcpy(&(head_t->data[head_t->size]),buffer,a_size);
    head_t->size += a_size;
    head_t->data[head_t->size] = '\0';
  }

  return nmemb;
}

char *head_request(const char *url)
{
  CURL *ch = NULL;
  CURLcode response;
  struct head_data_t head_t;
  struct curl_slist *headers = NULL;

  head_t.data = slapt_malloc( sizeof *head_t.data );
  head_t.size = 0;

  ch = curl_easy_init();
  curl_easy_setopt(ch, CURLOPT_URL, url);
  curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_header_callback);
  curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
  curl_easy_setopt(ch, CURLOPT_USERAGENT, PROGRAM_NAME );
  curl_easy_setopt(ch, CURLOPT_FILE, &head_t);
  curl_easy_setopt(ch, CURLOPT_HEADER, 1);
  curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
  curl_easy_setopt(ch, CURLOPT_FTP_USE_EPSV , 0);
/* this is a check for slack 9.0's curl libs */
#ifdef CURLOPT_FTP_USE_EPRT
  curl_easy_setopt(ch, CURLOPT_FTP_USE_EPRT, 0);
#endif
  curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
#ifdef CURLOPT_HTTPAUTH
  curl_easy_setopt(ch, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
#endif
  curl_easy_setopt(ch, CURLOPT_FILETIME, 1 );
  curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1 );

  headers = curl_slist_append(headers, "Pragma: "); /* override no-cache */

  if ( (response = curl_easy_perform(ch)) != 0 ) {
    free(head_t.data);
    curl_easy_cleanup(ch);
    curl_slist_free_all(headers);
    return NULL;
  }

  curl_easy_cleanup(ch);
  curl_slist_free_all(headers);
  return head_t.data;

}

int get_mirror_data_from_source(FILE *fh,const rc_config *global_config,
                                const char *base_url,const char *filename)
{
  int return_code = 0;
  char *url = NULL;

  url = slapt_calloc(
    strlen(base_url) + strlen(filename) + 1, sizeof *url
  );

  strncpy(url,base_url,strlen(base_url) );
  url[ strlen(base_url) ] = '\0';
  strncat(url,filename,strlen(filename) );

  return_code = download_data(fh,url,0,global_config);

  free(url);
  /* make sure we are back at the front of the file */
  /* DISABLED */
  /* rewind(fh); */
  return return_code;
}

int download_pkg(const rc_config *global_config,pkg_info_t *pkg)
{
  FILE *fh = NULL;
  char *file_name = NULL;
  char *url = NULL;
  size_t f_size = 0;
  int pkg_verify_return = -1;
  int dl_return = -1;

  if ( verify_downloaded_pkg(global_config,pkg) == 0 )
    return 0;

  chdir(global_config->working_dir); /* just in case */
  create_dir_structure(pkg->location);

  /* build the url, file name, and get the file size if the file is present */
  url = gen_pkg_url(pkg);
  file_name = gen_pkg_file_name(global_config,pkg);
  f_size = get_pkg_file_size(global_config,pkg);

  if ( global_config->dl_stats == TRUE ) {
    int dl_total_size = pkg->size_c - (f_size/1024);
    printf(_("Downloading %s %s %s [%.1d%s]...\n"),
      pkg->mirror,pkg->name,pkg->version,
      ( dl_total_size > 1024 ) ? dl_total_size / 1024 : dl_total_size,
      ( dl_total_size > 1024 ) ? "MB" : "kB"
    );
  } else {
    int dl_total_size = pkg->size_c - (f_size/1024);
    printf(_("Downloading %s %s %s [%.1d%s]..."),
      pkg->mirror,pkg->name,pkg->version,
      ( dl_total_size > 1024 ) ? dl_total_size / 1024 : dl_total_size,
      ( dl_total_size > 1024 ) ? "MB" : "kB"
    );
  }

  /* open the file to write, append if already present */
  fh = open_file(file_name,"a+b");
  if ( fh == NULL ) {
    free(file_name);
    free(url);
    return -1;
  }

  /* download the file to our file handle */
  dl_return = download_data(fh,url,f_size,global_config);
  if ( dl_return == 0 ) {
    if ( global_config->dl_stats == FALSE ) printf(_("Done\n"));
  }else if ( dl_return == CURLE_HTTP_RANGE_ERROR ) {
    /*
      * this is for errors trying to resume.  unlink the file and
      * try again.
    */
    printf("\r");
    fclose(fh);

    if ( unlink(file_name) == -1 ) {
      fprintf(stderr,_("Failed to unlink %s\n"),file_name);

      if ( errno )
        perror(file_name);

      exit(1);
    }

    free(file_name);
    free(url);

    return download_pkg(global_config,pkg);

  } else {
    fclose(fh);
    #if DEBUG == 1
    printf("Failure: %s-%s from %s %s\n",pkg->name,pkg->version,
           pkg->mirror,pkg->location);
    #endif
    #if DO_NOT_UNLINK_BAD_FILES == 0
    /* if the d/l fails, unlink the empty file */
    if ( unlink(file_name) == -1 ) {
      fprintf(stderr,_("Failed to unlink %s\n"),file_name);

      if ( errno )
        perror(file_name);

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
  if ( pkg_verify_return == 0 ) {

    free(file_name);
    return 0;

  }else if ( pkg_verify_return == MD5_CHECKSUM_FAILED ) {
    fprintf(stderr,
      _("md5 sum for %s is not correct, override with --no-md5!\n"),
      pkg->name);
    #if DO_NOT_UNLINK_BAD_FILES == 0
    /* if the checksum fails, unlink the bogus file */
    if ( unlink(file_name) == -1 ) {
      fprintf(stderr,_("Failed to unlink %s\n"),file_name);

      if ( errno )
        perror("unlink");

    }
    #endif
    free(file_name);

    return -1;

  } else {
    printf(_("Download of %s incomplete\n"),pkg->name);
    free(file_name);
    return -1;
  }

}

int progress_callback(void *clientp, double dltotal, double dlnow,
                      double ultotal, double ulnow)
{
  size_t percent = 0;
  size_t *bytes = (size_t *)clientp;
  (void) ultotal;
  (void) ulnow;

  if ( (dltotal + *bytes) == 0 ) {
    percent = 0;
  } else {
    percent = ((*bytes + dlnow)*100)/(dltotal + *bytes);
  }
  printf("%3d%%\b\b\b\b",(int)percent);
  return 0;
}

char spinner(void)
{
  static int spinner_index = 0;
  static const char spinner_parts[] = "\\|/-";

  if ( spinner_index > 3 ) {
    spinner_index = 0;
    return spinner_parts[spinner_index];
  } else {
    return spinner_parts[spinner_index++];
  }
}

