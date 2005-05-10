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

typedef struct {
  char **pkgs;
  unsigned int count;
} pkg_action_args_t;

pkg_action_args_t *init_pkg_action_args(int arg_count);
void free_pkg_action_args(pkg_action_args_t *paa);

void pkg_action_install(const rc_config *global_config,
                        const pkg_action_args_t *action_args);
void pkg_action_list(const int show);
void pkg_action_remove(const rc_config *global_config,
                       const pkg_action_args_t *action_args);
void pkg_action_search(const char *pattern);
void pkg_action_show(const char *pkg_name);
void pkg_action_upgrade_all(const rc_config *global_config);

