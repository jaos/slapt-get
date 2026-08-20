#include <stdint.h>
#include <sys/stat.h>

#include "common.h"

/*
 * Malformed PACKAGES.TXT corpus for slapt_parse_packages_txt().
 *
 * The fixtures encode hostile/broken lines: markers without a
 * colon, empty values, bare markers, out-of-range fields. They are
 * built in memory with fmemopen so no data files are needed.
 *
 * Built with ASan+UBSan: several of these segfault or read out of
 * bounds before the parser fixes (neither -ftrapv nor overflow-only
 * UBSan reports them).
 */

static slapt_vector_t *parse_record(const char *record)
{
    FILE *fh = fmemopen((void *)record, strlen(record), "r");
    if (fh == NULL) {
        return NULL;
    }

    slapt_vector_t *pkgs = slapt_parse_packages_txt(fh);
    fclose(fh);
    return pkgs;
}

/* a "PACKAGE MD5" line with no colon must not crash (strpbrk NULL)
 * and must be treated as "md5 not provided". */
START_TEST(test_md5_line_without_colon)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE MD5\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 1);
    ck_assert(((slapt_pkg_t *)pkgs->items[0])->md5[0] == '\0');
    slapt_vector_t_free(pkgs);
}
END_TEST

/* an empty PACKAGE MIRROR value (empty regex capture -> NULL)
 * must not crash and must leave the mirror unset (fillin defaults it
 * to ""). */
START_TEST(test_empty_mirror_value)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE MIRROR: \n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 1);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->mirror, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->location, "./x") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->required, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->conflicts, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->suggests, "") == 0);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* an empty PACKAGE MIRROR value (empty regex capture -> NULL)
 * must not crash and must leave the mirror unset (fillin defaults it
 * to ""). */
START_TEST(test_empty_required_conflicts_suggests_values)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE MIRROR: \n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE REQUIRED:  \n"
        "PACKAGE CONFLICTS:  \n"
        "PACKAGE SUGGESTS:  \n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 1);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->mirror, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->location, "./x") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->required, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->conflicts, "") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->suggests, "") == 0);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* an empty PACKAGE LOCATION value must not crash; since
 * location is mandatory the package is dropped cleanly. */
START_TEST(test_empty_location_value)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE LOCATION: \n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 0);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* a bare "PACKAGE REQUIRED" marker with no ":  <value>" must not
 * read past the end of the line; it is treated as "not provided". */
START_TEST(test_bare_required_marker)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE REQUIRED\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 1);
    ck_assert(strcmp(((slapt_pkg_t *)pkgs->items[0])->required, "") == 0);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* an out-of-range PRIORITY must not be honored as-is; the
 * package is dropped (the parser's "bad line -> drop package" style). */
START_TEST(test_huge_priority_rejected)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE PRIORITY:  99999999999999999999\n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  100 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 0);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* an out-of-range size is clamped to UINT32_MAX (pins behavior;
 * the .pkgdata lround(HUGE_VALF) UB is covered by the UBSan build). */
START_TEST(test_huge_size_clamped)
{
    const char *record =
        "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
        "PACKAGE LOCATION:  ./x\n"
        "PACKAGE SIZE (compressed):  99999999999999999999 K\n"
        "PACKAGE SIZE (uncompressed):  200 K\n"
        "PACKAGE DESCRIPTION:\n"
        " foo - a test package\n"
        "\n";

    slapt_vector_t *pkgs = parse_record(record);
    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 1);
    ck_assert(((slapt_pkg_t *)pkgs->items[0])->size_c == UINT32_MAX);
    slapt_vector_t_free(pkgs);
}
END_TEST

/* version ordering for numeric parts and build numbers that
 * overflow int. On this libc atoi() wraps rather than clamps, so
 * "2147483647" vs "2147483648" ordered inverted before the
 * slapt_version_num_cmp() fix; the 9999... pair only ordered
 * correctly via the strverscmp() fallback for clamped-equal parts.
 * The build comparison read the dash-prefixed string (atoi("-1")
 * == -1), which masked the inverted branches until the build
 * numbers overflowed int. */
