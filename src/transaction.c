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

#include <main.h>
static void add_suggestion(transaction_t *tran, pkg_info_t *pkg);

void init_transaction(transaction_t *tran){

	tran->install_pkgs = slapt_malloc( sizeof *tran->install_pkgs );
	tran->remove_pkgs = slapt_malloc( sizeof *tran->remove_pkgs );
	tran->upgrade_pkgs = slapt_malloc( sizeof *tran->upgrade_pkgs );
	tran->exclude_pkgs = slapt_malloc( sizeof *tran->exclude_pkgs );

	tran->install_pkgs->pkgs = slapt_malloc( sizeof *tran->install_pkgs->pkgs );
	tran->remove_pkgs->pkgs = slapt_malloc( sizeof *tran->remove_pkgs->pkgs );
	tran->upgrade_pkgs->pkgs = slapt_malloc( sizeof *tran->upgrade_pkgs->pkgs );
	tran->exclude_pkgs->pkgs = slapt_malloc( sizeof *tran->exclude_pkgs->pkgs );

	tran->suggests = slapt_malloc(sizeof *tran->suggests );
	tran->suggests[0] = '\0';

	tran->install_pkgs->pkg_count = 0;
	tran->remove_pkgs->pkg_count = 0;
	tran->upgrade_pkgs->pkg_count = 0;
	tran->exclude_pkgs->pkg_count = 0;
}

