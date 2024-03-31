/*
 * Copyright (C) 2003-2024 Jason Woodward <woodwardj at jaos dot org>
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
//extern char *optarg;
//extern int optind, opterr, optopt;

static void usage(void);
static void version_info(void);
static void slapt_pkg_action_install(const slapt_config_t *global_config, const slapt_vector_t *action_args);
static void slapt_pkg_action_list(const int show);
static void slapt_pkg_action_remove(const slapt_config_t *global_config, const slapt_vector_t *action_args);
static void slapt_pkg_action_search(const char *pattern);
static void slapt_pkg_action_show(const char *pkg_name);
static void slapt_pkg_action_upgrade_all(const slapt_config_t *global_config);
#ifdef SLAPT_HAS_GPGME
static void slapt_pkg_action_add_keys(const slapt_config_t *global_config);
#endif
static void slapt_pkg_action_filelist(const char *pkg_name);
static int cmp_pkg_arch(const char *a, const char *b);

int main(const int argc, char *argv[])
{
    slapt_config_t *global_config, *initial_config; /* our config struct */

    static struct option long_options[] = {
        {"update", 0, 0, SLAPT_UPDATE_OPT},
        {"u", 0, 0, SLAPT_UPDATE_OPT},
        {"upgrade", 0, 0, SLAPT_UPGRADE_OPT},
        {"install", 0, 0, SLAPT_INSTALL_OPT},
        {"i", 0, 0, SLAPT_INSTALL_OPT},
        {"remove", 0, 0, SLAPT_REMOVE_OPT},
        {"show", 0, 0, SLAPT_SHOW_OPT},
        {"search", 0, 0, SLAPT_SEARCH_OPT},
        {"list", 0, 0, SLAPT_LIST_OPT},
        {"installed", 0, 0, SLAPT_INSTALLED_OPT},
        {"clean", 0, 0, SLAPT_CLEAN_OPT},
        {"download-only", 0, 0, SLAPT_DOWNLOAD_ONLY_OPT},
        {"d", 0, 0, SLAPT_DOWNLOAD_ONLY_OPT},
        {"simulate", 0, 0, SLAPT_SIMULATE_OPT},
        {"s", 0, 0, SLAPT_SIMULATE_OPT},
        {"version", 0, 0, SLAPT_VERSION_OPT},
        {"no-prompt", 0, 0, SLAPT_NO_PROMPT_OPT},
        {"y", 0, 0, SLAPT_NO_PROMPT_OPT},
        {"prompt", 0, 0, SLAPT_PROMPT_OPT},
        {"p", 0, 0, SLAPT_PROMPT_OPT},
        {"reinstall", 0, 0, SLAPT_REINSTALL_OPT},
        {"ignore-excludes", 0, 0, SLAPT_IGNORE_EXCLUDES_OPT},
        {"no-md5", 0, 0, SLAPT_NO_MD5_OPT},
        {"dist-upgrade", 0, 0, SLAPT_DIST_UPGRADE_OPT},
        {"help", 0, 0, SLAPT_HELP_OPT},
        {"h", 0, 0, SLAPT_HELP_OPT},
        {"ignore-dep", 0, 0, SLAPT_IGNORE_DEP_OPT},
        {"no-dep", 0, 0, SLAPT_NO_DEP_OPT},
        {"print-uris", 0, 0, SLAPT_PRINT_URIS_OPT},
        {"show-stats", 0, 0, SLAPT_SHOW_STATS_OPT},
        {"S", 0, 0, SLAPT_SHOW_STATS_OPT},
        {"config", 1, 0, SLAPT_CONFIG_OPT},
        {"c", 1, 0, SLAPT_CONFIG_OPT},
        {"autoclean", 0, 0, SLAPT_AUTOCLEAN_OPT},
        {"remove-obsolete", 0, 0, SLAPT_OBSOLETE_OPT},
        {"available", 0, 0, SLAPT_AVAILABLE_OPT},
        {"retry", 1, 0, SLAPT_RETRY_OPT},
        {"no-upgrade", 0, 0, SLAPT_NO_UPGRADE_OPT},
        {"install-set", 0, 0, SLAPT_INSTALL_DISK_SET_OPT},
#ifdef SLAPT_HAS_GPGME
        {"add-keys", 0, 0, SLAPT_ADD_KEYS_OPT},
        {"allow-unauthenticated", 0, 0, SLAPT_ALLOW_UNAUTH},
#endif
        {"filelist", 0, 0, SLAPT_FILELIST},
        {0, 0, 0, 0},
    };
    char *custom_rc_location = NULL;

    setbuf(stdout, NULL);

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    textdomain(GETTEXT_PACKAGE);
#endif

#ifdef SLAPT_HAS_GPGME
    gpgme_check_version(NULL);
#ifdef ENABLE_NLS
    gpgme_set_locale(NULL, LC_CTYPE, setlocale(LC_CTYPE, NULL));
