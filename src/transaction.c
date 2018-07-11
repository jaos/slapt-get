/*
 * Copyright (C) 2003-2018 Jason Woodward <woodwardj at jaos dot org>
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

#define SLAPT_PKG_DL_NOTE_LEN 16

static slapt_queue_t *slapt_queue_init(void);
static void queue_add_install(slapt_queue_t *t, slapt_pkg_info_t *p);
static void queue_add_upgrade(slapt_queue_t *t, slapt_pkg_upgrade_t *p);
static void queue_free(slapt_queue_t *t);

static void add_suggestion(slapt_transaction_t *tran, slapt_pkg_info_t *pkg);

static void _slapt_add_upgrade_to_transaction(slapt_transaction_t *tran,
                                              slapt_pkg_info_t *installed_pkg,
                                              slapt_pkg_info_t *slapt_upgrade_pkg,
                                              bool reinstall);

slapt_transaction_t *slapt_init_transaction(void)
{
    slapt_transaction_t *tran = slapt_malloc(sizeof *tran);

    tran->install_pkgs = slapt_init_pkg_list();
    tran->install_pkgs->free_pkgs = true;

    tran->remove_pkgs = slapt_init_pkg_list();
    tran->remove_pkgs->free_pkgs = true;

    tran->exclude_pkgs = slapt_init_pkg_list();
    tran->exclude_pkgs->free_pkgs = true;

    tran->upgrade_pkgs = slapt_malloc(sizeof *tran->upgrade_pkgs);
    tran->upgrade_pkgs->pkgs = slapt_malloc(sizeof *tran->upgrade_pkgs->pkgs);
    tran->upgrade_pkgs->pkg_count = 0;
    tran->upgrade_pkgs->reinstall_count = 0;

    tran->suggests = slapt_init_list();

    tran->queue = slapt_queue_init();

    tran->conflict_err = slapt_init_pkg_err_list();
    tran->missing_err = slapt_init_pkg_err_list();

    return tran;
}

int slapt_handle_transaction(const slapt_rc_config *global_config,
                             slapt_transaction_t *tran)
{
    uint32_t pkg_dl_count = 0, dl_counter = 0, len = 0;
    double download_size = 0;
    double already_download_size = 0;
    double uncompressed_size = 0;
    char dl_note[SLAPT_PKG_DL_NOTE_LEN];

    /* show unmet dependencies */
    if (tran->missing_err->err_count > 0) {
        fprintf(stderr, gettext("The following packages have unmet dependencies:\n"));
        slapt_pkg_err_list_t_foreach(error, tran->missing_err) {
            fprintf(stderr, gettext("  %s: Depends: %s\n"), error->pkg, error->error);
        }
    }

    /* show conflicts */
    if (tran->conflict_err->err_count > 0) {
        slapt_pkg_err_list_t_foreach(conflict_error, tran->conflict_err) {
            fprintf(stderr, gettext("%s, which is required by %s, is excluded\n"),
                    conflict_error->error, conflict_error->pkg);
        }
    }

    /* show pkgs to exclude */
    if (tran->exclude_pkgs->pkg_count > 0) {
        printf(gettext("The following packages have been EXCLUDED:\n"));
        printf("  ");

        len = 0;
        slapt_pkg_list_t_foreach(e, tran->exclude_pkgs) {
            if (len + strlen(e->name) + 1 < MAX_LINE_LEN) {
                printf("%s ", e->name);
                len += strlen(e->name) + 1;
            } else {
                printf("\n  %s ", e->name);
                len = strlen(e->name) + 3;
            }
        }

        printf("\n");
    }

    /* show suggested pkgs */
    slapt_generate_suggestions(tran);
    if (tran->suggests->count > 0) {

        printf(gettext("Suggested packages:\n"));
        printf("  ");

        len = 0;
        slapt_list_t_foreach(s, tran->suggests) {
            /* don't show suggestion for something we already have in the transaction */
            if (slapt_search_transaction(tran, s) == 1)
                continue;

            if (len + strlen(s) + 1 < MAX_LINE_LEN) {
                printf("%s ", s);
                len += strlen(s) + 1;
            } else {
                printf("\n  %s ", s);
                len = strlen(s) + 3;
            }
        }
        printf("\n");
    }

    /* show pkgs to install */
    if (tran->install_pkgs->pkg_count > 0) {
        printf(gettext("The following NEW packages will be installed:\n"));
        printf("  ");

        len = 0;
        slapt_pkg_list_t_foreach(p, tran->install_pkgs) {
            size_t existing_file_size = 0;

            if (len + strlen(p->name) + 1 < MAX_LINE_LEN) {
                printf("%s ", p->name);
                len += strlen(p->name) + 1;
            } else {
                printf("\n  %s ", p->name);
                len = strlen(p->name) + 3;
            }

            existing_file_size = slapt_get_pkg_file_size(global_config, p) / 1024;

            download_size += p->size_c;

            if (existing_file_size <= p->size_c)
                already_download_size += existing_file_size;

            uncompressed_size += p->size_u;
        }
        printf("\n");
    }

    /* show pkgs to remove */
    if (tran->remove_pkgs->pkg_count > 0) {
        printf(gettext("The following packages will be REMOVED:\n"));
        printf("  ");

        len = 0;
        slapt_pkg_list_t_foreach(r, tran->remove_pkgs) {
            if (len + strlen(r->name) + 1 < MAX_LINE_LEN) {
                printf("%s ", r->name);
                len += strlen(r->name) + 1;
            } else {
                printf("\n  %s ", r->name);
                len = strlen(r->name) + 3;
            }

            uncompressed_size -= r->size_u;
        }

        printf("\n");
    }

    /* show pkgs to upgrade */
    if (tran->upgrade_pkgs->pkg_count > 0) {

        if ((tran->upgrade_pkgs->pkg_count - tran->upgrade_pkgs->reinstall_count) > 0) {
            printf(gettext("The following packages will be upgraded:\n"));
            printf("  ");
        }

        len = 0;
        slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
            slapt_pkg_info_t *u = upgrade->upgrade;
            slapt_pkg_info_t *p = upgrade->installed;

            size_t existing_file_size = 0;
            int line_len = len + strlen(u->name) + 1;

            existing_file_size = slapt_get_pkg_file_size(
                                     global_config, u) /
                                 1024;

            download_size += u->size_c;

            if (existing_file_size <= u->size_c)
                already_download_size += existing_file_size;

            uncompressed_size += u->size_u;
            uncompressed_size -= p->size_u;

            if (upgrade->reinstall)
                continue;

            if (line_len < MAX_LINE_LEN) {
                printf("%s ", u->name);
                len += strlen(u->name) + 1;
            } else {
                printf("\n  %s ", u->name);
                len = strlen(u->name) + 3;
            }
        }

        if ((tran->upgrade_pkgs->pkg_count - tran->upgrade_pkgs->reinstall_count) > 0)
            printf("\n");

        if (tran->upgrade_pkgs->reinstall_count > 0) {
            printf(gettext("The following packages will be reinstalled:\n"));
            printf("  ");

            len = 0;
            slapt_pkg_upgrade_list_t_foreach(reinstall_upgrade, tran->upgrade_pkgs) {
                slapt_pkg_info_t *u = reinstall_upgrade->upgrade;
                int line_len = len + strlen(u->name) + 1;

                if (!reinstall_upgrade->reinstall)
                    continue;

                if (line_len < MAX_LINE_LEN) {
                    printf("%s ", u->name);
                    len += strlen(u->name) + 1;
                } else {
                    printf("\n  %s ", u->name);
                    len = strlen(u->name) + 3;
                }
            }
            printf("\n");
        }
    }

    /* print the summary */
    printf(ngettext("%d upgraded, ", "%d upgraded, ",
                    tran->upgrade_pkgs->pkg_count - tran->upgrade_pkgs->reinstall_count),
           tran->upgrade_pkgs->pkg_count - tran->upgrade_pkgs->reinstall_count);
    printf(ngettext("%d reinstalled, ", "%d reinstalled, ",
                    tran->upgrade_pkgs->reinstall_count),
           tran->upgrade_pkgs->reinstall_count);
    printf(ngettext("%d newly installed, ", "%d newly installed, ",
                    tran->install_pkgs->pkg_count),
           tran->install_pkgs->pkg_count);
    printf(ngettext("%d to remove, ", "%d to remove, ",
                    tran->remove_pkgs->pkg_count),
           tran->remove_pkgs->pkg_count);
    printf(ngettext("%d not upgraded.\n", "%d not upgraded.\n",
                    tran->exclude_pkgs->pkg_count),
           tran->exclude_pkgs->pkg_count);

    /* only show this if we are going to do download something */
    if (tran->upgrade_pkgs->pkg_count > 0 || tran->install_pkgs->pkg_count > 0) {
        /* how much we need to download */
        double need_to_download_size = download_size - already_download_size;

        /* make sure it's not negative due to changing pkg sizes on upgrades */
        if (need_to_download_size < 0)
            need_to_download_size = 0;

        if (already_download_size > 0) {
            printf(gettext("Need to get %.1f%s/%.1f%s of archives.\n"),
                   (need_to_download_size > 1024) ? need_to_download_size / (double)1024
                                                  : need_to_download_size,
                   (need_to_download_size > 1024) ? "MB" : "kB",
                   (download_size > 1024) ? download_size / (double)1024 : download_size,
                   (download_size > 1024) ? "MB" : "kB");
        } else {
            printf(gettext("Need to get %.1f%s of archives.\n"),
                   (download_size > 1024) ? download_size / (double)1024 : download_size,
                   (download_size > 1024) ? "MB" : "kB");
        }

        /* check we have enough space to download to our working dir */
        if (!slapt_disk_space_check(global_config->working_dir, need_to_download_size)) {
            printf(
                gettext("You don't have enough free space in %s\n"),
                global_config->working_dir);
            exit(EXIT_FAILURE);
        }
        /* check that we have enough space in the root filesystem */
        if (!global_config->download_only) {
            if (!slapt_disk_space_check("/", uncompressed_size)) {
                printf(gettext("You don't have enough free space in %s\n"), "/");
                exit(EXIT_FAILURE);
            }
        }
    }

    if (tran->upgrade_pkgs->pkg_count > 0 || tran->remove_pkgs->pkg_count > 0 ||
        tran->install_pkgs->pkg_count > 0) {
        if (global_config->download_only == false) {
            if ((int)uncompressed_size < 0) {
                uncompressed_size *= -1;
                printf(gettext("After unpacking %.1f%s disk space will be freed.\n"),
                       (uncompressed_size > 1024) ? uncompressed_size / (double)1024
                                                  : uncompressed_size,
                       (uncompressed_size > 1024) ? "MB" : "kB");
            } else {
                printf(
                    gettext("After unpacking %.1f%s of additional disk space will be used.\n"),
                    (uncompressed_size > 1024) ? uncompressed_size / (double)1024
                                               : uncompressed_size,
                    (uncompressed_size > 1024) ? "MB" : "kB");
            }
        }
    }

    /* prompt */
    if ((global_config->prompt) ||
        ((tran->upgrade_pkgs->pkg_count > 0 || tran->remove_pkgs->pkg_count > 0 ||
          (tran->install_pkgs->pkg_count > 0 &&
           global_config->dist_upgrade)) &&
         (global_config->no_prompt == false &&
          global_config->download_only == false &&
          global_config->simulate == false &&
          global_config->print_uris == false))) {
        if (slapt_ask_yes_no(gettext("Do you want to continue? [y/N] ")) != 1) {
            printf(gettext("Abort.\n"));
            return 1;
        }
    }

    if (global_config->print_uris) {
        slapt_pkg_list_t_foreach(info, tran->install_pkgs) {
            const char *location = info->location + strspn(info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   info->mirror, location, info->name,
                   info->version, info->file_ext);
        }
        slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
            const slapt_pkg_info_t *upgrade_info = upgrade->upgrade;
            const char *location = upgrade_info->location + strspn(upgrade_info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   upgrade_info->mirror, location, upgrade_info->name,
                   upgrade_info->version, upgrade_info->file_ext);
        }
        return 0;
    }

    /* if simulate is requested, just show what could happen and return */
    if (global_config->simulate) {
        slapt_pkg_list_t_foreach(r, tran->remove_pkgs) {
            printf(gettext("%s-%s is to be removed\n"), r->name, r->version);
        }

        slapt_queue_t_foreach(q, tran->queue) {
            if (q->type == INSTALL) {
                printf(gettext("%s-%s is to be installed\n"),
                       q->pkg.i->name,
                       q->pkg.i->version);
            } else if (q->type == UPGRADE) {
                printf(gettext("%s-%s is to be upgraded to version %s\n"),
                       q->pkg.u->upgrade->name,
                       q->pkg.u->installed->version,
                       q->pkg.u->upgrade->version);
            }
        }

        printf(gettext("Done\n"));
        return 0;
    }

    pkg_dl_count = tran->install_pkgs->pkg_count + tran->upgrade_pkgs->pkg_count;

    /* download pkgs */
    slapt_pkg_list_t_foreach(pkg, tran->install_pkgs) {
        uint32_t failed = 1;

        ++dl_counter;
        snprintf(dl_note, SLAPT_PKG_DL_NOTE_LEN, "%d/%d", dl_counter, pkg_dl_count);

        for (uint32_t retry_count = 0; retry_count < global_config->retry; ++retry_count) {
            const char *err = slapt_download_pkg(global_config, pkg, dl_note);
            if (err) {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                failed = 1;
            } else {
                failed = 0;
                break;
            }
        }

        if (failed == 1)
            exit(EXIT_FAILURE);
    }

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        uint32_t failed = 1;

        ++dl_counter;
        snprintf(dl_note, SLAPT_PKG_DL_NOTE_LEN, "%d/%d", dl_counter, pkg_dl_count);

        for (uint32_t retry_count = 0; retry_count < global_config->retry; ++retry_count) {
            const char *err = slapt_download_pkg(global_config, upgrade->upgrade, dl_note);
            if (err) {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                failed = 1;
            } else {
                failed = 0;
                break;
            }
        }
        if (failed == 1)
            exit(EXIT_FAILURE);
    }

    printf("\n");

    /* run transaction, remove, install, and upgrade */
    if (global_config->download_only == false) {
        slapt_pkg_list_t_foreach(remove_pkg, tran->remove_pkgs) {
            if (slapt_remove_pkg(global_config, remove_pkg) == -1) {
                exit(EXIT_FAILURE);
            }
        }

        slapt_queue_t_foreach(q, tran->queue) {
            if (q->type == INSTALL) {
                printf(gettext("Preparing to install %s-%s\n"),
                       q->pkg.i->name,
                       q->pkg.i->version);
                if (slapt_install_pkg(global_config,
                                      q->pkg.i) == -1) {
                    exit(EXIT_FAILURE);
                }
            } else if (q->type == UPGRADE) {
                printf(gettext("Preparing to replace %s-%s with %s-%s\n"),
                       q->pkg.u->upgrade->name,
                       q->pkg.u->installed->version,
                       q->pkg.u->upgrade->name,
                       q->pkg.u->upgrade->version);
                if (slapt_upgrade_pkg(global_config,
                                      q->pkg.u->upgrade) == -1) {
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    printf(gettext("Done\n"));

    return 0;
}

void slapt_add_install_to_transaction(slapt_transaction_t *tran,
                                      slapt_pkg_info_t *pkg)
{
    slapt_pkg_info_t **tmp_list;

    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg) == 1)
        return;

    tmp_list = realloc(
        tran->install_pkgs->pkgs,
        sizeof *tran->install_pkgs->pkgs * (tran->install_pkgs->pkg_count + 1));

    if (tmp_list != NULL) {
        tran->install_pkgs->pkgs = tmp_list;

        tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count] = slapt_malloc(
            sizeof *tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count]);
        tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count] = slapt_copy_pkg(
            tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count],
            pkg);
        queue_add_install(
            tran->queue,
            tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count]);

        ++tran->install_pkgs->pkg_count;
    }
}

