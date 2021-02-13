/*
 * Copyright (C) 2003-2021 Jason Woodward <woodwardj at jaos dot org>
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

FILE *slapt_open_file(const char *file_name, const char *mode)
{
    FILE *fh = NULL;
    if ((fh = fopen(file_name, mode)) == NULL) {
        fprintf(stderr, gettext("Failed to open %s\n"), file_name);

        if (errno)
            perror(file_name);

        return NULL;
    }
    return fh;
}

/* initialize regex structure and compile the regular expression */
slapt_regex_t *slapt_regex_t_init(const char *regex_string)
{
    slapt_regex_t *r;

    if (regex_string == NULL)
        return NULL;

    r = slapt_malloc(sizeof *r);

    r->nmatch = SLAPT_MAX_REGEX_PARTS;
    r->reg_return = -1;

    /* compile our regex */
    r->reg_return = regcomp(&r->regex, regex_string, REG_EXTENDED | REG_NEWLINE | REG_ICASE);
    if (r->reg_return != 0) {
        size_t regerror_size;
        char errbuf[1024];
        size_t errbuf_size = 1024;
        fprintf(stderr, gettext("Failed to compile regex\n"));

        if ((regerror_size = regerror(r->reg_return, &r->regex, errbuf, errbuf_size)) != 0) {
            printf(gettext("Regex Error: %s\n"), errbuf);
        }
        free(r);
        return NULL;
    }

    return r;
}

/* execute the regular expression and set the return code in the passed in structure */
void slapt_regex_t_execute(slapt_regex_t *r, const char *string)
{
    r->reg_return = regexec(&r->regex, string, r->nmatch, r->pmatch, 0);
}

char *slapt_regex_t_extract_match(const slapt_regex_t *r, const char *src, const int i)
{
    regmatch_t m = r->pmatch[i];
    char *str = NULL;

    if (m.rm_so != -1) {
        uint32_t len = m.rm_eo - m.rm_so + 1;
        str = slapt_malloc(sizeof *str * len);

        slapt_strlcpy(str, src + m.rm_so, len);
    }

    return str;
}

void slapt_regex_t_free(slapt_regex_t *r)
{
    regfree(&r->regex);
    free(r);
}

