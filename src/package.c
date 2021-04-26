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

struct slapt_pkg_version_parts {
    char **parts;
    uint32_t count;
};

/* analyze the pkg version hunk by hunk */
static struct slapt_pkg_version_parts *break_down_pkg_version(const char *version);
/* called by slapt_is_required_by */
static void required_by(const slapt_vector_t *avail,
                        const slapt_vector_t *installed_pkgs,
                        slapt_vector_t *pkgs_to_install,
                        slapt_vector_t *pkgs_to_remove,
                        const slapt_pkg_t *pkg,
                        slapt_vector_t *required_by_list);
/* free pkg_version_parts struct */
static void slapt_pkg_t_free_version_parts(struct slapt_pkg_version_parts *parts);
/* uncompress compressed package data */
static FILE *slapt_gunzip_file(const char *file_name, FILE *dest_file);

#ifdef SLAPT_HAS_GPGME
/* check if signature is unauthenticated by "acceptable" reasons */
bool slapt_pkg_sign_is_unauthenticated(slapt_code_t code);
#endif

/* parse the PACKAGES.TXT file */
slapt_vector_t *slapt_get_available_pkgs(void)
{
    /* open pkg list */
    FILE *pkg_list_fh = slapt_open_file(SLAPT_PKG_LIST_L, "r");
    if (pkg_list_fh == NULL) {
        fprintf(stderr, gettext("Perhaps you want to run --update?\n"));
        return slapt_vector_t_init(NULL); /* return an empty list */
    }
    slapt_vector_t *list = slapt_parse_packages_txt(pkg_list_fh);
    fclose(pkg_list_fh);

    /* this is pointless to do if we wrote the data sorted, but this
     ensures upgrades from older, presorting slapt-gets still work
     as expected. */
    slapt_vector_t_sort(list, slapt_pkg_t_qsort_cmp);
    list->sorted = true;

    return list;
}

slapt_vector_t *slapt_parse_packages_txt(FILE *pkg_list_fh)
{
    /* compile our regexen */
    slapt_regex_t *name_regex = slapt_regex_t_init(SLAPT_PKG_NAME_PATTERN);
    if (name_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *mirror_regex = slapt_regex_t_init(SLAPT_PKG_MIRROR_PATTERN);
    if (mirror_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *priority_regex = slapt_regex_t_init(SLAPT_PKG_PRIORITY_PATTERN);
    if (priority_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *location_regex = slapt_regex_t_init(SLAPT_PKG_LOCATION_PATTERN);
    if (location_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *size_c_regex = slapt_regex_t_init(SLAPT_PKG_SIZEC_PATTERN);
    if (size_c_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *size_u_regex = slapt_regex_t_init(SLAPT_PKG_SIZEU_PATTERN);
    if (size_u_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_vector_t *list = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    ssize_t bytes_read;
    size_t getline_len = 0;
    char *getline_buffer = NULL;
    char *char_pointer = NULL;
    while ((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) != EOF) {
        slapt_pkg_t *tmp_pkg;

        getline_buffer[bytes_read - 1] = '\0';

        /* pull out package data */
        if (strstr(getline_buffer, "PACKAGE NAME") == NULL)
            continue;

        slapt_regex_t_execute(name_regex, getline_buffer);

        /* skip this line if we didn't find a package name */
        if (name_regex->reg_return != 0) {
            fprintf(stderr, gettext("regex failed on [%s]\n"), getline_buffer);
            continue;
        }
        /* otherwise keep going and parse out the rest of the pkg data */

        tmp_pkg = slapt_pkg_t_init();

        /* pkg name base */
        tmp_pkg->name = slapt_regex_t_extract_match(name_regex, getline_buffer, 1);
        /* pkg version */
        tmp_pkg->version = slapt_regex_t_extract_match(name_regex, getline_buffer, 2);
        /* file extension */
        tmp_pkg->file_ext = slapt_regex_t_extract_match(name_regex, getline_buffer, 3);

        /* mirror */
        long f_pos = ftell(pkg_list_fh);
        if (getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF) {
            slapt_regex_t_execute(mirror_regex, getline_buffer);

            if (mirror_regex->reg_return == 0) {
                tmp_pkg->mirror = slapt_regex_t_extract_match(mirror_regex, getline_buffer, 1);
            } else {
                /* mirror isn't provided... rewind one line */
                fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
            }
        }

        /* priority */
        f_pos = ftell(pkg_list_fh);
        if (getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF) {
            slapt_regex_t_execute(priority_regex, getline_buffer);

            if (priority_regex->reg_return == 0) {
                char *priority_string = slapt_regex_t_extract_match(priority_regex, getline_buffer, 1);
                if (priority_string != NULL) {
                    tmp_pkg->priority = atoi(priority_string);
                    free(priority_string);
                }
            } else {
                /* priority isn't provided... rewind one line */
                fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
            }
        }

        /* location */
        if ((getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF)) {
            slapt_regex_t_execute(location_regex, getline_buffer);

            if (location_regex->reg_return == 0) {
                tmp_pkg->location = slapt_regex_t_extract_match(location_regex, getline_buffer, 1);

#if SLACKWARE_EXTRA_TESTING_PASTURE_WORKAROUND == 1
                /* extra, testing, and pasture support
                  they add in extraneous /extra/, /testing/, or /pasture/ in the
                  PACKAGES.TXT location. this fixes the downloads and md5 checksum
                  matching
                */
                if (strstr(tmp_pkg->location, "./testing/") != NULL) {
                    char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - 7)); /* ./testing - 2 */
                    tmp_location[0] = '.';
                    tmp_location[1] = '\0';
                    strncat(tmp_location, &tmp_pkg->location[0] + 9, strlen(tmp_pkg->location) - 9); /* ./testing */
                    free(tmp_pkg->location);
                    tmp_pkg->location = tmp_location;
                } else if (strstr(tmp_pkg->location, "./extra/") != NULL) {
                    char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - 5)); /* ./extra/ - 2 */
                    tmp_location[0] = '.';
                    tmp_location[1] = '\0';
                    strncat(tmp_location, &tmp_pkg->location[0] + 7, strlen(tmp_pkg->location) - 7); /* ./extra */
                    free(tmp_pkg->location);
                    tmp_pkg->location = tmp_location;
                } else if (strstr(tmp_pkg->location, "./pasture") != NULL) {
                    char *tmp_location = slapt_malloc(sizeof *tmp_location * (strlen(tmp_pkg->location) - 7)); /* ./pasture - 2 */
                    tmp_location[0] = '.';
                    tmp_location[1] = '\0';
                    strncat(tmp_location, &tmp_pkg->location[0] + 9, strlen(tmp_pkg->location) - 9); /* ./pasture */
                    free(tmp_pkg->location);
                    tmp_pkg->location = tmp_location;
                }
#endif

            } else {
                fprintf(stderr, gettext("regexec failed to parse location\n"));
                slapt_pkg_t_free(tmp_pkg);
                continue;
            }
        } else {
            fprintf(stderr, gettext("getline reached EOF attempting to read location\n"));
            slapt_pkg_t_free(tmp_pkg);
            continue;
        }

        /* size_c */
        if ((getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF)) {
            char *size_c = NULL;

            slapt_regex_t_execute(size_c_regex, getline_buffer);

            if (size_c_regex->reg_return == 0) {
                size_c = slapt_regex_t_extract_match(size_c_regex, getline_buffer, 1);
                tmp_pkg->size_c = strtol(size_c, (char **)NULL, 10);
                free(size_c);
            } else {
                fprintf(stderr, gettext("regexec failed to parse size_c\n"));
                slapt_pkg_t_free(tmp_pkg);
                continue;
            }
        } else {
            fprintf(stderr, gettext("getline reached EOF attempting to read size_c\n"));
            slapt_pkg_t_free(tmp_pkg);
            continue;
        }

        /* size_u */
        if ((getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF)) {
            char *size_u = NULL;

            slapt_regex_t_execute(size_u_regex, getline_buffer);

            if (size_u_regex->reg_return == 0) {
                size_u = slapt_regex_t_extract_match(size_u_regex, getline_buffer, 1);
                tmp_pkg->size_u = strtol(size_u, (char **)NULL, 10);
                free(size_u);
            } else {
                fprintf(stderr, gettext("regexec failed to parse size_u\n"));
                slapt_pkg_t_free(tmp_pkg);
                continue;
            }
        } else {
            fprintf(stderr, gettext("getline reached EOF attempting to read size_u\n"));
            slapt_pkg_t_free(tmp_pkg);
            continue;
        }

        /* required, if provided */
        f_pos = ftell(pkg_list_fh);
        if (((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) != EOF) && ((char_pointer = strstr(getline_buffer, "PACKAGE REQUIRED")) != NULL)) {
            const size_t req_len = 18; /* "PACKAGE REQUIRED" + 2 */
            getline_buffer[bytes_read - 1] = '\0';

            const size_t substr_len = strlen(char_pointer + req_len) + 1;
            char *tmp_realloc = realloc(tmp_pkg->required, sizeof *tmp_pkg->required * substr_len);
            if (tmp_realloc != NULL) {
                tmp_pkg->required = tmp_realloc;
                slapt_strlcpy(tmp_pkg->required, char_pointer + req_len, substr_len);
            }

            // parse into tmp_pkg->dependencies
            slapt_vector_t *dep_parts = slapt_parse_delimited_list(tmp_pkg->required, ',');
            slapt_vector_t_foreach(const char *, dep_part, dep_parts) {
                slapt_dependency_t *dependency_declaration = slapt_dependency_t_parse_required(dep_part);
                slapt_vector_t_add(tmp_pkg->dependencies, dependency_declaration);
            }
            slapt_vector_t_free(dep_parts);
        } else {
            /* required isn't provided... rewind one line */
            fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
        }

        /* conflicts, if provided */
        f_pos = ftell(pkg_list_fh);
        if (((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) != EOF) && ((char_pointer = strstr(getline_buffer, "PACKAGE CONFLICTS")) != NULL)) {
            const size_t req_len = 19; /* "PACKAGE CONFLICTS" + 2 */
            getline_buffer[bytes_read - 1] = '\0';

            const size_t substr_len = strlen(char_pointer + req_len) + 1;
            char *tmp_realloc = realloc(tmp_pkg->conflicts, sizeof *tmp_pkg->conflicts * substr_len);
            if (tmp_realloc != NULL) {
                tmp_pkg->conflicts = tmp_realloc;
                slapt_strlcpy(tmp_pkg->conflicts, char_pointer + req_len, substr_len);
            }
        } else {
            /* conflicts isn't provided... rewind one line */
            fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
        }

        /* suggests, if provided */
        f_pos = ftell(pkg_list_fh);
        if (((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) != EOF) && ((char_pointer = strstr(getline_buffer, "PACKAGE SUGGESTS")) != NULL)) {
            const size_t req_len = 18; /* "PACKAGE SUGGESTS" + 2 */
            getline_buffer[bytes_read - 1] = '\0';

            const size_t substr_len = strlen(char_pointer + req_len) + 1;
            char *tmp_realloc = realloc(tmp_pkg->suggests, sizeof *tmp_pkg->suggests * substr_len);
            if (tmp_realloc != NULL) {
                tmp_pkg->suggests = tmp_realloc;
                slapt_strlcpy(tmp_pkg->suggests, char_pointer + req_len, substr_len);
            }
        } else {
            /* suggests isn't provided... rewind one line */
            fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
        }

        /* md5 checksum */
        f_pos = ftell(pkg_list_fh);
        if (((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) != EOF) && (strstr(getline_buffer, "PACKAGE MD5") != NULL)) {
            char *md5sum;
            getline_buffer[bytes_read - 1] = '\0';
            md5sum = (char *)strpbrk(getline_buffer, ":") + 1;
            while (*md5sum != 0 && isspace(*md5sum)) {
                md5sum++;
            }
            /* don't overflow the buffer */
            if (strlen(md5sum) > SLAPT_MD5_STR_LEN) {
                fprintf(stderr, gettext("md5 sum too long\n"));
                slapt_pkg_t_free(tmp_pkg);
                continue;
            }

            slapt_strlcpy(tmp_pkg->md5, md5sum, SLAPT_MD5_STR_LEN + 1);
        } else {
            /* md5 sum isn't provided... rewind one line */
            fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
        }

        /* description */
        if ((getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF) && (strstr(getline_buffer, "PACKAGE DESCRIPTION") != NULL)) {
            while (1) {
                char *tmp_desc = NULL;

                if ((bytes_read = getline(&getline_buffer, &getline_len, pkg_list_fh)) == EOF) {
                    break;
                }

                if (strcmp(getline_buffer, "\n") == 0) {
                    break;
                }

                tmp_desc = realloc(tmp_pkg->description, sizeof *tmp_pkg->description * (strlen(tmp_pkg->description) + bytes_read + 1));
                if (tmp_desc == NULL) {
                    break;
                }
                tmp_pkg->description = tmp_desc;
                strncat(tmp_pkg->description, getline_buffer, bytes_read);
            }
        } else {
            fprintf(stderr, gettext("error attempting to read pkg description\n"));
            slapt_pkg_t_free(tmp_pkg);
            continue;
        }

        /* fillin details */
        if (tmp_pkg->location == NULL) {
            tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
            tmp_pkg->location[0] = '\0';
        }
        if (tmp_pkg->description == NULL) {
            tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->description);
            tmp_pkg->description[0] = '\0';
        }
        if (tmp_pkg->mirror == NULL) {
            tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->mirror);
            tmp_pkg->mirror[0] = '\0';
        }

        slapt_vector_t_add(list, tmp_pkg);
        tmp_pkg = NULL;
    }

    if (getline_buffer)
        free(getline_buffer);

    slapt_regex_t_free(name_regex);
    slapt_regex_t_free(mirror_regex);
    slapt_regex_t_free(priority_regex);
    slapt_regex_t_free(location_regex);
    slapt_regex_t_free(size_c_regex);
    slapt_regex_t_free(size_u_regex);

    return list;
}

