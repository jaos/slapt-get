/*
 * Copyright (C) 2003-2019 Jason Woodward <woodwardj at jaos dot org>
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

#define SLAPT_MAX_REGEX_PARTS 10
#define SLAPT_SLACK_BASE_SET_REGEX "^./slackware/a$"

typedef enum {
    SLAPT_OK = 0,
    SLAPT_MD5_CHECKSUM_MISMATCH,
    SLAPT_MD5_CHECKSUM_MISSING,
#ifdef SLAPT_HAS_GPGME
    SLAPT_GPG_KEY_IMPORTED,
    SLAPT_GPG_KEY_NOT_IMPORTED,
    SLAPT_GPG_KEY_UNCHANGED,
    SLAPT_CHECKSUMS_VERIFIED,
    SLAPT_CHECKSUMS_MISSING_KEY,
    SLAPT_CHECKSUMS_NOT_VERIFIED,
    SLAPT_CHECKSUMS_NOT_VERIFIED_NULL_CONTEXT,
    SLAPT_CHECKSUMS_NOT_VERIFIED_READ_CHECKSUMS,
    SLAPT_CHECKSUMS_NOT_VERIFIED_READ_SIGNATURE,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_REVOKED,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_EXPIRED,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SIG_EXPIRED,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_MISSING,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_TOO_OLD,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_BAD_POLICY,
    SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SYS_ERROR,
#endif
    SLAPT_DOWNLOAD_INCOMPLETE
} slapt_code_t;

typedef enum {
    SLAPT_PRIORITY_DEFAULT = 0,
    SLAPT_PRIORITY_DEFAULT_PATCH,
    SLAPT_PRIORITY_OFFICIAL,
    SLAPT_PRIORITY_OFFICIAL_PATCH,
    SLAPT_PRIORITY_PREFERRED,
    SLAPT_PRIORITY_PREFERRED_PATCH,
    SLAPT_PRIORITY_CUSTOM,
    SLAPT_PRIORITY_CUSTOM_PATCH
} SLAPT_PRIORITY_T;

#define SLAPT_PRIORITY_DEFAULT_TOKEN "DEFAULT"
#define SLAPT_PRIORITY_PREFERRED_TOKEN "PREFERRED"
#define SLAPT_PRIORITY_OFFICIAL_TOKEN "OFFICIAL"
#define SLAPT_PRIORITY_CUSTOM_TOKEN "CUSTOM"

typedef struct {
    regmatch_t pmatch[SLAPT_MAX_REGEX_PARTS];
    regex_t regex;
    size_t nmatch;
    int reg_return;
} slapt_regex_t;

typedef int (*slapt_vector_t_cmp)(const void *, const void *);
typedef int (*slapt_vector_t_qsort_cmp)(const void *, const void *);
typedef void (*slapt_vector_t_free_function)(void *);
typedef struct slapt_vector_t {
    uint32_t size;
    uint32_t capacity;
    slapt_vector_t_free_function free_function;
    bool sorted;
    void **items;
} slapt_vector_t;
slapt_vector_t *slapt_vector_t_init(slapt_vector_t_free_function);
void slapt_vector_t_free(slapt_vector_t *);
void slapt_vector_t_add(slapt_vector_t *, void *);
void slapt_vector_t_remove(slapt_vector_t *v, void *);
void slapt_vector_t_sort(slapt_vector_t *, slapt_vector_t_qsort_cmp);
int slapt_vector_t_index_of(slapt_vector_t *, slapt_vector_t_cmp, void *);
slapt_vector_t *slapt_vector_t_search(slapt_vector_t *, slapt_vector_t_cmp, void *);
#define slapt_vector_t_foreach(type, item, list) \
    type item;                                   \
    for (uint32_t item##_counter = 0; (item##_counter < list->size) && (item = list->items[item##_counter]); item##_counter++)

FILE *slapt_open_file(const char *file_name, const char *mode);
slapt_regex_t *slapt_regex_t_init(const char *regex_string);
void slapt_regex_t_execute(slapt_regex_t *regex_t, const char *string);
/* extract the string from the match, starts with 1 (not 0) */
char *slapt_regex_t_extract_match(const slapt_regex_t *r, const char *src, const int i);
void slapt_regex_t_free(slapt_regex_t *regex_t);
void slapt_create_dir_structure(const char *dir_name);
/* generate an md5sum of filehandle */
void slapt_gen_md5_sum_of_file(FILE *f, char *result_sum);

/* Ask the user to answer yes or no.
 * return 1 on yes, 0 on no, else -1.
 */
int slapt_ask_yes_no(const char *format, ...);
char *slapt_str_replace_chr(const char *string, const char find, const char replace);
void *slapt_malloc(size_t s);
void *slapt_calloc(size_t n, size_t s);

/* return human readable error */
const char *slapt_strerror(slapt_code_t code);
/* return human readable priority */
const char *slapt_priority_to_str(SLAPT_PRIORITY_T priority);
bool slapt_disk_space_check(const char *path, double space_needed);

/* utils */
slapt_vector_t *slapt_parse_delimited_list(char *line, char delim);
char *slapt_strip_whitespace(const char *s);