void slapt_add_remove_to_transaction(slapt_transaction_t *tran,
                                     slapt_pkg_info_t *pkg)
{
    slapt_pkg_info_t **tmp_list;

    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg) == 1)
        return;

    tmp_list = realloc(
        tran->remove_pkgs->pkgs,
        sizeof *tran->remove_pkgs->pkgs * (tran->remove_pkgs->pkg_count + 1));
    if (tmp_list != NULL) {
        tran->remove_pkgs->pkgs = tmp_list;

        tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count] = slapt_malloc(
            sizeof *tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count]);
        tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count] = slapt_copy_pkg(
            tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count],
            pkg);
        ++tran->remove_pkgs->pkg_count;
    }
}

void slapt_add_exclude_to_transaction(slapt_transaction_t *tran,
                                      slapt_pkg_info_t *pkg)
{
    slapt_pkg_info_t **tmp_list;

    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg) == 1)
        return;

    tmp_list = realloc(
        tran->exclude_pkgs->pkgs,
        sizeof *tran->exclude_pkgs->pkgs * (tran->exclude_pkgs->pkg_count + 1));
    if (tmp_list != NULL) {
        tran->exclude_pkgs->pkgs = tmp_list;

        tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count] = slapt_malloc(
            sizeof *tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count]);
        tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count] = slapt_copy_pkg(
            tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count],
            pkg);
        ++tran->exclude_pkgs->pkg_count;
    }
}