char *slapt_pkg_t_short_description(const slapt_pkg_t *pkg)
{
    char *short_description = NULL;
    size_t string_size = 0;

    if (strchr(pkg->description, '\n') != NULL) {
        string_size = strlen(pkg->description) - (strlen(pkg->name) + 2) - strlen(strchr(pkg->description, '\n'));
    }

    /* quit now if the description is going to be empty */
    if ((int)string_size < 1)
        return NULL;

    short_description = strndup(pkg->description + (strlen(pkg->name) + 2), string_size);

    return short_description;
}

slapt_vector_t *slapt_get_installed_pkgs(void)
{
    slapt_vector_t *list = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    DIR *pkg_log_dir;
    char *pkg_log_dirname = slapt_gen_package_log_dir_name();
    if ((pkg_log_dir = opendir(pkg_log_dirname)) == NULL) {
        if (errno)
            perror(pkg_log_dirname);

        free(pkg_log_dirname);
        return list;
    }

    slapt_regex_t *ip_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN);
    if (ip_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *compressed_size_reg = slapt_regex_t_init(SLAPT_PKG_LOG_SIZEC_PATTERN);
    if (compressed_size_reg == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *uncompressed_size_reg = slapt_regex_t_init(SLAPT_PKG_LOG_SIZEU_PATTERN);
    if (uncompressed_size_reg == NULL) {
        exit(EXIT_FAILURE);
    }

    struct dirent *file;
    while ((file = readdir(pkg_log_dir)) != NULL) {
        slapt_regex_t_execute(ip_regex, file->d_name);

        /* skip if it doesn't match our regex */
        if (ip_regex->reg_return != 0)
            continue;

        slapt_pkg_t *tmp_pkg = slapt_pkg_t_init();

        tmp_pkg->name = slapt_regex_t_extract_match(ip_regex, file->d_name, 1);
        tmp_pkg->version = slapt_regex_t_extract_match(ip_regex, file->d_name, 2);

        tmp_pkg->file_ext = slapt_malloc(sizeof *tmp_pkg->file_ext * 1);
        tmp_pkg->file_ext[0] = '\0';

        /* build the package filename including the package directory */
        const int pkg_f_name_len = strlen(pkg_log_dirname) + strlen(file->d_name) + 2;
        char pkg_f_name[pkg_f_name_len];
        const int written = snprintf(pkg_f_name, pkg_f_name_len, "%s/%s", pkg_log_dirname, file->d_name);
        if (written != (pkg_f_name_len - 1)) {
            fprintf(stderr, "slapt_get_installed_pkgs error for %s\n", file->d_name);
            exit(EXIT_FAILURE);
        }

        /* open the package log file so that we can mmap it and parse out the package attributes. */
        FILE *pkg_f = slapt_open_file(pkg_f_name, "r");
        if (pkg_f == NULL)
            exit(EXIT_FAILURE);

        /* used with mmap */
        struct stat stat_buf;
        if (stat(pkg_f_name, &stat_buf) == -1) {
            if (errno)
                perror(pkg_f_name);

            fprintf(stderr, "stat failed: %s\n", pkg_f_name);
            exit(EXIT_FAILURE);
        }

        /* don't mmap empty files */
        size_t pls = 1;
        if ((int)stat_buf.st_size < 1) {
            slapt_pkg_t_free(tmp_pkg);
            fclose(pkg_f);
            continue;
        } else {
            /* only mmap what we need */
            pls = (size_t)stat_buf.st_size;
            if (pls > SLAPT_MAX_MMAP_SIZE)
                pls = SLAPT_MAX_MMAP_SIZE;
        }

        char *pkg_data = (char *)mmap(0, pls, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(pkg_f), 0);
        if (pkg_data == (void *)-1) {
            if (errno)
                perror(pkg_f_name);

            fprintf(stderr, "mmap failed: %s\n", pkg_f_name);
            exit(EXIT_FAILURE);
        }

        fclose(pkg_f);

        /* add \0 for strlen to work */
        pkg_data[pls - 1] = '\0';

        /* pull out compressed size */
        slapt_regex_t_execute(compressed_size_reg, pkg_data);
        if (compressed_size_reg->reg_return == 0) {
            char *size_c = slapt_regex_t_extract_match(compressed_size_reg, pkg_data, 1);
            char *unit = slapt_regex_t_extract_match(compressed_size_reg, pkg_data, 2);
            double c = strtof(size_c, (char **)NULL);
            if (strcmp(unit, "M") == 0)
                c *= 1024;
            tmp_pkg->size_c = round(c);
            free(size_c);
            free(unit);
        }

        /* pull out uncompressed size */
        slapt_regex_t_execute(uncompressed_size_reg, pkg_data);
        if (uncompressed_size_reg->reg_return == 0) {
            char *size_u = slapt_regex_t_extract_match(uncompressed_size_reg, pkg_data, 1);
            char *unit = slapt_regex_t_extract_match(uncompressed_size_reg, pkg_data, 2);
            double u = strtof(size_u, (char **)NULL);
            if (strcmp(unit, "M") == 0)
                u *= 1024;
            tmp_pkg->size_u = round(u);
            free(size_u);
            free(unit);
        }

        /* Ignore the location for installed packages */
        if (strstr(pkg_data, "PACKAGE DESCRIPTION") != NULL) {
            char *desc_p = strstr(pkg_data, "PACKAGE DESCRIPTION");
            char *nl = strchr(desc_p, '\n');
            char *filelist_p = NULL;

            if (nl != NULL)
                desc_p = ++nl;

            filelist_p = strstr(desc_p, "FILE LIST");
            if (filelist_p != NULL) {
                char *tmp_desc = NULL;
                const size_t len = strlen(desc_p) - strlen(filelist_p) + 1;

                tmp_desc = realloc(tmp_pkg->description, sizeof *tmp_pkg->description * len);
                if (tmp_desc != NULL) {
                    tmp_pkg->description = tmp_desc;
                    slapt_strlcpy(tmp_pkg->description, desc_p, len);
                }

            } else {
                char *tmp_desc = NULL;
                const size_t len = strlen(desc_p) + 1;

                tmp_desc = realloc(tmp_pkg->description, sizeof *tmp_pkg->description * len);
                if (tmp_desc != NULL) {
                    tmp_pkg->description = tmp_desc;
                    slapt_strlcpy(tmp_pkg->description, desc_p, len);
                }
            }
        }

        /* munmap now that we are done */
        if (munmap(pkg_data, pls) == -1) {
            if (errno)
                perror(pkg_f_name);

            fprintf(stderr, "munmap failed: %s\n", pkg_f_name);
            exit(EXIT_FAILURE);
        }

        /* fillin details */
        if (tmp_pkg->location == NULL) {
            tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
            tmp_pkg->location[0] = '\0';
        }
        if (tmp_pkg->description == NULL) {
            tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->description);
            tmp_pkg->description[0] = '\0';
        }
        if (tmp_pkg->mirror == NULL) {
            tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->mirror);
            tmp_pkg->mirror[0] = '\0';
        }

        /* mark as installed */
        tmp_pkg->installed = true;

        slapt_vector_t_add(list, tmp_pkg);
        tmp_pkg = NULL;

    } /* end while */
    closedir(pkg_log_dir);
    free(pkg_log_dirname);
    slapt_regex_t_free(ip_regex);
    slapt_regex_t_free(compressed_size_reg);
    slapt_regex_t_free(uncompressed_size_reg);

    slapt_vector_t_sort(list, slapt_pkg_t_qsort_cmp);

    return list;
}

/*
static int by_name(const void *pkg, const void *name){
    slapt_pkg_t *p = *(slapt_pkg_t **)pkg;
    char *cname = *(char **)name;
    return strcmp(p->name, cname);
}
*/
static int by_details(const void *a, const void *b)
{
    return slapt_pkg_t_qsort_cmp(&a, &b);
}

/* lookup newest package from pkg_list */
slapt_pkg_t *slapt_get_newest_pkg(const slapt_vector_t *pkg_list, const char *pkg_name)
{
    slapt_pkg_t *found = NULL;
    slapt_vector_t *matches = slapt_vector_t_search(pkg_list, by_details, &(slapt_pkg_t){.name = (char *)pkg_name});
    if (!matches) {
        return found;
    }
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, matches) {
        if (strcmp(pkg->name, pkg_name) != 0)
            continue;
        if ((found == NULL) || (slapt_pkg_t_cmp(found, pkg) < 0))
            found = pkg;
    }
    slapt_vector_t_free(matches);
    return found;
}

slapt_pkg_t *slapt_get_exact_pkg(const slapt_vector_t *list, const char *name, const char *version)
{
    const int idx = slapt_vector_t_index_of(list, by_details, &(slapt_pkg_t){.name = (char *)name, .version = (char *)version});
    if (idx > -1) {
        return list->items[idx];
    }
    return NULL;
}

int slapt_install_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    /* build the file name */
    char *pkg_file_name = slapt_gen_pkg_file_name(global_config, pkg);

    /* build and execute our command */
    const int command_len = strlen(SLAPT_INSTALL_CMD) + strlen(pkg_file_name) + 1;
    char command[command_len];
    const int written = snprintf(command, command_len, "%s%s", SLAPT_INSTALL_CMD, pkg_file_name);
    if (written != (command_len - 1)) {
        fprintf(stderr, "slapt_install_pkg error for %s\n", pkg_file_name);
        exit(EXIT_FAILURE);
    }

    const int cmd_return = system(command);
    free(pkg_file_name);
    if (cmd_return != 0) {
        printf(gettext("Failed to execute command: [%s]\n"), command);
        return -1;
    }
    return 0;
}

int slapt_upgrade_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    /* build the file name */
    char *pkg_file_name = slapt_gen_pkg_file_name(global_config, pkg);

    /* build and execute our command */
    const int command_len = strlen(SLAPT_UPGRADE_CMD) + strlen(pkg_file_name) + 1;
    char command[command_len];
    const int written = snprintf(command, command_len, "%s%s", SLAPT_UPGRADE_CMD, pkg_file_name);
    if (written != (command_len - 1)) {
        fprintf(stderr, "slapt_upgrade_pkg error for %s\n", pkg_file_name);
        exit(EXIT_FAILURE);
    }

    const int cmd_return = system(command);
    free(pkg_file_name);
    if (cmd_return != 0) {
        printf(gettext("Failed to execute command: [%s]\n"), command);
        return -1;
    }
    return 0;
}

int slapt_remove_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    (void)global_config;

    /* build and execute our command */
    const int command_len = strlen(SLAPT_REMOVE_CMD) + strlen(pkg->name) + strlen(pkg->version) + 2;
    char command[command_len];
    const int written = snprintf(command, command_len, "%s%s-%s", SLAPT_REMOVE_CMD, pkg->name, pkg->version);
    if (written != (command_len - 1)) {
        fprintf(stderr, "slapt_remove_pkg error for %s\n", pkg->name);
        exit(EXIT_FAILURE);
    }

    const int cmd_return = system(command);
    if (cmd_return != 0) {
        printf(gettext("Failed to execute command: [%s]\n"), command);
        return -1;
    }
    return 0;
}