int handle_transaction(const rc_config *global_config, transaction_t *tran){
	unsigned int i;
	size_t download_size = 0;
	size_t already_download_size = 0;
	size_t uncompressed_size = 0;

	/* show pkgs to exclude */
	if( tran->exclude_pkgs->pkg_count > 0 ){
		printf(_("The following packages have been EXCLUDED:\n"));
		printf("  ");
		for(i = 0; i < tran->exclude_pkgs->pkg_count;i++){
			printf("%s ",tran->exclude_pkgs->pkgs[i]->name);
		}
		printf("\n");
	}

	/* show suggested pkgs */
	if( strlen(tran->suggests) > 0 ){
		printf(_("Suggested packages:\n"));
		printf("  %s\n",tran->suggests);
	}

	/* show pkgs to install */
	if( tran->install_pkgs->pkg_count > 0 ){
		printf(_("The following NEW packages will be installed:\n"));
		printf("  ");
		for(i = 0; i < tran->install_pkgs->pkg_count;i++){
			printf("%s ",tran->install_pkgs->pkgs[i]->name);
			already_download_size += get_pkg_file_size(
				global_config,tran->install_pkgs->pkgs[i]
			) / 1024;
			download_size += tran->install_pkgs->pkgs[i]->size_c;
			uncompressed_size += tran->install_pkgs->pkgs[i]->size_u;
		}
		printf("\n");
	}

	/* show pkgs to remove */
	if( tran->remove_pkgs->pkg_count > 0 ){
		printf(_("The following packages will be REMOVED:\n"));
		printf("  ");
		for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
			printf("%s ",tran->remove_pkgs->pkgs[i]->name);
			uncompressed_size -= tran->remove_pkgs->pkgs[i]->size_u;
		}
		printf("\n");
	}

	/* show pkgs to upgrade */
	if( tran->upgrade_pkgs->pkg_count > 0 ){
		printf(_("The following packages will be upgraded:\n"));
		printf("  ");
		for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
			printf("%s ",tran->upgrade_pkgs->pkgs[i]->upgrade->name);
			already_download_size += get_pkg_file_size(
				global_config,tran->upgrade_pkgs->pkgs[i]->upgrade
			) / 1024;
			download_size += tran->upgrade_pkgs->pkgs[i]->upgrade->size_c;
			uncompressed_size += tran->upgrade_pkgs->pkgs[i]->upgrade->size_u;
			uncompressed_size -= tran->upgrade_pkgs->pkgs[i]->installed->size_u;
		}
		printf("\n");
	}

	/* print the summary */
	printf(
		_("%d upgraded, %d newly installed, %d to remove and %d not upgraded.\n"),
		tran->upgrade_pkgs->pkg_count,
		tran->install_pkgs->pkg_count,
		tran->remove_pkgs->pkg_count,
		tran->exclude_pkgs->pkg_count
	);

	/* only show this if we are going to do download something */
	if( tran->upgrade_pkgs->pkg_count > 0 || tran->install_pkgs->pkg_count > 0 ){
		/* how much we need to download */
		int need_to_download_size = download_size - already_download_size;
		/* make sure it's not negative due to changing pkg sizes on upgrades */
		if( need_to_download_size < 0 ) need_to_download_size = 0;

		if( already_download_size > 0 ){
			printf(_("Need to get %dK/%dK of archives.\n"),
				need_to_download_size, download_size
			);
		}else{
			printf(_("Need to get %dK of archives.\n"), download_size );
		}
	}

	if( tran->upgrade_pkgs->pkg_count > 0 || tran->remove_pkgs->pkg_count > 0 ||
	tran->install_pkgs->pkg_count > 0 ){

		if( global_config->download_only == 0 ){
			if( (int)uncompressed_size < 0 ){
				printf(_("After unpacking %dK disk space will be freed.\n"),
					uncompressed_size * -1
				);
			}else{
				printf(_("After unpacking %dK of additional disk space will be used.\n"),
					uncompressed_size
				);
			}
		}
	}

	/* prompt */
	if(
			(tran->upgrade_pkgs->pkg_count > 0 || tran->remove_pkgs->pkg_count > 0 ||
			( tran->install_pkgs->pkg_count > 0 && global_config->dist_upgrade != 0 ) ) &&
			(global_config->no_prompt == 0 && global_config->download_only == 0 &&
			global_config->simulate == 0 && global_config->print_uris == 0 )
	) {
		if( ask_yes_no(_("Do you want to continue? [y/N] ")) != 1 ){
			printf(_("Abort.\n"));
			return 0;
		}
	}

	if ( global_config->print_uris == 1 ){
		for(i = 0; i < tran->install_pkgs->pkg_count;i++) {
			const pkg_info_t *info = tran->install_pkgs->pkgs[i];
			const char *location = info->location + strspn(info->location, "./");
			printf("%s%s/%s-%s.tgz\n", info->mirror, location, info->name, info->version);
		}
		for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++) {
			const pkg_info_t *info = tran->upgrade_pkgs->pkgs[i]->upgrade;
			const char *location = info->location + strspn(info->location, "./");
			printf("%s%s/%s-%s.tgz\n", info->mirror, location, info->name, info->version);
		}
		return 0;
	}

	/* if simulate is requested, just show what could happen and return */
	if( global_config->simulate == 1 ){
		for(i = 0; i < tran->install_pkgs->pkg_count;i++){
			printf(_("%s-%s is to be installed\n"),
				tran->install_pkgs->pkgs[i]->name,tran->install_pkgs->pkgs[i]->version
			);
		}
		for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
			printf(_("%s-%s is to be upgraded to version %s\n"),
				tran->upgrade_pkgs->pkgs[i]->upgrade->name,
				tran->upgrade_pkgs->pkgs[i]->installed->version,
				tran->upgrade_pkgs->pkgs[i]->upgrade->version
				);
		}
		for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
			printf(_("%s-%s is to be removed\n"),
				tran->remove_pkgs->pkgs[i]->name,tran->remove_pkgs->pkgs[i]->version
			);
		}
		printf(_("Done\n"));
		return 0;
	}

	/* download pkgs */
	for(i = 0; i < tran->install_pkgs->pkg_count;i++)
		if( download_pkg(global_config,tran->install_pkgs->pkgs[i]) != 0 ) exit(1);
	for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++)
		if( download_pkg(global_config,tran->upgrade_pkgs->pkgs[i]->upgrade) != 0 ) exit(1);

	printf("\n");

	/* run transaction, install, upgrade, and remove */
	for(i = 0; i < tran->install_pkgs->pkg_count;i++){
		if( global_config->download_only == 0 )
			if( install_pkg(global_config,tran->install_pkgs->pkgs[i]) == -1 ) exit(1);
	}
	for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
		if( global_config->download_only == 0 ){
			if( upgrade_pkg( global_config,
				tran->upgrade_pkgs->pkgs[i]->installed,
				tran->upgrade_pkgs->pkgs[i]->upgrade
			) == -1 ) exit(1);
		}

	}
	for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
		if( remove_pkg(global_config,tran->remove_pkgs->pkgs[i]) == -1 ) exit(1);
	}

	printf(_("Done\n"));

	return 0;
}

