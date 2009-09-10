#include "test_packages.h"
extern slapt_pkg_info_t pkg;

extern int _progress_cb(void *clientp, double dltotal, double dlnow,
                        double ultotal, double ulnow);

START_TEST (test_struct_pkg)
{
  slapt_pkg_info_t *cpy = slapt_init_pkg();
  fail_if (cpy == NULL);

  cpy = slapt_copy_pkg(cpy, &pkg);
  fail_if (cpy == NULL);

  {
    char *pkg_str = slapt_stringify_pkg(&pkg);
    char *cpy_str = slapt_stringify_pkg(cpy);

    fail_if (pkg_str == NULL);
    fail_if (cpy_str == NULL);
    fail_unless (strcmp(pkg_str,cpy_str) == 0);

    free(pkg_str);
    free(cpy_str);
  }

  slapt_free_pkg(cpy);
}
END_TEST


START_TEST (test_pkg_info)
{
  size_t i            = -1;
  char *string        = NULL;
  slapt_rc_config *rc = slapt_read_rc_config("./data/rc1");

  string = slapt_gen_short_pkg_description(&pkg);
  fail_if (string == NULL);
  fail_unless (strcmp(string,"gslapt (GTK slapt-get, an APT like system for Slackware)") == 0);
  free(string); string = NULL;

  string = slapt_gen_filename_from_url("http://software.jaos.org/slackpacks/11.0/","PACKAGES.TXT");
  fail_if (string == NULL);
  fail_unless (strcmp(string,".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT") == 0);
  free(string); string = NULL;
  
  string = slapt_gen_head_cache_filename(".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT");
  fail_if (string == NULL);
  fail_unless (strcmp(string,".http:##software.jaos.org#slackpacks#11.0#PACKAGES.TXT.head") == 0);
  free(string); string = NULL;

  string = slapt_gen_pkg_url(&pkg);
  fail_if (string == NULL);
  fail_unless (strcmp(string,"http://software.jaos.org/slackpacks/11.0/./gslapt-0.3.15-i386-1.tgz") == 0);
  free(string); string = NULL;

  slapt_add_exclude(rc->exclude_list,"^gslapt$");
  fail_if (slapt_is_excluded(rc,&pkg) == 0);
  slapt_remove_exclude(rc->exclude_list,"^gslapt$");

  fail_unless (slapt_download_pkg(rc, &pkg, NULL) == 0);
  fail_unless (slapt_verify_downloaded_pkg(rc,&pkg) == 0);

  i = slapt_get_pkg_file_size(rc, &pkg);
  fail_if (i < 1);

  string = strdup(pkg.description);
  slapt_clean_description(string, pkg.name);
  fail_unless (strcmp(string," gslapt (GTK slapt-get, an APT like system for Slackware)\n") == 0);
  free(string); string = NULL;

  /* retrieve the packages changelog entry, if any.  Returns NULL otherwise */
  /* char *slapt_get_pkg_changelog(const slapt_pkg_info_t *pkg); */

  /* get the package filelist, returns (char *) on success or NULL on error */
  /* char *slapt_get_pkg_filelist(const slapt_pkg_info_t *pkg); */

  fail_unless (pkg.priority == SLAPT_PRIORITY_DEFAULT);
  fail_unless (strcmp(slapt_priority_to_str(pkg.priority),gettext("Default")) == 0);

  slapt_free_rc_config(rc);
}
END_TEST


START_TEST (test_pkg_list)
{
  struct slapt_pkg_list *list = slapt_init_pkg_list();
  fail_if (list == NULL);
  fail_unless (list->pkg_count == 0);

  slapt_add_pkg_to_pkg_list(list, &pkg);
  fail_unless (list->pkg_count == 1);

  slapt_free_pkg_list(list);
  list = NULL;

  list = slapt_get_installed_pkgs();
  fail_if (list == NULL);
  /* fail_unless (list->pkg_count == 0); could be anything */
  slapt_free_pkg_list(list);
  list = NULL;


  /* parse the PACKAGES.TXT file
  struct slapt_pkg_list *slapt_parse_packages_txt(FILE *);
  */

  /*
    return a list of available packages must be already chdir'd to
    rc_config->working_dir.  Otherwise, open a filehandle to the package data
    and pass it to slapt_parse_packages_txt();
  struct slapt_pkg_list *slapt_get_available_pkgs(void);
  */

  /* get a list of obsolete packages
  struct slapt_pkg_list *
    slapt_get_obsolete_pkgs ( const slapt_rc_config *global_config,
                              struct slapt_pkg_list *avail_pkgs,
                              struct slapt_pkg_list *installed_pkgs);
  */

  /*
    fill in the md5sum of the package
  void slapt_get_md5sums(struct slapt_pkg_list *pkgs, FILE *checksum_file);
  */

}
END_TEST


