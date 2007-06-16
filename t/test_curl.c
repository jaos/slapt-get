#include "test_curl.h"
extern slapt_pkg_info_t pkg;


static int _progress_cb(void *clientp, double dltotal, double dlnow,
                        double ultotal, double ulnow)
{
  (void) clientp;
  (void) dltotal;
  (void) dlnow;
  (void) ultotal;
  (void) ulnow;
  return 0;
}

START_TEST (test_slapt_get_mirror_data_from_source)
{
  FILE *f             = NULL;
  const char *err     = NULL;
  slapt_rc_config *rc = slapt_read_rc_config("./data/rc1");
  const char *url     = "http://software.jaos.org/slackpacks/10.0/";
  char *packages      = "PACKAGES.TXT"; 
  char *packages_gz   = "PACKAGES.TXT.gz"; 
  char *checksums     = "CHECKSUMS.md5"; 
  char *checksums_gz  = "CHECKSUMS.md5.gz"; 
  rc->progress_cb     = _progress_cb; /* silence */

  f = slapt_open_file("data/PACKAGES.TXT","w+b");
  err = slapt_get_mirror_data_from_source(f, rc, url, packages);
  fail_if (err);
  fclose(f);

  f = slapt_open_file("data/PACKAGES.TXT.gz","w+b");
  err = slapt_get_mirror_data_from_source(f, rc, url, packages_gz);
  fail_if (err);
  fclose(f);

  f = slapt_open_file("data/CHECKSUMS.md5","w+b");
  err = slapt_get_mirror_data_from_source(f, rc, url, checksums);
  fail_if (err);
  fclose(f);

  f = slapt_open_file("data/CHECKSUMS.md5.gz","w+b");
  err = slapt_get_mirror_data_from_source(f, rc, url, checksums_gz);
  fail_if (err);
  fclose(f);

  unlink("data/PACKAGES.TXT");
  unlink("data/PACKAGES.TXT.gz");
  unlink("data/CHECKSUMS.md5");
  unlink("data/CHECKSUMS.md5.gz");

  slapt_free_rc_config(rc);
}
END_TEST


START_TEST (test_slapt_download_pkg)
{
  const char *err       = NULL;
  slapt_rc_config *rc   = slapt_read_rc_config("./data/rc1");
  rc->progress_cb       = _progress_cb; /* silence */
  slapt_working_dir_init(rc);

  err = slapt_download_pkg(rc, &pkg);
  fail_if (err);

  slapt_free_rc_config(rc);
}
END_TEST


START_TEST (test_head)
{
  char *head = slapt_head_request("http://www.google.com/");
  fail_if (head == NULL);
  fail_if ( strstr(head,"Content-Type") == NULL);

  slapt_write_head_cache(head, "data/head_request");
  free(head);
  head = NULL;

  head = slapt_head_mirror_data("http://www.google.com/","data/head_request");
  fail_unless (head == NULL);
  free(head);
  head = NULL;

  head = slapt_read_head_cache("data/head_request");
  fail_if (head == NULL);
  free(head);

  slapt_clear_head_cache("data/head_request");
  unlink("data/head_request.head");
}
END_TEST


START_TEST (test_progress_data)
{
  struct slapt_progress_data *d = slapt_init_progress_data();
  fail_if (d == NULL);
  fail_if (d->bytes != 0);
  fail_if (d->start == 0);
  slapt_free_progress_data(d);
}
END_TEST


Suite *curl_test_suite()
{
  Suite *s = suite_create ("Curl");
  TCase *tc = tcase_create ("Curl");

  tcase_add_test (tc, test_slapt_get_mirror_data_from_source);
  tcase_add_test (tc, test_slapt_download_pkg);
  tcase_add_test (tc, test_head);
  tcase_add_test (tc, test_progress_data);

  suite_add_tcase (s, tc);
  return s;
}

