
/* http://www.gnupg.org/documentation/manuals/gpgme/Largefile-Support-_0028LFS_0029.html#Largefile-Support-_0028LFS_0029
   we now need to define _FILE_OFFSET_BITS prior to including gpgme.h
   */
#define _FILE_OFFSET_BITS 64
#include <gpgme.h>

#define SLAPT_GPG_KEY "GPG-KEY"
#define SLAPT_CHECKSUM_ASC_FILE "CHECKSUMS.md5.asc"
#define SLAPT_CHECKSUM_ASC_FILE_GZ "CHECKSUMS.md5.gz.asc"

/* retrieve the signature of the CHECKSUMS.md5 file */
FILE *slapt_get_pkg_source_checksums_signature (const slapt_rc_config *global_config,
                                                const char *url,
                                                unsigned int *compressed);
/* retrieve the package sources GPG-KEY */
FILE *slapt_get_pkg_source_gpg_key(const slapt_rc_config *global_config,
                                   const char *url,
                                   unsigned int *compressed);
/* Add the GPG-KEY to the local keyring */
slapt_code_t slapt_add_pkg_source_gpg_key (FILE *key);
/* Verify the signature is valid for the checksum file */
slapt_code_t slapt_gpg_verify_checksums(FILE *checksums, FILE *signature);

