#include "test_packages.h"
extern slapt_pkg_t pkg;

extern int _progress_cb(void *clientp, off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow);

START_TEST(test_struct_pkg)
{
    slapt_pkg_t *cpy = NULL;
    cpy = slapt_pkg_t_copy(cpy, &pkg);
    ck_assert(cpy != NULL);

    {
        char *pkg_str = slapt_pkg_t_string(&pkg);
        char *cpy_str = slapt_pkg_t_string(cpy);

        ck_assert(pkg_str != NULL);
        ck_assert(cpy_str != NULL);
        ck_assert(strcmp(pkg_str, cpy_str) == 0);

        free(pkg_str);
        free(cpy_str);
    }

    slapt_pkg_t_free(cpy);
}
END_TEST

START_TEST(test_pkg_info)
{
    size_t i = 0;
    char *string = NULL;
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    slapt_working_dir_init(rc);

    string = slapt_pkg_t_short_description(&pkg);
    const char *expected_desc = "slapt-get (an apt-like front-end to Slackware's pkgtools";
    ck_assert(string != NULL);
    ck_assert_msg(strcmp(string, expected_desc) == 0, "failed: %s != %s\n", string, expected_desc);
    free(string);
    string = NULL;

    string = slapt_gen_filename_from_url(rc, "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", "PACKAGES.TXT");
    {
        const size_t expected_len = strlen(rc->working_dir) + strlen("/.https:##storage.googleapis.com#slackpacks.jaos.org#slackware64-15.0#PACKAGES.TXT") + 1;
        char expected_filename[expected_len];
        snprintf(expected_filename, expected_len, "%s/.https:##storage.googleapis.com#slackpacks.jaos.org#slackware64-15.0#PACKAGES.TXT", rc->working_dir);
        ck_assert(string != NULL);
        ck_assert_msg(strcmp(string, expected_filename) == 0, "failed: %s != %s\n", string, expected_filename);
    }
    free(string);
    string = NULL;

    string = slapt_gen_head_cache_filename(".https:##storage.googleapis.com#slackpacks.jaos.org#slackware64-15.0#PACKAGES.TXT");
    const char *expected_cache_filename = ".https:##storage.googleapis.com#slackpacks.jaos.org#slackware64-15.0#PACKAGES.TXT.head";
    ck_assert(string != NULL);
    ck_assert_msg(strcmp(string, expected_cache_filename) == 0, "failed: %s != %s\n", string, expected_cache_filename);
    free(string);
    string = NULL;

    struct pkg_url_tests {
        slapt_pkg_t pkg;
        const char *url;
    } url_tests[] = {
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)".", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"./", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"./slapt-get", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"./slapt-get/", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"slapt-get/", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get/slapt-get-0.11.11-x86_64-1.txz",
        },
        {
            {.name = (char *)"slapt-get", .version = (char *)"0.11.11-x86_64-1", .mirror = (char *)"https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/", .location = (char *)"slapt-get", .file_ext = (char *)".txz"},
            "https://storage.googleapis.com/slackpacks.jaos.org/slackware64-15.0/slapt-get/slapt-get-0.11.11-x86_64-1.txz",
        },
    };

    for (size_t idx = 0; idx < sizeof(url_tests) / sizeof(struct pkg_url_tests); idx++) {
        const slapt_pkg_t *tpkg = &url_tests[idx].pkg;
        const char *turl = url_tests[idx].url;

        char *url = slapt_pkg_t_url(tpkg);
        ck_assert(url != NULL);
        ck_assert_msg(strcmp(url, turl) == 0, "failed: [%zu] %s != %s\n", idx, url, turl);
        free(url);
    }

    slapt_vector_t_add(rc->exclude_list, strdup("^slapt-get$"));
    ck_assert_msg(slapt_is_excluded(rc, &pkg), "failed: test package is not excluded");
    slapt_vector_t_remove(rc->exclude_list, "^slapt-get$");

    const char *dlerr = slapt_download_pkg(rc, &pkg, NULL);
    ck_assert_msg(!dlerr, "failed: unsuccessful download\n");
    ck_assert_msg(slapt_verify_downloaded_pkg(rc, &pkg) == SLAPT_OK, "failed: unsuccessful verify of downloaded package\n");

    i = slapt_get_pkg_file_size(rc, &pkg);
    ck_assert_msg(i >= pkg.size_c, "%zu is not >= %d\n", i, pkg.size_c);

    string = slapt_pkg_t_clean_description(&pkg);
    const char *expected_clean_desc = "";
    ck_assert_msg(strcmp(string, expected_clean_desc) != 0, "failed: %s != %s\n", string, expected_clean_desc);
    free(string);
    string = NULL;

    /* retrieve the packages changelog entry, if any.  Returns NULL otherwise */
    /* char *slapt_pkg_t_changelog(const slapt_pkg_t *pkg); */

    /* get the package filelist, returns (char *) on success or NULL on error */
    /* char *slapt_pkg_t_filelist(const slapt_pkg_t *pkg); */

    ck_assert(pkg.priority == SLAPT_PRIORITY_DEFAULT);
    ck_assert(strcmp(slapt_priority_to_str(pkg.priority), gettext("Default")) == 0);

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_pkg_search)
{
    slapt_pkg_t *p = NULL;
    slapt_vector_t *l = NULL;
    slapt_vector_t *list = slapt_vector_t_init(NULL);
    slapt_vector_t_add(list, &pkg);

    p = slapt_get_newest_pkg(list, "slapt-get");
    ck_assert(p != NULL);

    p = slapt_get_exact_pkg(list, "slapt-get", "0.11.11-x86_64-1");
    ck_assert(p != NULL);

    p = slapt_get_pkg_by_details(list, "slapt-get", "0.11.11-x86_64-1", ".");
    ck_assert(p != NULL);

    l = slapt_search_pkg_list(list, "^slapt-get$");
    ck_assert(l != NULL);
    ck_assert_msg(l->size > 0, "failed, search returned empty\n");
    slapt_vector_t_free(l);

    slapt_vector_t_free(list);
}
END_TEST

