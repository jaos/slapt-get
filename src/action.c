/*
 * Copyright (C) 2003,2004,2005 Jason Woodward <woodwardj at jaos dot org>
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

#include <main.h>

/* install pkg */
void pkg_action_install(const rc_config *global_config,
                        const pkg_action_args_t *action_args)
{
  unsigned int i;
  transaction_t tran;
  struct pkg_list *installed_pkgs = NULL;
  struct pkg_list *avail_pkgs = NULL;
  sg_regex pkg_regex;

  printf( _("Reading Package Lists... ") );
  installed_pkgs = get_installed_pkgs();
  avail_pkgs = get_available_pkgs();

  if ( avail_pkgs == NULL || avail_pkgs->pkg_count == 0 )
    exit(1);

  printf( _("Done\n") );

  init_transaction(&tran);

  init_regex(&pkg_regex,PKG_LOG_PATTERN);

  for (i = 0; i < action_args->count; i++) {
    pkg_info_t *pkg = NULL;
    pkg_info_t *installed_pkg = NULL;

    /* Use regex to see if they specified a particular version */
    execute_regex(&pkg_regex,action_args->pkgs[i]);

    /* If so, parse it out and try to get that version only */
    if ( pkg_regex.reg_return == 0 ) {
      char *pkg_name,*pkg_version;

      pkg_name = strndup(
        action_args->pkgs[i] + pkg_regex.pmatch[1].rm_so,
        pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
      );

      pkg_version = strndup(
        action_args->pkgs[i] + pkg_regex.pmatch[2].rm_so,
        pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
      );

      pkg = get_exact_pkg(avail_pkgs, pkg_name, pkg_version);
      free(pkg_name);
      free(pkg_version);

    }

    /* If regex doesnt match */
    if ( pkg_regex.reg_return != 0 || pkg == NULL ) {
      /* make sure there is a package called action_args->pkgs[i] */
      pkg = get_newest_pkg(avail_pkgs,action_args->pkgs[i]);

      if ( pkg == NULL ) {
        fprintf(stderr,_("No such package: %s\n"),action_args->pkgs[i]);
        continue;
      }

    }

    installed_pkg = get_newest_pkg(installed_pkgs,pkg->name);

    /* if it is not already installed, install it */
    if ( installed_pkg == NULL ) {

      if ( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 ) {
        pkg_info_t *conflicted_pkg = NULL;

        /* if there is a conflict, we schedule the conflict for removal */
        if ( (conflicted_pkg = is_conflicted(&tran,avail_pkgs,installed_pkgs,pkg)) != NULL ) {
          add_remove_to_transaction(&tran,conflicted_pkg);
        }
        add_install_to_transaction(&tran,pkg);

      } else {
        add_exclude_to_transaction(&tran,pkg);
      }

    } else { /* else we upgrade or reinstall */

      /* it is already installed, attempt an upgrade */
      if (
        ((cmp_pkg_versions(installed_pkg->version,pkg->version)) < 0) ||
        (global_config->re_install == TRUE)
      ) {

        if ( add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,pkg) == 0 ) {
          pkg_info_t *conflicted_pkg = NULL;

          if ( (conflicted_pkg = is_conflicted(&tran,avail_pkgs,installed_pkgs,pkg)) != NULL ) {
            add_remove_to_transaction(&tran,conflicted_pkg);
          }
          add_upgrade_to_transaction(&tran,installed_pkg,pkg);

        } else {
          add_exclude_to_transaction(&tran,pkg);
        }

      } else {
        printf(_("%s is up to date.\n"),installed_pkg->name);
      }

    }

  }

  free_pkg_list(installed_pkgs);
  free_pkg_list(avail_pkgs);

  free_regex(&pkg_regex);

  handle_transaction(global_config,&tran);

  free_transaction(&tran);
  return;
}

