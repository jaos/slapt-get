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
void pkg_action_install(const rc_config *global_config,const pkg_action_args_t *action_args){
	int i;
	transaction tran;
	struct pkg_list *installed;
	struct pkg_list *all;
	pkg_info_t *pkg = NULL;
	pkg_info_t *installed_pkg = NULL;

	printf("Reading Package Lists... ");
	installed = get_installed_pkgs();
	all = get_available_pkgs();
	printf("Done\n");

	init_transaction(&tran);

	for(i = 0; i < action_args->count; i++){

		/* make sure there is a package called action_args->pkgs[i] */
		if( (pkg = get_newest_pkg(all,action_args->pkgs[i])) == NULL ){
			fprintf(stderr,"No Such package: %s\n",action_args->pkgs[i]);
			continue;
		}
		installed_pkg = get_newest_pkg(installed,action_args->pkgs[i]);

		/* if it's not already installed, install it */
		if( installed_pkg == NULL ){
			int c;
			struct pkg_list *deps;
			deps = lookup_pkg_dependencies(all,installed,pkg);
			for(c = 0; c < deps->pkg_count;c++){
				if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
					pkg_info_t *dep_installed;
					if( (dep_installed = get_newest_pkg(installed,deps->pkgs[c]->name)) == NULL ){
						add_install_to_transaction(&tran,deps->pkgs[c]);
					}else{
						add_upgrade_to_transaction(&tran,dep_installed,deps->pkgs[c]);
					}
				}
			}
			free(deps->pkgs);
			free(deps);
			/* this way we install the most up to date pkg */
			add_install_to_transaction(&tran,pkg);

		}else{ /* else we upgrade or reinstall */

			/* it's already installed, attempt an upgrade */
			if(
				((cmp_pkg_versions(installed_pkg->version,pkg->version)) < 0)
				|| (global_config->re_install == 1)
			){
				int c;
				struct pkg_list *deps;
				deps = lookup_pkg_dependencies(all,installed,pkg);
				for(c = 0; c < deps->pkg_count;c++){
					if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
						if( get_newest_pkg(installed,deps->pkgs[c]->name) == NULL ){
							add_install_to_transaction(&tran,deps->pkgs[c]);
						}else{
							add_upgrade_to_transaction(&tran,installed_pkg,deps->pkgs[c]);
						}
					}
				}
				free(deps->pkgs);
				free(deps);
				add_upgrade_to_transaction(&tran,installed_pkg,pkg);
			}else{
				printf("%s is up to date.\n",installed_pkg->name);
			}
	
		}

	}

	free_pkg_list(installed);
	free_pkg_list(all);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
	return;
}

/* list pkgs */
void pkg_action_list(void){
	struct pkg_list *pkgs = NULL;
	int i;

	pkgs = get_available_pkgs();

	for(i = 0; i < pkgs->pkg_count; i++ ){
		/* this should eliminate the printing of updates */
		if( strstr(pkgs->pkgs[i]->description,"no description") == NULL ){
			char *short_description = gen_short_pkg_description(pkgs->pkgs[i]);
			printf("%s - %s : %s\n",	
				pkgs->pkgs[i]->name,	
				pkgs->pkgs[i]->version,	
				short_description
			);
			free(short_description);
		}
	}

	free_pkg_list(pkgs);

}

/* list installed pkgs */
void pkg_action_list_installed(void){
	int i;
	struct pkg_list *installed_pkgs = NULL;

	installed_pkgs = get_installed_pkgs();

	for(i = 0; i < installed_pkgs->pkg_count; i++ ){
		printf("%s - %s\n",
			installed_pkgs->pkgs[i]->name,
			installed_pkgs->pkgs[i]->version
		);
	}

	free_pkg_list(installed_pkgs);
}

/* remove/uninstall pkg */
void pkg_action_remove(const rc_config *global_config,const pkg_action_args_t *action_args){
	int i;
	pkg_info_t *pkg;
	struct pkg_list *installed;
	struct pkg_list *available;
	transaction tran;

	installed = get_installed_pkgs();
	available = get_available_pkgs();
	init_transaction(&tran);

	for(i = 0; i < action_args->count; i++){
		if( (pkg = get_newest_pkg(installed,action_args->pkgs[i])) != NULL){
			int c;
			struct pkg_list *deps;
			deps = is_required_by(available,pkg);
			for(c = 0; c < deps->pkg_count;c++){
				printf("%s\n",deps->pkgs[c]->name);
				if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
					if( get_newest_pkg(installed,deps->pkgs[c]->name) != NULL ){
						add_remove_to_transaction(&tran,deps->pkgs[c]);
					}
				}
			}
			free(deps->pkgs);
			free(deps);
			add_remove_to_transaction(&tran,pkg);
		}else{
			printf("%s is not installed.\n",action_args->pkgs[i]);
		}
	}

	free_pkg_list(installed);
	free_pkg_list(available);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
}