START_TEST(test_pkgtool)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    /* disabled... */
    /*
  int r               = -1;
  r = slapt_install_pkg(rc, &pkg);
  r = slapt_upgrade_pkg(rc, &pkg);
  r = slapt_remove_pkg(rc, &pkg);
  */

    slapt_config_t_free(rc);
}
END_TEST

/*
http://software.jaos.org/pipermail/slapt-get-devel/2008-November/000762.html

* When comparing two packages on mirrors:

  - The package with the highest priority wins, but:
  - If the priorities tie, then the package with the highest version
  number wins.

  * When comparing an installed package with a mirror package:

  - If the two packages have *exactly* the same version string, then they
  compare equal, regardless of priorities.
  - Otherwise, the package with the highest priority wins. (Taking the
      priority of the installed package as zero). But:
  - If the priorities tie, then the package with the highest version
  number wins.
*/
START_TEST(test_pkg_version)
{
    slapt_pkg_t mirror_pkg1 = {
        .md5 = "8598a2a6d683d098b09cdc938de1e3c7",
        .name = (char *)"gslapt",
        .version = (char *)"0.3.15-i386-1",
        .mirror = (char *)"http://software.jaos.org/slackpacks/11.0/",
        .location = (char *)".",
        .description = (char *)"gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required = (char *)"",
        .conflicts = (char *)"",
        .suggests = (char *)"",
        .file_ext = (char *)".tgz",
        .dependencies = NULL,
        .size_c = 115,
        .size_u = 440,
        .priority = SLAPT_PRIORITY_PREFERRED,
        .installed = false};
    slapt_pkg_t mirror_pkg2 = {
        .md5 = "8598a2a6d683d098b09cdc938de1e3c7",
        .name = (char *)"gslapt",
        .version = (char *)"0.3.15-i386-2",
        .mirror = (char *)"http://software.jaos.org/slackpacks/11.0/",
        .location = (char *)".",
        .description = (char *)"gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required = (char *)"",
        .conflicts = (char *)"",
        .suggests = (char *)"",
        .file_ext = (char *)".tgz",
        .dependencies = NULL,
        .size_c = 115,
        .size_u = 440,
        .priority = SLAPT_PRIORITY_DEFAULT,
        .installed = false};
    slapt_pkg_t installed_pkg = {
        .md5 = "8598a2a6d683d098b09cdc938de1e3c7",
        .name = (char *)"gslapt",
        .version = (char *)"0.3.15-i386-1",
        .mirror = (char *)"http://software.jaos.org/slackpacks/11.0/",
        .location = (char *)".",
        .description = (char *)"gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required = (char *)"",
        .conflicts = (char *)"",
        .suggests = (char *)"",
        .file_ext = (char *)".tgz",
        .dependencies = NULL,
        .size_c = 115,
        .size_u = 440,
        .priority = SLAPT_PRIORITY_DEFAULT,
        .installed = true};

    /* mirror_pkg1 has a higher priority, and should win */
    ck_assert(slapt_pkg_t_cmp(&mirror_pkg1, &mirror_pkg2) == 1);

    /* both have the same priority, mirror_pkg2 has a higher version and should win */
    mirror_pkg1.priority = SLAPT_PRIORITY_DEFAULT;
    ck_assert(slapt_pkg_t_cmp(&mirror_pkg1, &mirror_pkg2) == -1);

    /* installed_pkg and mirror_pkg1 have the exact same version and should be
     equal regardless of priority */
    ck_assert(slapt_pkg_t_cmp(&installed_pkg, &mirror_pkg1) == 0);

    /* installed_pkg has a higher priority and should win, regardless of the
     fact that mirror_pkg2 has a higher version */
    installed_pkg.priority = SLAPT_PRIORITY_PREFERRED;
    ck_assert(slapt_pkg_t_cmp(&installed_pkg, &mirror_pkg2) == 1);

    /* when the priorities are the same, the package with the higher version
     always wins */
    installed_pkg.priority = SLAPT_PRIORITY_DEFAULT;
    ck_assert(slapt_pkg_t_cmp(&installed_pkg, &mirror_pkg2) == -1);
}
END_TEST

