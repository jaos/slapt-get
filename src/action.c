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

/* clean out local cache */
void pkg_action_clean(const rc_config *global_config){
	clean_pkg_dir(global_config->working_dir);
	chdir(global_config->working_dir);
}

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

	/* compile our regex */
	pkg_regex.nmatch = MAX_REGEX_PARTS;
	pkg_regex.reg_return = regcomp(&pkg_regex.regex, PKG_LOG_PATTERN, REG_EXTENDED|REG_NEWLINE);
	if( pkg_regex.reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, _("Failed to compile regex\n"));

		if( (regerror_size = regerror(pkg_regex.reg_return, &pkg_regex.regex,errbuf,errbuf_size)) ){
			printf(_("Regex Error: %s\n"),errbuf);
		}
		exit(1);
	}

	for(i = 0; i < action_args->count; i++){

		/* Use regex to see if they specified a particular version */
		pkg_regex.reg_return = regexec(&pkg_regex.regex,action_args->pkgs[i], pkg_regex.nmatch,pkg_regex.pmatch,0);

		/* If so, parse it out and try to get that version only */
		if( pkg_regex.reg_return == 0 ){

			pkg_info_t *tmp_pkg;
			tmp_pkg = malloc( sizeof *tmp_pkg );
			if( tmp_pkg == NULL ){
				fprintf(stderr,_("Failed to malloc %s\n"),"tmp_pkg");
				exit(1);
			}

			strncpy(
				tmp_pkg->name,
				action_args->pkgs[i] + pkg_regex.pmatch[1].rm_so,
				pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
			);

			tmp_pkg->name[ pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so ] = '\0';

			strncpy(
				tmp_pkg->version,
				action_args->pkgs[i] + pkg_regex.pmatch[2].rm_so,
				pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
			);

			tmp_pkg->version[ pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so ] = '\0';
			pkg = get_exact_pkg(avail_pkgs, tmp_pkg->name, tmp_pkg->version);

			if( pkg == NULL ){
				fprintf(stderr,_("No such package: %s\n"),tmp_pkg->name);
				continue;
			}

			installed_pkg = get_newest_pkg(installed_pkgs,tmp_pkg->name);

			free(tmp_pkg);

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
					/* make sure it is not already present from a dep check */
					if( search_transaction(&tran,pkg) == 0 ){

						if ( is_conflicted(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 )
							add_install_to_transaction(&tran,pkg);

					}
				}

		}else{ /* else we upgrade or reinstall */

			/* it is already installed, attempt an upgrade */
			if(
				((cmp_pkg_versions(installed_pkg->version,pkg->version)) < 0)
				|| (global_config->re_install == 1)
			){

				if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 ){
					/* make sure it is not already present from a dep check */
					if( search_transaction(&tran,pkg) == 0 ){

						if ( is_conflicted(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 )
							add_upgrade_to_transaction(&tran,installed_pkg,pkg);

					}
				}

			}else{
				printf(_("%s is up to date.\n"),installed_pkg->name);
			}
	
		}

	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(avail_pkgs);

	regfree(&pkg_regex.regex);

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

				/* if not already in the transaction, add if installed */
				if( search_transaction(&tran,deps->pkgs[c]) == 0 ){
					if( get_newest_pkg(installed_pkgs,deps->pkgs[c]->name) != NULL ){
						add_remove_to_transaction(&tran,deps->pkgs[c]);
					}
				}

			}

			free(deps->pkgs);
			free(deps);

			if( search_transaction(&tran,pkg) == 0 )
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
	matches = malloc( sizeof *matches );
	matches->pkgs = malloc( sizeof *matches->pkgs * pkgs->pkg_count );
	matches->pkg_count = 0;

	search_pkg_list(pkgs,matches,pattern);
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

	/* compile our regex */
	pkg_regex.nmatch = MAX_REGEX_PARTS;
	pkg_regex.reg_return = regcomp(&pkg_regex.regex, PKG_LOG_PATTERN, REG_EXTENDED|REG_NEWLINE);
	if( pkg_regex.reg_return != 0 ){
		size_t regerror_size;
		char errbuf[1024];
		size_t errbuf_size = 1024;
		fprintf(stderr, _("Failed to compile regex\n"));

		if( (regerror_size = regerror(pkg_regex.reg_return, &pkg_regex.regex,errbuf,errbuf_size)) ){
			printf(_("Regex Error: %s\n"),errbuf);
		}
		exit(1);
	}

	/* Use regex to see if they specified a particular version */
	pkg_regex.reg_return = regexec(&pkg_regex.regex,pkg_name, pkg_regex.nmatch,pkg_regex.pmatch,0);

	/* If so, parse it out and try to get that version only */
	if( pkg_regex.reg_return == 0 ){

		pkg_info_t *tmp_pkg;
		tmp_pkg = malloc( sizeof *tmp_pkg );
		if( tmp_pkg == NULL ){
			fprintf(stderr,_("Failed to malloc %s\n"),"tmp_pkg");
			exit(1);
		}

		strncpy(tmp_pkg->name,
			pkg_name + pkg_regex.pmatch[1].rm_so,
			pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
		);

		tmp_pkg->name[ pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so ] = '\0';

		strncpy(tmp_pkg->version,
			pkg_name + pkg_regex.pmatch[2].rm_so,
			pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
		);

		tmp_pkg->version[ pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so ] = '\0';
		pkg = get_exact_pkg(avail_pkgs, tmp_pkg->name, tmp_pkg->version);

		free(tmp_pkg);

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

			if( is_excluded(global_config,installed_pkgs->pkgs[i]) == 1 ){
				add_exclude_to_transaction(&tran,update_pkg);
				continue;
			}

			/* if the update has a newer version, attempt to upgrade */
			if( (cmp_pkg_versions(installed_pkgs->pkgs[i]->version,update_pkg->version)) < 0
				|| (global_config->re_install == 1)
			){

				if( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,update_pkg) == 0 ){
					/* add if it is not already present in trans */
					if( search_transaction(&tran,update_pkg) == 0 ){
						if( is_excluded(global_config,update_pkg) == 1 ){
							add_exclude_to_transaction(&tran,update_pkg);
						}else{

							if ( is_conflicted(global_config,&tran,avail_pkgs,installed_pkgs,update_pkg) == 0 )
								add_upgrade_to_transaction(&tran,installed_pkgs->pkgs[i],update_pkg);

						}
					}
				}

			}

		}/* end upgrade pkg found */

	}/* end for */

	if( global_config->dist_upgrade == 1 ){

		matches = malloc( sizeof *matches );
		matches->pkgs = malloc( sizeof *matches->pkgs * avail_pkgs->pkg_count );
		matches->pkg_count = 0;

		search_pkg_list(avail_pkgs,matches,SLACK_BASE_SET_REGEX);
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
						/* add if it is not already present in trans */
						if( search_transaction(&tran,matches->pkgs[i]) == 0 ){

							if ( is_conflicted(global_config,&tran,avail_pkgs,installed_pkgs,matches->pkgs[i]) == 0 )
								add_install_to_transaction(&tran,matches->pkgs[i]);

						}
					}

				}
			}

		}

		free(matches->pkgs);
		free(matches);
	}

	free_pkg_list(installed_pkgs);
	free_pkg_list(avail_pkgs);

	handle_transaction(global_config,&tran);

	free_transaction(&tran);
}

