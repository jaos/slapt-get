/*
 * Copyright (C) 2003,2004,2005 Jason Woodward <woodwardj at jaos dot org>
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

int main( int argc, char *argv[] ){
	rc_config *global_config; /* our config struct */
	pkg_action_args_t *paa;
	/* getopt needs these */
	int c = 0;
	extern char *optarg;
	extern int optind, opterr, optopt;
	enum action do_action = 0;
	int option_index = 0;
	static struct option long_options[] = {
		{"update", 0, 0, 'u'},
		{"upgrade", 0, 0, 'g'},
		{"install", 0, 0, 'i'},
		{"remove", 0, 0, 'r'},
		{"show", 0, 0, 's'},
		{"search", 0, 0, 'e'},
		{"list", 0, 0, 't'},
		{"installed", 0, 0, 'd'},
		{"clean", 0, 0, 'c'},
		{"download-only", 0, 0, 'o'},
		{"simulate", 0, 0, 'm'},
		{"s", 0, 0, 'm'},
		{"version", 0, 0, 'v'},
		{"no-prompt", 0, 0, 'b'},
		{"y", 0, 0, 'b'},
		{"reinstall", 0, 0, 'n'},
		{"ignore-excludes", 0, 0, 'x'},
		{"no-md5", 0, 0, '5'},
		{"dist-upgrade",0, 0, 'h'},
		{"help",0, 0, 'l'},
		{"h",0, 0, 'l'},
		{"ignore-dep",0, 0, 'p'},
		{"no-dep",0, 0, 'q'},
		{"print-uris",0, 0, 'P'},
		{"show-stats",0, 0, 'S'},
		{"S",0, 0, 'S'},
		{"config",1, 0, 'C'},
		{"autoclean", 0, 0, 'a'},
	};

	/*
		this causes problems with slackware-current...
		segfaults in getopt_long_only() when invalid
		option is used
	*/
	setbuf(stdout,NULL);

	#ifdef ENABLE_NLS
	setlocale(LC_ALL,"");
	bindtextdomain(PROGRAM_NAME,LOCALESDIR);
	textdomain(PROGRAM_NAME);
	#endif

	if( argc < 2 ) usage(), exit(1);

	/* load up the configuration file */
	global_config = read_rc_config(RC_LOCATION);
	if( global_config == NULL ){
		exit(1);
	}
	curl_global_init(CURL_GLOBAL_ALL);

	while( ( c = getopt_long_only(argc,argv,"",long_options,&option_index ) ) != EOF ){
		switch(c){
			case 'u': /* update */
				do_action = UPDATE;
				break;
			case 'i': /* install */
				do_action = INSTALL;
				break;
			case 'r': /* remove */
				do_action = REMOVE;
				break;
			case 's': /* show */
				do_action = SHOW;
				break;
			case 'e': /* search */
				do_action = SEARCH;
				break;
			case 't': /* list */
				do_action = LIST;
				break;
			case 'd': /* installed */
				do_action = INSTALLED;
				break;
			case 'c': /* clean */
				do_action = CLEAN;
				break;
			case 'g': /* upgrade */
				do_action = UPGRADE;
				break;
			case 'o': /* download only flag */
				global_config->download_only = 1;
				break;
			case 'm': /* simulate */
				global_config->simulate = 1;
				break;
			case 'v': /* version */
				do_action = SHOWVERSION;
				break;
			case 'b': /* auto */
				global_config->no_prompt = 1;
				break;
			case 'n': /* reinstall */
				global_config->re_install = 1;
				break;
			case 'x': /* ignore-excludes */
				global_config->ignore_excludes = 1;
				break;
			case '5': /* no-md5 */
				global_config->no_md5_check = 1;
				break;
			case 'h': /* dist-upgrade */
				global_config->dist_upgrade = 1;
				do_action = UPGRADE;
				break;
			case 'l': /* help */
				usage();
				exit(1);
			case 'p': /* ignore-dep */
				global_config->ignore_dep = 1;
				break;
			case 'q': /* no-dep */
				global_config->disable_dep_check = 1;
				break;
			case 'P': /* print-uris */
				global_config->print_uris = 1;
				break;
			case 'S': /* download-stats */
				global_config->dl_stats = 1;
				break;
			case 'C': /* override rc location */
				{
					rc_config *tmp_gc = global_config;
					global_config = read_rc_config(optarg);
					if( global_config == NULL ){
						exit(1);
					}
					/* preserve existing command line options */
					global_config->download_only = tmp_gc->download_only;
					global_config->dist_upgrade = tmp_gc->dist_upgrade;
					global_config->simulate = tmp_gc->simulate;
					global_config->no_prompt = tmp_gc->no_prompt;
					global_config->re_install = tmp_gc->re_install;
					global_config->ignore_excludes = tmp_gc->ignore_excludes;
					global_config->no_md5_check = tmp_gc->no_md5_check;
					global_config->ignore_dep = tmp_gc->ignore_dep;
					global_config->disable_dep_check = tmp_gc->disable_dep_check;
					global_config->print_uris = tmp_gc->print_uris;
					global_config->dl_stats = tmp_gc->dl_stats;
					free_rc_config(tmp_gc);
				}
				break;
			case 'a': /* clean old old package versions */
				do_action = AUTOCLEAN;
				break;
			default:
				usage();
				exit(1);
		}
	}

	/* Check optionnal arguments presence */
	switch(do_action){
		case INSTALL:
		case REMOVE:
		case SHOW:
		case SEARCH:
			if( optind >= argc )
				do_action = 0;
			break;
		default:
			if (optind < argc)
				do_action = USAGE;
			break;
	}

	if( do_action == USAGE ){
		usage();
		exit(1);
	}

	/* create the working directory if needed */
	working_dir_init(global_config);
	chdir(global_config->working_dir);

	switch(do_action){
		case UPDATE:
			if( update_pkg_cache(global_config) == 1 ) exit(1);
			break;
		case INSTALL:
			paa = init_pkg_action_args((argc - optind));
			while (optind < argc){
				paa->pkgs[paa->count] = malloc(
					( strlen(argv[optind]) + 1 ) * sizeof *paa->pkgs[paa->count]
				);
				memcpy(paa->pkgs[paa->count],argv[optind],strlen(argv[optind]) + 1);
				++optind;
				++paa->count;
			}
			pkg_action_install( global_config, paa );
			free_pkg_action_args(paa);
			break;
		case REMOVE:
			paa = init_pkg_action_args((argc - optind));
			while (optind < argc){
				paa->pkgs[paa->count] = malloc(
					( strlen(argv[optind]) + 1 ) * sizeof *paa->pkgs[paa->count]
				);
				memcpy(paa->pkgs[paa->count],argv[optind],strlen(argv[optind]) + 1);
				++optind;
				++paa->count;
			}
			pkg_action_remove( global_config, paa );
			free_pkg_action_args(paa);
			break;
		case SHOW:
			while (optind < argc){
				pkg_action_show( argv[optind++] );
			}
			break;
		case SEARCH:
			while (optind < argc){
				pkg_action_search( argv[optind++] );
			}
			break;
		case UPGRADE:
			pkg_action_upgrade_all(global_config);
			break;
		case LIST:
			pkg_action_list();
			break;
		case INSTALLED:
			pkg_action_list_installed();
			break;
		case CLEAN:
			/* clean out local cache */
			clean_pkg_dir(global_config->working_dir);
			chdir(global_config->working_dir);
			break;
		case SHOWVERSION:
			version_info();
			break;
		case AUTOCLEAN:
			purge_old_cached_pkgs(global_config, NULL, NULL);
			break;
		case USAGE:
		default:
			printf("main.c(l.%d): This should never be reached\n", __LINE__);
			exit(255);
	}

	free_rc_config(global_config);
	curl_global_cleanup();
	return 0;
}

