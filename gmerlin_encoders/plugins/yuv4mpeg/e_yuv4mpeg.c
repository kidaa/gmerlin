/*****************************************************************
 * gmerlin-encoders - encoder plugins for gmerlin
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

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> // STDOUT_FILENO

#include <config.h>


#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/pluginfuncs.h>
#include <gmerlin/utils.h>
#include <gmerlin_encoders.h>
#include <yuv4mpeg.h>
#include "y4m_common.h"

typedef struct
  {
  bg_y4m_common_t com;
  char * filename;
  bg_encoder_callbacks_t * cb;
  
  } e_y4m_t;

static void * create_y4m()
  {
  e_y4m_t * ret = calloc(1, sizeof(*ret));
  return ret;
  }

static void set_callbacks_y4m(void * data, bg_encoder_callbacks_t * cb)
  {
  e_y4m_t * y4m = data;
  y4m->cb = cb;
  }


static int open_y4m(void * data, const char * filename,
                    const gavl_metadata_t * metadata,
                    const gavl_chapter_list_t * chapter_list)
  {
  e_y4m_t * e = data;

  if(!strcmp(filename, "-"))
    {
    e->com.fd = STDOUT_FILENO;
    }
  else
    {
    /* Copy filename for later reusal */
    e->filename = bg_filename_ensure_extension(filename, "y4m");

    if(!bg_encoder_cb_create_output_file(e->cb, e->filename))
      return 0;
  
    e->com.fd = open(e->filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if(e->com.fd == -1)
      return 0;
    }
  
  return 1;
  }

static int add_video_stream_y4m(void * data,
                                const gavl_metadata_t * m,
                                const gavl_video_format_t* format)
  {
  e_y4m_t * e = data;
  gavl_video_format_copy(&e->com.format, format);
  return 0;
  }

static gavl_video_sink_t *
get_video_sink_y4m(void * data, int stream)
  {
  e_y4m_t * e = data;
  return e->com.sink;
  }

static int start_y4m(void * data)
  {
  int result;
  e_y4m_t * e = data;
  
  bg_encoder_set_framerate(&e->com.fr,
                           &e->com.format);
  
  result = bg_y4m_write_header(&e->com);
  return result;
  }

static int close_y4m(void * data, int do_delete)
  {
  e_y4m_t * e = data;

  if(e->com.fd != STDOUT_FILENO)
    close(e->com.fd);
  if(do_delete)
    remove(e->filename);
  return 1;
  }

static void destroy_y4m(void * data)
  {
  e_y4m_t * e = data;
  bg_y4m_cleanup(&e->com);

  if(e->filename)
    free(e->filename);
  
  free(e);
  }

/* Per stream parameters */

static const bg_parameter_info_t video_parameters[] =
  {
    {
      .name =        "chroma_mode",
      .long_name =   TRS("Chroma mode"),
      .type =        BG_PARAMETER_STRINGLIST,
      .val_default = { .val_str = "auto" },
      .multi_names = (char const *[]){ "auto",
                              "420jpeg",
                              "420mpeg2",
                              "420paldv",
                              "444",
                              "422",
                              "411",
                              "mono",
                              "yuva4444",
                              NULL },
      .multi_labels = (char const *[]){ TRS("Auto"),
                               TRS("4:2:0 (MPEG-1/JPEG)"),
                               TRS("4:2:0 (MPEG-2)"),
                               TRS("4:2:0 (PAL DV)"),
                               TRS("4:4:4"),
                               TRS("4:2:2"),
                               TRS("4:1:1"),
                               TRS("Greyscale"),
                               TRS("4:4:4:4 (YUVA)"),
                               NULL },
      .help_string = TRS("Set the chroma mode of the output file. Auto means to take the format most similar to the source.")
    },
    BG_ENCODER_FRAMERATE_PARAMS,
    { /* End of parameters */ }
  };

static const bg_parameter_info_t * get_video_parameters_y4m(void * data)
  {
  return video_parameters;
  }