/* parse the dependencies for a package, and add them to the transaction as needed */
int add_deps_to_trans(const rc_config *global_config, transaction *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg){
	int c;
	struct pkg_list *deps;

	deps = lookup_pkg_dependencies(global_config,avail_pkgs,installed_pkgs,pkg);

	/* check to see if there where issues with dep checking */
	/* exclude the package if dep check barfed */
	if( (deps->pkg_count == -1) && (global_config->no_dep == 0) ){
		printf("Excluding %s, use --no-dep to override\n",pkg->name);
		add_exclude_to_transaction(tran,pkg);
		return 1;
	}

	/* loop through the deps */
	for(c = 0; c < deps->pkg_count;c++){

		/* only check if it is not already present in trans */
		if( search_transaction(tran,deps->pkgs[c]) == 0 ){

			pkg_info_t *dep_installed;

			if( (dep_installed = get_newest_pkg(installed_pkgs,deps->pkgs[c]->name)) == NULL ){

				if ( is_conflicted(global_config,tran,avail_pkgs,installed_pkgs,deps->pkgs[c]) == 0 )
					add_install_to_transaction(tran,deps->pkgs[c]);

			}else{

				/* add only if its a valid upgrade */
				if(cmp_pkg_versions(dep_installed->version,deps->pkgs[c]->version) < 0 ){
					if ( is_conflicted(global_config,tran,avail_pkgs,installed_pkgs,deps->pkgs[c]) == 0 )
						add_upgrade_to_transaction(tran,dep_installed,deps->pkgs[c]);

				}
			}

		}

	}

	free(deps->pkgs);
	free(deps);

	return 0;
}

int is_conflicted(const rc_config *global_config, transaction *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg){
	int i;
	int conflicted = 0;
	struct pkg_list *conflicts;

	/* if conflicts exist, check to see if they are installed or in the current transaction */
	conflicts = lookup_pkg_conflicts(global_config,avail_pkgs,installed_pkgs,pkg);
	for(i = 0; i < conflicts->pkg_count; i++){
		if(search_transaction(tran,conflicts->pkgs[i]) == 1){
			printf(_("%s, which is to be installed, conflicts with %s\n"),
				conflicts->pkgs[i]->name,conflicts->pkgs[i]->version, pkg->name,pkg->version
			);
			conflicted = 1;
		}
		if(get_newest_pkg(installed_pkgs,conflicts->pkgs[i]->name) != NULL) {
			printf(_("Installed %s conflicts with %s\n"),conflicts->pkgs[i]->name,pkg->name);
			conflicted = 1;
		}
	}

	free(conflicts->pkgs);
	free(conflicts);

	return conflicted;
}
