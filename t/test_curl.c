#include "test_curl.h"
extern slapt_pkg_t pkg;

extern int _progress_cb(void *clientp, double dltotal, double dlnow,
                        double ultotal, double ulnow);

START_TEST(test_slapt_get_mirror_data_from_source)
{
    FILE *f = NULL;
    const char *err = NULL;
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    slapt_source_t *src = rc->sources->items[0];
    const char *url = src->url;
    char *packages = "PACKAGES.TXT";
    char *packages_gz = "PACKAGES.TXT.gz";
    char *checksums = "CHECKSUMS.md5";
    char *checksums_gz = "CHECKSUMS.md5.gz";
    rc->progress_cb = _progress_cb; /* silence */

    f = slapt_open_file("data/PACKAGES.TXT", "w+b");
    err = slapt_get_mirror_data_from_source(f, rc, url, packages);
    ck_assert(!err);
    fclose(f);

    f = slapt_open_file("data/PACKAGES.TXT.gz", "w+b");
    err = slapt_get_mirror_data_from_source(f, rc, url, packages_gz);
    ck_assert(!err);
    fclose(f);

    f = slapt_open_file("data/CHECKSUMS.md5", "w+b");
    err = slapt_get_mirror_data_from_source(f, rc, url, checksums);
    ck_assert(!err);
    fclose(f);

    f = slapt_open_file("data/CHECKSUMS.md5.gz", "w+b");
    err = slapt_get_mirror_data_from_source(f, rc, url, checksums_gz);
    ck_assert(!err);
    fclose(f);

    unlink("data/PACKAGES.TXT");
    unlink("data/PACKAGES.TXT.gz");
    unlink("data/CHECKSUMS.md5");
    unlink("data/CHECKSUMS.md5.gz");

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_slapt_download_pkg)
{
    const char *err = NULL;
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    rc->progress_cb = _progress_cb; /* silence */
    slapt_working_dir_init(rc);

    err = slapt_download_pkg(rc, &pkg, NULL);
    ck_assert(!err);

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_head)
{
    char *head = slapt_head_request("http://www.google.com/");
    ck_assert(head != NULL);
    ck_assert(strstr(head, "Content-Type") != NULL);

    slapt_write_head_cache(head, "data/head_request");
    free(head);
    head = NULL;

    head = slapt_head_mirror_data("http://www.google.com/", "data/head_request");
    ck_assert(head == NULL);
    free(head);
    head = NULL;

    head = slapt_read_head_cache("data/head_request");
    ck_assert(head != NULL);
    free(head);

    slapt_clear_head_cache("data/head_request");
    unlink("data/head_request.head");
}
END_TEST

START_TEST(test_progress_data)
{
    struct slapt_progress_data *d = slapt_init_progress_data();
    ck_assert(d != NULL);
    ck_assert_int_eq(d->bytes, 0);
    ck_assert_int_ne(d->start, 0);
    slapt_free_progress_data(d);
}
END_TEST

Suite *curl_test_suite()
{
    Suite *s = suite_create("Curl");
    TCase *tc = tcase_create("Curl");

    tcase_add_test(tc, test_slapt_get_mirror_data_from_source);
    tcase_add_test(tc, test_slapt_download_pkg);
    tcase_add_test(tc, test_head);
    tcase_add_test(tc, test_progress_data);

    suite_add_tcase(s, tc);
    return s;
}
