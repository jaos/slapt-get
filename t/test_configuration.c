#include "test_configuration.h"

START_TEST(test_struct_config)
{
    slapt_rc_config *rc = NULL;

    rc = slapt_init_config();
    fail_if(rc == NULL);
    slapt_free_rc_config(rc);
    rc = NULL;

    rc = slapt_read_rc_config("./data/rc1");
    fail_if(rc == NULL);
    {
        slapt_vector_t *s = rc->sources;
        slapt_vector_t *e = rc->exclude_list;

        fail_if(s->size < 1);
        fail_if(e->size != 5);
    }
    slapt_free_rc_config(rc);
    rc = NULL;
}
END_TEST

START_TEST(test_working_dir)
{
    DIR *d = NULL;
    slapt_rc_config *rc = slapt_read_rc_config("data/rc1");

    /* check that working_dir exists or make it if permissions allow */
    /* void slapt_working_dir_init(const slapt_rc_config *global_config); */

    slapt_working_dir_init(rc);

    d = opendir("data/slapt-get");
    fail_if(d == NULL);
    closedir(d);
    rmdir("data/slapt-get");
}
END_TEST

START_TEST(test_source_trimming)
{
    slapt_source_t *src1 = slapt_init_source("http://www.test.org/dist ");
    slapt_source_t *src2 = slapt_init_source("http://www.test.org/dist:PREFERRED ");

    fail_if(strcmp(src1->url, "http://www.test.org/dist/") != 0);
    fail_if(strcmp(src2->url, "http://www.test.org/dist/") != 0);

    slapt_free_source(src1);
    slapt_free_source(src2);
}
END_TEST

Suite *configuration_test_suite()
{
    Suite *s = suite_create("Configuration");
    TCase *tc = tcase_create("Configuration");

    tcase_add_test(tc, test_struct_config);
    tcase_add_test(tc, test_working_dir);
    tcase_add_test(tc, test_source_trimming);

    suite_add_tcase(s, tc);
    return s;
}
