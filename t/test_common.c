#include "test_common.h"

START_TEST(test_slapt_open_file)
{
    const char *file = "./PACKAGES.TXT";

    FILE *f = slapt_open_file(file, "w");
    ck_assert_int_ne(fileno(f), -1);

    fclose(f);
    unlink(file);
}
END_TEST

START_TEST(test_regex)
{
    slapt_regex_t *regex = slapt_regex_t_init("^[a-z]+$");
    const slapt_regex_t *invalid_regex = slapt_regex_t_init("^[-");

    ck_assert(regex != NULL);
    ck_assert(invalid_regex == NULL);

    slapt_regex_t_execute(regex, "abc");
    ck_assert(regex->reg_return != REG_NOMATCH);

    slapt_regex_t_execute(regex, "123");
    ck_assert(regex->reg_return == REG_NOMATCH);

    slapt_regex_t_free(regex);
}
END_TEST

START_TEST(test_slapt_create_dir_structure)
{
    const char *dir_name = "var/cache/slapt-get";
    slapt_create_dir_structure(dir_name);
    DIR *d = opendir(dir_name);
    ck_assert(d != NULL);
    closedir(d);

    rmdir("var/cache/slapt-get");
    rmdir("var/cache");
    rmdir("var");
}
END_TEST

START_TEST(test_slapt_gen_md5_sum_of_file)
{
    const char *file = "data/md5_dummy";
    char result_sum[SLAPT_MD5_STR_LEN + 1];
    FILE *f = fopen(file, "r");

    slapt_gen_md5_sum_of_file(f, result_sum);
    ck_assert(strcmp("96ee23abf2770468e1aac755a0a99809", result_sum) == 0);

    fclose(f);
}
END_TEST

START_TEST(test_slapt_str_replace_chr)
{
    const char *s = "/fake/path/to/file";
    char *s_modified = slapt_str_replace_chr(s, '/', '_');

    ck_assert(strcmp(s_modified, "_fake_path_to_file") == 0);

    free(s_modified);
}
END_TEST

int test_cmp_via_strcmp(const void *a, const void *b) {
    const char *sa = *(char **)a;
    const char *sb = *(char **)b;
    return strcmp(sa, sb);
}
START_TEST(test_slapt_vector_t)
{
    slapt_vector_t *v = slapt_vector_t_init(NULL);
    slapt_vector_t_add(v, "one");
    slapt_vector_t_add(v, "two");
    slapt_vector_t_add(v, "three");
    slapt_vector_t_add(v, "four");

    ck_assert(v->size == 4);
    ck_assert(v->capacity >= v->size);

    int idx = slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "three");
    ck_assert(idx == 2);

    slapt_vector_t_remove(v, v->items[idx]);
    ck_assert(v->size == 3);
    ck_assert(-1 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "three"));

    slapt_vector_t_sort(v, test_cmp_via_strcmp);
    ck_assert(0 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "four"));
    ck_assert(1 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "one"));
    ck_assert(2 == slapt_vector_t_index_of(v, (slapt_vector_t_cmp)strcmp, "two"));

    slapt_vector_t_free(v);
}
END_TEST

START_TEST(test_slapt_strip_whitespace)
{
    typedef struct {
        char *input;
        char *output;
    } std_test_case;

    const std_test_case tests[] = {
        {.input="foo", .output="foo"},
        {.input="foo ", .output="foo"},
        {.input=" foo ", .output="foo"},
    };
    for(uint32_t i = 0; i < (sizeof(tests)/sizeof(std_test_case)); i++) {
        std_test_case t = tests[i];
        char *output = slapt_strip_whitespace(t.input);
        ck_assert(strcmp(output, t.output) == 0);
        free(output);
    }
}
END_TEST

START_TEST(test_slapt_parse_delimited_list)
{
    slapt_vector_t *foo_v = slapt_vector_t_init(NULL);
    slapt_vector_t *foo_and_bar_v = slapt_vector_t_init(NULL);
    slapt_vector_t_add(foo_v, "foo");
    slapt_vector_t_add(foo_and_bar_v, "foo");
    slapt_vector_t_add(foo_and_bar_v, "bar");

     typedef struct {
        char *input;
        char delim;
        slapt_vector_t *output;
    } std_test_case;

    const std_test_case tests[] = {
        {.input="foo", .delim=',', .output=foo_v},
        {.input="foo,bar", .delim=',', .output=foo_and_bar_v},
    };
    for(uint32_t i = 0; i < (sizeof(tests)/sizeof(std_test_case)); i++) {
        std_test_case t = tests[i];
        slapt_vector_t *output = slapt_parse_delimited_list(t.input, t.delim);
        ck_assert_uint_eq (output->size, t.output->size);
        for(uint32_t c = 0; c < output->size; c++) {
            ck_assert_str_eq(output->items[c], t.output->items[c]);
        }
        slapt_vector_t_free(output);
    }

    slapt_vector_t_free(foo_v);
    slapt_vector_t_free(foo_and_bar_v);

}
END_TEST

Suite *common_test_suite(void)
{
    Suite *s = suite_create("Common");
    TCase *tc = tcase_create("Common");

    tcase_add_test(tc, test_slapt_open_file);
    tcase_add_test(tc, test_regex);
    tcase_add_test(tc, test_slapt_create_dir_structure);
    tcase_add_test(tc, test_slapt_gen_md5_sum_of_file);
    tcase_add_test(tc, test_slapt_str_replace_chr);
    tcase_add_test(tc, test_slapt_vector_t);
    tcase_add_test(tc, test_slapt_strip_whitespace);
    tcase_add_test(tc, test_slapt_parse_delimited_list);

    suite_add_tcase(s, tc);
    return s;
}
