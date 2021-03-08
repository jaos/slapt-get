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

#define SLAPT_PKG_DL_NOTE_LEN 16

static void add_suggestion(slapt_transaction_t *tran, slapt_pkg_t *pkg);

slapt_transaction_t *slapt_init_transaction(void)
{
    slapt_transaction_t *tran = slapt_malloc(sizeof *tran);

    tran->install_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    tran->remove_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    tran->exclude_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    tran->upgrade_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_upgrade_t_free);
    tran->reinstall_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_upgrade_t_free);

    tran->suggests = slapt_vector_t_init(NULL);

    tran->queue = slapt_vector_t_init((slapt_vector_t_free_function)slapt_queue_i_free);

    tran->conflict_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);
    tran->missing_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);

    return tran;
}

slapt_queue_i *slapt_queue_i_init(slapt_pkg_t *i, slapt_pkg_upgrade_t *u)
{
    slapt_queue_i *qi = NULL;
    qi = slapt_malloc(sizeof *qi);
    if ((i && u) || (!i && !u)) {
        exit(EXIT_FAILURE);
    }
    if (i) {
        qi->pkg.i = i;
        qi->type = SLAPT_ACTION_INSTALL;
    } else if (u) {
        qi->pkg.u = u;
        qi->type = SLAPT_ACTION_UPGRADE;
    }
    return qi;
}

void slapt_queue_i_free(slapt_queue_i *qi)
{
    free(qi);
}