void slapt_pkg_t_free(slapt_pkg_t *pkg)
{
    if (pkg->required != NULL)
        free(pkg->required);

    if (pkg->conflicts != NULL)
        free(pkg->conflicts);

    if (pkg->suggests != NULL)
        free(pkg->suggests);

    if (pkg->name != NULL)
        free(pkg->name);

    if (pkg->file_ext != NULL)
        free(pkg->file_ext);

    if (pkg->version != NULL)
        free(pkg->version);

    if (pkg->mirror != NULL)
        free(pkg->mirror);

    if (pkg->location != NULL)
        free(pkg->location);

    if (pkg->description != NULL)
        free(pkg->description);

    slapt_vector_t_free(pkg->dependencies);

    free(pkg);
}

bool slapt_is_excluded(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    if (global_config->ignore_excludes)
        return false;

    /* maybe EXCLUDE= isn't defined in our rc? */
    if (!global_config->exclude_list->size)
        return false;

    slapt_vector_t_foreach (const char *, exclude, global_config->exclude_list) {
        slapt_regex_t *exclude_reg = NULL;

        /* return if its an exact match */
        if ((strncmp(exclude, pkg->name, strlen(pkg->name)) == 0))
            return true;

        /* this regex has to be init'd and free'd within the loop b/c the regex is pulled from the exclude list */
        if ((exclude_reg = slapt_regex_t_init(exclude)) == NULL) {
            continue;
        }

        slapt_regex_t_execute(exclude_reg, pkg->name);
        const int name_reg_ret = exclude_reg->reg_return;

        slapt_regex_t_execute(exclude_reg, pkg->version);
        const int version_reg_ret = exclude_reg->reg_return;

        slapt_regex_t_execute(exclude_reg, pkg->location);
        const int location_reg_ret = exclude_reg->reg_return;

        slapt_regex_t_free(exclude_reg);

        if (name_reg_ret == 0 || version_reg_ret == 0 || location_reg_ret == 0) {
            return true;
        }
    }

    return false;
}

void slapt_get_md5sums(slapt_vector_t *pkgs, FILE *checksum_file)
{
    slapt_regex_t *md5sum_regex = NULL;
    if ((md5sum_regex = slapt_regex_t_init(SLAPT_MD5SUM_REGEX)) == NULL) {
        exit(EXIT_FAILURE);
    }

    ssize_t getline_read;
    size_t getline_len = 0;
    char *getline_buffer = NULL;
    while ((getline_read = getline(&getline_buffer, &getline_len, checksum_file)) != EOF) {
        if (
            (strstr(getline_buffer, ".tgz") == NULL) &&
            (strstr(getline_buffer, ".tlz") == NULL) &&
            (strstr(getline_buffer, ".txz") == NULL) &&
            (strstr(getline_buffer, ".ikg") == NULL) &&
            (strstr(getline_buffer, ".tbz") == NULL))
            continue;
        if (strstr(getline_buffer, ".asc") != NULL)
            continue;

        slapt_regex_t_execute(md5sum_regex, getline_buffer);

        if (md5sum_regex->reg_return == 0) {
            char *sum, *location, *name, *version;

            /* md5 sum */
            sum = slapt_regex_t_extract_match(md5sum_regex, getline_buffer, 1);
            /* location/directory */
            location = slapt_regex_t_extract_match(md5sum_regex, getline_buffer, 2);
            /* pkg name */
            name = slapt_regex_t_extract_match(md5sum_regex, getline_buffer, 3);
            /* pkg version */
            version = slapt_regex_t_extract_match(md5sum_regex, getline_buffer, 4);

            /* see if we can match up name, version, and location */
            slapt_vector_t_foreach (slapt_pkg_t *, pkg, pkgs) {
                if ((strcmp(pkg->name, name) == 0) && (slapt_pkg_t_cmp_versions(pkg->version, version) == 0) && (strcmp(pkg->location, location) == 0)) {
                    slapt_strlcpy(pkg->md5, sum, SLAPT_MD5_STR_LEN + 1);
                    break;
                }
            }

            free(sum);
            free(name);
            free(version);
            free(location);
        }
    }

    if (getline_buffer)
        free(getline_buffer);

    slapt_regex_t_free(md5sum_regex);
    rewind(checksum_file);

    return;
}

static void slapt_pkg_t_free_version_parts(struct slapt_pkg_version_parts *parts)
{
    for (uint32_t i = 0; i < parts->count; i++) {
        free(parts->parts[i]);
    }
    free(parts->parts);
    free(parts);
}

int slapt_pkg_t_cmp(const slapt_pkg_t *a, const slapt_pkg_t *b)
{
    const int greater = 1, lesser = -1, equal = 0;

    /* if either of the two packages is installed, we look for the same version to bail out early if possible */
    if (a->installed || b->installed)
        if (strcasecmp(a->version, b->version) == 0)
            return equal;

    /* check the priorities */
    if (a->priority > b->priority)
        return greater;
    else if (a->priority < b->priority)
        return lesser;

    return slapt_pkg_t_cmp_versions(a->version, b->version);
}

int slapt_pkg_t_cmp_versions(const char *a, const char *b)
{
    const int greater = 1, lesser = -1, equal = 0;

    /* bail out early if possible */
    if (strcasecmp(a, b) == 0) {
        return equal;
    }

    struct slapt_pkg_version_parts *a_parts = break_down_pkg_version(a);
    struct slapt_pkg_version_parts *b_parts = break_down_pkg_version(b);

    uint32_t position = 0;
    while (position < a_parts->count && position < b_parts->count) {
        if (strcasecmp(a_parts->parts[position], b_parts->parts[position]) != 0) {
            /* if the integer value of the version part is the same and the # of version parts is the same (fixes 3.8.1p1-i486-1 to 3.8p1-i486-1) */
            if ((atoi(a_parts->parts[position]) == atoi(b_parts->parts[position])) && (a_parts->count == b_parts->count)) {
                if (strverscmp(a_parts->parts[position], b_parts->parts[position]) < 0) {
                    slapt_pkg_t_free_version_parts(a_parts);
                    slapt_pkg_t_free_version_parts(b_parts);
                    return lesser;
                }
                if (strverscmp(a_parts->parts[position], b_parts->parts[position]) > 0) {
                    slapt_pkg_t_free_version_parts(a_parts);
                    slapt_pkg_t_free_version_parts(b_parts);
                    return greater;
                }
            }

            if (atoi(a_parts->parts[position]) < atoi(b_parts->parts[position])) {
                slapt_pkg_t_free_version_parts(a_parts);
                slapt_pkg_t_free_version_parts(b_parts);
                return lesser;
            }

            if (atoi(a_parts->parts[position]) > atoi(b_parts->parts[position])) {
                slapt_pkg_t_free_version_parts(a_parts);
                slapt_pkg_t_free_version_parts(b_parts);
                return greater;
            }
        }
        ++position;
    }

    /*
    if we got this far, we know that some or all of the version
    parts are equal in both packages.  If pkg-a has 3 version parts
    and pkg-b has 2, then we assume pkg-a to be greater.
    */
    if (a_parts->count != b_parts->count) {
        if (a_parts->count > b_parts->count) {
            slapt_pkg_t_free_version_parts(a_parts);
            slapt_pkg_t_free_version_parts(b_parts);
            return greater;
        } else {
            slapt_pkg_t_free_version_parts(a_parts);
            slapt_pkg_t_free_version_parts(b_parts);
            return lesser;
        }
    }

    slapt_pkg_t_free_version_parts(a_parts);
    slapt_pkg_t_free_version_parts(b_parts);

    /*
    Now we check to see that the version follows the standard slackware
    convention.  If it does, we will compare the build portions.
    */
    /* make sure the packages have at least two separators */
    if ((index(a, '-') != rindex(a, '-')) && (index(b, '-') != rindex(b, '-'))) {
        char *a_build, *b_build;

        /* pointer to build portions */
        a_build = rindex(a, '-');
        b_build = rindex(b, '-');

        if (a_build != NULL && b_build != NULL) {
            /* they are equal if the integer values are equal */
            /* for instance, "1rob" and "1" will be equal */
            if (atoi(a_build) == atoi(b_build)) {
/* prefer our architecture in x86/x86_64 multilib settings */
#if defined(__x86_64__)
                if ((strcmp(a_build, b_build) == 0) && (strcmp(a, b) != 0) && (strstr(a, "-x86_64") != NULL && (strstr(b, "-x86_64-") == NULL)))
                    return greater;
                if ((strcmp(a_build, b_build) == 0) && (strcmp(a, b) != 0) && (strstr(a, "-x86_64") == NULL && (strstr(b, "-x86_64-") != NULL)))
                    return lesser;
#elif defined(__i386__)
                if ((strcmp(a_build, b_build) == 0) && (strcmp(a, b) != 0) && (strstr(a, "-x86_64") == NULL && (strstr(b, "-x86_64-") != NULL)))
                    return greater;
                if ((strcmp(a_build, b_build) == 0) && (strcmp(a, b) != 0) && (strstr(a, "-x86_64") != NULL && (strstr(b, "-x86_64-") == NULL)))
                    return lesser;
#endif
                return equal;
            }

            if (atoi(a_build) < atoi(b_build))
                return greater;

            if (atoi(a_build) > atoi(b_build))
                return lesser;
        }
    }

    /*
    If both have the same # of version parts, non-standard version convention,
    then we fall back on strverscmp.
    */
    if (strchr(a, '-') == NULL && strchr(b, '-') == NULL)
        return strverscmp(a, b);

    return equal;
}

static struct slapt_pkg_version_parts *break_down_pkg_version(const char *version)
{
    struct slapt_pkg_version_parts *pvp = slapt_malloc(sizeof *pvp);
    pvp->parts = slapt_malloc(sizeof *pvp->parts);
    pvp->count = 0;

    int sv_size = 0;
    char *pointer, *short_version;
    /* generate a short version, leave out arch and release */
    if ((pointer = strchr(version, '-')) == NULL) {
        sv_size = strlen(version) + 1;
        short_version = slapt_malloc(sizeof *short_version * sv_size);
        memcpy(short_version, version, sv_size);
        short_version[sv_size - 1] = '\0';
    } else {
        sv_size = (strlen(version) - strlen(pointer) + 1);
        short_version = slapt_malloc(sizeof *short_version * sv_size);
        memcpy(short_version, version, sv_size);
        short_version[sv_size - 1] = '\0';
        pointer = NULL;
    }

    int pos = 0;
    while (pos < (sv_size - 1)) {
        char **tmp;

        tmp = realloc(pvp->parts, sizeof *pvp->parts * (pvp->count + 1));
        if (tmp == NULL) {
            fprintf(stderr, gettext("Failed to realloc %s\n"), "pvp->parts");
            exit(EXIT_FAILURE);
        }
        pvp->parts = tmp;

        /* check for . as a seperator */
        if ((pointer = strchr(short_version + pos, '.')) != NULL) {
            const int b_count = (strlen(short_version + pos) - strlen(pointer) + 1);
            pvp->parts[pvp->count] = slapt_malloc(sizeof *pvp->parts[pvp->count] * b_count);

            memcpy(pvp->parts[pvp->count], short_version + pos, b_count - 1);
            pvp->parts[pvp->count][b_count - 1] = '\0';
            ++pvp->count;
            pointer = NULL;
            pos += b_count;
            /* check for _ as a seperator */
        } else if ((pointer = strchr(short_version + pos, '_')) != NULL) {
            const int b_count = (strlen(short_version + pos) - strlen(pointer) + 1);
            pvp->parts[pvp->count] = slapt_malloc(sizeof *pvp->parts[pvp->count] * b_count);

            memcpy(pvp->parts[pvp->count], short_version + pos, b_count - 1);
            pvp->parts[pvp->count][b_count - 1] = '\0';
            ++pvp->count;
            pointer = NULL;
            pos += b_count;
            /* must be the end of the string */
        } else {
            const int b_count = (strlen(short_version + pos) + 1);
            pvp->parts[pvp->count] = slapt_malloc(sizeof *pvp->parts[pvp->count] * b_count);

            memcpy(pvp->parts[pvp->count], short_version + pos, b_count - 1);
            pvp->parts[pvp->count][b_count - 1] = '\0';
            ++pvp->count;
            pos += b_count;
        }
    }

    free(short_version);
    return pvp;
}