/* search for a pkg (support extended POSIX regex) */
void pkg_action_search(const char *pattern){
	int i;
	struct pkg_list *pkgs = NULL;
	struct pkg_list *matches = NULL;

	/* read in pkg data */
	pkgs = get_available_pkgs();
	matches = malloc( sizeof *matches );
	matches->pkgs = malloc( sizeof *matches->pkgs * pkgs->pkg_count );
	matches->pkg_count = 0;

	search_pkg_list(pkgs,matches,pattern);
	for(i = 0; i < matches->pkg_count; i++){
		char *short_description = gen_short_pkg_description(matches->pkgs[i]);
		printf("%s - %s : %s\n",	
			matches->pkgs[i]->name,	
			matches->pkgs[i]->version,	
			short_description
		);
		free(short_description);
	}

	free(matches->pkgs);
	free(matches);
	free_pkg_list(pkgs);

}/* end search */

/* show the details for a specific package */
void pkg_action_show(const char *pkg_name){
	pkg_info_t *pkg;
	struct pkg_list *available_pkgs;

	available_pkgs = get_available_pkgs();

	pkg = get_newest_pkg(available_pkgs,pkg_name);

	if( pkg != NULL ){
		printf("Package Name: %s\n",pkg->name);
		printf("Package Mirror: %s\n",pkg->mirror);
		printf("Package Location: %s\n",pkg->location);
		printf("Package Version: %s\n",pkg->version);
		printf("Package Size: %d K\n",pkg->size_c);
		printf("Package Installed Size: %d K\n",pkg->size_u);
		printf("Package Required: %s\n",pkg->required);
		printf("Package Description:\n");
		printf("%s",pkg->description);
	}else{
		printf("No such package: %s\n",pkg_name);
	}

	free_pkg_list(available_pkgs);
}

/* update package data from mirror url */
void pkg_action_update(const rc_config *global_config){
	int i;
	FILE *pkg_list_fh;
	FILE *patches_list_fh;
	FILE *checksum_list_fh;
	FILE *tmp_file;
	struct pkg_list *available_pkgs = NULL;

	/* download our PKG_LIST */
	pkg_list_fh = open_file(PKG_LIST_L,"w+");
	for(i = 0; i < global_config->sources.count; i++){
		tmp_file = tmpfile();
		#if USE_CURL_PROGRESS == 0
		printf("Retrieving package data [%s]...",global_config->sources.url[i]);
		#else
		printf("Retrieving package data [%s]...\n",global_config->sources.url[i]);
		#endif
		if( get_mirror_data_from_source(tmp_file,global_config->sources.url[i],PKG_LIST) == 0 ){
			rewind(tmp_file); /* make sure we are back at the front of the file */
			available_pkgs = parse_packages_txt(tmp_file);
			write_pkg_data(global_config->sources.url[i],pkg_list_fh,available_pkgs);
			free_pkg_list(available_pkgs);
			#if USE_CURL_PROGRESS == 0
			printf("Done\n");
			#endif
		}
		fclose(tmp_file);
	}

	/* download EXTRAS_LIST */
#if GRAB_EXTRAS == 1
	for(i = 0; i < global_config->sources.count; i++){
		tmp_file = tmpfile();
		#if USE_CURL_PROGRESS == 0
		printf("Retrieving extras list [%s]...",global_config->sources.url[i]);
		#else
		printf("Retrieving extras list [%s]...\n",global_config->sources.url[i]);
		#endif
		if( get_mirror_data_from_source(tmp_file,global_config->sources.url[i],EXTRAS_LIST) == 0 ){
			rewind(tmp_file); /* make sure we are back at the front of the file */
			available_pkgs = parse_packages_txt(tmp_file);
			write_pkg_data(global_config->sources.url[i],pkg_list_fh,available_pkgs);
			free_pkg_list(available_pkgs);
			#if USE_CURL_PROGRESS == 0
			printf("Done\n");
			#endif
		}
		fclose(tmp_file);
	}
#endif

	/* download PATCHES_LIST */
	for(i = 0; i < global_config->sources.count; i++){
		patches_list_fh = tmpfile();
		#if USE_CURL_PROGRESS == 0
		printf("Retrieving patch list [%s]...",global_config->sources.url[i]);
		#else
		printf("Retrieving patch list [%s]...\n",global_config->sources.url[i]);
		#endif
		if( get_mirror_data_from_source(patches_list_fh,global_config->sources.url[i],PATCHES_LIST) == 0 ){
			rewind(patches_list_fh); /* make sure we are back at the front of the file */
			available_pkgs = parse_packages_txt(patches_list_fh);
			write_pkg_data(global_config->sources.url[i],pkg_list_fh,available_pkgs);
			free_pkg_list(available_pkgs);
			#if USE_CURL_PROGRESS == 0
			printf("Done\n");
			#endif
		}
		fclose(patches_list_fh);
	}
	fclose(pkg_list_fh);

	/* download checksum file */
	checksum_list_fh = open_file(CHECKSUM_FILE,"w+");
	for(i = 0; i < global_config->sources.count; i++){
		#if USE_CURL_PROGRESS == 0
		printf("Retrieving checksum list [%s]...",global_config->sources.url[i]);
		#else
		printf("Retrieving checksum list [%s]...\n",global_config->sources.url[i]);
		#endif
		if( get_mirror_data_from_source(checksum_list_fh,global_config->sources.url[i],CHECKSUM_FILE) == 0 ){
			#if USE_CURL_PROGRESS == 0
			printf("Done\n");
			#endif
		}
	}
	fclose(checksum_list_fh);

	/* source listing to go here */

	return;
}