int slapt_handle_transaction(const slapt_config_t *global_config, slapt_transaction_t *tran)
{
    uint32_t pkg_dl_count = 0, dl_counter = 0, len = 0;
    double download_size = 0;
    double already_download_size = 0;
    double uncompressed_size = 0;
    char dl_note[SLAPT_PKG_DL_NOTE_LEN];

    /* show unmet dependencies */
    if (tran->missing_err->size > 0) {
        fprintf(stderr, gettext("The following packages have unmet dependencies:\n"));
        slapt_vector_t_foreach (slapt_pkg_err_t *, error, tran->missing_err) {
            fprintf(stderr, gettext("  %s: Depends: %s\n"), error->pkg, error->error);
        }
    }

    /* show conflicts */
    if (tran->conflict_err->size > 0) {
        slapt_vector_t_foreach (slapt_pkg_err_t *, conflict_error, tran->conflict_err) {
            fprintf(stderr, gettext("%s, which is required by %s, is excluded\n"), conflict_error->error, conflict_error->pkg);
        }
    }

    /* show pkgs to exclude */
    if (tran->exclude_pkgs->size > 0) {
        printf(gettext("The following packages have been EXCLUDED:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (slapt_pkg_t *, e, tran->exclude_pkgs) {
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
    if (tran->suggests->size > 0) {
        printf(gettext("Suggested packages:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (char *, s, tran->suggests) {
            /* don't show suggestion for something we already have in the transaction */
            if (slapt_search_transaction(tran, s))
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
    if (tran->install_pkgs->size > 0) {
        printf(gettext("The following NEW packages will be installed:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (slapt_pkg_t *, p, tran->install_pkgs) {
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
    if (tran->remove_pkgs->size > 0) {
        printf(gettext("The following packages will be REMOVED:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (slapt_pkg_t *, r, tran->remove_pkgs) {
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
    if (tran->upgrade_pkgs->size > 0) {
        printf(gettext("The following packages will be upgraded:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
            slapt_pkg_t *u = upgrade->upgrade;
            slapt_pkg_t *p = upgrade->installed;

            int line_len = len + strlen(u->name) + 1;
            size_t existing_file_size = slapt_get_pkg_file_size(global_config, u) / 1024;
            download_size += u->size_c;
            if (existing_file_size <= u->size_c)
                already_download_size += existing_file_size;
            uncompressed_size += u->size_u;
            uncompressed_size -= p->size_u;

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

    if (tran->reinstall_pkgs->size > 0) {
        printf(gettext("The following packages will be reinstalled:\n"));
        printf("  ");

        len = 0;
        slapt_vector_t_foreach (slapt_pkg_upgrade_t *, reinstall_upgrade, tran->reinstall_pkgs) {
            slapt_pkg_t *u = reinstall_upgrade->upgrade;
            slapt_pkg_t *p = reinstall_upgrade->installed;

            int line_len = len + strlen(u->name) + 1;
            size_t existing_file_size = slapt_get_pkg_file_size(global_config, u) / 1024;
            download_size += u->size_c;
            if (existing_file_size <= u->size_c)
                already_download_size += existing_file_size;
            uncompressed_size += u->size_u;
            uncompressed_size -= p->size_u;

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

    /* print the summary */
    printf(ngettext("%d upgraded, ", "%d upgraded, ", tran->upgrade_pkgs->size), tran->upgrade_pkgs->size);
    printf(ngettext("%d reinstalled, ", "%d reinstalled, ", tran->reinstall_pkgs->size), tran->reinstall_pkgs->size);
    printf(ngettext("%d newly installed, ", "%d newly installed, ", tran->install_pkgs->size), tran->install_pkgs->size);
    printf(ngettext("%d to remove, ", "%d to remove, ", tran->remove_pkgs->size), tran->remove_pkgs->size);
    printf(ngettext("%d not upgraded.\n", "%d not upgraded.\n", tran->exclude_pkgs->size), tran->exclude_pkgs->size);

    /* only show this if we are going to do download something */
    if (tran->upgrade_pkgs->size > 0 || tran->install_pkgs->size > 0 || tran->reinstall_pkgs->size > 0) {
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
            printf(gettext("You don't have enough free space in %s\n"), global_config->working_dir);
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

    if (tran->upgrade_pkgs->size > 0 || tran->remove_pkgs->size > 0 || tran->install_pkgs->size > 0 || tran->reinstall_pkgs->size > 0) {
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
        ((tran->upgrade_pkgs->size > 0 || tran->remove_pkgs->size > 0 || tran->reinstall_pkgs->size > 0 ||
          (tran->install_pkgs->size > 0 &&
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
        slapt_vector_t_foreach (slapt_pkg_t *, info, tran->install_pkgs) {
            const char *location = info->location + strspn(info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   info->mirror, location, info->name,
                   info->version, info->file_ext);
        }
        slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
            const slapt_pkg_t *upgrade_info = upgrade->upgrade;
            const char *location = upgrade_info->location + strspn(upgrade_info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   upgrade_info->mirror, location, upgrade_info->name,
                   upgrade_info->version, upgrade_info->file_ext);
        }
        slapt_vector_t_foreach (slapt_pkg_upgrade_t *, reinstall, tran->reinstall_pkgs) {
            const slapt_pkg_t *upgrade_info = reinstall->upgrade;
            const char *location = upgrade_info->location + strspn(upgrade_info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   upgrade_info->mirror, location, upgrade_info->name,
                   upgrade_info->version, upgrade_info->file_ext);
        }
        return 0;
    }

    /* if simulate is requested, just show what could happen and return */
    if (global_config->simulate) {
        slapt_vector_t_foreach (slapt_pkg_t *, r, tran->remove_pkgs) {
            printf(gettext("%s-%s is to be removed\n"), r->name, r->version);
        }

        slapt_vector_t_foreach (slapt_queue_i *, q, tran->queue) {
            if (q->type == SLAPT_ACTION_INSTALL) {
                printf(gettext("%s-%s is to be installed\n"),
                       q->pkg.i->name,
                       q->pkg.i->version);
            } else if (q->type == SLAPT_ACTION_UPGRADE) {
                printf(gettext("%s-%s is to be upgraded to version %s\n"),
                       q->pkg.u->upgrade->name,
                       q->pkg.u->installed->version,
                       q->pkg.u->upgrade->version);
            } else {
                exit(EXIT_FAILURE);
            }
        }

        printf(gettext("Done\n"));
        return 0;
    }

    pkg_dl_count = tran->install_pkgs->size + tran->upgrade_pkgs->size + tran->reinstall_pkgs->size;

    /* download pkgs */
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, tran->install_pkgs) {
        bool failed = true;

        ++dl_counter;
        snprintf(dl_note, SLAPT_PKG_DL_NOTE_LEN, "%d/%d", dl_counter, pkg_dl_count);

        for (uint32_t retry_count = 0; retry_count < global_config->retry; ++retry_count) {
            const char *err = slapt_download_pkg(global_config, pkg, dl_note);
            if (err) {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                failed = true;
            } else {
                failed = false;
                break;
            }
        }

        if (failed)
            exit(EXIT_FAILURE);
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
        bool failed = true;

        ++dl_counter;
        snprintf(dl_note, SLAPT_PKG_DL_NOTE_LEN, "%d/%d", dl_counter, pkg_dl_count);

        for (uint32_t retry_count = 0; retry_count < global_config->retry; ++retry_count) {
            const char *err = slapt_download_pkg(global_config, upgrade->upgrade, dl_note);
            if (err) {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                failed = true;
            } else {
                failed = false;
                break;
            }
        }
        if (failed)
            exit(EXIT_FAILURE);
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, reinstall, tran->reinstall_pkgs) {
        bool failed = true;

        ++dl_counter;
        snprintf(dl_note, SLAPT_PKG_DL_NOTE_LEN, "%d/%d", dl_counter, pkg_dl_count);

        for (uint32_t retry_count = 0; retry_count < global_config->retry; ++retry_count) {
            const char *err = slapt_download_pkg(global_config, reinstall->upgrade, dl_note);
            if (err) {
                fprintf(stderr, gettext("Failed to download: %s\n"), err);
                failed = true;
            } else {
                failed = false;
                break;
            }
        }
        if (failed)
            exit(EXIT_FAILURE);
    }

    printf("\n");

    /* run transaction, remove, install, and upgrade */
    if (global_config->download_only == false) {
        slapt_vector_t_foreach (slapt_pkg_t *, remove_pkg, tran->remove_pkgs) {
            if (slapt_remove_pkg(global_config, remove_pkg) == -1) {
                exit(EXIT_FAILURE);
            }
        }

        slapt_vector_t_foreach (slapt_queue_i *, q, tran->queue) {
            if (q->type == SLAPT_ACTION_INSTALL) {
                printf(gettext("Preparing to install %s-%s\n"), q->pkg.i->name, q->pkg.i->version);
                if (slapt_install_pkg(global_config, q->pkg.i) == -1) {
                    exit(EXIT_FAILURE);
                }
            } else if (q->type == SLAPT_ACTION_UPGRADE) {
                printf(gettext("Preparing to replace %s-%s with %s-%s\n"),
                       q->pkg.u->upgrade->name,
                       q->pkg.u->installed->version,
                       q->pkg.u->upgrade->name,
                       q->pkg.u->upgrade->version);
                if (slapt_upgrade_pkg(global_config, q->pkg.u->upgrade) == -1) {
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    printf(gettext("Done\n"));

    return 0;
}

void slapt_add_install_to_transaction(slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg))
        return;

    slapt_pkg_t *i = slapt_copy_pkg(NULL, pkg);
    slapt_vector_t_add(tran->install_pkgs, i);
    slapt_vector_t_add(tran->queue, slapt_queue_i_init(tran->install_pkgs->items[tran->install_pkgs->size - 1], NULL));
}

void slapt_add_remove_to_transaction(slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg))
        return;

    slapt_pkg_t *r = slapt_copy_pkg(NULL, pkg);
    slapt_vector_t_add(tran->remove_pkgs, r);
}

void slapt_add_exclude_to_transaction(slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, pkg))
        return;

    slapt_pkg_t *e = slapt_copy_pkg(NULL, pkg);
    slapt_vector_t_add(tran->exclude_pkgs, e);
}

void slapt_add_reinstall_to_transaction(slapt_transaction_t *tran, slapt_pkg_t *installed_pkg, slapt_pkg_t *slapt_upgrade_pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, slapt_upgrade_pkg))
        return;

    slapt_pkg_t *i = slapt_copy_pkg(NULL, installed_pkg);
    slapt_pkg_t *u = slapt_copy_pkg(NULL, slapt_upgrade_pkg);

    slapt_vector_t_add(tran->reinstall_pkgs, slapt_pkg_upgrade_t_init(i, u));
    slapt_vector_t_add(tran->queue, slapt_queue_i_init(NULL, tran->reinstall_pkgs->items[tran->reinstall_pkgs->size - 1]));
}

void slapt_add_upgrade_to_transaction(slapt_transaction_t *tran, const slapt_pkg_t *installed_pkg, const slapt_pkg_t *slapt_upgrade_pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_search_transaction_by_pkg(tran, slapt_upgrade_pkg))
        return;

    slapt_pkg_t *i = slapt_copy_pkg(NULL, installed_pkg);
    slapt_pkg_t *u = slapt_copy_pkg(NULL, slapt_upgrade_pkg);

    slapt_vector_t_add(tran->upgrade_pkgs, slapt_pkg_upgrade_t_init(i, u));
    slapt_vector_t_add(tran->queue, slapt_queue_i_init(NULL, tran->upgrade_pkgs->items[tran->upgrade_pkgs->size - 1]));
}

bool slapt_search_transaction(const slapt_transaction_t *tran, const char *pkg_name)
{
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, tran->install_pkgs) {
        if (strcmp(pkg_name, pkg->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
        if (strcmp(pkg_name, upgrade->upgrade->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, reinstall, tran->reinstall_pkgs) {
        if (strcmp(pkg_name, reinstall->upgrade->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_t *, remove_pkg, tran->remove_pkgs) {
        if (strcmp(pkg_name, remove_pkg->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_t *, exclude_pkg, tran->exclude_pkgs) {
        if (strcmp(pkg_name, exclude_pkg->name) == 0)
            return true;
    }

    return false;
}

bool slapt_search_upgrade_transaction(const slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
        if (strcmp(pkg->name, upgrade->upgrade->name) == 0)
            return true;
    }

    return false;
}

void slapt_free_transaction(slapt_transaction_t *tran)
{
    slapt_vector_t_free(tran->install_pkgs);
    slapt_vector_t_free(tran->remove_pkgs);
    slapt_vector_t_free(tran->upgrade_pkgs);
    slapt_vector_t_free(tran->reinstall_pkgs);
    slapt_vector_t_free(tran->exclude_pkgs);
    slapt_vector_t_free(tran->suggests);
    slapt_vector_t_free(tran->queue);
    slapt_vector_t_free(tran->conflict_err);
    slapt_vector_t_free(tran->missing_err);

    free(tran);
}

slapt_transaction_t *slapt_remove_from_transaction(slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    slapt_transaction_t *new_tran = NULL;

    if (!slapt_search_transaction_by_pkg(tran, pkg))
        return tran;

    /* since this is a pointer, slapt_malloc before calling init */
    new_tran = slapt_malloc(sizeof *new_tran);
    new_tran->install_pkgs = slapt_malloc(sizeof *new_tran->install_pkgs);
    new_tran->remove_pkgs = slapt_malloc(sizeof *new_tran->remove_pkgs);
    new_tran->upgrade_pkgs = slapt_malloc(sizeof *new_tran->upgrade_pkgs);
    new_tran->exclude_pkgs = slapt_malloc(sizeof *new_tran->exclude_pkgs);
    new_tran = slapt_init_transaction();

    slapt_vector_t_foreach (slapt_pkg_t *, installed_pkg, tran->install_pkgs) {
        if (strcmp(pkg->name, installed_pkg->name) == 0 &&
            strcmp(pkg->version, installed_pkg->version) == 0 &&
            strcmp(pkg->location, installed_pkg->location) == 0) {
            continue;
        }

        slapt_add_install_to_transaction(new_tran, installed_pkg);
    }

    slapt_vector_t_foreach (slapt_pkg_t *, remove_pkg, tran->remove_pkgs) {
        if (strcmp(pkg->name, remove_pkg->name) == 0 &&
            strcmp(pkg->version, remove_pkg->version) == 0 &&
            strcmp(pkg->location, remove_pkg->location) == 0) {
            continue;
        }

        slapt_add_remove_to_transaction(new_tran, remove_pkg);
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
        slapt_pkg_t *u = upgrade->upgrade;
        slapt_pkg_t *p = upgrade->installed;

        if (strcmp(pkg->name, u->name) == 0 &&
            strcmp(pkg->version, u->version) == 0 &&
            strcmp(pkg->location, u->location) == 0) {
            continue;
        }

        slapt_add_upgrade_to_transaction(new_tran, p, u);
    }

    slapt_vector_t_foreach (slapt_pkg_t *, exclude_pkg, tran->exclude_pkgs) {
        if (strcmp(pkg->name, exclude_pkg->name) == 0 &&
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
int slapt_add_deps_to_trans(const slapt_config_t *global_config,
                            slapt_transaction_t *tran,
                            const slapt_vector_t *avail_pkgs,
                            const slapt_vector_t *installed_pkgs,
                            slapt_pkg_t *pkg)
{
    int dep_return = -1;

    if (global_config->disable_dep_check)
        return 0;

    if (pkg == NULL)
        return 0;

    slapt_vector_t *deps = slapt_vector_t_init(NULL);

    dep_return = slapt_get_pkg_dependencies(global_config, avail_pkgs, installed_pkgs, pkg, deps, tran->conflict_err, tran->missing_err);

    /* check to see if there where issues with dep checking */
    /* exclude the package if dep check barfed */
    if ((dep_return == -1) && (global_config->ignore_dep == false) && (slapt_get_exact_pkg(tran->exclude_pkgs, pkg->name, pkg->version) == NULL)) {
        slapt_vector_t_free(deps);
        return -1;
    }

    /* loop through the deps */
    slapt_vector_t_foreach (slapt_pkg_t *, dep, deps) {
        slapt_pkg_t *dep_installed = NULL;

        /* the dep wouldn't get this far if it where excluded, so we don't check for that here */
        slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, dep);

        slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
            slapt_add_remove_to_transaction(tran, conflict);
        }

        slapt_vector_t_free(conflicts);

        dep_installed = slapt_get_newest_pkg(installed_pkgs, dep->name);
        if (dep_installed == NULL) {
            slapt_add_install_to_transaction(tran, dep);
        } else {
            /* add only if its a valid upgrade */
            if (slapt_cmp_pkgs(dep_installed, dep) < 0)
                slapt_add_upgrade_to_transaction(tran, dep_installed, dep);
        }
    }

    slapt_vector_t_free(deps);

    return 0;
}

/* make sure pkg isn't conflicted with what's already in the transaction */
slapt_vector_t *slapt_is_conflicted(const slapt_transaction_t *tran,
                                    const slapt_vector_t *avail_pkgs,
                                    const slapt_vector_t *installed_pkgs,
                                    const slapt_pkg_t *pkg)
{
    slapt_vector_t *conflicts = NULL;
    slapt_vector_t *conflicts_in_transaction = slapt_vector_t_init(NULL);

    /* if conflicts exist, check to see if they are installed or in the current transaction */
    conflicts = slapt_get_pkg_conflicts(avail_pkgs, installed_pkgs, pkg);
    slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
        if (slapt_search_upgrade_transaction(tran, conflict) == 1 || slapt_get_newest_pkg(tran->install_pkgs, conflict->name) != NULL) {
            printf(gettext("%s, which is to be installed, conflicts with %s\n"), conflict->name, pkg->name);

            slapt_vector_t_add(conflicts_in_transaction, conflict);
        }
        if (slapt_get_newest_pkg(installed_pkgs, conflict->name) != NULL) {
            printf(gettext("Installed %s conflicts with %s\n"), conflict->name, pkg->name);

            slapt_vector_t_add(conflicts_in_transaction, conflict);
        }
    }

    slapt_vector_t_free(conflicts);

    return conflicts_in_transaction;
}

static void add_suggestion(slapt_transaction_t *tran, slapt_pkg_t *pkg)
{
    slapt_vector_t *suggests = NULL;

    if (pkg->suggests == NULL || strlen(pkg->suggests) == 0) {
        return;
    }

    suggests = slapt_parse_delimited_list(pkg->suggests, ',');

    slapt_vector_t_foreach (char *, s, suggests) {
        /* no need to add it if we already have it */
        if (slapt_search_transaction(tran, s))
            continue;

        slapt_vector_t_add(tran->suggests, strdup(s));
    }

    slapt_vector_t_free(suggests);
}

bool slapt_search_transaction_by_pkg(const slapt_transaction_t *tran, const slapt_pkg_t *pkg)
{
    slapt_vector_t_foreach (slapt_pkg_t *, install_pkg, tran->install_pkgs) {
        if ((strcmp(pkg->name, install_pkg->name) == 0) && (strcmp(pkg->version, install_pkg->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, upgrade, tran->upgrade_pkgs) {
        slapt_pkg_t *p = upgrade->upgrade;
        if ((strcmp(pkg->name, p->name) == 0) && (strcmp(pkg->version, p->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_upgrade_t *, reinstall, tran->reinstall_pkgs) {
        slapt_pkg_t *p = reinstall->upgrade;
        if ((strcmp(pkg->name, p->name) == 0) && (strcmp(pkg->version, p->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_t *, remove_pkg, tran->remove_pkgs) {
        if ((strcmp(pkg->name, remove_pkg->name) == 0) && (strcmp(pkg->version, remove_pkg->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (slapt_pkg_t *, exclude_pkg, tran->exclude_pkgs) {
        if ((strcmp(pkg->name, exclude_pkg->name) == 0) && (strcmp(pkg->version, exclude_pkg->version) == 0))
            return true;
    }

    return false;
}

void slapt_generate_suggestions(slapt_transaction_t *tran)
{
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, tran->install_pkgs) {
        add_suggestion(tran, pkg);
    }
}