void slapt_add_reinstall_to_transaction(slapt_transaction_t *tran,
                                        slapt_pkg_info_t *installed_pkg,
                                        slapt_pkg_info_t *slapt_upgrade_pkg)
{
    _slapt_add_upgrade_to_transaction(tran, installed_pkg, slapt_upgrade_pkg, true);
}

void slapt_add_upgrade_to_transaction(slapt_transaction_t *tran,
                                      slapt_pkg_info_t *installed_pkg,
                                      slapt_pkg_info_t *slapt_upgrade_pkg)
{
    _slapt_add_upgrade_to_transaction(tran, installed_pkg, slapt_upgrade_pkg, false);
}

static void _slapt_add_upgrade_to_transaction(slapt_transaction_t *tran,
                                              slapt_pkg_info_t *installed_pkg,
                                              slapt_pkg_info_t *slapt_upgrade_pkg,
                                              bool reinstall)
{
    slapt_pkg_upgrade_t **tmp_list;

    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, slapt_upgrade_pkg) == 1)
        return;

    tmp_list = realloc(
        tran->upgrade_pkgs->pkgs,
        sizeof *tran->upgrade_pkgs->pkgs * (tran->upgrade_pkgs->pkg_count + 1));

    if (tmp_list != NULL) {
        tran->upgrade_pkgs->pkgs = tmp_list;

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count] = slapt_malloc(
            sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]);

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed =
            slapt_malloc(
                sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed);

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade =
            slapt_malloc(
                sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade);

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed = slapt_copy_pkg(
            tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed,
            installed_pkg);

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade = slapt_copy_pkg(
            tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade,
            slapt_upgrade_pkg);

        tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->reinstall = reinstall;

        queue_add_upgrade(tran->queue,
                          tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]);

        ++tran->upgrade_pkgs->pkg_count;

        if (reinstall)
            ++tran->upgrade_pkgs->reinstall_count;
    }
}

