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

/* this is the main download routine */
int download_data(FILE *,const char *);

/*
	this fills FILE with data from url, used for PACKAGES.TXT and CHECKSUMS
*/
int get_mirror_data_from_source(FILE *,const char *,const char *);

/* download pkg, cals download_data */
char *download_pkg(const rc_config *,pkg_info_t *);

/* callback for curl progress  */
int progress_callback(void *,double,double,double,double);
