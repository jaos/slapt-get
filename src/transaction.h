/*
 * Copyright (C) 2003-2022 Jason Woodward <woodwardj at jaos dot org>
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

enum slapt_action {
    SLAPT_ACTION_USAGE = 0,
    SLAPT_ACTION_UPDATE,
    SLAPT_ACTION_INSTALL,
    SLAPT_ACTION_REMOVE,
    SLAPT_ACTION_SHOW,
    SLAPT_ACTION_SEARCH,
    SLAPT_ACTION_UPGRADE,
    SLAPT_ACTION_LIST,
    SLAPT_ACTION_INSTALLED,
    SLAPT_ACTION_CLEAN,
    SLAPT_ACTION_SHOWVERSION,
    SLAPT_ACTION_AUTOCLEAN,
    SLAPT_ACTION_AVAILABLE,
    SLAPT_ACTION_INSTALL_DISK_SET,
    SLAPT_ACTION_FILELIST,
#ifdef SLAPT_HAS_GPGME
    SLAPT_ACTION_ADD_KEYS,
#endif
    SLAPT_ACTION_END
};

typedef struct {
    union {
        slapt_pkg_t *i;
        slapt_pkg_upgrade_t *u;
    } pkg;
    enum slapt_action type;
} slapt_queue_i;
slapt_queue_i *slapt_queue_i_init(slapt_pkg_t *, slapt_pkg_upgrade_t *);
void slapt_queue_i_free(slapt_queue_i *);

typedef struct {
    slapt_vector_t *install_pkgs;
    slapt_vector_t *upgrade_pkgs;
    slapt_vector_t *reinstall_pkgs;
    slapt_vector_t *remove_pkgs;
    slapt_vector_t *exclude_pkgs;
    slapt_vector_t *suggests;
    slapt_vector_t *conflict_err;
    slapt_vector_t *missing_err;
    slapt_vector_t *queue;
} slapt_transaction_t;

/* fill in transaction structure with defaults */
slapt_transaction_t *slapt_transaction_t_init(void);
/* free the transaction structure and it's members */
void slapt_transaction_t_free(slapt_transaction_t *);
/* download and install/remove/upgrade packages as defined in the transaction, returns 0 on success */
int slapt_transaction_t_run(const slapt_config_t *, slapt_transaction_t *);

/* add package for installation to transaction */
void slapt_transaction_t_add_install(slapt_transaction_t *, const slapt_pkg_t *pkg);
/* add package for removal to transaction */
void slapt_transaction_t_add_remove(slapt_transaction_t *, const slapt_pkg_t *pkg);
/* add package to upgrade to transaction */
void slapt_transaction_t_add_upgrade(slapt_transaction_t *, const slapt_pkg_t *installed_pkg, const slapt_pkg_t *upgrade_pkg);
/* add package to reinstall to transaction */
void slapt_transaction_t_add_reinstall(slapt_transaction_t *, const slapt_pkg_t *installed_pkg, const slapt_pkg_t *upgrade_pkg);
/* add package to exclude to transaction */
void slapt_transaction_t_add_exclude(slapt_transaction_t *, const slapt_pkg_t *pkg);
/* remove package from transaction, returns modified transaction */
slapt_transaction_t *slapt_transaction_t_remove(slapt_transaction_t *trxn, const slapt_pkg_t *pkg);

/* search transaction by package name.  returns true if found, false otherwise */
bool slapt_transaction_t_search(const slapt_transaction_t *, const char *pkg_name);
/* search transaction by package attributes, returns true if found, false otherwise */
bool slapt_transaction_t_search_by_pkg(const slapt_transaction_t *trxn, const slapt_pkg_t *pkg);
/* searches the upgrade list of the transaction for the present of the package, returns true if found, false if not found */
bool slapt_transaction_t_search_upgrade(const slapt_transaction_t *trxn, const slapt_pkg_t *pkg);

/* add dependencies for package to transaction, returns -1 on error, 0 otherwise */
int slapt_transaction_t_add_dependencies(const slapt_config_t *global_config,
                            slapt_transaction_t *trxn,
                            const slapt_vector_t *avail_pkgs,
                            const slapt_vector_t *installed_pkgs,
                            const slapt_pkg_t *pkg);

/* check to see if a package has a conflict already present in the transaction, returns conflicted package or NULL if none */
slapt_vector_t *slapt_transaction_t_find_conflicts(const slapt_transaction_t *trxn,
                                    const slapt_vector_t *avail_pkgs,
                                    const slapt_vector_t *installed_pkgs,
                                    const slapt_pkg_t *pkg);

/* generate a list of suggestions based on the current packages in the transaction */
void slapt_transaction_t_suggestions(slapt_transaction_t *trxn);
