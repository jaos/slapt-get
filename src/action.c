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
void pkg_action_clean(const rc_config *global_config){
	clean_pkg_dir(global_config->working_dir);
}

/* install pkg */
void pkg_action_install(const rc_config *global_config,const char *pkg_name){
	pkg_info_t *installed_pkg;
	pkg_info_t *update_pkg;
	pkg_info_t *pkg;

	struct pkg_list *installed;
	struct pkg_list *updates;
	struct pkg_list *available;

	updates = get_update_pkgs();
	installed = get_installed_pkgs();
	available = get_available_pkgs();

	if( (pkg = get_newest_pkg(available->pkgs,pkg_name,available->pkg_count)) == NULL ){
		fprintf(stderr,"No Such package: %s\n",pkg_name);
		return;
	}

	if( ((installed_pkg = get_newest_pkg(installed->pkgs,pkg_name,installed->pkg_count)) != NULL)
		&& (global_config->re_install != 1 ) ){

		/* it's already installed, attempt an upgrade */
		pkg_action_upgrade(global_config,installed_pkg,updates,pkg);

	}else{

		/* check to see if the package exists in the update list */
		/* this way we install the most up to date pkg */
		if( (update_pkg = get_newest_pkg(updates->pkgs,pkg_name,updates->pkg_count)) != NULL ){
			if( (install_pkg(global_config,update_pkg)) == -1 ){
				fprintf(stderr,"Installation of %s failed.\n",update_pkg->name);
			}
		}else{
			/* install from list */
			if( (install_pkg(global_config,pkg)) == -1 ){
				fprintf(stderr,"Installation of %s failed.\n",pkg->name);
			}
		}

	}

	free_pkg_list(installed);
	free_pkg_list(updates);
	free_pkg_list(available);
	return;
}

/* list pkgs */
void pkg_action_list(void){
	struct pkg_list *pkgs = NULL;
	int iterator;

	pkgs = get_available_pkgs();

	for(iterator = 0; iterator < pkgs->pkg_count; iterator++ ){
		char *short_description = gen_short_pkg_description(pkgs->pkgs[iterator]);
		printf("%s - %s\n",pkgs->pkgs[iterator]->name,short_description);
		free(short_description);
	}

	free_pkg_list(pkgs);

}

/* list installed pkgs */
void pkg_action_list_installed(void){
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
void pkg_action_remove(const char *pkg_name){
	pkg_info_t *pkg;
	struct pkg_list *installed;

	installed = get_installed_pkgs();

	if( (pkg = get_newest_pkg(installed->pkgs,pkg_name,installed->pkg_count)) != NULL ){
		if( (remove_pkg(pkg)) == -1 ){
			fprintf(stderr,"Failed to remove %s.\n",pkg_name);
		}
	}else{
		printf("%s is not installed.\n",pkg_name);
	}

	free_pkg_list(installed);
}

/* search for a pkg (support extended POSIX regex) */
void pkg_action_search(const char *pattern){
	struct pkg_list *pkgs = NULL;
	int iterator;
	sg_regex search_regex;
	search_regex.nmatch = MAX_REGEX_PARTS;

	/* compile our regex */
	search_regex.reg_return = regcomp(&search_regex.regex, pattern, REG_EXTENDED|REG_NEWLINE);
	if( search_regex.reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, "Failed to compile regex\n");

		regerror_size = regerror(search_regex.reg_return, &search_regex.regex,errbuf,errbuf_size);
		if( regerror_size != 0 ){
			printf("Regex Error: %s\n",errbuf);
		}
		exit(1);
	}

	/* read in pkg data */
	pkgs = get_available_pkgs();

	for(iterator = 0; iterator < pkgs->pkg_count; iterator++ ){
		if(
			( regexec(
				&search_regex.regex,
				pkgs->pkgs[iterator]->name,
				search_regex.nmatch,
				search_regex.pmatch,
				0
			) == 0)
			||
			( regexec(
				&search_regex.regex,
				pkgs->pkgs[iterator]->description,
				search_regex.nmatch,
				search_regex.pmatch,
				0
			) == 0)
		){
				char *short_description = gen_short_pkg_description(pkgs->pkgs[iterator]);
				printf("%s - %s\n",pkgs->pkgs[iterator]->name,short_description);
				free(short_description);
		}
	}
	regfree(&search_regex.regex);
	free_pkg_list(pkgs);

}/* end search */

/* show the details for a specific package */
void pkg_action_show(const char *pkg_name){
	pkg_info_t *pkg;
	struct pkg_list *available_pkgs;

	available_pkgs = get_available_pkgs();

	pkg = get_newest_pkg(available_pkgs->pkgs,pkg_name,available_pkgs->pkg_count);

	if( pkg != NULL ){
		printf("Package Name: %s\n",pkg->name);
		printf("Package Location: %s\n",pkg->location);
		printf("Package version: %s\n",pkg->version);
		printf("Package Description:\n");
		printf("%s",pkg->description);
	}else{
		printf("No such package: %s\n",pkg_name);
	}

	free_pkg_list(available_pkgs);
}

