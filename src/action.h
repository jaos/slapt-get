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
/* FUNCTION DEFINITIONS */
void jaospkg_clean(const rc_config *);
void jaospkg_install(const rc_config *,const char *);
void jaospkg_list(void);
void jaospkg_list_installed(void);
void jaospkg_remove(const char *);
void jaospkg_search(const char *);
void jaospkg_show(const char *);
void jaospkg_upgrade(const rc_config *,pkg_info *);
void jaospkg_upgrade_all(const rc_config *);
void jaospkg_update(const rc_config *);

