/*
 * Copyright (C) 2004 Jason Woodward <woodwardj at jaos dot org>
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

#define _GNU_SOURCE
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

#include "common.h"
#include "configuration.h"
#include "package.h"
#include "curl.h"
#include "transaction.h"
#include "action.h"

enum action {
	USAGE = 0, UPDATE, INSTALL, REMOVE, SHOW, SEARCH, UPGRADE,
	LIST, INSTALLED, CLEAN, SHOWVERSION, AUTOCLEAN
};

#define DEBUG 0
#define DO_NOT_UNLINK_BAD_FILES 1

#define _(text) gettext(text)

