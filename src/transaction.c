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

void init_transaction(transaction *tran){

	tran->install_pkgs = malloc( sizeof *tran->install_pkgs );
	tran->remove_pkgs = malloc( sizeof *tran->remove_pkgs );
	tran->upgrade_pkgs = malloc( sizeof *tran->upgrade_pkgs );
	tran->exclude_pkgs = malloc( sizeof *tran->exclude_pkgs );

	tran->install_pkgs->pkgs = malloc( sizeof *tran->install_pkgs->pkgs );
	tran->remove_pkgs->pkgs = malloc( sizeof *tran->remove_pkgs->pkgs );
	tran->upgrade_pkgs->pkgs = malloc( sizeof *tran->upgrade_pkgs->pkgs );
	tran->exclude_pkgs->pkgs = malloc( sizeof *tran->exclude_pkgs->pkgs );

	tran->install_pkgs->pkg_count = 0;
	tran->remove_pkgs->pkg_count = 0;
	tran->upgrade_pkgs->pkg_count = 0;
	tran->exclude_pkgs->pkg_count = 0;
}

int handle_transaction(const rc_config *global_config, transaction *tran){
	int i;
	char prompt_answer[10];
	size_t download_size = 0;
	size_t uncompressed_size = 0;

	/*
		 if we have an update, or a removal...
		and download_only, simulate, and no_prompt are not set
	*/
	if( global_config->download_only == 0 && global_config->simulate == 0 ){

		/* show pkgs to exclude */
		if( tran->exclude_pkgs->pkg_count > 0 ){
			printf("The following packages have been EXCLUDED:\n");
			printf("  ");
			for(i = 0; i < tran->exclude_pkgs->pkg_count;i++){
				printf("%s ",tran->exclude_pkgs->pkgs[i]->name);
			}
			printf("\n");
		}

		/* show pkgs to install */
		if( tran->install_pkgs->pkg_count > 0 ){
			printf("The following NEW packages will be installed:\n");
			printf("  ");
			for(i = 0; i < tran->install_pkgs->pkg_count;i++){
				printf("%s ",tran->install_pkgs->pkgs[i]->name);
				download_size += tran->install_pkgs->pkgs[i]->size_c;
				uncompressed_size += tran->install_pkgs->pkgs[i]->size_u;
			}
			printf("\n");
		}

		/* show pkgs to remove */
		if( tran->remove_pkgs->pkg_count > 0 ){
			printf("The following packages will be REMOVED:\n");
			printf("  ");
			for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
				printf("%s ",tran->remove_pkgs->pkgs[i]->name);
				uncompressed_size -= tran->remove_pkgs->pkgs[i]->size_u;
			}
			printf("\n");
		}

		/* show pkgs to upgrade */
		if( tran->upgrade_pkgs->pkg_count > 0 ){
			printf("The following packages will be upgraded:\n");
			printf("  ");
			for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
				printf("%s ",tran->upgrade_pkgs->pkgs[i]->installed->name);
				download_size += tran->upgrade_pkgs->pkgs[i]->upgrade->size_c;
				/*
					* since the installed member of the struct has no size,
					* we leave this out and assume the new package is close to
					* being the same size 
				uncompressed_size += tran->upgrade_pkgs->pkgs[i]->upgrade->size_u;
				uncompressed_size -= tran->upgrade_pkgs->pkgs[i]->installed->size_u;
				*/
			}
			printf("\n");
		}

		/* print the summary */
		printf(
			"%d upgraded, %d newly installed, %d to remove and %d not upgraded.\n",
			tran->upgrade_pkgs->pkg_count,
			tran->install_pkgs->pkg_count,
			tran->remove_pkgs->pkg_count,
			tran->exclude_pkgs->pkg_count
		);

		printf("Need to get %dK of archives.\n", download_size );
		if( (int)uncompressed_size < 0 ){
			printf("After unpacking %dK disk space will be freed.\n", uncompressed_size * -1 );
		}else{
			printf("After unpacking %dK of additional disk space will be used.\n", uncompressed_size );
		}

		/* prompt */
		if(
				(tran->upgrade_pkgs->pkg_count > 0 || tran->remove_pkgs->pkg_count > 0)
				&& (global_config->no_prompt == 0 && global_config->interactive == 0)
			) {
			printf("Do you want to continue? [y/N] ");
			fgets(prompt_answer,10,stdin);
			if( tolower(prompt_answer[0]) != 'y' ){
				printf("Abort.\n");
				return 1;
			}
		}
	}

	for(i = 0; i < tran->install_pkgs->pkg_count;i++){
		install_pkg(global_config,tran->install_pkgs->pkgs[i]);
	}
	for(i = 0; i < tran->remove_pkgs->pkg_count;i++){
		remove_pkg(global_config,tran->remove_pkgs->pkgs[i]);
	}
	for(i = 0; i < tran->upgrade_pkgs->pkg_count;i++){
		upgrade_pkg(
			global_config,
			tran->upgrade_pkgs->pkgs[i]->installed,
			tran->upgrade_pkgs->pkgs[i]->upgrade
		);
	}

	printf("Done.\n");

	return 0;
}

