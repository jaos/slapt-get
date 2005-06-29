
/*
  this defines the max length of the transaction summary lines, hopefully
  someday this will be replaced with a more dynamic solution.
*/
#define MAX_LINE_LEN 80

struct suggests {
  char **pkgs;
  unsigned int count;
};

typedef struct {
  union { pkg_info_t *i; pkg_upgrade_t *u; } pkg;
  unsigned int type; /* this is enum action defined in main.h */
} queue_i;

typedef struct {
  queue_i **pkgs;
  unsigned int count;
} queue_t;

typedef struct {
  struct pkg_list *install_pkgs;
  struct pkg_upgrade_list *upgrade_pkgs;
  struct pkg_list *remove_pkgs;
  struct pkg_list *exclude_pkgs;
  struct suggests *suggests;
  struct pkg_err_list *conflict_err;
  struct pkg_err_list *missing_err;
  queue_t *queue;
} transaction_t;

/* fill in transaction structure with defaults */
void init_transaction(transaction_t *);
/*
  download and install/remove/upgrade packages as defined in the transaction
  returns 0 on success
*/
int handle_transaction(const rc_config *,transaction_t *);

/* add package for installation to transaction */
void add_install_to_transaction(transaction_t *,pkg_info_t *pkg);
/* add package for removal to transaction */
void add_remove_to_transaction(transaction_t *,pkg_info_t *pkg);
/* add package to upgrade to transaction */
void add_upgrade_to_transaction(transaction_t *,pkg_info_t *installed_pkg,
                                pkg_info_t *upgrade_pkg);
/* add package to exclude to transaction */
void add_exclude_to_transaction(transaction_t *,pkg_info_t *pkg);
/* remove package from transaction, returns modified transaction */
transaction_t *remove_from_transaction(transaction_t *tran,pkg_info_t *pkg);

/* search transaction by package name.  returns 1 if found, 0 otherwise */
int search_transaction(transaction_t *,char *pkg_name);
/*
  search transaction by package attributes
  returns 1 if found, 0 otherwise
*/
int search_transaction_by_pkg(transaction_t *tran,pkg_info_t *pkg);
/*
  searches the upgrade list of the transaction for the present of the package
  returns 1 if found, 0 if not found
*/
int search_upgrade_transaction(transaction_t *tran,pkg_info_t *pkg);

/*
  add dependencies for package to transaction, returns -1 on error, 0 otherwise
*/
int add_deps_to_trans(const rc_config *global_config, transaction_t *tran,
                      struct pkg_list *avail_pkgs,
                      struct pkg_list *installed_pkgs, pkg_info_t *pkg);

/*
  check to see if a package has a conflict already present in the transaction
  returns conflicted package or NULL if none
*/
pkg_info_t *is_conflicted(transaction_t *tran, struct pkg_list *avail_pkgs,
                          struct pkg_list *installed_pkgs, pkg_info_t *pkg);

/*
  generate a list of suggestions based on the current packages in the transaction
*/
void generate_suggestions(transaction_t *tran);

/* free the transaction structure and it's members */
void free_transaction(transaction_t *);