#define SET_ENUM(str, dst, v) if(!strcmp(val->val_str, str)) dst = v

static void set_video_parameter_y4m(void * data, int stream, const char * name,
                                    const bg_parameter_value_t * val)
  {
  int sub_h, sub_v;
  e_y4m_t * e = data;
  if(!name)
    {
    /* Detect chroma mode from input format */
    if(e->com.chroma_mode == -1)
      {
      if(gavl_pixelformat_has_alpha(e->com.format.pixelformat))
        {
        e->com.chroma_mode = Y4M_CHROMA_444ALPHA;
        }
      else
        {
        gavl_pixelformat_chroma_sub(e->com.format.pixelformat, &sub_h, &sub_v);
        /* 4:2:2 */
        if((sub_h == 2) && (sub_v == 1))
          e->com.chroma_mode = Y4M_CHROMA_422;
        
        /* 4:1:1 */
        else if((sub_h == 4) && (sub_v == 1))
          e->com.chroma_mode = Y4M_CHROMA_411;
        
        /* 4:2:0 */
        else if((sub_h == 2) && (sub_v == 2))
          {
          switch(e->com.format.chroma_placement)
            {
            case GAVL_CHROMA_PLACEMENT_DEFAULT:
              e->com.chroma_mode = Y4M_CHROMA_420JPEG;
              break;
            case GAVL_CHROMA_PLACEMENT_MPEG2:
              e->com.chroma_mode = Y4M_CHROMA_420MPEG2;
              break;
            case GAVL_CHROMA_PLACEMENT_DVPAL:
              e->com.chroma_mode = Y4M_CHROMA_420PALDV;
              break;
            }
          }
        else
          e->com.chroma_mode = Y4M_CHROMA_444;
        }
      }
    bg_y4m_set_pixelformat(&e->com);
    return;
    }
  else if(bg_encoder_set_framerate_parameter(&e->com.fr,
                                             name,
                                             val))
    {
    return;
    }
  else if(!strcmp(name, "chroma_mode"))
    {
    SET_ENUM("auto",     e->com.chroma_mode, -1);
    SET_ENUM("420jpeg",  e->com.chroma_mode, Y4M_CHROMA_420JPEG);
    SET_ENUM("420mpeg2", e->com.chroma_mode, Y4M_CHROMA_420MPEG2);
    SET_ENUM("420paldv", e->com.chroma_mode, Y4M_CHROMA_420PALDV);
    SET_ENUM("444",      e->com.chroma_mode, Y4M_CHROMA_444);
    SET_ENUM("422",      e->com.chroma_mode, Y4M_CHROMA_422);
    SET_ENUM("411",      e->com.chroma_mode, Y4M_CHROMA_411);
    SET_ENUM("mono",     e->com.chroma_mode, Y4M_CHROMA_MONO);
    SET_ENUM("yuva4444", e->com.chroma_mode, Y4M_CHROMA_444ALPHA);
    }
    
  }

#undef SET_ENUM

const bg_encoder_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =           "e_y4m",       /* Unique short name */
      .long_name =      TRS("yuv4mpeg2 encoder"),
      .description =     TRS("Encoder for yuv4mpeg files.\
 Based on mjpegtools (http://mjpeg.sourceforge.net)."),
      .type =           BG_PLUGIN_ENCODER_VIDEO,
      .flags =          BG_PLUGIN_FILE | BG_PLUGIN_PIPE,
      .priority =       BG_PLUGIN_PRIORITY_MAX,
      .create =         create_y4m,
      .destroy =        destroy_y4m,
    },

    .max_audio_streams =  0,
    .max_video_streams =  1,

    .get_video_parameters = get_video_parameters_y4m,

    .set_callbacks =        set_callbacks_y4m,

    .open =                 open_y4m,

    .add_video_stream =     add_video_stream_y4m,

    .set_video_parameter =  set_video_parameter_y4m,

    .get_video_sink =     get_video_sink_y4m,

    .start =                start_y4m,

    .close =             close_y4m,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
