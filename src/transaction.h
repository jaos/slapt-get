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

struct _transaction {
	struct pkg_list *install_pkgs;
	struct pkg_upgrade_list *upgrade_pkgs;
	struct pkg_list *remove_pkgs;
	struct pkg_list *exclude_pkgs;
};
typedef struct _transaction transaction;

void init_transaction(transaction *);
int handle_transaction(const rc_config *,transaction *);
void add_install_to_transaction(transaction *,pkg_info_t *);
void add_remove_to_transaction(transaction *,pkg_info_t *);
void add_upgrade_to_transaction(transaction *,pkg_info_t *,pkg_info_t *);
void add_exclude_to_transaction(transaction *,pkg_info_t *);
int search_transaction(transaction *,pkg_info_t *);
void free_transaction(transaction *);

