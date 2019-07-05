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

#include "main.h"
/* parse the exclude list */
static slapt_vector_t *parse_exclude(char *line);

slapt_rc_config *slapt_init_config(void)
{
    slapt_rc_config *global_config = slapt_malloc(sizeof *global_config);

    global_config->download_only = false;
    global_config->simulate = false;
    global_config->ignore_excludes = false;
    global_config->no_md5_check = false;
    global_config->dist_upgrade = false;
    global_config->ignore_dep = false;
    global_config->disable_dep_check = false;
    global_config->print_uris = false;
    global_config->dl_stats = false;
    global_config->no_prompt = false;
    global_config->prompt = false;
    global_config->re_install = false;
    global_config->remove_obsolete = false;
    global_config->no_upgrade = false;
    global_config->use_priority = false;
    global_config->working_dir[0] = '\0';
    global_config->progress_cb = NULL;
    global_config->gpgme_allow_unauth = false; /* even without GPGME */

    global_config->sources = slapt_vector_t_init((slapt_vector_t_free_function)slapt_free_source);
    global_config->exclude_list = slapt_vector_t_init(free);

    global_config->retry = 1;

    return global_config;
}

slapt_rc_config *slapt_read_rc_config(const char *file_name)
{
    FILE *rc = NULL;
    char *getline_buffer = NULL;
    size_t gb_length = 0;
    ssize_t g_size;
    slapt_rc_config *global_config = NULL;

    rc = slapt_open_file(file_name, "r");
    if (rc == NULL)
        return NULL;

    global_config = slapt_init_config();

    while ((g_size = getline(&getline_buffer, &gb_length, rc)) != EOF) {
        char *token_ptr = NULL;
        getline_buffer[g_size - 1] = '\0';

        /* check to see if it has our key and value separated by our token */
        /* and extract them */
        if ((strchr(getline_buffer, '#') != NULL) && (strstr(getline_buffer, SLAPT_DISABLED_SOURCE_TOKEN) == NULL))
            continue;

        if ((token_ptr = strstr(getline_buffer, SLAPT_SOURCE_TOKEN)) != NULL) {
            /* SOURCE URL */

            if (strlen(token_ptr) > strlen(SLAPT_SOURCE_TOKEN)) {
                slapt_source_t *s = slapt_init_source(token_ptr + strlen(SLAPT_SOURCE_TOKEN));
                if (s != NULL) {
                    slapt_vector_t_add(global_config->sources, s);
                    if (s->priority != SLAPT_PRIORITY_DEFAULT) {
                        global_config->use_priority = true;
                    }
                }
            }

        } else if ((token_ptr = strstr(getline_buffer, SLAPT_DISABLED_SOURCE_TOKEN)) != NULL) {
            /* DISABLED SOURCE */

            if (strlen(token_ptr) > strlen(SLAPT_DISABLED_SOURCE_TOKEN)) {
                slapt_source_t *s = slapt_init_source(token_ptr + strlen(SLAPT_DISABLED_SOURCE_TOKEN));
                if (s != NULL) {
                    s->disabled = true;
                    slapt_vector_t_add(global_config->sources, s);
                }
            }

        } else if ((token_ptr = strstr(getline_buffer, SLAPT_WORKINGDIR_TOKEN)) != NULL) {
            /* WORKING DIR */

            if (strlen(token_ptr) > strlen(SLAPT_WORKINGDIR_TOKEN)) {
                strncpy(
                    global_config->working_dir,
                    token_ptr + strlen(SLAPT_WORKINGDIR_TOKEN),
                    (strlen(token_ptr) - strlen(SLAPT_WORKINGDIR_TOKEN)));
                global_config->working_dir[(strlen(token_ptr) - strlen(SLAPT_WORKINGDIR_TOKEN))] = '\0';
            }

        } else if ((token_ptr = strstr(getline_buffer, SLAPT_EXCLUDE_TOKEN)) != NULL) {
            /* exclude list */
            slapt_vector_t_free(global_config->exclude_list);
            global_config->exclude_list = parse_exclude(token_ptr);
        }
    }

    fclose(rc);

    if (getline_buffer)
        free(getline_buffer);

    if (strcmp(global_config->working_dir, "") == 0) {
        fprintf(stderr, gettext("WORKINGDIR directive not set within %s.\n"),
                file_name);
        return NULL;
    }
    if (!global_config->sources->size) {
        fprintf(stderr, gettext("SOURCE directive not set within %s.\n"), file_name);
        return NULL;
    }

    return global_config;
}

void slapt_working_dir_init(const slapt_rc_config *global_config)
{
    DIR *working_dir;
    int mode = W_OK, r;
    char *cwd = NULL;

    if ((working_dir = opendir(global_config->working_dir)) == NULL) {
        cwd = getcwd(NULL, 0);
        if (cwd != NULL) {
            r = chdir("/");
            if (r == 0)
                slapt_create_dir_structure(global_config->working_dir);
            r = chdir(cwd);
            free(cwd);
        } else {
            printf(gettext("Failed to build working directory [%s]\n"),
                   global_config->working_dir);
            exit(EXIT_FAILURE);
        }
    }
    if (working_dir != NULL)
        closedir(working_dir);

    /* allow read access if we are simulating */
    if (global_config->simulate)
        mode = R_OK;

    if (access(global_config->working_dir, mode) == -1) {
        if (errno)
            perror(global_config->working_dir);

        fprintf(stderr,
                gettext("Please update permissions on %s or run with appropriate privileges\n"),
                global_config->working_dir);
        exit(EXIT_FAILURE);
    }

    return;
}

void slapt_free_rc_config(slapt_rc_config *global_config)
{
    slapt_vector_t_free(global_config->exclude_list);
    slapt_vector_t_free(global_config->sources);
    free(global_config);
}

