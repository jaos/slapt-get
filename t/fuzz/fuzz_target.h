#ifndef FUZZ_TARGET_H
#define FUZZ_TARGET_H

#include "../src/slapt.h"
#include <stddef.h>
#include <stdint.h>

#define SLAPT_FUZZ_CHECK(cond, ...) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "fuzz invariant failed: " __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            abort(); \
        } \
    } while (0)

static void slapt_fuzz_packages_txt(const uint8_t *data, size_t size)
{
    if (size == 0)
        return;

    FILE *fh = fmemopen((void *)(uintptr_t)data, size, "r");
    if (fh == NULL)
        return;

    slapt_vector_t *pkgs = slapt_parse_packages_txt(fh);
    fclose(fh);

    SLAPT_FUZZ_CHECK(pkgs != NULL, "slapt_parse_packages_txt returned NULL");

    slapt_vector_t_foreach (slapt_pkg_t *, pkg, pkgs) {
        SLAPT_FUZZ_CHECK(pkg->name != NULL, "package with NULL name");
        SLAPT_FUZZ_CHECK(pkg->version != NULL, "package with NULL version");
        SLAPT_FUZZ_CHECK(pkg->file_ext != NULL, "package with NULL file_ext");
        SLAPT_FUZZ_CHECK(pkg->location != NULL, "package with NULL location");
        SLAPT_FUZZ_CHECK(pkg->description != NULL, "package with NULL description");
        SLAPT_FUZZ_CHECK(pkg->mirror != NULL, "package with NULL mirror");
        SLAPT_FUZZ_CHECK(pkg->required != NULL, "package with NULL required");
        SLAPT_FUZZ_CHECK(pkg->conflicts != NULL, "package with NULL conflicts");
        SLAPT_FUZZ_CHECK(pkg->suggests != NULL, "package with NULL suggests");
        SLAPT_FUZZ_CHECK(pkg->dependencies != NULL, "package with NULL dependencies vector");
        SLAPT_FUZZ_CHECK(strlen(pkg->md5) <= SLAPT_MD5_STR_LEN,
                       "md5 longer than SLAPT_MD5_STR_LEN: %s", pkg->md5);
        SLAPT_FUZZ_CHECK(strstr(pkg->location, "..") == NULL,
                       "location contains '..': %s", pkg->location);
        SLAPT_FUZZ_CHECK(pkg->priority <= SLAPT_PRIORITY_CUSTOM_PATCH,
                       "priority out of range: %u", pkg->priority);
    }

    slapt_vector_t_free(pkgs);
}

#undef SLAPT_FUZZ_CHECK

#endif