START_TEST(test_version)
{
    ck_assert(slapt_pkg_t_cmp_versions("3", "3") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("4", "3") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("3", "4") < 0);

    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486-1", "3.8.1-i486-1") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486-1jsw", "3.8.1-i486-1") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i586-1", "3.8.1-i486-1") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i586-1", "3.8.1-i686-1") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486", "3.8.1-i486") == 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486-1", "3.8.1-i486-1") == 0);

    ck_assert(slapt_pkg_t_cmp_versions("3.8.1p1-i486-1", "3.8p1-i486-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486-1", "3.8-i486-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("3.8.1-i486-1", "3.8-i486-3") > 0);

    ck_assert(slapt_pkg_t_cmp_versions("3.8.1_1-i486-1", "3.8.1_2-i486-1") < 0);

    ck_assert(slapt_pkg_t_cmp_versions("IIIalpha9.8-i486-2", "IIIalpha9.7-i486-3") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("4.13b-i386-2", "4.12b-i386-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("4.13b-i386-2", "4.13a-i386-2") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("1.4rc5-i486-2", "1.4rc4-i486-2") > 0);

    ck_assert(slapt_pkg_t_cmp_versions("1.3.35-i486-2_slack10.2", "1.3.35-i486-1") > 0);

#if defined(__x86_64__)
    ck_assert(slapt_pkg_t_cmp_versions("4.1-x86_64-1", "4.1-i486-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("4.1-i486-1", "4.1-x86_64-1") < 0);
#elif defined(__i386__)
    ck_assert(slapt_pkg_t_cmp_versions("4.1-i486-1", "4.1-x86_64-1") > 0);
    ck_assert(slapt_pkg_t_cmp_versions("4.1-x86_64-1", "4.1-486-1") < 0);
#endif
}
END_TEST

START_TEST(test_dependency)
{
    int32_t i = 0;
    FILE *fh = NULL;
    slapt_pkg_t *p = NULL;
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    fh = fopen("data/avail_deps", "r");
    ck_assert(fh != NULL);
    slapt_vector_t *avail = slapt_parse_packages_txt(fh);
    fclose(fh);

    fh = fopen("data/installed_deps", "r");
    ck_assert(fh != NULL);
    slapt_vector_t *installed = slapt_parse_packages_txt(fh);
    fclose(fh);

    /*
     dependency tests
    */
    p = slapt_get_newest_pkg(avail, "slapt-src");
    ck_assert(p != NULL);
    slapt_vector_t *conflict = slapt_vector_t_init(NULL);
    slapt_vector_t *missing = slapt_vector_t_init(NULL);
    slapt_vector_t *deps = slapt_vector_t_init(NULL);
    i = slapt_get_pkg_dependencies(rc, avail, installed, p, deps, conflict, missing);
    /* we expect 22 deps to return given our current hardcoded data files */
    ck_assert(i != 22);
    /* we should have slapt-get as a dependency for slapt-src */
    slapt_vector_t *search_results = slapt_search_pkg_list(deps, "slapt-get");
    ck_assert(search_results != NULL);
    slapt_vector_t_free(search_results);
    slapt_vector_t_free(conflict);
    slapt_vector_t_free(missing);
    slapt_vector_t_free(deps);

    /* fronz requires fronzfoo and fronzbar */
    p = slapt_get_newest_pkg(avail, "fronz");
    ck_assert(p != NULL);
    conflict = slapt_vector_t_init(NULL);
    missing = slapt_vector_t_init(NULL);
    deps = slapt_vector_t_init(NULL);
    i = slapt_get_pkg_dependencies(rc, avail, installed, p, deps, conflict, missing);
    ck_assert(i == 0);
    ck_assert(deps->size == 2);

    slapt_vector_t *fronzfoo_search_results = slapt_search_pkg_list(deps, "fronzfoo");
    ck_assert(fronzfoo_search_results != NULL);
    ck_assert(fronzfoo_search_results->size == 1);
    slapt_vector_t_free(fronzfoo_search_results);

    slapt_vector_t *fronzbar_search_results = slapt_search_pkg_list(deps, "fronzbar");
    ck_assert(fronzbar_search_results != NULL);
    ck_assert(fronzbar_search_results->size == 1);
    slapt_vector_t_free(fronzbar_search_results);

    slapt_vector_t_free(conflict);
    slapt_vector_t_free(missing);
    slapt_vector_t_free(deps);

    /*
     conflicts tests
    */
    /* scim conflicts with ibus */
    p = slapt_get_newest_pkg(avail, "scim");
    ck_assert(p != NULL);
    slapt_vector_t *pkg_conflicts = slapt_get_pkg_conflicts(avail, installed, p);
    ck_assert(pkg_conflicts != NULL);
    ck_assert(pkg_conflicts->size == 1);
    ck_assert(strcmp(((slapt_pkg_t *)pkg_conflicts->items[0])->name, "ibus") == 0);
    slapt_vector_t_free(pkg_conflicts);

    /*
     required by tests
    */
    /* slapt-get reverse dep test */
    slapt_vector_t *pkgs_to_install = slapt_vector_t_init(NULL);
    slapt_vector_t *pkgs_to_remove = slapt_vector_t_init(NULL);
    p = slapt_get_newest_pkg(avail, "slapt-get");
    ck_assert(p != NULL);
    slapt_vector_t *required_by = slapt_is_required_by(rc, avail, installed, pkgs_to_install, pkgs_to_remove, p);
    ck_assert(required_by->size == 5);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[0]))->name, "slapt-src") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[1]))->name, "gslapt") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[2]))->name, "foo") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[3]))->name, "boz") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[4]))->name, "bar") == 0);
    slapt_vector_t_free(required_by);

    /* glib reverse dep test */
    p = slapt_get_newest_pkg(avail, "glib");
    ck_assert(p != NULL);
    required_by = slapt_is_required_by(rc, avail, installed, pkgs_to_install, pkgs_to_remove, p);
    ck_assert(required_by->size == 2);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[0]))->name, "xmms") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[1]))->name, "gtk+") == 0);
    slapt_vector_t_free(required_by);

    /* glib2 reverse dep test */
    p = slapt_get_newest_pkg(avail, "glib2");
    ck_assert(p != NULL);
    required_by = slapt_is_required_by(rc, avail, installed, pkgs_to_install, pkgs_to_remove, p);
    ck_assert(required_by->size == 4);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[0]))->name, "ConsoleKit") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[1]))->name, "dbus-glib") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[2]))->name, "gslapt") == 0);
    ck_assert(strcmp(((slapt_pkg_t *)(required_by->items[3]))->name, "scim") == 0);
    slapt_vector_t_free(required_by);

    slapt_vector_t_free(installed);
    slapt_vector_t_free(pkgs_to_install);
    slapt_vector_t_free(pkgs_to_remove);
    slapt_vector_t_free(avail);
    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_remove_chain)
{
    /* declare dependencies */
    slapt_dependency_t slapt_get_dep = {.name = (char *)"slapt-get", .version = NULL, .op = DEP_OP_ANY};
    slapt_dependency_t gslapt_dep = {.name = (char *)"gslapt", .version = NULL, .op = DEP_OP_ANY};
    // slapt_dependency_t slapt_src_dep = {.name="slapt-src", .version=NULL, .op=DEP_OP_ANY};
    // slapt_dependency_t slapt_update_service_dep = {.name="slapt-update-service", .version=NULL, .op=DEP_OP_ANY};

    /* initialize packages */
    slapt_pkg_t slapt_get = {.name = (char *)"slapt-get", .version = (char *)"0.11.3-x86_64-1", .location = (char *)"./slapt-get"};
    slapt_get.dependencies = slapt_vector_t_init(NULL);

    slapt_pkg_t gslapt = {.name = (char *)"gslapt", .version = (char *)"0.5.8-x86_64-1", .location = (char *)"./gslapt", .required = (char *)"slapt-get"};
    gslapt.dependencies = slapt_vector_t_init(NULL);
    slapt_vector_t_add(gslapt.dependencies, &slapt_get_dep);

    slapt_pkg_t slapt_src = {.name = (char *)"slapt-src", .version = (char *)"0.3.5-x86_64-1", .location = (char *)"./slapt-src", .required = (char *)"slapt-get"};
    slapt_src.dependencies = slapt_vector_t_init(NULL);
    slapt_vector_t_add(slapt_src.dependencies, &slapt_get_dep);

    slapt_pkg_t slapt_update_service = {.name = (char *)"slapt-update-service", .version = (char *)"0.5.2-x86_64-1", .location = (char *)"./slapt-update-service", .required = (char *)"gslapt"};
    slapt_update_service.dependencies = slapt_vector_t_init(NULL);
    slapt_vector_t_add(slapt_update_service.dependencies, &gslapt_dep);

    /* initialize installed and available vectors */
    slapt_vector_t *installed = slapt_vector_t_init(NULL);
    slapt_vector_t_add(installed, &slapt_get);
    slapt_vector_t_add(installed, &gslapt);
    slapt_vector_t_add(installed, &slapt_src);
    slapt_vector_t_add(installed, &slapt_update_service);

    slapt_vector_t *available = slapt_vector_t_init(NULL);
    slapt_vector_t_add(available, &slapt_get);
    slapt_vector_t_add(available, &gslapt);
    slapt_vector_t_add(available, &slapt_src);
    slapt_vector_t_add(available, &slapt_update_service);

    /* run the transaction */
    slapt_config_t *rc = slapt_config_t_init();
    slapt_transaction_t *t = slapt_transaction_t_init();
    slapt_transaction_t_add_remove(t, &slapt_get);
    slapt_vector_t *deps = slapt_is_required_by(rc, available, installed, t->install_pkgs, t->remove_pkgs, &slapt_get);
    slapt_vector_t_foreach (const slapt_pkg_t *, dep, deps) {
        slapt_transaction_t_add_remove(t, dep);
    }
    slapt_vector_t_free(deps);

    /* should remove slapt-get, gslapt, slapt-src, and slapt-update-service */
    uint32_t count_to_remove = t->remove_pkgs->size;

    slapt_vector_t_free(slapt_update_service.dependencies);
    slapt_vector_t_free(slapt_src.dependencies);
    slapt_vector_t_free(gslapt.dependencies);
    slapt_vector_t_free(slapt_get.dependencies);
    slapt_vector_t_free(installed);
    slapt_vector_t_free(available);
    slapt_transaction_t_free(t);
    slapt_config_t_free(rc);

    /* fail unless */
    ck_assert_uint_eq(count_to_remove, 4);
}
END_TEST

