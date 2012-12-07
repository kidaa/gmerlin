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

#include <stdlib.h>
#include <string.h>

#include <gmerlin/mediaconnector.h>

#include <gmerlin/translation.h>
#include <gmerlin/log.h>
#define LOG_DOMAIN "mediaconnector"


static bg_mediaconnector_stream_t *
append_stream(bg_mediaconnector_t * conn,
              const gavl_metadata_t * m, gavl_packet_source_t * psrc)
  {
  bg_mediaconnector_stream_t * ret;
  conn->streams = realloc(conn->streams,
                          (conn->num_streams+1) * sizeof(*conn->streams));
  ret = conn->streams + conn->num_streams;
  conn->num_streams++;
  memset(ret, 0, sizeof(*ret));
  if(m)
    gavl_metadata_copy(&ret->m, m);
  ret->psrc = psrc;
  return ret;
  }

void
bg_mediaconnector_add_audio_stream(bg_mediaconnector_t * conn,
                                   const gavl_metadata_t * m,
                                   gavl_audio_source_t * asrc,
                                   gavl_packet_source_t * psrc,
                                   bg_cfg_section_t * encode_section)
  {
  bg_mediaconnector_stream_t * ret = append_stream(conn, m, psrc);
  ret->type = GAVF_STREAM_AUDIO;
  ret->asrc = asrc;

  if(ret->psrc)
    ret->pconn = gavl_packet_connector_create(ret->psrc);
  else
    ret->aconn = gavl_audio_connector_create(ret->asrc);
  ret->encode_section = encode_section;
  }

void
bg_mediaconnector_add_video_stream(bg_mediaconnector_t * conn,
                                   const gavl_metadata_t * m,
                                   gavl_video_source_t * vsrc,
                                   gavl_packet_source_t * psrc,
                                   bg_cfg_section_t * encode_section)
  {
  bg_mediaconnector_stream_t * ret = append_stream(conn, m, psrc);
  ret->type = GAVF_STREAM_VIDEO;
  ret->vsrc = vsrc;

  if(ret->psrc)
    ret->pconn = gavl_packet_connector_create(ret->psrc);
  else
    ret->vconn = gavl_video_connector_create(ret->vsrc);
  ret->encode_section = encode_section;
  }

void
bg_mediaconnector_add_text_stream(bg_mediaconnector_t * conn,
                                  const gavl_metadata_t * m,
                                  gavl_packet_source_t * psrc,
                                  int timescale)
  {
  bg_mediaconnector_stream_t * ret = append_stream(conn, m, psrc);
  ret->type = GAVF_STREAM_TEXT;
  ret->timescale = timescale;
  ret->pconn = gavl_packet_connector_create(ret->psrc);
  }

void
bg_mediaconnector_free(bg_mediaconnector_t * conn)
  {
  int i;
  bg_mediaconnector_stream_t * s;
  if(conn->streams)
    {
    for(i = 0; i < conn->num_streams; i++)
      {
      s = conn->streams + i;
      if(s->aconn)
        gavl_audio_connector_destroy(s->aconn);
      if(s->vconn)
        gavl_video_connector_destroy(s->vconn);
      if(s->pconn)
        gavl_packet_connector_destroy(s->pconn);
      }
    
    free(conn->streams);
    }
  }

void
bg_mediaconnector_init(bg_mediaconnector_t * conn)
  {
  memset(conn, 0, sizeof(*conn));
  }

static void process_cb_audio(void * priv, gavl_audio_frame_t * frame)
  {
  bg_mediaconnector_stream_t * s = priv;
  s->time = gavl_time_unscale(s->timescale,
                              frame->timestamp + frame->valid_samples);
  }

static void process_cb_video(void * priv, gavl_video_frame_t * frame)
  {
  bg_mediaconnector_stream_t * s = priv;
  s->time = gavl_time_unscale(s->timescale,
                              frame->timestamp + frame->duration);
  }