void add_install_to_transaction(transaction_t *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	/* don't add if already present in the transaction */
	if( search_transaction(tran,pkg) == 1 ) return;

	#if DEBUG == 1
	printf("adding install of %s-%s@%s to transaction\n",
		pkg->name,pkg->version,pkg->location);
	#endif

	tmp_list = realloc(
		tran->install_pkgs->pkgs,
		sizeof *tran->install_pkgs->pkgs * ( tran->install_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->install_pkgs->pkgs = tmp_list;

		tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count] = slapt_malloc(
			sizeof *tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count]
		);
		memcpy(
			tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count],
			pkg,
			sizeof *pkg
		);
		++tran->install_pkgs->pkg_count;
	}

	add_suggestion(tran,pkg);

}

void add_remove_to_transaction(transaction_t *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	/* don't add if already present in the transaction */
	if( search_transaction(tran,pkg) == 1 ) return;

	#if DEBUG == 1
	printf("adding remove of %s-%s@%s to transaction\n",
		pkg->name,pkg->version,pkg->location);
	#endif

	tmp_list = realloc(
		tran->remove_pkgs->pkgs,
		sizeof *tran->remove_pkgs->pkgs * ( tran->remove_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->remove_pkgs->pkgs = tmp_list;

		tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count] = slapt_malloc(
			sizeof *tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count]
		);
		memcpy(
			tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count],
			pkg,
			sizeof *pkg
		);
		++tran->remove_pkgs->pkg_count;
	}

}

void add_exclude_to_transaction(transaction_t *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	/* don't add if already present in the transaction */
	if( search_transaction(tran,pkg) == 1 ) return;

	#if DEBUG == 1
	printf("adding exclude of %s-%s@%s to transaction\n",
		pkg->name,pkg->version,pkg->location);
	#endif

	tmp_list = realloc(
		tran->exclude_pkgs->pkgs,
		sizeof *tran->exclude_pkgs->pkgs * ( tran->exclude_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->exclude_pkgs->pkgs = tmp_list;

		tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count] = slapt_malloc(
			sizeof *tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count]
		);
		memcpy(
			tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count],
			pkg,
			sizeof *pkg
		);
		++tran->exclude_pkgs->pkg_count;
	}

}

void add_upgrade_to_transaction(
	transaction_t *tran, pkg_info_t *installed_pkg, pkg_info_t *upgrade_pkg
){
	pkg_upgrade_t **tmp_list;

	/* don't add if already present in the transaction */
	if( search_transaction(tran,upgrade_pkg) == 1 ) return;

	#if DEBUG == 1
	printf("adding upgrade of %s-%s@%s to transaction\n",
		upgrade_pkg->name,upgrade_pkg->version,upgrade_pkg->location);
	#endif

	tmp_list = realloc(
		tran->upgrade_pkgs->pkgs,
		sizeof *tran->upgrade_pkgs->pkgs * ( tran->upgrade_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->upgrade_pkgs->pkgs = tmp_list;

		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count] = slapt_malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]
		);
		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed = slapt_malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed
		);
		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade = slapt_malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade
		);

		memcpy(
			tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed,
			installed_pkg,
			sizeof *installed_pkg
		);
		memcpy(
			tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade,
			upgrade_pkg,
			sizeof *upgrade_pkg
		);
		++tran->upgrade_pkgs->pkg_count;
	}

}

int search_transaction(transaction_t *tran,pkg_info_t *pkg){
	unsigned int i,found = 1, not_found = 0;

	for(i = 0; i < tran->install_pkgs->pkg_count;i++){
		if( strcmp(pkg->name,tran->install_pkgs->pkgs[i]->name)==0 )
			return found;
	}
	for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
		if( strcmp(pkg->name,tran->upgrade_pkgs->pkgs[i]->upgrade->name)==0 )
			return found;
	}
	for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
		if( strcmp(pkg->name,tran->remove_pkgs->pkgs[i]->name)==0 )
			return found;
	}
	for(i = 0; i < tran->exclude_pkgs->pkg_count;i++){
		if( strcmp(pkg->name,tran->exclude_pkgs->pkgs[i]->name)==0 )
			return found;
	}
	return not_found;
}

