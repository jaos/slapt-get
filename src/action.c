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
	pkg_info_t *pkg;

	struct pkg_list *installed;
	struct pkg_list *all;

	installed = get_installed_pkgs();
	all = get_available_and_update_pkgs();

	/* make sure there is a package called pkg_name */
	if( (pkg = get_newest_pkg(all->pkgs,pkg_name,all->pkg_count)) == NULL ){
		fprintf(stderr,"No Such package: %s\n",pkg_name);
		return;
	}

	/* if it's not already installed, install it */
	if((installed_pkg = get_newest_pkg(installed->pkgs,pkg_name,installed->pkg_count)) == NULL){

		/* this way we install the most up to date pkg */
		install_pkg(global_config,pkg);

	}else{ /* else we upgrade or reinstall */

		/* it's already installed, attempt an upgrade */
		if(
			((cmp_pkg_versions(installed_pkg->version,pkg->version)) < 0)
			|| (global_config->re_install == 1)
		){
			upgrade_pkg(global_config,installed_pkg,pkg);
		}else{
			printf("%s is up to date.\n",installed_pkg->name);
		}

	}

	free_pkg_list(installed);
	free_pkg_list(all);
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
		remove_pkg(pkg);
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

/* upgrade all installed pkgs with available updates */
/* use pkg_action_upgrade() soon, pass in pkg_list(s) */
void pkg_action_upgrade_all(const rc_config *global_config){
	int iterator;
	struct pkg_list *installed_pkgs;
	struct pkg_list *all_pkgs;
	pkg_info_t *update_pkg;

	/* faster here to retrieve the listings once */
	/* then use get_newest_pkg() to pull newest from each list */
	printf("Reading Package Lists... ");
	installed_pkgs = get_installed_pkgs();
	all_pkgs = get_available_and_update_pkgs();
	printf("Done\n");

	for(iterator = 0; iterator < installed_pkgs->pkg_count;iterator++){

		/* see if we have an available update for the pkg */
		update_pkg = get_newest_pkg(
			all_pkgs->pkgs,
			installed_pkgs->pkgs[iterator]->name,
			all_pkgs->pkg_count
		);
		if( update_pkg != NULL ){

			/* if the update has a newer version */
			if( (cmp_pkg_versions(installed_pkgs->pkgs[iterator]->version,update_pkg->version)) < 0 ){

				/* attempt to upgrade */
				upgrade_pkg(global_config,installed_pkgs->pkgs[iterator],update_pkg);

			} /* end version check */

		}/* end upgrade pkg found */

	}/* end for */


	printf("Done.\n");

	free_pkg_list(installed_pkgs);
	free_pkg_list(all_pkgs);
}