#endif
#endif

    if (argc < 2) {
        usage();
        exit(EXIT_FAILURE);
    }

    initial_config = slapt_config_t_init();
    if (initial_config == NULL) {
        exit(EXIT_FAILURE);
    }

    curl_global_init(CURL_GLOBAL_ALL);

    enum slapt_action do_action = SLAPT_ACTION_USAGE;
    int c = 0;
    int option_index = 0;
    while ((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != -1) {
        switch (c) {
        case SLAPT_UPDATE_OPT: /* update */
            do_action = SLAPT_ACTION_UPDATE;
            break;
        case SLAPT_INSTALL_OPT: /* install */
            do_action = SLAPT_ACTION_INSTALL;
            break;
        case SLAPT_REMOVE_OPT: /* remove */
            do_action = SLAPT_ACTION_REMOVE;
            break;
        case SLAPT_SHOW_OPT: /* show */
            do_action = SLAPT_ACTION_SHOW;
            initial_config->simulate = true; /* allow read access */
            break;
        case SLAPT_SEARCH_OPT: /* search */
            do_action = SLAPT_ACTION_SEARCH;
            initial_config->simulate = true; /* allow read access */
            break;
        case SLAPT_LIST_OPT: /* list */
            do_action = SLAPT_ACTION_LIST;
            initial_config->simulate = true; /* allow read access */
            break;
        case SLAPT_INSTALLED_OPT: /* installed */
            do_action = SLAPT_ACTION_INSTALLED;
            initial_config->simulate = true; /* allow read access */
            break;
        case SLAPT_CLEAN_OPT: /* clean */
            do_action = SLAPT_ACTION_CLEAN;
            break;
        case SLAPT_UPGRADE_OPT: /* upgrade */
            do_action = SLAPT_ACTION_UPGRADE;
            break;
        case SLAPT_DOWNLOAD_ONLY_OPT: /* download only flag */
            initial_config->download_only = true;
            break;
        case SLAPT_SIMULATE_OPT: /* simulate */
            initial_config->simulate = true;
            break;
        case SLAPT_VERSION_OPT: /* version */
            version_info();
            slapt_config_t_free(initial_config);
            curl_global_cleanup();
            exit(EXIT_SUCCESS);
        case SLAPT_NO_PROMPT_OPT: /* auto */
            initial_config->no_prompt = true;
            break;
        case SLAPT_PROMPT_OPT: /* always prompt */
            initial_config->prompt = true;
            break;
        case SLAPT_REINSTALL_OPT: /* reinstall */
            initial_config->re_install = true;
            break;
        case SLAPT_IGNORE_EXCLUDES_OPT: /* ignore-excludes */
            initial_config->ignore_excludes = true;
            break;
        case SLAPT_NO_MD5_OPT: /* no-md5 */
            initial_config->no_md5_check = true;
            break;
        case SLAPT_DIST_UPGRADE_OPT: /* dist-upgrade */
            initial_config->dist_upgrade = true;
            do_action = SLAPT_ACTION_UPGRADE;
            break;
        case SLAPT_HELP_OPT: /* help */
            usage();
            slapt_config_t_free(initial_config);
            curl_global_cleanup();
            exit(EXIT_SUCCESS);
        case SLAPT_IGNORE_DEP_OPT: /* ignore-dep */
            initial_config->ignore_dep = true;
            break;
        case SLAPT_NO_DEP_OPT: /* no-dep */
            initial_config->disable_dep_check = true;
            break;
        case SLAPT_PRINT_URIS_OPT: /* print-uris */
            initial_config->print_uris = true;
            break;
        case SLAPT_SHOW_STATS_OPT: /* download-stats */
            initial_config->dl_stats = true;
            break;
        case SLAPT_CONFIG_OPT: /* override rc location */
            custom_rc_location = strdup(optarg);
            break;
        case SLAPT_RETRY_OPT: /* set number of retry attempts */
            initial_config->retry = (strtoul(optarg, NULL, 0) > 0) ? (uint32_t)strtoul(optarg, NULL, 0) : 1;
            break;
        case SLAPT_NO_UPGRADE_OPT: /* do not attempt to upgrade */
            initial_config->no_upgrade = true;
            break;
        case SLAPT_AUTOCLEAN_OPT: /* clean old old package versions */
            do_action = SLAPT_ACTION_AUTOCLEAN;
            break;
        case SLAPT_OBSOLETE_OPT: /* remove obsolete packages */
            initial_config->remove_obsolete = true;
            break;
        case SLAPT_AVAILABLE_OPT: /* show available packages */
            do_action = SLAPT_ACTION_AVAILABLE;
            initial_config->simulate = true; /* allow read access */
            break;
        case SLAPT_INSTALL_DISK_SET_OPT: /* install a disk set */
            do_action = SLAPT_ACTION_INSTALL_DISK_SET;
            break;
#ifdef SLAPT_HAS_GPGME
        case SLAPT_ADD_KEYS_OPT: /* retrieve GPG keys for sources */
            do_action = SLAPT_ACTION_ADD_KEYS;
            break;
        case SLAPT_ALLOW_UNAUTH: /* allow unauthenticated key */
            initial_config->gpgme_allow_unauth = true;
            break;
#endif
        case SLAPT_FILELIST:
            do_action = SLAPT_ACTION_FILELIST;
            initial_config->simulate = true; /* allow read access */
            break;
        default:
            usage();
            slapt_config_t_free(initial_config);
            curl_global_cleanup();
            exit(EXIT_FAILURE);
        }
    }

    /* load up the configuration file */
    if (custom_rc_location == NULL) {
        global_config = slapt_config_t_read(RC_LOCATION);
    } else {
        global_config = slapt_config_t_read(custom_rc_location);
        free(custom_rc_location);
    }

    if (global_config == NULL) {
        exit(EXIT_FAILURE);
    }

    /* preserve existing command line options */
    global_config->disable_dep_check = initial_config->disable_dep_check;
    global_config->dist_upgrade = initial_config->dist_upgrade;
    global_config->dl_stats = initial_config->dl_stats;
    global_config->download_only = initial_config->download_only;
    global_config->ignore_dep = initial_config->ignore_dep;
    global_config->ignore_excludes = initial_config->ignore_excludes;
    global_config->no_md5_check = initial_config->no_md5_check;
    global_config->no_prompt = initial_config->no_prompt;
    global_config->no_upgrade = initial_config->no_upgrade;
    global_config->print_uris = initial_config->print_uris;
    global_config->prompt = initial_config->prompt;
    global_config->re_install = initial_config->re_install;
    global_config->remove_obsolete = initial_config->remove_obsolete;
    global_config->retry = initial_config->retry;
    global_config->simulate = initial_config->simulate;
    global_config->gpgme_allow_unauth = initial_config->gpgme_allow_unauth;

    slapt_config_t_free(initial_config);

    /* Check optional arguments presence */
    switch (do_action) {
    /* can't simulate update, clean, autoclean, or add keys */
    case SLAPT_ACTION_CLEAN:
    case SLAPT_ACTION_AUTOCLEAN:
#ifdef SLAPT_HAS_GPGME
    case SLAPT_ACTION_ADD_KEYS:
#endif
    case SLAPT_ACTION_UPDATE:
        global_config->simulate = false;
        break;

    /* remove obsolete can take the place of arguments */
    case SLAPT_ACTION_INSTALL:
    case SLAPT_ACTION_INSTALL_DISK_SET:
    case SLAPT_ACTION_REMOVE:
        if (global_config->remove_obsolete)
            break;
        /* fall through */

    /* show, search, filelist must have arguments */
    case SLAPT_ACTION_SHOW:
    case SLAPT_ACTION_SEARCH:
    case SLAPT_ACTION_FILELIST:
        if (optind >= argc)
            do_action = SLAPT_ACTION_USAGE;
        break;

    default:
        if (optind < argc)
            do_action = SLAPT_ACTION_USAGE;
        break;
    }

    if (do_action == SLAPT_ACTION_USAGE) {
        usage();
        slapt_config_t_free(global_config);
        curl_global_cleanup();
        exit(EXIT_FAILURE);
    }

    /* create the working directory if needed */
    slapt_vector_t *paa = NULL;
    slapt_working_dir_init(global_config);
    if ((chdir(global_config->working_dir)) == -1) {
        fprintf(stderr, gettext("Failed to chdir: %s\n"), global_config->working_dir);
        exit(EXIT_FAILURE);
    }

    switch (do_action) {
    case SLAPT_ACTION_UPDATE:
        if (slapt_update_pkg_cache(global_config) == 1) {
            slapt_config_t_free(global_config);
            curl_global_cleanup();
            exit(EXIT_FAILURE);
        }
        break;
    case SLAPT_ACTION_INSTALL:
        paa = slapt_vector_t_init(free);
        while (optind < argc) {
            slapt_vector_t_add(paa, strdup(argv[optind]));
            ++optind;
        }
        slapt_pkg_action_install(global_config, paa);
        slapt_vector_t_free(paa);
        break;
    case SLAPT_ACTION_INSTALL_DISK_SET: {
        paa = slapt_vector_t_init(NULL);
        slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();

        while (optind < argc) {
            const size_t search_len = strlen(argv[optind]) + 3;
            char search[search_len];
            const int written = snprintf(search, strlen(argv[optind]) + 3, "/%s$", argv[optind]);
            if (written <= 0 || (size_t)written != (search_len - 1)) {
                fprintf(stderr, "search string construction failed\n");
                exit(EXIT_FAILURE);
            }
            slapt_vector_t *matches = slapt_search_pkg_list(avail_pkgs, search);

            slapt_vector_t_foreach (const slapt_pkg_t *, match, matches) {
                if (!slapt_is_excluded(global_config, match)) {
                    slapt_vector_t_add(paa, match->name);
                }
            }

            slapt_vector_t_free(matches);
            ++optind;
        }

        slapt_pkg_action_install(global_config, paa);
        slapt_vector_t_free(paa);
        slapt_vector_t_free(avail_pkgs);

    } break;
    case SLAPT_ACTION_REMOVE:
        paa = slapt_vector_t_init(free);
        while (optind < argc) {
            slapt_vector_t_add(paa, strdup(argv[optind]));
            ++optind;
        }
        slapt_pkg_action_remove(global_config, paa);
        slapt_vector_t_free(paa);
        break;
    case SLAPT_ACTION_SHOW:
        while (optind < argc) {
            slapt_pkg_action_show(argv[optind++]);
        }
        break;
    case SLAPT_ACTION_SEARCH:
        while (optind < argc) {
            slapt_pkg_action_search(argv[optind++]);
        }
        break;
    case SLAPT_ACTION_UPGRADE:
        slapt_pkg_action_upgrade_all(global_config);
        break;
    case SLAPT_ACTION_LIST:
        slapt_pkg_action_list(SLAPT_ACTION_LIST);
        break;
    case SLAPT_ACTION_INSTALLED:
        slapt_pkg_action_list(SLAPT_ACTION_INSTALLED);
        break;
    case SLAPT_ACTION_CLEAN:
        /* clean out local cache */
        slapt_clean_pkg_dir(global_config->working_dir);
        if ((chdir(global_config->working_dir)) == -1) {
            fprintf(stderr, gettext("Failed to chdir: %s\n"), global_config->working_dir);
            exit(EXIT_FAILURE);
        }
        break;
    case SLAPT_ACTION_AUTOCLEAN:
        slapt_purge_old_cached_pkgs(global_config, NULL, NULL);
        break;
    case SLAPT_ACTION_AVAILABLE:
        slapt_pkg_action_list(SLAPT_ACTION_AVAILABLE);
        break;
#ifdef SLAPT_HAS_GPGME
    case SLAPT_ACTION_ADD_KEYS:
        slapt_pkg_action_add_keys(global_config);
        break;
#endif
    case SLAPT_ACTION_FILELIST:
        while (optind < argc) {
            slapt_pkg_action_filelist(argv[optind++]);
        }
        break;
    case SLAPT_ACTION_USAGE:
    default:
        printf("main.c(l.%d): This should never be reached\n", __LINE__);
        exit(255);
    }

    slapt_config_t_free(global_config);
    curl_global_cleanup();
    return EXIT_SUCCESS;
}

