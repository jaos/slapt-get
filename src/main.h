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

#ifndef _GNU_SOURCE
  #define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>
#include <regex.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <openssl/evp.h>
#include <libintl.h>
#include <locale.h>
#include <sys/statvfs.h>
#include <sys/mman.h>

#include "common.h"
#include "configuration.h"
#include "package.h"
#include "curl.h"
#include "transaction.h"
#include "action.h"

enum action {
  USAGE = 0, UPDATE, INSTALL, REMOVE, SHOW, SEARCH, UPGRADE,
  LIST, INSTALLED, CLEAN, SHOWVERSION, AUTOCLEAN, AVAILABLE
};

#define UPDATE_OPT 'u'
#define UPGRADE_OPT 'g'
#define INSTALL_OPT 'i'
#define REMOVE_OPT 'r'
#define SHOW_OPT 's'
#define SEARCH_OPT 'e'
#define LIST_OPT 't'
#define INSTALLED_OPT 'd'
#define CLEAN_OPT 'c'
#define DOWNLOAD_ONLY_OPT 'o'
#define SIMULATE_OPT 'm'
#define VERSION_OPT 'v'
#define NO_PROMPT_OPT 'b'
#define REINSTALL_OPT 'n'
#define IGNORE_EXCLUDES_OPT 'x'
#define NO_MD5_OPT '5'
#define DIST_UPGRADE_OPT 'h'
#define HELP_OPT 'l'
#define IGNORE_DEP_OPT 'p'
#define NO_DEP_OPT 'q'
#define PRINT_URIS_OPT 'P'
#define SHOW_STATS_OPT 'S'
#define CONFIG_OPT 'C'
#define AUTOCLEAN_OPT 'a'
#define OBSOLETE_OPT 'O'
#define AVAILABLE_OPT 'A'

#define DEBUG 0
#define DO_NOT_UNLINK_BAD_FILES 1

#define _(text) gettext(text)