static void process_cb_packet(void * priv, gavl_packet_t * p)
  {
  bg_mediaconnector_stream_t * s = priv;
  s->time = gavl_time_unscale(s->timescale,
                              p->pts + p->duration);
  }


void
bg_mediaconnector_start(bg_mediaconnector_t * conn)
  {
  int i;
  bg_mediaconnector_stream_t * s;
  for(i = 0; i < conn->num_streams; i++)
    {
    s = conn->streams + i;

    s->time = GAVL_TIME_UNDEFINED;

    if(s->pconn)
      {
      gavl_packet_connector_set_process_func(s->pconn,
                                             process_cb_packet,
                                             s);
      }
    
    switch(s->type)
      {
      case GAVF_STREAM_AUDIO:
        {
        const gavl_audio_format_t * afmt;
        if(s->aconn)
          {
          gavl_audio_connector_set_process_func(s->aconn,
                                                process_cb_audio,
                                                s);
          
          gavl_audio_connector_start(s->aconn);
          afmt = gavl_audio_connector_get_process_format(s->aconn);
          }
        else
          afmt = gavl_packet_source_get_audio_format(s->psrc);
        s->timescale = afmt->samplerate;
        }
        break;
      case GAVF_STREAM_VIDEO:
        {
        const gavl_video_format_t * vfmt;
        if(s->vconn)
          {
          gavl_video_connector_set_process_func(s->vconn,
                                                process_cb_video,
                                                s);
          gavl_video_connector_start(s->vconn);
          vfmt = gavl_video_connector_get_process_format(s->vconn);
          }
        else
          vfmt = gavl_packet_source_get_video_format(s->psrc);

        if(vfmt->framerate_mode == GAVL_FRAMERATE_STILL)
          s->flags |= BG_MEDIACONNECTOR_FLAG_DISCONT;
        
        s->timescale = vfmt->timescale;
        }
        break;
      case GAVF_STREAM_TEXT:
        {
        s->flags |= BG_MEDIACONNECTOR_FLAG_DISCONT;
        }
        break;
      }
    }
  }

static int process_stream(bg_mediaconnector_stream_t * s)
  {
  int ret;
  if(s->aconn)
    ret = gavl_audio_connector_process(s->aconn);
  else if(s->vconn)
    ret = gavl_video_connector_process(s->vconn);
  else if(s->pconn)
    ret = gavl_packet_connector_process(s->pconn);
  if(!ret)
    s->flags |= BG_MEDIACONNECTOR_FLAG_EOF;
  else
    s->counter++;
  
  return ret;
  }

int bg_mediaconnector_iteration(bg_mediaconnector_t * conn)
  {
  int i;
  gavl_time_t min_time;
  int min_index = -1;
  bg_mediaconnector_stream_t * s;
  int ret = 0;
  
  /* Process discontinuous streams */
  for(i = 0; i < conn->num_streams; i++)
    {
    s = conn->streams + i;

    if(s->flags & BG_MEDIACONNECTOR_FLAG_EOF)
      continue;
    
    if(s->flags & BG_MEDIACONNECTOR_FLAG_DISCONT)
      ret += process_stream(s);
    }

  /* Find stream with minimum timestamp */
  
  for(i = 0; i < conn->num_streams; i++)
    {
    s = conn->streams + i;
    
    if(s->flags & (BG_MEDIACONNECTOR_FLAG_EOF ||
                   BG_MEDIACONNECTOR_FLAG_DISCONT))
      continue;

    ret++;
    
    if(s->time == GAVL_TIME_UNDEFINED)
      {
      min_index = i;
      break;
      }

    if((min_index == -1) ||
       (s->time < min_time))
      {
      min_time = s->time;
      min_index = i;
      }
    }
  
  /* Process this stream */
  if(min_index < 0)
    return ret;

  s = conn->streams + min_index;

  if(!process_stream(s))
    {
    if(!s->counter)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN,
             "Couldn't get first frame/packet");
      return 0;
      }
    }
  ret++;
  return ret;
  
  }