int slapt_search_transaction(slapt_transaction_t *tran, char *pkg_name)
{
    uint32_t found = 1, not_found = 0;

    slapt_pkg_list_t_foreach(pkg, tran->install_pkgs) {
        if (strcmp(pkg_name, pkg->name) == 0)
            return found;
    }

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        if (strcmp(pkg_name, upgrade->upgrade->name) == 0)
            return found;
    }

    slapt_pkg_list_t_foreach(remove_pkg, tran->remove_pkgs) {
        if (strcmp(pkg_name, remove_pkg->name) == 0)
            return found;
    }

    slapt_pkg_list_t_foreach(exclude_pkg, tran->exclude_pkgs) {
        if (strcmp(pkg_name, exclude_pkg->name) == 0)
            return found;
    }

    return not_found;
}

int slapt_search_upgrade_transaction(slapt_transaction_t *tran,
                                     slapt_pkg_info_t *pkg)
{
    uint32_t found = 1, not_found = 0;

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        if (strcmp(pkg->name, upgrade->upgrade->name) == 0)
            return found;
    }

    return not_found;
}

void slapt_free_transaction(slapt_transaction_t *tran)
{
    if (tran->install_pkgs->free_pkgs) {
        slapt_pkg_list_t_foreach(pkg, tran->install_pkgs) {
            slapt_free_pkg(pkg);
        }
    }
    free(tran->install_pkgs->pkgs);
    free(tran->install_pkgs);

    if (tran->remove_pkgs->free_pkgs) {
        slapt_pkg_list_t_foreach(remove_pkg, tran->remove_pkgs) {
            slapt_free_pkg(remove_pkg);
        }
    }
    free(tran->remove_pkgs->pkgs);
    free(tran->remove_pkgs);

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        slapt_free_pkg(upgrade->upgrade);
        slapt_free_pkg(upgrade->installed);
        free(upgrade);
    }
    free(tran->upgrade_pkgs->pkgs);
    free(tran->upgrade_pkgs);

    if (tran->exclude_pkgs->free_pkgs) {
        slapt_pkg_list_t_foreach(exclude_pkg, tran->exclude_pkgs) {
            slapt_free_pkg(exclude_pkg);
        }
    }
    free(tran->exclude_pkgs->pkgs);
    free(tran->exclude_pkgs);

    slapt_free_list(tran->suggests);

    queue_free(tran->queue);

    slapt_free_pkg_err_list(tran->conflict_err);
    slapt_free_pkg_err_list(tran->missing_err);

    free(tran);
}

