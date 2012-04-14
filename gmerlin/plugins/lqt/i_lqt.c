/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2011 Members of the Gmerlin project
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

#include <string.h>
#include <ctype.h>

#include <config.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/log.h>

#include <gavl/metatags.h>

#include "lqt_common.h"
#include "lqtgavl.h"

#define LOG_DOMAIN "i_lqt"

#define PARAM_AUDIO 1
#define PARAM_VIDEO 3

static const bg_parameter_info_t parameters[] = 
  {
    {
      .name =      "audio",
      .long_name = TRS("Audio"),
      .type =      BG_PARAMETER_SECTION,
    },
    {
      .name =      "audio_codecs",
      .opt =       "ac",
      .long_name = TRS("Audio Codecs"),
      .help_string = TRS("Sort and configure audio codecs"),
    },
    {
      .name =      "video",
      .long_name = TRS("Video"),
      .type =      BG_PARAMETER_SECTION,
    },
    {
      .name =      "video_codecs",
      .opt =       "vc",
      .long_name = TRS("Video Codecs"),
      .help_string = TRS("Sort and configure video codecs"),
    },
    { /* End of parameters */ }
  };

typedef struct
  {
  quicktime_t * file;
  bg_parameter_info_t * parameters;

  char * audio_codec_string;
  char * video_codec_string;

  bg_track_info_t track_info;

  struct
    {
    int quicktime_index;
    int64_t pts_offset;
    } * audio_streams;
  
  struct
    {
    int quicktime_index;
    unsigned char ** rows;
    int has_timecodes;
    int64_t pts_offset;
    } * video_streams;
  struct
    {
    int quicktime_index;
    int timescale;
    int64_t pts_offset;
    } * subtitle_streams;
  } i_lqt_t;

static void * create_lqt()
  {
  i_lqt_t * ret = calloc(1, sizeof(*ret));

  lqt_set_log_callback(bg_lqt_log, NULL);

  return ret;
  }

static void setup_chapters(i_lqt_t * e, int track)
  {
  int i, num;
  int64_t timestamp, duration;
  char * text = NULL;
  int text_alloc = 0;
  
  e->track_info.chapter_list = bg_chapter_list_create(0);
  e->track_info.chapter_list->timescale = lqt_text_time_scale(e->file, track);

  num = lqt_text_samples(e->file, track);
  
  for(i = 0; i < num; i++)
    {
    if(lqt_read_text(e->file, track, &text, &text_alloc, &timestamp, &duration))
      {
      bg_chapter_list_insert(e->track_info.chapter_list, i, timestamp, text);
      }
    else
      break;
    }
  if(text) free(text);
  }