static slapt_vector_t *parse_exclude(char *line)
{
    /* skip ahead past the = */
    line = strchr(line, '=') + 1;

    return slapt_parse_delimited_list(line, ',');
}

bool slapt_is_interactive(const slapt_rc_config *global_config)
{
    bool interactive = global_config->progress_cb == NULL
                           ? true
                           : false;

    return interactive;
}

static void slapt_source_parse_attributes(slapt_source_t *s, const char *string)
{
    int offset = 0;
    int len = strlen(string);

    while (offset < len) {
        char *token = NULL;

        if (strchr(string + offset, ',') != NULL) {
            size_t token_len = strcspn(string + offset, ",");
            if (token_len > 0) {
                token = strndup(string + offset, token_len);
                offset += token_len + 1;
            }
        } else {
            token = strdup(string + offset);
            offset += len;
        }

        if (token != NULL) {
            if (strcmp(token, SLAPT_PRIORITY_DEFAULT_TOKEN) == 0) {
                s->priority = SLAPT_PRIORITY_DEFAULT;
            } else if (strcmp(token, SLAPT_PRIORITY_PREFERRED_TOKEN) == 0) {
                s->priority = SLAPT_PRIORITY_PREFERRED;
            } else if (strcmp(token, SLAPT_PRIORITY_OFFICIAL_TOKEN) == 0) {
                s->priority = SLAPT_PRIORITY_OFFICIAL;
            } else if (strcmp(token, SLAPT_PRIORITY_CUSTOM_TOKEN) == 0) {
                s->priority = SLAPT_PRIORITY_CUSTOM;
            } else {
                fprintf(stderr, "Unknown token: %s\n", token);
            }

            free(token);
        }
    }
}

slapt_source_t *slapt_init_source(const char *s)
{
    slapt_source_t *src;
    uint32_t source_len = 0;
    uint32_t attribute_len = 0;
    slapt_regex_t *attribute_regex = NULL;
    char *source_string = NULL;
    char *attribute_string = NULL;

    if (s == NULL)
        return NULL;

    src = slapt_malloc(sizeof *src);
    src->priority = SLAPT_PRIORITY_DEFAULT;
    src->disabled = false;
    source_string = slapt_strip_whitespace(s);
    source_len = strlen(source_string);

    /* parse for :[attr] in the source url */
    if ((attribute_regex = slapt_init_regex(SLAPT_SOURCE_ATTRIBUTE_REGEX)) == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_execute_regex(attribute_regex, source_string);
    if (attribute_regex->reg_return == 0) {
        /* if we find an attribute string, extract it */
        attribute_string = slapt_regex_extract_match(attribute_regex, source_string, 1);
        attribute_len = strlen(attribute_string);
        source_len -= attribute_len;
    }
    slapt_free_regex(attribute_regex);

    /* now add a trailing / if not already there */
    if (source_string[source_len - 1] == '/') {
        src->url = strndup(source_string, source_len);
    } else {
        src->url = slapt_malloc(sizeof *src->url * (source_len + 2));
        src->url[0] = '\0';

        src->url = strncat(
            src->url,
            source_string,
            source_len);

        if (isblank(src->url[source_len - 1]) == 0) {
            src->url = strcat(src->url, "/");
        } else {
            if (src->url[source_len - 2] == '/') {
                src->url[source_len - 2] = '/';
                src->url[source_len - 1] = '\0';
            } else {
                src->url[source_len - 1] = '/';
            }
        }

        src->url[source_len + 1] = '\0';
    }

    free(source_string);

    /* now parse the attribute string */
    if (attribute_string != NULL) {
        slapt_source_parse_attributes(src, attribute_string + 1);
        free(attribute_string);
    }

    return src;
}

void slapt_free_source(slapt_source_t *src)
{
    free(src->url);
    free(src);
}

int slapt_write_rc_config(const slapt_rc_config *global_config, const char *location)
{
    uint32_t i = 0;
    FILE *rc;

    rc = slapt_open_file(location, "w+");
    if (rc == NULL)
        return -1;

    fprintf(rc, "%s%s\n", SLAPT_WORKINGDIR_TOKEN, global_config->working_dir);

    fprintf(rc, "%s", SLAPT_EXCLUDE_TOKEN);
    slapt_vector_t_foreach (char *, exclude, global_config->exclude_list) {
        if (i + 1 == global_config->exclude_list->size) {
            fprintf(rc, "%s", exclude);
        } else {
            fprintf(rc, "%s,", exclude);
        }
        i++;
    }
    fprintf(rc, "\n");

    slapt_vector_t_foreach (slapt_source_t *, src, global_config->sources) {
        SLAPT_PRIORITY_T priority = src->priority;
        const char *token = SLAPT_SOURCE_TOKEN;

        if (src->disabled)
            token = SLAPT_DISABLED_SOURCE_TOKEN;

        if (priority > SLAPT_PRIORITY_DEFAULT) {
            const char *priority_token;

            if (priority == SLAPT_PRIORITY_PREFERRED)
                priority_token = SLAPT_PRIORITY_PREFERRED_TOKEN;
            else if (priority == SLAPT_PRIORITY_OFFICIAL)
                priority_token = SLAPT_PRIORITY_OFFICIAL_TOKEN;
            else if (priority == SLAPT_PRIORITY_CUSTOM)
                priority_token = SLAPT_PRIORITY_CUSTOM_TOKEN;
            else
                priority_token = SLAPT_PRIORITY_DEFAULT_TOKEN;

            fprintf(rc, "%s%s:%s\n", token, src->url, priority_token);
        } else {
            fprintf(rc, "%s%s\n", token, src->url);
        }
    }

    fclose(rc);

    return 0;
}