/* upgrade all installed pkgs with available updates */
void pkg_action_upgrade_all(const rc_config *global_config){
	int i;
	struct pkg_list *installed_pkgs;
	struct pkg_list *all_pkgs;
	struct pkg_list *matches;
	pkg_info_t *update_pkg;
	pkg_info_t *installed_pkg;
	transaction tran;

	printf("Reading Package Lists... ");
	installed_pkgs = get_installed_pkgs();
	all_pkgs = get_available_pkgs();
	printf("Done\n");
	init_transaction(&tran);

	for(i = 0; i < installed_pkgs->pkg_count;i++){

		/* see if we have an available update for the pkg */
		update_pkg = get_newest_pkg(
			all_pkgs,
			installed_pkgs->pkgs[i]->name
		);
		if( update_pkg != NULL ){

			if( is_excluded(global_config,installed_pkgs->pkgs[i]) == 1 ){
				add_exclude_to_transaction(&tran,update_pkg);
				continue;
			}

			/* if the update has a newer version, attempt to upgrade */
			if( (cmp_pkg_versions(installed_pkgs->pkgs[i]->version,update_pkg->version)) < 0 ){
				int c;
				struct pkg_list *deps;
				deps = lookup_pkg_dependencies(all_pkgs,installed_pkgs,update_pkg);
				for(c = 0; c < deps->pkg_count;c++){
					if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
						if( get_newest_pkg(installed_pkgs,deps->pkgs[c]->name) == NULL ){
							add_install_to_transaction(&tran,deps->pkgs[c]);
						}else{
							add_upgrade_to_transaction(&tran,installed_pkgs->pkgs[i],deps->pkgs[c]);
						}
					}
				}
				free(deps->pkgs);
				free(deps);
				add_upgrade_to_transaction(&tran,installed_pkgs->pkgs[i],update_pkg);
			}

		}/* end upgrade pkg found */

	}/* end for */

	if( global_config->dist_upgrade == 1 ){

		matches = malloc( sizeof *matches );
		matches->pkgs = malloc( sizeof *matches->pkgs * all_pkgs->pkg_count );
		matches->pkg_count = 0;

		search_pkg_list(all_pkgs,matches,SLACK_BASE_SET_REGEX);
		for(i = 0; i < matches->pkg_count; i++){

			installed_pkg = get_newest_pkg(
				installed_pkgs,
				matches->pkgs[i]->name
			);
			/* add to install list if not already installed */
			if( installed_pkg == NULL ){
				if( is_excluded(global_config,matches->pkgs[i]) == 1 ){
					add_exclude_to_transaction(&tran,matches->pkgs[i]);
				}else{
					int c;
					struct pkg_list *deps;
					deps = lookup_pkg_dependencies(all_pkgs,installed_pkgs,matches->pkgs[i]);
					for(c = 0; i < deps->pkg_count;c++){
						if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
							pkg_info_t *dep_installed;
							if( (dep_installed = get_newest_pkg(installed_pkgs,deps->pkgs[c]->name)) == NULL ){
								add_install_to_transaction(&tran,deps->pkgs[c]);
							}else{
								add_upgrade_to_transaction(&tran,dep_installed,deps->pkgs[c]);
							}
						}
					}
					free(deps->pkgs);
					free(deps);
					add_install_to_transaction(&tran,matches->pkgs[i]);
				}
			}

		}

		free(matches->pkgs);
		free(matches);
	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(all_pkgs);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
}