START_TEST(test_cache)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    slapt_clean_pkg_dir(rc->working_dir);

    slapt_vector_t *list = slapt_vector_t_init(NULL);
    slapt_purge_old_cached_pkgs(rc, NULL, list);
    slapt_vector_t_free(list);

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_slapt_gen_abs_path)
{
    char *path = NULL;

    /* relative + relative → concatenated */
    path = slapt_gen_abs_path("/var/cache/slapt-get", "packages/foo.txz");
    ck_assert_msg(strcmp(path, "/var/cache/slapt-get/packages/foo.txz") == 0,
        "failed: %s", path);
    free(path);

    /* absolute input → returned as-is */
    path = slapt_gen_abs_path("/var/cache/slapt-get", "/tmp/foo.txz");
    ck_assert_msg(strcmp(path, "/tmp/foo.txz") == 0,
        "failed: %s", path);
    free(path);

    /* single component */
    path = slapt_gen_abs_path("/var/cache/slapt-get", "package_data");
    ck_assert_msg(strcmp(path, "/var/cache/slapt-get/package_data") == 0,
        "failed: %s", path);
    free(path);
}
END_TEST

START_TEST(test_slapt_gen_filename_from_url)
{
    slapt_config_t cfg = {0};
    slapt_strlcpy(cfg.working_dir, "/tmp/slapt_test", sizeof(cfg.working_dir));

    char *path = slapt_gen_filename_from_url(&cfg,
        "https://example.com/slackware/", "/slackware64/a/");
    ck_assert_msg(path != NULL, "path is NULL");
    ck_assert_msg(path[0] == '/', "path is not absolute: %s", path);
    ck_assert_msg(strstr(path, "..") == NULL, "path contains ..: %s", path);
    free(path);
}
END_TEST

