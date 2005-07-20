

#define SLAPT_MAX_REGEX_PARTS 10
#define SLAPT_SLACK_BASE_SET_REGEX "^./slackware/a$"

typedef enum {
#if !defined(FALSE) && !defined(TRUE)
SLAPT_FALSE = 0, SLAPT_TRUE
#else
SLAPT_FALSE = FALSE, SLAPT_TRUE = TRUE
#endif
} SLAPT_BOOL_T;

typedef struct {
  regex_t regex;
  size_t nmatch;
  regmatch_t pmatch[SLAPT_MAX_REGEX_PARTS];
  int reg_return;
} slapt_regex;

FILE *slapt_open_file(const char *file_name,const char *mode);
int slapt_init_regex(slapt_regex *regex_t, const char *regex_string);
void slapt_execute_regex(slapt_regex *regex_t,const char *string);
void slapt_free_regex(slapt_regex *regex_t);
void slapt_create_dir_structure(const char *dir_name);
/* generate an md5sum of filehandle */
void slapt_gen_md5_sum_of_file(FILE *f,char *result_sum);

/* Ask the user to answer yes or no.
 * return 1 on yes, 0 on no, else -1.
 */
int slapt_ask_yes_no(const char *format, ...);
char *slapt_str_replace_chr(const char *string,const char find,
                            const char replace);
__inline void *slapt_malloc(size_t s);
__inline void *slapt_calloc(size_t n,size_t s);