/* update package data from mirror url */
void pkg_action_update(const rc_config *global_config){
	FILE *pkg_list_fh;
	FILE *patches_list_fh;
	FILE *checksum_list_fh;

	/* download our PKG_LIST */
	pkg_list_fh = download_pkg_list(global_config);
	fclose(pkg_list_fh);

	/* download PATCHES_LIST */
	patches_list_fh = download_patches_list(global_config);
	fclose(patches_list_fh);

	/* download */
	checksum_list_fh = download_checksum_list(global_config);
	fclose(checksum_list_fh);

	/* source listing to go here */

	return;
}

/* upgrade pkg */
/* flesh me out so that pkg_action_upgrade_all() calls me */
void pkg_action_upgrade(const rc_config *global_config,pkg_info_t *installed_pkg,struct pkg_list *update_pkgs,pkg_info_t *available_pkg){
	pkg_info_t *update_pkg;
	int cmp_result;

	/* if we found an update, make sure it's version is greater */
	if(
		(update_pkg = get_newest_pkg(update_pkgs->pkgs,installed_pkg->name,update_pkgs->pkg_count)) != NULL
		&& cmp_pkg_versions(available_pkg->version,update_pkg->version) < 0	
	){

		cmp_result = cmp_pkg_versions(installed_pkg->version,update_pkg->version);

		if( cmp_result < 0 ){ /* update_pkg is newer than installed_pkg */
			if( (upgrade_pkg(global_config,installed_pkg,update_pkg)) == -1 ){
				fprintf(stderr,"Failed to update %s.\n",installed_pkg->name);
			}
		}else{ 
			if( cmp_result > 0 ){
				printf("Newer version of %s already installed.\n",installed_pkg->name);
			}else if( cmp_result == 0 ){
				printf("%s is up to date.\n",installed_pkg->name);
			}
		}
	}else{
		if( cmp_pkg_versions(installed_pkg->version,available_pkg->version) < 0 ){
			if( (upgrade_pkg(global_config,installed_pkg,available_pkg)) == -1 ){
				fprintf(stderr,"Failed to update %s.\n",installed_pkg->name);
			}
		}else{
			printf("%s is already the newest version.\n",installed_pkg->name);
		}
	}

}

/* upgrade all installed pkgs with available updates */
/* use pkg_action_upgrade() soon, pass in pkg_list(s) */
void pkg_action_upgrade_all(const rc_config *global_config){
	int iterator;
	int try_dist_upgrade = 0;
	struct pkg_list *installed_pkgs;
	struct pkg_list *update_pkgs;
	struct pkg_list *current_pkgs;
	pkg_info_t *update_pkg;
	pkg_info_t *current_pkg;

	/* faster here to retrieve the listings once */
	/* then use get_newest_pkg() to pull newest from each list */
	printf("Reading Package Lists... ");
	installed_pkgs = get_installed_pkgs();
	update_pkgs = get_update_pkgs();
	current_pkgs = get_available_pkgs();
	printf("Done\n");

	for(iterator = 0; iterator < installed_pkgs->pkg_count;iterator++){

		/* see if we have an available update for the pkg */
		update_pkg = get_newest_pkg(
			update_pkgs->pkgs,
			installed_pkgs->pkgs[iterator]->name,
			update_pkgs->pkg_count
		);
		if( update_pkg != NULL ){

			/* if the update has a newer version */
			if( (cmp_pkg_versions(installed_pkgs->pkgs[iterator]->version,update_pkg->version)) < 0 ){

				/* attempt to upgrade */
				if( (upgrade_pkg(global_config,installed_pkgs->pkgs[iterator],update_pkg)) == -1 ){
					fprintf(stderr,"Failed to update %s.\n",installed_pkgs->pkgs[iterator]->name);
				}/* end upgrade attempt */

			}else{
				try_dist_upgrade = 1;
			} /* end version check */

		} else {
			try_dist_upgrade = 1;
		}/* end upgrade pkg found */

		if( try_dist_upgrade == 1 ){
			current_pkg = get_newest_pkg(
				current_pkgs->pkgs,
				installed_pkgs->pkgs[iterator]->name,
				current_pkgs->pkg_count
			);
			if( current_pkg != NULL ){
				/* the current version of the pkg is greater than the installed version */
				if( (cmp_pkg_versions(installed_pkgs->pkgs[iterator]->version,current_pkg->version)) < 0 ){
					/* attempt to upgrade */
					if( (upgrade_pkg(global_config,installed_pkgs->pkgs[iterator],current_pkg)) == -1 ){
						fprintf(
							stderr,
							"Failed to update %s.\n",
							installed_pkgs->pkgs[iterator]->name
						);
					}/* end upgrade attempt */
				}/* end if cmp_pkg_versions */
			}/* end if current_pkg */
		}

	}/* end for */


	printf("Done.\n");

	free_pkg_list(current_pkgs);
	free_pkg_list(installed_pkgs);
	free_pkg_list(update_pkgs);
}