void slapt_write_pkg_data(const char *source_url, FILE *d_file, const slapt_vector_t *pkgs)
{
    slapt_vector_t_foreach (const slapt_pkg_t *, pkg, pkgs) {
        fprintf(d_file, "PACKAGE NAME:  %s-%s%s\n", pkg->name, pkg->version, pkg->file_ext);
        if (pkg->mirror != NULL && strlen(pkg->mirror) > 0) {
            fprintf(d_file, "PACKAGE MIRROR:  %s\n", pkg->mirror);
        } else {
            fprintf(d_file, "PACKAGE MIRROR:  %s\n", source_url);
        }
        fprintf(d_file, "PACKAGE PRIORITY:  %d\n", pkg->priority);
        fprintf(d_file, "PACKAGE LOCATION:  %s\n", pkg->location);
        fprintf(d_file, "PACKAGE SIZE (compressed):  %d K\n", pkg->size_c);
        fprintf(d_file, "PACKAGE SIZE (uncompressed):  %d K\n", pkg->size_u);
        fprintf(d_file, "PACKAGE REQUIRED:  %s\n", pkg->required);
        fprintf(d_file, "PACKAGE CONFLICTS:  %s\n", pkg->conflicts);
        fprintf(d_file, "PACKAGE SUGGESTS:  %s\n", pkg->suggests);
        fprintf(d_file, "PACKAGE MD5SUM:  %s\n", pkg->md5);
        fprintf(d_file, "PACKAGE DESCRIPTION:\n");
        /* do we have to make up an empty description? */
        if (strlen(pkg->description) < strlen(pkg->name)) {
            fprintf(d_file, "%s: no description\n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n", pkg->name);
            fprintf(d_file, "%s: \n\n", pkg->name);
        } else {
            fprintf(d_file, "%s\n", pkg->description);
        }
    }
}

slapt_vector_t *slapt_search_pkg_list(const slapt_vector_t *list, const char *pattern)
{
    slapt_vector_t *matches = slapt_vector_t_init(NULL);

    slapt_regex_t *search_regex = slapt_regex_t_init(pattern);
    if (search_regex == NULL) {
        return matches;
    }

    slapt_vector_t_foreach (slapt_pkg_t *, pkg, list) {
        if (strcmp(pkg->name, pattern) == 0) {
            slapt_vector_t_add(matches, pkg);
            continue;
        }

        slapt_regex_t_execute(search_regex, pkg->name);
        const int name_r = search_regex->reg_return;

        slapt_regex_t_execute(search_regex, pkg->version);
        const int version_r = search_regex->reg_return;

        int desc_r = -1;
        if (pkg->description != NULL) {
            slapt_regex_t_execute(search_regex, pkg->description);
            desc_r = search_regex->reg_return;
        }

        int loc_r = -1;
        if (pkg->location != NULL) {
            slapt_regex_t_execute(search_regex, pkg->location);
            loc_r = search_regex->reg_return;
        }

        /* search pkg name, pkg description, pkg location */
        if (name_r == 0 || version_r == 0 || desc_r == 0 || loc_r == 0) {
            slapt_vector_t_add(matches, pkg);
        }
    }
    slapt_regex_t_free(search_regex);

    return matches;
}

static slapt_pkg_t *resolve_dep(const slapt_dependency_t *dep_declaration, const slapt_vector_t *installed, const slapt_vector_t *available)
{
    slapt_pkg_t *found = NULL;
    /* try installed packages first */
    if (installed != NULL) {
        slapt_pkg_t *ipkg = slapt_get_newest_pkg(installed, dep_declaration->name);
        if (ipkg != NULL) {
            if (dep_declaration->op == DEP_OP_ANY) {
                return ipkg;
            }
            const int cmp = slapt_pkg_t_cmp_versions(ipkg->version, dep_declaration->version);
            if ((cmp == 0) && (dep_declaration->op == DEP_OP_EQ || dep_declaration->op == DEP_OP_GTE || dep_declaration->op == DEP_OP_LTE)) {
                return ipkg;
            }
            if ((cmp > 0) && (dep_declaration->op == DEP_OP_GT)) {
                return ipkg;
            }
            if ((cmp < 0) && (dep_declaration->op == DEP_OP_LT)) {
                return ipkg;
            }
        }
    }

    /* now look externally */
    if (available != NULL) {
        slapt_pkg_t *apkg = slapt_get_newest_pkg(available, dep_declaration->name);
        if (apkg != NULL) {
            if (dep_declaration->op == DEP_OP_ANY) {
                return apkg;
            }
            const int cmp = slapt_pkg_t_cmp_versions(apkg->version, dep_declaration->version);
            if ((cmp == 0) && (dep_declaration->op == DEP_OP_EQ || dep_declaration->op == DEP_OP_GTE || dep_declaration->op == DEP_OP_LTE)) {
                return apkg;
            }
            if ((cmp > 0) && (dep_declaration->op == DEP_OP_GT)) {
                return apkg;
            }
            if ((cmp < 0) && (dep_declaration->op == DEP_OP_LT)) {
                return apkg;
            }
        }

        slapt_vector_t *all = slapt_vector_t_search(available, by_details, &(slapt_pkg_t){.name = (char *)dep_declaration->name});
        slapt_vector_t_foreach(slapt_pkg_t *, candidate, all) {
            if (dep_declaration->op == DEP_OP_ANY) {
                found = candidate;
                break;
            }
            const int cmp = slapt_pkg_t_cmp_versions(candidate->version, dep_declaration->version);
            if ((cmp == 0) && (dep_declaration->op == DEP_OP_EQ || dep_declaration->op == DEP_OP_GTE || dep_declaration->op == DEP_OP_LTE)) {
                found = candidate;
                break;
            }
            if ((cmp > 0) && (dep_declaration->op == DEP_OP_GT)) {
                found = candidate;
                break;
            }
            if ((cmp < 0) && (dep_declaration->op == DEP_OP_LT)) {
                found = candidate;
                break;
            }
        }
        slapt_vector_t_free(all);
    }
    return found;
}

/* lookup dependencies for pkg */
int slapt_get_pkg_dependencies(const slapt_config_t *global_config,
                               const slapt_vector_t *avail_pkgs,
                               const slapt_vector_t *installed_pkgs,
                               const slapt_pkg_t *pkg,
                               slapt_vector_t *deps,
                               slapt_vector_t *conflict_err,
                               slapt_vector_t *missing_err)
{
    if (deps == NULL)
        deps = slapt_vector_t_init(NULL);

    if (conflict_err == NULL)
        conflict_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);

    if (missing_err == NULL)
        missing_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);

    if ((slapt_get_newest_pkg(deps, pkg->name) != NULL)) {
        return 0;
    }

    /* don't go any further if the required member is empty or disable_dep_check is set */
    if (global_config->disable_dep_check || pkg->dependencies->size == 0)
        return 0;

    slapt_vector_t_foreach(const slapt_dependency_t *, dep_declaration, pkg->dependencies) {
        if (dep_declaration->op == DEP_OP_OR) {
            // look through alternatives until we either find an installed one or an available one */
            slapt_pkg_t *ipkg = NULL;
            slapt_vector_t_foreach(const slapt_dependency_t *, installed_alt_dep, dep_declaration->alternatives) {
                ipkg = resolve_dep(installed_alt_dep, installed_pkgs, NULL);
                if (ipkg != NULL) {
                    break;
                }
            }
            if (ipkg != NULL) {
                continue;
            }

            slapt_pkg_t *apkg = NULL;
            slapt_vector_t_foreach(const slapt_dependency_t *, alt_dep, dep_declaration->alternatives) {
                apkg = resolve_dep(alt_dep, NULL, avail_pkgs);
                if (apkg != NULL) {
                    /* if this pkg is excluded */
                    if ((slapt_is_excluded(global_config, apkg)) && !global_config->ignore_dep) {
                        slapt_pkg_err_t *err = slapt_pkg_err_t_init(strdup(pkg->name), strdup(apkg->name));
                        slapt_vector_t_add(conflict_err, err);
                        return -1;
                    }
                    if (!apkg->installed) {
                        slapt_vector_t_add(deps, apkg);
                        const int rv = slapt_get_pkg_dependencies(global_config, avail_pkgs, installed_pkgs, apkg, deps, conflict_err, missing_err);
                        if (rv != 0)
                            return rv;
                        slapt_vector_t_remove(deps, apkg);
                        slapt_vector_t_add(deps, apkg);
                    }
                    break;
                }
            }
            if (apkg != NULL) {
                continue;
            }

            slapt_vector_t_foreach(const slapt_dependency_t *, no_such_alt_dep, dep_declaration->alternatives) {
                slapt_pkg_err_t *err = slapt_pkg_err_t_init(strdup(pkg->name), strdup(no_such_alt_dep->name));
                slapt_vector_t_add(missing_err, err);
            }
            return -1;

        } else {
            slapt_pkg_t *dep_pkg = resolve_dep(dep_declaration, installed_pkgs, avail_pkgs);
            if (dep_pkg != NULL) {
                /* if this pkg is excluded */
                if ((slapt_is_excluded(global_config, dep_pkg)) && !global_config->ignore_dep) {
                    slapt_pkg_err_t *err = slapt_pkg_err_t_init(strdup(pkg->name), strdup(dep_pkg->name));
                    slapt_vector_t_add(conflict_err, err);
                    return -1;
                }
                if (!dep_pkg->installed) {
                    slapt_vector_t_add(deps, dep_pkg);
                    const int rv = slapt_get_pkg_dependencies(global_config, avail_pkgs, installed_pkgs, dep_pkg, deps, conflict_err, missing_err);
                    if (rv != 0)
                        return rv;
                    slapt_vector_t_remove(deps, dep_pkg);
                    slapt_vector_t_add(deps, dep_pkg);
                }
                continue;
            }

            slapt_pkg_err_t *err = slapt_pkg_err_t_init(strdup(pkg->name), strdup(dep_declaration->name));
            slapt_vector_t_add(missing_err, err);
            return -1;;
        }
    }
    return 0;
}

/* lookup conflicts for package */
slapt_vector_t *slapt_get_pkg_conflicts(const slapt_vector_t *avail_pkgs, const slapt_vector_t *installed_pkgs, const slapt_pkg_t *pkg)
{
    slapt_vector_t *conflicts = slapt_vector_t_init(NULL);
    if (!pkg->conflicts) {
        return conflicts;
    }

    slapt_vector_t *conflict_parts = slapt_parse_delimited_list(pkg->conflicts, ',');
    if (conflict_parts->size == 0) {
        slapt_vector_t_free(conflict_parts);
        return conflicts;
    }

    slapt_vector_t_foreach(const char *, conflict_decl, conflict_parts) {
        slapt_dependency_t *conflict_dep = slapt_dependency_t_parse_required(conflict_decl);
        if (conflict_dep->op == DEP_OP_OR) {
            slapt_vector_t_foreach(const slapt_dependency_t *, conflict_or, conflict_dep->alternatives) {
                slapt_pkg_t *conflicted_pkg = resolve_dep(conflict_or, installed_pkgs, avail_pkgs);
                if (conflicted_pkg != NULL) {
                    slapt_vector_t_add(conflicts, conflicted_pkg);
                }
            }
        } else {
            slapt_pkg_t *conflicted_pkg = resolve_dep(conflict_dep, installed_pkgs, avail_pkgs);
            if (conflicted_pkg != NULL) {
                slapt_vector_t_add(conflicts, conflicted_pkg);
            }
        }
        slapt_dependency_t_free(conflict_dep);
    }

    return conflicts;
}

slapt_vector_t *slapt_is_required_by(const slapt_config_t *global_config,
                                     const slapt_vector_t *avail,
                                     const slapt_vector_t *installed_pkgs,
                                     slapt_vector_t *pkgs_to_install,
                                     slapt_vector_t *pkgs_to_remove,
                                     const slapt_pkg_t *pkg)
{
    slapt_vector_t *required_by_list = slapt_vector_t_init(NULL);

    /* don't go any further if disable_dep_check is set */
    if (global_config->disable_dep_check)
        return required_by_list;

    required_by(avail, installed_pkgs, pkgs_to_install, pkgs_to_remove, pkg, required_by_list);

    return required_by_list;
}