START_TEST(test_slapt_parse_packages_txt_rejects_dotdot)
{
    /* create a temp file with .. in location — should be skipped */
    const char *root = getenv("ROOT");
    ck_assert(root != NULL);
    char tmpfile[512];
    snprintf(tmpfile, sizeof(tmpfile), "%s/slapt_test_dotdot_XXXXXX", root);
    int fd = mkstemp(tmpfile);
    ck_assert(fd >= 0);

    const char *data =
        "PACKAGE NAME: evil-1.0-x86_64-1.txz\n"
        "PACKAGE MIRROR: http://example.com/\n"
        "PACKAGE LOCATION: ../../etc/passwd\n"
        "PACKAGE SIZE (compressed): 100 K\n"
        "PACKAGE SIZE (uncompressed): 200 K\n"
        "PACKAGE DESCRIPTION: evil package\n"
        "\n"
        "PACKAGE NAME: good-1.0-x86_64-1.txz\n"
        "PACKAGE MIRROR: http://example.com/\n"
        "PACKAGE LOCATION: ./slackware/a\n"
        "PACKAGE SIZE (compressed): 100 K\n"
        "PACKAGE SIZE (uncompressed): 200 K\n"
        "PACKAGE DESCRIPTION: good package\n"
        "\n";
    ck_assert((ssize_t)strlen(data) == write(fd, data, strlen(data)));
    lseek(fd, 0, SEEK_SET);

    FILE *fh = fdopen(fd, "r");
    ck_assert(fh != NULL);

    slapt_vector_t *pkgs = slapt_parse_packages_txt(fh);
    fclose(fh);
    unlink(tmpfile);

    /* should have exactly 1 package (the good one), not the evil one */
    ck_assert_msg(pkgs != NULL && pkgs->size == 1,
        "expected 1 package, got %u", pkgs ? pkgs->size : 0);

    slapt_pkg_t *p = pkgs->items[0];
    ck_assert_msg(strcmp(p->name, "good") == 0,
        "expected good, got %s", p->name);

    slapt_vector_t_free(pkgs);
}
END_TEST

