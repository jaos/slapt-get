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

#define SLAPT_PKG_DL_NOTE_LEN 16

static void add_suggestion(slapt_transaction_t *trxn, slapt_pkg_t *pkg);

slapt_transaction_t *slapt_transaction_t_init(void)
{
    slapt_transaction_t *trxn = slapt_malloc(sizeof *trxn);

    trxn->install_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    trxn->remove_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    trxn->exclude_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_t_free);

    trxn->upgrade_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_upgrade_t_free);
    trxn->reinstall_pkgs = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_upgrade_t_free);

    trxn->suggests = slapt_vector_t_init(NULL);

    trxn->queue = slapt_vector_t_init((slapt_vector_t_free_function)slapt_queue_i_free);

    trxn->conflict_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);
    trxn->missing_err = slapt_vector_t_init((slapt_vector_t_free_function)slapt_pkg_err_t_free);

    return trxn;
}

slapt_queue_i *slapt_queue_i_init(slapt_pkg_t *i, slapt_pkg_upgrade_t *u)
{
    slapt_queue_i *qi = slapt_malloc(sizeof *qi);
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

int slapt_transaction_t_run(const slapt_config_t *global_config, slapt_transaction_t *trxn)
{
    double download_size = 0;
    double already_download_size = 0;
    double uncompressed_size = 0;
    char dl_note[SLAPT_PKG_DL_NOTE_LEN];

    /* show unmet dependencies */
    if (trxn->missing_err->size > 0) {
        fprintf(stderr, gettext("The following packages have unmet dependencies:\n"));
        slapt_vector_t_foreach (const slapt_pkg_err_t *, error, trxn->missing_err) {
            fprintf(stderr, gettext("  %s: Depends: %s\n"), error->pkg, error->error);
        }
    }

    /* show conflicts */
    if (trxn->conflict_err->size > 0) {
        slapt_vector_t_foreach (const slapt_pkg_err_t *, conflict_error, trxn->conflict_err) {
            fprintf(stderr, gettext("%s, which is required by %s, is excluded\n"), conflict_error->error, conflict_error->pkg);
        }
    }

    /* show pkgs to exclude */
    if (trxn->exclude_pkgs->size > 0) {
        printf(gettext("The following packages have been EXCLUDED:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const slapt_pkg_t *, e, trxn->exclude_pkgs) {
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
    slapt_transaction_t_suggestions(trxn);
    if (trxn->suggests->size > 0) {
        printf(gettext("Suggested packages:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const char *, s, trxn->suggests) {
            /* don't show suggestion for something we already have in the transaction */
            if (slapt_transaction_t_search(trxn, s))
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
    if (trxn->install_pkgs->size > 0) {
        printf(gettext("The following NEW packages will be installed:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const slapt_pkg_t *, p, trxn->install_pkgs) {
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
    if (trxn->remove_pkgs->size > 0) {
        printf(gettext("The following packages will be REMOVED:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const slapt_pkg_t *, r, trxn->remove_pkgs) {
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
    if (trxn->upgrade_pkgs->size > 0) {
        printf(gettext("The following packages will be upgraded:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
            slapt_pkg_t *u = upgrade->upgrade;
            slapt_pkg_t *p = upgrade->installed;

            const int line_len = len + strlen(u->name) + 1;
            const size_t existing_file_size = slapt_get_pkg_file_size(global_config, u) / 1024;
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

    if (trxn->reinstall_pkgs->size > 0) {
        printf(gettext("The following packages will be reinstalled:\n"));
        printf("  ");

        uint32_t len = 0;
        slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, reinstall_upgrade, trxn->reinstall_pkgs) {
            slapt_pkg_t *u = reinstall_upgrade->upgrade;
            slapt_pkg_t *p = reinstall_upgrade->installed;

            const int line_len = len + strlen(u->name) + 1;
            const size_t existing_file_size = slapt_get_pkg_file_size(global_config, u) / 1024;
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
    printf(ngettext("%d upgraded, ", "%d upgraded, ", trxn->upgrade_pkgs->size), trxn->upgrade_pkgs->size);
    printf(ngettext("%d reinstalled, ", "%d reinstalled, ", trxn->reinstall_pkgs->size), trxn->reinstall_pkgs->size);
    printf(ngettext("%d newly installed, ", "%d newly installed, ", trxn->install_pkgs->size), trxn->install_pkgs->size);
    printf(ngettext("%d to remove, ", "%d to remove, ", trxn->remove_pkgs->size), trxn->remove_pkgs->size);
    printf(ngettext("%d not upgraded.\n", "%d not upgraded.\n", trxn->exclude_pkgs->size), trxn->exclude_pkgs->size);

    /* only show this if we are going to do download something */
    if (trxn->upgrade_pkgs->size > 0 || trxn->install_pkgs->size > 0 || trxn->reinstall_pkgs->size > 0) {
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

    if (trxn->upgrade_pkgs->size > 0 || trxn->remove_pkgs->size > 0 || trxn->install_pkgs->size > 0 || trxn->reinstall_pkgs->size > 0) {
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
        ((trxn->upgrade_pkgs->size > 0 || trxn->remove_pkgs->size > 0 || trxn->reinstall_pkgs->size > 0 ||
          (trxn->install_pkgs->size > 0 &&
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
        slapt_vector_t_foreach (const slapt_pkg_t *, info, trxn->install_pkgs) {
            const char *location = info->location + strspn(info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   info->mirror, location, info->name,
                   info->version, info->file_ext);
        }
        slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
            const slapt_pkg_t *upgrade_info = upgrade->upgrade;
            const char *location = upgrade_info->location + strspn(upgrade_info->location, "./");
            printf("%s%s/%s-%s%s\n",
                   upgrade_info->mirror, location, upgrade_info->name,
                   upgrade_info->version, upgrade_info->file_ext);
        }
        slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, reinstall, trxn->reinstall_pkgs) {
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
        slapt_vector_t_foreach (const slapt_pkg_t *, r, trxn->remove_pkgs) {
            printf(gettext("%s-%s is to be removed\n"), r->name, r->version);
        }

        slapt_vector_t_foreach (const slapt_queue_i *, q, trxn->queue) {
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

    const uint32_t pkg_dl_count = trxn->install_pkgs->size + trxn->upgrade_pkgs->size + trxn->reinstall_pkgs->size;
    uint32_t dl_counter = 0;

    /* download pkgs */
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, trxn->install_pkgs) {
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

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
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

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, reinstall, trxn->reinstall_pkgs) {
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
        slapt_vector_t_foreach (const slapt_pkg_t *, remove_pkg, trxn->remove_pkgs) {
            if (slapt_remove_pkg(global_config, remove_pkg) == -1) {
                exit(EXIT_FAILURE);
            }
        }

        slapt_vector_t_foreach (const slapt_queue_i *, q, trxn->queue) {
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

void slapt_transaction_t_add_install(slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_transaction_t_search_by_pkg(trxn, pkg))
        return;

    slapt_pkg_t *i = slapt_pkg_t_copy(NULL, pkg);
    slapt_vector_t_add(trxn->install_pkgs, i);
    slapt_vector_t_add(trxn->queue, slapt_queue_i_init(trxn->install_pkgs->items[trxn->install_pkgs->size - 1], NULL));
}

void slapt_transaction_t_add_remove(slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_transaction_t_search_by_pkg(trxn, pkg))
        return;

    slapt_pkg_t *r = slapt_pkg_t_copy(NULL, pkg);
    slapt_vector_t_add(trxn->remove_pkgs, r);
}

void slapt_transaction_t_add_exclude(slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_transaction_t_search_by_pkg(trxn, pkg))
        return;

    slapt_pkg_t *e = slapt_pkg_t_copy(NULL, pkg);
    slapt_vector_t_add(trxn->exclude_pkgs, e);
}

void slapt_transaction_t_add_reinstall(slapt_transaction_t *trxn, const slapt_pkg_t *installed_pkg, const slapt_pkg_t *slapt_upgrade_pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_transaction_t_search_by_pkg(trxn, slapt_upgrade_pkg))
        return;

    slapt_pkg_t *i = slapt_pkg_t_copy(NULL, installed_pkg);
    slapt_pkg_t *u = slapt_pkg_t_copy(NULL, slapt_upgrade_pkg);

    slapt_vector_t_add(trxn->reinstall_pkgs, slapt_pkg_upgrade_t_init(i, u));
    slapt_vector_t_add(trxn->queue, slapt_queue_i_init(NULL, trxn->reinstall_pkgs->items[trxn->reinstall_pkgs->size - 1]));
}

void slapt_transaction_t_add_upgrade(slapt_transaction_t *trxn, const slapt_pkg_t *installed_pkg, const slapt_pkg_t *slapt_upgrade_pkg)
{
    /* don't add if already present in the transaction */
    if (slapt_transaction_t_search_by_pkg(trxn, slapt_upgrade_pkg))
        return;

    slapt_pkg_t *i = slapt_pkg_t_copy(NULL, installed_pkg);
    slapt_pkg_t *u = slapt_pkg_t_copy(NULL, slapt_upgrade_pkg);

    slapt_vector_t_add(trxn->upgrade_pkgs, slapt_pkg_upgrade_t_init(i, u));
    slapt_vector_t_add(trxn->queue, slapt_queue_i_init(NULL, trxn->upgrade_pkgs->items[trxn->upgrade_pkgs->size - 1]));
}

bool slapt_transaction_t_search(const slapt_transaction_t *trxn, const char *pkg_name)
{
    slapt_vector_t_foreach (const slapt_pkg_t *, pkg, trxn->install_pkgs) {
        if (strcmp(pkg_name, pkg->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
        if (strcmp(pkg_name, upgrade->upgrade->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, reinstall, trxn->reinstall_pkgs) {
        if (strcmp(pkg_name, reinstall->upgrade->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, remove_pkg, trxn->remove_pkgs) {
        if (strcmp(pkg_name, remove_pkg->name) == 0)
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, exclude_pkg, trxn->exclude_pkgs) {
        if (strcmp(pkg_name, exclude_pkg->name) == 0)
            return true;
    }

    return false;
}

bool slapt_transaction_t_search_upgrade(const slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
        if (strcmp(pkg->name, upgrade->upgrade->name) == 0)
            return true;
    }

    return false;
}

void slapt_transaction_t_free(slapt_transaction_t *trxn)
{
    slapt_vector_t_free(trxn->install_pkgs);
    slapt_vector_t_free(trxn->remove_pkgs);
    slapt_vector_t_free(trxn->upgrade_pkgs);
    slapt_vector_t_free(trxn->reinstall_pkgs);
    slapt_vector_t_free(trxn->exclude_pkgs);
    slapt_vector_t_free(trxn->suggests);
    slapt_vector_t_free(trxn->queue);
    slapt_vector_t_free(trxn->conflict_err);
    slapt_vector_t_free(trxn->missing_err);

    free(trxn);
}

slapt_transaction_t *slapt_transaction_t_remove(slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    slapt_transaction_t *new_trxn = NULL;

    if (!slapt_transaction_t_search_by_pkg(trxn, pkg))
        return trxn;

    /* since this is a pointer, slapt_malloc before calling init */
    new_trxn = slapt_malloc(sizeof *new_trxn);
    new_trxn->install_pkgs = slapt_malloc(sizeof *new_trxn->install_pkgs);
    new_trxn->remove_pkgs = slapt_malloc(sizeof *new_trxn->remove_pkgs);
    new_trxn->upgrade_pkgs = slapt_malloc(sizeof *new_trxn->upgrade_pkgs);
    new_trxn->exclude_pkgs = slapt_malloc(sizeof *new_trxn->exclude_pkgs);
    new_trxn = slapt_transaction_t_init();

    slapt_vector_t_foreach (const slapt_pkg_t *, installed_pkg, trxn->install_pkgs) {
        if (strcmp(pkg->name, installed_pkg->name) == 0 &&
            strcmp(pkg->version, installed_pkg->version) == 0 &&
            strcmp(pkg->location, installed_pkg->location) == 0) {
            continue;
        }

        slapt_transaction_t_add_install(new_trxn, installed_pkg);
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, remove_pkg, trxn->remove_pkgs) {
        if (strcmp(pkg->name, remove_pkg->name) == 0 &&
            strcmp(pkg->version, remove_pkg->version) == 0 &&
            strcmp(pkg->location, remove_pkg->location) == 0) {
            continue;
        }

        slapt_transaction_t_add_remove(new_trxn, remove_pkg);
    }

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
        slapt_pkg_t *u = upgrade->upgrade;
        slapt_pkg_t *p = upgrade->installed;

        if (strcmp(pkg->name, u->name) == 0 &&
            strcmp(pkg->version, u->version) == 0 &&
            strcmp(pkg->location, u->location) == 0) {
            continue;
        }

        slapt_transaction_t_add_upgrade(new_trxn, p, u);
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, exclude_pkg, trxn->exclude_pkgs) {
        if (strcmp(pkg->name, exclude_pkg->name) == 0 &&
            strcmp(pkg->version, exclude_pkg->version) == 0 &&
            strcmp(pkg->location, exclude_pkg->location) == 0) {
            continue;
        }

        slapt_transaction_t_add_exclude(new_trxn, exclude_pkg);
    }

    return new_trxn;
}

/* parse the dependencies for a package, and add them to the transaction as */
/* needed check to see if a package is conflicted */
int slapt_transaction_t_add_dependencies(const slapt_config_t *global_config,
                            slapt_transaction_t *trxn,
                            const slapt_vector_t *avail_pkgs,
                            const slapt_vector_t *installed_pkgs,
                            const slapt_pkg_t *pkg)
{
    if (global_config->disable_dep_check) {
        return 0;
    }

    if (pkg == NULL) {
        return 0;
    }

    slapt_vector_t *deps = slapt_vector_t_init(NULL);

    const int dep_return = slapt_get_pkg_dependencies(global_config, avail_pkgs, installed_pkgs, pkg, deps, trxn->conflict_err, trxn->missing_err);

    /* check to see if there where issues with dep checking */
    /* exclude the package if dep check barfed */
    if ((dep_return == -1) && (global_config->ignore_dep == false) && (slapt_get_exact_pkg(trxn->exclude_pkgs, pkg->name, pkg->version) == NULL)) {
        slapt_vector_t_free(deps);
        return -1;
    }

    /* loop through the deps */
    slapt_vector_t_foreach (const slapt_pkg_t *, dep, deps) {
        slapt_pkg_t *dep_installed = NULL;

        /* the dep wouldn't get this far if it where excluded, so we don't check for that here */
        slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, dep);

        slapt_vector_t_foreach (const slapt_pkg_t *, conflict, conflicts) {
            slapt_transaction_t_add_remove(trxn, conflict);
        }

        slapt_vector_t_free(conflicts);

        dep_installed = slapt_get_newest_pkg(installed_pkgs, dep->name);
        if (dep_installed == NULL) {
            slapt_transaction_t_add_install(trxn, dep);
        } else {
            /* add only if its a valid upgrade */
            if (slapt_pkg_t_cmp(dep_installed, dep) < 0)
                slapt_transaction_t_add_upgrade(trxn, dep_installed, dep);
        }
    }

    slapt_vector_t_free(deps);

    return 0;
}

/* make sure pkg isn't conflicted with what's already in the transaction */
slapt_vector_t *slapt_transaction_t_find_conflicts(const slapt_transaction_t *trxn,
                                    const slapt_vector_t *avail_pkgs,
                                    const slapt_vector_t *installed_pkgs,
                                    const slapt_pkg_t *pkg)
{
    slapt_vector_t *conflicts = NULL;
    slapt_vector_t *conflicts_in_transaction = slapt_vector_t_init(NULL);

    /* if conflicts exist, check to see if they are installed or in the current transaction */
    conflicts = slapt_get_pkg_conflicts(avail_pkgs, installed_pkgs, pkg);
    slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
        if (slapt_transaction_t_search_upgrade(trxn, conflict) == 1 || slapt_get_newest_pkg(trxn->install_pkgs, conflict->name) != NULL) {
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

static void add_suggestion(slapt_transaction_t *trxn, slapt_pkg_t *pkg)
{
    if (pkg->suggests == NULL || strlen(pkg->suggests) == 0) {
        return;
    }

    slapt_vector_t *suggests = slapt_parse_delimited_list(pkg->suggests, ',');
    slapt_vector_t_foreach (const char *, s, suggests) {
        /* no need to add it if we already have it */
        if (slapt_transaction_t_search(trxn, s))
            continue;

        slapt_vector_t_add(trxn->suggests, strdup(s));
    }

    slapt_vector_t_free(suggests);
}

bool slapt_transaction_t_search_by_pkg(const slapt_transaction_t *trxn, const slapt_pkg_t *pkg)
{
    slapt_vector_t_foreach (const slapt_pkg_t *, install_pkg, trxn->install_pkgs) {
        if ((strcmp(pkg->name, install_pkg->name) == 0) && (strcmp(pkg->version, install_pkg->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, upgrade, trxn->upgrade_pkgs) {
        slapt_pkg_t *p = upgrade->upgrade;
        if ((strcmp(pkg->name, p->name) == 0) && (strcmp(pkg->version, p->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_upgrade_t *, reinstall, trxn->reinstall_pkgs) {
        slapt_pkg_t *p = reinstall->upgrade;
        if ((strcmp(pkg->name, p->name) == 0) && (strcmp(pkg->version, p->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, remove_pkg, trxn->remove_pkgs) {
        if ((strcmp(pkg->name, remove_pkg->name) == 0) && (strcmp(pkg->version, remove_pkg->version) == 0))
            return true;
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, exclude_pkg, trxn->exclude_pkgs) {
        if ((strcmp(pkg->name, exclude_pkg->name) == 0) && (strcmp(pkg->version, exclude_pkg->version) == 0))
            return true;
    }

    return false;
}

void slapt_transaction_t_suggestions(slapt_transaction_t *trxn)
{
    slapt_vector_t_foreach (slapt_pkg_t *, pkg, trxn->install_pkgs) {
        add_suggestion(trxn, pkg);
    }
}