slapt_transaction_t *slapt_remove_from_transaction(slapt_transaction_t *tran,
                                                   slapt_pkg_info_t *pkg)
{
    slapt_transaction_t *new_tran = NULL;

    if (slapt_search_transaction_by_pkg(tran, pkg) == 0)
        return tran;

    /* since this is a pointer, slapt_malloc before calling init */
    new_tran = slapt_malloc(sizeof *new_tran);
    new_tran->install_pkgs = slapt_malloc(sizeof *new_tran->install_pkgs);
    new_tran->remove_pkgs = slapt_malloc(sizeof *new_tran->remove_pkgs);
    new_tran->upgrade_pkgs = slapt_malloc(sizeof *new_tran->upgrade_pkgs);
    new_tran->exclude_pkgs = slapt_malloc(sizeof *new_tran->exclude_pkgs);
    new_tran = slapt_init_transaction();

    slapt_pkg_list_t_foreach(installed_pkg, tran->install_pkgs) {
        if (
            strcmp(pkg->name, installed_pkg->name) == 0 &&
            strcmp(pkg->version, installed_pkg->version) == 0 &&
            strcmp(pkg->location, installed_pkg->location) == 0) {
            continue;
        }

        slapt_add_install_to_transaction(new_tran, installed_pkg);
    }

    slapt_pkg_list_t_foreach(remove_pkg, tran->remove_pkgs) {
        if (
            strcmp(pkg->name, remove_pkg->name) == 0 &&
            strcmp(pkg->version, remove_pkg->version) == 0 &&
            strcmp(pkg->location, remove_pkg->location) == 0) {
            continue;
        }

        slapt_add_remove_to_transaction(new_tran, remove_pkg);
    }

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        slapt_pkg_info_t *u = upgrade->upgrade;
        slapt_pkg_info_t *p = upgrade->installed;

        if (
            strcmp(pkg->name, u->name) == 0 &&
            strcmp(pkg->version, u->version) == 0 &&
            strcmp(pkg->location, u->location) == 0) {
            continue;
        }

        slapt_add_upgrade_to_transaction(new_tran, p, u);
    }

    slapt_pkg_list_t_foreach(exclude_pkg, tran->exclude_pkgs) {
        if (
            strcmp(pkg->name, exclude_pkg->name) == 0 &&
            strcmp(pkg->version, exclude_pkg->version) == 0 &&
            strcmp(pkg->location, exclude_pkg->location) == 0) {
            continue;
        }

        slapt_add_exclude_to_transaction(new_tran, exclude_pkg);
    }

    return new_tran;
}