static int search_upgrade_transaction(transaction_t *tran,pkg_info_t *pkg){
	unsigned int i,found = 1, not_found = 0;
	for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
		if( strcmp(pkg->name,tran->upgrade_pkgs->pkgs[i]->upgrade->name)==0 )
			return found;
	}
	return not_found;
}

void free_transaction(transaction_t *tran){
	unsigned int i;

	for(i = 0;i < tran->install_pkgs->pkg_count; i++){
		free(tran->install_pkgs->pkgs[i]);
	}
	free(tran->install_pkgs->pkgs);

	for(i = 0;i < tran->remove_pkgs->pkg_count; i++){
		free(tran->remove_pkgs->pkgs[i]);
	}
	free(tran->remove_pkgs->pkgs);

	for(i = 0;i < tran->upgrade_pkgs->pkg_count; i++){
		free(tran->upgrade_pkgs->pkgs[i]->upgrade);
		free(tran->upgrade_pkgs->pkgs[i]->installed);
		free(tran->upgrade_pkgs->pkgs[i]);
	}
	free(tran->upgrade_pkgs->pkgs);

	for(i = 0; i < tran->exclude_pkgs->pkg_count;i++){
		free(tran->exclude_pkgs->pkgs[i]);
	}
	free(tran->exclude_pkgs->pkgs);

	free(tran->install_pkgs);
	free(tran->remove_pkgs);
	free(tran->upgrade_pkgs);
	free(tran->exclude_pkgs);

	free(tran->suggests);

}

transaction_t *remove_from_transaction(transaction_t *tran,pkg_info_t *pkg){
	unsigned int i;
	transaction_t *new_tran = NULL;

	if( search_transaction(tran,pkg) == 0 )
		return tran;

	/* since this is a pointer, slapt_malloc before calling init */
	new_tran = slapt_malloc(sizeof *new_tran);
	new_tran->install_pkgs = slapt_malloc( sizeof *new_tran->install_pkgs );
	new_tran->remove_pkgs = slapt_malloc( sizeof *new_tran->remove_pkgs );
	new_tran->upgrade_pkgs = slapt_malloc( sizeof *new_tran->upgrade_pkgs );
	new_tran->exclude_pkgs = slapt_malloc( sizeof *new_tran->exclude_pkgs );
	init_transaction(new_tran);

	for(i = 0;i < tran->install_pkgs->pkg_count; i++){
		if(
			strcmp(pkg->name,tran->install_pkgs->pkgs[i]->name) != 0 &&
			strcmp(pkg->version,tran->install_pkgs->pkgs[i]->version) != 0 &&
			strcmp(pkg->location,tran->install_pkgs->pkgs[i]->location) != 0
		) add_install_to_transaction(new_tran,tran->install_pkgs->pkgs[i]);
	}
	for(i = 0;i < tran->remove_pkgs->pkg_count; i++){
		if(
			strcmp(pkg->name,tran->remove_pkgs->pkgs[i]->name) != 0 &&
			strcmp(pkg->version,tran->remove_pkgs->pkgs[i]->version) != 0 &&
			strcmp(pkg->location,tran->remove_pkgs->pkgs[i]->location) != 0
		) add_remove_to_transaction(new_tran,tran->remove_pkgs->pkgs[i]);
	}
	for(i = 0;i < tran->upgrade_pkgs->pkg_count; i++){
		if(
			strcmp(pkg->name,tran->upgrade_pkgs->pkgs[i]->upgrade->name) != 0 &&
			strcmp(pkg->version,tran->upgrade_pkgs->pkgs[i]->upgrade->version) != 0 &&
			strcmp(pkg->location,tran->upgrade_pkgs->pkgs[i]->upgrade->location) != 0
		) add_upgrade_to_transaction(
			new_tran,
			tran->upgrade_pkgs->pkgs[i]->installed,
			tran->upgrade_pkgs->pkgs[i]->upgrade
		);
	}
	for(i = 0; i < tran->exclude_pkgs->pkg_count;i++){
		if(
			strcmp(pkg->name,tran->exclude_pkgs->pkgs[i]->name) != 0 &&
			strcmp(pkg->version,tran->exclude_pkgs->pkgs[i]->version) != 0 &&
			strcmp(pkg->location,tran->exclude_pkgs->pkgs[i]->location) != 0
		) add_exclude_to_transaction(new_tran,tran->exclude_pkgs->pkgs[i]);
	}

	return new_tran;
}

