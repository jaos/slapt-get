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

static int cmp_pkg_arch(const char *a, const char *b);

/* install pkg */
void slapt_pkg_action_install(const slapt_config_t *global_config, const slapt_vector_t *action_args)
{
    slapt_transaction_t *tran = NULL;
    slapt_vector_t *installed_pkgs = NULL;
    slapt_vector_t *avail_pkgs = NULL;
    slapt_regex_t *pkg_regex = NULL;

    printf(gettext("Reading Package Lists..."));
    installed_pkgs = slapt_get_installed_pkgs();
    avail_pkgs = slapt_get_available_pkgs();

    if (avail_pkgs == NULL || !avail_pkgs->size)
        exit(EXIT_FAILURE);

    printf(gettext("Done\n"));

    tran = slapt_init_transaction();

    if ((pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN)) == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_vector_t_foreach (char *, arg, action_args) {
        slapt_pkg_t *pkg = NULL;
        slapt_pkg_t *installed_pkg = NULL;

        /* Use regex to see if they specified a particular version */
        slapt_regex_t_execute(pkg_regex, arg);

        /* If so, parse it out and try to get that version only */
        if (pkg_regex->reg_return == 0) {
            char *pkg_name, *pkg_version;

            pkg_name = slapt_regex_t_extract_match(pkg_regex, arg, 1);
            pkg_version = slapt_regex_t_extract_match(pkg_regex, arg, 2);
            pkg = slapt_get_exact_pkg(avail_pkgs, pkg_name, pkg_version);

            free(pkg_name);
            free(pkg_version);
        }

        /* If regex doesnt match */
        if (pkg_regex->reg_return != 0 || pkg == NULL) {
            /* make sure there is a package called arg */
            pkg = slapt_get_newest_pkg(avail_pkgs, arg);

            if (pkg == NULL) {
                fprintf(stderr, gettext("No such package: %s\n"), arg);
                exit(EXIT_FAILURE);
            }
        }

        installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg->name);

        /* if it is not already installed, install it */
        if (installed_pkg == NULL || global_config->no_upgrade) {
            if (slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, pkg) == 0) {
                slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, pkg);

                /* this comes first so we can pick up that we are installed the package when checking conflicts */
                slapt_add_install_to_transaction(tran, pkg);

                /* if there are conflicts, we schedule the conflicts for removal */
                if (conflicts->size && !global_config->ignore_dep) {
                    slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
                        /* make sure to remove the conflict's dependencies as well */
                        slapt_vector_t *conflict_deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, tran->install_pkgs, tran->remove_pkgs, conflict);
                        slapt_vector_t_foreach (slapt_pkg_t *, conflict_dep, conflict_deps) {
                            if (slapt_get_exact_pkg(installed_pkgs, conflict_dep->name, conflict_dep->version) != NULL) {
                                slapt_add_remove_to_transaction(tran, conflict_dep);
                            }
                        }
                        slapt_vector_t_free(conflict_deps);

                        slapt_add_remove_to_transaction(tran, conflict);
                    }
                }
                slapt_vector_t_free(conflicts);

            } else {
                printf(gettext("Excluding %s, use --ignore-dep to override\n"), pkg->name);
                slapt_add_exclude_to_transaction(tran, pkg);
            }

        } else { /* else we upgrade or reinstall */

            /* it is already installed, attempt an upgrade */
            if (((slapt_cmp_pkgs(installed_pkg, pkg)) < 0) || (global_config->re_install)) {
                if (slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, pkg) == 0) {
                    slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, pkg);
                    if (conflicts->size && !global_config->ignore_dep) {
                        slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
                            /* make sure to remove the conflict's dependencies as well */
                            slapt_vector_t *conflict_deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, tran->install_pkgs, tran->remove_pkgs, conflict);
                            slapt_vector_t_foreach (slapt_pkg_t *, conflict_dep, conflict_deps) {
                                if (slapt_get_exact_pkg(installed_pkgs, conflict_dep->name, conflict_dep->version) != NULL) {
                                    slapt_add_remove_to_transaction(tran, conflict_dep);
                                }
                            }
                            slapt_vector_t_free(conflict_deps);

                            slapt_add_remove_to_transaction(tran, conflict);
                        }
                    }

                    if (slapt_cmp_pkgs(installed_pkg, pkg) == 0)
                        slapt_add_reinstall_to_transaction(tran, installed_pkg, pkg);
                    else
                        slapt_add_upgrade_to_transaction(tran, installed_pkg, pkg);

                    slapt_vector_t_free(conflicts);

                } else {
                    printf(gettext("Excluding %s, use --ignore-dep to override\n"), pkg->name);
                    slapt_add_exclude_to_transaction(tran, pkg);
                }

            } else {
                printf(gettext("%s is up to date.\n"), installed_pkg->name);
            }
        }
    }

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);

    slapt_regex_t_free(pkg_regex);

    if (slapt_handle_transaction(global_config, tran) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_free_transaction(tran);
    return;
}