START_TEST(test_version_ordering_overflow_parts)
{
    ck_assert(slapt_pkg_t_cmp_versions("99999999999999999998-x86-1", "99999999999999999999-x86-1") < 0);
    ck_assert(slapt_pkg_t_cmp_versions("99999999999999999999-x86-1", "99999999999999999998-x86-1") > 0);

    ck_assert(slapt_pkg_t_cmp_versions("2147483647-x86-1", "2147483648-x86-1") < 0);
    ck_assert(slapt_pkg_t_cmp_versions("2147483648-x86-1", "2147483647-x86-1") > 0);

    /* build number ordering */
    ck_assert(slapt_pkg_t_cmp_versions("1.0-x86-1", "1.0-x86-2") < 0);
    ck_assert(slapt_pkg_t_cmp_versions("1.0-x86-2", "1.0-x86-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("1.0-x86-2147483647", "1.0-x86-2147483648") < 0);

    /* documented: "1rob" and "1" compare equal */
    ck_assert(slapt_pkg_t_cmp_versions("1.0-x86-1rob", "1.0-x86-1") == 0);
}
END_TEST

/* a .pkgdata (package log) with a size that overflows strtof()
 * must not reach lround(HUGE_VALF) (UB); invalid sizes are left
 * unset (0) and warned about, valid ones still parse. Uses a
 * throwaway ROOT so the real /var/log/packages is never touched. */
START_TEST(test_huge_pkgdata_size_unknown)
{
    char log_file[512];
    char log_file2[512];
    snprintf(log_file, sizeof(log_file),
             "/tmp/slapt-malformed-pkgdata-root/var/log/packages/foo-1.0-i486-1.txz");
    snprintf(log_file2, sizeof(log_file2),
             "/tmp/slapt-malformed-pkgdata-root/var/log/packages/bar-1.0-i486-1.txz");

    /* create each ancestor directory (EEXIST is fine) */
    for (char *p = log_file; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(log_file, 0755);
            *p = '/';
        }
    }

    FILE *fh = fopen(log_file, "w");
    ck_assert(fh != NULL);
    fprintf(fh, "PACKAGE NAME:  foo-1.0-i486-1.txz\n"
                "COMPRESSED PACKAGE SIZE:  99999999999999999999 K\n"
                "UNCOMPRESSED PACKAGE SIZE:  200 K\n");
    fclose(fh);

    /* finite but above UINT32_MAX: a different branch into the same warning */
    fh = fopen(log_file2, "w");
    ck_assert(fh != NULL);
    fprintf(fh, "PACKAGE NAME:  bar-1.0-i486-1.txz\n"
                "COMPRESSED PACKAGE SIZE:  9999999999 K\n"
                "UNCOMPRESSED PACKAGE SIZE:  99999999999999999999 K\n");
    fclose(fh);

    setenv("ROOT", "/tmp/slapt-malformed-pkgdata-root", 1);
    slapt_vector_t *pkgs = slapt_get_installed_pkgs();
    unsetenv("ROOT");

    ck_assert(pkgs != NULL);
    ck_assert(pkgs->size == 2);
    const slapt_pkg_t *foo = NULL;
    const slapt_pkg_t *bar = NULL;
    slapt_vector_t_foreach (const slapt_pkg_t *, p, pkgs) {
        if (strcmp(p->name, "foo") == 0)
            foo = p;
        else
            bar = p;
    }
    ck_assert(foo != NULL && bar != NULL);
    ck_assert(foo->size_c == 0);
    ck_assert(foo->size_u == 200);
    ck_assert(bar->size_c == 0);
    ck_assert(bar->size_u == 0);
    slapt_vector_t_free(pkgs);

    remove(log_file);
    remove(log_file2);
    rmdir("/tmp/slapt-malformed-pkgdata-root/var/log/packages");
    rmdir("/tmp/slapt-malformed-pkgdata-root/var/log");
    rmdir("/tmp/slapt-malformed-pkgdata-root/var");
    rmdir("/tmp/slapt-malformed-pkgdata-root");
}
END_TEST

int main(void)
{
    int number_failed;

    Suite *s = suite_create("libslapt-malformed");
    TCase *tc = tcase_create("parse_packages_txt");
    suite_add_tcase(s, tc);

    tcase_add_test(tc, test_md5_line_without_colon);
    tcase_add_test(tc, test_empty_mirror_value);
    tcase_add_test(tc, test_empty_required_conflicts_suggests_values);
    tcase_add_test(tc, test_empty_location_value);
    tcase_add_test(tc, test_bare_required_marker);
    tcase_add_test(tc, test_huge_priority_rejected);
    tcase_add_test(tc, test_huge_size_clamped);
    tcase_add_test(tc, test_huge_pkgdata_size_unknown);
    tcase_add_test(tc, test_version_ordering_overflow_parts);

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);

    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
