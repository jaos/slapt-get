
typedef struct {
  char **pkgs;
  unsigned int count;
} pkg_action_args_t;

pkg_action_args_t *init_pkg_action_args(int arg_count);
void free_pkg_action_args(pkg_action_args_t *paa);

void pkg_action_install(const rc_config *global_config,
                        const pkg_action_args_t *action_args);
void pkg_action_list(const int show);
void pkg_action_remove(const rc_config *global_config,
                       const pkg_action_args_t *action_args);
void pkg_action_search(const char *pattern);
void pkg_action_show(const char *pkg_name);
void pkg_action_upgrade_all(const rc_config *global_config);

