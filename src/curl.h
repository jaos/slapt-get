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

#define USE_CURL_PROGRESS 0

/* FUNCTION DEFINITIONS */
void download_data(FILE *,const char *);
FILE *download_pkg_list(const rc_config *);
FILE *download_file_list(const rc_config *);
FILE *download_patches_list(const rc_config *);
char *download_pkg(const rc_config *,pkg_info_t *);
int progress_callback(void *,double,double,double,double);
