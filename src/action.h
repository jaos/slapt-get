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

struct _pkg_action_args {
	char **pkgs;
	int count;
};
typedef struct _pkg_action_args pkg_action_args_t;

void pkg_action_clean(const rc_config *);
void pkg_action_install(const rc_config *,const pkg_action_args_t *);
void pkg_action_list(void);
void pkg_action_list_installed(void);
void pkg_action_remove(const rc_config *,const pkg_action_args_t *);
void pkg_action_search(const char *);
void pkg_action_show(const char *);
void pkg_action_upgrade_all(const rc_config *);
void pkg_action_update(const rc_config *);

