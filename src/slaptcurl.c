/*
 * Copyright (C) 2003-2022 Jason Woodward <woodwardj at jaos dot org>
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

#include "main.h"
static size_t write_header_callback(void *buffer, size_t size, size_t nmemb, void *userp);

struct head_data_t {
    char *data;
    size_t size;
};

int slapt_download_data(FILE *fh, const char *url, size_t bytes, long *filetime, const slapt_config_t *global_config)
{
    struct slapt_progress_data *cb_data = slapt_init_progress_data();
    cb_data->bytes = bytes;

    CURL *ch = curl_easy_init();
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, fh);
    curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, PACKAGE);
    curl_easy_setopt(ch, CURLOPT_FTP_USE_EPSV, 0);
/* this is a check for slack 9.0's curl libs */
#ifdef CURLOPT_FTP_USE_EPRT
    curl_easy_setopt(ch, CURLOPT_FTP_USE_EPRT, 0);
#endif
    curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
#ifdef CURLOPT_HTTPAUTH
    curl_easy_setopt(ch, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
#endif
#ifdef CURLFTPMETHOD_NOCWD
    curl_easy_setopt(ch, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_NOCWD);
#endif
    curl_easy_setopt(ch, CURLOPT_FILETIME, 1);
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

    /* ugh, if someone wants to do this */
    if (getenv(SLAPT_NO_SSL_VERIFYPEER))
        curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0);
    if (getenv("SLAPT_CURL_DEBUG"))
        curl_easy_setopt(ch, CURLOPT_VERBOSE, 1);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Pragma: "); /* override no-cache */

    if (!global_config->dl_stats) {
        if (global_config->progress_cb == NULL) {
            curl_easy_setopt(ch, CURLOPT_XFERINFOFUNCTION, slapt_progress_callback);
        } else {
            curl_easy_setopt(ch, CURLOPT_XFERINFOFUNCTION, global_config->progress_cb);
        }
        curl_easy_setopt(ch, CURLOPT_PROGRESSDATA, cb_data);
    }

    /* resume */
    if (bytes > 0) {
        fseek(fh, 0, SEEK_END);
        curl_easy_setopt(ch, CURLOPT_RESUME_FROM, bytes);
    }

    int return_code = 0;
    CURLcode response = curl_easy_perform(ch);
    if (response != CURLE_OK) {
        return_code = response;
    }

    /* XXX Use CURLINFO_FILETIME_T with 7.59+ */
    if (filetime != NULL)
        curl_easy_getinfo(ch, CURLINFO_FILETIME, filetime);

    /* need to use curl_easy_cleanup() so that we don't have tons of open connections, getting rejected by ftp servers for being naughty. */
    curl_easy_cleanup(ch);
    /* can't do a curl_free() after curl_easy_cleanup() */
    /* curl_free(ch); */
    curl_slist_free_all(headers);
    slapt_free_progress_data(cb_data);

    return return_code;
}

static size_t write_header_callback(void *buffer, size_t size, size_t nmemb, void *userp)
{
    register int a_size = size * nmemb;
    struct head_data_t *head_t = (struct head_data_t *)userp;

    char *tmp = (char *)realloc(head_t->data, head_t->size + a_size + 1);
    if (tmp != NULL) {
        head_t->data = tmp;
        memcpy(&(head_t->data[head_t->size]), buffer, a_size);
        head_t->size += a_size;
        head_t->data[head_t->size] = '\0';
    }

    return nmemb;
}

char *slapt_head_request(const char *url)
{
    struct head_data_t head_t;
    head_t.data = NULL;
    head_t.size = 0;

    CURL *ch = curl_easy_init();
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_header_callback);
    curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, PACKAGE);
    curl_easy_setopt(ch, CURLOPT_FILE, &head_t);
    curl_easy_setopt(ch, CURLOPT_HEADER, 1);
    curl_easy_setopt(ch, CURLOPT_NOBODY, 1);
    curl_easy_setopt(ch, CURLOPT_FTP_USE_EPSV, 0);
/* this is a check for slack 9.0's curl libs */
#ifdef CURLOPT_FTP_USE_EPRT
    curl_easy_setopt(ch, CURLOPT_FTP_USE_EPRT, 0);
#endif
    curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1);
#ifdef CURLOPT_HTTPAUTH
    curl_easy_setopt(ch, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
#endif
#ifdef CURLFTPMETHOD_NOCWD
    curl_easy_setopt(ch, CURLOPT_FTP_FILEMETHOD, CURLFTPMETHOD_NOCWD);
#endif
    curl_easy_setopt(ch, CURLOPT_FILETIME, 1);
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

    if (getenv("SLAPT_CURL_DEBUG"))
        curl_easy_setopt(ch, CURLOPT_VERBOSE, 1);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Pragma: "); /* override no-cache */

    CURLcode response = curl_easy_perform(ch);
    if (response != CURLE_OK) {
        free(head_t.data);
        curl_easy_cleanup(ch);
        curl_slist_free_all(headers);
        return NULL;
    }

    curl_easy_cleanup(ch);
    curl_slist_free_all(headers);
    return head_t.data;
}