/* list pkgs */
void slapt_pkg_action_list(const int show)
{
    slapt_vector_t *pkgs = NULL;
    slapt_vector_t *installed_pkgs = NULL;

    pkgs = slapt_get_available_pkgs();
    installed_pkgs = slapt_get_installed_pkgs();

    if (show == SLAPT_ACTION_LIST || show == SLAPT_ACTION_AVAILABLE) {
        slapt_vector_t_foreach (slapt_pkg_t *, pkg, pkgs) {
            bool installed = false;
            char *short_description = slapt_gen_short_pkg_description(pkg);

            /* is it installed? */
            if (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL)
                installed = true;

            printf("%s-%s [inst=%s]: %s\n",
                   pkg->name,
                   pkg->version,
                   installed
                       ? gettext("yes")
                       : gettext("no"),
                   (short_description == NULL) ? "" : short_description);
            free(short_description);
        }
    }
    if (show == SLAPT_ACTION_LIST || show == SLAPT_ACTION_INSTALLED) {
        slapt_vector_t_foreach (slapt_pkg_t *, pkg, installed_pkgs) {
            char *short_description = NULL;

            if (show == SLAPT_ACTION_LIST) {
                if (slapt_get_exact_pkg(pkgs, pkg->name, pkg->version) != NULL) {
                    continue;
                }
            }

            short_description = slapt_gen_short_pkg_description(pkg);

            printf("%s-%s [inst=%s]: %s\n",
                   pkg->name,
                   pkg->version,
                   gettext("yes"),
                   (short_description == NULL) ? "" : short_description);
            free(short_description);
        }
    }

    slapt_vector_t_free(pkgs);
    slapt_vector_t_free(installed_pkgs);
}

/* remove/uninstall pkg */
void slapt_pkg_action_remove(const slapt_config_t *global_config, const slapt_vector_t *action_args)
{
    slapt_vector_t *installed_pkgs = NULL;
    slapt_vector_t *avail_pkgs = NULL;
    slapt_regex_t *pkg_regex = NULL;
    slapt_transaction_t *tran = NULL;

    printf(gettext("Reading Package Lists..."));
    installed_pkgs = slapt_get_installed_pkgs();
    avail_pkgs = slapt_get_available_pkgs();
    printf(gettext("Done\n"));

    tran = slapt_init_transaction();
    if ((pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN)) == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_vector_t_foreach (char *, arg, action_args) {
        slapt_vector_t *deps = NULL;
        slapt_pkg_t *pkg = NULL;

        /* Use regex to see if they specified a particular version */
        slapt_regex_t_execute(pkg_regex, arg);

        /* If so, parse it out and try to get that version only */
        if (pkg_regex->reg_return == 0) {
            char *pkg_name, *pkg_version;

            pkg_name = slapt_regex_t_extract_match(pkg_regex, arg, 1);
            pkg_version = slapt_regex_t_extract_match(pkg_regex, arg, 2);
            pkg = slapt_get_exact_pkg(installed_pkgs, pkg_name, pkg_version);

            free(pkg_name);
            free(pkg_version);
        }

        /* If regex doesnt match */
        if (pkg_regex->reg_return != 0 || pkg == NULL) {
            /* make sure there is a package called arg */
            pkg = slapt_get_newest_pkg(installed_pkgs, arg);

            if (pkg == NULL) {
                printf(gettext("%s is not installed.\n"), arg);
                continue;
            }
        }

        deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, tran->install_pkgs, tran->remove_pkgs, pkg);

        slapt_vector_t_foreach (slapt_pkg_t *, dep, deps) {
            if (slapt_get_exact_pkg(installed_pkgs, dep->name, dep->version) != NULL) {
                slapt_add_remove_to_transaction(tran, dep);
            }
        }

        slapt_vector_t_free(deps);

        slapt_add_remove_to_transaction(tran, pkg);
    }

    if (global_config->remove_obsolete) {
        slapt_vector_t *obsolete = slapt_get_obsolete_pkgs(global_config, avail_pkgs, installed_pkgs);

        slapt_vector_t_foreach (slapt_pkg_t *, pkg, obsolete) {
            if (slapt_is_excluded(global_config, pkg)) {
                slapt_add_exclude_to_transaction(tran, pkg);
            } else {
                slapt_add_remove_to_transaction(tran, pkg);
            }
        }

        slapt_vector_t_free(obsolete);
    }

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);
    slapt_regex_t_free(pkg_regex);

    if (slapt_handle_transaction(global_config, tran) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_free_transaction(tran);
}

