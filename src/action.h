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

struct _pkg_action_args {
	char **pkgs;
	int count;
};
typedef struct _pkg_action_args pkg_action_args_t;

void pkg_action_install(const rc_config *global_config,const pkg_action_args_t *action_args);
void pkg_action_list(void);
void pkg_action_list_installed(void);
void pkg_action_remove(const rc_config *global_config,const pkg_action_args_t *action_args);
void pkg_action_search(const char *pattern);
void pkg_action_show(const char *pkg_name);
void pkg_action_upgrade_all(const rc_config *global_config);
int add_deps_to_trans(const rc_config *global_config, transaction *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg);
/* check to see if a package is conflicted */
int is_conflicted(transaction *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg);

