/* Fuzz known community package sources */
#include "common.h"
#include "fuzz_target.h"

struct live_source {
    const char *name;
    const char *base_url;
};

static const struct live_source live_sources[] = {
    { "osuosl-15.0",         "https://slackware.osuosl.org/slackware64-15.0/" },
    { "osuosl-15.0-testing", "https://slackware.osuosl.org/slackware64-15.0/testing/" },
    { "osuosl-15.0-extra",   "https://slackware.osuosl.org/slackware64-15.0/extra/" },
    { "osuosl-15.0-pasture", "https://slackware.osuosl.org/slackware64-15.0/pasture/" },
    { "jaos-15.0",           "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/" },
    { "alien-slackbuilds",   "https://slackware.nl/people/alien/slackbuilds/" },
    { "alien-restricted",    "https://slackware.nl/people/alien/restricted_slackbuilds/" },
    { "alien-multilib",      "https://slackware.nl/people/alien/multilib/" },
};

static int silence_progress_cb(void *clientp, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)
{
    (void)clientp;
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;
    return 0;
}

START_TEST(test_live_pkg_sources)
{
    slapt_config_t *rc = slapt_config_t_init();
    rc->progress_cb = silence_progress_cb;

    for (size_t i = 0; i < sizeof(live_sources) / sizeof(live_sources[0]); i++) {
        const struct live_source *src = &live_sources[i];
        char *buffer = NULL;
        size_t size = 0;
        const char *err = NULL;

        for (int attempt = 1; attempt <= 3; attempt++) {
            free(buffer);
            buffer = NULL;
            size = 0;
            FILE *fh = open_memstream(&buffer, &size);
            if (fh == NULL) {
                err = "open_memstream failed";
                break;
            }
            err = slapt_get_mirror_data_from_source(fh, rc, src->base_url, "PACKAGES.TXT");
            fclose(fh);
            if (err == NULL)
                break;
            fprintf(stderr, "%s: attempt %d/3 failed: %s\n", src->name, attempt, err);
        }
        ck_assert_msg(err == NULL, "%s: all 3 attempts failed", src->name);

        slapt_fuzz_packages_txt((const uint8_t *)buffer, size);

        /* the target frees its own parse; parse again for the count */
        FILE *fh = fmemopen((void *)(uintptr_t)buffer, size, "r");
        ck_assert(fh != NULL);
        slapt_vector_t *pkgs = slapt_parse_packages_txt(fh);
        fclose(fh);
        ck_assert_msg(pkgs != NULL && pkgs->size > 0, "%s: parsed no packages", src->name);
        slapt_vector_t_free(pkgs);
        free(buffer);
    }

    slapt_config_t_free(rc);
}
END_TEST

int main(void)
{
    int number_failed;

    Suite *s = suite_create("live-pkg-sources");
    TCase *tc = tcase_create("fetch");
    /* network bound: no libcheck timeout, the meson timeout is the bound */
    tcase_set_timeout(tc, 0);
    tcase_add_test(tc, test_live_pkg_sources);
    suite_add_tcase(s, tc);

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
