/*
 * Copyright (C) 2003 Jason Woodward <woodwardj at jaos dot org>
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

int main( int argc, char *argv[] ){
	rc_config *global_config; /* our config struct */
	/* getopt needs these */
	int c = 0;
	extern char *optarg;
	extern int optind, opterr, optopt;
	enum action do_action = 0;
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
		{"version", 0, 0, 'v'},
		{"no-prompt", 0, 0, 'b'},
		{"reinstall", 0, 0, 'n'},
		{"ignore-excludes", 0, 0, 'x'},
		{"no-md5", 0, 0, '5'},
		{"dist-upgrade",0, 0, 'h'},
		{"help",0, 0, 'l'},
		{"h",0, 0, 'l'},
		{"no-dep",0, 0, 'p'},
	};
	int option_index = 0;
	/* */

	setvbuf(stdout, (char *)NULL, _IONBF, 0); /* unbuffer stdout */

	#ifdef ENABLE_NLS
	setlocale(LC_MESSAGES,getenv("LANG"));
	bindtextdomain(PROGRAM_NAME,LOCALESDIR);
	textdomain(PROGRAM_NAME);
	#endif

	if( argc < 2 ) usage(), exit(1);

	/* load up the configuration file */
	global_config = read_rc_config(RC_LOCATION);
	working_dir_init(global_config);
	chdir(global_config->working_dir);
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
			case 'p': /* no-dep */
				global_config->no_dep = 1;
				break;
			default:
				usage();
				exit(1);
		}
	}

	if( do_action == UPDATE ){
		update_pkg_cache(global_config,progress_callback);
	}else if( do_action == INSTALL ){
		if (optind < argc) {
			int i;
			pkg_action_args_t *paa;

			paa = malloc( sizeof *paa );
			paa->pkgs = malloc( sizeof *paa->pkgs * (argc - optind) );
			paa->count = 0;
			while (optind < argc){
				paa->pkgs[paa->count] = calloc(
					sizeof *paa->pkgs[paa->count] , ( strlen(argv[optind]) + 1 )
				);
				memcpy(paa->pkgs[paa->count],argv[optind],strlen(argv[optind]));
				++optind;
				++paa->count;
			}
			pkg_action_install( global_config, paa );
			for(i = 0; i < paa->count; i++){
				free(paa->pkgs[i]);
			}
			free(paa->pkgs);
			free(paa);
		}else{
			usage();
		}
	}else if( do_action == REMOVE ){
		if (optind < argc) {
			int i;
			pkg_action_args_t *paa;

			paa = malloc( sizeof *paa );
			paa->pkgs = malloc( sizeof *paa->pkgs * (argc - optind) );
			paa->count = 0;
			while (optind < argc){
				paa->pkgs[paa->count] = calloc(
					sizeof *paa->pkgs[paa->count] , ( strlen(argv[optind]) + 1 )
				);
				memcpy(paa->pkgs[paa->count],argv[optind],strlen(argv[optind]));
				++optind;
				++paa->count;
			}
			pkg_action_remove( global_config, paa );
			for(i = 0; i < paa->count; i++){
				free(paa->pkgs[i]);
			}
			free(paa->pkgs);
			free(paa);
		}else{
			usage();
		}
	}else if( do_action == SHOW ){
		if (optind < argc) {
			while (optind < argc){
				pkg_action_show( argv[optind++] );
			}
		}else{
			usage();
		}
	}else if( do_action == SEARCH ){
		if (optind < argc) {
			while (optind < argc){
				pkg_action_search( argv[optind++] );
			}
		}else{
			usage();
		}
	}else if( do_action == UPGRADE ){
		pkg_action_upgrade_all(global_config);
	}else if( do_action == LIST ){
		pkg_action_list();
	}else if( do_action == INSTALLED ){
		pkg_action_list_installed();
	}else if( do_action == CLEAN ){
		pkg_action_clean(global_config);
	}else if( do_action == SHOWVERSION ){
		version_info();
	}else if( do_action == 0 ){
		/* default initialized value */
	}else{
		usage();
		exit(1);
	}

	free_rc_config(global_config);
	curl_global_cleanup();
	return 0;
}

