/*
 * Copyright (C) 2003-2008 Jason Woodward <woodwardj at jaos dot org>
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

int main( int argc, char *argv[] )
{
  slapt_rc_config *global_config; /* our config struct */
  slapt_pkg_action_args_t *paa;

  int c = 0;
  enum slapt_action do_action = 0;
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
    {"dist-upgrade",0, 0, SLAPT_DIST_UPGRADE_OPT},
    {"help",0, 0, SLAPT_HELP_OPT},
    {"h",0, 0, SLAPT_HELP_OPT},
    {"ignore-dep",0, 0, SLAPT_IGNORE_DEP_OPT},
    {"no-dep",0, 0, SLAPT_NO_DEP_OPT},
    {"print-uris",0, 0, SLAPT_PRINT_URIS_OPT},
    {"show-stats",0, 0, SLAPT_SHOW_STATS_OPT},
    {"S",0, 0, SLAPT_SHOW_STATS_OPT},
    {"config",1, 0, SLAPT_CONFIG_OPT},
    {"c",1, 0, SLAPT_CONFIG_OPT},
    {"autoclean", 0, 0, SLAPT_AUTOCLEAN_OPT},
    {"remove-obsolete", 0, 0, SLAPT_OBSOLETE_OPT},
    {"available", 0, 0, SLAPT_AVAILABLE_OPT},
    {"retry", 1, 0, SLAPT_RETRY_OPT},
    {"no-upgrade", 0, 0, SLAPT_NO_UPGRADE_OPT},
    {"install-set", 0, 0, SLAPT_INSTALL_DISK_SET_OPT},
    #ifdef SLAPT_HAS_GPGME
    {"add-keys", 0, 0, SLAPT_ADD_KEYS_OPT},
    #endif
    {0, 0, 0, 0},
  };

  setbuf(stdout,NULL);

  #ifdef ENABLE_NLS
  setlocale(LC_ALL,"");
  bindtextdomain(GETTEXT_PACKAGE,PACKAGE_LOCALE_DIR);
  textdomain(GETTEXT_PACKAGE);
  #endif

  if ( argc < 2 ) {
    usage();
    exit(EXIT_FAILURE);
  }

  /* load up the configuration file */
  global_config = slapt_read_rc_config(RC_LOCATION);
  if ( global_config == NULL ) {
    exit(EXIT_FAILURE);
  }
  curl_global_init(CURL_GLOBAL_ALL);

  while ((c = getopt_long_only(argc,argv,"",long_options,
                               &option_index)) != -1) {
    switch(c) {
      case SLAPT_UPDATE_OPT: /* update */
        do_action = UPDATE;
        break;
      case SLAPT_INSTALL_OPT: /* install */
        do_action = INSTALL;
        break;
      case SLAPT_REMOVE_OPT: /* remove */
        do_action = REMOVE;
        break;
      case SLAPT_SHOW_OPT: /* show */
        do_action = SHOW;
        global_config->simulate = SLAPT_TRUE; /* allow read access */
        break;
      case SLAPT_SEARCH_OPT: /* search */
        do_action = SEARCH;
        global_config->simulate = SLAPT_TRUE; /* allow read access */
        break;
      case SLAPT_LIST_OPT: /* list */
        do_action = LIST;
        global_config->simulate = SLAPT_TRUE; /* allow read access */
        break;
      case SLAPT_INSTALLED_OPT: /* installed */
        do_action = INSTALLED;
        global_config->simulate = SLAPT_TRUE; /* allow read access */
        break;
      case SLAPT_CLEAN_OPT: /* clean */
        do_action = CLEAN;
        break;
      case SLAPT_UPGRADE_OPT: /* upgrade */
        do_action = UPGRADE;
        break;
      case SLAPT_DOWNLOAD_ONLY_OPT: /* download only flag */
        global_config->download_only = SLAPT_TRUE;
        break;
      case SLAPT_SIMULATE_OPT: /* simulate */
        global_config->simulate = SLAPT_TRUE;
        break;
      case SLAPT_VERSION_OPT: /* version */
        do_action = SHOWVERSION;
        break;
      case SLAPT_NO_PROMPT_OPT: /* auto */
        global_config->no_prompt = SLAPT_TRUE;
        break;
      case SLAPT_PROMPT_OPT: /* always prompt */
        global_config->prompt = SLAPT_TRUE;
        break;
      case SLAPT_REINSTALL_OPT: /* reinstall */
        global_config->re_install = SLAPT_TRUE;
        break;
      case SLAPT_IGNORE_EXCLUDES_OPT: /* ignore-excludes */
        global_config->ignore_excludes = SLAPT_TRUE;
        break;
      case SLAPT_NO_MD5_OPT: /* no-md5 */
        global_config->no_md5_check = SLAPT_TRUE;
        break;
      case SLAPT_DIST_UPGRADE_OPT: /* dist-upgrade */
        global_config->dist_upgrade = SLAPT_TRUE;
        do_action = UPGRADE;
        break;
      case SLAPT_HELP_OPT: /* help */
        usage();
        slapt_free_rc_config(global_config);
        curl_global_cleanup();
        exit(EXIT_FAILURE);
      case SLAPT_IGNORE_DEP_OPT: /* ignore-dep */
        global_config->ignore_dep = SLAPT_TRUE;
        break;
      case SLAPT_NO_DEP_OPT: /* no-dep */
        global_config->disable_dep_check = SLAPT_TRUE;
        break;
      case SLAPT_PRINT_URIS_OPT: /* print-uris */
        global_config->print_uris = SLAPT_TRUE;
        break;
      case SLAPT_SHOW_STATS_OPT: /* download-stats */
        global_config->dl_stats = SLAPT_TRUE;
        break;
      case SLAPT_CONFIG_OPT: /* override rc location */
        {
          slapt_rc_config *tmp_gc = global_config;
          global_config = slapt_read_rc_config(optarg);
          if ( global_config == NULL ) {
            slapt_free_rc_config(tmp_gc);
            curl_global_cleanup();
            exit(EXIT_FAILURE);
          }
          /* preserve existing command line options */
          global_config->download_only = tmp_gc->download_only;
          global_config->dist_upgrade = tmp_gc->dist_upgrade;
          global_config->simulate = tmp_gc->simulate;
          global_config->no_prompt = tmp_gc->no_prompt;
          global_config->prompt = tmp_gc->prompt;
          global_config->re_install = tmp_gc->re_install;
          global_config->ignore_excludes = tmp_gc->ignore_excludes;
          global_config->no_md5_check = tmp_gc->no_md5_check;
          global_config->ignore_dep = tmp_gc->ignore_dep;
          global_config->disable_dep_check = tmp_gc->disable_dep_check;
          global_config->print_uris = tmp_gc->print_uris;
          global_config->dl_stats = tmp_gc->dl_stats;
          slapt_free_rc_config(tmp_gc);
        }
        break;
      case SLAPT_RETRY_OPT: /* set number of retry attempts */
        global_config->retry = (atoi(optarg) > 0) ? atoi(optarg) : 1;
        break;
      case SLAPT_NO_UPGRADE_OPT: /* do not attempt to upgrade */
        global_config->no_upgrade = SLAPT_TRUE;
        break;
      case SLAPT_AUTOCLEAN_OPT: /* clean old old package versions */
        do_action = AUTOCLEAN;
        break;
      case SLAPT_OBSOLETE_OPT: /* remove obsolete packages */
        global_config->remove_obsolete = SLAPT_TRUE;
        break;
      case SLAPT_AVAILABLE_OPT: /* show available packages */
        do_action = AVAILABLE;
        global_config->simulate = SLAPT_TRUE; /* allow read access */
        break;
      case SLAPT_INSTALL_DISK_SET_OPT: /* install a disk set */
        do_action = INSTALL_DISK_SET;
        break;
      #ifdef SLAPT_HAS_GPGME
      case SLAPT_ADD_KEYS_OPT: /* retrieve GPG keys for sources */
        do_action = ADD_KEYS;
        break;
      #endif
      default:
        usage();
        slapt_free_rc_config(global_config);
        curl_global_cleanup();
        exit(EXIT_FAILURE);
    }
  }

  /* Check optional arguments presence */
  switch(do_action) {

    /* can't simulate update, clean, autoclean, or add keys */
    case CLEAN:  
    case AUTOCLEAN:  
    #ifdef SLAPT_HAS_GPGME
    case ADD_KEYS:  
    #endif
    case UPDATE:  
      global_config->simulate = SLAPT_FALSE;
      break;

    /* remove obsolete can take the place of arguments */
    case INSTALL:
    case INSTALL_DISK_SET:
    case REMOVE:
        if (global_config->remove_obsolete == SLAPT_TRUE)
          break;

    /* show and search must have arguments */
    case SHOW:
    case SEARCH:
      if ( optind >= argc )
        do_action = 0;
      break;

    default:
      if (optind < argc)
        do_action = USAGE;
      break;
  }

  if ( do_action == USAGE ) {
    usage();
    slapt_free_rc_config(global_config);
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }

  if ( do_action == SHOWVERSION ) {
    version_info();
    slapt_free_rc_config(global_config);
    curl_global_cleanup();
    exit(EXIT_SUCCESS);
  }

  /* create the working directory if needed */
  slapt_working_dir_init(global_config);
  if ((chdir(global_config->working_dir)) == -1) { 
    fprintf(stderr,gettext("Failed to chdir: %s\n"),global_config->working_dir);
    exit(EXIT_FAILURE);
  }

  switch(do_action) {
    case UPDATE:
      if ( slapt_update_pkg_cache(global_config) == 1 ) {
        slapt_free_rc_config(global_config);
        curl_global_cleanup();
        exit(EXIT_FAILURE);
      }
      break;
    case INSTALL:
      paa = slapt_init_pkg_action_args((argc - optind));
      while (optind < argc) {
        slapt_add_pkg_action_args(paa,argv[optind]);
        ++optind;
      }
      slapt_pkg_action_install( global_config, paa );
      slapt_free_pkg_action_args(paa);
      break;
    case INSTALL_DISK_SET:
      {
        unsigned int set_i;
        struct slapt_pkg_list *set_pkgs = slapt_init_pkg_list();
        struct slapt_pkg_list *avail_pkgs = slapt_get_available_pkgs();

        while (optind < argc) {
          unsigned int search_i;
          struct slapt_pkg_list *matches = NULL;
          char *search = slapt_malloc(sizeof *search * (strlen(argv[optind]) + 3));

          snprintf(search,strlen(argv[optind]) + 3,"/%s$",argv[optind]);
          matches = slapt_search_pkg_list(avail_pkgs,search);
          free(search);

          for (search_i = 0; search_i < matches->pkg_count; ++search_i) {
            if (slapt_is_excluded(global_config,matches->pkgs[search_i]) == 0) {
              slapt_add_pkg_to_pkg_list(set_pkgs,matches->pkgs[search_i]);
            }
          }

          slapt_free_pkg_list(matches);
          ++optind;

        }

        paa = slapt_init_pkg_action_args(set_pkgs->pkg_count);

        for (set_i = 0; set_i < set_pkgs->pkg_count; ++set_i) {
          slapt_add_pkg_action_args(paa,set_pkgs->pkgs[set_i]->name);
        }

        slapt_free_pkg_list(set_pkgs);
        slapt_free_pkg_list(avail_pkgs);

        slapt_pkg_action_install( global_config, paa );
        slapt_free_pkg_action_args(paa);

      }
      break;
    case REMOVE:
      paa = slapt_init_pkg_action_args((argc - optind));
      while (optind < argc) {
        slapt_add_pkg_action_args(paa,argv[optind]);
        ++optind;
      }
      slapt_pkg_action_remove( global_config, paa );
      slapt_free_pkg_action_args(paa);
      break;
    case SHOW:
      while (optind < argc) {
        slapt_pkg_action_show( argv[optind++] );
      }
      break;
    case SEARCH:
      while (optind < argc) {
        slapt_pkg_action_search( argv[optind++] );
      }
      break;
    case UPGRADE:
      slapt_pkg_action_upgrade_all(global_config);
      break;
    case LIST:
      slapt_pkg_action_list(LIST);
      break;
    case INSTALLED:
      slapt_pkg_action_list(INSTALLED);
      break;
    case CLEAN:
      /* clean out local cache */
      slapt_clean_pkg_dir(global_config->working_dir);
      if ((chdir(global_config->working_dir)) == -1) {
        fprintf(stderr,gettext("Failed to chdir: %s\n"),global_config->working_dir);
        exit(EXIT_FAILURE);
      }
      break;
    case AUTOCLEAN:
      slapt_purge_old_cached_pkgs(global_config, NULL, NULL);
      break;
    case AVAILABLE:
      slapt_pkg_action_list(AVAILABLE);
      break;
    #ifdef SLAPT_HAS_GPGME
    case ADD_KEYS:
      slapt_pkg_action_add_keys(global_config);
      break;
    #endif
    case USAGE:
    default:
      printf("main.c(l.%d): This should never be reached\n", __LINE__);
      exit(255);
  }

  slapt_free_rc_config(global_config);
  curl_global_cleanup();
  return EXIT_SUCCESS;
}

