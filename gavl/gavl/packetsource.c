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

#include <stdlib.h>
#include <stdio.h>

#include <gavl/connectors.h>

#define FLAG_HAS_AFMT         (1<<0)
#define FLAG_HAS_VFMT         (1<<1)
#define FLAG_HAS_CI           (1<<2)

struct gavl_packet_source_s
  {
  gavl_audio_format_t audio_format;
  gavl_video_format_t video_format;
  gavl_compression_info_t ci;
  
  gavl_packet_t p;
  
  int src_flags;
  
  gavl_packet_source_func_t func;
  void * priv;
  int flags;
  };


gavl_packet_source_t *
gavl_packet_source_create(gavl_packet_source_func_t func,
                          void * priv, int src_flags,
                          const gavl_compression_info_t * ci,
                          const gavl_audio_format_t * afmt,
                          const gavl_video_format_t * vfmt)
  {
  gavl_packet_source_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->func = func;
  ret->priv = priv;
  ret->src_flags = src_flags;

  if(ci)
    {
    gavl_compression_info_copy(&ret->ci, ci);
    ret->flags |= FLAG_HAS_CI;
    }

  if(afmt)
    {
    gavl_audio_format_copy(&ret->audio_format, afmt);
    ret->flags |= FLAG_HAS_AFMT;
    }

  if(vfmt)
    {
    gavl_video_format_copy(&ret->video_format, vfmt);
    ret->flags |= FLAG_HAS_VFMT;
    }
  
  return ret;
  }
  
const gavl_compression_info_t *
gavl_packet_source_get_ci(gavl_packet_source_t * s)
  {
  if(s->flags & FLAG_HAS_CI)
    return &s->ci;
  else
    return NULL;
  }
 
const gavl_audio_format_t *
gavl_packet_source_get_audio_format(gavl_packet_source_t * s)
  {
  if(s->flags & FLAG_HAS_AFMT)
    return &s->audio_format;
  else
    return NULL;
  }


const gavl_video_format_t *
gavl_packet_source_get_video_format(gavl_packet_source_t * s)
  {
  if(s->flags & FLAG_HAS_VFMT)
    return &s->video_format;
  else
    return NULL;
  }
  
gavl_source_status_t
gavl_packet_source_read_packet(void*sp, gavl_packet_t ** p)
  {
  gavl_source_status_t st;
  gavl_packet_t *p_src;
  gavl_packet_t *p_dst;
  
  gavl_packet_source_t * s = sp;
  
  if(s->src_flags & GAVL_SOURCE_SRC_ALLOC)
    p_src = NULL;
  else
    p_src = &s->p;

  if(*p)
    p_dst = *p;
  else
    p_dst = &s->p;

  gavl_packet_reset(p_src);
  st = s->func(s->priv, &p_src);

  if(st != GAVL_SOURCE_OK)
    return st;

  if(p_src != p_dst)
    gavl_packet_copy(p_dst, p_src);
  
  return GAVL_SOURCE_SRC_ALLOC;
  }

void
gavl_packet_source_destroy(gavl_packet_source_t * s)
  {
  gavl_compression_info_free(&s->ci);
  gavl_packet_free(&s->p);
  free(s);
  }