/* list pkgs */
void pkg_action_list(const int show)
{
  struct pkg_list *pkgs = NULL;
  struct pkg_list *installed_pkgs = NULL;
  unsigned int i;

  pkgs = get_available_pkgs();
  installed_pkgs = get_installed_pkgs();

  if ( show == LIST || show == AVAILABLE ) {
    for (i = 0; i < pkgs->pkg_count; i++ ) {
      unsigned int bool_installed = 0;
      char *short_description = gen_short_pkg_description(pkgs->pkgs[i]);

      /* is it installed? */
      if ( get_exact_pkg(installed_pkgs,pkgs->pkgs[i]->name,pkgs->pkgs[i]->version) != NULL )
        bool_installed = 1;
 
      printf("%s %s [inst=%s]: %s\n",
        pkgs->pkgs[i]->name,
        pkgs->pkgs[i]->version,
          bool_installed == 1
        ? _("yes")
        : _("no"),
        (short_description == NULL) ? "" : short_description
      );
      free(short_description);
    }
  }
  if ( show == LIST || show == INSTALLED ) {
    for (i = 0; i < installed_pkgs->pkg_count;++i) {
      char *short_description = NULL;

      if ( show == LIST ) {

        if ( get_exact_pkg(pkgs,
            installed_pkgs->pkgs[i]->name,
            installed_pkgs->pkgs[i]->version
          ) != NULL 
        ) {
          continue;
        }

      }

      short_description = gen_short_pkg_description(installed_pkgs->pkgs[i]);
  
      printf("%s %s [inst=%s]: %s\n",
        installed_pkgs->pkgs[i]->name,
        installed_pkgs->pkgs[i]->version,
        _("yes"),
        (short_description == NULL) ? "" : short_description
      );
      free(short_description);

    }
  }

  free_pkg_list(pkgs);
  free_pkg_list(installed_pkgs);

}

/* remove/uninstall pkg */
void pkg_action_remove(const rc_config *global_config,
                       const pkg_action_args_t *action_args)
{
  unsigned int i;
  struct pkg_list *installed_pkgs = NULL;
  struct pkg_list *avail_pkgs = NULL;
  sg_regex pkg_regex;
  transaction_t tran;

  installed_pkgs = get_installed_pkgs();
  avail_pkgs = get_available_pkgs();
  init_transaction(&tran);
  init_regex(&pkg_regex,PKG_LOG_PATTERN);

  for (i = 0; i < action_args->count; i++) {
    unsigned int c;
    struct pkg_list *deps = NULL;
    pkg_info_t *pkg = NULL;

    /* Use regex to see if they specified a particular version */
    execute_regex(&pkg_regex,action_args->pkgs[i]);

    /* If so, parse it out and try to get that version only */
    if ( pkg_regex.reg_return == 0 ) {
      char *pkg_name,*pkg_version;

      pkg_name = strndup(
        action_args->pkgs[i] + pkg_regex.pmatch[1].rm_so,
        pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
      );

      pkg_version = strndup(
        action_args->pkgs[i] + pkg_regex.pmatch[2].rm_so,
        pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
      );

      pkg = get_exact_pkg(installed_pkgs, pkg_name, pkg_version);
      free(pkg_name);
      free(pkg_version);

    }

    /* If regex doesnt match */
    if ( pkg_regex.reg_return != 0 || pkg == NULL ) {
      /* make sure there is a package called action_args->pkgs[i] */
      pkg = get_newest_pkg(installed_pkgs,action_args->pkgs[i]);

      if ( pkg == NULL ) {
        printf(_("%s is not installed.\n"),action_args->pkgs[i]);
        continue;
      }

    }

    deps = is_required_by(global_config,avail_pkgs,pkg);

    for (c = 0; c < deps->pkg_count;c++) {

      if ( get_exact_pkg(installed_pkgs,deps->pkgs[c]->name,
      deps->pkgs[c]->version) != NULL ) {
        add_remove_to_transaction(&tran,deps->pkgs[c]);
      }

    }

    free_pkg_list(deps);

    add_remove_to_transaction(&tran,pkg);

  }

  free_pkg_list(installed_pkgs);
  free_pkg_list(avail_pkgs);
  free_regex(&pkg_regex);

  handle_transaction(global_config,&tran);

  free_transaction(&tran);
}