/* parse the dependencies for a package, and add them to the transaction as */
/* needed check to see if a package is conflicted */
int slapt_add_deps_to_trans(const slapt_rc_config *global_config,
                            slapt_transaction_t *tran,
                            slapt_pkg_list_t *avail_pkgs,
                            slapt_pkg_list_t *installed_pkgs,
                            slapt_pkg_info_t *pkg)
{
    int dep_return = -1;
    slapt_pkg_list_t *deps = NULL;

    if (global_config->disable_dep_check)
        return 0;

    if (pkg == NULL)
        return 0;

    deps = slapt_init_pkg_list();

    dep_return = slapt_get_pkg_dependencies(
        global_config, avail_pkgs, installed_pkgs, pkg,
        deps, tran->conflict_err, tran->missing_err);

    /* check to see if there where issues with dep checking */
    /* exclude the package if dep check barfed */
    if ((dep_return == -1) && (global_config->ignore_dep == false) &&
        (slapt_get_exact_pkg(tran->exclude_pkgs, pkg->name, pkg->version) == NULL)) {
        slapt_free_pkg_list(deps);
        return -1;
    }

    /* loop through the deps */
    slapt_pkg_list_t_foreach(dep, deps) {
        slapt_pkg_info_t *dep_installed = NULL;
        slapt_pkg_list_t *conflicts = NULL;

        /*
         * the dep wouldn't get this far if it where excluded,
         * so we don't check for that here
       */

        conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, dep);

        slapt_pkg_list_t_foreach(conflict, conflicts) {
            slapt_add_remove_to_transaction(tran, conflict);
        }

        slapt_free_pkg_list(conflicts);

        dep_installed = slapt_get_newest_pkg(installed_pkgs, dep->name);
        if (dep_installed == NULL) {
            slapt_add_install_to_transaction(tran, dep);
        } else {
            /* add only if its a valid upgrade */
            if (slapt_cmp_pkgs(dep_installed, dep) < 0)
                slapt_add_upgrade_to_transaction(tran, dep_installed, dep);
        }
    }

    slapt_free_pkg_list(deps);

    return 0;
}

