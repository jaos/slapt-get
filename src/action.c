/*
 * Copyright (C) 2004 Jason Woodward <woodwardj at jaos dot org>
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

/* install pkg */
void pkg_action_install(const rc_config *global_config,const pkg_action_args_t *action_args){
	int i;
	transaction tran;
	struct pkg_list *installed_pkgs;
	struct pkg_list *avail_pkgs;
	pkg_info_t *pkg = NULL;
	pkg_info_t *installed_pkg = NULL;
	sg_regex pkg_regex;

	printf( _("Reading Package Lists... ") );
	installed_pkgs = get_installed_pkgs();
	avail_pkgs = get_available_pkgs();
	printf( _("Done\n") );

	init_transaction(&tran);

	init_regex(&pkg_regex,PKG_LOG_PATTERN);

	for(i = 0; i < action_args->count; i++){

		/* Use regex to see if they specified a particular version */
		execute_regex(&pkg_regex,action_args->pkgs[i]);

		/* If so, parse it out and try to get that version only */
		if( pkg_regex.reg_return == 0 ){

			char pkg_name[NAME_LEN];
			char pkg_version[VERSION_LEN];

			if( (pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so) > NAME_LEN ){
				fprintf(stderr,"Package name exceeds NAME_LEN: %d\n",NAME_LEN);
				exit(1);
			}
			if( (pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so) > VERSION_LEN ){
				fprintf(stderr,"Package version exceeds NAME_LEN: %d\n",VERSION_LEN);
				exit(1);
			}

			strncpy(
				pkg_name,
				action_args->pkgs[i] + pkg_regex.pmatch[1].rm_so,
				pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
			);
			pkg_name[ pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so ] = '\0';

			strncpy(
				pkg_version,
				action_args->pkgs[i] + pkg_regex.pmatch[2].rm_so,
				pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
			);
			pkg_version[ pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so ] = '\0';

			pkg = get_exact_pkg(avail_pkgs, pkg_name, pkg_version);

			if( pkg == NULL ){
				fprintf(stderr,_("No such package: %s\n"),pkg_name);
				continue;
			}

			installed_pkg = get_newest_pkg(installed_pkgs,pkg_name);

		/* If regex doesnt match, find latest version of request */
		}else{
			/* make sure there is a package called action_args->pkgs[i] */
			pkg = get_newest_pkg(avail_pkgs,action_args->pkgs[i]);

			if( pkg == NULL ){
				fprintf(stderr,_("No such package: %s\n"),action_args->pkgs[i]);
				continue;
			}

			installed_pkg = get_newest_pkg(installed_pkgs,action_args->pkgs[i]);
		}

		/* if it is not already installed, install it */
		if( installed_pkg == NULL ){

				if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 ){
					/* this way we install the most up to date pkg */
					if ( is_conflicted(&tran,avail_pkgs,installed_pkgs,pkg) == 0 )
						add_install_to_transaction(&tran,pkg);
				}

		}else{ /* else we upgrade or reinstall */

			/* it is already installed, attempt an upgrade */
			if(
				((cmp_pkg_versions(installed_pkg->version,pkg->version)) < 0) ||
				(global_config->re_install == 1)
			){

				if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 ){
					if ( is_conflicted(&tran,avail_pkgs,installed_pkgs,pkg) == 0 )
						add_upgrade_to_transaction(&tran,installed_pkg,pkg);
				}

			}else{
				printf(_("%s is up to date.\n"),installed_pkg->name);
			}
	
		}

	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(avail_pkgs);

	free_regex(&pkg_regex);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
	return;
}

