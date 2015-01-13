/*
 * Copyright (C) 2003-2015 Jason Woodward <woodwardj at jaos dot org>
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

struct slapt_pkg_version_parts {
  char **parts;
  unsigned int count;
};

/* used by qsort */
static int pkg_compare (const void *a, const void *b);
/* analyze the pkg version hunk by hunk */
static struct slapt_pkg_version_parts *break_down_pkg_version(const char *version);
/* parse the meta lines */
static slapt_pkg_info_t *parse_meta_entry(slapt_pkg_list_t *avail_pkgs,
                                          slapt_pkg_list_t *installed_pkgs,
                                          char *dep_entry);
/* called by slapt_is_required_by */
static void required_by(slapt_pkg_list_t *avail,
                        slapt_pkg_list_t *installed_pkgs,
                        slapt_pkg_list_t *pkgs_to_install,
                        slapt_pkg_list_t *pkgs_to_remove,
                        slapt_pkg_info_t *pkg,
                        slapt_pkg_list_t *required_by_list);
static char *escape_package_name(slapt_pkg_info_t *pkg);
/* free pkg_version_parts struct */
static void slapt_free_pkg_version_parts(struct slapt_pkg_version_parts *parts);
/* find dependency from "or" requirement */
static slapt_pkg_info_t *find_or_requirement(slapt_pkg_list_t *avail_pkgs,
                                             slapt_pkg_list_t *installed_pkgs,
                                             char *required_str);
/* uncompress compressed package data */
static FILE *slapt_gunzip_file (const char *file_name,FILE *dest_file);

#ifdef SLAPT_HAS_GPGME
/* check if signature is unauthenticated by "acceptable" reasons */
SLAPT_BOOL_T slapt_pkg_sign_is_unauthenticated(slapt_code_t code);
#endif

/* parse the PACKAGES.TXT file */
slapt_pkg_list_t *slapt_get_available_pkgs(void)
{
  FILE *pkg_list_fh;
  slapt_pkg_list_t *list = NULL;

  /* open pkg list */
  pkg_list_fh = slapt_open_file(SLAPT_PKG_LIST_L,"r");
  if (pkg_list_fh == NULL) {
    fprintf(stderr,gettext("Perhaps you want to run --update?\n"));
    list = slapt_init_pkg_list();
    return list; /* return an empty list */
  }
  list = slapt_parse_packages_txt(pkg_list_fh);
  fclose(pkg_list_fh);

  /* this is pointless to do if we wrote the data sorted, but this 
     ensures upgrades from older, presorting slapt-gets still work 
     as expected. */
  qsort( list->pkgs, list->pkg_count, sizeof(list->pkgs[0]), pkg_compare );

  list->ordered = SLAPT_TRUE;

  return list;
}