START_TEST (test_pkg_search)
{
  slapt_pkg_info_t *p         = NULL;
  struct slapt_pkg_list *l    = NULL;
  struct slapt_pkg_list *list = slapt_init_pkg_list();
  slapt_add_pkg_to_pkg_list(list, &pkg);

  p = slapt_get_newest_pkg(list, "gslapt");
  fail_if (p == NULL);

  p = slapt_get_exact_pkg(list, "gslapt", "0.3.15-i386-1");
  fail_if (p == NULL);

  p = slapt_get_pkg_by_details(list, "gslapt", "0.3.15-i386-1", ".");
  fail_if (p == NULL);

  l = slapt_search_pkg_list(list, "^gslapt$");
  fail_if (l == NULL);
  fail_unless (l->pkg_count == 1);

  slapt_free_pkg_list(list);

}
END_TEST


START_TEST (test_pkgtool)
{
  slapt_rc_config *rc = slapt_read_rc_config("./data/rc1");
  /* disabled... */
  /*
  int r               = -1;
  r = slapt_install_pkg(rc, &pkg);
  r = slapt_upgrade_pkg(rc, &pkg);
  r = slapt_remove_pkg(rc, &pkg);
  */

  slapt_free_rc_config(rc);
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
START_TEST (test_pkg_version)
{
  slapt_pkg_info_t mirror_pkg1 = {
                        "8598a2a6d683d098b09cdc938de1e3c7",
                        "gslapt",
                        "0.3.15-i386-1",
                        "http://software.jaos.org/slackpacks/11.0/",
                        ".",
                        "gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
                        "",
                        "",
                        "",
                        ".tgz",
                        115,
                        440,
                        SLAPT_PRIORITY_PREFERRED,
                        SLAPT_FALSE
  };
  slapt_pkg_info_t mirror_pkg2 = {
                        "8598a2a6d683d098b09cdc938de1e3c7",
                        "gslapt",
                        "0.3.15-i386-2",
                        "http://software.jaos.org/slackpacks/11.0/",
                        ".",
                        "gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
                        "",
                        "",
                        "",
                        ".tgz",
                        115,
                        440,
                        SLAPT_PRIORITY_DEFAULT,
                        SLAPT_FALSE
  };
  slapt_pkg_info_t installed_pkg = {
                        "8598a2a6d683d098b09cdc938de1e3c7",
                        "gslapt",
                        "0.3.15-i386-1",
                        "http://software.jaos.org/slackpacks/11.0/",
                        ".",
                        "gslapt: gslapt (GTK slapt-get, an APT like system for Slackware)\n",
                        "",
                        "",
                        "",
                        ".tgz",
                        115,
                        440,
                        SLAPT_PRIORITY_DEFAULT,
                        SLAPT_TRUE
  };

  /* mirror_pkg1 has a higher priority, and should win */
  fail_unless (slapt_cmp_pkgs(&mirror_pkg1,&mirror_pkg2) == 1);

  /* both have the same priority, mirror_pkg2 has a higher version and should win */
  mirror_pkg1.priority = SLAPT_PRIORITY_DEFAULT;
  fail_unless (slapt_cmp_pkgs(&mirror_pkg1,&mirror_pkg2) == -1);

  /* installed_pkg and mirror_pkg1 have the exact same version and should be
     equal regardless of priority */
  fail_unless (slapt_cmp_pkgs(&installed_pkg,&mirror_pkg1) == 0);

  /* installed_pkg has a higher priority and should win, regardless of the
     fact that mirror_pkg2 has a higher version */
  installed_pkg.priority = SLAPT_PRIORITY_PREFERRED;
  fail_unless (slapt_cmp_pkgs(&installed_pkg,&mirror_pkg2) == 1);

  /* when the priorities are the same, the package with the higher version
     always wins */
  installed_pkg.priority = SLAPT_PRIORITY_DEFAULT;
  fail_unless (slapt_cmp_pkgs(&installed_pkg,&mirror_pkg2) == -1);

}
END_TEST

START_TEST (test_version)
{
  fail_unless (slapt_cmp_pkg_versions("3","3") == 0);
  fail_unless (slapt_cmp_pkg_versions("4","3") > 0);
  fail_unless (slapt_cmp_pkg_versions("3","4") < 0);

  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486-1","3.8.1-i486-1") == 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486-1jsw","3.8.1-i486-1") == 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i586-1","3.8.1-i486-1") == 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i586-1","3.8.1-i686-1") == 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486","3.8.1-i486") == 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486-1","3.8.1-i486-1") == 0);

  fail_unless (slapt_cmp_pkg_versions("3.8.1p1-i486-1","3.8p1-i486-1") > 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486-1","3.8-i486-1") > 0);
  fail_unless (slapt_cmp_pkg_versions("3.8.1-i486-1","3.8-i486-3") > 0);

  fail_unless (slapt_cmp_pkg_versions("3.8.1_1-i486-1","3.8.1_2-i486-1") < 0);

  fail_unless (slapt_cmp_pkg_versions("IIIalpha9.8-i486-2","IIIalpha9.7-i486-3") > 0);
  fail_unless (slapt_cmp_pkg_versions("4.13b-i386-2","4.12b-i386-1") > 0);
  fail_unless (slapt_cmp_pkg_versions("4.13b-i386-2","4.13a-i386-2") > 0);
  fail_unless (slapt_cmp_pkg_versions("1.4rc5-i486-2","1.4rc4-i486-2") > 0);

  fail_unless (slapt_cmp_pkg_versions("1.3.35-i486-2_slack10.2","1.3.35-i486-1") > 0);
}
END_TEST