void usage(void)
{
  printf("%s - Jason Woodward <woodwardj at jaos dot org>\n",PACKAGE);
  printf(gettext("An implementation of the Debian APT system to Slackware\n"));
  printf(gettext("Usage:\n"));
  printf(gettext("%s [option(s)] [target]\n"),PACKAGE);
  printf("\n");
  printf(gettext("Targets:\n"));
  printf("  --update|-u    - %s\n",gettext("retrieves pkg data from MIRROR"));
  printf("  --upgrade      - %s\n",gettext("upgrade installed pkgs"));
  printf("  --dist-upgrade - %s\n",gettext("upgrade to newer release"));
  printf("  --install|-i   %s\n",gettext("[pkg name(s)] - install specified pkg(s)"));
  printf("  --install-set  %s\n",gettext("[disk set(s)] - install specified disk set(s)"));
  printf("  --remove       %s\n",gettext("[pkg name(s)] - remove specified pkg(s)"));
  printf("  --show         %s\n",gettext("[pkg name] - show pkg description"));
  printf("  --search       %s\n",gettext("[expression] - search available pkgs"));
  printf("  --list         - %s\n",gettext("list pkgs"));
  printf("  --available    - %s\n",gettext("list available pkgs"));
  printf("  --installed    - %s\n",gettext("list installed pkgs"));
  printf("  --clean        - %s\n",gettext("purge cached pkgs"));
  printf("  --autoclean    - %s\n",gettext("only purge cache of older, unreacheable pkgs"));
  #ifdef SLAPT_HAS_GPGME
  printf("  --add-keys     - %s\n",gettext("retrieve GPG keys for sources"));
  #endif
  printf("  --version      - %s\n",gettext("print version and license info"));
  printf("\n");
  printf(gettext("Options:\n"));
  printf("  --download-only|-d  - %s\n",gettext("only download pkg on install/upgrade"));
  printf("  --simulate|-s       - %s\n",gettext("show pkgs to be installed/upgraded"));
  printf("  --no-prompt|-y      - %s\n",gettext("do not prompt during install/upgrade"));
  printf("  --prompt|-p         - %s\n",gettext("always prompt during install/upgrade"));
  printf("  --reinstall         - %s\n",gettext("re-install the pkg"));
  printf("  --ignore-excludes   - %s\n",gettext("install/upgrade excludes"));
  printf("  --no-md5            - %s\n",gettext("do not perform md5 check sum"));
  printf("  --no-dep            - %s\n",gettext("skip dependency check"));
  printf("  --ignore-dep        - %s\n",gettext("ignore dependency failures"));
  printf("  --print-uris        - %s\n",gettext("print URIs only, do not download"));
  printf("  --show-stats|-S     - %s\n",gettext("show download statistics"));
  printf("  --config|-c []      - %s\n",gettext("specify alternate slapt-getrc location"));
  printf("  --remove-obsolete   - %s\n",gettext("remove obsolete packages"));
  printf("  --retry []          - %s\n",gettext("specify number of download retry attempts"));
  printf("  --no-upgrade        - %s\n",gettext("install package, do not attempt to upgrade"));
}

void version_info(void)
{
  printf(gettext("%s version %s\n"),PACKAGE,VERSION);
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

