/*****************************************************************
 * gavl - a general purpose audio/video processing library
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <config.h>

#include <gavl/gavl.h>
#include <gavl/utils.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gavl_dprintf(const char * format, ...)
  {
  va_list argp; /* arg ptr */
  va_start( argp, format);
  vfprintf(stderr, format, argp);
  va_end(argp);
  }

void gavl_diprintf(int indent, const char * format, ...)
  {
  int i;
  va_list argp; /* arg ptr */
  for(i = 0; i < indent; i++)
    gavl_dprintf( " ");
  
  va_start(argp, format);
  vfprintf(stderr, format, argp);
  va_end(argp);
  }

void gavl_hexdumpi(const uint8_t * data, int len, int linebreak, int indent)
  {
  int i;
  int bytes_written = 0;
  int imax;
  
  while(bytes_written < len)
    {
    for(i = 0; i < indent; i++)
      fprintf(stderr, " ");
    imax = (bytes_written + linebreak > len) ? len - bytes_written : linebreak;
    for(i = 0; i < imax; i++)
      fprintf(stderr, "%02x ", data[bytes_written + i]);
    for(i = imax; i < linebreak; i++)
      fprintf(stderr, "   ");
    for(i = 0; i < imax; i++)
      {
      if((data[bytes_written + i] < 0x7f) && (data[bytes_written + i] >= 32))
        fprintf(stderr, "%c", data[bytes_written + i]);
      else
        fprintf(stderr, ".");
      }
    bytes_written += imax;
    fprintf(stderr, "\n");
    }
  }

void gavl_hexdump(const uint8_t * data, int len, int linebreak)
  {
  gavl_hexdumpi(data, len, linebreak, 0);
  }

char * gavl_strrep(char * old_string,
                   const char * new_string)
  {
  char * ret;
  int len;
  if(!new_string || (*new_string == '\0'))
    {
    if(old_string)
      free(old_string);
    return NULL;
    }

  if(old_string)
    {
    if(!strcmp(old_string, new_string))
      return old_string;
    else
      free(old_string);
    }

  len = ((strlen(new_string)+1 + 3) / 4) * 4 ;
  
  ret = malloc(len);
  strcpy(ret, new_string);
  return ret;
  }

char * gavl_strnrep(char * old_string,
                    const char * new_string_start,
                    const char * new_string_end)
  {
  char * ret;
  if(!new_string_start || (*new_string_start == '\0'))
    {
    if(old_string)
      free(old_string);
    return NULL;
    }

  if(old_string)
    {
    if(!strncmp(old_string, new_string_start,
                new_string_end - new_string_start))
      return old_string;
    else
      free(old_string);
    }
  ret = malloc(new_string_end - new_string_start + 1);
  strncpy(ret, new_string_start, new_string_end - new_string_start);
  ret[new_string_end - new_string_start] = '\0';
  return ret;
  }

char * gavl_strdup(const char * new_string)
  {
  return gavl_strrep(NULL, new_string);
  }

char * gavl_strndup(const char * new_string,
                    const char * new_string_end)
  {
  return gavl_strnrep(NULL, new_string, new_string_end);
  }

char * gavl_strncat(char * old, const char * start, const char * end)
  {
  int len, old_len;
  old_len = old ? strlen(old) : 0;
  
  len = (end) ? (end - start) : strlen(start);
  old = realloc(old, len + old_len + 1);
  strncpy(old + old_len, start, len);
  old[old_len + len] = '\0';
  return old;
  }

char * gavl_strcat(char * old, const char * tail)
  {
  return gavl_strncat(old, tail, NULL);
  }

char * gavl_escape_string(char * old_string, const char * escape_chars)
  {
  char escape_seq[3];
  char * new_string = NULL;
  int i, done;

  const char * start;
  const char * end;
  const char * pos;

  int escape_len = strlen(escape_chars);

  /* 1st round: Check if the string can be passed unchanged */

  done = 1;
  for(i = 0; i < escape_len; i++)
    {
    if(strchr(old_string, escape_chars[i]))
      {
      done = 0;
      break;
      }
    }
  if(done)
    return old_string;

  /* 2nd round: Escape characters */

  escape_seq[0] = '\\';
  escape_seq[2] = '\0';

  start = old_string;
  end = start;

  done = 0;

  while(1)
    {
    /* Copy unescaped stuff */
    while(!strchr(escape_chars, *end) && (*end != '\0'))
      end++;

    if(end - start)
      {
      new_string = gavl_strncat(new_string, start, end);
      start = end;
      }

    if(*end == '\0')
      {
      free(old_string);
      return new_string;
      }
    /* Escape stuff */

    while((pos = strchr(escape_chars, *start)))
      {
      escape_seq[1] = *pos;
      new_string = gavl_strcat(new_string, escape_seq);
      start++;
      }
    end = start;
    if(*end == '\0')
      {
      free(old_string);
      return new_string;
      }
    }
  return NULL; // Never get here
  }

int gavl_string_starts_with(const char * str, const char * start)
  {
  return !strncmp(str, start, strlen(start)) ? 1 : 0;
  }

int gavl_string_starts_with_i(const char * str, const char * start)
  {
  return !strncasecmp(str, start, strlen(start)) ? 1 : 0;
  }

int gavl_string_ends_with(const char * str, const char * end)
  {
  int slen = strlen(str);
  int elen = strlen(end);

  if(slen < elen)
    return 0;
  return !strcmp(str + slen - elen, end) ? 1 : 0;
  }

int gavl_string_ends_with_i(const char * str, const char * end)
  {
  int slen = strlen(str);
  int elen = strlen(end);

  if(slen < elen)
    return 0;
  return !strcasecmp(str + slen - elen, end) ? 1 : 0;
  }


/* TODO */
char *
gavl_sprintf(const char * format,...)
  {
  return NULL;
  }

