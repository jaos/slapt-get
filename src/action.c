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

#include <main.h>

/* clean out local cache */
void jaospkg_clean(rc_config *global_config){
	clean_pkg_dir(global_config->working_dir);
}

/* install pkg */
void jaospkg_install(rc_config *global_config,char *pkg_name){
	pkg_info *installed_pkg;
	pkg_info *update_pkg;
	pkg_info *pkg;

	if( (pkg = lookup_pkg(pkg_name)) == NULL ){
		fprintf(stderr,"No Such package: %s\n",pkg_name);
		return;
	}

	if( ((installed_pkg = get_newest_installed_pkg(pkg_name)) != NULL)
		&& (global_config->re_install != 1 ) ){

		/* it's already installed, attempt an upgrade */
		jaospkg_upgrade(global_config,installed_pkg);
		free(installed_pkg);

	}else{

		/* check to see if the package exists in the update list */
		/* this way we install the most up to date pkg */
		if( (update_pkg = get_newest_update_pkg(pkg_name)) != NULL ){
			if( (install_pkg(global_config,update_pkg)) == -1 ){
				fprintf(stderr,"Installation of %s failed.\n",update_pkg->name);
			}
			free(update_pkg);
		}else{
			/* install from list */
			if( (install_pkg(global_config,pkg)) == -1 ){
				fprintf(stderr,"Installation of %s failed.\n",pkg->name);
			}
		}

	}

	free(pkg);
	return;
}

/* list pkgs */
void jaospkg_list(void){
	struct pkg_list *pkgs = NULL;
	int iterator;

	pkgs = parse_pkg_list();

	for(iterator = 0; iterator < pkgs->pkg_count; iterator++ ){
		char *short_description = gen_short_pkg_description(pkgs->pkgs[iterator]);
		printf("%s - %s\n",pkgs->pkgs[iterator]->name,short_description);
		free(short_description);
	}

	free_pkg_list(pkgs);

}

/* list installed pkgs */
void jaospkg_list_installed(void){
	int iterator;
	struct pkg_list *installed_pkgs = NULL;

	installed_pkgs = get_installed_pkgs();

	for(iterator = 0; iterator < installed_pkgs->pkg_count; iterator++ ){
		printf("%s - %s\n",
			installed_pkgs->pkgs[iterator]->name,
			installed_pkgs->pkgs[iterator]->version
		);
	}

	free_pkg_list(installed_pkgs);
}

/* remove/uninstall pkg */
void jaospkg_remove(char *pkg_name){
	pkg_info *pkg;

	if( (pkg = get_newest_installed_pkg(pkg_name)) != NULL ){
		if( (remove_pkg(pkg)) == -1 ){
			fprintf(stderr,"Failed to remove %s.\n",pkg_name);
		}
		free(pkg);
	}else{
		printf("%s is not installed.\n",pkg_name);
	}
}

/* search for a pkg (support extended POSIX regex) */
void jaospkg_search(char *pattern){
	struct pkg_list *pkgs = NULL;
	int iterator,regexec_return;
	regex_t regex;
	size_t nmatch = MAX_NMATCH;
	regmatch_t pmatch[MAX_PMATCH];

	/* compile our regex */
	if( (regexec_return = regcomp(&regex, pattern, REG_EXTENDED|REG_NEWLINE)) ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, "Failed to compile regex\n");

		if( (regerror_size = regerror(regexec_return, &regex,errbuf,errbuf_size)) ){
			printf("Regex Error: %s\n",errbuf);
		}
		exit(1);
	}

	/* read in pkg data */
	pkgs = parse_pkg_list();

	for(iterator = 0; iterator < pkgs->pkg_count; iterator++ ){
		if( (regexec(&regex,pkgs->pkgs[iterator]->name,nmatch,pmatch,0) == 0)
			|| (regexec(&regex,pkgs->pkgs[iterator]->description,nmatch,pmatch,0) == 0) ){
				char *short_description = gen_short_pkg_description(pkgs->pkgs[iterator]);
				printf("%s - %s\n",pkgs->pkgs[iterator]->name,short_description);
				free(short_description);
		}
	}
	regfree(&regex);
	free_pkg_list(pkgs);

}/* end search */

/* show the details for a specific package */
void jaospkg_show(char *pkg_name){
	pkg_info *pkg;
	if( (pkg = lookup_pkg(pkg_name)) != NULL ){
		printf("Package Name: %s\n",pkg->name);
		printf("Package Location: %s\n",pkg->location);
		printf("Package version: %s\n",pkg->version);
		printf("Package Description:\n");
		printf("%s",pkg->description);
		free(pkg);
	}else{
		printf("No such package: %s\n",pkg_name);
	}
}