const char *slapt_get_mirror_data_from_source(FILE *fh, const slapt_config_t *global_config, const char *base_url, const char *filename)
{
    const int url_len = strlen(base_url) + strlen(filename) + 1;
    char url[url_len];
    const int written = snprintf(url, url_len, "%s%s", base_url, filename);
    if (written != (url_len - 1)) {
        fprintf(stderr, "slapt_get_mirror_data_from_source error for %s\n", base_url);
        exit(EXIT_FAILURE);
    }

    const int return_code = slapt_download_data(fh, url, 0, NULL, global_config);

    /* make sure we are back at the front of the file */
    /* DISABLED */
    /* rewind(fh); */
    return return_code != 0 ? curl_easy_strerror(return_code) : NULL;
}

const char *slapt_download_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg, const char *note)
{
    slapt_code_t verify = SLAPT_OK;
    bool is_interactive = slapt_is_interactive(global_config);

    if (pkg == NULL) {
        fprintf(stderr, "slapt_download_pkg() called without a package!\n");
        exit(EXIT_FAILURE);
    }

    if (slapt_verify_downloaded_pkg(global_config, pkg) == SLAPT_OK)
        return NULL;

    if (pkg->mirror == NULL || strlen(pkg->mirror) == 0) {
        fprintf(stderr, "slapt_download_pkg() called with a package that does not have a mirror!\n");
        exit(EXIT_FAILURE);
    }

    /* chdir(global_config->working_dir); */ /* just in case */
    slapt_create_dir_structure(pkg->location);

    /* build the url, file name, and get the file size if the file is present */
    char *url = slapt_pkg_t_url(pkg);
    char *file_name = slapt_gen_pkg_file_name(global_config, pkg);
    size_t f_size = slapt_get_pkg_file_size(global_config, pkg);
    int dl_total_size = pkg->size_c - (f_size / 1024);

    /* if file on disk is larger than the supposed size, unlink it */
    if (dl_total_size < 0) {
        if (unlink(file_name) == -1) {
            fprintf(stderr, gettext("Failed to unlink %s\n"), file_name);

            if (errno)
                perror(file_name);

            exit(EXIT_FAILURE);
        }
        dl_total_size = pkg->size_c;
        f_size = 0;
    }

    if (global_config->progress_cb == NULL) {
        if (note != NULL)
            printf("%s ", note);

        printf(gettext("Get %s %s %s [%.1f%s]..."),
               pkg->mirror, pkg->name, pkg->version,
               (dl_total_size > 1024) ? dl_total_size / 1024.0 : dl_total_size,
               (dl_total_size > 1024) ? "MB" : "kB");
        if (global_config->dl_stats)
            printf("\n");
    }

    /* open the file to write, append if already present */
    FILE *fh = slapt_open_file(file_name, "a+b");
    if (fh == NULL) {
        exit(EXIT_FAILURE);
    }

    /* download the file to our file handle */
    long filetime = 0;
    const int dl_return = slapt_download_data(fh, url, f_size, &filetime, global_config);
    if (dl_return == 0) {
        if (is_interactive)
            printf(gettext("Done\n"));

    } else if (dl_return == CURLE_HTTP_RANGE_ERROR ||
               dl_return == CURLE_FTP_BAD_DOWNLOAD_RESUME ||
               dl_return == CURLE_PARTIAL_FILE) {
        /* this is for errors trying to resume.  unlink the file and try again. */
        printf("\r");
        fclose(fh);

        if (unlink(file_name) == -1) {
            fprintf(stderr, gettext("Failed to unlink %s\n"), file_name);

            if (errno)
                perror(file_name);

            exit(EXIT_FAILURE);
        }

        free(file_name);
        free(url);

        /* if we set retry, make sure this counts as a retry */
        if (global_config->retry > 1)
            return curl_easy_strerror(dl_return);
        else
            return slapt_download_pkg(global_config, pkg, note);

    } else {
        fclose(fh);
#if SLAPT_DO_NOT_UNLINK_BAD_FILES == 0
        /* if the d/l fails, unlink the empty file */
        if (unlink(file_name) == -1) {
            fprintf(stderr, gettext("Failed to unlink %s\n"), file_name);

            if (errno)
                perror(file_name);
        }
#endif
        free(url);
        free(file_name);

        return curl_easy_strerror(dl_return);
    }

    fclose(fh);
    free(url);

    /* preserve the file access and modification time */
    if (filetime > 0) {
        struct utimbuf times;
        times.actime = (time_t)filetime;
        times.modtime = (time_t)filetime;
        utime(file_name, &times); /* set the time we got */
    }

    /* check to make sure we have the complete file */
    verify = slapt_verify_downloaded_pkg(global_config, pkg);
    if (verify == SLAPT_OK) {
        free(file_name);
        return NULL;

    } else {
#if SLAPT_DO_NOT_UNLINK_BAD_FILES == 0
        /* if the checksum fails, unlink the bogus file */
        if (unlink(file_name) == -1) {
            fprintf(stderr, gettext("Failed to unlink %s\n"), file_name);

            if (errno)
                perror("unlink");
        }
#endif

        free(file_name);
        return slapt_strerror(verify);
    }
}

