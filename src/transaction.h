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

#define MAX_LINE_LEN 80

struct suggests {
	char **pkgs;
	unsigned int count;
};

typedef struct {
	struct pkg_list *install_pkgs;
	struct pkg_upgrade_list *upgrade_pkgs;
	struct pkg_list *remove_pkgs;
	struct pkg_list *exclude_pkgs;
	struct suggests *suggests;
} transaction_t;

void init_transaction(transaction_t *);
int handle_transaction(const rc_config *,transaction_t *);
void add_install_to_transaction(transaction_t *,pkg_info_t *pkg);
void add_remove_to_transaction(transaction_t *,pkg_info_t *pkg);
void add_upgrade_to_transaction(transaction_t *,pkg_info_t *installed_pkg,pkg_info_t *upgrade_pkg);
void add_exclude_to_transaction(transaction_t *,pkg_info_t *pkg);
int search_transaction(transaction_t *,pkg_info_t *pkg);
void free_transaction(transaction_t *);
transaction_t *remove_from_transaction(transaction_t *tran,pkg_info_t *pkg);
int add_deps_to_trans(const rc_config *global_config, transaction_t *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg);
/* check to see if a package is conflicted */
pkg_info_t *is_conflicted(transaction_t *tran, struct pkg_list *avail_pkgs, struct pkg_list *installed_pkgs, pkg_info_t *pkg);