static int open_lqt(void * data, const char * arg)
  {
  const char * tag;
  int i;
  char * filename;
  int num_audio_streams = 0;
  int num_video_streams = 0;
  int num_text_streams = 0;
  gavl_metadata_t * m;
  i_lqt_t * e = data;

  lqt_codec_info_t ** codec_info;

  char lang[4];
  lang[3] = '\0';
  
  /* We want to keep the thing const-clean */
  filename = bg_strdup(NULL, arg);
  e->file = quicktime_open(filename, 1, 0);
  free(filename);

  if(!e->file)
    return 0;

  bg_set_track_name_default(&e->track_info, arg);

  /* Set metadata */

  m = &e->track_info.metadata;
  
  tag = quicktime_get_name(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_TITLE, tag);

  tag = quicktime_get_copyright(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_COPYRIGHT, tag);

  tag = lqt_get_comment(e->file);
  if(!tag)
    tag = quicktime_get_info(e->file);

  if(tag)
    gavl_metadata_set(m, GAVL_META_COMMENT, tag);

  

  tag = lqt_get_track(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_TRACKNUMBER, tag);

  tag = lqt_get_artist(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_ARTIST, tag);

  tag = lqt_get_album(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_ALBUM, tag);

  tag = lqt_get_genre(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_GENRE, tag);

  tag = lqt_get_author(e->file);
  if(tag)
    gavl_metadata_set(m, GAVL_META_AUTHOR, tag);
  
  
  /* Query streams */

  num_audio_streams = quicktime_audio_tracks(e->file);
  num_video_streams = quicktime_video_tracks(e->file);
  num_text_streams  = lqt_text_tracks(e->file);

  e->track_info.duration = 0;
  e->track_info.flags = BG_TRACK_SEEKABLE | BG_TRACK_PAUSABLE;
  if(num_audio_streams)
    {
    e->audio_streams = calloc(num_audio_streams, sizeof(*e->audio_streams));
    e->track_info.audio_streams =
      calloc(num_audio_streams, sizeof(*e->track_info.audio_streams));
    
    for(i = 0; i < num_audio_streams; i++)
      {
      if(quicktime_supported_audio(e->file, i))
        {
        m = &e->track_info.audio_streams[e->track_info.num_audio_streams].m;
        
        e->audio_streams[e->track_info.num_audio_streams].quicktime_index = i;

        e->audio_streams[e->track_info.num_audio_streams].pts_offset =
          lqt_get_audio_pts_offset(e->file, i);
        
        codec_info = lqt_audio_codec_from_file(e->file, i);

        gavl_metadata_set(m, GAVL_META_FORMAT, codec_info[0]->long_name);

        lqt_destroy_codec_info(codec_info);

        lqt_get_audio_language(e->file, i, lang);
        gavl_metadata_set(m, GAVL_META_LANGUAGE, lang);
        

        e->track_info.num_audio_streams++;
        }
      }
    }

  if(num_video_streams)
    {
    e->video_streams = calloc(num_video_streams, sizeof(*e->video_streams));
    e->track_info.video_streams =
      calloc(num_video_streams, sizeof(*e->track_info.video_streams));
    
    
    for(i = 0; i < num_video_streams; i++)
      {
      if(quicktime_supported_video(e->file, i))
        {
        m = &e->track_info.video_streams[e->track_info.num_video_streams].m;
        
        e->video_streams[e->track_info.num_video_streams].quicktime_index = i;
        
        codec_info = lqt_video_codec_from_file(e->file, i);
        gavl_metadata_set(m, GAVL_META_FORMAT, codec_info[0]->long_name);
        lqt_destroy_codec_info(codec_info);
        
        e->video_streams[e->track_info.num_video_streams].rows =
          lqt_gavl_rows_create(e->file, i);
        e->video_streams[e->track_info.num_video_streams].pts_offset =
          lqt_get_video_pts_offset(e->file, i);
        e->track_info.num_video_streams++;
        }
      }
    }
  if(num_text_streams)
    {
    e->subtitle_streams = calloc(num_text_streams, sizeof(*e->subtitle_streams));
    e->track_info.subtitle_streams =
      calloc(num_text_streams, sizeof(*e->track_info.subtitle_streams));
    
    for(i = 0; i < num_text_streams; i++)
      {
      if(lqt_is_chapter_track(e->file, i))
        {
        if(e->track_info.chapter_list)
          {
          bg_log(BG_LOG_WARNING, LOG_DOMAIN,
                 "More than one chapter track found, using first one");
          }
        else
          setup_chapters(e, i);
        }
      else
        {
        m = &e->track_info.subtitle_streams[e->track_info.num_subtitle_streams].m;
        
        e->subtitle_streams[e->track_info.num_subtitle_streams].quicktime_index = i;
        e->subtitle_streams[e->track_info.num_subtitle_streams].timescale =
          lqt_text_time_scale(e->file, i);

        e->subtitle_streams[e->track_info.num_subtitle_streams].pts_offset =
          lqt_get_text_pts_offset(e->file, i);
        
        lqt_get_text_language(e->file, i, lang);
        gavl_metadata_set(m, GAVL_META_LANGUAGE, lang);
        
        e->track_info.subtitle_streams[e->track_info.num_subtitle_streams].is_text = 1;
        
        e->track_info.num_subtitle_streams++;
        }
      }

    
    }

  e->track_info.duration = lqt_gavl_duration(e->file);

  if(lqt_is_avi(e->file))
    gavl_metadata_set(&e->track_info.metadata, GAVL_META_FORMAT, "AVI (lqt)");
  else
    gavl_metadata_set(&e->track_info.metadata, GAVL_META_FORMAT, "Quicktime (lqt)");
  
  //  if(!e->track_info.num_audio_streams && !e->track_info.num_video_streams)
  //    return 0;
  return 1;
  }