/* list pkgs */
void pkg_action_list(void){
	struct pkg_list *pkgs = NULL;
	struct pkg_list *installed_pkgs = NULL;
	int i;

	pkgs = get_available_pkgs();
	installed_pkgs = get_installed_pkgs();

	for(i = 0; i < pkgs->pkg_count; i++ ){
		/* this should eliminate the printing of updates */
		if( strstr(pkgs->pkgs[i]->description,"no description") == NULL ){
			int bool_installed = 0;
			char *short_description = gen_short_pkg_description(pkgs->pkgs[i]);

			/* is it installed? */
			if( get_exact_pkg(installed_pkgs,pkgs->pkgs[i]->name,pkgs->pkgs[i]->version) != NULL )
				bool_installed = 1;

			printf("%s %s [inst=%s]: %s\n",	
				pkgs->pkgs[i]->name,	
				pkgs->pkgs[i]->version,	
				bool_installed == 1
					? _("yes")
					: _("no"),
				short_description
			);
			free(short_description);
		}
	}

	free_pkg_list(pkgs);
	free_pkg_list(installed_pkgs);

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
	struct pkg_list *installed_pkgs;
	struct pkg_list *available;
	transaction tran;

	installed_pkgs = get_installed_pkgs();
	available = get_available_pkgs();
	init_transaction(&tran);

	for(i = 0; i < action_args->count; i++){
		if( (pkg = get_newest_pkg(installed_pkgs,action_args->pkgs[i])) != NULL){
			int c;
			struct pkg_list *deps;

			deps = is_required_by(global_config,available,pkg);

			for(c = 0; c < deps->pkg_count;c++){

				if( get_newest_pkg(installed_pkgs,deps->pkgs[c]->name) != NULL ){
					add_remove_to_transaction(&tran,deps->pkgs[c]);
				}

			}

			/* don't free, list of pointers to other packages that will be freed later */
			/* free_pkg_list(deps); */
			free(deps->pkgs);
			free(deps);

			add_remove_to_transaction(&tran,pkg);

		}else{
			printf(_("%s is not installed.\n"),action_args->pkgs[i]);
		}
	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(available);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
}

/* search for a pkg (support extended POSIX regex) */
void pkg_action_search(const char *pattern){
	int i;
	struct pkg_list *pkgs = NULL;
	struct pkg_list *installed_pkgs = NULL;
	struct pkg_list *matches = NULL;

	/* read in pkg data */
	pkgs = get_available_pkgs();
	installed_pkgs = get_installed_pkgs();

	matches = search_pkg_list(pkgs,pattern);

	for(i = 0; i < matches->pkg_count; i++){
		int bool_installed = 0;
		char *short_description = gen_short_pkg_description(matches->pkgs[i]);

		/* is it installed? */
		if( get_exact_pkg(installed_pkgs,matches->pkgs[i]->name,matches->pkgs[i]->version) != NULL )
			bool_installed = 1;

		printf("%s %s [inst=%s]: %s\n",	
			matches->pkgs[i]->name,	
			matches->pkgs[i]->version,	
			bool_installed == 1
				? _("yes")
				: _("no"),
			short_description
		);
		free(short_description);
	}

	/* don't free, list of pointers to other packages that will be freed later */
	/* free_pkg_list(matches) */
	free(matches->pkgs);
	free(matches);
	free_pkg_list(pkgs);
	free_pkg_list(installed_pkgs);

}/* end search */

/* show the details for a specific package */
void pkg_action_show(const char *pkg_name){
	pkg_info_t *pkg;
	struct pkg_list *avail_pkgs;
	struct pkg_list *installed_pkgs;
	sg_regex pkg_regex;
	int bool_installed = 0;

	avail_pkgs = get_available_pkgs();
	installed_pkgs = get_installed_pkgs();
	if( avail_pkgs == NULL || installed_pkgs == NULL ) exit(1);

	init_regex(&pkg_regex,PKG_LOG_PATTERN);

	/* Use regex to see if they specified a particular version */
	execute_regex(&pkg_regex,pkg_name);

	/* If so, parse it out and try to get that version only */
	if( pkg_regex.reg_return == 0 ){

		char p_name[NAME_LEN];
		char p_version[VERSION_LEN];

		if( (pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so) > NAME_LEN ){
			fprintf(stderr,"Package name exceeds NAME_LEN: %d\n",NAME_LEN);
			exit(1);
		}
		if( (pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so) > VERSION_LEN ){
			fprintf(stderr,"Package version exceeds NAME_LEN: %d\n",VERSION_LEN);
			exit(1);
		}

		strncpy(p_name,
			pkg_name + pkg_regex.pmatch[1].rm_so,
			pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
		);
		p_name[ pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so ] = '\0';

		strncpy(p_version,
			pkg_name + pkg_regex.pmatch[2].rm_so,
			pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
		);
		p_version[ pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so ] = '\0';

		pkg = get_exact_pkg(avail_pkgs, p_name, p_version);

	}else{
		pkg = get_newest_pkg(avail_pkgs,pkg_name);
	}


	if( pkg != NULL ){
	
		if( get_exact_pkg(installed_pkgs,pkg->name,pkg->version) != NULL)
			bool_installed = 1;

		printf(_("Package Name: %s\n"),pkg->name);
		printf(_("Package Mirror: %s\n"),pkg->mirror);
		printf(_("Package Location: %s\n"),pkg->location);
		printf(_("Package Version: %s\n"),pkg->version);
		printf(_("Package Size: %d K\n"),pkg->size_c);
		printf(_("Package Installed Size: %d K\n"),pkg->size_u);
		printf(_("Package Required: %s\n"),pkg->required);
		printf(_("Package Conflicts: %s\n"),pkg->conflicts);
		printf(_("Package MD5 Sum:  %s\n"),pkg->md5);
		printf(_("Package Description:\n"));
		printf("%s",pkg->description);
		printf(_("Package Installed: %s\n"),
			bool_installed == 1 
				? _("yes")
				: _("no")
		);
	
	}else{
		printf(_("No such package: %s\n"),pkg_name);
	}

	free_pkg_list(avail_pkgs);
	free_pkg_list(installed_pkgs);
	free_regex(&pkg_regex);
}

/* upgrade all installed pkgs with available updates */
void pkg_action_upgrade_all(const rc_config *global_config){
	int i;
	struct pkg_list *installed_pkgs;
	struct pkg_list *avail_pkgs;
	struct pkg_list *matches;
	pkg_info_t *update_pkg;
	pkg_info_t *installed_pkg;
	transaction tran;

	printf(_("Reading Package Lists... "));
	installed_pkgs = get_installed_pkgs();
	avail_pkgs = get_available_pkgs();
	if( avail_pkgs == NULL || installed_pkgs == NULL ) exit(1);
	printf(_("Done\n"));
	init_transaction(&tran);

	for(i = 0; i < installed_pkgs->pkg_count;i++){

		/* see if we have an available update for the pkg */
		update_pkg = get_newest_pkg(
			avail_pkgs,
			installed_pkgs->pkgs[i]->name
		);
		if( update_pkg != NULL ){
			int cmp_r;

			if( is_excluded(global_config,installed_pkgs->pkgs[i]) == 1 ){
				add_exclude_to_transaction(&tran,update_pkg);
				continue;
			}

			/* if the update has a newer version, attempt to upgrade */
			cmp_r = cmp_pkg_versions(installed_pkgs->pkgs[i]->version,update_pkg->version);
			if(
				/* either it's greater, or we want to reinstall */
				cmp_r < 0 || (global_config->re_install == 1) ||
				/* or this is a dist upgrade and the versions are the save except for the arch */
				(
					global_config->dist_upgrade == 1 &&
					cmp_r == 0 &&
					strcmp(installed_pkgs->pkgs[i]->version,update_pkg->version) != 0
				)
			){

				if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,update_pkg) == 0 ){
					if( is_excluded(global_config,update_pkg) == 1 ){
						add_exclude_to_transaction(&tran,update_pkg);
					}else{
						if ( is_conflicted(&tran,avail_pkgs,installed_pkgs,update_pkg) == 0 )
							add_upgrade_to_transaction(&tran,installed_pkgs->pkgs[i],update_pkg);
					}
				}

			}

		}/* end upgrade pkg found */

	}/* end for */

	if( global_config->dist_upgrade == 1 ){

		matches = search_pkg_list(avail_pkgs,SLACK_BASE_SET_REGEX);
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

					if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,matches->pkgs[i]) == 0 ){
						if ( is_conflicted(&tran,avail_pkgs,installed_pkgs,matches->pkgs[i]) == 0 )
							add_install_to_transaction(&tran,matches->pkgs[i]);
					}

				}
			/* even if it's installed, check to see that the packages are different */
			/* simply running a version comparison won't do it since sometimes the */
			/* arch is the only thing that changes */
			}else if(
				(cmp_pkg_versions(installed_pkg->version,matches->pkgs[i]->version) <= 0) &&
				strcmp(installed_pkg->version,matches->pkgs[i]->version) != 0
			){

				if( is_excluded(global_config,matches->pkgs[i]) == 1 ){
					add_exclude_to_transaction(&tran,matches->pkgs[i]);
				}else{
					if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,matches->pkgs[i]) == 0 ){
						if ( is_conflicted(&tran,avail_pkgs,installed_pkgs,matches->pkgs[i]) == 0 )
							add_upgrade_to_transaction(&tran,installed_pkg,matches->pkgs[i]);
					}
				}

			}

		}

		/* don't free, list of pointers to other packages that will be freed later */
		/* free_pkg_list(matches); */
		free(matches->pkgs);
		free(matches);
	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(avail_pkgs);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
}

