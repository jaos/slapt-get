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
	static struct option long_options[] = {
		{"update", 0, 0, 'u'},
		{"upgrade", 0, 0, 'g'},
		{"install", 1, 0, 'i'},
		{"remove", 1, 0, 'r'},
		{"show", 1, 0, 's'},
		{"search", 1, 0, 'e'},
		{"list", 0, 0, 't'},
		{"installed", 0, 0, 'd'},
		{"clean", 0, 0, 'c'},
		{"download-only", 0, 0, 'o'},
		{"dist-upgrade", 0, 0, 'a'},
		{"simulate", 0, 0, 'm'},
		{"version", 0, 0, 'v'},
		{"no-prompt", 0, 0, 'b'},
		{"reinstall", 0, 0, 'n'},
		{"ignore-excludes", 0, 0, 'x'},
	};
	int option_index = 0;
	/* */

	setvbuf(stdout, (char *)NULL, _IONBF, 0); /* unbuffer stdout */

	if( argc < 2 )
		usage();

	/* load up the configuration file */
	global_config = read_rc_config(RC_LOCATION);
	working_dir_init(global_config);
	chdir(global_config->working_dir);
	curl_global_init(CURL_GLOBAL_ALL);

	while( ( c = getopt_long_only(argc,argv,"",long_options,&option_index ) ) != EOF ){
		switch(c){
			case 'u': /* update */
				jaospkg_update(global_config);
				break;
			case 'i': /* install */
				if( optarg != NULL ){
					jaospkg_install( global_config, optarg );
					for(;optind < argc;optind++){
						jaospkg_install( global_config, argv[optind] );
					}
				}else{
					usage();
				}
				break;
			case 'r': /* remove */
				if( optarg != NULL ){
					jaospkg_remove( optarg );
					for(;optind < argc;optind++){
						jaospkg_remove( argv[optind] );
					}
				}else{
					usage();
				}
				break;
			case 's': /* show */
				if( optarg != NULL ){
					jaospkg_show( optarg );
				}else{
					usage();
				}
				break;
			case 'e': /* search */
				if( optarg != NULL ){
					jaospkg_search( optarg );
				}else{
					usage();
				}
				break;
			case 't': /* list */
				jaospkg_list();
				break;
			case 'd': /* installed */
				jaospkg_list_installed();
				break;
			case 'c': /* clean */
				jaospkg_clean(global_config);
				break;
			case 'g': /* upgrade */
				jaospkg_upgrade_all(global_config);
				break;
			case 'o': /* download only flag */
				global_config->download_only = 1;
				break;
			case 'a': /* dist-upgrade */
				global_config->dist_upgrade = 1;
				break;
			case 'm': /* simulate */
				global_config->simulate = 1;
				break;
			case 'v': /* version */
				version_info();
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
			default:
				usage();
				break;
		}
	}

	free(global_config->exclude_list);
	free(global_config);
	return 0;
}

void usage(){
	printf("%s - Jason Woodward <woodwardj at jaos dot org>\n",PROGRAM_NAME);
	printf("A crude port of the Debian APT system to Slackware\n");
	printf("Usage:\n");
	printf("jaospkg [option(s)] [target]\n");
	printf("\n");
	printf("Targets:\n");
	printf("  --update    - retrieves pkg data from MIRROR\n");
	printf("  --upgrade   - upgrade installed pkgs\n");
	printf("  --install   [pkg name(s)] - install specified pkg(s)\n");
	printf("  --remove    [pkg name(s)] - remove specified pkg(s)\n");
	printf("  --show      [pkg name] - show pkg description\n");
	printf("  --search    [expression] - search available pkgs\n");
	printf("  --list      - list available pkgs\n");
	printf("  --installed - list installed pkgs\n");
	printf("  --clean     - purge cached pkgs\n");
	printf("  --version   - print version and license info\n");
	printf("\n");
	printf("Options:\n");
	printf("  --download-only   - only download pkg on install/upgrade\n");
	printf("  --dist-upgrade    - assumes MIRROR is set to newer release\n");
	printf("  --simulate        - show pkgs to be upgraded\n");
	printf("  --no-prompt       - do not prompt during upgrade\n");
	printf("  --reinstall      - re-install the pkg even if installed\n");
	printf("  --ignore-excludes - install excludes anyway\n");
}

void version_info(void){
	printf("%s version %s\n",PROGRAM_NAME,VERSION);
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