static void usage(void)
{
    printf("%s - Jason Woodward <woodwardj at jaos dot org>\n", PACKAGE);
    printf(gettext("An implementation of the Debian APT system to Slackware\n"));
    printf(gettext("Usage:\n"));
    printf(gettext("%s [option(s)] [target]\n"), PACKAGE);
    printf("\n");
    printf(gettext("Targets:\n"));
    printf("  -u, --update   %s\n", gettext("retrieve pkg data from MIRROR"));
    printf("  --upgrade      %s\n", gettext("upgrade installed pkgs"));
    printf("  --dist-upgrade %s\n", gettext("upgrade to newer release"));
    printf("  -i, --install  %s\n", gettext("[pkg name(s)] - install specified pkg(s)"));
    printf("  --install-set  %s\n", gettext("[disk set(s)] - install specified disk set(s)"));
    printf("  --remove       %s\n", gettext("[pkg name(s)] - remove specified pkg(s)"));
    printf("  --show         %s\n", gettext("[pkg name(s)] - show pkg(s) description"));
    printf("  --filelist     %s\n", gettext("[pkg name(s)] - show pkg(s) installed files"));
    printf("  --search       %s\n", gettext("[expression] - search available pkgs"));
    printf("  --list         %s\n", gettext("list pkgs"));
    printf("  --available    %s\n", gettext("list available pkgs"));
    printf("  --installed    %s\n", gettext("list installed pkgs"));
    printf("  --clean        %s\n", gettext("purge cached pkgs"));
    printf("  --autoclean    %s\n", gettext("only purge cache of older, unreacheable pkgs"));
#ifdef SLAPT_HAS_GPGME
    printf("  --add-keys     %s\n", gettext("retrieve GPG keys for sources"));
#endif
    printf("  -h, --help     %s\n", gettext("display this help and exit"));
    printf("  --version      %s\n", gettext("print version and license info"));
    printf("\n");
    printf(gettext("Options:\n"));
    printf("  -d, --download-only     %s\n", gettext("only download pkg on install/upgrade"));
    printf("  -s, --simulate          %s\n", gettext("show pkgs to be installed/upgraded"));
    printf("  -y, --no-prompt         %s\n", gettext("do not prompt during install/upgrade"));
    printf("  -p, --prompt            %s\n", gettext("always prompt during install/upgrade"));
    printf("  --reinstall             %s\n", gettext("reinstall the pkg"));
    printf("  --ignore-excludes       %s\n", gettext("install/upgrade excludes"));
    printf("  --no-md5                %s\n", gettext("do not perform md5 check sum"));
    printf("  --no-dep                %s\n", gettext("skip dependency check"));
    printf("  --ignore-dep            %s\n", gettext("ignore dependency failures"));
    printf("  --print-uris            %s\n", gettext("print URIs only, do not download"));
    printf("  -S, --show-stats        %s\n", gettext("show download statistics"));
    printf("  -c, --config []         %s\n", gettext("specify alternate slapt-getrc location"));
    printf("  --remove-obsolete       %s\n", gettext("remove obsolete packages"));
    printf("  --retry []              %s\n", gettext("specify number of download retry attempts"));
    printf("  --no-upgrade            %s\n", gettext("install package, do not attempt to upgrade"));
#ifdef SLAPT_HAS_GPGME
    printf("  --allow-unauthenticated %s\n", gettext("allow unauthenticated packages"));
#endif
}