/* make sure pkg isn't conflicted with what's already in the transaction */
slapt_pkg_list_t *slapt_is_conflicted(slapt_transaction_t *tran,
                                      slapt_pkg_list_t *avail_pkgs,
                                      slapt_pkg_list_t *installed_pkgs,
                                      slapt_pkg_info_t *pkg)
{
    slapt_pkg_list_t *conflicts = NULL;
    slapt_pkg_list_t *conflicts_in_transaction = slapt_init_pkg_list();

    /* if conflicts exist, check to see if they are installed
     or in the current transaction
  */
    conflicts = slapt_get_pkg_conflicts(avail_pkgs, installed_pkgs, pkg);
    slapt_pkg_list_t_foreach(conflict, conflicts) {
        if (slapt_search_upgrade_transaction(tran, conflict) == 1 || slapt_get_newest_pkg(tran->install_pkgs, conflict->name) != NULL) {
            printf(gettext("%s, which is to be installed, conflicts with %s\n"), conflict->name, pkg->name);

            slapt_add_pkg_to_pkg_list(conflicts_in_transaction, conflict);
        }
        if (slapt_get_newest_pkg(installed_pkgs, conflict->name) != NULL) {
            printf(gettext("Installed %s conflicts with %s\n"), conflict->name, pkg->name);

            slapt_add_pkg_to_pkg_list(conflicts_in_transaction, conflict);
        }
    }

    slapt_free_pkg_list(conflicts);

    return conflicts_in_transaction;
}