/* search for a pkg (support extended POSIX regex) */
void slapt_pkg_action_search(const char *pattern)
{
    slapt_vector_t *pkgs = NULL;
    slapt_vector_t *installed_pkgs = NULL;
    slapt_vector_t *matches = NULL, *i_matches = NULL;

    /* read in pkg data */
    pkgs = slapt_get_available_pkgs();
    installed_pkgs = slapt_get_installed_pkgs();

    matches = slapt_search_pkg_list(pkgs, pattern);
    i_matches = slapt_search_pkg_list(installed_pkgs, pattern);

    slapt_vector_t_foreach (slapt_pkg_t *, pkg, matches) {
        char *short_description = slapt_gen_short_pkg_description(pkg);

        printf("%s-%s [inst=%s]: %s\n",
               pkg->name,
               pkg->version,
               (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL)
                   ? gettext("yes")
                   : gettext("no"),
               short_description);
        free(short_description);
    }

    slapt_vector_t_foreach (slapt_pkg_t *, installed_pkg, i_matches) {
        char *short_description = NULL;

        if (slapt_get_exact_pkg(matches, installed_pkg->name, installed_pkg->version) != NULL) {
            continue;
        }

        short_description = slapt_gen_short_pkg_description(installed_pkg);

        printf("%s-%s [inst=%s]: %s\n",
               installed_pkg->name,
               installed_pkg->version,
               gettext("yes"),
               short_description);
        free(short_description);
    }

    slapt_vector_t_free(matches);
    slapt_vector_t_free(i_matches);
    slapt_vector_t_free(pkgs);
    slapt_vector_t_free(installed_pkgs);
}

/* show the details for a specific package */
void slapt_pkg_action_show(const char *pkg_name)
{
    slapt_vector_t *avail_pkgs = NULL;
    slapt_vector_t *installed_pkgs = NULL;
    slapt_regex_t *pkg_regex = NULL;
    bool installed = false;
    slapt_pkg_t *pkg = NULL;

    avail_pkgs = slapt_get_available_pkgs();
    installed_pkgs = slapt_get_installed_pkgs();

    if (avail_pkgs == NULL || installed_pkgs == NULL)
        exit(EXIT_FAILURE);

    if ((pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN)) == NULL)
        exit(EXIT_FAILURE);

    /* Use regex to see if they specified a particular version */
    slapt_regex_t_execute(pkg_regex, pkg_name);

    /* If so, parse it out and try to get that version only */
    if (pkg_regex->reg_return == 0) {
        char *p_name, *p_version;

        p_name = slapt_regex_t_extract_match(pkg_regex, pkg_name, 1);
        p_version = slapt_regex_t_extract_match(pkg_regex, pkg_name, 2);

        pkg = slapt_get_exact_pkg(avail_pkgs, p_name, p_version);

        if (pkg == NULL)
            pkg = slapt_get_exact_pkg(installed_pkgs, p_name, p_version);

        free(p_name);
        free(p_version);
    }

    if (pkg == NULL) {
        slapt_pkg_t *installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg_name);
        pkg = slapt_get_newest_pkg(avail_pkgs, pkg_name);
        if (pkg == NULL)
            pkg = installed_pkg;
        else if (pkg != NULL && installed_pkg != NULL) {
            if (slapt_cmp_pkgs(installed_pkg, pkg) > 0)
                pkg = installed_pkg;
        }
    }

    if (pkg != NULL) {
        char *changelog = slapt_get_pkg_changelog(pkg);
        char *description = strdup(pkg->description);
        slapt_clean_description(description, pkg->name);

        if (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL)
            installed = true;

        printf(gettext("Package Name: %s\n"), pkg->name);
        printf(gettext("Package Mirror: %s\n"), pkg->mirror);
        printf(gettext("Package Priority: %s\n"), slapt_priority_to_str(pkg->priority));
        printf(gettext("Package Location: %s\n"), pkg->location);
        printf(gettext("Package Version: %s\n"), pkg->version);
        printf(gettext("Package Size: %d K\n"), pkg->size_c);
        printf(gettext("Package Installed Size: %d K\n"), pkg->size_u);
        printf(gettext("Package Required: %s\n"), pkg->required);
        printf(gettext("Package Conflicts: %s\n"), pkg->conflicts);
        printf(gettext("Package Suggests: %s\n"), pkg->suggests);
        printf(gettext("Package MD5 Sum:  %s\n"), pkg->md5);
        printf(gettext("Package Description:\n"));
        printf("%s", description);

        if (changelog != NULL) {
            printf("%s:\n", gettext("Package ChangeLog"));
            printf("%s\n\n", changelog);
            free(changelog);
        }

        printf(gettext("Package Installed: %s\n"), installed ? gettext("yes") : gettext("no"));

        free(description);
    } else {
        printf(gettext("No such package: %s\n"), pkg_name);
        exit(EXIT_FAILURE);
    }

    slapt_vector_t_free(avail_pkgs);
    slapt_vector_t_free(installed_pkgs);
    slapt_regex_t_free(pkg_regex);
}