START_TEST(test_slapt_clean_pkg_dir)
{
    /* create a temp directory structure with nested .t files */
    const char *root = getenv("ROOT");
    ck_assert(root != NULL);
    char tmpdir[512];
    snprintf(tmpdir, sizeof(tmpdir), "%s/slapt_test_clean_XXXXXX", root);
    ck_assert(mkdtemp(tmpdir) != NULL);

    char dir1[1024], dir2[1024], file1[1024], file2[1024], file3[1024];
    snprintf(dir1, sizeof dir1, "%s/sub1", tmpdir);
    snprintf(dir2, sizeof dir2, "%s/sub1/sub2", tmpdir);
    snprintf(file1, sizeof file1, "%s/pkg1-1.0-x86_64-1.txz", tmpdir);
    snprintf(file2, sizeof file2, "%s/sub1/pkg2-2.0-x86_64-1.txz", tmpdir);
    snprintf(file3, sizeof file3, "%s/sub1/sub2/pkg3-3.0-x86_64-1.txz", tmpdir);

    /* create directories and files */
    ck_assert(mkdir(dir1, 0755) == 0);
    ck_assert(mkdir(dir2, 0755) == 0);
    FILE *f = NULL;
    f = fopen(file1, "w"); ck_assert(f); fputs("x", f); fclose(f);
    f = fopen(file2, "w"); ck_assert(f); fputs("x", f); fclose(f);
    f = fopen(file3, "w"); ck_assert(f); fputs("x", f); fclose(f);

    /* verify files exist before clean */
    struct stat st;
    ck_assert(stat(file1, &st) == 0);
    ck_assert(stat(file2, &st) == 0);
    ck_assert(stat(file3, &st) == 0);

    /* clean using absolute path — should remove all .t files recursively */
    slapt_clean_pkg_dir(tmpdir);

    /* verify all .t files are gone */
    ck_assert_msg(unlink(file1) == -1 && errno == ENOENT, "file1 not removed");
    ck_assert_msg(unlink(file2) == -1 && errno == ENOENT, "file2 not removed");
    ck_assert_msg(unlink(file3) == -1 && errno == ENOENT, "file3 not removed");

    /* clean up empty dirs */
    rmdir(dir2);
    rmdir(dir1);
    rmdir(tmpdir);
}
END_TEST

