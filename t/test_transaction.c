#include "test_transaction.h"
extern slapt_pkg_t pkg;

START_TEST(test_transaction)
{
    slapt_transaction_t *t = slapt_transaction_t_init();
    ck_assert(t != NULL);

    slapt_transaction_t_add_install(t, &pkg);
    ck_assert(t->install_pkgs->size == 1);
    ck_assert(slapt_transaction_t_search(t, "slapt-get"));
    ck_assert(slapt_transaction_t_search_by_pkg(t, &pkg));
    t = slapt_transaction_t_remove(t, &pkg);

    slapt_transaction_t_add_remove(t, &pkg);
    ck_assert(t->remove_pkgs->size == 1);
    ck_assert(slapt_transaction_t_search(t, "slapt-get"));
    ck_assert(slapt_transaction_t_search_by_pkg(t, &pkg));
    t = slapt_transaction_t_remove(t, &pkg);

    slapt_transaction_t_add_exclude(t, &pkg);
    ck_assert(t->exclude_pkgs->size == 1);

    slapt_transaction_t_add_upgrade(t, &pkg, &pkg);
    /* ck_assert (slapt_transaction_t_search_upgrade(t, &pkg)); */

    slapt_transaction_t_free(t);
}
END_TEST

START_TEST(test_handle_transaction)
{
    slapt_transaction_t *t = slapt_transaction_t_init();
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    slapt_transaction_t_free(t);
    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_transaction_dependencies)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    FILE *fh = fopen("data/avail_deps", "r");
    ck_assert(fh != NULL);
    slapt_vector_t *avail = slapt_parse_packages_txt(fh);
    fclose(fh);

    fh = fopen("data/installed_deps", "r");
    ck_assert(fh != NULL);
    slapt_vector_t *installed = slapt_parse_packages_txt(fh);
    fclose(fh);

    slapt_transaction_t *t = slapt_transaction_t_init();

    const slapt_pkg_t *p = slapt_get_newest_pkg(avail, "scim");
    slapt_pkg_t *installed_p = slapt_get_newest_pkg(installed, "scim");
    ck_assert(installed_p == NULL);

    slapt_vector_t *conflicts = slapt_transaction_t_find_conflicts(t, avail, installed, p);
    if (conflicts->size > 0) {
        slapt_transaction_t_add_install(t, conflicts->items[0]);
        slapt_transaction_t_add_dependencies(rc, t, avail, installed, conflicts->items[0]);
    }
    slapt_vector_t_free(conflicts);

    slapt_transaction_t_add_dependencies(rc, t, avail, installed, p);
    slapt_transaction_t_add_install(t, p);

    slapt_transaction_t_suggestions(t);

    slapt_transaction_t_free(t);
    slapt_config_t_free(rc);
    slapt_vector_t_free(avail);
    slapt_vector_t_free(installed);
}
END_TEST

Suite *transaction_test_suite(void)
{
    Suite *s = suite_create("Transaction");
    TCase *tc = tcase_create("Transaction");

    tcase_add_test(tc, test_transaction);
    tcase_add_test(tc, test_handle_transaction);
    tcase_add_test(tc, test_transaction_dependencies);

    suite_add_tcase(s, tc);
    return s;
}