void usage(void){
	printf("%s - Jason Woodward <woodwardj at jaos dot org>\n",PROGRAM_NAME);
	printf(_("An implementation of the Debian APT system to Slackware\n"));
	printf(_("Usage:\n"));
	printf(_("%s [option(s)] [target]\n"),PROGRAM_NAME);
	printf("\n");
	printf(_("Targets:\n"));
	printf("  --update       - %s\n",_("retrieves pkg data from MIRROR"));
	printf("  --upgrade      - %s\n",_("upgrade installed pkgs"));
	printf("  --dist-upgrade - %s\n",_("upgrade to newer release"));
	printf("  --install      %s\n",_("[pkg name(s)] - install specified pkg(s)"));
	printf("  --remove       %s\n",_("[pkg name(s)] - remove specified pkg(s)"));
	printf("  --show         %s\n",_("[pkg name] - show pkg description"));
	printf("  --search       %s\n",_("[expression] - search available pkgs"));
	printf("  --list         - %s\n",_("list available pkgs"));
	printf("  --installed    - %s\n",_("list installed pkgs"));
	printf("  --clean        - %s\n",_("purge cached pkgs"));
	printf("  --autoclean    - %s\n",_("only purge cache of older, unreacheable pkgs"));
	printf("  --version      - %s\n",_("print version and license info"));
	printf("\n");
	printf(_("Options:\n"));
	printf("  --download-only     - %s\n",_("only download pkg on install/upgrade"));
	printf("  --simulate|-s       - %s\n",_("show pkgs to be installed/upgraded"));
	printf("  --no-prompt|-y      - %s\n",_("do not prompt during install/upgrade"));
	printf("  --reinstall         - %s\n",_("re-install the pkg"));
	printf("  --ignore-excludes   - %s\n",_("install/upgrade excludes"));
	printf("  --no-md5            - %s\n",_("do not perform md5 check sum"));
	printf("  --no-dep            - %s\n",_("skip dependency check"));
	printf("  --ignore-dep        - %s\n",_("ignore dependency failures"));
	printf("  --print-uris        - %s\n",_("print URIs only, do not download"));
	printf("  --show-stats|-S     - %s\n",_("show download statistics"));
	printf("  --config []         - %s\n",_("specify alternate slapt-getrc location"));
}

void version_info(void){
	printf(_("%s version %s\n"),PROGRAM_NAME,VERSION);
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

