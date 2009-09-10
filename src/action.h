
typedef struct {
  char **pkgs;
  unsigned int count;
} slapt_pkg_action_args_t;

slapt_pkg_action_args_t *slapt_init_pkg_action_args(int arg_count);
slapt_pkg_action_args_t *slapt_add_pkg_action_args(slapt_pkg_action_args_t *p,
                                                   const char *arg);
void slapt_free_pkg_action_args(slapt_pkg_action_args_t *paa);

void slapt_pkg_action_install(const slapt_rc_config *global_config,
                              const slapt_pkg_action_args_t *action_args);
void slapt_pkg_action_list(const int show);
void slapt_pkg_action_remove(const slapt_rc_config *global_config,
                             const slapt_pkg_action_args_t *action_args);
void slapt_pkg_action_search(const char *pattern);
void slapt_pkg_action_show(const char *pkg_name);
void slapt_pkg_action_upgrade_all(const slapt_rc_config *global_config);

#ifdef SLAPT_HAS_GPGME
void slapt_pkg_action_add_keys(const slapt_rc_config *global_config);
#endif

void slapt_pkg_action_filelist( const char *pkg_name );