int slapt_progress_callback(void *clientp, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)
{
    size_t percent = 0;
    struct slapt_progress_data *cb_data = (struct slapt_progress_data *)clientp;
    const size_t bytes = cb_data->bytes;
    (void)ultotal;
    (void)ulnow;

    if ((dltotal + bytes) == 0) {
        percent = 0;
    } else {
        percent = ((bytes + dlnow) * 100) / (dltotal + bytes);
    }
    printf("%3d%%\b\b\b\b", (int)percent);
    return 0;
}

void slapt_write_head_cache(const char *cache, const char *cache_filename)
{
    char *head_filename = slapt_gen_head_cache_filename(cache_filename);

    /* store the last modified date */
    FILE *tmp = slapt_open_file(head_filename, "w");
    if (tmp == NULL) {
        exit(EXIT_FAILURE);
    }

    fprintf(tmp, "%s", cache);
    fclose(tmp);

    free(head_filename);
}

char *slapt_read_head_cache(const char *cache_filename)
{
    char *head_filename = slapt_gen_head_cache_filename(cache_filename);

    FILE *tmp = slapt_open_file(head_filename, "a+");
    free(head_filename);

    if (tmp == NULL)
        exit(EXIT_FAILURE);

    rewind(tmp);

    char *getline_buffer = NULL;
    size_t gl_n;
    ssize_t gl_return_size;
    gl_return_size = getline(&getline_buffer, &gl_n, tmp);
    fclose(tmp);

    if (gl_return_size == -1) {
        free(getline_buffer);
        return NULL;
    }

    return getline_buffer;
}

char *slapt_gen_head_cache_filename(const char *filename_from_url)
{
    const size_t head_filename_len = strlen(filename_from_url) + strlen(SLAPT_HEAD_FILE_EXT) + 1;
    char *head_filename = slapt_calloc(head_filename_len, sizeof *head_filename);
    if (snprintf(head_filename, head_filename_len, "%s%s", filename_from_url, SLAPT_HEAD_FILE_EXT) < 1) {
        fprintf(stderr, "slapt_gen_head_cache_filename error for %s\n", filename_from_url);
        exit(EXIT_FAILURE);
    }
    return head_filename;
}

void slapt_clear_head_cache(const char *cache_filename)
{
    char *head_filename = slapt_gen_head_cache_filename(cache_filename);

    FILE *tmp = slapt_open_file(head_filename, "w");

    if (tmp == NULL) {
        exit(EXIT_FAILURE);
    }

    fclose(tmp);
    free(head_filename);
}

/* do a head request on the mirror data to find out if it's new */
char *slapt_head_mirror_data(const char *wurl, const char *file)
{
    /* build url */
    const int url_len = strlen(wurl) + strlen(file) + 1;
    char url[url_len];
    const int written = snprintf(url, url_len, "%s%s", wurl, file);
    if (written != (url_len - 1)) {
        fprintf(stderr, "slapt_head_mirror_data error for %s\n", wurl);
        exit(EXIT_FAILURE);
    }

    /* retrieve the header info */
    char *head_data = slapt_head_request(url);
    if (head_data == NULL) {
        return NULL;
    }

    /* extract the last modified date for storage and later comparison */
    char *request_header_ptr = strcasestr(head_data, "Last-Modified");
    if (request_header_ptr == NULL) {
        /* this is ftp, in which case the Content-Length will have to do */
        request_header_ptr = strcasestr(head_data, "Content-Length");
        if (request_header_ptr == NULL || strstr(wurl, "ftp") == NULL) {
            free(head_data);
            return NULL;
        } /* give up finally */
    }
    char *delim_ptr = strpbrk(request_header_ptr, "\r\n");
    if (delim_ptr == NULL) {
        free(head_data);
        return NULL;
    }

    const int request_header_len = strlen(request_header_ptr) - strlen(delim_ptr);
    char *request_header = slapt_calloc(request_header_len + 1, sizeof *request_header);
    memcpy(request_header, request_header_ptr, request_header_len);

    free(head_data);
    return request_header;
}

struct slapt_progress_data *slapt_init_progress_data(void)
{
    struct slapt_progress_data *d = slapt_malloc(sizeof *d);
    d->bytes = 0;
    d->start = time(NULL);
    return d;
}

void slapt_free_progress_data(struct slapt_progress_data *d)
{
    free(d);
}