static int get_num_tracks_lqt(void * data)
  {
  return 1;
  }

static bg_track_info_t * get_track_info_lqt(void * data, int track)
  {
  i_lqt_t * e = data;
  return &e->track_info;
  }

/* Read one audio frame (returns FALSE on EOF) */
static  
int read_audio_samples_lqt(void * data, gavl_audio_frame_t * f, int stream,
                          int num_samples)
  {
  i_lqt_t * e = data;
  
  lqt_gavl_decode_audio(e->file, e->audio_streams[stream].quicktime_index,
                        f, num_samples);
  if(f->valid_samples)
    f->timestamp += e->audio_streams[stream].pts_offset;
  return f->valid_samples;
  }


static int has_subtitle_lqt(void * data, int stream)
  {
  return 1;
  }

static int read_subtitle_text_lqt(void * priv,
                                  char ** text, int * text_alloc,
                                  int64_t * start_time,
                                  int64_t * duration, int stream)
  {
  i_lqt_t * e = priv;
  int ret =  lqt_read_text(e->file,
                           e->subtitle_streams[stream].quicktime_index,
                           text, text_alloc,
                           start_time, duration);
  if(ret)
    *start_time += e->subtitle_streams[stream].pts_offset;
  return ret;
  }

/* Read one video frame (returns FALSE on EOF) */
static
int read_video_frame_lqt(void * data, gavl_video_frame_t * f, int stream)
  {
  i_lqt_t * e = data;
  int ret = lqt_gavl_decode_video(e->file,
                                  e->video_streams[stream].quicktime_index,
                                  f, e->video_streams[stream].rows);
  if(ret)
    f->timestamp += e->video_streams[stream].pts_offset;
  return ret;
  }


static void close_lqt(void * data)
  {
  int i;
  i_lqt_t * e = data;
  
  if(e->file)
    {
    quicktime_close(e->file);
    e->file = NULL;
    }
  if(e->audio_streams)
    {
    free(e->audio_streams);
    e->audio_streams = NULL;
    }
  if(e->video_streams)
    {
    for(i = 0; i < e->track_info.num_video_streams; i++)
      {
      if(e->video_streams[i].rows)
        free(e->video_streams[i].rows);
      }
    free(e->video_streams);
    e->video_streams = NULL;
    }
  bg_track_info_free(&e->track_info);  
  }

static void seek_lqt(void * data, gavl_time_t * time, int scale)
  {
  i_lqt_t * e = data;
  lqt_gavl_seek_scaled(e->file, time, scale);
  }

static void destroy_lqt(void * data)
  {
  i_lqt_t * e = data;
  close_lqt(data);

  if(e->parameters)
    bg_parameter_info_destroy_array(e->parameters);

  if(e->audio_codec_string)
    free(e->audio_codec_string);

  if(e->video_codec_string)
    free(e->video_codec_string);
      
  
  free(e);
  }

static void create_parameters(i_lqt_t * e)
  {

  e->parameters = bg_parameter_info_copy_array(parameters);
    
  bg_lqt_create_codec_info(&e->parameters[PARAM_AUDIO],
                           1, 0, 0, 1);
  bg_lqt_create_codec_info(&e->parameters[PARAM_VIDEO],
                           0, 1, 0, 1);

  
  }

static const bg_parameter_info_t * get_parameters_lqt(void * data)
  {
  i_lqt_t * e = data;
  
  if(!e->parameters)
    create_parameters(e);
  
  return e->parameters;
  }

