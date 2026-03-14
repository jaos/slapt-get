#include "common.h"
#include "test_common.h"
#include "test_configuration.h"
#include "test_curl.h"
#include "test_packages.h"
#include "test_transaction.h"

slapt_pkg_t pkg = {
    .md5 = "da018e032b5741ce4c51d8bc4d52a84b",
    .name = (char *)"slapt-get",
    .version = (char *)"0.11.11-x86_64-1",
    .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get/",
    .location = (char *)".", // NOTE: unusually '.' since we use the subdir in the location above
    .description = (char *)"slapt-get: slapt-get (an apt-like front-end to Slackware's pkgtools\n",
    .required = (char *)"",
    .conflicts = (char *)"",
    .suggests = (char *)"",
    .file_ext = (char *)".txz",
    .dependencies = NULL,
    .size_c = 160,
    .size_u = 810,
    .priority = SLAPT_PRIORITY_DEFAULT,
    .installed = true};

int _progress_cb(void *clientp, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow)
{
    (void)clientp;
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;
    return 0;
}

int main(void)
{
    int number_failed;

    Suite *s = suite_create("libslapt");
    SRunner *sr = srunner_create(s);

#ifdef SLAPT_HAS_GPGME
    gpgme_check_version(NULL);
#endif

    srunner_add_suite(sr, common_test_suite());
    srunner_add_suite(sr, configuration_test_suite());
    srunner_add_suite(sr, curl_test_suite());
    srunner_add_suite(sr, packages_test_suite());
    srunner_add_suite(sr, transaction_test_suite());

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);

    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