static void required_by(const slapt_vector_t *avail,
                        const slapt_vector_t *installed_pkgs,
                        slapt_vector_t *pkgs_to_install,
                        slapt_vector_t *pkgs_to_remove,
                        const slapt_pkg_t *pkg,
                        slapt_vector_t *required_by_list)
{
    /* Note this is super naive because we have no dependency information
       serialized with installed packages... so we have to traverse the current
       available packages that declare they depend on the given package... with
       a slightly fuzzy lookup that ignores version information.
    */
    slapt_vector_t_foreach (slapt_pkg_t *, avail_pkg, avail) {
        // avoid work
        if (avail_pkg->dependencies->size == 0) {
            continue;
        }
        if (strstr(avail_pkg->required, pkg->name) == NULL) {
            continue;
        }

        if (strcmp(avail_pkg->name, pkg->name) == 0) {
            continue;
        }

        // skip if already processed
        if (slapt_get_newest_pkg(required_by_list, avail_pkg->name) != NULL) {
            continue;
        }
        // only care about the newest version
        slapt_pkg_t *newest_avail_pkg = slapt_get_newest_pkg(avail, avail_pkg->name);
        if (newest_avail_pkg != NULL && (slapt_pkg_t_cmp_versions(avail_pkg->version, newest_avail_pkg->version) < 0)) {
            continue;
        }

        slapt_vector_t_foreach(slapt_dependency_t *, dep, avail_pkg->dependencies) {
            slapt_dependency_t *found = NULL;
            if (dep->op == DEP_OP_OR) {
                /* alternatives have special handling... we need to check that all alternatives
                   are not scheduled to remove and either installed or scheduled to install */
                bool has_alternative = false;
                slapt_vector_t_foreach(slapt_dependency_t *, alt_dep, dep->alternatives) {
                    if (slapt_get_newest_pkg(pkgs_to_remove, alt_dep->name) == NULL) {
                        slapt_pkg_t *installed_dep = resolve_dep(alt_dep, installed_pkgs, NULL);
                        if (installed_dep != NULL) {
                            has_alternative = true;
                        }
                        else if (slapt_get_newest_pkg(pkgs_to_install, alt_dep->name) != NULL) {
                            has_alternative = true;
                        }
                    }

                    if (strcmp(alt_dep->name, pkg->name) == 0) {
                        found = alt_dep;
                    }
                }
                if (has_alternative && found) {
                    found = NULL;
                }
            } else {
                if (strcmp(dep->name, pkg->name) == 0) {
                    found = dep;
                }
            }
            if (found == NULL) {
                continue;
            }

            // at least check it is reasonable
            slapt_pkg_t *dep_pkg = resolve_dep(found, installed_pkgs, avail);
            if (dep_pkg == NULL) {
                continue;
            }

            slapt_vector_t_add(required_by_list, avail_pkg);
            required_by(avail, installed_pkgs, pkgs_to_install, pkgs_to_remove, avail_pkg, required_by_list);
            break;
        }
    }

}

slapt_pkg_t *slapt_get_pkg_by_details(const slapt_vector_t *list, const char *name, const char *version, const char *location)
{
    const int idx = slapt_vector_t_index_of(list, by_details, &(slapt_pkg_t){.name = (char *)name, .version = (char *)version, .location = (char *)location});
    if (idx > -1) {
        return list->items[idx];
    }
    return NULL;
}

/* update package data from mirror url */
int slapt_update_pkg_cache(const slapt_config_t *global_config)
{
    bool source_dl_failed = false;
    slapt_vector_t *new_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    /* go through each package source and download the meta data */
    slapt_vector_t_foreach (const slapt_source_t *, source, global_config->sources) {
        bool compressed = false;
        const char *source_url = source->url;
        slapt_priority_t source_priority = source->priority;

        if (source->disabled)
            continue;

        /* download our SLAPT_PKG_LIST */
        printf(gettext("Retrieving package data [%s]..."), source_url);

        slapt_vector_t *available_pkgs = slapt_get_pkg_source_packages(global_config, source_url, &compressed);
        if (available_pkgs == NULL) {
            source_dl_failed = true;
            continue;
        }

        /* download SLAPT_PATCHES_LIST */
        printf(gettext("Retrieving patch list [%s]..."), source_url);

        slapt_vector_t *patch_pkgs = slapt_get_pkg_source_patches(global_config, source_url, &compressed);

        /* download checksum file */
        printf(gettext("Retrieving checksum list [%s]..."), source_url);
        FILE *tmp_checksum_f = slapt_get_pkg_source_checksums(global_config, source_url, &compressed);

#ifdef SLAPT_HAS_GPGME
        printf(gettext("Retrieving checksum signature [%s]..."), source_url);
        FILE *tmp_signature_f = slapt_get_pkg_source_checksums_signature(global_config, source_url, &compressed);
        FILE *tmp_checksum_to_verify_f = NULL;

        /* if we downloaded the compressed checksums, open it raw (w/o gunzipping) */
        if (compressed) {
            char *filename = slapt_gen_filename_from_url(source_url, SLAPT_CHECKSUM_FILE_GZ);
            tmp_checksum_to_verify_f = slapt_open_file(filename, "r");
            free(filename);
        } else {
            tmp_checksum_to_verify_f = tmp_checksum_f;
        }

        if (tmp_signature_f != NULL && tmp_checksum_to_verify_f != NULL) {
            slapt_code_t verified = SLAPT_CHECKSUMS_NOT_VERIFIED;
            printf(gettext("Verifying checksum signature [%s]..."), source_url);
            verified = slapt_gpg_verify_checksums(tmp_checksum_to_verify_f, tmp_signature_f);
            if (verified == SLAPT_CHECKSUMS_VERIFIED) {
                printf("%s\n", gettext("Verified"));
            } else if (verified == SLAPT_CHECKSUMS_MISSING_KEY) {
                printf("%s\n", gettext("No key for verification"));
            } else if ((global_config->gpgme_allow_unauth) && (slapt_pkg_sign_is_unauthenticated(verified))) {
                printf("%s%s\n", slapt_strerror(verified), gettext(", but accepted as an exception"));
            } else {
                printf("%s\n", gettext(slapt_strerror(verified)));
                source_dl_failed = true;
                fclose(tmp_checksum_f);
                tmp_checksum_f = NULL;
            }
        }

        if (tmp_signature_f)
            fclose(tmp_signature_f);

        /* if we opened the raw gzipped checksums, close it here */
        if (compressed) {
            fclose(tmp_checksum_to_verify_f);
        } else {
            if (tmp_checksum_f)
                rewind(tmp_checksum_f);
        }
#endif

        if (!source_dl_failed) {
            printf(gettext("Retrieving ChangeLog.txt [%s]..."), source_url);
            slapt_get_pkg_source_changelog(global_config, source_url, &compressed);
        }

        if (tmp_checksum_f != NULL) {
            /* now map md5 checksums to packages */
            printf(gettext("Reading Package Lists..."));

            slapt_get_md5sums(available_pkgs, tmp_checksum_f);

            slapt_vector_t_foreach (slapt_pkg_t *, p, available_pkgs) {
                int mirror_len = -1;

                /* honor the mirror if it was set in the PACKAGES.TXT */
                if (p->mirror == NULL || (mirror_len = strlen(p->mirror)) == 0) {
                    if (mirror_len == 0)
                        free(p->mirror);

                    p->mirror = strdup(source_url);
                }

                /* set the priority of the package based on the source */
                p->priority = source_priority;

                slapt_vector_t_add(new_pkgs, p);
            }
            available_pkgs->free_function = NULL;

            if (patch_pkgs) {
                slapt_get_md5sums(patch_pkgs, tmp_checksum_f);
                slapt_vector_t_foreach (slapt_pkg_t *, patch_pkg, patch_pkgs) {
                    int mirror_len = -1;

                    /* honor the mirror if it was set in the PACKAGES.TXT */
                    if (patch_pkg->mirror == NULL || (mirror_len = strlen(patch_pkg->mirror)) == 0) {
                        if (mirror_len == 0)
                            free(patch_pkg->mirror);

                        patch_pkg->mirror = strdup(source_url);
                    }

                    /* set the priority of the package based on the source, plus 1 for the patch priority */
                    if (global_config->use_priority)
                        patch_pkg->priority = source_priority + 1;
                    else
                        patch_pkg->priority = source_priority;

                    slapt_vector_t_add(new_pkgs, patch_pkg);
                }
                patch_pkgs->free_function = NULL;
                slapt_vector_t_free(patch_pkgs);
            }

            printf(gettext("Done\n"));

            fclose(tmp_checksum_f);
        } else {
            source_dl_failed = true;
        }

        if (available_pkgs) {
            slapt_vector_t_free(available_pkgs);
        }

    } /* end for loop */

    /* if all our downloads where a success, write to SLAPT_PKG_LIST_L */
    if (!source_dl_failed) {
        slapt_vector_t_sort(new_pkgs, slapt_pkg_t_qsort_cmp);

        FILE *pkg_list_fh = slapt_open_file(SLAPT_PKG_LIST_L, "w+");
        if (pkg_list_fh == NULL) {
            exit(EXIT_FAILURE);
        }
        slapt_write_pkg_data(NULL, pkg_list_fh, new_pkgs);
        fclose(pkg_list_fh);

    } else {
        printf(gettext("Sources failed to download, correct sources and rerun --update\n"));
    }

    slapt_vector_t_free(new_pkgs);

    return source_dl_failed;
}

#ifdef SLAPT_HAS_GPGME
bool slapt_pkg_sign_is_unauthenticated(slapt_code_t code)
{
    switch (code) {
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_REVOKED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_EXPIRED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SIG_EXPIRED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_MISSING:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_TOO_OLD:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_BAD_POLICY:
        return true;
    default:
        break;
    }

    return false;
}
#endif

slapt_pkg_t *slapt_pkg_t_init(void)
{
    slapt_pkg_t *pkg;

    pkg = slapt_malloc(sizeof *pkg);

    pkg->size_c = 0;
    pkg->size_u = 0;

    pkg->name = NULL;
    pkg->version = NULL;
    pkg->mirror = NULL;
    pkg->location = NULL;
    pkg->file_ext = NULL;

    pkg->description = slapt_malloc(sizeof *pkg->description);
    pkg->description[0] = '\0';

    pkg->required = slapt_malloc(sizeof *pkg->required);
    pkg->required[0] = '\0';

    pkg->conflicts = slapt_malloc(sizeof *pkg->conflicts);
    pkg->conflicts[0] = '\0';

    pkg->suggests = slapt_malloc(sizeof *pkg->suggests);
    pkg->suggests[0] = '\0';

    pkg->md5[0] = '\0';

    pkg->priority = SLAPT_PRIORITY_DEFAULT;

    pkg->installed = false;

    pkg->dependencies = slapt_vector_t_init((slapt_vector_t_free_function)slapt_dependency_t_free);

    return pkg;
}

/* generate the package file name */
char *slapt_gen_pkg_file_name(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    /* build the file name */
    const size_t file_name_len = strlen(global_config->working_dir) + strlen(pkg->location) + strlen(pkg->name) + strlen(pkg->version) + strlen(pkg->file_ext) + 4;
    char *file_name = slapt_calloc(file_name_len, sizeof *file_name);
    if (snprintf(file_name, file_name_len, "%s/%s/%s-%s%s", global_config->working_dir, pkg->location, pkg->name, pkg->version, pkg->file_ext) != (int)(file_name_len - 1)) {
        fprintf(stderr, "slapt_gen_pkg_file_name error for %s\n", pkg->name);
        exit(EXIT_FAILURE);
    }

    return file_name;
}

/* generate the download url for a package */
char *slapt_pkg_t_url(const slapt_pkg_t *pkg)
{
    /* build the file name */
    char *file_name = slapt_pkg_t_string(pkg);

    /* build the url */
    const size_t url_len = strlen(pkg->mirror) + strlen(pkg->location) + strlen(file_name) + 2;
    char *url = slapt_calloc(url_len, sizeof *url);
    if (snprintf(url, url_len, "%s%s/%s", pkg->mirror, pkg->location, file_name) != (int)(url_len - 1)) {
        fprintf(stderr, "slapt_pkg_t_url error for %s\n", pkg->name);
        exit(EXIT_FAILURE);
    }

    free(file_name);
    return url;
}

/* find out the pkg file size (post download) */
size_t slapt_get_pkg_file_size(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    /* build the file name */
    char *file_name = slapt_gen_pkg_file_name(global_config, pkg);

    struct stat file_stat;
    size_t file_size = 0;
    if (stat(file_name, &file_stat) == 0) {
        file_size = file_stat.st_size;
    }
    free(file_name);

    return file_size;
}