static void add_suggestion(slapt_transaction_t *tran, slapt_pkg_info_t *pkg)
{
    slapt_list_t *suggests = NULL;

    if (pkg->suggests == NULL || strlen(pkg->suggests) == 0) {
        return;
    }

    suggests = slapt_parse_delimited_list(pkg->suggests, ',');

    slapt_list_t_foreach(s, suggests) {
        /* no need to add it if we already have it */
        if (slapt_search_transaction(tran, s) == 1)
            continue;

        slapt_add_list_item(tran->suggests, s);
    }

    slapt_free_list(suggests);
}

int slapt_search_transaction_by_pkg(slapt_transaction_t *tran,
                                    slapt_pkg_info_t *pkg)
{
    uint32_t found = 1, not_found = 0;

    slapt_pkg_list_t_foreach(install_pkg, tran->install_pkgs) {
        if ((strcmp(pkg->name, install_pkg->name) == 0) && (strcmp(pkg->version, install_pkg->version) == 0))
            return found;
    }

    slapt_pkg_upgrade_list_t_foreach(upgrade, tran->upgrade_pkgs) {
        slapt_pkg_info_t *p = upgrade->upgrade;

        if ((strcmp(pkg->name, p->name) == 0) && (strcmp(pkg->version, p->version) == 0))
            return found;
    }

    slapt_pkg_list_t_foreach(remove_pkg, tran->remove_pkgs) {
        if ((strcmp(pkg->name, remove_pkg->name) == 0) && (strcmp(pkg->version, remove_pkg->version) == 0))
            return found;
    }

    slapt_pkg_list_t_foreach(exclude_pkg, tran->exclude_pkgs) {
        if ((strcmp(pkg->name, exclude_pkg->name) == 0) && (strcmp(pkg->version, exclude_pkg->version) == 0))
            return found;
    }

    return not_found;
}

static slapt_queue_t *slapt_queue_init(void)
{
    slapt_queue_t *t = NULL;
    t = slapt_malloc(sizeof *t);
    t->pkgs = slapt_malloc(sizeof *t->pkgs);
    t->count = 0;

    return t;
}

static void queue_add_install(slapt_queue_t *t, slapt_pkg_info_t *p)
{
    slapt_queue_i **tmp;
    tmp = realloc(t->pkgs, sizeof *t->pkgs * (t->count + 1));

    if (!tmp)
        return;

    t->pkgs = tmp;
    t->pkgs[t->count] = slapt_malloc(sizeof *t->pkgs[t->count]);
    t->pkgs[t->count]->pkg.i = p;
    t->pkgs[t->count]->type = INSTALL;
    ++t->count;
}

static void queue_add_upgrade(slapt_queue_t *t, slapt_pkg_upgrade_t *p)
{
    slapt_queue_i **tmp;
    tmp = realloc(t->pkgs, sizeof *t->pkgs * (t->count + 1));

    if (!tmp)
        return;

    t->pkgs = tmp;
    t->pkgs[t->count] = slapt_malloc(sizeof *t->pkgs[t->count]);
    t->pkgs[t->count]->pkg.u = p;
    t->pkgs[t->count]->type = UPGRADE;
    ++t->count;
}

static void queue_free(slapt_queue_t *t)
{
    slapt_queue_t_foreach(i, t) {
        free(i);
    }
    free(t->pkgs);
    free(t);
}

void slapt_generate_suggestions(slapt_transaction_t *tran)
{
    slapt_pkg_list_t_foreach(pkg, tran->install_pkgs) {
        add_suggestion(tran, pkg);
    }
}
