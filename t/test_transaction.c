#include "test_transaction.h"
extern slapt_pkg_t pkg;

START_TEST(test_transaction)
{
    slapt_transaction_t *t = slapt_init_transaction();
    fail_if(t == NULL);

    slapt_add_install_to_transaction(t, &pkg);
    fail_unless(t->install_pkgs->size == 1);
    fail_unless(slapt_search_transaction(t, "gslapt"));
    fail_unless(slapt_search_transaction_by_pkg(t, &pkg));
    t = slapt_remove_from_transaction(t, &pkg);

    slapt_add_remove_to_transaction(t, &pkg);
    fail_unless(t->remove_pkgs->size == 1);
    fail_unless(slapt_search_transaction(t, "gslapt"));
    fail_unless(slapt_search_transaction_by_pkg(t, &pkg));
    t = slapt_remove_from_transaction(t, &pkg);

    slapt_add_exclude_to_transaction(t, &pkg);
    fail_unless(t->exclude_pkgs->size == 1);

    slapt_add_upgrade_to_transaction(t, &pkg, &pkg);
    /* fail_unless (slapt_search_upgrade_transaction(t, &pkg)); */

    slapt_free_transaction(t);
}
END_TEST

START_TEST(test_handle_transaction)
{
    slapt_transaction_t *t = slapt_init_transaction();
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    /*
    download and install/remove/upgrade packages as defined in the transaction
    returns 0 on success
  int slapt_handle_transaction(const slapt_config_t *,slapt_transaction_t *);
  */

    slapt_free_transaction(t);
    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_transaction_dependencies)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    FILE *fh = fopen("data/avail_deps", "r");
    fail_unless(fh != NULL);
    slapt_vector_t *avail = slapt_parse_packages_txt(fh);
    fclose(fh);

    fh = fopen("data/installed_deps", "r");
    fail_unless(fh != NULL);
    slapt_vector_t *installed = slapt_parse_packages_txt(fh);
    fclose(fh);

    slapt_transaction_t *t = slapt_init_transaction();

    /*
    add dependencies for package to transaction, returns -1 on error, 0 otherwise
  int slapt_add_deps_to_trans(const slapt_config_t *global_config,
                              slapt_transaction_t *tran,
                              struct slapt_pkg_list *avail_pkgs,
                              struct slapt_pkg_list *installed_pkgs, slapt_pkg_t *pkg);
  */

    /*
    check to see if a package has a conflict already present in the transaction
    returns conflicted package or NULL if none
  slapt_pkg_t *slapt_is_conflicted(slapt_transaction_t *tran,
                                        struct slapt_pkg_list *avail_pkgs,
                                        struct slapt_pkg_list *installed_pkgs,
                                        slapt_pkg_t *pkg);
  */

    /*
    generate a list of suggestions based on the current packages
    in the transaction
  void slapt_generate_suggestions(slapt_transaction_t *tran);
  */
    slapt_pkg_t *p = slapt_get_newest_pkg(avail, "scim");
    slapt_pkg_t *installed_p = slapt_get_newest_pkg(installed, "scim");
    (void)installed_p;

    slapt_vector_t *conflicts = slapt_is_conflicted(t, avail, installed, p);
    if (conflicts->size > 0) {
        slapt_add_install_to_transaction(t, conflicts->items[0]);
        slapt_add_deps_to_trans(rc, t, avail, installed, conflicts->items[0]);
    }

    slapt_add_deps_to_trans(rc, t, avail, installed, p);
    slapt_add_install_to_transaction(t, p);

    slapt_generate_suggestions(t);

    slapt_free_transaction(t);
    slapt_config_t_free(rc);
}
END_TEST

Suite *transaction_test_suite()
{
    Suite *s = suite_create("Transaction");
    TCase *tc = tcase_create("Transaction");

    tcase_add_test(tc, test_transaction);
    tcase_add_test(tc, test_handle_transaction);
    tcase_add_test(tc, test_transaction_dependencies);

    suite_add_tcase(s, tc);
    return s;
}