/* package is already downloaded and cached, md5sum if applicable is ok */
slapt_code_t slapt_verify_downloaded_pkg(const slapt_config_t *global_config, const slapt_pkg_t *pkg)
{
    /*
    size_t file_size = 0;
    check the file size first so we don't run an md5 checksum
    on an incomplete file

    XXX 2009-04-27 XXX
    This has become increasingly less reliable, especially with
    recent changes in how the size is calculated when generating
    the PACKAGES.TXT... we do not really lose a lot by not checking
    since we are validating the checksum anyway.

    file_size = slapt_get_pkg_file_size(global_config,pkg);
    if ((uint32_t)(file_size/1024) != pkg->size_c) {
        return SLAPT_DOWNLOAD_INCOMPLETE;
    }
    */

    /* if not checking the md5 checksum and the sizes match, assume its good */
    if (global_config->no_md5_check)
        return SLAPT_OK;

    /* check to see that we actually have an md5 checksum */
    if (strcmp(pkg->md5, "") == 0) {
        return SLAPT_MD5_CHECKSUM_MISSING;
    }

    /* build the file name */
    char *file_name = slapt_gen_pkg_file_name(global_config, pkg);

    /* return if we can't open the file */
    FILE *fh_test = fopen(file_name, "r");
    free(file_name);
    if (fh_test == NULL) {
        return SLAPT_DOWNLOAD_INCOMPLETE;
    }

    /* generate the md5 checksum */
    char md5sum_f[SLAPT_MD5_STR_LEN + 1];
    slapt_gen_md5_sum_of_file(fh_test, md5sum_f);
    fclose(fh_test);

    /* check to see if the md5sum is correct */
    if (strcmp(md5sum_f, pkg->md5) == 0)
        return SLAPT_OK;

    return SLAPT_MD5_CHECKSUM_MISMATCH;
}

char *slapt_gen_filename_from_url(const char *url, const char *file)
{
    const int filename_len = strlen(url) + strlen(file) + 2;
    char filename[filename_len];
    const int written = snprintf(filename, filename_len, ".%s%s", url, file);
    if (written != (filename_len - 1)) {
        fprintf(stderr, "slapt_gen_filename_from_url error for %s\n", url);
        exit(EXIT_FAILURE);
    }
    char *cleaned = slapt_str_replace_chr(filename, '/', '#');
    return cleaned;
}

void slapt_purge_old_cached_pkgs(const slapt_config_t *global_config, const char *dir_name, slapt_vector_t *avail_pkgs)
{
    bool local_pkg_list = false;
    slapt_regex_t *cached_pkgs_regex = slapt_regex_t_init(SLAPT_PKG_PARSE_REGEX);
    if (cached_pkgs_regex == NULL) {
        exit(EXIT_FAILURE);
    }


    if (avail_pkgs == NULL) {
        avail_pkgs = slapt_get_available_pkgs();
        local_pkg_list = true;
    }

    if (dir_name == NULL) {
        dir_name = (char *)global_config->working_dir;
    }

    DIR *dir = opendir(dir_name);
    if (dir == NULL) {
        if (errno)
            perror(dir_name);

        fprintf(stderr, gettext("Failed to opendir %s\n"), dir_name);
        return;
    }

    if (chdir(dir_name) == -1) {
        if (errno)
            perror(dir_name);

        fprintf(stderr, gettext("Failed to chdir: %s\n"), dir_name);
        closedir(dir);
        return;
    }

    struct dirent *file;
    while ((file = readdir(dir))) {
        /* make sure we don't have . or .. */
        if ((strcmp(file->d_name, "..")) == 0 || (strcmp(file->d_name, ".") == 0))
            continue;

        /* setup file_stat struct */
        struct stat file_stat;
        if ((lstat(file->d_name, &file_stat)) == -1)
            continue;

        /* if its a directory, recurse */
        if (S_ISDIR(file_stat.st_mode)) {
            slapt_purge_old_cached_pkgs(global_config, file->d_name, avail_pkgs);
            if ((chdir("..")) == -1) {
                fprintf(stderr, gettext("Failed to chdir: %s\n"), dir_name);
                return;
            }
            continue;
        }

        /* if its a package */
        if (strstr(file->d_name, ".t") != NULL) {
            slapt_regex_t_execute(cached_pkgs_regex, file->d_name);

            /* if our regex matches */
            if (cached_pkgs_regex->reg_return == 0) {
                char *tmp_pkg_name, *tmp_pkg_version;
                slapt_pkg_t *tmp_pkg;

                tmp_pkg_name = slapt_regex_t_extract_match(cached_pkgs_regex, file->d_name, 1);
                tmp_pkg_version = slapt_regex_t_extract_match(cached_pkgs_regex, file->d_name, 2);
                tmp_pkg = slapt_get_exact_pkg(avail_pkgs, tmp_pkg_name, tmp_pkg_version);

                free(tmp_pkg_name);
                free(tmp_pkg_version);

                if (tmp_pkg == NULL) {
                    if (global_config->no_prompt) {
                        if (unlink(file->d_name) != 0) {
                            perror(file->d_name);
                        }
                    } else {
                        if (slapt_ask_yes_no(gettext("Delete %s ? [y/N]"), file->d_name) == 1) {
                            if (unlink(file->d_name) != 0) {
                                perror(file->d_name);
                            }
                        }
                    }
                }
                tmp_pkg = NULL;
            }
        }
    }
    closedir(dir);

    slapt_regex_t_free(cached_pkgs_regex);
    if (local_pkg_list) {
        slapt_vector_t_free(avail_pkgs);
    }
}

slapt_pkg_t *slapt_pkg_t_copy(slapt_pkg_t *dst, const slapt_pkg_t *src)
{
    if (dst == NULL) {
        dst = slapt_malloc(sizeof *dst);
    }
    dst = memcpy(dst, src, sizeof *src);

    if (src->name != NULL)
        dst->name = strndup(src->name, strlen(src->name));

    if (src->version != NULL)
        dst->version = strndup(src->version, strlen(src->version));

    if (src->file_ext != NULL)
        dst->file_ext = strndup(src->file_ext, strlen(src->file_ext));

    if (src->mirror != NULL)
        dst->mirror = strndup(src->mirror, strlen(src->mirror));

    if (src->location != NULL)
        dst->location = strndup(src->location, strlen(src->location));

    if (src->description != NULL)
        dst->description = strndup(src->description, strlen(src->description));

    if (src->suggests != NULL)
        dst->suggests = strndup(src->suggests, strlen(src->suggests));

    if (src->conflicts != NULL)
        dst->conflicts = strndup(src->conflicts, strlen(src->conflicts));

    if (src->required != NULL)
        dst->required = strndup(src->required, strlen(src->required));

    dst->dependencies = slapt_vector_t_init((slapt_vector_t_free_function)slapt_dependency_t_free);

    return dst;
}

slapt_pkg_err_t *slapt_pkg_err_t_init(char *pkg, char *err)
{
    slapt_pkg_err_t *e = slapt_malloc(sizeof *e);
    e->pkg = pkg;
    e->error = err;
    return e;
}

void slapt_pkg_err_t_free(slapt_pkg_err_t *e)
{
    free(e->pkg);
    free(e->error);
    free(e);
}

/* FIXME this sucks... it needs to check file headers and more */
static FILE *slapt_gunzip_file(const char *file_name, FILE *dest_file)
{
    if (dest_file == NULL) {
        if ((dest_file = tmpfile()) == NULL) {
            exit(EXIT_FAILURE);
        }
    }

    gzFile data = gzopen(file_name, "rb");
    if (data == NULL) {
        exit(EXIT_FAILURE);
    }

    char buffer[SLAPT_MAX_ZLIB_BUFFER];
    while (gzgets(data, buffer, SLAPT_MAX_ZLIB_BUFFER) != Z_NULL) {
        fprintf(dest_file, "%s", buffer);
    }
    gzclose(data);
    rewind(dest_file);

    return dest_file;
}

slapt_vector_t *slapt_get_pkg_source_packages(const slapt_config_t *global_config, const char *url, bool *compressed)
{
    slapt_vector_t *available_pkgs = NULL;
    bool is_interactive = slapt_is_interactive(global_config);

    *compressed = 0;

    /* try gzipped package list */
    char *pkg_head = slapt_head_mirror_data(url, SLAPT_PKG_LIST_GZ);
    if (pkg_head != NULL) {
        char *pkg_filename = slapt_gen_filename_from_url(url, SLAPT_PKG_LIST_GZ);
        char *pkg_local_head = slapt_read_head_cache(pkg_filename);

        /* is it cached ? */
        if (pkg_local_head != NULL && strcmp(pkg_head, pkg_local_head) == 0) {

            FILE *tmp_pkg_f = tmpfile();
            if (tmp_pkg_f == NULL) {
                exit(EXIT_FAILURE);
            }

            slapt_gunzip_file(pkg_filename, tmp_pkg_f);

            available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);
            fclose(tmp_pkg_f);

            if (available_pkgs == NULL || available_pkgs->size < 1) {
                slapt_clear_head_cache(pkg_filename);
                fprintf(stderr, gettext("Failed to parse package data from %s\n"), pkg_filename);

                if (available_pkgs) {
                    slapt_vector_t_free(available_pkgs);
                }

                free(pkg_filename);
                free(pkg_local_head);
                free(pkg_head);
                return NULL;
            }

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

        } else {
            if (global_config->dl_stats) {
                printf("\n");
            }

            FILE *tmp_pkg_f = slapt_open_file(pkg_filename, "w+b");
            if (tmp_pkg_f == NULL) {
                exit(EXIT_FAILURE);
            }

            /* retrieve the compressed package data */
            const char *err = slapt_get_mirror_data_from_source(tmp_pkg_f, global_config, url, SLAPT_PKG_LIST_GZ);
            if (!err) {
                fclose(tmp_pkg_f);

                FILE *tmp_pkg_uncompressed_f = tmpfile();
                if (tmp_pkg_uncompressed_f == NULL) {
                    exit(EXIT_FAILURE);
                }

                slapt_gunzip_file(pkg_filename, tmp_pkg_uncompressed_f);

                /* parse packages from what we downloaded */
                available_pkgs = slapt_parse_packages_txt(tmp_pkg_uncompressed_f);

                fclose(tmp_pkg_uncompressed_f);

                /* if we can't parse any packages out of this */
                if (available_pkgs == NULL || available_pkgs->size < 1) {
                    slapt_clear_head_cache(pkg_filename);

                    fprintf(stderr, gettext("Failed to parse package data from %s\n"), url);

                    if (available_pkgs)
                        slapt_vector_t_free(available_pkgs);

                    free(pkg_filename);
                    free(pkg_local_head);
                    free(pkg_head);
                    return NULL;
                }

                /* commit the head data for later comparisons */
                slapt_write_head_cache(pkg_head, pkg_filename);

                if (is_interactive)
                    printf(gettext("Done\n"));

            } else {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                fclose(tmp_pkg_f);
                slapt_clear_head_cache(pkg_filename);
                free(pkg_filename);
                free(pkg_local_head);
                free(pkg_head);
                return NULL;
            }
        }

        free(pkg_filename);
        free(pkg_local_head);
        free(pkg_head);
        *compressed = 1;

    } else { /* fall back to uncompressed package list */
        char *pkg_filename = slapt_gen_filename_from_url(url, SLAPT_PKG_LIST);
        char *pkg_local_head = slapt_read_head_cache(pkg_filename);
        /* we go ahead and run the head request, not caring if it failed.
          If the subsequent download fails as well, it will give a nice
          error message of why.
        */
        pkg_head = slapt_head_mirror_data(url, SLAPT_PKG_LIST);

        /* is it cached ? */
        if (pkg_head != NULL && pkg_local_head != NULL && strcmp(pkg_head, pkg_local_head) == 0) {
            FILE *tmp_pkg_f = slapt_open_file(pkg_filename, "r");
            if (tmp_pkg_f == NULL) {
                exit(EXIT_FAILURE);
            }

            available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);
            fclose(tmp_pkg_f);

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

        } else {
            if (global_config->dl_stats) {
                printf("\n");
            }

            FILE *tmp_pkg_f = slapt_open_file(pkg_filename, "w+b");
            if (tmp_pkg_f == NULL) {
                exit(EXIT_FAILURE);
            }

            /* retrieve the uncompressed package data */
            const char *err = slapt_get_mirror_data_from_source(tmp_pkg_f, global_config, url, SLAPT_PKG_LIST);
            if (!err) {
                rewind(tmp_pkg_f); /* make sure we are back at the front of the file */

                /* parse packages from what we downloaded */
                available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);

                /* if we can't parse any packages out of this */
                if (available_pkgs == NULL || available_pkgs->size < 1) {
                    slapt_clear_head_cache(pkg_filename);

                    fprintf(stderr, gettext("Failed to parse package data from %s\n"), url);

                    if (available_pkgs) {
                        slapt_vector_t_free(available_pkgs);
                    }

                    fclose(tmp_pkg_f);
                    free(pkg_filename);
                    free(pkg_local_head);
                    if (pkg_head != NULL) {
                        free(pkg_head);
                    }
                    return NULL;
                }

                /* commit the head data for later comparisons */
                if (pkg_head != NULL) {
                    slapt_write_head_cache(pkg_head, pkg_filename);
                }

                if (is_interactive) {
                    printf(gettext("Done\n"));
                }

            } else {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                slapt_clear_head_cache(pkg_filename);
                free(pkg_filename);
                free(pkg_local_head);
                fclose(tmp_pkg_f);
                if (pkg_head != NULL) {
                    free(pkg_head);
                }
                return NULL;
            }

            fclose(tmp_pkg_f);
        }

        free(pkg_filename);
        free(pkg_local_head);
        if (pkg_head != NULL) {
            free(pkg_head);
        }
    }

    return available_pkgs;
}

