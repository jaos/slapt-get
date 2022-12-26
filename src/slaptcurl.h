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

#define SLAPT_NO_SSL_VERIFYPEER "SLAPT_NO_SSL_VERIFYPEER"

/* download data to file, resuming from bytes and preserving filetime.  returns curl code */
int slapt_download_data(FILE *fh, const char *url, size_t bytes, long *filetime, const slapt_config_t *global_config);

/* retrieves the head data for the url, returns (char *) or NULL on error */
char *slapt_head_request(const char *url);

/* this fills FILE with data from url, used for PACKAGES.TXT and CHECKSUMS Returns error on failure.  */
const char *slapt_get_mirror_data_from_source(FILE *fh, const slapt_config_t *global_config, const char *base_url, const char *filename);

/* download pkg, calls download_data.  returns error on failure.  */
const char *slapt_download_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg, const char *note);

/* this is the default progress callback if global_config->progress_cb == NULL */
int slapt_progress_callback(void *clientp, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow);

/* generate the head cache filename, caller responsible for freeing the returned data */
char *slapt_gen_head_cache_filename(const char *filename_from_url);
/* do a head request on the mirror data to find out if it's new returns (char *) or NULL */
char *slapt_head_mirror_data(const char *wurl, const char *file);
/* clear head cache storage file */
void slapt_clear_head_cache(const char *cache_filename);
/* cache the head request */
void slapt_write_head_cache(const char *cache, const char *cache_filename);
/* read the cached head request, returns (char *) or NULL */
char *slapt_read_head_cache(const char *cache_filename);

struct slapt_progress_data {
    size_t bytes;
    time_t start;
};

struct slapt_progress_data *slapt_init_progress_data(void);
void slapt_free_progress_data(struct slapt_progress_data *d);
