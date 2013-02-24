/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
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

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <errno.h>

#include <gmerlin/utils.h>
#include <gmerlin/xmlutils.h>

#define BLOCK_SIZE 2048

#include <gmerlin/log.h>
#define LOG_DOMAIN "xmlutils"

typedef struct
  {
  int bytes_written;
  int bytes_allocated;
  char * buffer;
  } bg_xml_output_mem_t;

static int mem_write_callback(void * context, const char * buffer,
                              int len)
  {
  bg_xml_output_mem_t * o = context;

  if(o->bytes_allocated - o->bytes_written < len)
    {
    o->bytes_allocated += BLOCK_SIZE;
    while(o->bytes_allocated < o->bytes_written + len)
      o->bytes_allocated += BLOCK_SIZE;
    o->buffer = realloc(o->buffer, o->bytes_allocated);
    }
  memcpy(&o->buffer[o->bytes_written], buffer, len);
  o->bytes_written += len;
  return len;
  }

static int mem_close_callback(void * context)
  {
  bg_xml_output_mem_t * o = context;

  if(o->bytes_allocated == o->bytes_written)
    {
    o->bytes_allocated++;
    o->buffer = realloc(o->buffer, o->bytes_allocated);
    }
  o->buffer[o->bytes_written] = '\0';
  return 0;
  }

char * bg_xml_save_to_memory(xmlDocPtr doc)
  {
  xmlOutputBufferPtr b;
  bg_xml_output_mem_t ctx;
  memset(&ctx, 0, sizeof(ctx));
  
  b = xmlOutputBufferCreateIO (mem_write_callback,
                               mem_close_callback,
                               &ctx,
                               NULL);
  xmlSaveFileTo(b, doc, NULL);
  return ctx.buffer;
  }

static int FILE_write_callback(void * context, const char * buffer,
                               int len)
  {
  return fwrite(buffer, 1, len, context);
  }

static int FILE_read_callback(void * context, char * buffer,
                               int len)
  {
  return fread(buffer, 1, len, context);
  }

xmlDocPtr bg_xml_load_FILE(FILE * f)
  {
  return xmlReadIO(FILE_read_callback, NULL, f, NULL, NULL, 0);
  }

void bg_xml_save_FILE(xmlDocPtr doc, FILE * f)
  {
  xmlOutputBufferPtr b;

  b = xmlOutputBufferCreateIO (FILE_write_callback,
                               NULL, f, NULL);
  xmlSaveFileTo(b, doc, NULL);
  }

xmlDocPtr bg_xml_parse_file(const char * filename, int lock)
  {
  xmlDocPtr ret = NULL;
  FILE * f = fopen(filename, "r");
  if(!f)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot open %s: %s",
           filename, strerror(errno));
    return NULL;
    }

  if(lock)
    {
    if(!bg_lock_file(f, 0))
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot lock file %s: %s",
             filename, strerror(errno));
    }

  if(bg_file_size(f))
    ret = bg_xml_load_FILE(f);
  
  if(lock)
    {
    if(!bg_unlock_file(f))
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot unlock file %s: %s",
             filename, strerror(errno));
    }
  
  fclose(f);
  return ret;
  }

void bg_xml_save_file(xmlDocPtr doc, const char * filename, int lock)
  {
  FILE * f = fopen(filename, "w");
  if(!f)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot open %s: %s",
           filename, strerror(errno));
    return;
    }
  
  if(lock)
    {
    if(!bg_lock_file(f, 1))
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot lock file %s: %s",
             filename, strerror(errno));
    }
  
  bg_xml_save_FILE(doc, f);
  fflush(f);
  
  if(lock)
    {
    if(!bg_unlock_file(f))
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Cannot unlock file %s: %s",
             filename, strerror(errno));
    }
  fclose(f);
  }