START_TEST (test_dependency)
{
  /*
    resolve dependencies
    returns 0 on success, -1 on error setting conflict_err and missing_err
    (usually called with transaction->conflict_err and transaction->missing_err)
  int slapt_get_pkg_dependencies(const slapt_rc_config *global_config,
                           struct slapt_pkg_list *avail_pkgs,
                           struct slapt_pkg_list *installed_pkgs,
                           slapt_pkg_info_t *pkg,
                           struct slapt_pkg_list *deps,
                           struct slapt_pkg_err_list *conflict_err,
                           struct slapt_pkg_err_list *missing_err);
  */

  /*
    return list of package conflicts
  struct slapt_pkg_list *slapt_get_pkg_conflicts(struct slapt_pkg_list *avail_pkgs,
                                                 struct slapt_pkg_list *installed_pkgs,
                                                 slapt_pkg_info_t *pkg);
  */

  /*
    return list of packages required by
  struct slapt_pkg_list *slapt_is_required_by(const slapt_rc_config *global_config,
                                              struct slapt_pkg_list *avail,
                                              slapt_pkg_info_t *pkg);
  */
}
END_TEST


START_TEST (test_cache)
{
  slapt_rc_config *rc         = slapt_read_rc_config("./data/rc1");

  slapt_clean_pkg_dir(rc->working_dir);

  /* not easily testable due to timeout
  struct slapt_pkg_list *list = slapt_init_pkg_list();
  slapt_purge_old_cached_pkgs(rc, NULL, list);
  */

  slapt_free_rc_config(rc);
}
END_TEST


START_TEST (test_error)
{
  struct slapt_pkg_err_list *list = slapt_init_pkg_err_list();

  slapt_add_pkg_err_to_list(list, "gslapt", "Server returned 404");

  fail_unless ( slapt_search_pkg_err_list(list, "gslapt", "Server returned 404") == 1);

  slapt_free_pkg_err_list(list);
}
END_TEST


START_TEST (test_network)
{
  char *cwd                   = get_current_dir_name();
  slapt_rc_config *rc         = slapt_read_rc_config("./data/rc1");
  rc->progress_cb             = _progress_cb; /* silence */

  /* must chdir to working dir */
  fail_unless (chdir(rc->working_dir) == 0);

  /* write pkg data to disk
  void slapt_write_pkg_data(const char *source_url,FILE *d_file,
                            struct slapt_pkg_list *pkgs);
  */

  /* download the PACKAGES.TXT and CHECKSUMS.md5 files
  struct slapt_pkg_list *slapt_get_pkg_source_packages (const slapt_rc_config *global_config,
                                                        const char *url);
  struct slapt_pkg_list *slapt_get_pkg_source_patches (const slapt_rc_config *global_config,
                                                       const char *url);
  FILE *slapt_get_pkg_source_checksums (const slapt_rc_config *global_config,
                                        const char *url);
  int slapt_get_pkg_source_changelog (const slapt_rc_config *global_config,
                                        const char *url);
  */

  fail_unless (slapt_update_pkg_cache(rc) == 0);

  fail_unless (chdir(cwd) == 0);
  slapt_free_rc_config(rc);
  free(cwd);
}
END_TEST



Suite *packages_test_suite()
{
  Suite *s = suite_create ("Packages");
  TCase *tc = tcase_create ("Packages");

  tcase_add_test (tc, test_struct_pkg);
  tcase_add_test (tc, test_pkg_info);
  tcase_add_test (tc, test_pkg_list);
  tcase_add_test (tc, test_pkg_search);
  tcase_add_test (tc, test_pkgtool);
  tcase_add_test (tc, test_pkg_version);
  tcase_add_test (tc, test_version);
  tcase_add_test (tc, test_dependency);
  tcase_add_test (tc, test_cache);
  tcase_add_test (tc, test_error);
  tcase_add_test (tc, test_network);

  suite_add_tcase (s, tc);
  return s;
}