/* parse the dependencies for a package, and add them to the transaction as needed */
/* check to see if a package is conflicted */
int add_deps_to_trans(const rc_config *global_config, transaction_t *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg){
	unsigned int c;
	int dep_return = -1;
	struct pkg_list *deps;

	if( global_config->disable_dep_check == 1) return -1;

	deps = init_pkg_list();

	dep_return = get_pkg_dependencies(global_config,avail_pkgs,installed_pkgs,pkg,deps);

	/* check to see if there where issues with dep checking */
	/* exclude the package if dep check barfed */
	if( (dep_return == -1) && (global_config->ignore_dep == 0) ){
		printf("Excluding %s, use --ignore-dep to override\n",pkg->name);
		add_exclude_to_transaction(tran,pkg);
		/* free_pkg_list(deps); */
		free(deps->pkgs);
		free(deps);
		return -1;
	}

	/* loop through the deps */
	for(c = 0; c < deps->pkg_count;c++){

		/* only check if it is not already present in trans */
		if( search_transaction(tran,deps->pkgs[c]) == 0 ){

			pkg_info_t *dep_installed;

			/*
			 * the dep wouldn't get this far if it where excluded,
			 * so we don't check for that here
			 */

			if( (dep_installed = get_newest_pkg(installed_pkgs,deps->pkgs[c]->name)) == NULL ){
				pkg_info_t *conflicted_pkg = NULL;

				if ( (conflicted_pkg = is_conflicted(tran,avail_pkgs,installed_pkgs,deps->pkgs[c])) != NULL ){
					add_remove_to_transaction(tran,conflicted_pkg);
				}
				add_install_to_transaction(tran,deps->pkgs[c]);

			}else{

				/* add only if its a valid upgrade */
				if(cmp_pkg_versions(dep_installed->version,deps->pkgs[c]->version) < 0 ){
				pkg_info_t *conflicted_pkg = NULL;

					if ( (conflicted_pkg = is_conflicted(tran,avail_pkgs,installed_pkgs,deps->pkgs[c])) != NULL ){
						add_remove_to_transaction(tran,conflicted_pkg);
					}
					add_upgrade_to_transaction(tran,dep_installed,deps->pkgs[c]);

				}

			}

		}

	}

	/* free_pkg_list(deps); */
	free(deps->pkgs);
	free(deps);

	return 0;
}

/* make sure pkg isn't conflicted with what's already in the transaction */
pkg_info_t *is_conflicted(transaction_t *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg){
	unsigned int i;
	struct pkg_list *conflicts;

	/* if conflicts exist, check to see if they are installed or in the current transaction */
	conflicts = get_pkg_conflicts(avail_pkgs,installed_pkgs,pkg);
	for(i = 0; i < conflicts->pkg_count; i++){
		if(
			search_upgrade_transaction(tran,conflicts->pkgs[i]) == 1
			|| get_newest_pkg(tran->install_pkgs,conflicts->pkgs[i]->name) != NULL
		){
			pkg_info_t *c = conflicts->pkgs[i];
			printf(_("%s, which is to be installed, conflicts with %s\n"),
				conflicts->pkgs[i]->name,conflicts->pkgs[i]->version, pkg->name,pkg->version
			);
			free(conflicts->pkgs);
			free(conflicts);
			return c;
		}
		if(get_newest_pkg(installed_pkgs,conflicts->pkgs[i]->name) != NULL) {
			pkg_info_t *c = conflicts->pkgs[i];
			printf(_("Installed %s conflicts with %s\n"),conflicts->pkgs[i]->name,pkg->name);
			free(conflicts->pkgs);
			free(conflicts);
			return c;
		}
	}

	free(conflicts->pkgs);
	free(conflicts);

	return NULL;
}

static void add_suggestion(transaction_t *tran, pkg_info_t *pkg){
	char *tmp_buffer;

	if( pkg->suggests == NULL || strlen(pkg->suggests) == 0 ){
		return;
	}

	tmp_buffer = realloc(tran->suggests, strlen(tran->suggests) + strlen(pkg->suggests) + 1);
	if( tmp_buffer != NULL ){
		tran->suggests = tmp_buffer;
		strncat(tran->suggests,pkg->suggests,strlen(pkg->suggests));
	}
}