slapt_vector_t *slapt_get_pkg_source_patches(const slapt_config_t *global_config, const char *url, bool *compressed)
{
    slapt_vector_t *patch_pkgs = NULL;
    bool is_interactive = slapt_is_interactive(global_config);
    *compressed = 0;

    char *patch_head = slapt_head_mirror_data(url, SLAPT_PATCHES_LIST_GZ);
    if (patch_head != NULL) {
        char *patch_filename = slapt_gen_filename_from_url(url, SLAPT_PATCHES_LIST_GZ);
        char *patch_local_head = slapt_read_head_cache(patch_filename);

        if (patch_local_head != NULL && strcmp(patch_head, patch_local_head) == 0) {
            FILE *tmp_patch_f = tmpfile();
            if (tmp_patch_f == NULL) {
                exit(EXIT_FAILURE);
            }

            slapt_gunzip_file(patch_filename, tmp_patch_f);

            patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);
            fclose(tmp_patch_f);

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

        } else {
            if (global_config->dl_stats)
                printf("\n");

            FILE *tmp_patch_f = slapt_open_file(patch_filename, "w+b");
            if (tmp_patch_f == NULL) {
                exit(1);
            }

            const char *err = slapt_get_mirror_data_from_source(tmp_patch_f, global_config, url, SLAPT_PATCHES_LIST_GZ);
            if (!err) {
                fclose(tmp_patch_f);

                FILE *tmp_patch_uncompressed_f = tmpfile();
                if (tmp_patch_uncompressed_f == NULL) {
                    exit(EXIT_FAILURE);
                }

                slapt_gunzip_file(patch_filename, tmp_patch_uncompressed_f);

                patch_pkgs = slapt_parse_packages_txt(tmp_patch_uncompressed_f);

                if (is_interactive) {
                    printf(gettext("Done\n"));
                }

                if (patch_head != NULL) {
                    slapt_write_head_cache(patch_head, patch_filename);
                }

                fclose(tmp_patch_uncompressed_f);
            } else {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                fclose(tmp_patch_f);
                /* we don't care if the patch fails, for example current
                doesn't have patches source_dl_failed = true; */
                slapt_clear_head_cache(patch_filename);
            }

            if (global_config->dl_stats) {
                printf("\n");
            }
        }

        free(patch_local_head);
        free(patch_filename);
        free(patch_head);
        *compressed = 1;

    } else {
        char *patch_filename = slapt_gen_filename_from_url(url, SLAPT_PATCHES_LIST);
        char *patch_local_head = slapt_read_head_cache(patch_filename);
        /*
        we go ahead and run the head request, not caring if it failed.
        If the subsequent download fails as well, it will give a nice
        error message of why.
        */
        patch_head = slapt_head_mirror_data(url, SLAPT_PATCHES_LIST);

        if (patch_head != NULL && patch_local_head != NULL && strcmp(patch_head, patch_local_head) == 0) {
            FILE *tmp_patch_f = slapt_open_file(patch_filename, "r");
            if (tmp_patch_f == NULL) {
                exit(EXIT_FAILURE);
            }

            patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

            fclose(tmp_patch_f);
        } else {
            if (global_config->dl_stats)
                printf("\n");

            FILE *tmp_patch_f = slapt_open_file(patch_filename, "w+b");
            if (tmp_patch_f == NULL) {
                exit(1);
            }

            const char *err = slapt_get_mirror_data_from_source(tmp_patch_f, global_config, url, SLAPT_PATCHES_LIST);
            if (!err) {
                rewind(tmp_patch_f); /* make sure we are back at the front of the file */
                patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);

                if (is_interactive) {
                    printf(gettext("Done\n"));
                }

                if (patch_head != NULL) {
                    slapt_write_head_cache(patch_head, patch_filename);
                }

            } else {
                /* we don't care if the patch fails, for example current
                  doesn't have patches source_dl_failed = true; */
                slapt_clear_head_cache(patch_filename);

                if (is_interactive) {
                    printf(gettext("Done\n"));
                }
            }

            if (global_config->dl_stats) {
                printf("\n");
            }

            fclose(tmp_patch_f);
        }
        if (patch_head != NULL) {
            free(patch_head);
        }

        free(patch_local_head);
        free(patch_filename);
    }

    return patch_pkgs;
}

FILE *slapt_get_pkg_source_checksums(const slapt_config_t *global_config, const char *url, bool *compressed)
{
    FILE *tmp_checksum_f = NULL;
    bool is_interactive = slapt_is_interactive(global_config);
    *compressed = 0;

    char *checksum_head = slapt_head_mirror_data(url, SLAPT_CHECKSUM_FILE_GZ);
    if (checksum_head != NULL) {
        char *filename = slapt_gen_filename_from_url(url, SLAPT_CHECKSUM_FILE_GZ);
        char *local_head = slapt_read_head_cache(filename);

        if (local_head != NULL && strcmp(checksum_head, local_head) == 0) {
            if ((tmp_checksum_f = tmpfile()) == NULL) {
                exit(EXIT_FAILURE);
            }

            slapt_gunzip_file(filename, tmp_checksum_f);

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

        } else {
            if (global_config->dl_stats) {
                printf("\n");
            }

            FILE *working_checksum_f = slapt_open_file(filename, "w+b");
            if (working_checksum_f == NULL) {
                exit(EXIT_FAILURE);
            }

            const char *err = slapt_get_mirror_data_from_source(working_checksum_f, global_config, url, SLAPT_CHECKSUM_FILE_GZ);
            if (!err) {
                if (global_config->dl_stats) {
                    printf("\n");
                }
                if (is_interactive) {
                    printf(gettext("Done\n"));
                }

                fclose(working_checksum_f);

                if ((tmp_checksum_f = tmpfile()) == NULL) {
                    exit(EXIT_FAILURE);
                }

                slapt_gunzip_file(filename, tmp_checksum_f);

                /* if all is good, write it */
                if (checksum_head != NULL) {
                    slapt_write_head_cache(checksum_head, filename);
                }

            } else {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                slapt_clear_head_cache(filename);
                tmp_checksum_f = working_checksum_f;
                working_checksum_f = NULL;
                free(filename);
                free(local_head);
                fclose(tmp_checksum_f);
                free(checksum_head);
                return NULL;
            }
            /* make sure we are back at the front of the file */
            rewind(tmp_checksum_f);
        }

        free(filename);
        free(local_head);
        free(checksum_head);
        *compressed = 1;

    } else {
        char *filename = slapt_gen_filename_from_url(url, SLAPT_CHECKSUM_FILE);
        char *local_head = slapt_read_head_cache(filename);
        /*
        we go ahead and run the head request, not caring if it failed.
        If the subsequent download fails as well, it will give a nice
        error message of why.
        */
        checksum_head = slapt_head_mirror_data(url, SLAPT_CHECKSUM_FILE);

        if (checksum_head != NULL && local_head != NULL && strcmp(checksum_head, local_head) == 0) {
            if ((tmp_checksum_f = slapt_open_file(filename, "r")) == NULL) {
                exit(EXIT_FAILURE);
            }

            if (is_interactive) {
                printf(gettext("Cached\n"));
            }

        } else {
            if ((tmp_checksum_f = slapt_open_file(filename, "w+b")) == NULL) {
                exit(EXIT_FAILURE);
            }

            if (global_config->dl_stats) {
                printf("\n");
            }

            const char *err = slapt_get_mirror_data_from_source(tmp_checksum_f, global_config, url, SLAPT_CHECKSUM_FILE);
            if (!err) {
                if (is_interactive) {
                    printf(gettext("Done\n"));
                }

            } else {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                slapt_clear_head_cache(filename);
                fclose(tmp_checksum_f);
                free(filename);
                free(local_head);
                if (checksum_head != NULL) {
                    free(checksum_head);
                }
                return NULL;
            }
            /* make sure we are back at the front of the file */
            rewind(tmp_checksum_f);

            /* if all is good, write it */
            if (checksum_head != NULL) {
                slapt_write_head_cache(checksum_head, filename);
            }

            if (global_config->dl_stats) {
                printf("\n");
            }
        }

        free(filename);
        free(local_head);
        if (checksum_head != NULL) {
            free(checksum_head);
        }
    }

    return tmp_checksum_f;
}

bool slapt_get_pkg_source_changelog(const slapt_config_t *global_config, const char *url, bool *compressed)
{
    char *location_gz = SLAPT_CHANGELOG_FILE_GZ;
    char *location_uncomp = SLAPT_CHANGELOG_FILE;
    char *location = location_gz;
    bool is_interactive = slapt_is_interactive(global_config);
    *compressed = 0;

    char *changelog_head = slapt_head_mirror_data(url, location);

    if (changelog_head == NULL) {
        location = location_uncomp;
        changelog_head = slapt_head_mirror_data(url, location);
    } else {
        *compressed = 1;
    }

    if (changelog_head == NULL) {
        if (is_interactive) {
            printf(gettext("Done\n"));
        }
        return true;
    }

    char *filename = slapt_gen_filename_from_url(url, location);
    char *local_head = slapt_read_head_cache(filename);

    if (local_head != NULL && strcmp(changelog_head, local_head) == 0) {
        if (is_interactive) {
            printf(gettext("Cached\n"));
        }

    } else {
        if (global_config->dl_stats) {
            printf("\n");
        }

        FILE *working_changelog_f = slapt_open_file(filename, "w+b");
        if (working_changelog_f == NULL) {
            exit(EXIT_FAILURE);
        }

        const char *err = slapt_get_mirror_data_from_source(working_changelog_f, global_config, url, location);
        if (!err) {
            if (global_config->dl_stats) {
                printf("\n");
            }
            if (is_interactive) {
                printf(gettext("Done\n"));
            }

            /* if all is good, write it */
            if (changelog_head != NULL) {
                slapt_write_head_cache(changelog_head, filename);
            }

            fclose(working_changelog_f);

            if (strcmp(location, location_gz) == 0) {
                char *uncomp_filename = slapt_gen_filename_from_url(url, location_uncomp);
                FILE *uncomp_f = slapt_open_file(uncomp_filename, "w+b");
                free(uncomp_filename);

                slapt_gunzip_file(filename, uncomp_f);
                fclose(uncomp_f);
            }

        } else {
            fprintf(stderr, gettext("Failed to download: %s\n"), err);
            slapt_clear_head_cache(filename);
            free(filename);
            free(local_head);
            free(changelog_head);
            return false;
        }
    }
    free(filename);
    free(local_head);
    free(changelog_head);

    return true;
}

char *slapt_pkg_t_clean_description(const slapt_pkg_t *pkg)
{
    const int token_len = strlen(pkg->name) + 2;
    char token[token_len];
    const int written = snprintf(token, token_len, "%s:", pkg->name);
    if (written != (token_len - 1)) {
        fprintf(stderr, "slapt_pkg_t_clean_description error for %s\n", pkg->name);
        exit(EXIT_FAILURE);
    }

    char *p = NULL;
    char *description = strdup(pkg->description);
    while ((p = strstr(description, token)) != NULL) {
        memmove(p, p + strlen(token), strlen(p) - strlen(token) + 1);
    }

    return description;
}

