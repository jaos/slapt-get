/*
 * Copyright (C) 2003-2016 Jason Woodward <woodwardj at jaos dot org>
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

/*
  this defines the max length of the transaction summary lines, hopefully
  someday this will be replaced with a more dynamic solution.
*/
#define MAX_LINE_LEN 80

typedef struct {
  union { slapt_pkg_info_t *i; slapt_pkg_upgrade_t *u; } pkg;
  unsigned int type; /* this is enum slapt_action defined in main.h */
} slapt_queue_i;

typedef struct {
  slapt_queue_i **pkgs;
  unsigned int count;
} slapt_queue_t;

typedef struct {
  slapt_pkg_list_t *install_pkgs;
  slapt_pkg_upgrade_list_t *upgrade_pkgs;
  slapt_pkg_list_t *remove_pkgs;
  slapt_pkg_list_t *exclude_pkgs;
  slapt_list_t *suggests;
  slapt_pkg_err_list_t *conflict_err;
  slapt_pkg_err_list_t *missing_err;
  slapt_queue_t *queue;
} slapt_transaction_t;

/* fill in transaction structure with defaults */
slapt_transaction_t *slapt_init_transaction(void);
/*
  download and install/remove/upgrade packages as defined in the transaction
  returns 0 on success
*/
int slapt_handle_transaction(const slapt_rc_config *,slapt_transaction_t *);

/* add package for installation to transaction */
void slapt_add_install_to_transaction(slapt_transaction_t *,
                                      slapt_pkg_info_t *pkg);
/* add package for removal to transaction */
void slapt_add_remove_to_transaction(slapt_transaction_t *,
                                     slapt_pkg_info_t *pkg);
/* add package to upgrade to transaction */
void slapt_add_upgrade_to_transaction(slapt_transaction_t *,
                                      slapt_pkg_info_t *installed_pkg,
                                      slapt_pkg_info_t *upgrade_pkg);
/* add package to reinstall to transaction */
void slapt_add_reinstall_to_transaction(slapt_transaction_t *,
                                      slapt_pkg_info_t *installed_pkg,
                                      slapt_pkg_info_t *upgrade_pkg);
/* add package to exclude to transaction */
void slapt_add_exclude_to_transaction(slapt_transaction_t *,
                                      slapt_pkg_info_t *pkg);
/* remove package from transaction, returns modified transaction */
slapt_transaction_t *slapt_remove_from_transaction(slapt_transaction_t *tran,
                                                   slapt_pkg_info_t *pkg);

/* search transaction by package name.  returns 1 if found, 0 otherwise */
int slapt_search_transaction(slapt_transaction_t *,char *pkg_name);
/*
  search transaction by package attributes
  returns 1 if found, 0 otherwise
*/
int slapt_search_transaction_by_pkg(slapt_transaction_t *tran,
                                    slapt_pkg_info_t *pkg);
/*
  searches the upgrade list of the transaction for the present of the package
  returns 1 if found, 0 if not found
*/
int slapt_search_upgrade_transaction(slapt_transaction_t *tran,
                                     slapt_pkg_info_t *pkg);

/*
  add dependencies for package to transaction, returns -1 on error, 0 otherwise
*/
int slapt_add_deps_to_trans(const slapt_rc_config *global_config,
                            slapt_transaction_t *tran,
                            slapt_pkg_list_t *avail_pkgs,
                            slapt_pkg_list_t *installed_pkgs, slapt_pkg_info_t *pkg);

/*
  check to see if a package has a conflict already present in the transaction
  returns conflicted package or NULL if none
*/
slapt_pkg_list_t *slapt_is_conflicted(slapt_transaction_t *tran,
                                      slapt_pkg_list_t *avail_pkgs,
                                      slapt_pkg_list_t *installed_pkgs,
                                      slapt_pkg_info_t *pkg);

/*
  generate a list of suggestions based on the current packages
  in the transaction
*/
void slapt_generate_suggestions(slapt_transaction_t *tran);

/* free the transaction structure and it's members */
void slapt_free_transaction(slapt_transaction_t *);
