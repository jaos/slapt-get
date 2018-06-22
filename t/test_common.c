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
    slapt_regex_t *regex = slapt_init_regex("^[a-z]+$");
    slapt_regex_t *invalid_regex = slapt_init_regex("^[-");

    fail_if(regex == NULL);
    fail_unless(invalid_regex == NULL);

    slapt_execute_regex(regex, "abc");
    fail_if(regex->reg_return == REG_NOMATCH);

    slapt_execute_regex(regex, "123");
    fail_unless(regex->reg_return == REG_NOMATCH);

    slapt_free_regex(regex);
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
    char result_sum[SLAPT_MD5_STR_LEN];
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

Suite *common_test_suite()
{
    Suite *s = suite_create("Common");
    TCase *tc = tcase_create("Common");

    tcase_add_test(tc, test_slapt_open_file);
    tcase_add_test(tc, test_regex);
    tcase_add_test(tc, test_slapt_create_dir_structure);
    tcase_add_test(tc, test_slapt_gen_md5_sum_of_file);
    tcase_add_test(tc, test_slapt_str_replace_chr);

    suite_add_tcase(s, tc);
    return s;
}
