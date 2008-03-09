/*
 * Copyright (C) 2003-2008 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "main.h"

static int cmp_pkg_arch(const char *a,const char *b);

/* install pkg */
void slapt_pkg_action_install(const slapt_rc_config *global_config,
                              const slapt_pkg_action_args_t *action_args)
{
  unsigned int i;
  slapt_transaction_t *tran = NULL;
  struct slapt_pkg_list *installed_pkgs = NULL;
  struct slapt_pkg_list *avail_pkgs = NULL;
  slapt_regex_t *pkg_regex = NULL;

  printf( gettext("Reading Package Lists... ") );
  installed_pkgs = slapt_get_installed_pkgs();
  avail_pkgs = slapt_get_available_pkgs();

  if ( avail_pkgs == NULL || avail_pkgs->pkg_count == 0 )
    exit(EXIT_FAILURE);

  printf( gettext("Done\n") );

  tran = slapt_init_transaction();

  if ((pkg_regex = slapt_init_regex(SLAPT_PKG_LOG_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < action_args->count; ++i) {
    char *arg = action_args->pkgs[i];
    slapt_pkg_info_t *pkg = NULL;
    slapt_pkg_info_t *installed_pkg = NULL;

    /* Use regex to see if they specified a particular version */
    slapt_execute_regex(pkg_regex,arg);

    /* If so, parse it out and try to get that version only */
    if ( pkg_regex->reg_return == 0 ) {
      char *pkg_name,*pkg_version;

      pkg_name    = slapt_regex_extract_match(pkg_regex, arg, 1);
      pkg_version = slapt_regex_extract_match(pkg_regex, arg, 2);
      pkg         = slapt_get_exact_pkg(avail_pkgs, pkg_name, pkg_version);

      free(pkg_name);
      free(pkg_version);

    }

    /* If regex doesnt match */
    if ( pkg_regex->reg_return != 0 || pkg == NULL ) {
      /* make sure there is a package called arg */
      pkg = slapt_get_newest_pkg(avail_pkgs,arg);

      if ( pkg == NULL ) {
        fprintf(stderr,gettext("No such package: %s\n"),arg);
        continue;
      }

    }

    installed_pkg = slapt_get_newest_pkg(installed_pkgs,pkg->name);

    /* if it is not already installed, install it */
    if ( installed_pkg == NULL || global_config->no_upgrade == SLAPT_TRUE) {

      if ( slapt_add_deps_to_trans(global_config,tran,avail_pkgs,
                                   installed_pkgs,pkg) == 0 ) {
        slapt_pkg_info_t *conflicted_pkg = NULL;

        /* if there is a conflict, we schedule the conflict for removal */
        if ( (conflicted_pkg = slapt_is_conflicted(tran,avail_pkgs,
                                                   installed_pkgs,
                                                   pkg)) != NULL ) {
          slapt_add_remove_to_transaction(tran,conflicted_pkg);
        }
        slapt_add_install_to_transaction(tran,pkg);

      } else {
        printf(gettext("Excluding %s, use --ignore-dep to override\n"),
               pkg->name);
        slapt_add_exclude_to_transaction(tran,pkg);
      }

    } else { /* else we upgrade or reinstall */

      /* it is already installed, attempt an upgrade */
      if (
        ((slapt_cmp_pkgs(installed_pkg,pkg)) < 0) ||
        (global_config->re_install == SLAPT_TRUE)
      ) {

        if ( slapt_add_deps_to_trans(global_config,tran,avail_pkgs,
                                     installed_pkgs,pkg) == 0 ) {
          slapt_pkg_info_t *conflicted_pkg = NULL;

          if ( (conflicted_pkg = slapt_is_conflicted(tran,avail_pkgs,
                                                     installed_pkgs,
                                                     pkg)) != NULL ) {
            slapt_add_remove_to_transaction(tran,conflicted_pkg);
          }
          slapt_add_upgrade_to_transaction(tran,installed_pkg,pkg);

        } else {
          printf(gettext("Excluding %s, use --ignore-dep to override\n"),
                 pkg->name);
          slapt_add_exclude_to_transaction(tran,pkg);
        }

      } else {
        printf(gettext("%s is up to date.\n"),installed_pkg->name);
      }

    }

  }

  slapt_free_pkg_list(installed_pkgs);
  slapt_free_pkg_list(avail_pkgs);

  slapt_free_regex(pkg_regex);

  if (slapt_handle_transaction(global_config,tran) != 0) {
    exit(EXIT_FAILURE);
  }

  slapt_free_transaction(tran);
  return;
}

/* list pkgs */
void slapt_pkg_action_list(const int show)
{
  struct slapt_pkg_list *pkgs = NULL;
  struct slapt_pkg_list *installed_pkgs = NULL;
  unsigned int i;

  pkgs = slapt_get_available_pkgs();
  installed_pkgs = slapt_get_installed_pkgs();

  if ( show == LIST || show == AVAILABLE ) {
    for (i = 0; i < pkgs->pkg_count; ++i ) {
      unsigned int bool_installed = 0;
      char *short_description = slapt_gen_short_pkg_description(pkgs->pkgs[i]);

      /* is it installed? */
      if (slapt_get_exact_pkg(installed_pkgs,pkgs->pkgs[i]->name,
                              pkgs->pkgs[i]->version) != NULL )
        bool_installed = 1;
 
      printf("%s-%s [inst=%s]: %s\n",
        pkgs->pkgs[i]->name,
        pkgs->pkgs[i]->version,
          bool_installed == 1
        ? gettext("yes")
        : gettext("no"),
        (short_description == NULL) ? "" : short_description
      );
      free(short_description);
    }
  }
  if ( show == LIST || show == INSTALLED ) {
    for (i = 0; i < installed_pkgs->pkg_count;++i) {
      char *short_description = NULL;

      if ( show == LIST ) {

        if ( slapt_get_exact_pkg(pkgs,
            installed_pkgs->pkgs[i]->name,
            installed_pkgs->pkgs[i]->version
          ) != NULL 
        ) {
          continue;
        }

      }

      short_description =
        slapt_gen_short_pkg_description(installed_pkgs->pkgs[i]);
  
      printf("%s-%s [inst=%s]: %s\n",
        installed_pkgs->pkgs[i]->name,
        installed_pkgs->pkgs[i]->version,
        gettext("yes"),
        (short_description == NULL) ? "" : short_description
      );
      free(short_description);

    }
  }

  slapt_free_pkg_list(pkgs);
  slapt_free_pkg_list(installed_pkgs);

}

/* remove/uninstall pkg */
void slapt_pkg_action_remove(const slapt_rc_config *global_config,
                             const slapt_pkg_action_args_t *action_args)
{
  unsigned int i;
  struct slapt_pkg_list *installed_pkgs = NULL;
  struct slapt_pkg_list *avail_pkgs = NULL;
  slapt_regex_t *pkg_regex = NULL;
  slapt_transaction_t *tran = NULL;

  printf(gettext("Reading Package Lists... "));
  installed_pkgs = slapt_get_installed_pkgs();
  avail_pkgs = slapt_get_available_pkgs();
  printf(gettext("Done\n"));

  tran = slapt_init_transaction();
  if ((pkg_regex = slapt_init_regex(SLAPT_PKG_LOG_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < action_args->count; ++i) {
    unsigned int c;
    struct slapt_pkg_list *deps = NULL;
    char *arg = action_args->pkgs[i];
    slapt_pkg_info_t *pkg = NULL;

    /* Use regex to see if they specified a particular version */
    slapt_execute_regex(pkg_regex,arg);

    /* If so, parse it out and try to get that version only */
    if ( pkg_regex->reg_return == 0 ) {
      char *pkg_name,*pkg_version;

      pkg_name    = slapt_regex_extract_match(pkg_regex, arg, 1);
      pkg_version = slapt_regex_extract_match(pkg_regex, arg, 2);
      pkg         = slapt_get_exact_pkg(installed_pkgs, pkg_name, pkg_version);

      free(pkg_name);
      free(pkg_version);

    }

    /* If regex doesnt match */
    if ( pkg_regex->reg_return != 0 || pkg == NULL ) {
      /* make sure there is a package called arg */
      pkg = slapt_get_newest_pkg(installed_pkgs,arg);

      if ( pkg == NULL ) {
        printf(gettext("%s is not installed.\n"),arg);
        continue;
      }

    }

    deps = slapt_is_required_by(global_config,avail_pkgs,pkg);

    for (c = 0; c < deps->pkg_count; ++c) {
      slapt_pkg_info_t *dep = deps->pkgs[c];

      if ( slapt_get_exact_pkg(installed_pkgs,dep->name, dep->version) != NULL) {
        slapt_add_remove_to_transaction(tran,dep);
      }

    }

    slapt_free_pkg_list(deps);

    slapt_add_remove_to_transaction(tran,pkg);

  }

  if (global_config->remove_obsolete == SLAPT_TRUE) {
    struct slapt_pkg_list *obsolete = slapt_get_obsolete_pkgs(
      global_config, avail_pkgs, installed_pkgs);

    for (i = 0; i < obsolete->pkg_count; ++i) {
      if ( slapt_is_excluded(global_config,obsolete->pkgs[i]) != 1 ) {
        slapt_add_remove_to_transaction(tran,obsolete->pkgs[i]);
      } else {
        slapt_add_exclude_to_transaction(tran,obsolete->pkgs[i]);
      }
    }

    slapt_free_pkg_list(obsolete);
  }

  slapt_free_pkg_list(installed_pkgs);
  slapt_free_pkg_list(avail_pkgs);
  slapt_free_regex(pkg_regex);

  if (slapt_handle_transaction(global_config,tran) != 0) {
    exit(EXIT_FAILURE);
  }

  slapt_free_transaction(tran);
}

/* search for a pkg (support extended POSIX regex) */
void slapt_pkg_action_search(const char *pattern)
{
  unsigned int i;
  struct slapt_pkg_list *pkgs = NULL;
  struct slapt_pkg_list *installed_pkgs = NULL;
  struct slapt_pkg_list *matches = NULL,*i_matches = NULL;

  /* read in pkg data */
  pkgs = slapt_get_available_pkgs();
  installed_pkgs = slapt_get_installed_pkgs();

  matches = slapt_search_pkg_list(pkgs,pattern);
  i_matches = slapt_search_pkg_list(installed_pkgs,pattern);

  for (i = 0; i < matches->pkg_count; ++i) {
    char *short_description =
      slapt_gen_short_pkg_description(matches->pkgs[i]);

    printf("%s-%s [inst=%s]: %s\n",
      matches->pkgs[i]->name,
      matches->pkgs[i]->version,
      ( slapt_get_exact_pkg( installed_pkgs,
        matches->pkgs[i]->name,matches->pkgs[i]->version)
      != NULL )
      ? gettext("yes")
      : gettext("no"),
      short_description
    );
    free(short_description);
  }

  for (i = 0; i < i_matches->pkg_count; ++i) {
    char *short_description = NULL;

    if ( slapt_get_exact_pkg(matches,i_matches->pkgs[i]->name,
         i_matches->pkgs[i]->version) != NULL) {
        continue;
    }

    short_description = slapt_gen_short_pkg_description(i_matches->pkgs[i]);

    printf("%s-%s [inst=%s]: %s\n",
      i_matches->pkgs[i]->name,
      i_matches->pkgs[i]->version,
      gettext("yes"),
      short_description
    );
    free(short_description);
  }

  slapt_free_pkg_list(matches);
  slapt_free_pkg_list(i_matches);
  slapt_free_pkg_list(pkgs);
  slapt_free_pkg_list(installed_pkgs);

}

/* show the details for a specific package */
void slapt_pkg_action_show(const char *pkg_name)
{
  struct slapt_pkg_list *avail_pkgs = NULL;
  struct slapt_pkg_list *installed_pkgs = NULL;
  slapt_regex_t *pkg_regex = NULL;
  unsigned int bool_installed = 0;
  slapt_pkg_info_t *pkg = NULL;

  avail_pkgs = slapt_get_available_pkgs();
  installed_pkgs = slapt_get_installed_pkgs();

  if ( avail_pkgs == NULL || installed_pkgs == NULL )
    exit(EXIT_FAILURE);

  if ((pkg_regex = slapt_init_regex(SLAPT_PKG_LOG_PATTERN)) == NULL)
    exit(EXIT_FAILURE);

  /* Use regex to see if they specified a particular version */
  slapt_execute_regex(pkg_regex,pkg_name);

  /* If so, parse it out and try to get that version only */
  if ( pkg_regex->reg_return == 0 ) {
    char *p_name,*p_version;

    p_name    = slapt_regex_extract_match(pkg_regex, pkg_name, 1);
    p_version = slapt_regex_extract_match(pkg_regex, pkg_name, 2);

    pkg = slapt_get_exact_pkg(avail_pkgs, p_name, p_version);

    if ( pkg == NULL )
      pkg = slapt_get_exact_pkg(installed_pkgs,p_name,p_version);

    free(p_name);
    free(p_version);

  } else {
    slapt_pkg_info_t *installed_pkg = slapt_get_newest_pkg(installed_pkgs,pkg_name);
    pkg = slapt_get_newest_pkg(avail_pkgs,pkg_name);
    if ( pkg == NULL )
      pkg = installed_pkg;
    else if (pkg != NULL && installed_pkg != NULL) {
        if (slapt_cmp_pkgs(installed_pkg,pkg) > 0)
           pkg = installed_pkg;
    }
  }

  if ( pkg != NULL ) {
    char *changelog = slapt_get_pkg_changelog(pkg);
    char *description = strdup(pkg->description);
    slapt_clean_description(description,pkg->name);

    if ( slapt_get_exact_pkg(installed_pkgs,pkg->name,pkg->version) != NULL)
      bool_installed = 1;

    printf(gettext("Package Name: %s\n"),pkg->name);
    printf(gettext("Package Mirror: %s\n"),pkg->mirror);
    printf(gettext("Package Location: %s\n"),pkg->location);
    printf(gettext("Package Version: %s\n"),pkg->version);
    printf(gettext("Package Size: %d K\n"),pkg->size_c);
    printf(gettext("Package Installed Size: %d K\n"),pkg->size_u);
    printf(gettext("Package Required: %s\n"),pkg->required);
    printf(gettext("Package Conflicts: %s\n"),pkg->conflicts);
    printf(gettext("Package Suggests: %s\n"),pkg->suggests);
    printf(gettext("Package MD5 Sum:  %s\n"),pkg->md5);
    printf(gettext("Package Description:\n"));
    printf("%s",description);

    if (changelog != NULL) {
      printf("%s:\n",gettext("Package ChangeLog"));
      printf("%s\n\n", changelog);
      free(changelog);
    }

    printf(gettext("Package Installed: %s\n"),
      bool_installed == 1
        ? gettext("yes")
        : gettext("no")
    );

    free(description);
  } else {
    printf(gettext("No such package: %s\n"),pkg_name);
  }

  slapt_free_pkg_list(avail_pkgs);
  slapt_free_pkg_list(installed_pkgs);
  slapt_free_regex(pkg_regex);
}

/* upgrade all installed pkgs with available updates */
void slapt_pkg_action_upgrade_all(const slapt_rc_config *global_config)
{
  unsigned int i;
  struct slapt_pkg_list *installed_pkgs = NULL;
  struct slapt_pkg_list *avail_pkgs = NULL;
  slapt_transaction_t *tran = NULL;

  printf(gettext("Reading Package Lists... "));
  installed_pkgs = slapt_get_installed_pkgs();
  avail_pkgs = slapt_get_available_pkgs();

  if ( avail_pkgs == NULL || installed_pkgs == NULL )
    exit(EXIT_FAILURE);

  if ( avail_pkgs->pkg_count == 0 )
    exit(EXIT_FAILURE);

  printf(gettext("Done\n"));
  tran = slapt_init_transaction();

  if ( global_config->dist_upgrade == SLAPT_TRUE ) {
    char *essential[] = {"glibc-solibs","sed","pkgtools",NULL};
    int epi = 0;
    struct slapt_pkg_list *matches =
      slapt_search_pkg_list(avail_pkgs,SLAPT_SLACK_BASE_SET_REGEX);

    /* make sure the essential packages are installed/upgraded first */
    while ( essential[epi] != NULL ) {
      slapt_pkg_info_t *inst_pkg = NULL;
      slapt_pkg_info_t *avail_pkg = NULL;

      inst_pkg = slapt_get_newest_pkg(installed_pkgs,essential[epi]);
      avail_pkg = slapt_get_newest_pkg(avail_pkgs,essential[epi]);

      /* can we upgrade */
      if ( inst_pkg != NULL && avail_pkg != NULL ) {
        if (slapt_cmp_pkgs(inst_pkg,avail_pkg) < 0 ) {
          slapt_add_upgrade_to_transaction(tran,inst_pkg,avail_pkg);
        }
      /* if not try to install */
      } else if ( avail_pkg != NULL ) {
        slapt_add_install_to_transaction(tran,avail_pkg);
      }

      ++epi;
    }

    /* loop through SLAPT_SLACK_BASE_SET_REGEX packages */
    for (i = 0; i < matches->pkg_count; ++i) {
      slapt_pkg_info_t *installed_pkg = NULL;
      slapt_pkg_info_t *newer_avail_pkg = NULL;
      slapt_pkg_info_t *slapt_upgrade_pkg = NULL;

      installed_pkg = slapt_get_newest_pkg(
        installed_pkgs,
        matches->pkgs[i]->name
      );
      newer_avail_pkg = slapt_get_newest_pkg(
        avail_pkgs,
        matches->pkgs[i]->name
      );
      /*
        * if there is a newer available version (such as from patches/)
        * use it instead
      */
      if (slapt_cmp_pkgs(matches->pkgs[i],newer_avail_pkg) < 0 ) {
        slapt_upgrade_pkg = newer_avail_pkg;
      } else {
        slapt_upgrade_pkg = matches->pkgs[i];
      }

      /* add to install list if not already installed */
      if ( installed_pkg == NULL ) {
        if ( slapt_is_excluded(global_config,slapt_upgrade_pkg) == 1 ) {
          slapt_add_exclude_to_transaction(tran,slapt_upgrade_pkg);
        } else {

          /* add install if all deps are good and it doesn't have conflicts */
          if (
            (slapt_add_deps_to_trans(global_config,tran,avail_pkgs,
                                     installed_pkgs,slapt_upgrade_pkg) == 0)
            && ( slapt_is_conflicted(tran,avail_pkgs,installed_pkgs,
                                     slapt_upgrade_pkg) == NULL )
          ) {
            slapt_add_install_to_transaction(tran,slapt_upgrade_pkg);
          } else {
            /* otherwise exclude */
            printf(gettext("Excluding %s, use --ignore-dep to override\n"),
                   slapt_upgrade_pkg->name);
            slapt_add_exclude_to_transaction(tran,slapt_upgrade_pkg);
          }

        }
      /* even if it's installed, check to see that the packages are different */
      /* simply running a version comparison won't do it since sometimes the */
      /* arch is the only thing that changes */
      } else if (
        (slapt_cmp_pkgs(installed_pkg,slapt_upgrade_pkg) <= 0) &&
        strcmp(installed_pkg->version,slapt_upgrade_pkg->version) != 0
      ) {

        if ( slapt_is_excluded(global_config,installed_pkg) == 1 
            || slapt_is_excluded(global_config,slapt_upgrade_pkg) == 1 ) {
          slapt_add_exclude_to_transaction(tran,slapt_upgrade_pkg);
        } else {
          /* if all deps are added and there is no conflicts, add on */
          if (
            (slapt_add_deps_to_trans(global_config,tran,avail_pkgs,
                                     installed_pkgs,slapt_upgrade_pkg) == 0)
            && ( slapt_is_conflicted(tran,avail_pkgs,installed_pkgs,
                                     slapt_upgrade_pkg) == NULL )
          ) {
            slapt_add_upgrade_to_transaction(tran,installed_pkg,
                                             slapt_upgrade_pkg);
          } else {
            /* otherwise exclude */
            printf(gettext("Excluding %s, use --ignore-dep to override\n"),
                   slapt_upgrade_pkg->name);
            slapt_add_exclude_to_transaction(tran,slapt_upgrade_pkg);
          }
        }

      }

    }

    slapt_free_pkg_list(matches);

    /* remove obsolete packages if prompted to */
    if ( global_config->remove_obsolete == SLAPT_TRUE ) {
      unsigned int r;
      struct slapt_pkg_list *obsolete = slapt_get_obsolete_pkgs(
        global_config, avail_pkgs, installed_pkgs);

      for (r = 0; r < obsolete->pkg_count; ++r) {

        if ( slapt_is_excluded(global_config,obsolete->pkgs[r]) != 1 ) {
          slapt_add_remove_to_transaction(tran,obsolete->pkgs[r]);
        } else {
          slapt_add_exclude_to_transaction(tran,obsolete->pkgs[r]);
        }

      }

      slapt_free_pkg_list(obsolete);

    }/* end if remove_obsolete */

    /* insurance so that all of slapt-get's requirements are also installed */
    slapt_add_deps_to_trans(
      global_config,tran,avail_pkgs,installed_pkgs,
      slapt_get_newest_pkg(avail_pkgs,"slapt-get")
    );
    
  }

  for (i = 0; i < installed_pkgs->pkg_count; ++i) {
    slapt_pkg_info_t *update_pkg = NULL;
    slapt_pkg_info_t *newer_installed_pkg = NULL;

    /*
      we need to see if there is another installed
      package that is newer than this one
    */
    if ((newer_installed_pkg =
            slapt_get_newest_pkg(installed_pkgs,
            installed_pkgs->pkgs[i]->name)) != NULL) {
      if (slapt_cmp_pkgs(installed_pkgs->pkgs[i],newer_installed_pkg) < 0) {
        continue;
      }
    }

    /* see if we have an available update for the pkg */
    update_pkg = slapt_get_newest_pkg(
      avail_pkgs,
      installed_pkgs->pkgs[i]->name
    );
    if ( update_pkg != NULL ) {
      int cmp_r = 0;

      /* if the update has a newer version, attempt to upgrade */
      cmp_r = slapt_cmp_pkgs(installed_pkgs->pkgs[i],update_pkg);
      if (
        /* either it's greater, or we want to reinstall */
        cmp_r < 0 || (global_config->re_install == SLAPT_TRUE) ||
        /* 
          * or this is a dist upgrade and the versions are the save
          * except for the arch
        */
        (
          global_config->dist_upgrade == SLAPT_TRUE &&
          cmp_r == 0 &&
          cmp_pkg_arch(installed_pkgs->pkgs[i]->version,
                       update_pkg->version) != 0
        )
      ) {

        if ( (slapt_is_excluded(global_config,update_pkg) == 1)
          || (slapt_is_excluded(global_config,installed_pkgs->pkgs[i]) == 1)
        ) {
          slapt_add_exclude_to_transaction(tran,update_pkg);
        } else {
          /* if all deps are added and there is no conflicts, add on */
          if (
            (slapt_add_deps_to_trans(global_config,tran,avail_pkgs,
                                     installed_pkgs,update_pkg) == 0)
            && ( slapt_is_conflicted(tran,avail_pkgs,installed_pkgs,
                                     update_pkg) == NULL )
          ) {
            slapt_add_upgrade_to_transaction(tran,installed_pkgs->pkgs[i],
                                             update_pkg);
          } else {
            /* otherwise exclude */
            printf(gettext("Excluding %s, use --ignore-dep to override\n"),
                   update_pkg->name);
            slapt_add_exclude_to_transaction(tran,update_pkg);
          }
        }

      }

    }/* end upgrade pkg found */

  }/* end for */

  slapt_free_pkg_list(installed_pkgs);
  slapt_free_pkg_list(avail_pkgs);

  if (slapt_handle_transaction(global_config,tran) != 0) {
    exit(EXIT_FAILURE);
  }

  slapt_free_transaction(tran);
}

slapt_pkg_action_args_t *slapt_init_pkg_action_args(int arg_count)
{
  slapt_pkg_action_args_t *paa;

  paa = slapt_malloc( sizeof *paa );
  paa->pkgs = slapt_malloc( sizeof *paa->pkgs * arg_count );
  paa->count = 0;

  return paa;
}

slapt_pkg_action_args_t *slapt_add_pkg_action_args(slapt_pkg_action_args_t *p,
                                                   const char *arg)
{
  p->pkgs[p->count] = slapt_malloc(
    ( strlen(arg) + 1 ) * sizeof *p->pkgs[p->count]
  );
  memcpy(p->pkgs[p->count],arg,strlen(arg) + 1);
  ++p->count;
  return p;
}

void slapt_free_pkg_action_args(slapt_pkg_action_args_t *paa)
{
  unsigned int i;

  for (i = 0; i < paa->count; ++i) {
    free(paa->pkgs[i]);
  }

  free(paa->pkgs);
  free(paa);
}

static int cmp_pkg_arch(const char *a,const char *b)
{
  int r = 0;
  slapt_regex_t *a_arch_regex = NULL, *b_arch_regex = NULL;

  if ((a_arch_regex = slapt_init_regex(SLAPT_PKG_VER)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((b_arch_regex = slapt_init_regex(SLAPT_PKG_VER)) == NULL) {
    exit(EXIT_FAILURE);
  }

  slapt_execute_regex(a_arch_regex,a);
  slapt_execute_regex(b_arch_regex,b);

  if (a_arch_regex->reg_return != 0 || a_arch_regex->reg_return != 0) {

    slapt_free_regex(a_arch_regex);
    slapt_free_regex(b_arch_regex);

    return strcmp(a,b);

  } else {
    char *a_arch = slapt_regex_extract_match(a_arch_regex, a, 2);
    char *b_arch = slapt_regex_extract_match(a_arch_regex, b, 2);

    r = strcmp(a_arch,b_arch);

    free(a_arch);
    free(b_arch);
  }

  slapt_free_regex(a_arch_regex);
  slapt_free_regex(b_arch_regex);

  return r;
}

#ifdef SLAPT_HAS_GPGME
void slapt_pkg_action_add_keys(const slapt_rc_config *global_config)
{
  unsigned int s = 0, compressed = 0;
  for(s = 0; s < global_config->sources->count; s++)
  {
    FILE *gpg_key = NULL;
    printf(gettext("Retrieving GPG key [%s]..."), global_config->sources->url[s]);
    gpg_key = slapt_get_pkg_source_gpg_key (global_config,
                                            global_config->sources->url[s],
                                            &compressed);
    if (gpg_key != NULL)
    {
      slapt_code_t r = slapt_add_pkg_source_gpg_key(gpg_key);
      if (r == SLAPT_GPG_KEY_UNCHANGED) {
        printf("%s.\n",gettext("GPG key already present"));
      } else if (r == SLAPT_GPG_KEY_IMPORTED) {
        printf("%s.\n",gettext("GPG key successfully imported"));
      } else {
        printf("%s.\n",gettext("GPG key could not be imported"));
      }
      
      fclose(gpg_key);
    }

  }

}
#endif
