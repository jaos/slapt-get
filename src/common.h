

#define MAX_REGEX_PARTS 10
#define SLACK_BASE_SET_REGEX "^./slackware/a$"

typedef enum {
#if !defined(FALSE) && !defined(TRUE)
FALSE = 0, TRUE
#else
JFALSE = FALSE, JTRUE = TRUE
#endif
} BOOL_T;

typedef struct {
  regex_t regex;
  size_t nmatch;
  regmatch_t pmatch[MAX_REGEX_PARTS];
  int reg_return;
} sg_regex;

FILE *open_file(const char *file_name,const char *mode);
int init_regex(sg_regex *regex_t, const char *regex_string);
void execute_regex(sg_regex *regex_t,const char *string);
void free_regex(sg_regex *regex_t);
void create_dir_structure(const char *dir_name);
/* generate an md5sum of filehandle */
void gen_md5_sum_of_file(FILE *f,char *result_sum);

/* Ask the user to answer yes or no.
 * return 1 on yes, 0 on no, else -1.
 */
int ask_yes_no(const char *format, ...);
char *str_replace_chr(const char *string,const char find, const char replace);
__inline void *slapt_malloc(size_t s);
__inline void *slapt_calloc(size_t n,size_t s);