static void version_info(void)
{
    printf(gettext("%s version %s\n"), PACKAGE, VERSION);
    printf("\n");
    printf("This program is free software; you can redistribute it and/or modify\n");
    printf("it under the terms of the GNU General Public License as published by\n");
    printf("the Free Software Foundation; either version 2 of the License, or\n");
    printf("any later version.\n");
    printf("This program is distributed in the hope that it will be useful,\n");
    printf("\n");
    printf("but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
    printf("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
    printf("GNU Library General Public License for more details.\n");
    printf("\n");
    printf("You should have received a copy of the GNU General Public License\n");
    printf("along with this program; if not, write to the Free Software\n");
    printf("Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n");
}

/* install pkg */
static void slapt_pkg_action_install(const slapt_config_t *global_config, const slapt_vector_t *action_args)
{
    printf(gettext("Reading Package Lists..."));
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();

    if (avail_pkgs == NULL || !avail_pkgs->size)
        exit(EXIT_FAILURE);

    printf(gettext("Done\n"));

    slapt_transaction_t *trxn = slapt_transaction_t_init();

    slapt_regex_t *pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN);
    if (pkg_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_vector_t_foreach (const char *, arg, action_args) {
        slapt_pkg_t *pkg = NULL;

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

        const slapt_pkg_t *installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg->name);

        /* if it is not already installed, install it */
        if (installed_pkg == NULL) {
            if (slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, pkg) == 0) {
                slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, pkg);

                /* this comes first so we can pick up that we are installed the package when checking conflicts */
                slapt_transaction_t_add_install(trxn, pkg);

                /* if there are conflicts, we schedule the conflicts for removal */
                if (conflicts->size && !global_config->ignore_dep) {
                    slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
                        /* make sure to remove the conflict's dependencies as well */
                        slapt_vector_t *conflict_deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, trxn->install_pkgs, trxn->remove_pkgs, conflict);
                        slapt_vector_t_foreach (slapt_pkg_t *, conflict_dep, conflict_deps) {
                            if (slapt_get_exact_pkg(installed_pkgs, conflict_dep->name, conflict_dep->version) != NULL) {
                                slapt_transaction_t_add_remove(trxn, conflict_dep);
                            }
                        }
                        slapt_vector_t_free(conflict_deps);

                        slapt_transaction_t_add_remove(trxn, conflict);
                    }
                }
                slapt_vector_t_free(conflicts);

            } else {
                printf(gettext("Excluding %s, use --ignore-dep to override\n"), pkg->name);
                slapt_transaction_t_add_exclude(trxn, pkg);
            }

        } else if (!global_config->no_upgrade || global_config->re_install) { /* else we upgrade or reinstall */

            /* it is already installed, attempt an upgrade */
            if (((slapt_pkg_t_cmp(installed_pkg, pkg)) < 0) || (global_config->re_install)) {
                if (slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, pkg) == 0) {
                    slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, pkg);
                    if (conflicts->size && !global_config->ignore_dep) {
                        slapt_vector_t_foreach (slapt_pkg_t *, conflict, conflicts) {
                            /* make sure to remove the conflict's dependencies as well */
                            slapt_vector_t *conflict_deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, trxn->install_pkgs, trxn->remove_pkgs, conflict);
                            slapt_vector_t_foreach (slapt_pkg_t *, conflict_dep, conflict_deps) {
                                if (slapt_get_exact_pkg(installed_pkgs, conflict_dep->name, conflict_dep->version) != NULL) {
                                    slapt_transaction_t_add_remove(trxn, conflict_dep);
                                }
                            }
                            slapt_vector_t_free(conflict_deps);

                            slapt_transaction_t_add_remove(trxn, conflict);
                        }
                    }

                    if (slapt_pkg_t_cmp(installed_pkg, pkg) == 0) {
                        slapt_transaction_t_add_reinstall(trxn, installed_pkg, pkg);
                    } else {
                        slapt_transaction_t_add_upgrade(trxn, installed_pkg, pkg);
                    }

                    slapt_vector_t_free(conflicts);

                } else {
                    printf(gettext("Excluding %s, use --ignore-dep to override\n"), pkg->name);
                    slapt_transaction_t_add_exclude(trxn, pkg);
                }

            } else {
                printf(gettext("%s is up to date.\n"), installed_pkg->name);
            }
        }
    }

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);

    slapt_regex_t_free(pkg_regex);

    if (slapt_transaction_t_run(global_config, trxn) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_transaction_t_free(trxn);
}