/* upgrade all installed pkgs with available updates */
void slapt_pkg_action_upgrade_all(const slapt_config_t *global_config)
{
    slapt_vector_t *installed_pkgs = NULL;
    slapt_vector_t *avail_pkgs = NULL;
    slapt_transaction_t *tran = NULL;

    printf(gettext("Reading Package Lists..."));
    installed_pkgs = slapt_get_installed_pkgs();
    avail_pkgs = slapt_get_available_pkgs();

    if (avail_pkgs == NULL || installed_pkgs == NULL)
        exit(EXIT_FAILURE);

    if (!avail_pkgs->size)
        exit(EXIT_FAILURE);

    printf(gettext("Done\n"));
    tran = slapt_init_transaction();

    if (global_config->dist_upgrade) {
        char *essential[] = {"aaa_base", "pkgtools", "glibc-solibs", "glibc", "aaa_elflibs", "xz", "sed", "tar", "gzip", NULL};
        int epi = 0;
        slapt_pkg_t *newest_slaptget = NULL;
        slapt_vector_t *matches = slapt_search_pkg_list(avail_pkgs, SLAPT_SLACK_BASE_SET_REGEX);

        /* make sure the essential packages are installed/upgraded first */
        while (essential[epi] != NULL) {
            slapt_pkg_t *inst_pkg = NULL;
            slapt_pkg_t *avail_pkg = NULL;

            inst_pkg = slapt_get_newest_pkg(installed_pkgs, essential[epi]);
            avail_pkg = slapt_get_newest_pkg(avail_pkgs, essential[epi]);

            /* can we upgrade */
            if (inst_pkg != NULL && avail_pkg != NULL) {
                if (slapt_cmp_pkgs(inst_pkg, avail_pkg) < 0) {
                    slapt_add_upgrade_to_transaction(tran, inst_pkg, avail_pkg);
                }
                /* if not try to install (unless glibc, then that is upgrade only) */
            } else if (strcmp(essential[epi], "glibc") != 0 && avail_pkg != NULL) {
                slapt_add_install_to_transaction(tran, avail_pkg);
            }

            ++epi;
        }

        /* loop through SLAPT_SLACK_BASE_SET_REGEX packages */
        slapt_vector_t_foreach (slapt_pkg_t *, pkg, matches) {
            slapt_pkg_t *installed_pkg = NULL;
            slapt_pkg_t *newer_avail_pkg = NULL;
            slapt_pkg_t *slapt_upgrade_pkg = NULL;

            installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg->name);
            newer_avail_pkg = slapt_get_newest_pkg(avail_pkgs, pkg->name);
            /* if there is a newer available version (such as from patches/) use it instead */
            if (slapt_cmp_pkgs(pkg, newer_avail_pkg) < 0) {
                slapt_upgrade_pkg = newer_avail_pkg;
            } else {
                slapt_upgrade_pkg = pkg;
            }

            /* add to install list if not already installed */
            if (installed_pkg == NULL) {
                if (slapt_is_excluded(global_config, slapt_upgrade_pkg)) {
                    slapt_add_exclude_to_transaction(tran, slapt_upgrade_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, slapt_upgrade_pkg);

                    /* add install if all deps are good and it doesn't have conflicts */
                    int rc = slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, slapt_upgrade_pkg);
                    if (!rc && !conflicts->size && !global_config->ignore_dep) {
                        slapt_add_install_to_transaction(tran, slapt_upgrade_pkg);
                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), slapt_upgrade_pkg->name);
                        slapt_add_exclude_to_transaction(tran, slapt_upgrade_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
                /* even if it's installed, check to see that the packages are different */
                /* simply running a version comparison won't do it since sometimes the */
                /* arch is the only thing that changes */
            } else if ((slapt_cmp_pkgs(installed_pkg, slapt_upgrade_pkg) <= 0) && strcmp(installed_pkg->version, slapt_upgrade_pkg->version) != 0) {
                if (slapt_is_excluded(global_config, installed_pkg) || slapt_is_excluded(global_config, slapt_upgrade_pkg)) {
                    slapt_add_exclude_to_transaction(tran, slapt_upgrade_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, slapt_upgrade_pkg);

                    /* if all deps are added and there is no conflicts, add on */
                    if ((slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, slapt_upgrade_pkg) == 0) && (!conflicts->size && !global_config->ignore_dep)) {
                        slapt_add_upgrade_to_transaction(tran, installed_pkg, slapt_upgrade_pkg);
                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), slapt_upgrade_pkg->name);
                        slapt_add_exclude_to_transaction(tran, slapt_upgrade_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
            }
        }

        slapt_vector_t_free(matches);

        /* remove obsolete packages if prompted to */
        if (global_config->remove_obsolete) {
            slapt_vector_t *obsolete = slapt_get_obsolete_pkgs(global_config, avail_pkgs, installed_pkgs);

            slapt_vector_t_foreach (slapt_pkg_t *, obsolete_pkg, obsolete) {
                if (slapt_is_excluded(global_config, obsolete_pkg)) {
                    slapt_add_exclude_to_transaction(tran, obsolete_pkg);
                } else {
                    slapt_add_remove_to_transaction(tran, obsolete_pkg);
                }
            }

            slapt_vector_t_free(obsolete);

        } /* end if remove_obsolete */

        /* insurance so that all of slapt-get's requirements are also installed */
        newest_slaptget = slapt_get_newest_pkg(avail_pkgs, "slapt-get");
        if (newest_slaptget != NULL) {
            slapt_pkg_t *installed_slaptget = slapt_get_newest_pkg(installed_pkgs, "slapt-get");
            slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, newest_slaptget);
            if (installed_slaptget != NULL) /* should never be null */
                slapt_add_reinstall_to_transaction(tran, installed_slaptget, newest_slaptget);
        }
    }

    slapt_vector_t_foreach (slapt_pkg_t *, installed_pkg, installed_pkgs) {
        slapt_pkg_t *update_pkg = NULL;
        slapt_pkg_t *newer_installed_pkg = NULL;

        /* we need to see if there is another installed package that is newer than this one */
        if ((newer_installed_pkg = slapt_get_newest_pkg(installed_pkgs, installed_pkg->name)) != NULL) {
            if (slapt_cmp_pkgs(installed_pkg, newer_installed_pkg) < 0) {
                continue;
            }
        }

        /* see if we have an available update for the pkg */
        update_pkg = slapt_get_newest_pkg(avail_pkgs, installed_pkg->name);
        if (update_pkg != NULL) {
            int cmp_r = 0;

            /* if the update has a newer version, attempt to upgrade */
            cmp_r = slapt_cmp_pkgs(installed_pkg, update_pkg);
            if (
                /* either it's greater, or we want to reinstall */
                cmp_r < 0 || (global_config->re_install) ||
                /* or this is a dist upgrade and the versions are the save except for the arch */
                (global_config->dist_upgrade && cmp_r == 0 && cmp_pkg_arch(installed_pkg->version, update_pkg->version) != 0)) {
                if ((slapt_is_excluded(global_config, update_pkg)) || (slapt_is_excluded(global_config, installed_pkg))) {
                    slapt_add_exclude_to_transaction(tran, update_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_is_conflicted(tran, avail_pkgs, installed_pkgs, update_pkg);

                    /* if all deps are added and there is no conflicts, add on */
                    if ((slapt_add_deps_to_trans(global_config, tran, avail_pkgs, installed_pkgs, update_pkg) == 0) && (global_config->ignore_dep || !conflicts->size)) {
                        if (cmp_r == 0)
                            slapt_add_reinstall_to_transaction(tran, installed_pkg, update_pkg);
                        else
                            slapt_add_upgrade_to_transaction(tran, installed_pkg, update_pkg);

                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), update_pkg->name);
                        slapt_add_exclude_to_transaction(tran, update_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
            }

        } /* end upgrade pkg found */

    } /* end for */

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);

    if (slapt_handle_transaction(global_config, tran) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_free_transaction(tran);
}

static int cmp_pkg_arch(const char *a, const char *b)
{
    int r = 0;
    slapt_regex_t *a_arch_regex = NULL, *b_arch_regex = NULL;

    if ((a_arch_regex = slapt_regex_t_init(SLAPT_PKG_VER)) == NULL) {
        exit(EXIT_FAILURE);
    }
    if ((b_arch_regex = slapt_regex_t_init(SLAPT_PKG_VER)) == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_regex_t_execute(a_arch_regex, a);
    slapt_regex_t_execute(b_arch_regex, b);

    if (a_arch_regex->reg_return != 0 || b_arch_regex->reg_return != 0) {
        slapt_regex_t_free(a_arch_regex);
        slapt_regex_t_free(b_arch_regex);

        return strcmp(a, b);

    } else {
        char *a_arch = slapt_regex_t_extract_match(a_arch_regex, a, 2);
        char *b_arch = slapt_regex_t_extract_match(a_arch_regex, b, 2);

        r = strcmp(a_arch, b_arch);

        free(a_arch);
        free(b_arch);
    }

    slapt_regex_t_free(a_arch_regex);
    slapt_regex_t_free(b_arch_regex);

    return r;
}

#ifdef SLAPT_HAS_GPGME
void slapt_pkg_action_add_keys(const slapt_config_t *global_config)
{
    int rc = 0;
    bool compressed = false;
    slapt_vector_t_foreach (slapt_source_t *, source, global_config->sources) {
        FILE *gpg_key = NULL;
        const char *source_url = source->url;
        printf(gettext("Retrieving GPG key [%s]..."), source_url);
        gpg_key = slapt_get_pkg_source_gpg_key(global_config, source_url, &compressed);
        if (gpg_key != NULL) {
            slapt_code_t r = slapt_add_pkg_source_gpg_key(gpg_key);
            if (r == SLAPT_GPG_KEY_UNCHANGED) {
                printf("%s.\n", gettext("GPG key already present"));
            } else if (r == SLAPT_GPG_KEY_IMPORTED) {
                printf("%s.\n", gettext("GPG key successfully imported"));
            } else {
                printf("%s.\n", gettext("GPG key could not be imported"));
                rc = 1;
            }

            fclose(gpg_key);
        }
    }

    if (rc) {
        exit(EXIT_FAILURE);
    }
}
#endif

void slapt_pkg_action_filelist(const char *pkg_name)
{
    slapt_regex_t *pkg_regex = NULL;
    slapt_vector_t *installed_pkgs = NULL;
    slapt_pkg_t *pkg = NULL;
    char *filelist = NULL;

    installed_pkgs = slapt_get_installed_pkgs();
    if (installed_pkgs == NULL)
        exit(EXIT_FAILURE);

    if ((pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN)) == NULL)
        exit(EXIT_FAILURE);

    /* Use regex to see if they specified a particular version */
    slapt_regex_t_execute(pkg_regex, pkg_name);

    /* If so, parse it out and try to get that version only */
    if (pkg_regex->reg_return == 0) {
        char *p_name, *p_version;

        p_name = slapt_regex_t_extract_match(pkg_regex, pkg_name, 1);
        p_version = slapt_regex_t_extract_match(pkg_regex, pkg_name, 2);

        pkg = slapt_get_exact_pkg(installed_pkgs, p_name, p_version);

        if (pkg == NULL)
            exit(EXIT_FAILURE);

        free(p_name);
        free(p_version);

    } else {
        pkg = slapt_get_newest_pkg(installed_pkgs, pkg_name);
        if (pkg == NULL)
            exit(EXIT_FAILURE);
    }

    filelist = slapt_get_pkg_filelist(pkg);

    printf("%s\n", filelist);

    free(filelist);
    slapt_regex_t_free(pkg_regex);
    slapt_vector_t_free(installed_pkgs);
}
