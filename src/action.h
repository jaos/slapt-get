/*
 * Copyright (C) 2003-2015 Jason Woodward <woodwardj at jaos dot org>
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

void slapt_pkg_action_install(const slapt_rc_config *global_config,
                              const slapt_list_t *action_args);
void slapt_pkg_action_list(const int show);
void slapt_pkg_action_remove(const slapt_rc_config *global_config,
                             const slapt_list_t *action_args);
void slapt_pkg_action_search(const char *pattern);
void slapt_pkg_action_show(const char *pkg_name);
void slapt_pkg_action_upgrade_all(const slapt_rc_config *global_config);

#ifdef SLAPT_HAS_GPGME
void slapt_pkg_action_add_keys(const slapt_rc_config *global_config);
#endif

void slapt_pkg_action_filelist( const char *pkg_name );

