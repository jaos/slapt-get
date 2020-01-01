/*
 * Copyright (C) 2003-2020 Jason Woodward <woodwardj at jaos dot org>
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

/* http://www.gnupg.org/documentation/manuals/gpgme/Largefile-Support-_0028LFS_0029.html#Largefile-Support-_0028LFS_0029
   we now need to define _FILE_OFFSET_BITS prior to including gpgme.h
   */
#define _FILE_OFFSET_BITS 64
#include <gpgme.h>

#define SLAPT_GPG_KEY "GPG-KEY"
#define SLAPT_CHECKSUM_ASC_FILE "CHECKSUMS.md5.asc"
#define SLAPT_CHECKSUM_ASC_FILE_GZ "CHECKSUMS.md5.gz.asc"

/* retrieve the signature of the CHECKSUMS.md5 file */
FILE *slapt_get_pkg_source_checksums_signature(const slapt_config_t *global_config, const char *url, bool *compressed);
/* retrieve the package sources GPG-KEY */
FILE *slapt_get_pkg_source_gpg_key(const slapt_config_t *global_config, const char *url, bool *compressed);
/* Add the GPG-KEY to the local keyring */
slapt_code_t slapt_add_pkg_source_gpg_key(FILE *key);
/* Verify the signature is valid for the checksum file */
slapt_code_t slapt_gpg_verify_checksums(FILE *checksums, FILE *signature);
