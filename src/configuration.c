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
/* parse the exclude list */
static struct slapt_exclude_list *parse_exclude(char *line);


slapt_rc_config *slapt_init_config(void)
{
  slapt_rc_config *global_config = slapt_malloc( sizeof *global_config );

  global_config->download_only      = SLAPT_FALSE;
  global_config->simulate           = SLAPT_FALSE;
  global_config->ignore_excludes    = SLAPT_FALSE;
  global_config->no_md5_check       = SLAPT_FALSE;
  global_config->dist_upgrade       = SLAPT_FALSE;
  global_config->ignore_dep         = SLAPT_FALSE;
  global_config->disable_dep_check  = SLAPT_FALSE;
  global_config->print_uris         = SLAPT_FALSE;
  global_config->dl_stats           = SLAPT_FALSE;
  global_config->no_prompt          = SLAPT_FALSE;
  global_config->prompt             = SLAPT_FALSE;
  global_config->re_install         = SLAPT_FALSE;
  global_config->remove_obsolete    = SLAPT_FALSE;
  global_config->no_upgrade         = SLAPT_FALSE;
  global_config->use_priority       = SLAPT_FALSE;
  global_config->working_dir[0]     = '\0';
  global_config->progress_cb        = NULL;

  global_config->sources      = slapt_init_source_list();
  global_config->exclude_list = slapt_init_exclude_list();

  global_config->retry = 1;

  return global_config;
}

slapt_rc_config *slapt_read_rc_config(const char *file_name)
{
  FILE *rc = NULL;
  slapt_rc_config *global_config = slapt_init_config();
  char *getline_buffer = NULL;
  size_t gb_length = 0;
  ssize_t g_size;

  rc = slapt_open_file(file_name,"r");

  if ( rc == NULL )
    exit(EXIT_FAILURE);

  while ( (g_size = getline(&getline_buffer,&gb_length,rc) ) != EOF ) {
    char *token_ptr = NULL;
    getline_buffer[g_size - 1] = '\0';

    /* check to see if it has our key and value seperated by our token */
    /* and extract them */
    if ( (strchr(getline_buffer,'#') != NULL) && (strstr(getline_buffer, SLAPT_DISABLED_SOURCE_TOKEN) == NULL) )
      continue;


    if ( (token_ptr = strstr(getline_buffer,SLAPT_SOURCE_TOKEN)) != NULL ) {
      /* SOURCE URL */

      if ( strlen(token_ptr) > strlen(SLAPT_SOURCE_TOKEN) ) {
        slapt_source_t *s = slapt_init_source(token_ptr + strlen(SLAPT_SOURCE_TOKEN));
        if (s != NULL) {
          slapt_add_source(global_config->sources,s);
          if (s->priority != SLAPT_PRIORITY_DEFAULT) {
            global_config->use_priority = SLAPT_TRUE;
          }
        }
      }

    } else if ( (token_ptr = strstr(getline_buffer,SLAPT_DISABLED_SOURCE_TOKEN)) != NULL ) {
      /* DISABLED SOURCE */

      if (strlen(token_ptr) > strlen(SLAPT_DISABLED_SOURCE_TOKEN) ) {
        slapt_source_t *s = slapt_init_source(token_ptr + strlen(SLAPT_DISABLED_SOURCE_TOKEN));
        if (s != NULL) {
          s->disabled = SLAPT_TRUE;
          slapt_add_source(global_config->sources,s);
        }
      }

    } else if ( (token_ptr = strstr(getline_buffer,SLAPT_WORKINGDIR_TOKEN)) != NULL ) {
      /* WORKING DIR */

      if ( strlen(token_ptr) > strlen(SLAPT_WORKINGDIR_TOKEN) ) {
        strncpy(
          global_config->working_dir,
          token_ptr + strlen(SLAPT_WORKINGDIR_TOKEN),
          (strlen(token_ptr) - strlen(SLAPT_WORKINGDIR_TOKEN))
        );
        global_config->working_dir[
          (strlen(token_ptr) - strlen(SLAPT_WORKINGDIR_TOKEN))
        ] = '\0';
      }

    } else if ( (token_ptr = strstr(getline_buffer,SLAPT_EXCLUDE_TOKEN)) != NULL ) {
       /* exclude list */
      slapt_free_exclude_list(global_config->exclude_list);
      global_config->exclude_list = parse_exclude(token_ptr);
    }

  }

  fclose(rc);

  if ( getline_buffer )
    free(getline_buffer);

  if ( strcmp(global_config->working_dir,"") == 0 ) {
    fprintf(stderr,gettext("WORKINGDIR directive not set within %s.\n"),
            file_name);
    return NULL;
  }
  if ( global_config->sources->count == 0 ) {
    fprintf(stderr,gettext("SOURCE directive not set within %s.\n"),file_name);
    return NULL;
  }

  return global_config;
}

