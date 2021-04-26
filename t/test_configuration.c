#include "test_configuration.h"

START_TEST(test_struct_config)
{
    slapt_config_t *rc = NULL;

    rc = slapt_config_t_init();
    ck_assert(rc != NULL);
    slapt_config_t_free(rc);
    rc = NULL;

    rc = slapt_config_t_read("./data/rc1");
    ck_assert(rc != NULL);
    {
        slapt_vector_t *s = rc->sources;
        slapt_vector_t *e = rc->exclude_list;

        ck_assert_int_ge(s->size, 0);
        ck_assert_int_eq(e->size, 5);
    }
    slapt_config_t_free(rc);
    rc = NULL;
}
END_TEST

START_TEST(test_working_dir)
{
    DIR *d = NULL;
    slapt_config_t *rc = slapt_config_t_read("data/rc1");

    /* check that working_dir exists or make it if permissions allow */
    /* void slapt_working_dir_init(const slapt_config_t *global_config); */

    slapt_working_dir_init(rc);

    d = opendir("data/slapt-get");
    ck_assert(d != NULL);
    closedir(d);
    rmdir("data/slapt-get");
    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_source_trimming)
{
    slapt_source_t *src1 = slapt_source_t_init("http://www.test.org/dist ");
    slapt_source_t *src2 = slapt_source_t_init("http://www.test.org/dist:PREFERRED ");

    ck_assert_str_eq(src1->url, "http://www.test.org/dist/");
    ck_assert_str_eq(src2->url, "http://www.test.org/dist/");

    slapt_source_t_free(src1);
    slapt_source_t_free(src2);
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
