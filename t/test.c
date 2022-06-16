#include "common.h"
#include "test_common.h"
#include "test_configuration.h"
#include "test_curl.h"
#include "test_packages.h"
#include "test_transaction.h"

slapt_pkg_t pkg = {
    .md5="8598a2a6d683d098b09cdc938de1e3c7",
    .name="gslapt",
    .version="0.3.15-i386-1",
    .mirror="http://software.jaos.org/slackpacks/11.0/",
    .location=".",
    .description="gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
    .required="",
    .conflicts="",
    .suggests="",
    .file_ext=".tgz",
    .dependencies=NULL,
    .size_c=115,
    .size_u=440,
    .priority=SLAPT_PRIORITY_DEFAULT,
    .installed=true};

int _progress_cb(void *clientp, double dltotal, double dlnow,
                 double ultotal, double ulnow)
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