void slapt_working_dir_init(const slapt_rc_config *global_config)
{
  DIR *working_dir;
  int mode = W_OK;

  if ( (working_dir = opendir(global_config->working_dir)) == NULL ) {
    if ( mkdir(global_config->working_dir,
        S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1
    ) {
      printf(gettext("Failed to build working directory [%s]\n"),
        global_config->working_dir);

      if ( errno )
        perror(global_config->working_dir);

      exit(EXIT_FAILURE);
    }
  }
  closedir(working_dir);

	/* allow read access if we are simulating */
  if (global_config->simulate)
  	mode = R_OK;

  if ( access(global_config->working_dir,mode) == -1 ) {

    if ( errno )
      perror(global_config->working_dir);

    fprintf(stderr,
      gettext("Please update permissions on %s or run with appropriate privileges\n"),
      global_config->working_dir);
    exit(EXIT_FAILURE);
  }

  return;
}

void slapt_free_rc_config(slapt_rc_config *global_config)
{
  slapt_free_exclude_list(global_config->exclude_list);
  slapt_free_source_list(global_config->sources);
  free(global_config);
}

static struct slapt_exclude_list *parse_exclude(char *line)
{
  struct slapt_exclude_list *list;
  unsigned int position = 0, len = 0;

  list = slapt_malloc( sizeof *list );
  list->excludes = slapt_malloc( sizeof *list->excludes );
  list->count = 0;

  /* skip ahead past the = */
  line = strchr(line,'=') + 1;

  len = strlen(line);
  while ( position < len ) {

    if ( strstr(line + position,",") == NULL ) {
      char *pointer = NULL;

      pointer = line + position;

      slapt_add_exclude(list,pointer);

      break;
    } else {

      if ( line[position] == ',' ) {
        ++position;
        continue;
      } else {
        char *buffer = NULL,*pointer = NULL;

        pointer = strchr(line + position,',');
        buffer = strndup(
          line + position,
          strlen(line + position) - strlen(pointer)
        );

        slapt_add_exclude(list,buffer);
        free(buffer);
        position += (strlen(line + position) - strlen(pointer) );
      }
      continue;
    }
  }
  
  return list;
}

struct slapt_exclude_list *slapt_init_exclude_list(void)
{
  struct slapt_exclude_list *list = slapt_malloc(sizeof *list);
  list->excludes = slapt_malloc(sizeof *list->excludes);
  list->count = 0;

  return list;
}

void slapt_add_exclude(struct slapt_exclude_list *list,const char *e)
{
  char **realloc_tmp;

  realloc_tmp =
    realloc( list->excludes, sizeof *list->excludes * (list->count + 1) );

  if ( realloc_tmp == NULL )
    return;

  list->excludes = realloc_tmp;
  list->excludes[ list->count ] = strndup(e, strlen(e));
  list->excludes[ list->count ][strlen(e)] = '\0';
  ++list->count;

}

void slapt_remove_exclude(struct slapt_exclude_list *list,const char *e)
{
  unsigned int i = 0;
  char *tmp = NULL;

  while (i < list->count) {
    if ( strcmp(e,list->excludes[i]) == 0 && tmp == NULL ) {
      tmp = list->excludes[i];
    }
    if ( tmp != NULL && (i+1 < list->count) ) {
      list->excludes[i] = list->excludes[i + 1];
    }
    ++i;
  }
  if ( tmp != NULL ) {
    char **realloc_tmp;
    int count = list->count - 1;
    if ( count < 1 )
      count = 1;

    free(tmp);

    realloc_tmp = realloc(list->excludes, sizeof *list->excludes * count);
    if ( realloc_tmp != NULL ) {
      list->excludes = realloc_tmp;
      if (list->count > 0)
        --list->count;
    }

  }
}

void slapt_free_exclude_list(struct slapt_exclude_list *list)
{
  unsigned int i;
  
  for (i = 0; i < list->count; ++i) {
    free(list->excludes[i]);
  }
  free(list->excludes);
  free(list);
}

struct slapt_source_list *slapt_init_source_list(void)
{
  struct slapt_source_list *list = slapt_malloc(sizeof *list);
  list->src = slapt_malloc(sizeof *list->src);
  list->count = 0;

  return list;
}

void slapt_add_source(struct slapt_source_list *list,slapt_source_t *s)
{
  slapt_source_t **realloc_tmp;

  if ( s == NULL )
    return;

  realloc_tmp = realloc(list->src,sizeof *list->src * (list->count + 1) );

  if ( realloc_tmp == NULL )
    return;

  list->src = realloc_tmp;

  list->src[list->count] = s;
  ++list->count;

}

void slapt_remove_source (struct slapt_source_list *list, const char *s)
{
  slapt_source_t *src_to_discard = NULL;
  unsigned int i = 0;

  while ( i < list->count ) {
    if ( strcmp(s,list->src[i]->url) == 0 && src_to_discard == NULL ) {
      src_to_discard = list->src[i];
    }
    if ( src_to_discard != NULL && (i+1 < list->count) ) {
      list->src[i] = list->src[i + 1];
    }
    ++i;
  }

  if ( src_to_discard != NULL ) {
    slapt_source_t **realloc_tmp;
    int count = list->count - 1;

    if ( count < 1 )
      count = 1;

    slapt_free_source(src_to_discard);

    realloc_tmp = realloc(list->src,sizeof *list->src * count );
    if ( realloc_tmp != NULL ) {
      list->src = realloc_tmp;
      if (list->count > 0)
        --list->count;
    }

  }

}

void slapt_free_source_list(struct slapt_source_list *list)
{
  unsigned int i;

  for (i = 0; i < list->count; ++i) {
    slapt_free_source(list->src[i]);
  }
  free(list->src);
  free(list);
}

SLAPT_BOOL_T slapt_is_interactive(const slapt_rc_config *global_config)
{
  SLAPT_BOOL_T interactive  = global_config->progress_cb == NULL 
                            ? SLAPT_TRUE
                            : SLAPT_FALSE;

  return interactive;
}


static void slapt_source_parse_attributes(slapt_source_t *s, const char *string)
{
  int offset = 0;
  int len = strlen(string);

  while (offset < len) {
    char *token = NULL;

    if (strchr(string + offset, ',') != NULL) {
      size_t token_len = strcspn(string + offset, ",");
      if (token_len > 0) {
        token = strndup(string + offset, token_len);
        offset += token_len + 1;
      }
    } else {
      token = strdup(string + offset);
      offset += len;
    }

    if (token != NULL) {

      if (strcmp(token,SLAPT_PRIORITY_DEFAULT_TOKEN) == 0) {
        s->priority = SLAPT_PRIORITY_DEFAULT;
      } else if (strcmp(token,SLAPT_PRIORITY_PREFERRED_TOKEN) == 0) {
        s->priority = SLAPT_PRIORITY_PREFERRED;
      } else if (strcmp(token,SLAPT_PRIORITY_OFFICIAL_TOKEN) == 0) {
        s->priority = SLAPT_PRIORITY_OFFICIAL;
      } else if (strcmp(token,SLAPT_PRIORITY_CUSTOM_TOKEN) == 0) {
        s->priority = SLAPT_PRIORITY_CUSTOM;
      } else {
        fprintf(stderr,"Unknown token: %s\n", token);
      }
    }

  }

}

slapt_source_t *slapt_init_source(const char *s)
{
  slapt_source_t *src;
  unsigned int source_len = 0;
  unsigned int attribute_len = 0;
  slapt_regex_t *attribute_regex = NULL;
  char *source_string = NULL;
  char *attribute_string = NULL;

  if (s == NULL)
    return NULL;

  src           = slapt_malloc(sizeof *src);
  src->priority = SLAPT_PRIORITY_DEFAULT;
  src->disabled = SLAPT_FALSE;
  source_len    = strlen(s);

  /* parse for :[attr] in the source url */
  if ((attribute_regex = slapt_init_regex(SLAPT_SOURCE_ATTRIBUTE_REGEX)) == NULL) {
    exit(EXIT_FAILURE);
  }
  slapt_execute_regex(attribute_regex,s);
  if (attribute_regex->reg_return == 0) {
    /* if we find an attribute string, extract it */
    attribute_string = slapt_regex_extract_match(attribute_regex, s, 1);
    attribute_len = strlen(attribute_string);
    source_string = strndup(s, source_len - attribute_len);
  } else {
    /* otherwise we just dup the const string */
    source_string = strndup(s, source_len);
  }
  slapt_free_regex(attribute_regex);


  /* now add a trailing / if not already there */
  source_len = strlen(source_string);
  if ( source_string[source_len - 1] == '/' ) {

    src->url = strdup(source_string);

  } else {

    src->url = slapt_malloc(sizeof *src->url * (source_len + 2));
    src->url[0] = '\0';

    src->url = strncat(
      src->url,
      source_string,
      source_len
    );

    if (isblank(src->url[source_len - 1]) == 0) {
      src->url = strncat(src->url, "/", 1);
    } else {
      if (src->url[source_len - 2] == '/') {
        src->url[source_len - 2] = '/';
        src->url[source_len - 1] = '\0';
      } else {
        src->url[source_len - 1] = '/';
      }
    }

    src->url[source_len + 1] = '\0';

  }

  free(source_string);

  /* now parse the attribute string */
  if (attribute_string != NULL) {
    slapt_source_parse_attributes(src, attribute_string + 1);
    free(attribute_string);
  }

  return src;
}

void slapt_free_source(slapt_source_t *src)
{
  free(src->url);
  free(src);
}

int slapt_write_rc_config(const slapt_rc_config *global_config, const char *location)
{
  unsigned int i;
  FILE *rc;

  rc = slapt_open_file(location,"w+");
  if (rc == NULL)
    return -1;

  fprintf(rc,"%s%s\n",SLAPT_WORKINGDIR_TOKEN,global_config->working_dir);

  fprintf(rc,"%s",SLAPT_EXCLUDE_TOKEN);
  for (i = 0;i < global_config->exclude_list->count;++i) {
    if ( i+1 == global_config->exclude_list->count) {
      fprintf(rc,"%s",global_config->exclude_list->excludes[i]);
    }else{
      fprintf(rc,"%s,",global_config->exclude_list->excludes[i]);
    }
  }
  fprintf(rc,"\n");

  for (i = 0; i < global_config->sources->count;++i) {
    slapt_source_t *src = global_config->sources->src[i];
    SLAPT_PRIORITY_T priority = src->priority;
    const char *token = SLAPT_SOURCE_TOKEN;

    if (global_config->sources->src[i]->disabled == SLAPT_TRUE)
      token = SLAPT_DISABLED_SOURCE_TOKEN;

    if (priority > SLAPT_PRIORITY_DEFAULT) {
      const char *priority_token;

      if (priority == SLAPT_PRIORITY_PREFERRED)
          priority_token = SLAPT_PRIORITY_PREFERRED_TOKEN;
      else if (priority == SLAPT_PRIORITY_OFFICIAL)
          priority_token = SLAPT_PRIORITY_OFFICIAL_TOKEN;
      else if (priority == SLAPT_PRIORITY_CUSTOM)
          priority_token = SLAPT_PRIORITY_CUSTOM_TOKEN;
      else
          priority_token = SLAPT_PRIORITY_DEFAULT_TOKEN;

      fprintf(rc,"%s%s:%s\n",token, src->url, priority_token);
    } else {
      fprintf(rc,"%s%s\n",token, src->url);
    }
  }

  fclose(rc);

  return 0;
}