static void set_parameter_lqt(void * data, const char * name,
                              const bg_parameter_value_t * val)
  {
  i_lqt_t * e = data;
  char * pos;
  char * tmp_string;
  if(!name)
    return;

  
  if(!e->parameters)
    create_parameters(e);
#if 0
  if(bg_lqt_set_parameter(name, val, &e->parameters[PARAM_AUDIO]) ||
     bg_lqt_set_parameter(name, val, &e->parameters[PARAM_VIDEO]))
    return;
#endif
  if(!strcmp(name, "audio_codecs"))
    {
    e->audio_codec_string = bg_strdup(e->audio_codec_string, val->val_str);
    }
  else if(!strcmp(name, "video_codecs"))
    {
    e->video_codec_string = bg_strdup(e->video_codec_string, val->val_str);
    }
  else if(!strncmp(name, "audio_codecs.", 13))
    {
    tmp_string = bg_strdup(NULL, name+13);
    pos = strchr(tmp_string, '.');
    *pos = '\0';
    pos++;
    
    bg_lqt_set_audio_decoder_parameter(tmp_string, pos, val);
    free(tmp_string);
    
    }
  else if(!strncmp(name, "video_codecs.", 13))
    {
    tmp_string = bg_strdup(NULL, name+13);
    pos = strchr(tmp_string, '.');
    *pos = '\0';
    pos++;
    
    bg_lqt_set_video_decoder_parameter(tmp_string, pos, val);
    free(tmp_string);
    
    }
  }

static int get_audio_compression_info_lqt(void * data,
                                          int stream, gavl_compression_info_t * ci)
  {
  i_lqt_t * e = data;
  return lqt_gavl_get_audio_compression_info(e->file, stream, ci);
  }

static int get_video_compression_info_lqt(void * data,
                                          int stream, gavl_compression_info_t * ci)
  {
  i_lqt_t * e = data;
  return lqt_gavl_get_video_compression_info(e->file, stream, ci);
  }

static int read_audio_packet_lqt(void * data, int stream, gavl_packet_t * p)
  {
  int ret;
  i_lqt_t * e = data;
  ret = lqt_gavl_read_audio_packet(e->file, stream, p);
  if(ret)
    p->pts += e->audio_streams[stream].pts_offset;
  return ret;
  }

static int read_video_packet_lqt(void * data, int stream, gavl_packet_t * p)
  {
  int ret;
  i_lqt_t * e = data;
  ret = lqt_gavl_read_video_packet(e->file, stream, p);
  if(ret)
    p->pts += e->video_streams[stream].pts_offset;
  return ret;
  }

static int start_lqt(void * data)
  {
  int i;
  i_lqt_t * e = data;

  for(i = 0; i < e->track_info.num_audio_streams; i++)
    {
    lqt_gavl_get_audio_format(e->file,
                              e->audio_streams[i].quicktime_index,
                              &e->track_info.audio_streams[i].format);
    }
  for(i = 0; i < e->track_info.num_video_streams; i++)
    {
    lqt_gavl_get_video_format(e->file,
                              e->video_streams[i].quicktime_index,
                              &e->track_info.video_streams[i].format, 0);
    }
  return 1;
  }

char const * const extensions = "mov";

static const char * get_extensions(void * priv)
  {
  return extensions;
  }

const bg_input_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =            "i_lqt",       /* Unique short name */
      .long_name =       TRS("libquicktime input plugin"),
      .description =     TRS("Input plugin based on libquicktime"),
      .type =            BG_PLUGIN_INPUT,
      .flags =           BG_PLUGIN_FILE,
      .priority =        5,
      .create =          create_lqt,
      .destroy =         destroy_lqt,
      .get_parameters =  get_parameters_lqt,
      .set_parameter =   set_parameter_lqt,
    },

    .get_extensions =    get_extensions,
    .open =              open_lqt,
    .get_num_tracks =    get_num_tracks_lqt,
    .get_track_info =    get_track_info_lqt,
    //    .set_audio_stream =  set_audio_stream_lqt,
    //    .set_video_stream =  set_audio_stream_lqt,

    .get_audio_compression_info = get_audio_compression_info_lqt,
    .get_video_compression_info = get_video_compression_info_lqt,

    .start =             start_lqt,

    .read_audio = read_audio_samples_lqt,
    .read_video =   read_video_frame_lqt,

    .has_subtitle =       has_subtitle_lqt,
    .read_subtitle_text = read_subtitle_text_lqt,

    .read_audio_packet = read_audio_packet_lqt,
    .read_video_packet = read_video_packet_lqt,
    
    .seek =               seek_lqt,
    //    .stop =               stop_lqt,
    .close =              close_lqt
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
