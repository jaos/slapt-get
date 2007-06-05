#include "test_configuration.h"


START_TEST (test_struct_config)
{
  slapt_rc_config *rc = NULL;

  rc = slapt_init_config();
  fail_if (rc == NULL);
  slapt_free_rc_config(rc);
  rc = NULL;

  rc = slapt_read_rc_config("./data/rc1");
  fail_if (rc == NULL);
  {
    struct slapt_source_list *s = rc->sources;
    struct slapt_exclude_list *e = rc->exclude_list;

    fail_if (s->count < 1);
    fail_if (e->count != 8);
  }
  slapt_free_rc_config(rc);
  rc = NULL;

}
END_TEST


START_TEST (test_working_dir)
{
  DIR *d              = NULL;
  slapt_rc_config *rc = slapt_read_rc_config("data/rc1");

  /* check that working_dir exists or make it if permissions allow */
  /* void slapt_working_dir_init(const slapt_rc_config *global_config); */

  slapt_working_dir_init(rc);

  d = opendir("data/slapt-get");
  fail_if (d == NULL);
  closedir(d);
  rmdir("data/slapt-get");

}
END_TEST


START_TEST (test_exclude_list)
{
  struct slapt_exclude_list *e = slapt_init_exclude_list();

  fail_if (e == NULL);
  fail_if (e->count != 0);

  slapt_add_exclude(e,"^foo$");
  fail_if (e->count != 1);

  slapt_remove_exclude(e,"^foo$");
  fail_if (e->count != 0);

  slapt_remove_exclude(e,"no_such_exclude");
  fail_if (e->count != 0);

  slapt_free_exclude_list(e);
}
END_TEST


START_TEST (test_source_list)
{
  struct slapt_source_list *s = slapt_init_source_list();
  fail_if (s == NULL);
  fail_if (s->count != 0);

  slapt_add_source(s,"http://www.test.org/dist/");
  fail_if (s->count != 1);

  slapt_remove_source (s,"http://www.test.org/dist/");
  fail_if (s->count != 0);

  slapt_free_source_list(s);
}
END_TEST




Suite *configuration_test_suite()
{
  Suite *s = suite_create ("Configuration");
  TCase *tc = tcase_create ("Configuration");

  tcase_add_test (tc, test_struct_config);
  tcase_add_test (tc, test_working_dir);
  tcase_add_test (tc, test_exclude_list);
  tcase_add_test (tc, test_source_list);

  suite_add_tcase (s, tc);
  return s;
}

