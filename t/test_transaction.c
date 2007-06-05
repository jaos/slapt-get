#include "test_transaction.h"
extern slapt_pkg_info_t pkg;


START_TEST (test_transaction)
{
  slapt_transaction_t *t = slapt_init_transaction();
  fail_if (t == NULL);

  slapt_add_install_to_transaction(t, &pkg);
  fail_unless (t->install_pkgs->pkg_count == 1);
  fail_unless (slapt_search_transaction(t, "gslapt") == 1);
  fail_unless (slapt_search_transaction_by_pkg(t, &pkg) == 1);
  t = slapt_remove_from_transaction(t, &pkg);

  slapt_add_remove_to_transaction(t, &pkg);
  fail_unless (t->remove_pkgs->pkg_count == 1);
  fail_unless (slapt_search_transaction(t, "gslapt") == 1);
  fail_unless (slapt_search_transaction_by_pkg(t, &pkg) == 1);
  t = slapt_remove_from_transaction(t, &pkg);

  slapt_add_exclude_to_transaction(t, &pkg);
  fail_unless (t->exclude_pkgs->pkg_count == 1);

  slapt_add_upgrade_to_transaction(t, &pkg, &pkg);
  /* fail_unless (slapt_search_upgrade_transaction(t, &pkg) == 1); */

  slapt_free_transaction(t);
}
END_TEST


START_TEST (test_handle_transaction)
{
  slapt_transaction_t *t = slapt_init_transaction();
  slapt_rc_config *rc     = slapt_read_rc_config("./data/rc1");

  /*
    download and install/remove/upgrade packages as defined in the transaction
    returns 0 on success
  int slapt_handle_transaction(const slapt_rc_config *,slapt_transaction_t *);
  */

  slapt_free_transaction(t);
  slapt_free_rc_config(rc);
}
END_TEST


START_TEST (test_transaction_dependencies)
{
  slapt_transaction_t *t  = slapt_init_transaction();
  slapt_rc_config *rc     = slapt_read_rc_config("./data/rc1");

  /*
    add dependencies for package to transaction, returns -1 on error, 0 otherwise
  int slapt_add_deps_to_trans(const slapt_rc_config *global_config,
                              slapt_transaction_t *tran,
                              struct slapt_pkg_list *avail_pkgs,
                              struct slapt_pkg_list *installed_pkgs, slapt_pkg_info_t *pkg);
  */

  /*
    check to see if a package has a conflict already present in the transaction
    returns conflicted package or NULL if none
  slapt_pkg_info_t *slapt_is_conflicted(slapt_transaction_t *tran,
                                        struct slapt_pkg_list *avail_pkgs,
                                        struct slapt_pkg_list *installed_pkgs,
                                        slapt_pkg_info_t *pkg);
  */

  /*
    generate a list of suggestions based on the current packages
    in the transaction
  void slapt_generate_suggestions(slapt_transaction_t *tran);
  */

  slapt_free_transaction(t);
  slapt_free_rc_config(rc);
}
END_TEST


Suite *transaction_test_suite()
{
  Suite *s = suite_create ("Transaction");
  TCase *tc = tcase_create ("Transaction");

  tcase_add_test (tc, test_transaction);
  tcase_add_test (tc, test_handle_transaction);
  tcase_add_test (tc, test_transaction_dependencies);

  suite_add_tcase (s, tc);
  return s;
}