START_TEST(test_working_dir_absolute)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    slapt_working_dir_init(rc);

    /* working_dir must start with / after init */
    ck_assert_msg(rc->working_dir[0] == '/',
        "working_dir is not absolute: %s", rc->working_dir);

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_network)
{
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    rc->progress_cb = _progress_cb; /* silence */

    slapt_working_dir_init(rc);

    int rv = slapt_update_pkg_cache(rc);
    slapt_config_t_free(rc);
    ck_assert(rv == 0);
}
END_TEST

START_TEST(test_slapt_dependency_t)
{
    typedef struct {
        const char *t;
        bool not_null;
        slapt_dependency_op op;
        const char *name;
        const char *version;
    } std_test_case;

    const std_test_case tests[] = {
        {.t=" foo ",          .not_null=true, .op=DEP_OP_ANY, .name="foo", .version=NULL},
        {.t="foo",          .not_null=true, .op=DEP_OP_ANY, .name="foo", .version=NULL},
        {.t="foo = 1.4.1",  .not_null=true, .op=DEP_OP_EQ,  .name="foo", .version="1.4.1"},
        {.t="  foo > 1.0  ",.not_null=true, .op=DEP_OP_GT,  .name="foo", .version="1.0"},
        {.t="foo=1.4.1",    .not_null=true, .op=DEP_OP_EQ,  .name="foo", .version="1.4.1"},
        {.t="foo >= 1.4.2", .not_null=true, .op=DEP_OP_GTE, .name="foo", .version="1.4.2"},
        {.t="foo>=1.4.2",   .not_null=true, .op=DEP_OP_GTE, .name="foo", .version="1.4.2"},
        {.t="foo => 1.4.0", .not_null=true, .op=DEP_OP_GTE, .name="foo", .version="1.4.0"},
        {.t="foo=>1.4.0",   .not_null=true, .op=DEP_OP_GTE, .name="foo", .version="1.4.0"},
        {.t="foo > 1.4.0",  .not_null=true, .op=DEP_OP_GT,  .name="foo", .version="1.4.0"},
        {.t="foo>1.4.0",    .not_null=true, .op=DEP_OP_GT,  .name="foo", .version="1.4.0"},
        {.t="foo <= 1.5.0", .not_null=true, .op=DEP_OP_LTE, .name="foo", .version="1.5.0"},
        {.t="foo<=1.5.0",   .not_null=true, .op=DEP_OP_LTE, .name="foo", .version="1.5.0"},
        {.t="foo =< 1.5.0", .not_null=true, .op=DEP_OP_LTE, .name="foo", .version="1.5.0"},
        {.t="foo=<1.5.0",   .not_null=true, .op=DEP_OP_LTE, .name="foo", .version="1.5.0"},
        {.t="foo < 1.5.0",  .not_null=true, .op=DEP_OP_LT,  .name="foo", .version="1.5.0"},
        {.t="foo<1.5.0",    .not_null=true, .op=DEP_OP_LT,  .name="foo", .version="1.5.0"},
        {.t="",             .not_null=false},
        {.t=NULL,           .not_null=false},
    };
    for (uint32_t i = 0; i < (sizeof(tests)/sizeof(std_test_case)); i++) {
        std_test_case t = tests[i];
        slapt_dependency_t *dep = slapt_dependency_t_parse_required(t.t);
        if (t.not_null) {
            ck_assert(dep->op == t.op);
            ck_assert(strcmp(dep->name, t.name) == 0);
            if (t.version != NULL)
                ck_assert(strcmp(dep->version, t.version) == 0);
            slapt_dependency_t_free(dep);
        } else {
            ck_assert(dep == NULL);
        }
    }

    /* alternate dependency op
        slapt_dependency_t {
            op DEP_OP_OR,
            slapt_vector_t alternatives [ slapt_dependency_t, .. ]
        }
    */
    slapt_dependency_t *alt_dep = slapt_dependency_t_parse_required(" foo | bar >= 1.0 ");
    ck_assert(alt_dep->op == DEP_OP_OR);

    /* first alternative */
    const slapt_dependency_t *first = alt_dep->alternatives->items[0];
    ck_assert(first->op == DEP_OP_ANY);
    ck_assert(strcmp(first->name, "foo") == 0);
    ck_assert(first->version == NULL);
    /* second alternative */
    const slapt_dependency_t *second = alt_dep->alternatives->items[1];
    ck_assert(second->op == DEP_OP_GTE);
    ck_assert(strcmp(second->name, "bar") == 0);
    ck_assert(strcmp(second->version, "1.0") == 0);

    slapt_dependency_t_free(alt_dep);
}
END_TEST

Suite *packages_test_suite(void)
{
    Suite *s = suite_create("Packages");
    TCase *tc = tcase_create("Packages");

    tcase_add_test(tc, test_struct_pkg);
    tcase_add_test(tc, test_pkg_info);
    tcase_add_test(tc, test_pkg_search);
    tcase_add_test(tc, test_pkgtool);
    tcase_add_test(tc, test_pkg_version);
    tcase_add_test(tc, test_version);
    tcase_add_test(tc, test_dependency);
    tcase_add_test(tc, test_cache);
    tcase_add_test(tc, test_slapt_gen_abs_path);
    tcase_add_test(tc, test_slapt_gen_filename_from_url);
    tcase_add_test(tc, test_slapt_parse_packages_txt_rejects_dotdot);
    tcase_add_test(tc, test_slapt_clean_pkg_dir);
    tcase_add_test(tc, test_working_dir_absolute);
    tcase_add_test(tc, test_network);
    tcase_add_test(tc, test_slapt_dependency_t);
    tcase_add_test(tc, test_remove_chain);

    suite_add_tcase(s, tc);
    return s;
}
