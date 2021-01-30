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

#define SLAPT_SOURCE_TOKEN "SOURCE="
#define SLAPT_DISABLED_SOURCE_TOKEN "#DISABLED="
#define SLAPT_WORKINGDIR_TOKEN "WORKINGDIR="
#define SLAPT_WORKINGDIR_TOKEN_LEN 256
#define SLAPT_EXCLUDE_TOKEN "EXCLUDE="
#define SLAPT_SOURCE_ATTRIBUTE_REGEX "(:[A-Z_,]+)$"

typedef struct {
    char *url;
    SLAPT_PRIORITY_T priority;
    bool disabled;
} slapt_source_t;

typedef struct {
    char working_dir[SLAPT_WORKINGDIR_TOKEN_LEN];
    slapt_vector_t *sources;
    slapt_vector_t *exclude_list;
    int (*progress_cb)(void *, double, double, double, double);
    bool download_only;
    bool dist_upgrade;
    bool simulate;
    bool no_prompt;
    bool prompt;
    bool re_install;
    bool ignore_excludes;
    bool no_md5_check;
    bool ignore_dep;
    bool disable_dep_check;
    bool print_uris;
    bool dl_stats;
    bool remove_obsolete;
    bool no_upgrade;
    uint32_t retry;
    bool use_priority;
    bool gpgme_allow_unauth;
} slapt_config_t;

/* initialize slapt_config_t */
slapt_config_t *slapt_config_t_init(void);
/* read the configuration from file_name.  Returns (rc_config *) or NULL */
slapt_config_t *slapt_config_t_read(const char *file_name);
/* free rc_config structure */
void slapt_config_t_free(slapt_config_t *global_config);

/* check that working_dir exists or make it if permissions allow */
void slapt_working_dir_init(const slapt_config_t *global_config);

/* create, destroy the source struct */
slapt_source_t *slapt_source_t_init(const char *s);
void slapt_source_t_free(slapt_source_t *src);

bool slapt_is_interactive(const slapt_config_t *);

int slapt_config_t_write(const slapt_config_t *global_config, const char *location);