/* search for a pkg (support extended POSIX regex) */
void pkg_action_search(const char *pattern)
{
  unsigned int i;
  struct pkg_list *pkgs = NULL;
  struct pkg_list *installed_pkgs = NULL;
  struct pkg_list *matches = NULL,*i_matches = NULL;

  /* read in pkg data */
  pkgs = get_available_pkgs();
  installed_pkgs = get_installed_pkgs();

  matches = search_pkg_list(pkgs,pattern);
  i_matches = search_pkg_list(installed_pkgs,pattern);

  for (i = 0; i < matches->pkg_count; ++i) {
    char *short_description = gen_short_pkg_description(matches->pkgs[i]);

    printf("%s %s [inst=%s]: %s\n",
      matches->pkgs[i]->name,
      matches->pkgs[i]->version,
      ( get_exact_pkg( installed_pkgs,
        matches->pkgs[i]->name,matches->pkgs[i]->version)
      != NULL )
      ? _("yes")
      : _("no"),
      short_description
    );
    free(short_description);
  }

  for (i = 0; i < i_matches->pkg_count; ++i) {
    char *short_description = NULL;

    if ( get_exact_pkg(matches,i_matches->pkgs[i]->name,
         i_matches->pkgs[i]->version) != NULL) {
        continue;
    }

    short_description = gen_short_pkg_description(i_matches->pkgs[i]);

    printf("%s %s [inst=%s]: %s\n",
      i_matches->pkgs[i]->name,
      i_matches->pkgs[i]->version,
      _("yes"),
      short_description
    );
    free(short_description);
  }

  free_pkg_list(matches);
  free_pkg_list(i_matches);
  free_pkg_list(pkgs);
  free_pkg_list(installed_pkgs);

}

/* show the details for a specific package */
void pkg_action_show(const char *pkg_name)
{
  struct pkg_list *avail_pkgs = NULL;
  struct pkg_list *installed_pkgs = NULL;
  sg_regex pkg_regex;
  unsigned int bool_installed = 0;
  pkg_info_t *pkg = NULL;

  avail_pkgs = get_available_pkgs();
  installed_pkgs = get_installed_pkgs();

  if ( avail_pkgs == NULL || installed_pkgs == NULL )
    exit(1);

  init_regex(&pkg_regex,PKG_LOG_PATTERN);

  /* Use regex to see if they specified a particular version */
  execute_regex(&pkg_regex,pkg_name);

  /* If so, parse it out and try to get that version only */
  if ( pkg_regex.reg_return == 0 ) {
    char *p_name,*p_version;

    p_name = strndup(
      pkg_name + pkg_regex.pmatch[1].rm_so,
      pkg_regex.pmatch[1].rm_eo - pkg_regex.pmatch[1].rm_so
    );

    p_version = strndup(
      pkg_name + pkg_regex.pmatch[2].rm_so,
      pkg_regex.pmatch[2].rm_eo - pkg_regex.pmatch[2].rm_so
    );

    pkg = get_exact_pkg(avail_pkgs, p_name, p_version);

    if ( pkg == NULL )
      pkg = get_exact_pkg(installed_pkgs,p_name,p_version);

    free(p_name);
    free(p_version);

  } else {
    pkg = get_newest_pkg(avail_pkgs,pkg_name);
    if ( pkg == NULL ) pkg = get_newest_pkg(installed_pkgs,pkg_name);
  }

  if ( pkg != NULL ) {

    if ( get_exact_pkg(installed_pkgs,pkg->name,pkg->version) != NULL)
      bool_installed = 1;

    printf(_("Package Name: %s\n"),pkg->name);
    printf(_("Package Mirror: %s\n"),pkg->mirror);
    printf(_("Package Location: %s\n"),pkg->location);
    printf(_("Package Version: %s\n"),pkg->version);
    printf(_("Package Size: %d K\n"),pkg->size_c);
    printf(_("Package Installed Size: %d K\n"),pkg->size_u);
    printf(_("Package Required: %s\n"),pkg->required);
    printf(_("Package Conflicts: %s\n"),pkg->conflicts);
    printf(_("Package Suggests: %s\n"),pkg->suggests);
    printf(_("Package MD5 Sum:  %s\n"),pkg->md5);
    printf(_("Package Description:\n"));
    printf("%s",pkg->description);
    printf(_("Package Installed: %s\n"),
      bool_installed == 1
        ? _("yes")
        : _("no")
    );

  } else {
    printf(_("No such package: %s\n"),pkg_name);
  }

  free_pkg_list(avail_pkgs);
  free_pkg_list(installed_pkgs);
  free_regex(&pkg_regex);
}