/* update package data from mirror url */
void jaospkg_update(rc_config *global_config){
	FILE *pkg_list_fh;
	FILE *patches_list_fh;

	/* download our PKG_LIST */
	pkg_list_fh = download_pkg_list(global_config);
	fclose(pkg_list_fh);

	/* download PATCHES_LIST */
	patches_list_fh = download_patches_list(global_config);
	fclose(patches_list_fh);

	/* source listing to go here */

	return;
}

/* upgrade pkg */
/* flesh me out so that jaospkg_upgrade_all() calls me */
void jaospkg_upgrade(rc_config *global_config,pkg_info *installed_pkg){
	pkg_info *update_pkg;
	int cmp_result;

	/* if we found an update, make sure it's version is greater */
	if( (update_pkg = get_newest_update_pkg(installed_pkg->name)) != NULL ){

		cmp_result = strcmp(installed_pkg->version,update_pkg->version);

		if( cmp_result < 0 ){ /* update_pkg is newer than installed_pkg */
			if( (upgrade_pkg(global_config,update_pkg)) == -1 ){
				fprintf(stderr,"Failed to update %s.\n",installed_pkg->name);
				exit(1);
			}
		}else{ 
			if( cmp_result > 0 ){
				printf("Newer version of %s already installed.\n",installed_pkg->name);
			}else if( cmp_result == 0 ){
				printf("%s is up to date.\n",installed_pkg->name);
			}
		}
		free(update_pkg);
	}else{
		printf("%s is already the newest version.\n",installed_pkg->name);
	}

}

/* upgrade all installed pkgs with available updates */
/* use jaospkg_upgrade() soon, pass in pkg_list(s) */
void jaospkg_upgrade_all(rc_config *global_config){
	int iterator;
	struct pkg_list *installed_pkgs = NULL;
	struct pkg_list *update_pkgs = NULL;
	struct pkg_list *current_pkgs = NULL;
	pkg_info *update_pkg = NULL;
	pkg_info *current_pkg = NULL;

	/* faster here to retrieve the listings once */
	/* then use get_newest_pkg() to pull newest from each list */
	printf("Reading Package Lists... ");
	installed_pkgs = get_installed_pkgs();
	update_pkgs = parse_update_pkg_list();
	current_pkgs = parse_pkg_list();
	printf("Done\n");

	for(iterator = 0; iterator < installed_pkgs->pkg_count;iterator++){

		/* skip if excluded */
		if( is_excluded(global_config,installed_pkgs->pkgs[iterator]->name) == 1 )
			continue;

		/* see if we have an available update for the pkg */
		update_pkg = get_newest_pkg(
			update_pkgs->pkgs,
			installed_pkgs->pkgs[iterator]->name,
			update_pkgs->pkg_count
		);
		if( update_pkg != NULL ){

			/* if the update has a newer version */
			if( (strcmp(installed_pkgs->pkgs[iterator]->version,update_pkg->version)) < 0 ){

				/* attempt to upgrade */
				if( (upgrade_pkg(global_config,update_pkg)) == -1 ){
					fprintf(stderr,"Failed to update %s.\n",installed_pkgs->pkgs[iterator]->name);
					exit(1);
				}/* end upgrade attempt */

			}else{
				/* if --dist-upgrade is set */
				if( global_config->dist_upgrade == 1 ){
					current_pkg = get_newest_pkg(
						current_pkgs->pkgs,
						installed_pkgs->pkgs[iterator]->name,
						current_pkgs->pkg_count
					);
					if( current_pkg != NULL ){
						/* the current version of the pkg is greater than the installed version */
						if( (strcmp(installed_pkgs->pkgs[iterator]->version,current_pkg->version)) < 0 ){
							/* attempt to upgrade */
							if( (upgrade_pkg(global_config,current_pkg)) == -1 ){
								fprintf(
									stderr,
									"Failed to update %s.\n",
									installed_pkgs->pkgs[iterator]->name
								);
								exit(1);
							}/* end upgrade attempt */
						}/* end if strcmp */
						free(current_pkg);
					}/* end if current_pkg */
				}
			} /* end version check */

			free(update_pkg);

		}/* end upgrade pkg found */

	}/* end for */

	free_pkg_list(current_pkgs);
	free_pkg_list(installed_pkgs);
	free_pkg_list(update_pkgs);
}

