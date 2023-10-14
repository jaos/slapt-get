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
    size_t i = -1;
    char *string = NULL;
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");

    string = slapt_pkg_t_short_description(&pkg);
    ck_assert(string != NULL);
    ck_assert(strcmp(string, "gslapt (GTK slapt-get, an APT like system for Slackware)") == 0);
    free(string);
    string = NULL;

    string = slapt_gen_filename_from_url("http://software.jaos.org/slackpacks/11.0/", "PACKAGES.TXT");
    ck_assert(string != NULL);
    ck_assert(strcmp(string, ".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT") == 0);
    free(string);
    string = NULL;

    string = slapt_gen_head_cache_filename(".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT");
    ck_assert(string != NULL);
    ck_assert(strcmp(string, ".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT.head") == 0);
    free(string);
    string = NULL;

    string = slapt_pkg_t_url(&pkg);
    ck_assert(string != NULL);
    ck_assert(strcmp(string, "http://software.jaos.org/slackpacks/11.0//gslapt-0.3.15-i386-1.tgz") == 0);
    free(string);
    string = NULL;

    slapt_vector_t_add(rc->exclude_list, strdup("^gslapt$"));
    ck_assert(slapt_is_excluded(rc, &pkg));
    slapt_vector_t_remove(rc->exclude_list, "^gslapt$");

    ck_assert(slapt_download_pkg(rc, &pkg, NULL) == 0);
    ck_assert(slapt_verify_downloaded_pkg(rc, &pkg) == 0);

    i = slapt_get_pkg_file_size(rc, &pkg);
    ck_assert_int_gt(i, 1);

    string = strdup(pkg.description);
    string = slapt_pkg_t_clean_description(&pkg);
    ck_assert(strcmp(string, " gslapt (GTK slapt-get, an APT like system for Slackware)\n") == 0);
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

    p = slapt_get_newest_pkg(list, "gslapt");
    ck_assert(p != NULL);

    p = slapt_get_exact_pkg(list, "gslapt", "0.3.15-i386-1");
    ck_assert(p != NULL);

    p = slapt_get_pkg_by_details(list, "gslapt", "0.3.15-i386-1", ".");
    ck_assert(p != NULL);

    l = slapt_search_pkg_list(list, "^gslapt$");
    ck_assert(l != NULL);
    ck_assert(l->size == 1);
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
        .md5="8598a2a6d683d098b09cdc938de1e3c7",
        .name="gslapt",
        .version="0.3.15-i386-1",
        .mirror="http://software.jaos.org/slackpacks/11.0/",
        .location=".",
        .description="gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required="",
        .conflicts="",
        .suggests="",
        .file_ext=".tgz",
        .dependencies=NULL,
        .size_c=115,
        .size_u=440,
        .priority=SLAPT_PRIORITY_PREFERRED,
        .installed=false};
    slapt_pkg_t mirror_pkg2 = {
        .md5="8598a2a6d683d098b09cdc938de1e3c7",
        .name="gslapt",
        .version="0.3.15-i386-2",
        .mirror="http://software.jaos.org/slackpacks/11.0/",
        .location=".",
        .description="gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required="",
        .conflicts="",
        .suggests="",
        .file_ext=".tgz",
        .dependencies=NULL,
        .size_c=115,
        .size_u=440,
        .priority=SLAPT_PRIORITY_DEFAULT,
        .installed=false};
    slapt_pkg_t installed_pkg = {
        .md5="8598a2a6d683d098b09cdc938de1e3c7",
        .name="gslapt",
        .version="0.3.15-i386-1",
        .mirror="http://software.jaos.org/slackpacks/11.0/",
        .location=".",
        .description="gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
        .required="",
        .conflicts="",
        .suggests="",
        .file_ext=".tgz",
        .dependencies=NULL,
        .size_c=115,
        .size_u=440,
        .priority=SLAPT_PRIORITY_DEFAULT,
        .installed=true};

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
    uint32_t i = 0;
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
    slapt_dependency_t slapt_get_dep = {.name="slapt-get", .version=NULL, .op=DEP_OP_ANY};
    slapt_dependency_t gslapt_dep = {.name="gslapt", .version=NULL, .op=DEP_OP_ANY};
    //slapt_dependency_t slapt_src_dep = {.name="slapt-src", .version=NULL, .op=DEP_OP_ANY};
    //slapt_dependency_t slapt_update_service_dep = {.name="slapt-update-service", .version=NULL, .op=DEP_OP_ANY};

    /* initialize packages */
    slapt_pkg_t slapt_get = {.name="slapt-get", .version="0.11.3-x86_64-1", .location="./slapt-get"};
    slapt_get.dependencies = slapt_vector_t_init(NULL);

    slapt_pkg_t gslapt = {.name="gslapt", .version="0.5.8-x86_64-1", .location="./gslapt", .required="slapt-get"};
    gslapt.dependencies = slapt_vector_t_init(NULL);
    slapt_vector_t_add(gslapt.dependencies, &slapt_get_dep);

    slapt_pkg_t slapt_src = {.name="slapt-src", .version="0.3.5-x86_64-1", .location="./slapt-src", .required="slapt-get"};
    slapt_src.dependencies = slapt_vector_t_init(NULL);
    slapt_vector_t_add(slapt_src.dependencies, &slapt_get_dep);

    slapt_pkg_t slapt_update_service = {.name="slapt-update-service", .version="0.5.2-x86_64-1", .location="./slapt-update-service", .required="gslapt"};
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
    slapt_vector_t_foreach(const slapt_pkg_t *, dep, deps) {
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

    /* not easily testable due to timeout
  slapt_vector_t *list = slapt_vector_t_init();
  slapt_purge_old_cached_pkgs(rc, NULL, list);
  */

    slapt_config_t_free(rc);
}
END_TEST

START_TEST(test_network)
{
    char *cwd = get_current_dir_name();
    slapt_config_t *rc = slapt_config_t_read("./data/rc1");
    rc->progress_cb = _progress_cb; /* silence */

    /* must chdir to working dir */
    ck_assert(chdir(rc->working_dir) == 0);

    /* write pkg data to disk
  void slapt_write_pkg_data(const char *source_url,FILE *d_file,
                            slapt_vector_t *pkgs);
  */

    /* download the PACKAGES.TXT and CHECKSUMS.md5 files
  slapt_vector_t *slapt_get_pkg_source_packages (const slapt_config_t *global_config,
                                                        const char *url);
  slapt_vector_t *slapt_get_pkg_source_patches (const slapt_config_t *global_config,
                                                       const char *url);
  FILE *slapt_get_pkg_source_checksums (const slapt_config_t *global_config,
                                        const char *url);
  int slapt_get_pkg_source_changelog (const slapt_config_t *global_config,
                                        const char *url);
  */

    int rv = slapt_update_pkg_cache(rc);
    slapt_config_t_free(rc);
    ck_assert(rv == 0);

    ck_assert(chdir(cwd) == 0);
    free(cwd);
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
    for(uint32_t i = 0; i < (sizeof(tests)/sizeof(std_test_case)); i++) {
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
    slapt_dependency_t *first = alt_dep->alternatives->items[0];
    ck_assert(first->op == DEP_OP_ANY);
    ck_assert(strcmp(first->name, "foo") == 0);
    ck_assert(first->version == NULL);
    /* second alternative */
    slapt_dependency_t *second = alt_dep->alternatives->items[1];
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
    tcase_add_test(tc, test_network);
    tcase_add_test(tc, test_slapt_dependency_t);
    tcase_add_test(tc, test_remove_chain);

    suite_add_tcase(s, tc);
    return s;
}