/* upgrade all installed pkgs with available updates */
void pkg_action_upgrade_all(const rc_config *global_config)
{
  unsigned int i;
  struct pkg_list *installed_pkgs = NULL;
  struct pkg_list *avail_pkgs = NULL;
  transaction_t tran;

  printf(_("Reading Package Lists... "));
  installed_pkgs = get_installed_pkgs();
  avail_pkgs = get_available_pkgs();

  if ( avail_pkgs == NULL || installed_pkgs == NULL )
    exit(1);

  if ( avail_pkgs->pkg_count == 0 )
    exit(1);

  printf(_("Done\n"));
  init_transaction(&tran);

  if ( global_config->dist_upgrade == TRUE ) {
    struct pkg_list *matches = search_pkg_list(avail_pkgs,SLACK_BASE_SET_REGEX);

    for (i = 0; i < matches->pkg_count; i++) {
      pkg_info_t *installed_pkg = NULL;
      pkg_info_t *newer_avail_pkg = NULL;
      pkg_info_t *upgrade_pkg = NULL;

      installed_pkg = get_newest_pkg(
        installed_pkgs,
        matches->pkgs[i]->name
      );
      newer_avail_pkg = get_newest_pkg(
        avail_pkgs,
        matches->pkgs[i]->name
      );
      /* if there is a newer available version (such as from patches/) use it instead */
      if ( cmp_pkg_versions(matches->pkgs[i]->version,newer_avail_pkg->version) < 0 ) {
        upgrade_pkg = newer_avail_pkg;
      } else {
        upgrade_pkg = matches->pkgs[i];
      }

      /* add to install list if not already installed */
      if ( installed_pkg == NULL ) {
        if ( is_excluded(global_config,upgrade_pkg) == 1 ) {
          add_exclude_to_transaction(&tran,upgrade_pkg);
        } else {

          /* add install if all deps are good and it doesn't have conflicts */
          if (
            (add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,upgrade_pkg) == 0)
            && ( is_conflicted(&tran,avail_pkgs,installed_pkgs,upgrade_pkg) == NULL )
          ) {
            add_install_to_transaction(&tran,upgrade_pkg);
          } else {
            /* otherwise exclude */
            add_exclude_to_transaction(&tran,upgrade_pkg);
          }

        }
      /* even if it's installed, check to see that the packages are different */
      /* simply running a version comparison won't do it since sometimes the */
      /* arch is the only thing that changes */
      }else if (
        (cmp_pkg_versions(installed_pkg->version,upgrade_pkg->version) <= 0) &&
        strcmp(installed_pkg->version,upgrade_pkg->version) != 0
      ) {

        if ( is_excluded(global_config,upgrade_pkg) == 1 ) {
          add_exclude_to_transaction(&tran,upgrade_pkg);
        } else {
          /* if all deps are added and there is no conflicts, add on */
          if (
            (add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,upgrade_pkg) == 0)
            && ( is_conflicted(&tran,avail_pkgs,installed_pkgs,upgrade_pkg) == NULL )
          ) {
            add_upgrade_to_transaction(&tran,installed_pkg,upgrade_pkg);
          } else {
            /* otherwise exclude */
            add_exclude_to_transaction(&tran,upgrade_pkg);
          }
        }

      }

    }

    free_pkg_list(matches);

    /* remove obsolete packages if prompted to */
    if ( global_config->remove_obsolete == TRUE ) {
      unsigned int r;

      for (r = 0; r < installed_pkgs->pkg_count; r++) {

        /* if we can't find the installed package in our available pkg list, it must be obsolete */
        if ( get_newest_pkg(avail_pkgs,installed_pkgs->pkgs[r]->name) == NULL ) {
            struct    pkg_list  *deps;
            unsigned  int       c;
            /*
              any packages that require this package we are about to remove should be
              scheduled to remove as well
            */
            deps = is_required_by(global_config,avail_pkgs,installed_pkgs->pkgs[r]);
            for (c = 0; c < deps->pkg_count; c++ ) {
              if ( get_exact_pkg(avail_pkgs,deps->pkgs[c]->name,
              deps->pkgs[c]->version) == NULL ) {
                if ( is_excluded(global_config,deps->pkgs[c]) != 1 ) {
                  add_remove_to_transaction(&tran,deps->pkgs[c]);
                } else {
                  add_exclude_to_transaction(&tran,deps->pkgs[c]);
                }
              }
            }
            free_pkg_list(deps);
            if ( is_excluded(global_config,installed_pkgs->pkgs[r]) != 1 ) {
              add_remove_to_transaction(&tran,installed_pkgs->pkgs[r]);
            } else {
              add_exclude_to_transaction(&tran,installed_pkgs->pkgs[r]);
            }
        }

      }

    }/* end if remove_obsolete */

    /* insurance so that all of slapt-get's requirements are also installed */
    add_deps_to_trans(
      global_config,&tran,avail_pkgs,installed_pkgs,
      get_newest_pkg(avail_pkgs,"slapt-get")
    );
    
  }

  for (i = 0; i < installed_pkgs->pkg_count;i++) {
    pkg_info_t *update_pkg = NULL;
    pkg_info_t *newer_installed_pkg = NULL;

    /*
      we need to see if there is another installed
      package that is newer than this one
    */
    if ( (newer_installed_pkg = get_newest_pkg(installed_pkgs,installed_pkgs->pkgs[i]->name)) != NULL ) {
      if ( cmp_pkg_versions(installed_pkgs->pkgs[i]->version,newer_installed_pkg->version) < 0 ) {
        continue;
      }
    }

    /* see if we have an available update for the pkg */
    update_pkg = get_newest_pkg(
      avail_pkgs,
      installed_pkgs->pkgs[i]->name
    );
    if ( update_pkg != NULL ) {
      int cmp_r = 0;

      /* if the update has a newer version, attempt to upgrade */
      cmp_r = cmp_pkg_versions(installed_pkgs->pkgs[i]->version,update_pkg->version);
      if (
        /* either it's greater, or we want to reinstall */
        cmp_r < 0 || (global_config->re_install == TRUE) ||
        /* or this is a dist upgrade and the versions are the save except for the arch */
        (
          global_config->dist_upgrade == TRUE &&
          cmp_r == 0 &&
          strcmp(installed_pkgs->pkgs[i]->version,update_pkg->version) != 0
        )
      ) {

        if ( (is_excluded(global_config,update_pkg) == 1)
          || (is_excluded(global_config,installed_pkgs->pkgs[i]) == 1)
        ) {
          add_exclude_to_transaction(&tran,update_pkg);
        } else {
          /* if all deps are added and there is no conflicts, add on */
          if (
            (add_deps_to_trans(global_config,&tran,avail_pkgs,installed_pkgs,update_pkg) == 0)
            && ( is_conflicted(&tran,avail_pkgs,installed_pkgs,update_pkg) == NULL )
          ) {
            add_upgrade_to_transaction(&tran,installed_pkgs->pkgs[i],update_pkg);
          } else {
            /* otherwise exclude */
            add_exclude_to_transaction(&tran,update_pkg);
          }
        }

      }

    }/* end upgrade pkg found */

  }/* end for */

  free_pkg_list(installed_pkgs);
  free_pkg_list(avail_pkgs);

  handle_transaction(global_config,&tran);

  free_transaction(&tran);
}

pkg_action_args_t *init_pkg_action_args(int arg_count)
{
  pkg_action_args_t *paa;

  paa = slapt_malloc( sizeof *paa );
  paa->pkgs = slapt_malloc( sizeof *paa->pkgs * arg_count );
  paa->count = 0;

  return paa;
}

void free_pkg_action_args(pkg_action_args_t *paa)
{
  unsigned int i;

  for (i = 0; i < paa->count; i++) {
    free(paa->pkgs[i]);
  }

  free(paa->pkgs);
  free(paa);
}