void add_install_to_transaction(transaction *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	tmp_list = realloc(
		tran->install_pkgs->pkgs,
		sizeof *tran->install_pkgs->pkgs * ( tran->install_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->install_pkgs->pkgs = tmp_list;

		tran->install_pkgs->pkgs[tran->install_pkgs->pkg_count] = pkg;
		++tran->install_pkgs->pkg_count;
	}

}

void add_remove_to_transaction(transaction *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	tmp_list = realloc(
		tran->remove_pkgs->pkgs,
		sizeof *tran->remove_pkgs->pkgs * ( tran->remove_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->remove_pkgs->pkgs = tmp_list;

		tran->remove_pkgs->pkgs[tran->remove_pkgs->pkg_count] = pkg;
		++tran->remove_pkgs->pkg_count;
	}

}

void add_exclude_to_transaction(transaction *tran,pkg_info_t *pkg){
	pkg_info_t **tmp_list;

	tmp_list = realloc(
		tran->exclude_pkgs->pkgs,
		sizeof *tran->exclude_pkgs->pkgs * ( tran->exclude_pkgs->pkg_count + 1 )
	);
	if( tmp_list != NULL ){
		tran->exclude_pkgs->pkgs = tmp_list;

		tran->exclude_pkgs->pkgs[tran->exclude_pkgs->pkg_count] = pkg;
		++tran->exclude_pkgs->pkg_count;
	}

}

void add_upgrade_to_transaction(
	transaction *tran, pkg_info_t *installed_pkg, pkg_info_t *upgrade_pkg
){
	pkg_upgrade_t **tmp_list;

	tmp_list = realloc(
		tran->upgrade_pkgs->pkgs,
		sizeof *tran->upgrade_pkgs->pkgs * ( tran->upgrade_pkgs->pkg_count + 1 ) 
	);
	if( tmp_list != NULL ){
		tran->upgrade_pkgs->pkgs = tmp_list;

		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count] = malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]
		);
		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed = malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed
		);
		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade = malloc(
			sizeof *tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade
		);

		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->installed = installed_pkg;
		tran->upgrade_pkgs->pkgs[tran->upgrade_pkgs->pkg_count]->upgrade = upgrade_pkg;
		++tran->upgrade_pkgs->pkg_count;
	}

}

void free_transaction(transaction *tran){

	free(tran->install_pkgs->pkgs);
	free(tran->remove_pkgs->pkgs);
	free(tran->upgrade_pkgs->pkgs);
	free(tran->exclude_pkgs->pkgs);

	free(tran->install_pkgs);
	free(tran->remove_pkgs);
	free(tran->upgrade_pkgs);
	free(tran->exclude_pkgs);

}

