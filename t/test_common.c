#include "test_common.h"

START_TEST(test_slapt_open_file)
{
    const char *file = "./PACKAGES.TXT";

    FILE *f = slapt_open_file(file, "w");
    fail_if(fileno(f) == -1, NULL);

    fclose(f);
    unlink(file);
}
END_TEST

START_TEST(test_regex)
{
    slapt_regex_t *regex = slapt_regex_t_init("^[a-z]+$");
    slapt_regex_t *invalid_regex = slapt_regex_t_init("^[-");

    fail_if(regex == NULL);
    fail_unless(invalid_regex == NULL);

    slapt_regex_t_execute(regex, "abc");
    fail_if(regex->reg_return == REG_NOMATCH);

    slapt_regex_t_execute(regex, "123");
    fail_unless(regex->reg_return == REG_NOMATCH);

    slapt_regex_t_free(regex);
}
END_TEST

START_TEST(test_slapt_create_dir_structure)
{
    const char *dir_name = "var/cache/slapt-get";
    DIR *d = NULL;

    slapt_create_dir_structure("var/cache/slapt-get");

    d = opendir(dir_name);

    fail_if(d == NULL);

    closedir(d);

    rmdir(dir_name);
    rmdir("var");
}
END_TEST

START_TEST(test_slapt_gen_md5_sum_of_file)
{
    const char *file = "data/md5_dummy";
    char result_sum[SLAPT_MD5_STR_LEN + 1];
    FILE *f = fopen(file, "r");

    slapt_gen_md5_sum_of_file(f, result_sum);
    fail_unless(strcmp("96ee23abf2770468e1aac755a0a99809", result_sum) == 0);

    fclose(f);
}
END_TEST

START_TEST(test_slapt_str_replace_chr)
{
    const char *s = "/fake/path/to/file";
    char *s_modified = slapt_str_replace_chr(s, '/', '_');

    fail_unless(strcmp(s_modified, "_fake_path_to_file") == 0);

    free(s_modified);
}
END_TEST

int test_cmp_via_strcmp(const void *a, const void *b) {
    char *sa = *(char **)a;
    char *sb = *(char **)b;
    return strcmp(sa, sb);
}
START_TEST(test_slapt_vector_t)
{
    slapt_vector_t *v = slapt_vector_t_init(NULL);
    slapt_vector_t_add(v, "one");
    slapt_vector_t_add(v, "two");
    slapt_vector_t_add(v, "three");
    slapt_vector_t_add(v, "four");

    fail_unless(v->size == 4);
    fail_unless(v->capacity >= v->size);

    int idx = slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "three");
    fail_unless(idx == 2);

    slapt_vector_t_remove(v, v->items[idx]);
    fail_unless(v->size == 3);
    fail_unless(-1 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "three"));

    slapt_vector_t_sort(v, test_cmp_via_strcmp);
    fail_unless(0 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "four"));
    fail_unless(1 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "one"));
    fail_unless(2 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "two"));

    slapt_vector_t_free(v);
}
END_TEST

Suite *common_test_suite()
{
    Suite *s = suite_create("Common");
    TCase *tc = tcase_create("Common");

    tcase_add_test(tc, test_slapt_open_file);
    tcase_add_test(tc, test_regex);
    tcase_add_test(tc, test_slapt_create_dir_structure);
    tcase_add_test(tc, test_slapt_gen_md5_sum_of_file);
    tcase_add_test(tc, test_slapt_str_replace_chr);
    tcase_add_test(tc, test_slapt_vector_t);

    suite_add_tcase(s, tc);
    return s;
}
