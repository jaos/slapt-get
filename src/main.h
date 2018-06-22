/*
 * Copyright (C) 2003-2017 Jason Woodward <woodwardj at jaos dot org>
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
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
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
#include <zlib.h>
#include <utime.h>
#include <math.h>

#include "common.h"
#include "configuration.h"
#include "package.h"
#include "curl.h"
#include "transaction.h"
#include "action.h"
#ifdef SLAPT_HAS_GPGME
  #include "gpgme.h"
#endif

enum slapt_action {
  USAGE = 0, UPDATE, INSTALL, REMOVE, SHOW, SEARCH, UPGRADE,
  LIST, INSTALLED, CLEAN, SHOWVERSION, AUTOCLEAN, AVAILABLE,
  #ifdef SLAPT_HAS_GPGME
  ADD_KEYS,
  #endif
  INSTALL_DISK_SET,
  FILELIST
};

#define SLAPT_UPDATE_OPT 'u'
#define SLAPT_UPGRADE_OPT 'g'
#define SLAPT_INSTALL_OPT 'i'
#define SLAPT_INSTALL_DISK_SET_OPT 'D'
#define SLAPT_REMOVE_OPT 'r'
#define SLAPT_SHOW_OPT 's'
#define SLAPT_SEARCH_OPT 'e'
#define SLAPT_LIST_OPT 't'
#define SLAPT_INSTALLED_OPT 'd'
#define SLAPT_CLEAN_OPT 'c'
#define SLAPT_DOWNLOAD_ONLY_OPT 'o'
#define SLAPT_SIMULATE_OPT 'm'
#define SLAPT_VERSION_OPT 'v'
#define SLAPT_NO_PROMPT_OPT 'b'
#define SLAPT_PROMPT_OPT 'y'
#define SLAPT_REINSTALL_OPT 'n'
#define SLAPT_IGNORE_EXCLUDES_OPT 'x'
#define SLAPT_NO_MD5_OPT '5'
#define SLAPT_DIST_UPGRADE_OPT 'h'
#define SLAPT_HELP_OPT 'l'
#define SLAPT_IGNORE_DEP_OPT 'p'
#define SLAPT_NO_DEP_OPT 'q'
#define SLAPT_PRINT_URIS_OPT 'P'
#define SLAPT_SHOW_STATS_OPT 'S'
#define SLAPT_CONFIG_OPT 'C'
#define SLAPT_AUTOCLEAN_OPT 'a'
#define SLAPT_OBSOLETE_OPT 'O'
#define SLAPT_AVAILABLE_OPT 'A'
#define SLAPT_RETRY_OPT 'R'
#define SLAPT_NO_UPGRADE_OPT 'N'
#ifdef SLAPT_HAS_GPGME
  #define SLAPT_ADD_KEYS_OPT 'k'
  #define SLAPT_ALLOW_UNAUTH 'U'
#endif
#define SLAPT_FILELIST 'f'

#define SLAPT_DO_NOT_UNLINK_BAD_FILES 1
#define SLACKWARE_EXTRA_TESTING_PASTURE_WORKAROUND 1

