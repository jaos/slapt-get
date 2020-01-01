/*
 * Copyright (C) 2003-2020 Jason Woodward <woodwardj at jaos dot org>
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
static void usage(void);
static void version_info(void);

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
    slapt_config_t *global_config, *initial_config; /* our config struct */
    slapt_vector_t *paa = NULL;

    int c = 0;
    enum slapt_action do_action = SLAPT_ACTION_USAGE;
    int option_index = 0;
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
            initial_config->retry = (atoi(optarg) > 0) ? atoi(optarg) : 1;
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
            char *search = slapt_malloc(sizeof *search * (strlen(argv[optind]) + 3));
            snprintf(search, strlen(argv[optind]) + 3, "/%s$", argv[optind]);
            slapt_vector_t *matches = slapt_search_pkg_list(avail_pkgs, search);
            free(search);

            slapt_vector_t_foreach (slapt_pkg_t *, match, matches) {
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

void usage(void)
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

void version_info(void)
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