slapt_pkg_list_t *slapt_parse_packages_txt(FILE *pkg_list_fh)
{
  slapt_regex_t *name_regex = NULL,
                *mirror_regex = NULL,
                *priority_regex = NULL,
                *location_regex = NULL,
                *size_c_regex = NULL,
                *size_u_regex = NULL;
  ssize_t bytes_read;
  slapt_pkg_list_t *list = NULL;
  long f_pos = 0;
  size_t getline_len = 0;
  char *getline_buffer = NULL;
  char *char_pointer = NULL;

  list = slapt_init_pkg_list();

  /* compile our regexen */
  if ((name_regex = slapt_init_regex(SLAPT_PKG_NAME_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((mirror_regex = slapt_init_regex(SLAPT_PKG_MIRROR_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((priority_regex = slapt_init_regex(SLAPT_PKG_PRIORITY_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((location_regex = slapt_init_regex(SLAPT_PKG_LOCATION_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((size_c_regex = slapt_init_regex(SLAPT_PKG_SIZEC_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((size_u_regex = slapt_init_regex(SLAPT_PKG_SIZEU_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }

  while ((bytes_read = getline(&getline_buffer,&getline_len,
                               pkg_list_fh)) != EOF) {

    slapt_pkg_info_t *tmp_pkg;

    getline_buffer[bytes_read - 1] = '\0';

    /* pull out package data */
    if (strstr(getline_buffer,"PACKAGE NAME") == NULL)
      continue;

    slapt_execute_regex(name_regex,getline_buffer);

    /* skip this line if we didn't find a package name */
    if (name_regex->reg_return != 0) {
      fprintf(stderr,gettext("regex failed on [%s]\n"),getline_buffer);
      continue;
    }
    /* otherwise keep going and parse out the rest of the pkg data */

    tmp_pkg = slapt_init_pkg();

    /* pkg name base */
    tmp_pkg->name = slapt_regex_extract_match(name_regex, getline_buffer, 1);
    /* pkg version */
    tmp_pkg->version = slapt_regex_extract_match(name_regex, getline_buffer, 2);
    /* file extension */
    tmp_pkg->file_ext = slapt_regex_extract_match(name_regex, getline_buffer, 3);

    /* mirror */
    f_pos = ftell(pkg_list_fh);
    if (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) {

      slapt_execute_regex(mirror_regex,getline_buffer);

      if (mirror_regex->reg_return == 0) {
        tmp_pkg->mirror = slapt_regex_extract_match(mirror_regex, getline_buffer, 1);
      } else {
        /* mirror isn't provided... rewind one line */
        fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
      }
    }

    /* priority */
    f_pos = ftell(pkg_list_fh);
    if (getline(&getline_buffer, &getline_len, pkg_list_fh) != EOF) {

      slapt_execute_regex(priority_regex, getline_buffer);

      if (priority_regex->reg_return == 0) {
        char *priority_string = slapt_regex_extract_match(priority_regex, getline_buffer, 1);
        if (priority_string != NULL) {
          tmp_pkg->priority = atoi(priority_string);
          free(priority_string);
        }
      } else {
        /* priority isn't provided... rewind one line */
        fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
      }
    }

    /* location */
    if ((getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)) {

      slapt_execute_regex(location_regex,getline_buffer);

      if (location_regex->reg_return == 0) {

        tmp_pkg->location = slapt_regex_extract_match(location_regex, getline_buffer, 1);

#if SLACKWARE_EXTRA_TESTING_PASTURE_WORKAROUND == 1
        /*
          extra, testing, and pasture support
          they add in extraneous /extra/, /testing/, or /pasture/ in the
          PACKAGES.TXT location. this fixes the downloads and md5 checksum
          matching
        */
        if (strstr(tmp_pkg->location,"./testing/") != NULL) {
          char *tmp_location = slapt_malloc(
            sizeof *tmp_location *
            (strlen(tmp_pkg->location) - 7) /* ./testing - 2 */
          );
          tmp_location[0] = '.';
          tmp_location[1] = '\0';
          strncat(tmp_location,
            &tmp_pkg->location[0] + 9, /* ./testing */
            strlen(tmp_pkg->location) - 9 /* ./testing */
          );
          free(tmp_pkg->location);
          tmp_pkg->location = tmp_location;
        } else if (strstr(tmp_pkg->location,"./extra/") != NULL) {
          char *tmp_location = slapt_malloc(
            sizeof *tmp_location *
            (strlen(tmp_pkg->location) - 5 ) /* ./extra/ - 2 */
          );
          tmp_location[0] = '.';
          tmp_location[1] = '\0';
          strncat(tmp_location,
            &tmp_pkg->location[0] + 7, /* ./extra */
            strlen(tmp_pkg->location) - 7 /* ./extra */
          );
          free(tmp_pkg->location);
          tmp_pkg->location = tmp_location;
        } else if (strstr(tmp_pkg->location,"./pasture") != NULL) {
          char *tmp_location = slapt_malloc(
            sizeof *tmp_location *
            (strlen(tmp_pkg->location) - 7) /* ./pasture - 2 */
          );
          tmp_location[0] = '.';
          tmp_location[1] = '\0';
          strncat(tmp_location,
            &tmp_pkg->location[0] + 9, /* ./pasture */
            strlen(tmp_pkg->location) - 9 /* ./pasture */
          );
          free(tmp_pkg->location);
          tmp_pkg->location = tmp_location;
        }
#endif

      } else {
        fprintf(stderr,gettext("regexec failed to parse location\n"));
        slapt_free_pkg(tmp_pkg);
        continue;
      }
    } else {
      fprintf(stderr,gettext("getline reached EOF attempting to read location\n"));
      slapt_free_pkg(tmp_pkg);
      continue;
    }

    /* size_c */
    if ((getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)) {

      char *size_c = NULL;

      slapt_execute_regex(size_c_regex,getline_buffer);

      if (size_c_regex->reg_return == 0) {
        size_c = slapt_regex_extract_match(size_c_regex, getline_buffer, 1);
        tmp_pkg->size_c = strtol(size_c, (char **)NULL, 10);
        free(size_c);
      } else {
        fprintf(stderr,gettext("regexec failed to parse size_c\n"));
        slapt_free_pkg(tmp_pkg);
        continue;
      }
    } else {
      fprintf(stderr,gettext("getline reached EOF attempting to read size_c\n"));
      slapt_free_pkg(tmp_pkg);
      continue;
    }

    /* size_u */
    if ((getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF)) {

      char *size_u = NULL;

      slapt_execute_regex(size_u_regex,getline_buffer);

      if (size_u_regex->reg_return == 0) {
        size_u = slapt_regex_extract_match(size_u_regex, getline_buffer, 1);
        tmp_pkg->size_u = strtol(size_u, (char **)NULL, 10);
        free(size_u);
      } else {
        fprintf(stderr,gettext("regexec failed to parse size_u\n"));
        slapt_free_pkg(tmp_pkg);
        continue;
      }
    } else {
      fprintf(stderr,gettext("getline reached EOF attempting to read size_u\n"));
      slapt_free_pkg(tmp_pkg);
      continue;
    }

    /* required, if provided */
    f_pos = ftell(pkg_list_fh);
    if (
      ((bytes_read = getline(&getline_buffer,&getline_len,pkg_list_fh)) != EOF) &&
      ((char_pointer = strstr(getline_buffer,"PACKAGE REQUIRED")) != NULL)
    ) {
        char *tmp_realloc = NULL;
        size_t req_len = 18; /* "PACKAGE REQUIRED" + 2 */
        getline_buffer[bytes_read - 1] = '\0';

        tmp_realloc = realloc(
          tmp_pkg->required,
          sizeof *tmp_pkg->required * (strlen(char_pointer + req_len) + 1)
        );
        if (tmp_realloc != NULL) {
          tmp_pkg->required = tmp_realloc;
          strncpy(tmp_pkg->required,char_pointer + req_len,
                  strlen(char_pointer + req_len));
          tmp_pkg->required[ strlen(char_pointer + req_len) ] = '\0';
        }
    } else {
      /* required isn't provided... rewind one line */
      fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
    }

    /* conflicts, if provided */
    f_pos = ftell(pkg_list_fh);
    if (
      ((bytes_read = getline(&getline_buffer,&getline_len,
                             pkg_list_fh)) != EOF) &&
      ((char_pointer = strstr(getline_buffer,"PACKAGE CONFLICTS")) != NULL)
    ) {
        char *tmp_realloc = NULL;
        size_t req_len = 19; /* "PACKAGE CONFLICTS" + 2 */
        getline_buffer[bytes_read - 1] = '\0';

        tmp_realloc = realloc(tmp_pkg->conflicts,
          sizeof *tmp_pkg->conflicts * (strlen(char_pointer + req_len) + 1)
        );
        if (tmp_realloc != NULL) {
          tmp_pkg->conflicts = tmp_realloc;
          strncpy(tmp_pkg->conflicts,char_pointer + req_len,
                  strlen(char_pointer + req_len));
          tmp_pkg->conflicts[ strlen(char_pointer + req_len) ] = '\0';
        }
    } else {
      /* conflicts isn't provided... rewind one line */
      fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
    }

    /* suggests, if provided */
    f_pos = ftell(pkg_list_fh);
    if (
      ((bytes_read = getline(&getline_buffer,&getline_len,
                             pkg_list_fh)) != EOF) &&
      ((char_pointer = strstr(getline_buffer,"PACKAGE SUGGESTS")) != NULL)
    ) {
        char *tmp_realloc = NULL;
        size_t req_len = 18; /* "PACKAGE SUGGESTS" + 2 */
        getline_buffer[bytes_read - 1] = '\0';

        tmp_realloc = realloc(tmp_pkg->suggests,
          sizeof *tmp_pkg->suggests * (strlen(char_pointer + req_len) + 1)
        );
        if (tmp_realloc != NULL) {
          tmp_pkg->suggests = tmp_realloc;
          strncpy(tmp_pkg->suggests,char_pointer + req_len,
                  strlen(char_pointer + req_len));
          tmp_pkg->suggests[ strlen(char_pointer + req_len) ] = '\0';
        }
    } else {
      /* suggests isn't provided... rewind one line */
      fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
    }

    /* md5 checksum */
    f_pos = ftell(pkg_list_fh);
    if (
      ((bytes_read = getline(&getline_buffer,&getline_len,
                             pkg_list_fh)) != EOF) &&
      (strstr(getline_buffer,"PACKAGE MD5") != NULL)
    ) {
        char *md5sum;
        getline_buffer[bytes_read - 1] = '\0';
        md5sum = (char *)strpbrk(getline_buffer,":") + 1;
        while (*md5sum != 0 && isspace(*md5sum)) {
          md5sum++;
        }
        /* don't overflow the buffer */
        if (strlen(md5sum) > SLAPT_MD5_STR_LEN) {
          fprintf(stderr, gettext("md5 sum too long\n"));
          slapt_free_pkg(tmp_pkg);
          continue;
        }

        strncpy(tmp_pkg->md5,md5sum,SLAPT_MD5_STR_LEN);
        tmp_pkg->md5[SLAPT_MD5_STR_LEN - 1] = '\0';
    } else {
      /* md5 sum isn't provided... rewind one line */
      fseek(pkg_list_fh, (ftell(pkg_list_fh) - f_pos) * -1, SEEK_CUR);
    }

    /* description */
    if (
      (getline(&getline_buffer,&getline_len,pkg_list_fh) != EOF) &&
      (strstr(getline_buffer,"PACKAGE DESCRIPTION") != NULL)
    ) {

      while (1) {
        char *tmp_desc = NULL;

        if ((bytes_read = getline(&getline_buffer,&getline_len,
                                  pkg_list_fh)) == EOF) {
          break;
        }

        if (strcmp(getline_buffer,"\n") == 0) {
          break;
        }

        tmp_desc = realloc(tmp_pkg->description,
          sizeof *tmp_pkg->description *
          (strlen(tmp_pkg->description) + bytes_read + 1)
        );
        if (tmp_desc == NULL) {
          break;
        }
        tmp_pkg->description = tmp_desc;
        strncat(tmp_pkg->description,getline_buffer,bytes_read);

      }
    } else {
      fprintf(stderr,gettext("error attempting to read pkg description\n"));
      slapt_free_pkg(tmp_pkg);
      continue;
    }

    /* fillin details */
    if (tmp_pkg->location == NULL) {
      tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
      tmp_pkg->location[0] = '\0';
    }
    if (tmp_pkg->description == NULL) {
      tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->description);
      tmp_pkg->description[0] = '\0';
    }
    if (tmp_pkg->mirror == NULL) {
      tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->mirror);
      tmp_pkg->mirror[0] = '\0';
    }

    slapt_add_pkg_to_pkg_list(list,tmp_pkg);
    tmp_pkg = NULL;
  }

  if (getline_buffer)
    free(getline_buffer);

  slapt_free_regex(name_regex);
  slapt_free_regex(mirror_regex);
  slapt_free_regex(priority_regex);
  slapt_free_regex(location_regex);
  slapt_free_regex(size_c_regex);
  slapt_free_regex(size_u_regex);

  list->free_pkgs = SLAPT_TRUE;
  return list;
}

char *slapt_gen_short_pkg_description(slapt_pkg_info_t *pkg)
{
  char *short_description = NULL;
  size_t string_size = 0;

  if (strchr(pkg->description,'\n') != NULL) {
    string_size = strlen(pkg->description) -
      (strlen(pkg->name) + 2) - strlen(strchr(pkg->description,'\n'));
  }

  /* quit now if the description is going to be empty */
  if ((int)string_size < 1)
    return NULL;

  short_description = strndup(
    pkg->description + (strlen(pkg->name) + 2),
    string_size
  );

  return short_description;
}


slapt_pkg_list_t *slapt_get_installed_pkgs(void)
{
  DIR *pkg_log_dir;
  char *pkg_log_dirname = NULL;
  struct dirent *file;
  slapt_regex_t *ip_regex = NULL,
              *compressed_size_reg = NULL,
              *uncompressed_size_reg = NULL;
  slapt_pkg_list_t *list = NULL;
  size_t pls = 1;

  list = slapt_init_pkg_list();

  if ((ip_regex = slapt_init_regex(SLAPT_PKG_LOG_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((compressed_size_reg = slapt_init_regex(SLAPT_PKG_LOG_SIZEC_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }
  if ((uncompressed_size_reg = slapt_init_regex(SLAPT_PKG_LOG_SIZEU_PATTERN)) == NULL) {
    exit(EXIT_FAILURE);
  }

  pkg_log_dirname = slapt_gen_package_log_dir_name();

  if ((pkg_log_dir = opendir(pkg_log_dirname)) == NULL) {

    if (errno)
      perror(pkg_log_dirname);

    free(pkg_log_dirname);
    return list;
  }

  while ((file = readdir(pkg_log_dir)) != NULL) {
    slapt_pkg_info_t *tmp_pkg = NULL;
    FILE *pkg_f = NULL;
    char *pkg_f_name = NULL;
    struct stat stat_buf;
    char *pkg_data = NULL;

    slapt_execute_regex(ip_regex,file->d_name);

    /* skip if it doesn't match our regex */
    if (ip_regex->reg_return != 0)
      continue;

    tmp_pkg = slapt_init_pkg();

    tmp_pkg->name    = slapt_regex_extract_match(ip_regex, file->d_name, 1);
    tmp_pkg->version = slapt_regex_extract_match(ip_regex, file->d_name, 2);

    tmp_pkg->file_ext = slapt_malloc(sizeof *tmp_pkg->file_ext * 1);
    tmp_pkg->file_ext[0] = '\0';

    /* build the package filename including the package directory */
    pkg_f_name = slapt_malloc(
      sizeof *pkg_f_name * (strlen(pkg_log_dirname) + strlen(file->d_name) + 2)
    );
    pkg_f_name[0] = '\0';
    strncat(pkg_f_name,pkg_log_dirname,strlen(pkg_log_dirname));
    strncat(pkg_f_name,"/",1);
    strncat(pkg_f_name,file->d_name,strlen(file->d_name));

    /*
       open the package log file so that we can mmap it and parse out the
       package attributes.
    */
    pkg_f = slapt_open_file(pkg_f_name,"r");
    if (pkg_f == NULL)
      exit(EXIT_FAILURE);

    /* used with mmap */
    if (stat(pkg_f_name,&stat_buf) == -1) {

      if (errno)
        perror(pkg_f_name);

      fprintf(stderr,"stat failed: %s\n",pkg_f_name);
      exit(EXIT_FAILURE);
    }

    /* don't mmap empty files */
    if ((int)stat_buf.st_size < 1) {
      slapt_free_pkg(tmp_pkg);
      free(pkg_f_name);
      fclose(pkg_f);
      continue;
    } else {
      /* only mmap what we need */
      pls = (size_t)stat_buf.st_size;
      if (pls > SLAPT_MAX_MMAP_SIZE)
        pls = SLAPT_MAX_MMAP_SIZE;
    }

    pkg_data = (char *)mmap(0,pls,
                            PROT_READ|PROT_WRITE,MAP_PRIVATE,fileno(pkg_f),0);
    if (pkg_data == (void *)-1) {

      if (errno)
        perror(pkg_f_name);

      fprintf(stderr,"mmap failed: %s\n",pkg_f_name);
      exit(EXIT_FAILURE);
    }

    fclose(pkg_f);

    /* add \0 for strlen to work */
    pkg_data[pls - 1] = '\0';

    /* pull out compressed size */
    slapt_execute_regex(compressed_size_reg,pkg_data);
    if (compressed_size_reg->reg_return == 0) {
      char *size_c = slapt_regex_extract_match(compressed_size_reg, pkg_data, 1);
      char *unit   = slapt_regex_extract_match(compressed_size_reg, pkg_data, 2);
      double c     = strtof(size_c, (char **)NULL);
      if ( strcmp(unit, "M") == 0 )
        c *= 1024;
      tmp_pkg->size_c = round(c);
      free(size_c);
      free(unit);
    }

    /* pull out uncompressed size */
    slapt_execute_regex(uncompressed_size_reg,pkg_data);
    if (uncompressed_size_reg->reg_return == 0 ) {
      char *size_u = slapt_regex_extract_match(uncompressed_size_reg, pkg_data, 1);
      char *unit   = slapt_regex_extract_match(uncompressed_size_reg, pkg_data, 2);
      double u     = strtof(size_u, (char **)NULL);
      if ( strcmp(unit, "M") == 0 )
        u *= 1024;
      tmp_pkg->size_u = round(u);
      free(size_u);
      free(unit);
    }

    /* Ignore the location for installed packages */

    if (strstr(pkg_data,"PACKAGE DESCRIPTION") != NULL) {
      char *desc_p = strstr(pkg_data,"PACKAGE DESCRIPTION");
      char *nl = strchr(desc_p,'\n');
      char *filelist_p = NULL;

      if (nl != NULL)
        desc_p = ++nl;

      filelist_p = strstr(desc_p,"FILE LIST");
      if (filelist_p != NULL) {
        char *tmp_desc = NULL;
        size_t len = strlen(desc_p) - strlen(filelist_p) + 1;

        tmp_desc = realloc(tmp_pkg->description,
          sizeof *tmp_pkg->description *
          (strlen(tmp_pkg->description) + len + 1)
        );
        if (tmp_desc != NULL) {
          tmp_pkg->description = tmp_desc;
          strncpy(tmp_pkg->description,desc_p,len - 1);
          tmp_pkg->description[len - 1] = '\0';
        }

      } else {
        char *tmp_desc = NULL;
        size_t len = strlen(desc_p) + 1;

        tmp_desc = realloc(tmp_pkg->description,
          sizeof *tmp_pkg->description *
          (strlen(tmp_pkg->description) + len + 1)
        );
        if (tmp_desc != NULL) {
          tmp_pkg->description = tmp_desc;
          strncpy(tmp_pkg->description,desc_p,len - 1);
          tmp_pkg->description[len - 1] = '\0';
        }

      }

    }

    /* munmap now that we are done */
    if (munmap(pkg_data,pls) == -1) {
      if (errno)
        perror(pkg_f_name);

      fprintf(stderr,"munmap failed: %s\n",pkg_f_name);
      exit(EXIT_FAILURE);
    }
    free(pkg_f_name);

    /* fillin details */
    if (tmp_pkg->location == NULL) {
      tmp_pkg->location = slapt_malloc(sizeof *tmp_pkg->location);
      tmp_pkg->location[0] = '\0';
    }
    if (tmp_pkg->description == NULL) {
      tmp_pkg->description = slapt_malloc(sizeof *tmp_pkg->description);
      tmp_pkg->description[0] = '\0';
    }
    if (tmp_pkg->mirror == NULL) {
      tmp_pkg->mirror = slapt_malloc(sizeof *tmp_pkg->mirror);
      tmp_pkg->mirror[0] = '\0';
    }

    /* mark as installed */
    tmp_pkg->installed = SLAPT_TRUE;

    slapt_add_pkg_to_pkg_list(list,tmp_pkg);
    tmp_pkg = NULL;

  }/* end while */
  closedir(pkg_log_dir);
  slapt_free_regex(ip_regex);
  free(pkg_log_dirname);
  slapt_free_regex(compressed_size_reg);
  slapt_free_regex(uncompressed_size_reg);

  list->free_pkgs = SLAPT_TRUE;

  qsort( list->pkgs, list->pkg_count, sizeof(list->pkgs[0]), pkg_compare );

  list->ordered = SLAPT_TRUE;

  return list;
}

/* lookup newest package from pkg_list */
slapt_pkg_info_t *slapt_get_newest_pkg(slapt_pkg_list_t *pkg_list,
                                       const char *pkg_name)
{
  unsigned int i;
  slapt_pkg_info_t *pkg = NULL;

  for (i = 0; i < pkg_list->pkg_count; ++i ) {

    /* if pkg has same name as our requested pkg */
    if ((strcmp(pkg_list->pkgs[i]->name,pkg_name)) == 0) {
      if ((pkg == NULL) || (slapt_cmp_pkgs(pkg,pkg_list->pkgs[i]) < 0) ) {
        pkg = pkg_list->pkgs[i];
      }
    }

  }

  return pkg;
}

slapt_pkg_info_t *slapt_get_exact_pkg(slapt_pkg_list_t *list,
                                      const char *name,
                                      const char *version)
{

  if (list->ordered) {
    int min = 0, max = list->pkg_count - 1;

    while (max >= min)
    {
      int pivot    = (min + max) / 2;
      int name_cmp  = strcmp(list->pkgs[pivot]->name, name);

      if ( name_cmp == 0 ) {

        int version_cmp = strverscmp(list->pkgs[pivot]->version, version);

        if ( version_cmp == 0 ) {

          return list->pkgs[pivot];

        } else {

          if ( version_cmp < 0 )
            min = pivot + 1;
          else
            max = pivot - 1;
           
        }

      } else {

        if ( name_cmp < 0 )
          min = pivot + 1;
        else
          max = pivot - 1;

      }

    }

  } else {
    unsigned int i;

    for (i = 0; i < list->pkg_count;i++) {
      if ((strcmp(name,list->pkgs[i]->name)==0) &&
        (strcmp(version,list->pkgs[i]->version)==0) ) {
          return list->pkgs[i];
      }
    }

  }

  return NULL;
}

int slapt_install_pkg(const slapt_rc_config *global_config,
                      slapt_pkg_info_t *pkg)
{
  char *pkg_file_name = NULL;
  char *command = NULL;
  int cmd_return = 0;

  /* build the file name */
  pkg_file_name = slapt_gen_pkg_file_name(global_config,pkg);

  /* build and execute our command */
  command = slapt_calloc(
    strlen(SLAPT_INSTALL_CMD) + strlen(pkg_file_name) + 1 , sizeof *command
   );
  command[0] = '\0';
  command = strncat(command,SLAPT_INSTALL_CMD,strlen(SLAPT_INSTALL_CMD));
  command = strncat(command,pkg_file_name,strlen(pkg_file_name));

  if ((cmd_return = system(command)) != 0) {
    printf(gettext("Failed to execute command: [%s]\n"),command);
    free(command);
    free(pkg_file_name);
    return -1;
  }

  free(pkg_file_name);
  free(command);
  return cmd_return;
}

int slapt_upgrade_pkg(const slapt_rc_config *global_config,
                      slapt_pkg_info_t *pkg)
{
  char *pkg_file_name = NULL;
  char *command = NULL;
  int cmd_return = 0;

  /* build the file name */
  pkg_file_name = slapt_gen_pkg_file_name(global_config,pkg);

  /* build and execute our command */
  command = slapt_calloc(
    strlen(SLAPT_UPGRADE_CMD) + strlen(pkg_file_name) + 1 , sizeof *command
  );
  command[0] = '\0';
  command = strncat(command,SLAPT_UPGRADE_CMD,strlen(SLAPT_UPGRADE_CMD));
  command = strncat(command,pkg_file_name,strlen(pkg_file_name));

  if ((cmd_return = system(command)) != 0) {
    printf(gettext("Failed to execute command: [%s]\n"),command);
    free(command);
    free(pkg_file_name);
    return -1;
  }

  free(pkg_file_name);
  free(command);
  return cmd_return;
}

int slapt_remove_pkg(const slapt_rc_config *global_config,slapt_pkg_info_t *pkg)
{
  char *command = NULL;
  int cmd_return = 0;

  (void)global_config;

  /* build and execute our command */
  command = slapt_calloc(
    strlen(SLAPT_REMOVE_CMD) + strlen(pkg->name) + strlen(pkg->version) + 2,
    sizeof *command
  );
  command[0] = '\0';
  command = strncat(command,SLAPT_REMOVE_CMD,strlen(SLAPT_REMOVE_CMD));
  command = strncat(command,pkg->name,strlen(pkg->name));
  command = strncat(command,"-",1);
  command = strncat(command,pkg->version,strlen(pkg->version));
  if ((cmd_return = system(command)) != 0) {
    printf(gettext("Failed to execute command: [%s]\n"),command);
    free(command);
    return -1;
  }

  free(command);
  return cmd_return;
}

void slapt_free_pkg(slapt_pkg_info_t *pkg)
{
  if (pkg->required != NULL)
    free(pkg->required);

  if (pkg->conflicts != NULL)
    free(pkg->conflicts);

  if (pkg->suggests != NULL)
    free(pkg->suggests);

  if (pkg->name != NULL)
    free(pkg->name);

  if (pkg->file_ext != NULL)
    free(pkg->file_ext);

  if (pkg->version != NULL)
    free(pkg->version);

  if (pkg->mirror != NULL)
    free(pkg->mirror);

  if (pkg->location != NULL)
    free(pkg->location);

  if (pkg->description != NULL)
    free(pkg->description);

  free(pkg);
}

void slapt_free_pkg_list(slapt_pkg_list_t *list)
{
  unsigned int i;
  if (list->free_pkgs == SLAPT_TRUE) {
    for (i = 0;i < list->pkg_count;i++) {
      slapt_free_pkg(list->pkgs[i]);
    }
  }
  free(list->pkgs);
  free(list);
}

int slapt_is_excluded(const slapt_rc_config *global_config,
                      slapt_pkg_info_t *pkg)
{
  unsigned int i,pkg_not_excluded = 0, pkg_slapt_is_excluded = 1;
  int name_reg_ret = -1,version_reg_ret = -1,location_reg_ret = -1;

  if (global_config->ignore_excludes == SLAPT_TRUE)
    return pkg_not_excluded;

  /* maybe EXCLUDE= isn't defined in our rc? */
  if (global_config->exclude_list->count == 0)
    return pkg_not_excluded;

  for (i = 0; i < global_config->exclude_list->count;i++) {
    slapt_regex_t *exclude_reg = NULL;

    /* return if its an exact match */
    if ((strncmp(global_config->exclude_list->items[i],
                 pkg->name,strlen(pkg->name)) == 0))
      return pkg_slapt_is_excluded;

    /*
      this regex has to be init'd and free'd within the loop b/c the regex is pulled 
      from the exclude list
    */
    if ((exclude_reg =
          slapt_init_regex(global_config->exclude_list->items[i])) == NULL) {
      fprintf(stderr,"\n\nugh %s\n\n",global_config->exclude_list->items[i]);
      continue;
    }

    slapt_execute_regex(exclude_reg,pkg->name);
    name_reg_ret = exclude_reg->reg_return;

    slapt_execute_regex(exclude_reg,pkg->version);
    version_reg_ret = exclude_reg->reg_return;

    slapt_execute_regex(exclude_reg,pkg->location);
    location_reg_ret = exclude_reg->reg_return;

    slapt_free_regex(exclude_reg);

    if (name_reg_ret == 0 || version_reg_ret == 0 || location_reg_ret == 0) {
      return pkg_slapt_is_excluded;
    }

  }

  return pkg_not_excluded;
}

void slapt_get_md5sums(slapt_pkg_list_t *pkgs, FILE *checksum_file)
{
  slapt_regex_t *md5sum_regex = NULL;
  ssize_t getline_read;
  size_t getline_len = 0;
  char *getline_buffer = NULL;
  unsigned int a;

  if ((md5sum_regex = slapt_init_regex(SLAPT_MD5SUM_REGEX)) == NULL) {
    exit(EXIT_FAILURE);
  }

  while ((getline_read = getline(&getline_buffer,&getline_len,
                                 checksum_file)) != EOF) {

    if (
      (strstr(getline_buffer,".tgz") == NULL) &&
      (strstr(getline_buffer,".tlz") == NULL) &&
      (strstr(getline_buffer,".txz") == NULL) &&
      (strstr(getline_buffer,".ikg") == NULL) &&
      (strstr(getline_buffer,".tbz") == NULL)
    )
      continue;
    if (strstr(getline_buffer,".asc") != NULL)
      continue;

    slapt_execute_regex(md5sum_regex,getline_buffer);

    if (md5sum_regex->reg_return == 0) {
      char *sum, *location, *name, *version;

      /* md5 sum */
      sum = slapt_regex_extract_match(md5sum_regex, getline_buffer, 1);
      /* location/directory */
      location = slapt_regex_extract_match(md5sum_regex, getline_buffer, 2);
      /* pkg name */
      name = slapt_regex_extract_match(md5sum_regex, getline_buffer, 3);
      /* pkg version */
      version = slapt_regex_extract_match(md5sum_regex, getline_buffer, 4);

      /* see if we can match up name, version, and location */
      for (a = 0;a < pkgs->pkg_count;a++) {
        if (
          (strcmp(pkgs->pkgs[a]->name,name) == 0) &&
          (slapt_cmp_pkg_versions(pkgs->pkgs[a]->version,version) == 0) &&
          (strcmp(pkgs->pkgs[a]->location,location) == 0)
        ) {
          memcpy(pkgs->pkgs[a]->md5, sum,SLAPT_MD5_STR_LEN);
          break;
        }
      }

      free(sum);
      free(name);
      free(version);
      free(location);
    }
  }

  if (getline_buffer)
    free(getline_buffer);

  slapt_free_regex(md5sum_regex);
  rewind(checksum_file);

  return;
}

static void slapt_free_pkg_version_parts(struct slapt_pkg_version_parts *parts)
{
  unsigned int i;
  for (i = 0;i < parts->count;i++) {
    free(parts->parts[i]);
  }
  free(parts->parts);
  free(parts);
}

int slapt_cmp_pkgs(slapt_pkg_info_t *a, slapt_pkg_info_t *b)
{
  int greater = 1,lesser = -1,equal = 0;

  /* if either of the two packages is installed, we look
     for the same version to bail out early if possible */
  if (a->installed == SLAPT_TRUE || b->installed == SLAPT_TRUE)
    if (strcasecmp(a->version,b->version) == 0)
      return equal;

  /* check the priorities */
  if(a->priority > b->priority)
    return greater;
  else if(a->priority < b->priority)
    return lesser;

  return slapt_cmp_pkg_versions(a->version, b->version);
}

int slapt_cmp_pkg_versions(const char *a, const char *b)
{
  unsigned int position = 0;
  int greater = 1,lesser = -1,equal = 0;
  struct slapt_pkg_version_parts *a_parts;
  struct slapt_pkg_version_parts *b_parts;

  /* bail out early if possible */
  if (strcasecmp(a,b) == 0)
    return equal;

  a_parts = break_down_pkg_version(a);
  b_parts = break_down_pkg_version(b);

  while (position < a_parts->count && position < b_parts->count) {
    if (strcasecmp(a_parts->parts[position],b_parts->parts[position]) != 0) {

      /*
       * if the integer value of the version part is the same
       * and the # of version parts is the same (fixes 3.8.1p1-i486-1
       * to 3.8p1-i486-1)
      */ 
      if ((atoi(a_parts->parts[position]) == atoi(b_parts->parts[position])) &&
        (a_parts->count == b_parts->count)) {

        if (strverscmp(a_parts->parts[position],b_parts->parts[position]) < 0) {
          slapt_free_pkg_version_parts(a_parts);
          slapt_free_pkg_version_parts(b_parts);
          return lesser;
        }
        if (strverscmp(a_parts->parts[position],b_parts->parts[position]) > 0) {
          slapt_free_pkg_version_parts(a_parts);
          slapt_free_pkg_version_parts(b_parts);
          return greater;
        }

      }

      if (atoi(a_parts->parts[position]) < atoi(b_parts->parts[position])) {
        slapt_free_pkg_version_parts(a_parts);
        slapt_free_pkg_version_parts(b_parts);
        return lesser;
      }

      if (atoi(a_parts->parts[position]) > atoi(b_parts->parts[position])) {
        slapt_free_pkg_version_parts(a_parts);
        slapt_free_pkg_version_parts(b_parts);
        return greater;
      }

    }
    ++position;
  }

  /*
    * if we got this far, we know that some or all of the version
   * parts are equal in both packages.  If pkg-a has 3 version parts
   * and pkg-b has 2, then we assume pkg-a to be greater.
  */
  if (a_parts->count != b_parts->count) {
    if (a_parts->count > b_parts->count) {
      slapt_free_pkg_version_parts(a_parts);
      slapt_free_pkg_version_parts(b_parts);
      return greater;
    } else {
      slapt_free_pkg_version_parts(a_parts);
      slapt_free_pkg_version_parts(b_parts);
      return lesser;
    }
  }

  slapt_free_pkg_version_parts(a_parts);
  slapt_free_pkg_version_parts(b_parts);

  /*
   * Now we check to see that the version follows the standard slackware
   * convention.  If it does, we will compare the build portions.
   */
  /* make sure the packages have at least two separators */
  if ((index(a,'-') != rindex(a,'-')) && (index(b,'-') != rindex(b,'-'))) {
    char *a_build,*b_build;

    /* pointer to build portions */
    a_build = rindex(a,'-');
    b_build = rindex(b,'-');

    if (a_build != NULL && b_build != NULL) {
      /* they are equal if the integer values are equal */
      /* for instance, "1rob" and "1" will be equal */
      if (atoi(a_build) == atoi(b_build)) {
/* prefer our architecture in x86/x86_64 multilib settings */
#if defined(__x86_64__)
        if ((strcmp(a_build,b_build) == 0) && (strcmp(a,b) != 0) && (strstr(a,"-x86_64") != NULL && (strstr(b,"-x86_64-") == NULL)))
          return greater;
        if ((strcmp(a_build,b_build) == 0) && (strcmp(a,b) != 0) && (strstr(a,"-x86_64") == NULL && (strstr(b,"-x86_64-") != NULL)))
          return lesser;
#elif defined(__i386__)
        if ((strcmp(a_build,b_build) == 0) && (strcmp(a,b) != 0) && (strstr(a,"-x86_64") == NULL && (strstr(b,"-x86_64-") != NULL)))
          return greater;
        if ((strcmp(a_build,b_build) == 0) && (strcmp(a,b) != 0) && (strstr(a,"-x86_64") != NULL && (strstr(b,"-x86_64-") == NULL)))
          return lesser;
#endif
        return equal;
      }

      if (atoi(a_build) < atoi(b_build))
        return greater;

      if (atoi(a_build) > atoi(b_build))
        return lesser;

    }

  }

  /*
   * If both have the same # of version parts, non-standard version convention,
   * then we fall back on strverscmp.
  */
  if (strchr(a,'-') == NULL && strchr(b,'-') == NULL)
    return strverscmp(a,b);

  return equal;
}

static struct slapt_pkg_version_parts *break_down_pkg_version(const char *version)
{
  int pos = 0,sv_size = 0;
  char *pointer,*short_version;
  struct slapt_pkg_version_parts *pvp;

  pvp = slapt_malloc(sizeof *pvp);
  pvp->parts = slapt_malloc(sizeof *pvp->parts);
  pvp->count = 0;

  /* generate a short version, leave out arch and release */
  if ((pointer = strchr(version,'-')) == NULL) {
    sv_size = strlen(version) + 1;
    short_version = slapt_malloc(sizeof *short_version * sv_size);
    memcpy(short_version,version,sv_size);
    short_version[sv_size - 1] = '\0';
  } else {
    sv_size = (strlen(version) - strlen(pointer) + 1);
    short_version = slapt_malloc(sizeof *short_version * sv_size);
    memcpy(short_version,version,sv_size);
    short_version[sv_size - 1] = '\0';
    pointer = NULL;
  }

  while (pos < (sv_size - 1) ) {
    char **tmp;

    tmp = realloc(pvp->parts, sizeof *pvp->parts * (pvp->count + 1) );
    if (tmp == NULL) {
      fprintf(stderr,gettext("Failed to realloc %s\n"),"pvp->parts");
      exit(EXIT_FAILURE);
    }
    pvp->parts = tmp;

    /* check for . as a seperator */
    if ((pointer = strchr(short_version + pos,'.')) != NULL) {
      int b_count = (strlen(short_version + pos) - strlen(pointer) + 1);
      pvp->parts[pvp->count] = slapt_malloc(
        sizeof *pvp->parts[pvp->count] * b_count);

      memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
      pvp->parts[pvp->count][b_count - 1] = '\0';
      ++pvp->count;
      pointer = NULL;
      pos += b_count;
    /* check for _ as a seperator */
    } else if ((pointer = strchr(short_version + pos,'_')) != NULL) {
      int b_count = (strlen(short_version + pos) - strlen(pointer) + 1);
      pvp->parts[pvp->count] = slapt_malloc(
        sizeof *pvp->parts[pvp->count] * b_count);

      memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
      pvp->parts[pvp->count][b_count - 1] = '\0';
      ++pvp->count;
      pointer = NULL;
      pos += b_count;
    /* must be the end of the string */
    } else {
      int b_count = (strlen(short_version + pos) + 1);
      pvp->parts[pvp->count] = slapt_malloc(
        sizeof *pvp->parts[pvp->count] * b_count);

      memcpy(pvp->parts[pvp->count],short_version + pos,b_count - 1);
      pvp->parts[pvp->count][b_count - 1] = '\0';
      ++pvp->count;
      pos += b_count;
    }
  }

  free(short_version);
  return pvp;
}

void slapt_write_pkg_data(const char *source_url,FILE *d_file,
                          slapt_pkg_list_t *pkgs)
{
  unsigned int i;

  for (i=0;i < pkgs->pkg_count;i++) {

    fprintf(d_file,"PACKAGE NAME:  %s-%s%s\n",
      pkgs->pkgs[i]->name,pkgs->pkgs[i]->version,pkgs->pkgs[i]->file_ext);
    if (pkgs->pkgs[i]->mirror != NULL && strlen(pkgs->pkgs[i]->mirror) > 0) {
      fprintf(d_file,"PACKAGE MIRROR:  %s\n",pkgs->pkgs[i]->mirror);
    } else {
      fprintf(d_file,"PACKAGE MIRROR:  %s\n",source_url);
    }
    fprintf(d_file,"PACKAGE PRIORITY:  %d\n", pkgs->pkgs[i]->priority);
    fprintf(d_file,"PACKAGE LOCATION:  %s\n",pkgs->pkgs[i]->location);
    fprintf(d_file,"PACKAGE SIZE (compressed):  %d K\n",pkgs->pkgs[i]->size_c);
    fprintf(d_file,"PACKAGE SIZE (uncompressed):  %d K\n",pkgs->pkgs[i]->size_u);
    fprintf(d_file,"PACKAGE REQUIRED:  %s\n",pkgs->pkgs[i]->required);
    fprintf(d_file,"PACKAGE CONFLICTS:  %s\n",pkgs->pkgs[i]->conflicts);
    fprintf(d_file,"PACKAGE SUGGESTS:  %s\n",pkgs->pkgs[i]->suggests);
    fprintf(d_file,"PACKAGE MD5SUM:  %s\n",pkgs->pkgs[i]->md5);
    fprintf(d_file,"PACKAGE DESCRIPTION:\n");
    /* do we have to make up an empty description? */
    if (strlen(pkgs->pkgs[i]->description) < strlen(pkgs->pkgs[i]->name)) {
      fprintf(d_file,"%s: no description\n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n",pkgs->pkgs[i]->name);
      fprintf(d_file,"%s: \n\n",pkgs->pkgs[i]->name);
    } else {
      fprintf(d_file,"%s\n",pkgs->pkgs[i]->description);
    }

  }
}

slapt_pkg_list_t *slapt_search_pkg_list(slapt_pkg_list_t *list,
                                             const char *pattern)
{
  unsigned int i;
  int name_r = -1,desc_r = -1,loc_r = -1,version_r = -1;
  slapt_regex_t *search_regex = NULL;
  slapt_pkg_list_t *matches = NULL;

  matches = slapt_init_pkg_list();

  if ((search_regex = slapt_init_regex(pattern)) == NULL)
    return matches;

  for (i = 0; i < list->pkg_count; i++ ) {
    if (strcmp(list->pkgs[i]->name, pattern) == 0) {
      slapt_add_pkg_to_pkg_list(matches,list->pkgs[i]);
      continue;
    }

    slapt_execute_regex(search_regex,list->pkgs[i]->name);
    name_r = search_regex->reg_return;

    slapt_execute_regex(search_regex,list->pkgs[i]->version);
    version_r = search_regex->reg_return;

    if (list->pkgs[i]->description != NULL) {
      slapt_execute_regex(search_regex,list->pkgs[i]->description);
      desc_r = search_regex->reg_return;
    }

    if (list->pkgs[i]->location != NULL) {
      slapt_execute_regex(search_regex,list->pkgs[i]->location);
      loc_r = search_regex->reg_return;
    }

    /* search pkg name, pkg description, pkg location */
    if (name_r == 0 || version_r == 0 || desc_r == 0 || loc_r == 0) {
      slapt_add_pkg_to_pkg_list(matches,list->pkgs[i]);
    }
  }
  slapt_free_regex(search_regex);

  return matches;
}

/* lookup dependencies for pkg */
int slapt_get_pkg_dependencies(const slapt_rc_config *global_config,
                         slapt_pkg_list_t *avail_pkgs,
                         slapt_pkg_list_t *installed_pkgs,slapt_pkg_info_t *pkg,
                         slapt_pkg_list_t *deps,
                         slapt_pkg_err_list_t *conflict_err,
                         slapt_pkg_err_list_t *missing_err)
{
  unsigned int i = 0;
  slapt_list_t *dep_parts = NULL;

  /*
   * don't go any further if the required member is empty
   * or disable_dep_check is set
  */
  if (global_config->disable_dep_check == SLAPT_TRUE ||
    strcmp(pkg->required,"") == 0 ||
    strcmp(pkg->required," ") == 0 ||
    strcmp(pkg->required,"  ") == 0
  )
    return 0;

  if (deps == NULL)
    deps = slapt_init_pkg_list();

  if (conflict_err == NULL)
    conflict_err = slapt_init_pkg_err_list();

  if (missing_err == NULL)
    missing_err = slapt_init_pkg_err_list();

  /* parse dep line */
  dep_parts = slapt_parse_delimited_list(pkg->required, ',');
  for (i = 0; i < dep_parts->count; i++) {
    slapt_pkg_info_t *tmp_pkg = NULL;

    if (strchr(dep_parts->items[i],'|') != NULL) {
      tmp_pkg = find_or_requirement(avail_pkgs,installed_pkgs,dep_parts->items[i]);
    } else {
      tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,dep_parts->items[i]);
    }

    if (tmp_pkg == NULL) {
      /* if we can't find a required dep, return -1 */
      slapt_add_pkg_err_to_list(missing_err,pkg->name,dep_parts->items[i]);
      slapt_free_list(dep_parts);
      return -1;
    }

    /* if this pkg is excluded */
    if ((slapt_is_excluded(global_config,tmp_pkg) == 1) && (global_config->ignore_dep == SLAPT_FALSE)) {
      if (slapt_get_exact_pkg(installed_pkgs,tmp_pkg->name, tmp_pkg->version) == NULL) {
        slapt_add_pkg_err_to_list(conflict_err,pkg->name,tmp_pkg->name);
        slapt_free_list(dep_parts);
        return -1;
      }
    }

    /* if tmp_pkg is not already in the deps pkg_list */
    if ((slapt_get_newest_pkg(deps,tmp_pkg->name) == NULL)) {
      int dep_check_return;

      /* add tmp_pkg to deps so that we don't needlessly recurse */
      slapt_add_pkg_to_pkg_list(deps,tmp_pkg);

      /* now check to see if tmp_pkg has dependencies */
      dep_check_return = slapt_get_pkg_dependencies(
        global_config,avail_pkgs,installed_pkgs,tmp_pkg,
        deps,conflict_err,missing_err
      );

      if (dep_check_return == -1 && global_config->ignore_dep == SLAPT_FALSE) {
        slapt_free_list(dep_parts);
        return -1;
      } else {
        /* now move the package to the end after it's dependencies */
        slapt_pkg_info_t *tmp = NULL;
        unsigned int i = 0;

        while (i < deps->pkg_count) {

          if (strcmp(deps->pkgs[i]->name,tmp_pkg->name) == 0 && tmp == NULL)
            tmp = deps->pkgs[i];

          /* move all subsequent packages up */
          if (tmp != NULL && (i+1 < deps->pkg_count))
            deps->pkgs[i] = deps->pkgs[i + 1];

          ++i;
        }

        /*
          * now put the pkg we found at the end...
          * note no resizing is necessary, we just moved the location
        */
        if (tmp != NULL)
          deps->pkgs[deps->pkg_count - 1] = tmp;
      }

    } /* end already exists in dep check */

  }

  slapt_free_list(dep_parts);
  return 0;
}

/* lookup conflicts for package */
slapt_pkg_list_t *slapt_get_pkg_conflicts(slapt_pkg_list_t *avail_pkgs,
                                               slapt_pkg_list_t *installed_pkgs,
                                               slapt_pkg_info_t *pkg)
{
  slapt_pkg_list_t *conflicts = NULL;
  int position = 0,len = 0;
  char *pointer = NULL;
  char *buffer = NULL;

  conflicts = slapt_init_pkg_list();

  /*
   * don't go any further if the required member is empty
  */
  if (strcmp(pkg->conflicts,"") == 0 ||
    strcmp(pkg->conflicts," ") == 0 ||
    strcmp(pkg->conflicts,"  ") == 0
  )
    return conflicts;

  /* parse conflict line */
  len = strlen(pkg->conflicts);
  while (position < len) {
    slapt_pkg_info_t *tmp_pkg = NULL;

    /* either the last or there was only one to begin with */
    if (strstr(pkg->conflicts + position,",") == NULL) {
      pointer = pkg->conflicts + position;

      /* parse the conflict entry and try to lookup a package */
      tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,pointer);

      position += strlen(pointer);
    } else {

      /* if we have a comma, skip it */
      if (pkg->conflicts[position] == ',' ) {
        ++position;
        continue;
      }

      /* build the buffer to contain the conflict entry */
      pointer = strchr(pkg->conflicts + position,',');
      buffer = strndup(
        pkg->conflicts + position,
        strlen(pkg->conflicts + position) - strlen(pointer)
      );

      /* parse the conflict entry and try to lookup a package */
      tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,buffer);

      position += strlen(pkg->conflicts + position) - strlen(pointer);
      free(buffer);
    }

    if (tmp_pkg != NULL) {
      slapt_add_pkg_to_pkg_list(conflicts,tmp_pkg);
    }

  }/* end while */

  return conflicts;
}

static slapt_pkg_info_t *parse_meta_entry(slapt_pkg_list_t *avail_pkgs,
                                          slapt_pkg_list_t *installed_pkgs,
                                          char *dep_entry)
{
  unsigned int i;
  slapt_regex_t *parse_dep_regex = NULL;
  char *tmp_pkg_name = NULL,*tmp_pkg_ver = NULL;
  char tmp_pkg_cond[3];
  slapt_pkg_info_t *newest_avail_pkg;
  slapt_pkg_info_t *newest_installed_pkg;
  int tmp_cond_len = 0;

  if ((parse_dep_regex = slapt_init_regex(SLAPT_REQUIRED_REGEX)) == NULL) {
    exit(EXIT_FAILURE);
  }

  /* regex to pull out pieces */
  slapt_execute_regex(parse_dep_regex,dep_entry);

  /* if the regex failed, just skip out */
  if (parse_dep_regex->reg_return != 0) {
    slapt_free_regex(parse_dep_regex);
    return NULL;
  }

  tmp_cond_len = parse_dep_regex->pmatch[2].rm_eo - parse_dep_regex->pmatch[2].rm_so;

  tmp_pkg_name = slapt_regex_extract_match(parse_dep_regex, dep_entry, 1);

  newest_avail_pkg = slapt_get_newest_pkg(avail_pkgs,tmp_pkg_name);
  newest_installed_pkg = slapt_get_newest_pkg(installed_pkgs,tmp_pkg_name);

  /* if there is no conditional and version, return newest */
  if (tmp_cond_len == 0 ) {
    if (newest_installed_pkg != NULL) {
      slapt_free_regex(parse_dep_regex);
      free(tmp_pkg_name);
      return newest_installed_pkg;
    }
    if (newest_avail_pkg != NULL) {
      slapt_free_regex(parse_dep_regex);
      free(tmp_pkg_name);
      return newest_avail_pkg;
    }
  }

  if (tmp_cond_len > 3 ) {
    fprintf(stderr, gettext("pkg conditional too long\n"));
    slapt_free_regex(parse_dep_regex);
    free(tmp_pkg_name);
    return NULL;
  }

  strncpy(tmp_pkg_cond,
    dep_entry + parse_dep_regex->pmatch[2].rm_so,
    tmp_cond_len
  );
  tmp_pkg_cond[ tmp_cond_len ] = '\0';

  tmp_pkg_ver = slapt_regex_extract_match(parse_dep_regex, dep_entry, 3);

  slapt_free_regex(parse_dep_regex);

  /*
    * check the newest version of tmp_pkg_name (in newest_installed_pkg)
    * before we try looping through installed_pkgs
  */
  if (newest_installed_pkg != NULL) {

    /* if condition is "=",">=", or "=<" and versions are the same */
    if ((strchr(tmp_pkg_cond,'=') != NULL) &&
    (slapt_cmp_pkg_versions(tmp_pkg_ver,newest_installed_pkg->version) == 0) ) {
      free(tmp_pkg_name);
      free(tmp_pkg_ver);
      return newest_installed_pkg;
    }

    /* if "<" */
    if (strchr(tmp_pkg_cond,'<') != NULL) {
      if (slapt_cmp_pkg_versions(newest_installed_pkg->version,tmp_pkg_ver) < 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return newest_installed_pkg;
      }
    }

    /* if ">" */
    if (strchr(tmp_pkg_cond,'>') != NULL) {
      if (slapt_cmp_pkg_versions(newest_installed_pkg->version,tmp_pkg_ver) > 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return newest_installed_pkg;
      }
    }

  }

  for (i = 0; i < installed_pkgs->pkg_count; i++) {

    if (strcmp(tmp_pkg_name,installed_pkgs->pkgs[i]->name) != 0 )
      continue;

    /* if condition is "=",">=", or "=<" and versions are the same */
    if ((strchr(tmp_pkg_cond,'=') != NULL) &&
    (slapt_cmp_pkg_versions(tmp_pkg_ver,installed_pkgs->pkgs[i]->version) == 0) ) {
      free(tmp_pkg_name);
      free(tmp_pkg_ver);
      return installed_pkgs->pkgs[i];
    }

    /* if "<" */
    if (strchr(tmp_pkg_cond,'<') != NULL) {
      if (slapt_cmp_pkg_versions(installed_pkgs->pkgs[i]->version,
                                 tmp_pkg_ver) < 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return installed_pkgs->pkgs[i];
      }
    }

    /* if ">" */
    if (strchr(tmp_pkg_cond,'>') != NULL) {
      if (slapt_cmp_pkg_versions(installed_pkgs->pkgs[i]->version,
                                 tmp_pkg_ver) > 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return installed_pkgs->pkgs[i];
      }
    }

  }

  /*
    * check the newest version of tmp_pkg_name (in newest_avail_pkg)
    * before we try looping through avail_pkgs
  */
  if (newest_avail_pkg != NULL) {

    /* if condition is "=",">=", or "=<" and versions are the same */
    if ((strchr(tmp_pkg_cond,'=') != NULL) &&
    (slapt_cmp_pkg_versions(tmp_pkg_ver,newest_avail_pkg->version) == 0) ) {
      free(tmp_pkg_name);
      free(tmp_pkg_ver);
      return newest_avail_pkg;
    }

    /* if "<" */
    if (strchr(tmp_pkg_cond,'<') != NULL) {
      if (slapt_cmp_pkg_versions(newest_avail_pkg->version,tmp_pkg_ver) < 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return newest_avail_pkg;
      }
    }

    /* if ">" */
    if (strchr(tmp_pkg_cond,'>') != NULL) {
      if (slapt_cmp_pkg_versions(newest_avail_pkg->version,tmp_pkg_ver) > 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return newest_avail_pkg;
      }
    }

  }

  /* loop through avail_pkgs */
  for (i = 0; i < avail_pkgs->pkg_count; i++) {

    if (strcmp(tmp_pkg_name,avail_pkgs->pkgs[i]->name) != 0 )
      continue;

    /* if condition is "=",">=", or "=<" and versions are the same */
    if ((strchr(tmp_pkg_cond,'=') != NULL) &&
    (slapt_cmp_pkg_versions(tmp_pkg_ver,avail_pkgs->pkgs[i]->version) == 0)) {
      free(tmp_pkg_name);
      free(tmp_pkg_ver);
      return avail_pkgs->pkgs[i];
    }

    /* if "<" */
    if (strchr(tmp_pkg_cond,'<') != NULL) {
      if (slapt_cmp_pkg_versions(avail_pkgs->pkgs[i]->version,tmp_pkg_ver) < 0) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return avail_pkgs->pkgs[i];
      }
    }

    /* if ">" */
    if (strchr(tmp_pkg_cond,'>') != NULL) {
      if (slapt_cmp_pkg_versions(avail_pkgs->pkgs[i]->version,tmp_pkg_ver) > 0 ) {
        free(tmp_pkg_name);
        free(tmp_pkg_ver);
        return avail_pkgs->pkgs[i];
      }
    }

  }

  free(tmp_pkg_name);
  free(tmp_pkg_ver);

  return NULL;
}

slapt_pkg_list_t *slapt_is_required_by(const slapt_rc_config *global_config,
                                            slapt_pkg_list_t *avail,
                                            slapt_pkg_list_t *installed_pkgs,
                                            slapt_pkg_list_t *pkgs_to_install,
                                            slapt_pkg_list_t *pkgs_to_remove,
                                            slapt_pkg_info_t *pkg)
{
  slapt_pkg_list_t *required_by_list = slapt_init_pkg_list();

  /*
   * don't go any further if disable_dep_check is set
  */
  if (global_config->disable_dep_check == SLAPT_TRUE)
    return required_by_list;

  required_by(avail,installed_pkgs,pkgs_to_install,pkgs_to_remove,pkg,required_by_list);

  return required_by_list;
}

static char *escape_package_name(slapt_pkg_info_t *pkg)
{
  unsigned int name_len = 0, escape_count = 0, i;
  char *escaped_name = NULL, *escaped_ptr;
  char p;

  escaped_ptr = pkg->name;
  while ( (p = *escaped_ptr++) )
  {
    if (p == '+')
      escape_count++;
  }
  escaped_ptr = NULL;

  escaped_name = slapt_malloc(sizeof *escaped_name * (strlen(pkg->name) + escape_count + 1) );

  name_len = strlen(pkg->name);
  for (i = 0, escaped_ptr = escaped_name; i < name_len && pkg->name[i]; i++) {
    if (pkg->name[i] == '+' ) {
      *escaped_ptr++ = '\\';
      *escaped_ptr++ = pkg->name[i];
    } else {
      *escaped_ptr++ = pkg->name[i];
    }
    *escaped_ptr = '\0';
  }

  return escaped_name;
}

static void required_by(slapt_pkg_list_t *avail,
                        slapt_pkg_list_t *installed_pkgs,
                        slapt_pkg_list_t *pkgs_to_install,
                        slapt_pkg_list_t *pkgs_to_remove,
                        slapt_pkg_info_t *pkg,
                        slapt_pkg_list_t *required_by_list)
{
  unsigned int i;
  slapt_regex_t *required_by_reg = NULL;
  char *pkg_name  = escape_package_name(pkg);
  int reg_str_len = strlen(pkg_name) + 31;
  char *reg = slapt_malloc(sizeof *reg * reg_str_len);
  /* add word boundary to search */
  int sprintf_r = snprintf(reg, (size_t)reg_str_len, "[^a-zA-Z0-9\\-]*%s[^a-zA-Z0-9\\-]*", pkg_name);

  if (sprintf_r < 0) {
    fprintf(stderr,"sprintf error for %s\n", pkg_name);
    exit(EXIT_FAILURE);
  }

  reg[reg_str_len-1] = '\0';
  if ((required_by_reg = slapt_init_regex(reg)) == NULL) {
    exit(EXIT_FAILURE);
  }

  free(pkg_name);
  free(reg);

  for (i = 0; i < avail->pkg_count;i++) {
    slapt_pkg_info_t *pkg_ptr = avail->pkgs[i];
    slapt_list_t *dep_list = NULL;
    unsigned int d = 0;

    if (strcmp(pkg_ptr->required,"") == 0 )
      continue;
    if (strcmp(pkg_ptr->required," ") == 0 )
      continue;
    if (strcmp(pkg_ptr->required,"  ") == 0 )
      continue;
    if (strstr(pkg_ptr->required, pkg->name) == NULL)
        continue;

    slapt_execute_regex(required_by_reg,pkg_ptr->required);
    if (required_by_reg->reg_return != 0 )
      continue;

    /* check for the offending dependency entry and see if we have an alternative */
    dep_list = slapt_parse_delimited_list(pkg_ptr->required,',');
    for (d = 0; d < dep_list->count; d++) {
      slapt_list_t *satisfies = NULL;
      unsigned int s = 0;
      SLAPT_BOOL_T has_alternative = SLAPT_FALSE, found = SLAPT_FALSE;

      /* found our package in the list of dependencies */
      if (strstr(dep_list->items[d], pkg->name) == NULL)
        continue;

      /* not an |or, just add it */
      if (strchr(dep_list->items[d],'|') == NULL) {
        if (strcmp(dep_list->items[d], pkg->name) != 0)
          continue;
        if (slapt_get_exact_pkg(required_by_list, pkg_ptr->name, pkg_ptr->version) == NULL) {
          slapt_add_pkg_to_pkg_list(required_by_list,pkg_ptr);
          required_by(avail,installed_pkgs,pkgs_to_install,pkgs_to_remove, pkg_ptr, required_by_list );
        }
        break;
      }

      /* we need to find out if we have something else that satisfies the dependency  */
      satisfies = slapt_parse_delimited_list(dep_list->items[d],'|');
      for (s = 0; s < satisfies->count; s++) {
        slapt_pkg_info_t *tmp_pkg = parse_meta_entry(avail,installed_pkgs,satisfies->items[s]);
        if (tmp_pkg == NULL) 
          continue;

        if (strcmp(tmp_pkg->name,pkg->name) == 0) {
          found = SLAPT_TRUE;
          continue;
        }

        if (slapt_get_exact_pkg(installed_pkgs,tmp_pkg->name, tmp_pkg->version) != NULL) {
          if (slapt_get_exact_pkg(pkgs_to_remove,tmp_pkg->name, tmp_pkg->version) == NULL) {
            has_alternative = SLAPT_TRUE;
            break;
          }
        } else if (slapt_get_exact_pkg(pkgs_to_install, tmp_pkg->name, tmp_pkg->version) != NULL) {
          has_alternative = SLAPT_TRUE;
          break;
        }

      }
      slapt_free_list(satisfies);

      /* we couldn't find an installed pkg that satisfies the |or */
      if (has_alternative == SLAPT_FALSE && found == SLAPT_TRUE) {
        if (slapt_get_exact_pkg(required_by_list, pkg_ptr->name, pkg_ptr->version) == NULL) {
          slapt_add_pkg_to_pkg_list(required_by_list,pkg_ptr);
          required_by(avail,installed_pkgs,pkgs_to_install,pkgs_to_remove, pkg_ptr, required_by_list );
        }
      }

    }
    slapt_free_list(dep_list);


  }

  slapt_free_regex(required_by_reg);
}

slapt_pkg_info_t *slapt_get_pkg_by_details(slapt_pkg_list_t *list,
                                           const char *name,
                                           const char *version,
                                           const char *location)
{

  if (list->ordered) {
    int min = 0, max = list->pkg_count - 1;

    while (max >= min)
    {
      int pivot    = (min + max) / 2;
      int name_cmp  = strcmp(list->pkgs[pivot]->name, name);

      if ( name_cmp == 0 ) {

        int version_cmp = strverscmp(list->pkgs[pivot]->version, version);

        if ( version_cmp == 0 ) {

          int location_cmp = strcmp(list->pkgs[pivot]->location, location);

          if ( location_cmp == 0 ) {

            return list->pkgs[pivot];

          } else {

            if (location_cmp < 0 )
              min = pivot + 1;
            else
              max = pivot - 1;

          }


        } else {

          if ( version_cmp < 0 )
            min = pivot + 1;
          else
            max = pivot - 1;
           
        }

      } else {

        if ( name_cmp < 0 )
          min = pivot + 1;
        else
          max = pivot - 1;

      }

    }

  } else {
    unsigned int i;

    for (i = 0; i < list->pkg_count; i++) {

      if (strcmp(list->pkgs[i]->name,name) == 0 ) {
        if (version != NULL) {
          if (strcmp(list->pkgs[i]->version,version) == 0) {
            if (location != NULL) {
              if (strcmp(list->pkgs[i]->location,location) == 0) {
                return list->pkgs[i];
              }
            } else {
              return list->pkgs[i];
            }
          }
        }
      }
    }
  }

  return NULL;
}

/* update package data from mirror url */
int slapt_update_pkg_cache(const slapt_rc_config *global_config)
{
  unsigned int i,source_dl_failed = 0;
  slapt_pkg_list_t *new_pkgs = slapt_init_pkg_list();
  new_pkgs->free_pkgs = SLAPT_TRUE;

  /* go through each package source and download the meta data */
  for (i = 0; i < global_config->sources->count; i++) {
    unsigned int compressed = 0;
    slapt_pkg_list_t *available_pkgs = NULL;
    slapt_pkg_list_t *patch_pkgs = NULL;
    FILE *tmp_checksum_f = NULL;
    #ifdef SLAPT_HAS_GPGME
    FILE *tmp_signature_f = NULL;
    FILE *tmp_checksum_to_verify_f = NULL;
    #endif
    const char *source_url = global_config->sources->src[i]->url;
    SLAPT_PRIORITY_T source_priority = global_config->sources->src[i]->priority;

    if (global_config->sources->src[i]->disabled == SLAPT_TRUE)
      continue;

    /* download our SLAPT_PKG_LIST */
    printf(gettext("Retrieving package data [%s]..."), source_url);

    available_pkgs = slapt_get_pkg_source_packages(global_config, source_url, &compressed);
    if (available_pkgs == NULL) {
      source_dl_failed = 1;
      continue;
    }

    /* download SLAPT_PATCHES_LIST */
    printf(gettext("Retrieving patch list [%s]..."), source_url);

    patch_pkgs = slapt_get_pkg_source_patches(global_config, source_url, &compressed);


    /* download checksum file */
    printf(gettext("Retrieving checksum list [%s]..."), source_url);
    tmp_checksum_f = slapt_get_pkg_source_checksums(global_config, source_url, &compressed);

    #ifdef SLAPT_HAS_GPGME
    printf(gettext("Retrieving checksum signature [%s]..."), source_url);
    tmp_signature_f = slapt_get_pkg_source_checksums_signature (global_config,source_url, &compressed);

    /* if we downloaded the compressed checksums, open it raw (w/o gunzipping) */
    if (compressed == 1)
    {
      char *filename = slapt_gen_filename_from_url(source_url, SLAPT_CHECKSUM_FILE_GZ);
      tmp_checksum_to_verify_f = slapt_open_file(filename,"r");
      free(filename);
    } else {
      tmp_checksum_to_verify_f = tmp_checksum_f;
    }

    if (tmp_signature_f != NULL && tmp_checksum_to_verify_f != NULL) {
      slapt_code_t verified = SLAPT_CHECKSUMS_NOT_VERIFIED;
      printf(gettext("Verifying checksum signature [%s]..."), source_url);
      verified = slapt_gpg_verify_checksums(tmp_checksum_to_verify_f, tmp_signature_f);
      if (verified == SLAPT_CHECKSUMS_VERIFIED) {
        printf("%s\n",gettext("Verified"));
      } else if (verified == SLAPT_CHECKSUMS_MISSING_KEY) {
        printf("%s\n",gettext("No key for verification"));
      } else if ((global_config->gpgme_allow_unauth == SLAPT_TRUE) && (slapt_pkg_sign_is_unauthenticated(verified) == SLAPT_TRUE)) {
        printf("%s%s\n", slapt_strerror(verified), gettext(", but accepted as an exception"));
      } else {
        printf("%s\n",gettext(slapt_strerror(verified)));
        source_dl_failed = 1;
        fclose(tmp_checksum_f);
        tmp_checksum_f = NULL;
      }
    }

    if (tmp_signature_f)
      fclose(tmp_signature_f);

    /* if we opened the raw gzipped checksums, close it here */
    if (compressed == 1) {
      fclose(tmp_checksum_to_verify_f);
    } else {
      if (tmp_checksum_f)
        rewind(tmp_checksum_f);
    }
    #endif

    if (source_dl_failed != 1) {
      printf(gettext("Retrieving ChangeLog.txt [%s]..."), source_url);
      slapt_get_pkg_source_changelog(global_config, source_url, &compressed);
    }

    if (tmp_checksum_f != NULL) {
      unsigned int pkg_i;

      /* now map md5 checksums to packages */
      printf(gettext("Reading Package Lists..."));

      slapt_get_md5sums(available_pkgs, tmp_checksum_f);

      for (pkg_i = 0; pkg_i < available_pkgs->pkg_count; ++pkg_i) {
        slapt_pkg_info_t *p = available_pkgs->pkgs[pkg_i];
        int mirror_len = -1;

        /* honor the mirror if it was set in the PACKAGES.TXT */
        if (p->mirror == NULL || (mirror_len = strlen(p->mirror)) == 0) {

          if (mirror_len == 0)
            free(p->mirror);

          p->mirror = strdup(source_url);
        }

        /* set the priority of the package based on the source */
        p->priority = source_priority;

        slapt_add_pkg_to_pkg_list(new_pkgs,p);
      }
      available_pkgs->free_pkgs = SLAPT_FALSE;

      if (patch_pkgs) {
        slapt_get_md5sums(patch_pkgs, tmp_checksum_f);
        for (pkg_i = 0; pkg_i < patch_pkgs->pkg_count; ++pkg_i) {
          slapt_pkg_info_t *p = patch_pkgs->pkgs[pkg_i];
          int mirror_len = -1;

          /* honor the mirror if it was set in the PACKAGES.TXT */
          if (p->mirror == NULL || (mirror_len = strlen(p->mirror)) == 0) {

            if (mirror_len == 0)
              free(p->mirror);

            p->mirror = strdup(source_url);
          }

          /* set the priority of the package based on the source, plus 1 for the patch priority */
          if (global_config->use_priority == SLAPT_TRUE)
            p->priority = source_priority + 1;
          else
            p->priority = source_priority;

          slapt_add_pkg_to_pkg_list(new_pkgs,p);
        }
        patch_pkgs->free_pkgs = SLAPT_FALSE;
      }

      printf(gettext("Done\n"));

      fclose(tmp_checksum_f);
    } else {
      source_dl_failed = 1;
    }


    if (available_pkgs)
      slapt_free_pkg_list(available_pkgs);

    if (patch_pkgs)
      slapt_free_pkg_list(patch_pkgs);

  }/* end for loop */

  /* if all our downloads where a success, write to SLAPT_PKG_LIST_L */
  if (source_dl_failed != 1 ) {
    FILE *pkg_list_fh;

    if ((pkg_list_fh = slapt_open_file(SLAPT_PKG_LIST_L,"w+")) == NULL)
      exit(EXIT_FAILURE);

    qsort( new_pkgs->pkgs, new_pkgs->pkg_count, sizeof(new_pkgs->pkgs[0]), pkg_compare );

    slapt_write_pkg_data(NULL,pkg_list_fh,new_pkgs);

    fclose(pkg_list_fh);

  } else {
    printf(gettext("Sources failed to download, correct sources and rerun --update\n"));
  }

  slapt_free_pkg_list(new_pkgs);

  return source_dl_failed;
}

#ifdef SLAPT_HAS_GPGME
SLAPT_BOOL_T slapt_pkg_sign_is_unauthenticated(slapt_code_t code)
{
  switch (code)
  {
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_REVOKED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_KEY_EXPIRED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_SIG_EXPIRED:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_MISSING:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_CRL_TOO_OLD:
    case SLAPT_CHECKSUMS_NOT_VERIFIED_GPGME_BAD_POLICY:
      return SLAPT_TRUE;
    default:
      break;
  }

  return SLAPT_FALSE;
}
#endif

slapt_pkg_list_t *slapt_init_pkg_list(void)
{
  slapt_pkg_list_t *list = NULL;

  list            = slapt_malloc(sizeof *list);
  list->pkgs      = slapt_malloc(sizeof *list->pkgs);
  list->pkg_count = 0;
  list->free_pkgs = SLAPT_FALSE;
  list->ordered   = SLAPT_FALSE;

  return list;
}

void slapt_add_pkg_to_pkg_list(slapt_pkg_list_t *list,
                               slapt_pkg_info_t *pkg)
{
  slapt_pkg_info_t **realloc_tmp;

  /* grow our struct array */
  realloc_tmp = realloc(list->pkgs,
    sizeof *list->pkgs * (list->pkg_count + 1)
  );
  if (realloc_tmp == NULL) {
    fprintf(stderr,gettext("Failed to realloc %s\n"),"pkgs");
    exit(EXIT_FAILURE);
  }

  list->pkgs = realloc_tmp;
  list->pkgs[list->pkg_count] = pkg;
  ++list->pkg_count;

}

slapt_pkg_info_t *slapt_init_pkg(void)
{
  slapt_pkg_info_t *pkg;

  pkg = slapt_malloc(sizeof *pkg);

  pkg->size_c = 0;
  pkg->size_u = 0;

  pkg->name     = NULL;
  pkg->version  = NULL;
  pkg->mirror   = NULL;
  pkg->location = NULL;
  pkg->file_ext = NULL;

  pkg->description    = slapt_malloc(sizeof *pkg->description); 
  pkg->description[0] = '\0';

  pkg->required       = slapt_malloc(sizeof *pkg->required);
  pkg->required[0]    = '\0';

  pkg->conflicts      = slapt_malloc(sizeof *pkg->conflicts);
  pkg->conflicts[0]   = '\0';

  pkg->suggests       = slapt_malloc(sizeof *pkg->suggests);
  pkg->suggests[0]    = '\0';

  pkg->md5[0] = '\0';

  pkg->priority = SLAPT_PRIORITY_DEFAULT;

  pkg->installed = SLAPT_FALSE;

  return pkg;
}

/* generate the package file name */
char *slapt_gen_pkg_file_name(const slapt_rc_config *global_config,
                              slapt_pkg_info_t *pkg)
{
  char *file_name = NULL;

  /* build the file name */
  file_name = slapt_calloc(
    strlen(global_config->working_dir) + strlen(pkg->location) + 
    strlen(pkg->name) + strlen(pkg->version)+strlen(pkg->file_ext) + 4,
    sizeof *file_name
  );
  file_name = strncpy(file_name,
    global_config->working_dir,strlen(global_config->working_dir)
  );
  file_name[ strlen(global_config->working_dir) ] = '\0';
  file_name = strncat(file_name,"/",1);
  file_name = strncat(file_name,pkg->location,strlen(pkg->location));
  file_name = strncat(file_name,"/",1);
  file_name = strncat(file_name,pkg->name,strlen(pkg->name));
  file_name = strncat(file_name,"-",1);
  file_name = strncat(file_name,pkg->version,strlen(pkg->version));
  file_name = strncat(file_name,pkg->file_ext,strlen(pkg->file_ext));

  return file_name;
}

/* generate the download url for a package */
char *slapt_gen_pkg_url(slapt_pkg_info_t *pkg)
{
  char *url = NULL;
  char *file_name = NULL;

  /* build the file name */
  file_name = slapt_stringify_pkg(pkg);

  /* build the url */
  url = slapt_calloc(strlen(pkg->mirror) + strlen(pkg->location) +
                     strlen(file_name) + 2, sizeof *url);

  url = strncpy(url,pkg->mirror,strlen(pkg->mirror));
  url[strlen(pkg->mirror)] = '\0';
  url = strncat(url,pkg->location,strlen(pkg->location));
  url = strncat(url,"/",1);
  url = strncat(url,file_name,strlen(file_name));

  free(file_name);
  return url;
}

/* find out the pkg file size (post download) */
size_t slapt_get_pkg_file_size(const slapt_rc_config *global_config,
                               slapt_pkg_info_t *pkg)
{
  char *file_name = NULL;
  struct stat file_stat;
  size_t file_size = 0;

  /* build the file name */
  file_name = slapt_gen_pkg_file_name(global_config,pkg);

  if (stat(file_name,&file_stat) == 0 ) {
    file_size = file_stat.st_size;
  }
  free(file_name);

  return file_size;
}

/* package is already downloaded and cached, md5sum if applicable is ok */
slapt_code_t slapt_verify_downloaded_pkg(const slapt_rc_config *global_config,
                                slapt_pkg_info_t *pkg)
{
  char *file_name = NULL;
  FILE *fh_test = NULL;
  char md5sum_f[SLAPT_MD5_STR_LEN];

  /*
  size_t file_size = 0;
    check the file size first so we don't run an md5 checksum
    on an incomplete file

    XXX 2009-04-27 XXX
    This has become increasingly less reliable, especially with
    recent changes in how the size is calculated when generating
    the PACKAGES.TXT... we do not really lose a lot by not checking
    since we are validating the checksum anyway.

  file_size = slapt_get_pkg_file_size(global_config,pkg);
  if ((unsigned int)(file_size/1024) != pkg->size_c) {
    return SLAPT_DOWNLOAD_INCOMPLETE;
  }
  */

  /* if not checking the md5 checksum and the sizes match, assume its good */
  if (global_config->no_md5_check == SLAPT_TRUE)
    return SLAPT_OK;

  /* check to see that we actually have an md5 checksum */
  if (strcmp(pkg->md5,"") == 0) {
    return SLAPT_MD5_CHECKSUM_MISSING;
  }

  /* build the file name */
  file_name = slapt_gen_pkg_file_name(global_config,pkg);

  /* return if we can't open the file */
  if ((fh_test = fopen(file_name,"r") ) == NULL) {
    free(file_name);
    return SLAPT_DOWNLOAD_INCOMPLETE;
  }
  free(file_name);

  /* generate the md5 checksum */
  slapt_gen_md5_sum_of_file(fh_test,md5sum_f);
  fclose(fh_test);

  /* check to see if the md5sum is correct */
  if (strcmp(md5sum_f,pkg->md5) == 0 )
    return SLAPT_OK;

  return SLAPT_MD5_CHECKSUM_MISMATCH;

}

char *slapt_gen_filename_from_url(const char *url,const char *file)
{
  char *filename,*cleaned;

  filename = slapt_calloc(strlen(url) + strlen(file) + 2 , sizeof *filename);
  filename[0] = '.';
  strncat(filename,url,strlen(url));
  strncat(filename,file,strlen(file));

  cleaned = slapt_str_replace_chr(filename,'/','#');

  free(filename);

  return cleaned;
}

void slapt_purge_old_cached_pkgs(const slapt_rc_config *global_config,
                                 const char *dir_name,
                                 slapt_pkg_list_t *avail_pkgs)
{
  DIR *dir;
  struct dirent *file;
  struct stat file_stat;
  slapt_regex_t *cached_pkgs_regex = NULL;
  int local_pkg_list = 0;

  if (avail_pkgs == NULL) {
    avail_pkgs = slapt_get_available_pkgs();
    local_pkg_list = 1;
  }

  if (dir_name == NULL)
    dir_name = (char *)global_config->working_dir;

  if ((cached_pkgs_regex = slapt_init_regex(SLAPT_PKG_PARSE_REGEX)) == NULL) {
    exit(EXIT_FAILURE);
  }

  if ((dir = opendir(dir_name)) == NULL) {

    if (errno)
      perror(dir_name);

    fprintf(stderr,gettext("Failed to opendir %s\n"),dir_name);
    return;
  }

  if (chdir(dir_name) == -1 ) {

    if (errno)
      perror(dir_name);

    fprintf(stderr,gettext("Failed to chdir: %s\n"),dir_name);
    closedir(dir);
    return;
  }

  while ((file = readdir(dir)) ) {

    /* make sure we don't have . or .. */
    if ((strcmp(file->d_name,"..")) == 0 || (strcmp(file->d_name,".") == 0))
      continue;

    /* setup file_stat struct */
    if ((lstat(file->d_name,&file_stat)) == -1)
      continue;

    /* if its a directory, recurse */
    if (S_ISDIR(file_stat.st_mode) ) {
      slapt_purge_old_cached_pkgs(global_config,file->d_name,avail_pkgs);
      if ((chdir("..")) == -1 ) {
        fprintf(stderr,gettext("Failed to chdir: %s\n"),dir_name);
        return;
      }
      continue;
    }

    /* if its a package */
    if (strstr(file->d_name,".t") != NULL) {

      slapt_execute_regex(cached_pkgs_regex,file->d_name);

      /* if our regex matches */
      if (cached_pkgs_regex->reg_return == 0 ) {
        char *tmp_pkg_name,*tmp_pkg_version;
        slapt_pkg_info_t *tmp_pkg;

        tmp_pkg_name    = slapt_regex_extract_match(cached_pkgs_regex, file->d_name, 1);
        tmp_pkg_version = slapt_regex_extract_match(cached_pkgs_regex, file->d_name, 2);
        tmp_pkg         = slapt_get_exact_pkg(avail_pkgs,tmp_pkg_name,tmp_pkg_version);

        free(tmp_pkg_name);
        free(tmp_pkg_version);

        if (tmp_pkg == NULL) {

          if (global_config->no_prompt == SLAPT_TRUE) {
              unlink(file->d_name);
          } else {
            if (slapt_ask_yes_no(gettext("Delete %s ? [y/N]"),file->d_name) == 1)
              unlink(file->d_name);
          }

        }
        tmp_pkg = NULL;

      }

    }

  }
  closedir(dir);

  slapt_free_regex(cached_pkgs_regex);
  if (local_pkg_list == 1 ) {
    slapt_free_pkg_list(avail_pkgs);
  }

}

void slapt_clean_pkg_dir(const char *dir_name)
{
  DIR *dir;
  struct dirent *file;
  struct stat file_stat;
  slapt_regex_t *cached_pkgs_regex = NULL;

  if ((dir = opendir(dir_name)) == NULL) {
    fprintf(stderr,gettext("Failed to opendir %s\n"),dir_name);
    return;
  }

  if (chdir(dir_name) == -1 ) {
    fprintf(stderr,gettext("Failed to chdir: %s\n"),dir_name);
    closedir(dir);
    return;
  }

  if ((cached_pkgs_regex = slapt_init_regex(SLAPT_PKG_PARSE_REGEX)) == NULL) {
    exit(EXIT_FAILURE);
  }

  while ((file = readdir(dir))) {

    /* make sure we don't have . or .. */
    if ((strcmp(file->d_name,"..")) == 0 || (strcmp(file->d_name,".") == 0))
      continue;

    if ((lstat(file->d_name,&file_stat)) == -1)
      continue;

    /* if its a directory, recurse */
    if (S_ISDIR(file_stat.st_mode) ) {
      slapt_clean_pkg_dir(file->d_name);
      if ((chdir("..")) == -1) {
        fprintf(stderr,gettext("Failed to chdir: %s\n"),dir_name);
        return;
      }
      continue;
    }
    if (strstr(file->d_name,".t") !=NULL) {
      slapt_execute_regex(cached_pkgs_regex,file->d_name);

      /* if our regex matches */
      if (cached_pkgs_regex->reg_return == 0 ) {
        unlink(file->d_name);
      }
    }
  }
  closedir(dir);

  slapt_free_regex(cached_pkgs_regex);

}

/* find dependency from "or" requirement */
static slapt_pkg_info_t *find_or_requirement(slapt_pkg_list_t *avail_pkgs,
                                             slapt_pkg_list_t *installed_pkgs,
                                             char *required_str)
{
  slapt_pkg_info_t *pkg = NULL;
  unsigned int i = 0;
  slapt_list_t *alternates = slapt_parse_delimited_list(required_str,'|');

  for (i = 0; i < alternates->count; i++) {
    slapt_pkg_info_t *tmp_pkg = parse_meta_entry(avail_pkgs,installed_pkgs,alternates->items[i]);

    if (tmp_pkg != NULL) {
      /* installed packages are preferred */
      if (slapt_get_exact_pkg(installed_pkgs,tmp_pkg->name, tmp_pkg->version) != NULL) {
        pkg = tmp_pkg;
        break;
      } else {
        /* otherwise remember the first package found and continue on */
        if (pkg == NULL) {
          pkg = tmp_pkg;
        }
      }
    }

  }

  slapt_free_list(alternates);
  return pkg;
}

slapt_pkg_info_t *slapt_copy_pkg(slapt_pkg_info_t *dst,slapt_pkg_info_t *src)
{
  dst = memcpy(dst,src, sizeof *src);

  if (src->name != NULL)
    dst->name = strndup(src->name,strlen(src->name));

  if (src->version != NULL)
    dst->version = strndup(src->version,strlen(src->version));

  if (src->file_ext != NULL)
    dst->file_ext = strndup(src->file_ext,strlen(src->file_ext));

  if (src->mirror != NULL)
    dst->mirror = strndup(src->mirror,strlen(src->mirror));

  if (src->location != NULL)
    dst->location = strndup(src->location,strlen(src->location));

  if (src->description != NULL)
    dst->description = strndup(src->description,strlen(src->description));

  if (src->suggests != NULL)
    dst->suggests = strndup(src->suggests, strlen(src->suggests));

  if (src->conflicts != NULL)
    dst->conflicts = strndup(src->conflicts, strlen(src->conflicts));

  if (src->required != NULL)
    dst->required = strndup(src->required, strlen(src->required));

  return dst;
}

slapt_pkg_err_list_t *slapt_init_pkg_err_list(void)
{
  slapt_pkg_err_list_t *l = slapt_malloc(sizeof *l);
  l->errs = slapt_malloc(sizeof *l->errs);
  l->err_count = 0;

  return l;
}

void slapt_add_pkg_err_to_list(slapt_pkg_err_list_t *l,
                         const char *pkg,const char *err)
{
  slapt_pkg_err_t **tmp;

  if (slapt_search_pkg_err_list(l,pkg,err) == 1)
    return;

  tmp = realloc(l->errs, sizeof *l->errs * (l->err_count + 1) );
  if (tmp == NULL)
    return;

  l->errs = tmp;

  l->errs[l->err_count] = slapt_malloc(sizeof *l->errs[l->err_count]);
  l->errs[l->err_count]->pkg = strdup(pkg);
  l->errs[l->err_count]->error = strdup(err);

  ++l->err_count;

}

int slapt_search_pkg_err_list(slapt_pkg_err_list_t *l,
                        const char *pkg, const char *err)
{
  unsigned int i,found = 1, not_found = 0;
  for (i = 0; i < l->err_count; ++i) {
    if (strcmp(l->errs[i]->pkg,pkg) == 0 &&
         strcmp(l->errs[i]->error,err) == 0
    ) {
      return found;
    }
  }
  return not_found;
}

void slapt_free_pkg_err_list(slapt_pkg_err_list_t *l)
{
  unsigned int i;

  for (i = 0; i < l->err_count; ++i) {
    free(l->errs[i]->pkg);
    free(l->errs[i]->error);
    free(l->errs[i]);
  }
  free(l->errs);
  free(l);
}

/* FIXME this sucks... it needs to check file headers and more */
static FILE *slapt_gunzip_file (const char *file_name,FILE *dest_file)
{
  gzFile data = NULL;
  char buffer[SLAPT_MAX_ZLIB_BUFFER];

  if (dest_file == NULL)
    if ((dest_file = tmpfile()) == NULL)
      exit(EXIT_FAILURE);

  if ((data = gzopen(file_name,"rb")) == NULL) 
    exit(EXIT_FAILURE);

  while (gzgets(data,buffer,SLAPT_MAX_ZLIB_BUFFER) != Z_NULL) {
    fprintf(dest_file,"%s",buffer);
  }
  gzclose(data);
  rewind(dest_file);

  return dest_file;
}

slapt_pkg_list_t *slapt_get_pkg_source_packages (const slapt_rc_config *global_config,
                                                      const char *url, unsigned int *compressed)
{
  slapt_pkg_list_t *available_pkgs = NULL;
  char *pkg_head = NULL;
  SLAPT_BOOL_T is_interactive = slapt_is_interactive(global_config);

  *compressed = 0;

  /* try gzipped package list */
  if ((pkg_head = slapt_head_mirror_data(url,SLAPT_PKG_LIST_GZ)) != NULL) {
    char *pkg_filename = slapt_gen_filename_from_url(url,SLAPT_PKG_LIST_GZ);
    char *pkg_local_head = slapt_read_head_cache(pkg_filename);

    /* is it cached ? */
    if (pkg_local_head != NULL && strcmp(pkg_head,pkg_local_head) == 0) {
      FILE *tmp_pkg_f = NULL;

      if ((tmp_pkg_f = tmpfile()) == NULL) 
        exit(EXIT_FAILURE);

      slapt_gunzip_file(pkg_filename,tmp_pkg_f);

      available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);
      fclose(tmp_pkg_f);

      if (available_pkgs == NULL || available_pkgs->pkg_count < 1 ) {
        slapt_clear_head_cache(pkg_filename);
        fprintf(stderr,gettext("Failed to parse package data from %s\n"),
          pkg_filename
        );

        if (available_pkgs)
          slapt_free_pkg_list(available_pkgs);

        free(pkg_filename);
        free(pkg_local_head);
        return NULL;
      }

      if (is_interactive)
        printf(gettext("Cached\n"));

    } else {
      FILE *tmp_pkg_f = NULL;
      const char *err = NULL;

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      if ((tmp_pkg_f = slapt_open_file(pkg_filename,"w+b")) == NULL)
        exit(EXIT_FAILURE);

      /* retrieve the compressed package data */
      err = slapt_get_mirror_data_from_source(tmp_pkg_f,  
                                              global_config,url,
                                              SLAPT_PKG_LIST_GZ);
      if (!err) {
        FILE *tmp_pkg_uncompressed_f = NULL;

        fclose(tmp_pkg_f);

        if ((tmp_pkg_uncompressed_f = tmpfile()) == NULL)
          exit(EXIT_FAILURE);

        slapt_gunzip_file(pkg_filename,tmp_pkg_uncompressed_f);

        /* parse packages from what we downloaded */
        available_pkgs = slapt_parse_packages_txt(tmp_pkg_uncompressed_f);

        fclose(tmp_pkg_uncompressed_f);

        /* if we can't parse any packages out of this */
        if (available_pkgs == NULL || available_pkgs->pkg_count < 1 ) {
          slapt_clear_head_cache(pkg_filename);

          fprintf(stderr,gettext("Failed to parse package data from %s\n"),url);

          if (available_pkgs)
            slapt_free_pkg_list(available_pkgs);

          free(pkg_filename);
          free(pkg_local_head);
          return NULL;
        }

        /* commit the head data for later comparisons */
        if (pkg_head != NULL)
          slapt_write_head_cache(pkg_head,pkg_filename);

        if (is_interactive)
          printf(gettext("Done\n"));

      } else {
        fprintf(stderr,gettext("Failed to download: %s\n"),err);
        fclose(tmp_pkg_f);
        slapt_clear_head_cache(pkg_filename);
        free(pkg_filename);
        free(pkg_local_head);
        return NULL;
      }

    }

    free(pkg_filename);
    free(pkg_local_head);
    *compressed = 1;

  } else { /* fall back to uncompressed package list */
    char *pkg_filename = slapt_gen_filename_from_url(url,SLAPT_PKG_LIST);
    char *pkg_local_head = slapt_read_head_cache(pkg_filename);
    /*
      we go ahead and run the head request, not caring if it failed.
      If the subsequent download fails as well, it will give a nice
      error message of why.
    */
    pkg_head = slapt_head_mirror_data(url,SLAPT_PKG_LIST);

    /* is it cached ? */
    if (pkg_head != NULL && pkg_local_head != NULL &&
        strcmp(pkg_head,pkg_local_head) == 0) {
      FILE *tmp_pkg_f = NULL;
      if ((tmp_pkg_f = slapt_open_file(pkg_filename,"r")) == NULL) 
        exit(EXIT_FAILURE);

      available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);
      fclose(tmp_pkg_f);

      if (is_interactive)
        printf(gettext("Cached\n"));

    } else {
      FILE *tmp_pkg_f = NULL;
      const char *err = NULL;

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      if ((tmp_pkg_f = slapt_open_file(pkg_filename,"w+b")) == NULL)
        exit(EXIT_FAILURE);

      /* retrieve the uncompressed package data */
      err = slapt_get_mirror_data_from_source(tmp_pkg_f,
                                              global_config,url,
                                              SLAPT_PKG_LIST);
      if (!err) {
        rewind(tmp_pkg_f); /* make sure we are back at the front of the file */

        /* parse packages from what we downloaded */
        available_pkgs = slapt_parse_packages_txt(tmp_pkg_f);

        /* if we can't parse any packages out of this */
        if (available_pkgs == NULL || available_pkgs->pkg_count < 1 ) {
          slapt_clear_head_cache(pkg_filename);

          fprintf(stderr,gettext("Failed to parse package data from %s\n"),url);

          if (available_pkgs)
            slapt_free_pkg_list(available_pkgs);

          fclose(tmp_pkg_f);
          free(pkg_filename);
          free(pkg_local_head);
          slapt_clear_head_cache(pkg_filename);
          return NULL;
        }

        /* commit the head data for later comparisons */
        if (pkg_head != NULL)
          slapt_write_head_cache(pkg_head,pkg_filename);

        if (is_interactive)
          printf(gettext("Done\n"));

      } else {
        fprintf(stderr,gettext("Failed to download: %s\n"),err);
        slapt_clear_head_cache(pkg_filename);
        free(pkg_filename);
        free(pkg_local_head);
        fclose(tmp_pkg_f);
        return NULL;
      }

      fclose(tmp_pkg_f);
    }

    free(pkg_filename);
    free(pkg_local_head);

  }
  if (pkg_head != NULL)
    free(pkg_head);

  return available_pkgs;
}

slapt_pkg_list_t *slapt_get_pkg_source_patches (const slapt_rc_config *global_config,
                                                     const char *url, unsigned int *compressed)
{
  slapt_pkg_list_t *patch_pkgs = NULL;
  char *patch_head = NULL;
  SLAPT_BOOL_T is_interactive = slapt_is_interactive(global_config);
  *compressed = 0;

  if ((patch_head = slapt_head_mirror_data(url,SLAPT_PATCHES_LIST_GZ)) != NULL) {
    char *patch_filename = slapt_gen_filename_from_url(url,SLAPT_PATCHES_LIST_GZ);
    char *patch_local_head = slapt_read_head_cache(patch_filename);

    if (patch_local_head != NULL && strcmp(patch_head,patch_local_head) == 0) {
      FILE *tmp_patch_f = NULL;

      if ((tmp_patch_f = tmpfile()) == NULL)
        exit(EXIT_FAILURE);

      slapt_gunzip_file(patch_filename,tmp_patch_f);

      patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);
      fclose(tmp_patch_f);

      if (is_interactive)
        printf(gettext("Cached\n"));

    } else {
      FILE *tmp_patch_f = NULL;
      const char *err = NULL;

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      if ((tmp_patch_f = slapt_open_file(patch_filename,"w+b")) == NULL)
        exit (1);

      err = slapt_get_mirror_data_from_source(tmp_patch_f,
                                              global_config,url,
                                              SLAPT_PATCHES_LIST_GZ);
      if (!err) {
        FILE *tmp_patch_uncompressed_f = NULL;

        fclose(tmp_patch_f);

        if ((tmp_patch_uncompressed_f = tmpfile()) == NULL)
          exit(EXIT_FAILURE);

        slapt_gunzip_file(patch_filename,tmp_patch_uncompressed_f);

        patch_pkgs = slapt_parse_packages_txt(tmp_patch_uncompressed_f);

        if (is_interactive)
          printf(gettext("Done\n"));

        if (patch_head != NULL)
          slapt_write_head_cache(patch_head,patch_filename);

        fclose(tmp_patch_uncompressed_f);
      } else {
        fprintf(stderr,gettext("Failed to download: %s\n"),err);
        fclose(tmp_patch_f);
        /* we don't care if the patch fails, for example current
          doesn't have patches source_dl_failed = 1; */
        slapt_clear_head_cache(patch_filename);
      }

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

    }

    free(patch_local_head);
    free(patch_filename);
    *compressed = 1;

  } else {
    char *patch_filename = slapt_gen_filename_from_url(url,SLAPT_PATCHES_LIST);
    char *patch_local_head = slapt_read_head_cache(patch_filename);
    /*
      we go ahead and run the head request, not caring if it failed.
      If the subsequent download fails as well, it will give a nice
      error message of why.
    */
    patch_head = slapt_head_mirror_data(url,SLAPT_PATCHES_LIST);

    if (patch_head != NULL && patch_local_head != NULL &&
        strcmp(patch_head,patch_local_head) == 0) {
      FILE *tmp_patch_f = NULL;

      if ((tmp_patch_f = slapt_open_file(patch_filename,"r")) == NULL)
        exit(EXIT_FAILURE);

      patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);

      if (is_interactive)
        printf(gettext("Cached\n"));

      fclose(tmp_patch_f);
    } else {
      FILE *tmp_patch_f = NULL;
      const char *err = NULL;

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      if ((tmp_patch_f = slapt_open_file(patch_filename,"w+b")) == NULL)
        exit (1);

      err = slapt_get_mirror_data_from_source(tmp_patch_f,
                                              global_config,url,
                                              SLAPT_PATCHES_LIST);
      if (!err) {
        rewind(tmp_patch_f); /* make sure we are back at the front of the file */
        patch_pkgs = slapt_parse_packages_txt(tmp_patch_f);

        if (is_interactive)
          printf(gettext("Done\n"));

        if (patch_head != NULL)
          slapt_write_head_cache(patch_head,patch_filename);

      } else {
        /* we don't care if the patch fails, for example current
           doesn't have patches source_dl_failed = 1; */
        slapt_clear_head_cache(patch_filename);

        if (is_interactive)
          printf(gettext("Done\n"));

      }

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      fclose(tmp_patch_f);
    }

    free(patch_local_head);
    free(patch_filename);
  }
  if (patch_head != NULL)
    free(patch_head);

  return patch_pkgs;
}

FILE *slapt_get_pkg_source_checksums (const slapt_rc_config *global_config,
                                      const char *url, unsigned int *compressed)
{
  FILE *tmp_checksum_f = NULL;
  char *checksum_head = NULL;
  SLAPT_BOOL_T is_interactive = slapt_is_interactive(global_config);
  *compressed = 0;

  if ((checksum_head = slapt_head_mirror_data(url,SLAPT_CHECKSUM_FILE_GZ)) != NULL) {
    char *filename = slapt_gen_filename_from_url(url,SLAPT_CHECKSUM_FILE_GZ);
    char *local_head = slapt_read_head_cache(filename);

    if (local_head != NULL && strcmp(checksum_head,local_head) == 0) {

      if ((tmp_checksum_f = tmpfile()) == NULL)
        exit(EXIT_FAILURE);

      slapt_gunzip_file(filename,tmp_checksum_f);

      if (is_interactive)
        printf(gettext("Cached\n"));

    } else {
      FILE *working_checksum_f = NULL;
      const char *err = NULL;

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      if ((working_checksum_f = slapt_open_file(filename,"w+b")) == NULL)
        exit(EXIT_FAILURE);

      err = slapt_get_mirror_data_from_source(working_checksum_f,
                                              global_config,url,
                                              SLAPT_CHECKSUM_FILE_GZ);
      if (!err) {

        if (global_config->dl_stats == SLAPT_TRUE)
          printf("\n");
        if (is_interactive) 
          printf(gettext("Done\n"));

        fclose(working_checksum_f);

        if ((tmp_checksum_f = tmpfile()) == NULL)
          exit(EXIT_FAILURE);

        slapt_gunzip_file(filename,tmp_checksum_f);

        /* if all is good, write it */
        if (checksum_head != NULL)
          slapt_write_head_cache(checksum_head,filename);

      } else {
        fprintf(stderr,gettext("Failed to download: %s\n"),err);
        slapt_clear_head_cache(filename);
        tmp_checksum_f = working_checksum_f;
        working_checksum_f = NULL;
        free(filename);
        free(local_head);
        fclose(tmp_checksum_f);
        if (checksum_head != NULL)
          free(checksum_head);
        return NULL;
      }
      /* make sure we are back at the front of the file */
      rewind(tmp_checksum_f);

    }

    free(filename);
    free(local_head);
    *compressed = 1;

  } else {
    char *filename = slapt_gen_filename_from_url(url,SLAPT_CHECKSUM_FILE);
    char *local_head = slapt_read_head_cache(filename);
    /*
      we go ahead and run the head request, not caring if it failed.
      If the subsequent download fails as well, it will give a nice
      error message of why.
    */
    checksum_head = slapt_head_mirror_data(url,SLAPT_CHECKSUM_FILE);

    if (checksum_head != NULL && local_head != NULL &&
        strcmp(checksum_head,local_head) == 0) {
      if ((tmp_checksum_f = slapt_open_file(filename,"r")) == NULL)
        exit(EXIT_FAILURE);

      if (is_interactive)
        printf(gettext("Cached\n"));

    } else {
      const char *err = NULL;

      if ((tmp_checksum_f = slapt_open_file(filename,"w+b")) == NULL)
        exit(EXIT_FAILURE);

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

      err = slapt_get_mirror_data_from_source(tmp_checksum_f,
                                              global_config,url,
                                              SLAPT_CHECKSUM_FILE);
      if (!err) {

        if (is_interactive)
          printf(gettext("Done\n"));

      } else {
        fprintf(stderr,gettext("Failed to download: %s\n"),err);
        slapt_clear_head_cache(filename);
        fclose(tmp_checksum_f);
        free(filename);
        free(local_head);
        if (checksum_head != NULL)
          free(checksum_head);
        return NULL;
      }
      /* make sure we are back at the front of the file */
      rewind(tmp_checksum_f);

      /* if all is good, write it */
      if (checksum_head != NULL)
        slapt_write_head_cache(checksum_head,filename);

      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");

    }

    free(filename);
    free(local_head);
  }
  if (checksum_head != NULL)
    free(checksum_head);

  return tmp_checksum_f;
}

int slapt_get_pkg_source_changelog (const slapt_rc_config *global_config,
                                      const char *url, unsigned int *compressed)
{
  char *changelog_head  = NULL;
  char *filename        = NULL;
  char *local_head      = NULL;
  char *location_gz     = SLAPT_CHANGELOG_FILE_GZ;
  char *location_uncomp = SLAPT_CHANGELOG_FILE;
  char *location        = location_gz;
  int success = 0,failure = -1;
  SLAPT_BOOL_T is_interactive = slapt_is_interactive(global_config);
  *compressed = 0;

  changelog_head  = slapt_head_mirror_data(url,location);

  if (changelog_head == NULL) {
    location = location_uncomp;
    changelog_head  = slapt_head_mirror_data(url,location);
  } else {
    *compressed = 1;
  }

  if (changelog_head == NULL) {
    if (is_interactive)
      printf(gettext("Done\n"));
    return success;
  }

  filename    = slapt_gen_filename_from_url(url,location);
  local_head  = slapt_read_head_cache(filename);

  if (local_head != NULL && strcmp(changelog_head,local_head) == 0) {

    if (is_interactive)
      printf(gettext("Cached\n"));
  
  } else {
    FILE *working_changelog_f = NULL;
    const char *err = NULL;

    if (global_config->dl_stats == SLAPT_TRUE)
      printf("\n");

    if ((working_changelog_f = slapt_open_file(filename,"w+b")) == NULL)
      exit(EXIT_FAILURE);

    err = slapt_get_mirror_data_from_source(working_changelog_f,
                                            global_config,url,
                                            location);
    if (!err) {
      if (global_config->dl_stats == SLAPT_TRUE)
        printf("\n");
      if (is_interactive)
        printf(gettext("Done\n"));

      /* if all is good, write it */
      if (changelog_head != NULL)
        slapt_write_head_cache(changelog_head,filename);

      fclose(working_changelog_f);

      if (strcmp(location,location_gz) == 0) {
        char *uncomp_filename = slapt_gen_filename_from_url(url, location_uncomp);
        FILE *uncomp_f = slapt_open_file(uncomp_filename,"w+b");
        free(uncomp_filename);

        slapt_gunzip_file(filename,uncomp_f);
        fclose(uncomp_f);
      }

    } else {
      fprintf(stderr,gettext("Failed to download: %s\n"),err);
      slapt_clear_head_cache(filename);
      free(filename);
      free(local_head);
      if (changelog_head != NULL)
        free(changelog_head);
      return failure;
    }

  }
  free(filename);
  free(local_head);

  if (changelog_head != NULL)
    free(changelog_head);

  return success;

}

void slapt_clean_description (char *description, const char *name)
{
  char *p = NULL;
  char *token = NULL;

  if (description == NULL || name == NULL)
    return;

  token = calloc(strlen(name) + 3, sizeof *token);
  token = strcat(token,name);
  token = strcat(token,":");

  while ( (p = strstr( description, token )) != NULL ) {
    memmove( p, p + strlen(token), strlen(p) - strlen(token) + 1);
  }

  free(token);
}

/* retrieve the packages changelog entry, if any.  Returns NULL otherwise */
char *slapt_get_pkg_changelog(const slapt_pkg_info_t *pkg)
{
  char *filename = slapt_gen_filename_from_url(pkg->mirror,SLAPT_CHANGELOG_FILE);
  FILE *working_changelog_f = NULL;
  struct stat stat_buf;
  char *pkg_data = NULL, *pkg_name = NULL, *changelog = NULL, *ptr = NULL;
  size_t pls = 1;
  int changelog_len = 0;

  if ((working_changelog_f = fopen(filename,"rb")) == NULL)
    return NULL;

  /* used with mmap */
  if (stat(filename,&stat_buf) == -1) {

    if (errno)
      perror(filename);

    fprintf(stderr,"stat failed: %s\n",filename);
    exit(EXIT_FAILURE);
  }

  /* don't mmap empty files */
  if ((int)stat_buf.st_size < 1) {
    free(filename);
    fclose(working_changelog_f);
    return NULL;
  }

  pls = (size_t)stat_buf.st_size;

  pkg_data = (char *)mmap(0, pls,
    PROT_READ|PROT_WRITE, MAP_PRIVATE, fileno(working_changelog_f), 0);

  if (pkg_data == (void *)-1) {
    if (errno)
      perror(filename);

    fprintf(stderr,"mmap failed: %s\n",filename);
    exit(EXIT_FAILURE);
  }

  fclose(working_changelog_f);

  /* add \0 for strlen to work */
  pkg_data[pls - 1] = '\0';

  /* search for the entry within the changelog */
  pkg_name = slapt_stringify_pkg(pkg);

  if ((ptr = strstr(pkg_data,pkg_name)) != NULL) {
    int finished_parsing = 0;

    ptr += strlen(pkg_name);
    if (ptr[0] == ':')
      ptr++;

    while (finished_parsing != 1) {
      char *newline = strchr(ptr,'\n');
      char *start_ptr = ptr, *tmp = NULL;
      int remaining_len = 0;

      if (ptr[0] != '\n' && isblank(ptr[0]) == 0)
        break;

      /* figure out how much to copy in */
      if (newline != NULL) {
        remaining_len = strlen(start_ptr) - strlen(newline);
        ptr = newline + 1; /* skip newline */
      } else {
        remaining_len = strlen(start_ptr);
        finished_parsing = 1;
      }

      tmp = realloc(changelog, sizeof *changelog * (changelog_len + remaining_len + 1));
      if (tmp != NULL) {
        changelog = tmp;

        if (changelog_len == 0)
          changelog[0] = '\0';

        changelog = strncat(changelog, start_ptr, remaining_len);
        changelog_len += remaining_len;
        changelog[changelog_len] = '\0';
      }

    }

  }
  free(pkg_name);

  /* munmap now that we are done */
  if (munmap(pkg_data,pls) == -1) {
    if (errno)
      perror(filename);

    fprintf(stderr,"munmap failed: %s\n",filename);
    exit(EXIT_FAILURE);
  }
  free(filename);

  return changelog;
}

char *slapt_stringify_pkg(const slapt_pkg_info_t *pkg)
{
  char *pkg_str = NULL;
  int pkg_str_len = 0;

  pkg_str_len = strlen(pkg->name) + strlen(pkg->version) + strlen(pkg->file_ext) + 2; /* for the - and \0 */
  
  pkg_str = slapt_malloc(sizeof *pkg_str * pkg_str_len);

  if (snprintf(pkg_str, pkg_str_len, "%s-%s%s",pkg->name,pkg->version,pkg->file_ext) < 1) {
    free(pkg_str);
    return NULL;
  }

  return pkg_str;
}

slapt_pkg_list_t *
  slapt_get_obsolete_pkgs ( const slapt_rc_config *global_config,
                            slapt_pkg_list_t *avail_pkgs,
                            slapt_pkg_list_t *installed_pkgs)
{
  unsigned int r;
  slapt_pkg_list_t *obsolete = slapt_init_pkg_list();
  slapt_pkg_list_t *to_install = slapt_init_pkg_list();
  slapt_pkg_list_t *to_remove = slapt_init_pkg_list();

  for (r = 0; r < installed_pkgs->pkg_count; ++r) {
    slapt_pkg_info_t *p = installed_pkgs->pkgs[r];

    /*
       * if we can't find the installed package in our available pkg list,
       * it must be obsolete
    */
    if (slapt_get_newest_pkg(avail_pkgs, p->name) == NULL) {
        slapt_pkg_list_t *deps;
        unsigned int c;

        /*
          any packages that require this package we are about to remove
          should be scheduled to remove as well
        */
        deps = slapt_is_required_by(global_config,avail_pkgs,installed_pkgs,to_install,to_remove,p);

        for (c = 0; c < deps->pkg_count; ++c ) {
          slapt_pkg_info_t *dep           = deps->pkgs[c];
          slapt_pkg_info_t *installed_dep = slapt_get_exact_pkg(installed_pkgs, dep->name, dep->version);

          /* if it is installed, we add it to the list */
          if (installed_dep != NULL) {
              slapt_add_pkg_to_pkg_list(obsolete, installed_dep);
          }
        }

        slapt_free_pkg_list(deps);

        slapt_add_pkg_to_pkg_list(obsolete, p);

    }

  }

  slapt_free_pkg_list(to_install);
  slapt_free_pkg_list(to_remove);
  return obsolete;
}

static int pkg_compare (const void *a, const void *b)
{
  int cmp = 0;

  slapt_pkg_info_t *pkg_a = *(slapt_pkg_info_t * const *)a;
  slapt_pkg_info_t *pkg_b = *(slapt_pkg_info_t * const *)b;

  cmp = strcmp(pkg_a->name, pkg_b->name);

  if (cmp == 0) {

    if ((cmp = strverscmp(pkg_a->version, pkg_b->version)) == 0) {

      return strcmp(pkg_a->location,pkg_b->location);

    } else {

      return cmp;

    }

  } else {

   return cmp;

  }

}

char *slapt_get_pkg_filelist(const slapt_pkg_info_t *pkg)
{
  FILE *pkg_f = NULL;
  char *pkg_log_dirname = NULL;
  char *pkg_f_name = NULL;
  struct stat stat_buf;
  char *pkg_data = NULL;
  char *pkg_name;
  int pkg_name_len;
  char *filelist_p = NULL;
  char *tmp_filelist = NULL;
  size_t tmp_len = 0;
  char *filelist = NULL;
  size_t filelist_len = 0;
  size_t pls = 1;

  /* this only handles installed packages at the moment */
  if (pkg->installed != SLAPT_TRUE)
    return filelist;

  pkg_log_dirname = slapt_gen_package_log_dir_name();

  pkg_name_len = strlen(pkg->name) + strlen(pkg->version) + 2; /* for the - and \0 */
  pkg_name = slapt_malloc(sizeof *pkg_name * pkg_name_len);
  if (snprintf(pkg_name, pkg_name_len, "%s-%s",pkg->name,pkg->version) < 1) {
    free(pkg_name);
    return NULL;
  }

  /* build the package filename including the package directory */
  pkg_f_name = slapt_malloc(
    sizeof *pkg_f_name * (strlen(pkg_log_dirname) + strlen(pkg_name) + 2)
  );
  pkg_f_name[0] = '\0';
  strncat(pkg_f_name,pkg_log_dirname,strlen(pkg_log_dirname));
  strncat(pkg_f_name,"/",1);
  strncat(pkg_f_name,pkg_name,strlen(pkg_name));

  free(pkg_log_dirname);

  /*
     open the package log file so that we can mmap it and parse out the
     file list.
  */
  pkg_f = slapt_open_file(pkg_f_name,"r");
  if (pkg_f == NULL)
    exit(EXIT_FAILURE);

  /* used with mmap */
  if (stat(pkg_f_name,&stat_buf) == -1) {

    if (errno)
        perror(pkg_f_name);

      fprintf(stderr,"stat failed: %s\n",pkg_f_name);
      exit(EXIT_FAILURE);
    }

    /* don't mmap empty files */
    if ((int)stat_buf.st_size < 1) {
      free(pkg_f_name);
      fclose(pkg_f);
      return "";
    }

    pls = (size_t)stat_buf.st_size;

    pkg_data = (char *)mmap(0,pls,
                            PROT_READ|PROT_WRITE,MAP_PRIVATE,fileno(pkg_f),0);
    if (pkg_data == (void *)-1) {

      if (errno)
        perror(pkg_f_name);

      fprintf(stderr,"mmap failed: %s\n",pkg_f_name);
      exit(EXIT_FAILURE);
    }

    fclose(pkg_f);

    /* add \0 for strlen to work */
    pkg_data[pls - 1] = '\0';

    filelist_p = strstr(pkg_data,"FILE LIST");
    if (filelist_p != NULL) {
      char *nl = strchr(filelist_p,'\n');
      int finished_parsing = 0;

      if (nl != NULL)
        filelist_p = ++nl;

       while (!finished_parsing) {
       
         if ((nl = strchr(filelist_p,'\n')) != NULL) {
            tmp_len = nl - filelist_p + 1;
         } else {
            tmp_len = strlen(filelist_p);
            finished_parsing = 1;
         }

          /* files in install/ are package metadata */
          if (strncmp(filelist_p, "./\n", 3) != 0 &&
              strncmp(filelist_p, "install/", 8) != 0) { 
           
          tmp_len += 1; /* prefix '/' */

          tmp_filelist = realloc(filelist,
            sizeof *filelist *
            (filelist_len + tmp_len + 1)
          );
     
             if (tmp_filelist != NULL) {
          filelist = tmp_filelist;
          tmp_filelist = filelist + filelist_len;
          tmp_filelist[0] = '/';
          strncpy(tmp_filelist + 1,filelist_p,tmp_len - 1);
          tmp_filelist[tmp_len] = '\0';
          filelist_len += tmp_len;
          }

          }

          filelist_p = nl + 1;
       }
    }

  /* munmap now that we are done */
  if (munmap(pkg_data,pls) == -1) {
    if (errno)
      perror(pkg_f_name);

    fprintf(stderr,"munmap failed: %s\n",pkg_f_name);
    exit(EXIT_FAILURE);
  }
  free(pkg_f_name);
  free(pkg_name);

  return filelist;
}

char *slapt_gen_package_log_dir_name(void)
{
  char *root_env_entry = NULL;
  char *pkg_log_dirname = NULL;

  /* Generate package log directory using ROOT env variable if set */
  if (getenv(SLAPT_ROOT_ENV_NAME) &&
      strlen(getenv(SLAPT_ROOT_ENV_NAME)) < SLAPT_ROOT_ENV_LEN) {
    root_env_entry = getenv(SLAPT_ROOT_ENV_NAME);
  }
  pkg_log_dirname = slapt_calloc(
    strlen(SLAPT_PKG_LOG_DIR)+
    (root_env_entry ? strlen(root_env_entry) : 0) + 1 ,
    sizeof *pkg_log_dirname
  );
  *pkg_log_dirname = '\0';
  if (root_env_entry) {
    strncpy(pkg_log_dirname, root_env_entry,strlen(root_env_entry));
    strncat(pkg_log_dirname, SLAPT_PKG_LOG_DIR,strlen(SLAPT_PKG_LOG_DIR));
  } else {
    strncat(pkg_log_dirname, SLAPT_PKG_LOG_DIR,strlen(SLAPT_PKG_LOG_DIR));
  }

  return pkg_log_dirname;
}