void slapt_gen_md5_sum_of_file(FILE *f, char *result_sum)
{
    EVP_MD_CTX *mdctx = NULL;
    const EVP_MD *md;
    unsigned char md_value[EVP_MAX_MD_SIZE];
    uint32_t md_len = 0;
    ssize_t getline_read;
    size_t getline_size;
    char *getline_buffer = NULL;

    md = EVP_md5();

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    mdctx = EVP_MD_CTX_new();
#else
    mdctx = slapt_malloc(sizeof *mdctx);
    EVP_MD_CTX_init(mdctx);
#endif
    EVP_DigestInit_ex(mdctx, md, NULL);

    rewind(f);

    while ((getline_read = getline(&getline_buffer, &getline_size, f)) != EOF)
        EVP_DigestUpdate(mdctx, getline_buffer, getline_read);

    free(getline_buffer);

    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    EVP_MD_CTX_free(mdctx);
#else
    free(mdctx);
#endif

    result_sum[0] = '\0';

    for (uint32_t i = 0; i < md_len; ++i) {
        if (snprintf(result_sum + strlen(result_sum), 3, "%02x", md_value[i]) != 2) {
            fprintf(stderr, "slapt_gen_md5_sum_of_file failed\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* recursively create dirs */
void slapt_create_dir_structure(const char *dir_name)
{
    char *cwd = NULL;
    int position = 0, len = 0;

    cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        fprintf(stderr, gettext("Failed to get cwd\n"));
        return;
    }

    len = strlen(dir_name);
    while (position < len) {
        char *pointer = NULL;
        char *dir_name_buffer = NULL;

        /* if no more directory delim, then this must be last dir */
        if (strstr(dir_name + position, "/") == NULL) {
            dir_name_buffer = strndup(dir_name + position, strlen(dir_name + position));

            if (strcmp(dir_name_buffer, ".") != 0) {
                if ((mkdir(dir_name_buffer, 0755)) == -1) {
                    if (errno != EEXIST) {
                        fprintf(stderr, gettext("Failed to mkdir: %s\n"), dir_name_buffer);
                        exit(EXIT_FAILURE);
                    }
                }
                if ((chdir(dir_name_buffer)) == -1) {
                    fprintf(stderr, gettext("Failed to chdir to %s\n"), dir_name_buffer);
                    exit(EXIT_FAILURE);
                }
            } /* don't create . */

            free(dir_name_buffer);
            break;
        } else {
            if (dir_name[position] == '/') {
                /* move on ahead */
                ++position;
            } else {
                /* figure our dir name and mk it */
                pointer = strchr(dir_name + position, '/');
                dir_name_buffer = strndup(dir_name + position, strlen(dir_name + position) - strlen(pointer));

                if (strcmp(dir_name_buffer, ".") != 0) {
                    if ((mkdir(dir_name_buffer, 0755)) == -1) {
                        if (errno != EEXIST) {
                            fprintf(stderr, gettext("Failed to mkdir: %s\n"), dir_name_buffer);
                            exit(EXIT_FAILURE);
                        }
                    }
                    if ((chdir(dir_name_buffer)) == -1) {
                        fprintf(stderr, gettext("Failed to chdir to %s\n"), dir_name_buffer);
                        exit(EXIT_FAILURE);
                    }
                } /* don't create . */

                free(dir_name_buffer);
                position += (strlen(dir_name + position) - strlen(pointer));
            }
        }
    } /* end while */

    if ((chdir(cwd)) == -1) {
        fprintf(stderr, gettext("Failed to chdir to %s\n"), cwd);
        return;
    }

    free(cwd);
}

int slapt_ask_yes_no(const char *format, ...)
{
    va_list arg_list;
    int answer, parsed_answer = 0;

    va_start(arg_list, format);
    vprintf(format, arg_list);
    va_end(arg_list);

    while ((answer = fgetc(stdin)) != EOF) {
        if (answer == '\n')
            break;

        if (((tolower(answer) == 'y') || (tolower(answer) == 'n')) && parsed_answer == 0)
            parsed_answer = tolower(answer);
    }

    if (parsed_answer == 'y')
        return 1;

    if (parsed_answer == 'n')
        return 0;

    return -1;
}

char *slapt_str_replace_chr(const char *string, const char find, const char replace)
{
    uint32_t len = 0;
    char *clean = slapt_calloc(strlen(string) + 1, sizeof *clean);
    ;

    len = strlen(string);
    for (uint32_t i = 0; i < len; ++i) {
        if (string[i] == find) {
            clean[i] = replace;
        } else {
            clean[i] = string[i];
        }
    }
    clean[strlen(string)] = '\0';

    return clean;
}

void *slapt_malloc(size_t s)
{
    void *p;
    if (!(p = malloc(s))) {
        fprintf(stderr, gettext("Failed to malloc\n"));

        if (errno)
            perror("malloc");

        exit(EXIT_FAILURE);
    }
    return p;
}

void *slapt_calloc(size_t n, size_t s)
{
    void *p;
    if (!(p = calloc(n, s))) {
        fprintf(stderr, gettext("Failed to calloc\n"));

        if (errno)
            perror("calloc");

        exit(EXIT_FAILURE);
    }
    return p;
}

const char *slapt_strerror(slapt_code_t code)
{
    switch (code) {
    case SLAPT_OK:
        return "No error";
    case SLAPT_MD5_CHECKSUM_MISMATCH:
        return gettext("MD5 checksum mismatch, override with --no-md5");
    case SLAPT_MD5_CHECKSUM_MISSING:
        return gettext("Missing MD5 checksum, override with --no-md5");
    case SLAPT_DOWNLOAD_INCOMPLETE:
        return gettext("Incomplete download");
#ifdef SLAPT_HAS_GPGME
    case SLAPT_GPG_KEY_IMPORTED:
        return gettext("GPG key successfully imported");
    case SLAPT_GPG_KEY_NOT_IMPORTED:
        return gettext("GPG key could not be imported");
    case SLAPT_GPG_KEY_UNCHANGED:
        return gettext("GPG key already present");
    case SLAPT_CHECKSUMS_VERIFIED:
        return gettext("Checksums signature successfully verified");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_NULL_CONTEXT:
        return gettext("Not verified: null context");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_READ_CHECKSUMS:
        return gettext("Checksums not read");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_READ_SIGNATURE:
        return gettext("Signature not read");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_REVOKED:
        return gettext("Not Verified: key revoked");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_EXPIRED:
        return gettext("Not Verified: key expired");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SIG_EXPIRED:
        return gettext("Not Verified: signature expired");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_MISSING:
        return gettext("Not Verified: missing CRL");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_TOO_OLD:
        return gettext("Not Verified: CRL too old");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_BAD_POLICY:
        return gettext("Not Verified: bad policy");
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SYS_ERROR:
        return gettext("Not Verified: system error");
    case SLAPT_CHECKSUMS_NOT_VERIFIED:
        return gettext("Checksums signature could not be verified");
    case SLAPT_CHECKSUMS_MISSING_KEY:
        return gettext("No key for verification");
#endif
    default:
        return gettext("Unknown error");
    };
}

const char *slapt_priority_to_str(SLAPT_PRIORITY_T priority)
{
    switch (priority) {
    case SLAPT_PRIORITY_DEFAULT:
        return gettext("Default");
    case SLAPT_PRIORITY_DEFAULT_PATCH:
        return gettext("Default Patch");
    case SLAPT_PRIORITY_PREFERRED:
        return gettext("Preferred");
    case SLAPT_PRIORITY_PREFERRED_PATCH:
        return gettext("Preferred Patch");
    case SLAPT_PRIORITY_OFFICIAL:
        return gettext("Official");
    case SLAPT_PRIORITY_OFFICIAL_PATCH:
        return gettext("Official Patch");
    case SLAPT_PRIORITY_CUSTOM:
        return gettext("Custom");
    case SLAPT_PRIORITY_CUSTOM_PATCH:
        return gettext("Custom Patch");
    default:
        return NULL;
    };
}

bool slapt_disk_space_check(const char *path, double space_needed)
{
    struct statvfs statvfs_buf;

    if (space_needed < 0)
        return true;

    space_needed *= 1024;

    if (statvfs(path, &statvfs_buf) != 0) {
        if (errno)
            perror("statvfs");

        return false;
    } else {
        if (statvfs_buf.f_bavail < (space_needed / statvfs_buf.f_bsize))
            return false;
    }

    return true;
}

slapt_vector_t *slapt_parse_delimited_list(char *line, char delim)
{
    slapt_vector_t *list = slapt_vector_t_init(free);
    int count = 0, position = 0, len = strlen(line);

    while (isspace(line[position]) != 0)
        ++position;

    while (position < len) {
        char *start = line + position;
        char *end = NULL, *ptr = NULL;
        int start_len = strlen(start), end_len = 0;

        if (strchr(start, delim) != NULL) {
            if (line[position] == delim || isspace(line[position]) != 0) {
                ++position;
                continue;
            }
            end = strchr(start, delim);
        }

        if (end != NULL)
            end_len = strlen(end);

        ptr = strndup(start, start_len - end_len);

        slapt_vector_t_add(list, ptr);
        ++count;

        position += start_len - end_len;
    }

    return list;
}

char *slapt_strip_whitespace(const char *s)
{
    int len = strlen(s);
    while (isspace(s[len - 1]))
        --len;
    while (*s && isspace(*s))
        ++s, --len;
    return strndup(s, len);
}

slapt_vector_t *slapt_vector_t_init(slapt_vector_t_free_function f)
{
    slapt_vector_t *v = NULL;
    v = slapt_malloc(sizeof *v);
    v->size = 0;
    v->capacity = 0;
    v->items = slapt_malloc(sizeof *v->items);
    v->free_function = f;
    v->sorted = false;
    return v;
}

void slapt_vector_t_free(slapt_vector_t *v)
{
    if (!v)
        return;

    if (v->free_function) {
        for (uint32_t i = 0; i < v->size; i++) {
            v->free_function(v->items[i]);
        }
    }
    free(v->items);
    free(v);
}

void slapt_vector_t_add(slapt_vector_t *v, void *a)
{
    if (v->capacity == v->size) {
        uint32_t new_capacity = (v->capacity + 1) * 2;
        v->items = realloc(v->items, sizeof *v->items * new_capacity);
        if (v->items == NULL) {
            exit(EXIT_FAILURE);
        }
        v->capacity = new_capacity;
    }
    v->items[v->size++] = a;
    v->sorted = false;
}

void slapt_vector_t_remove(slapt_vector_t *v, void *j)
{
    bool found = false;
    for (uint32_t i = 0; i < v->size; i++) {
        if (j == v->items[i]) {
            if (v->free_function) {
                v->free_function(v->items[i]);
            }
            found = true;
        }
        if (found && ((i + 1) < v->size)) {
            v->items[i] = v->items[i + 1];
        }
    }
    if (found) {
        v->size--;
    }
}

void slapt_vector_t_sort(slapt_vector_t *v, slapt_vector_t_qsort_cmp cmp)
{
    if (v->size > 0) {
        qsort(v->items, v->size, sizeof(v->items[0]), cmp);
        v->sorted = true;
    }
}

int slapt_vector_t_index_of(slapt_vector_t *v, slapt_vector_t_cmp cmp, void *opt)
{
    if (v->sorted) {
        int min = 0, max = v->size - 1;

        while (max >= min) {
            int pivot = (min + max) / 2;
            int cmpv = cmp(v->items[pivot], opt);
            if (cmpv == 0) {
                return pivot;
            } else {
                if (cmpv < 0)
                    min = pivot + 1;
                else
                    max = pivot - 1;
            }
        }
    } else {
        for (uint32_t i = 0; i < v->size; i++) {
            if (cmp(v->items[i], opt) == 0) {
                return i;
            }
        }
    }
    return -1;
}

slapt_vector_t *slapt_vector_t_search(slapt_vector_t *v, slapt_vector_t_cmp cmp, void *opt)
{
    slapt_vector_t *matches = slapt_vector_t_init(NULL);

    if (v->sorted) {
        uint32_t min = 0, max = v->size - 1;
        int idx = slapt_vector_t_index_of(v, cmp, opt);
        if (idx < 0) {
            slapt_vector_t_free(matches);
            return NULL;
        }

        uint32_t start = idx, end = idx;

        while (start > min) {
            if (cmp(v->items[start - 1], opt) == 0) {
                start -= 1;
            } else {
                break;
            }
        }

        while (end < max) {
            if (cmp(v->items[end + 1], opt) == 0) {
                end += 1;
            } else {
                break;
            }
        }

        for (uint32_t i = start; i <= end; i++) {
            slapt_vector_t_add(matches, v->items[i]);
        }

    } else {
        for (uint32_t i = 0; i < v->size; i++) {
            if (cmp(v->items[i], opt) == 0) {
                slapt_vector_t_add(matches, v->items[i]);
            }
        }
    }

    if (!matches->size) {
        slapt_vector_t_free(matches);
        return NULL;
    }
    return matches;
}

/* based on https://lwn.net/Articles/612257/ (LGPLv2.1+) */
size_t slapt_strlcpy(char *dst, const char *src, size_t size)
{
    if (size == 0) {
        return 0;
    }

    size_t src_length = strnlen (src, size);
    if (src_length >= size) {
        if (src_length != size) {
            fprintf(stderr, "Truncating %s [%zd to %zd]\n", src, size, src_length);
            exit(EXIT_FAILURE);
        }
        memcpy (dst, src, size);
        dst[size - 1] = '\0';
        return size - 1;
    } else {
        memcpy (dst, src, src_length + 1);
        return src_length;
    }
}