/* retrieve the packages changelog entry, if any.  Returns NULL otherwise */
char *slapt_pkg_t_changelog(const slapt_pkg_t *pkg)
{
    char *filename = slapt_gen_filename_from_url(pkg->mirror, SLAPT_CHANGELOG_FILE);

    FILE *working_changelog_f = fopen(filename, "rb");
    if (working_changelog_f == NULL) {
        free(filename);
        return NULL;
    }

    /* used with mmap */
    struct stat stat_buf;
    if (stat(filename, &stat_buf) == -1) {
        if (errno)
            perror(filename);

        fprintf(stderr, "stat failed: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    /* don't mmap empty files */
    if ((int)stat_buf.st_size < 1) {
        free(filename);
        fclose(working_changelog_f);
        return NULL;
    }

    const size_t pls = (size_t)stat_buf.st_size;

    char *pkg_data = (char *)mmap(0, pls, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(working_changelog_f), 0);

    if (pkg_data == (void *)-1) {
        if (errno)
            perror(filename);

        fprintf(stderr, "mmap failed: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    fclose(working_changelog_f);

    /* add \0 for strlen to work */
    pkg_data[pls - 1] = '\0';

    /* search for the entry within the changelog */
    char *pkg_name = slapt_pkg_t_string(pkg);

    int changelog_len = 0;
    char *changelog = NULL, *ptr = NULL;
    if ((ptr = strstr(pkg_data, pkg_name)) != NULL) {
        int finished_parsing = 0;

        ptr += strlen(pkg_name);
        if (ptr[0] == ':')
            ptr++;

        while (finished_parsing != 1) {
            char *newline = strchr(ptr, '\n');
            char *start_ptr = ptr;
            int remaining_len = 0;

            if (ptr[0] != '\n' && isblank(ptr[0]) == 0)
                break;

            /* figure out how much to copy in */
            if (newline != NULL) {
                remaining_len = strlen(start_ptr) - strlen(newline);
                ptr = newline + 1; /* skip newline */
            } else {
                remaining_len = strlen(start_ptr);
                finished_parsing = 1;
            }

            char *tmp = realloc(changelog, sizeof *changelog * (changelog_len + remaining_len + 1));
            if (tmp != NULL) {
                changelog = tmp;

                if (changelog_len == 0) {
                    changelog[0] = '\0';
                }

                changelog = strncat(changelog, start_ptr, remaining_len);
                changelog_len += remaining_len;
                changelog[changelog_len] = '\0';
            }
        }
    }
    free(pkg_name);

    /* munmap now that we are done */
    if (munmap(pkg_data, pls) == -1) {
        if (errno)
            perror(filename);

        fprintf(stderr, "munmap failed: %s\n", filename);
        exit(EXIT_FAILURE);
    }
    free(filename);

    return changelog;
}

char *slapt_pkg_t_string(const slapt_pkg_t *pkg)
{
    const int pkg_str_len = strlen(pkg->name) + strlen(pkg->version) + strlen(pkg->file_ext) + 2; /* for the - and \0 */

    char *pkg_str = slapt_malloc(sizeof *pkg_str * pkg_str_len);

    if (snprintf(pkg_str, pkg_str_len, "%s-%s%s", pkg->name, pkg->version, pkg->file_ext) != (int)(pkg_str_len - 1)) {
        free(pkg_str);
        return NULL;
    }

    return pkg_str;
}

slapt_vector_t *
slapt_get_obsolete_pkgs(const slapt_config_t *global_config, const slapt_vector_t *avail_pkgs, const slapt_vector_t *installed_pkgs)
{
    slapt_vector_t *obsolete = slapt_vector_t_init(NULL);
    slapt_vector_t *to_install = slapt_vector_t_init(NULL);
    slapt_vector_t *to_remove = slapt_vector_t_init(NULL);

    slapt_vector_t_foreach (slapt_pkg_t *, p, installed_pkgs) {

        if (slapt_is_excluded(global_config, p))
            continue;

        /* if we can't find the installed package in our available pkg list, it must be obsolete */
        if (slapt_get_newest_pkg(avail_pkgs, p->name) == NULL) {
            /* any packages that require this package we are about to remove should be scheduled to remove as well */
            slapt_vector_t *deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, to_install, to_remove, p);

            slapt_vector_t_foreach (const slapt_pkg_t *, dep, deps) {
                slapt_pkg_t *installed_dep = slapt_get_exact_pkg(installed_pkgs, dep->name, dep->version);

                /* if it is installed, we add it to the list */
                if (installed_dep != NULL) {
                    slapt_vector_t_add(obsolete, installed_dep);
                }
            }

            slapt_vector_t_free(deps);

            slapt_vector_t_add(obsolete, p);
        }
    }

    slapt_vector_t_free(to_install);
    slapt_vector_t_free(to_remove);
    return obsolete;
}

int slapt_pkg_t_qsort_cmp(const void *a, const void *b)
{
    int cmp = 0;

    slapt_pkg_t *pkg_a = *(slapt_pkg_t *const *)a;
    slapt_pkg_t *pkg_b = *(slapt_pkg_t *const *)b;

    if (!pkg_a->name || !pkg_b->name)
        return cmp;

    cmp = strcmp(pkg_a->name, pkg_b->name);

    if (cmp == 0) {
        if ((pkg_a->version && pkg_b->version) && (cmp = strverscmp(pkg_a->version, pkg_b->version)) == 0) {
            if (pkg_a->location && pkg_b->location) {
                return strcmp(pkg_a->location, pkg_b->location);
            } else {
                return cmp;
            }
        } else {
            return cmp;
        }
    } else {
        return cmp;
    }
}

char *slapt_pkg_t_filelist(const slapt_pkg_t *pkg)
{
    char *filelist = NULL;
    /* this only handles installed packages at the moment */
    if (!pkg->installed) {
        return filelist;
    }

    char *pkg_log_dirname = slapt_gen_package_log_dir_name();

    const int pkg_name_len = strlen(pkg->name) + strlen(pkg->version) + 2; /* for the - and \0 */
    char pkg_name[pkg_name_len];
    const int pkg_name_written = snprintf(pkg_name, pkg_name_len, "%s-%s", pkg->name, pkg->version);
    if (pkg_name_written != (pkg_name_len - 1)) {
        return NULL;
    }

    /* build the package filename including the package directory */
    const int pkg_f_name_len = strlen(pkg_log_dirname) + strlen(pkg_name) + 2;
    char pkg_f_name[pkg_f_name_len];
    const int pkg_f_name_written = snprintf(pkg_f_name, pkg_f_name_len, "%s/%s", pkg_log_dirname, pkg_name);
    if (pkg_f_name_written != (pkg_f_name_len - 1)) {
        fprintf(stderr, "slapt_pkg_t_filelist error for %s\n", pkg_name);
        exit(EXIT_FAILURE);
    }

    free(pkg_log_dirname);

    /* open the package log file so that we can mmap it and parse out the file list.  */
    FILE *pkg_f = slapt_open_file(pkg_f_name, "r");
    if (pkg_f == NULL)
        exit(EXIT_FAILURE);

    /* used with mmap */
    struct stat stat_buf;
    if (stat(pkg_f_name, &stat_buf) == -1) {
        if (errno)
            perror(pkg_f_name);

        fprintf(stderr, "stat failed: %s\n", pkg_f_name);
        exit(EXIT_FAILURE);
    }

    /* don't mmap empty files */
    if ((int)stat_buf.st_size < 1) {
        fclose(pkg_f);
        return "";
    }

    const size_t pls = (size_t)stat_buf.st_size;

    char *pkg_data = (char *)mmap(0, pls, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(pkg_f), 0);
    if (pkg_data == (void *)-1) {
        if (errno)
            perror(pkg_f_name);

        fprintf(stderr, "mmap failed: %s\n", pkg_f_name);
        exit(EXIT_FAILURE);
    }

    fclose(pkg_f);

    /* add \0 for strlen to work */
    pkg_data[pls - 1] = '\0';

    size_t filelist_len = 0;
    char *tmp_filelist = NULL;
    char *filelist_p = strstr(pkg_data, "FILE LIST");
    if (filelist_p != NULL) {
        char *nl = strchr(filelist_p, '\n');
        int finished_parsing = 0;

        if (nl != NULL)
            filelist_p = ++nl;

        while (!finished_parsing) {
            size_t tmp_len = 0;
            if ((nl = strchr(filelist_p, '\n')) != NULL) {
                tmp_len = nl - filelist_p + 1;
            } else {
                tmp_len = strlen(filelist_p) + 1;
                finished_parsing = 1;
            }

            /* files in install/ are package metadata */
            if (strncmp(filelist_p, "./\n", 3) != 0 && strncmp(filelist_p, "install/", 8) != 0) {
                tmp_len += 1; /* prefix '/' */

                tmp_filelist = realloc(filelist, sizeof *filelist * (filelist_len + tmp_len + 1));

                if (tmp_filelist != NULL) {
                    filelist = tmp_filelist;
                    tmp_filelist = filelist + filelist_len;
                    tmp_filelist[0] = '/';
                    slapt_strlcpy(tmp_filelist + 1, filelist_p, tmp_len);
                    filelist_len += tmp_len;
                }
            }

            filelist_p = nl + 1;
        }
    }

    /* munmap now that we are done */
    if (munmap(pkg_data, pls) == -1) {
        if (errno) {
            perror(pkg_f_name);
        }

        fprintf(stderr, "munmap failed: %s\n", pkg_f_name);
        exit(EXIT_FAILURE);
    }

    return filelist;
}

slapt_pkg_upgrade_t *slapt_pkg_upgrade_t_init(slapt_pkg_t *i, slapt_pkg_t *u)
{
    slapt_pkg_upgrade_t *upgrade = NULL;
    upgrade = slapt_malloc(sizeof *upgrade);
    upgrade->installed = i;
    upgrade->upgrade = u;
    return upgrade;
}

void slapt_pkg_upgrade_t_free(slapt_pkg_upgrade_t *upgrade)
{
    slapt_pkg_t_free(upgrade->installed);
    slapt_pkg_t_free(upgrade->upgrade);
    free(upgrade);
}

slapt_dependency_t* slapt_dependency_t_init()
{
    slapt_dependency_t *d = malloc(sizeof *d);
    d->alternatives = NULL;
    d->name = NULL;
    d->version = NULL;
    return d;
}

void slapt_dependency_t_free(slapt_dependency_t *d)
{
    if (d->op != DEP_OP_OR) {
        free(d->name);
        free(d->version);
    } else {
        slapt_vector_t_free(d->alternatives);
    }
    free(d);
}

slapt_dependency_t *slapt_dependency_t_parse_required(const char *required)
{
    if (!required) {
        return NULL;
    }
    const size_t required_len = strlen(required);
    if (required_len == 0) {
        return NULL;
    }

    if (strchr(required, '|') != NULL) {
        slapt_dependency_t *d = slapt_dependency_t_init();
        d->op = DEP_OP_OR;
        d->alternatives = slapt_vector_t_init((slapt_vector_t_free_function)slapt_dependency_t_free);
        slapt_vector_t *alts = slapt_parse_delimited_list(required, '|');
        slapt_vector_t_foreach(const char *, alt, alts) {
            slapt_dependency_t *alt_dep = slapt_dependency_t_parse_required(alt);
            slapt_vector_t_add(d->alternatives, alt_dep);
        }
        slapt_vector_t_free(alts);
        return d;
    }

    slapt_dependency_t *d = slapt_dependency_t_init();
    d->op = DEP_OP_ANY;

    const size_t skip_to_name = strspn(required, " ");
    const char *name = required + skip_to_name;
    const size_t pname_len = strcspn(name, " <=>");
    d->name = strndup(name, pname_len);

    if (required_len == pname_len) {
        return d;
    }

    if (strchr(required, '=')) {
        d->op = DEP_OP_EQ;
        if (strchr(required, '>')) {
            d->op = DEP_OP_GTE;
        }
        else if (strchr(required, '<')) {
            d->op = DEP_OP_LTE;
        }
    }
    else if (strchr(required, '>')) {
        d->op = DEP_OP_GT;
    }
    else if (strchr(required, '<')) {
        d->op = DEP_OP_LT;
    }

    const char *post_name = name + pname_len;
    const size_t skip_to_ver = strspn(post_name, "<=> ");
    if ((required_len - skip_to_name - pname_len - skip_to_ver) > 0) {
        const char *remaining = post_name + skip_to_ver;
        const size_t until_spaces = strcspn(remaining, " ");
        d->version = strndup(remaining, until_spaces);
    }

    return d;
}