/* list pkgs */
static void slapt_pkg_action_list(const int show)
{
    slapt_vector_t *pkgs = slapt_get_available_pkgs();
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();

    if (show == SLAPT_ACTION_LIST || show == SLAPT_ACTION_AVAILABLE) {
        slapt_vector_t_foreach (const slapt_pkg_t *, pkg, pkgs) {
            bool installed = false;
            char *short_description = slapt_pkg_t_short_description(pkg);

            /* is it installed? */
            if (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL) {
                installed = true;
            }

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
        slapt_vector_t_foreach (const slapt_pkg_t *, pkg, installed_pkgs) {
            if (show == SLAPT_ACTION_LIST) {
                if (slapt_get_exact_pkg(pkgs, pkg->name, pkg->version) != NULL) {
                    continue;
                }
            }

            char *short_description = slapt_pkg_t_short_description(pkg);
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
static void slapt_pkg_action_remove(const slapt_config_t *global_config, const slapt_vector_t *action_args)
{
    printf(gettext("Reading Package Lists..."));
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();
    printf(gettext("Done\n"));

    slapt_transaction_t *trxn = slapt_transaction_t_init();
    slapt_regex_t *pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN);
    if (pkg_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_vector_t *scheduled = slapt_vector_t_init(NULL);
    slapt_vector_t_foreach (const char *, arg, action_args) {
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

        slapt_vector_t_add(scheduled, pkg);
        slapt_transaction_t_add_remove(trxn, pkg);
    }
    slapt_vector_t_foreach(const slapt_pkg_t *, to_remove, scheduled) {
        slapt_vector_t *deps = slapt_is_required_by(global_config, avail_pkgs, installed_pkgs, trxn->install_pkgs, trxn->remove_pkgs, to_remove);
        slapt_vector_t_foreach (const slapt_pkg_t *, dep, deps) {
            if (slapt_get_exact_pkg(installed_pkgs, dep->name, dep->version) != NULL) {
                slapt_transaction_t_add_remove(trxn, dep);
            }
        }
        slapt_vector_t_free(deps);
    }
    slapt_vector_t_free(scheduled);

    if (global_config->remove_obsolete) {
        slapt_vector_t *obsolete = slapt_get_obsolete_pkgs(global_config, avail_pkgs, installed_pkgs);

        slapt_vector_t_foreach (const slapt_pkg_t *, pkg, obsolete) {
            if (slapt_is_excluded(global_config, pkg)) {
                slapt_transaction_t_add_exclude(trxn, pkg);
            } else {
                slapt_transaction_t_add_remove(trxn, pkg);
            }
        }

        slapt_vector_t_free(obsolete);
    }

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);
    slapt_regex_t_free(pkg_regex);

    if (slapt_transaction_t_run(global_config, trxn) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_transaction_t_free(trxn);
}

/* search for a pkg (support extended POSIX regex) */
static void slapt_pkg_action_search(const char *pattern)
{
    /* read in pkg data */
    slapt_vector_t *pkgs = slapt_get_available_pkgs();
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();

    slapt_vector_t *matches = slapt_search_pkg_list(pkgs, pattern);
    slapt_vector_t *i_matches = slapt_search_pkg_list(installed_pkgs, pattern);

    slapt_vector_t_foreach (const slapt_pkg_t *, pkg, matches) {
        char *short_description = slapt_pkg_t_short_description(pkg);
        printf("%s-%s [inst=%s]: %s\n",
               pkg->name,
               pkg->version,
               (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL)
                   ? gettext("yes")
                   : gettext("no"),
               short_description);
        free(short_description);
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, installed_pkg, i_matches) {
        char *short_description = NULL;
        if (slapt_get_exact_pkg(matches, installed_pkg->name, installed_pkg->version) != NULL) {
            continue;
        }

        short_description = slapt_pkg_t_short_description(installed_pkg);
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
static void slapt_pkg_action_show(const char *pkg_name)
{
    slapt_pkg_t *pkg = NULL;

    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();

    if (avail_pkgs == NULL || installed_pkgs == NULL)
        exit(EXIT_FAILURE);

    slapt_regex_t *pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN);
    if (pkg_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    /* Use regex to see if they specified a particular version */
    slapt_regex_t_execute(pkg_regex, pkg_name);

    /* If so, parse it out and try to get that version only */
    if (pkg_regex->reg_return == 0) {
        char *p_name = slapt_regex_t_extract_match(pkg_regex, pkg_name, 1);
        char *p_version = slapt_regex_t_extract_match(pkg_regex, pkg_name, 2);

        pkg = slapt_get_exact_pkg(avail_pkgs, p_name, p_version);
        if (pkg == NULL) {
            pkg = slapt_get_exact_pkg(installed_pkgs, p_name, p_version);
        }

        free(p_name);
        free(p_version);
    }

    if (pkg == NULL) {
        slapt_pkg_t *installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg_name);
        pkg = slapt_get_newest_pkg(avail_pkgs, pkg_name);
        if (pkg == NULL)
            pkg = installed_pkg;
        else if (installed_pkg != NULL) {
            if (slapt_pkg_t_cmp(installed_pkg, pkg) > 0)
                pkg = installed_pkg;
        }
    }

    if (pkg != NULL) {
        char *changelog = slapt_pkg_t_changelog(pkg);
        char *description = slapt_pkg_t_clean_description(pkg);
        bool installed = false;

        if (slapt_get_exact_pkg(installed_pkgs, pkg->name, pkg->version) != NULL) {
            installed = true;
        }

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

struct dist_upgrade_pkg {
    char *name;
    char *replaces;
    bool upgrade_only;
};

static struct dist_upgrade_pkg const dist_upgrade_pkgs[] = {
    {.name = (char *)"aaa_base",         .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"pkgtools",         .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"aaa_glibc-solibs", .upgrade_only = false, .replaces = (char *)"glibc-solibs"},
    {.name = (char *)"glibc-solibs",     .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"aaa_libraries",    .upgrade_only = false, .replaces = (char *)"aaa_elflibs"},
    {.name = (char *)"aaa_elflibs",      .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"glibc",            .upgrade_only = true,  .replaces = NULL},
    {.name = (char *)"xz",               .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"sed",              .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"tar",              .upgrade_only = false, .replaces = NULL},
    {.name = (char *)"gzip",             .upgrade_only = false, .replaces = NULL},
    {NULL, NULL, NULL}
};

/* upgrade all installed pkgs with available updates */
static void slapt_pkg_action_upgrade_all(const slapt_config_t *global_config)
{

    printf(gettext("Reading Package Lists..."));
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    slapt_vector_t *avail_pkgs = slapt_get_available_pkgs();

    if (avail_pkgs == NULL || installed_pkgs == NULL) {
        exit(EXIT_FAILURE);
    }
    if (!avail_pkgs->size) {
        exit(EXIT_FAILURE);
    }

    printf(gettext("Done\n"));
    slapt_transaction_t *trxn = slapt_transaction_t_init();

    if (global_config->dist_upgrade) {
        slapt_pkg_t *newest_slaptget = NULL;
        slapt_vector_t *matches = slapt_search_pkg_list(avail_pkgs, SLAPT_SLACK_BASE_SET_REGEX);

        /* make sure the essential packages are installed/upgraded first */
        for (uint32_t idx = 0; dist_upgrade_pkgs[idx].name != NULL; ++idx) {
            struct dist_upgrade_pkg essential = dist_upgrade_pkgs[idx];

            const slapt_pkg_t *inst_pkg = slapt_get_newest_pkg(installed_pkgs, essential.name);
            const slapt_pkg_t *avail_pkg = slapt_get_newest_pkg(avail_pkgs, essential.name);

            /* can we upgrade */
            if (inst_pkg != NULL && avail_pkg != NULL) {
                if (slapt_pkg_t_cmp(inst_pkg, avail_pkg) < 0) {
                    slapt_transaction_t_add_upgrade(trxn, inst_pkg, avail_pkg);
                }
            } else if (avail_pkg != NULL && essential.upgrade_only == false) {
                slapt_transaction_t_add_install(trxn, avail_pkg);
            }

            if (avail_pkg != NULL && essential.replaces) {
                const slapt_pkg_t *installed_obsolete = slapt_get_newest_pkg(installed_pkgs, essential.replaces);
                if (installed_obsolete) {
                    slapt_transaction_t_add_remove(trxn, installed_obsolete);
                }
            }
        }

        /* loop through SLAPT_SLACK_BASE_SET_REGEX packages */
        slapt_vector_t_foreach (slapt_pkg_t *, pkg, matches) {
            slapt_pkg_t *upgrade_pkg = NULL;

            const slapt_pkg_t *installed_pkg = slapt_get_newest_pkg(installed_pkgs, pkg->name);
            slapt_pkg_t *newer_avail_pkg = slapt_get_newest_pkg(avail_pkgs, pkg->name);
            /* if there is a newer available version (such as from patches/) use it instead */
            if (slapt_pkg_t_cmp(pkg, newer_avail_pkg) < 0) {
                upgrade_pkg = newer_avail_pkg;
            } else {
                upgrade_pkg = pkg;
            }

            /* add to install list if not already installed */
            if (installed_pkg == NULL) {
                if (slapt_is_excluded(global_config, upgrade_pkg)) {
                    slapt_transaction_t_add_exclude(trxn, upgrade_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, upgrade_pkg);

                    /* add install if all deps are good and it doesn't have conflicts */
                    const int rc = slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, upgrade_pkg);
                    if (!rc && !conflicts->size && !global_config->ignore_dep) {
                        slapt_transaction_t_add_install(trxn, upgrade_pkg);
                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), upgrade_pkg->name);
                        slapt_transaction_t_add_exclude(trxn, upgrade_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
                /* even if it's installed, check to see that the packages are different */
                /* simply running a version comparison won't do it since sometimes the */
                /* arch is the only thing that changes */
            } else if ((slapt_pkg_t_cmp(installed_pkg, upgrade_pkg) <= 0) && strcmp(installed_pkg->version, upgrade_pkg->version) != 0) {
                if (slapt_is_excluded(global_config, installed_pkg) || slapt_is_excluded(global_config, upgrade_pkg)) {
                    slapt_transaction_t_add_exclude(trxn, upgrade_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, upgrade_pkg);

                    /* if all deps are added and there is no conflicts, add on */
                    if ((slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, upgrade_pkg) == 0) && (!conflicts->size && !global_config->ignore_dep)) {
                        slapt_transaction_t_add_upgrade(trxn, installed_pkg, upgrade_pkg);
                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), upgrade_pkg->name);
                        slapt_transaction_t_add_exclude(trxn, upgrade_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
            }
        }

        slapt_vector_t_free(matches);

        /* remove obsolete packages if prompted to */
        if (global_config->remove_obsolete) {
            slapt_vector_t *obsolete = slapt_get_obsolete_pkgs(global_config, avail_pkgs, installed_pkgs);

            slapt_vector_t_foreach (const slapt_pkg_t *, obsolete_pkg, obsolete) {
                if (slapt_is_excluded(global_config, obsolete_pkg)) {
                    slapt_transaction_t_add_exclude(trxn, obsolete_pkg);
                } else {
                    slapt_transaction_t_add_remove(trxn, obsolete_pkg);
                }
            }

            slapt_vector_t_free(obsolete);

        } /* end if remove_obsolete */

        /* insurance so that all of slapt-get's requirements are also installed */
        newest_slaptget = slapt_get_newest_pkg(avail_pkgs, "slapt-get");
        if (newest_slaptget != NULL) {
            const slapt_pkg_t *installed_slaptget = slapt_get_newest_pkg(installed_pkgs, "slapt-get");
            slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, newest_slaptget);
            if (installed_slaptget != NULL) /* should never be null */
                slapt_transaction_t_add_reinstall(trxn, installed_slaptget, newest_slaptget);
        }
    }

    slapt_vector_t_foreach (const slapt_pkg_t *, installed_pkg, installed_pkgs) {
        slapt_pkg_t *update_pkg = NULL;
        slapt_pkg_t *newer_installed_pkg = NULL;

        /* we need to see if there is another installed package that is newer than this one */
        if ((newer_installed_pkg = slapt_get_newest_pkg(installed_pkgs, installed_pkg->name)) != NULL) {
            if (slapt_pkg_t_cmp(installed_pkg, newer_installed_pkg) < 0) {
                continue;
            }
        }

        /* see if we have an available update for the pkg */
        update_pkg = slapt_get_newest_pkg(avail_pkgs, installed_pkg->name);
        if (update_pkg != NULL) {
            /* if the update has a newer version, attempt to upgrade */
            const int cmp_r = slapt_pkg_t_cmp(installed_pkg, update_pkg);
            if (
                /* either it's greater, or we want to reinstall */
                cmp_r < 0 || (global_config->re_install) ||
                /* or this is a dist upgrade and the versions are the save except for the arch */
                (global_config->dist_upgrade && cmp_r == 0 && cmp_pkg_arch(installed_pkg->version, update_pkg->version) != 0)) {
                if ((slapt_is_excluded(global_config, update_pkg)) || (slapt_is_excluded(global_config, installed_pkg))) {
                    slapt_transaction_t_add_exclude(trxn, update_pkg);
                } else {
                    slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(trxn, avail_pkgs, installed_pkgs, update_pkg);

                    /* if all deps are added and there is no conflicts, add on */
                    if ((slapt_transaction_t_add_dependencies(global_config, trxn, avail_pkgs, installed_pkgs, update_pkg) == 0) && (global_config->ignore_dep || !conflicts->size)) {
                        if (cmp_r == 0)
                            slapt_transaction_t_add_reinstall(trxn, installed_pkg, update_pkg);
                        else
                            slapt_transaction_t_add_upgrade(trxn, installed_pkg, update_pkg);

                    } else {
                        /* otherwise exclude */
                        printf(gettext("Excluding %s, use --ignore-dep to override\n"), update_pkg->name);
                        slapt_transaction_t_add_exclude(trxn, update_pkg);
                    }

                    slapt_vector_t_free(conflicts);
                }
            }

        } /* end upgrade pkg found */

    } /* end for */

    slapt_vector_t_free(installed_pkgs);
    slapt_vector_t_free(avail_pkgs);

    if (slapt_transaction_t_run(global_config, trxn) != 0) {
        exit(EXIT_FAILURE);
    }

    slapt_transaction_t_free(trxn);
}

static int cmp_pkg_arch(const char *a, const char *b)
{
    slapt_regex_t *a_arch_regex = slapt_regex_t_init(SLAPT_PKG_VER);
    if (a_arch_regex == NULL) {
        exit(EXIT_FAILURE);
    }
    slapt_regex_t *b_arch_regex = slapt_regex_t_init(SLAPT_PKG_VER);
    if (b_arch_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    slapt_regex_t_execute(a_arch_regex, a);
    slapt_regex_t_execute(b_arch_regex, b);

    int r = 0;
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
static void slapt_pkg_action_add_keys(const slapt_config_t *global_config)
{
    int rc = 0;
    bool compressed = false;
    slapt_vector_t_foreach (const slapt_source_t *, source, global_config->sources) {
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

static void slapt_pkg_action_filelist(const char *pkg_name)
{
    slapt_vector_t *installed_pkgs = slapt_get_installed_pkgs();
    if (installed_pkgs == NULL)
        exit(EXIT_FAILURE);

    slapt_regex_t *pkg_regex = slapt_regex_t_init(SLAPT_PKG_LOG_PATTERN);
    if (pkg_regex == NULL) {
        exit(EXIT_FAILURE);
    }

    /* Use regex to see if they specified a particular version */
    slapt_regex_t_execute(pkg_regex, pkg_name);

    /* If so, parse it out and try to get that version only */
    slapt_pkg_t *pkg = NULL;
    if (pkg_regex->reg_return == 0) {
        char *p_name = slapt_regex_t_extract_match(pkg_regex, pkg_name, 1);
        char *p_version = slapt_regex_t_extract_match(pkg_regex, pkg_name, 2);

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

    char *filelist = slapt_pkg_t_filelist(pkg);

    printf("%s\n", filelist);

    free(filelist);
    slapt_regex_t_free(pkg_regex);
    slapt_vector_t_free(installed_pkgs);
}
