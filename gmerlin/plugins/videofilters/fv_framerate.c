/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2010 Members of the Gmerlin project
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
#include <stdio.h>

#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>
#include <gmerlin/log.h>
#include <gmerlin/bggavl.h>

#define LOG_DOMAIN "fv_framerate"

typedef struct
  {
  int need_restart;

  bg_read_video_func_t read_func;
  void * read_data;
  int read_stream;
  
  gavl_video_format_t format;

  bg_gavl_video_options_t opt;
  
  } framerate_priv_t;

static void * create_framerate()
  {
  framerate_priv_t * ret;
  ret = calloc(1, sizeof(*ret));

  bg_gavl_video_options_init(&ret->opt);

  return ret;
  }

static void destroy_framerate(void * priv)
  {
  framerate_priv_t * vp = priv;
  bg_gavl_video_options_free(&vp->opt);
  free(vp);
  }

static const bg_parameter_info_t parameters[] =
  {
    BG_GAVL_PARAM_FRAMERATE,
    { /* End of parameters */ },
  };

static const bg_parameter_info_t * get_parameters_framerate(void * priv)
  {
  return parameters;
  }

static void
set_parameter_framerate(void * priv, const char * name,
                          const bg_parameter_value_t * val)
  {
  framerate_priv_t * vp;
  int framerate_mode;
  int timescale;
  int frame_duration;

  vp = priv;

  if(!name)
    return;

  framerate_mode = vp->opt.framerate_mode;
  timescale = vp->opt.timescale;
  frame_duration = vp->opt.frame_duration;
  
  bg_gavl_video_set_parameter(&vp->opt, name, val);

  if((framerate_mode != vp->opt.framerate_mode) ||
     (timescale != vp->opt.timescale) ||
     (frame_duration = vp->opt.frame_duration))
    vp->need_restart = 1;
  }

static void connect_input_port_framerate(void * priv,
                                    bg_read_video_func_t func,
                                    void * data, int stream, int port)
  {
  framerate_priv_t * vp = priv;

  if(!port)
    {
    vp->read_func = func;
    vp->read_data = data;
    vp->read_stream = stream;
    }
  }

static void set_input_format_framerate(void * priv,
                                         gavl_video_format_t * format,
                                         int port)
  {
  framerate_priv_t * vp = priv;
  
  if(!port)
    {
    gavl_video_format_copy(&vp->format, format);
    bg_gavl_video_options_set_framerate(&vp->opt,
                                        format,
                                        &vp->format);
    gavl_video_format_copy(format, &vp->format);
    vp->need_restart = 0;

    
    }
  }

static void get_output_format_framerate(void * priv, gavl_video_format_t * format)
  {
  framerate_priv_t * vp = priv;
  
  gavl_video_format_copy(format, &vp->format);
  }

static int need_restart_framerate(void * priv)
  {
  framerate_priv_t * vp = priv;
  return vp->need_restart;
  }

static int read_video_framerate(void * priv,
                                  gavl_video_frame_t * frame, int stream)
  {
  framerate_priv_t * vp = priv;
  return vp->read_func(vp->read_data, frame, vp->read_stream);
  }

const bg_fv_plugin_t the_plugin = 
  {
    .common =
    {
      BG_LOCALE,
      .name =      "fv_framerate",
      .long_name = TRS("Force framerate"),
      .description = TRS("Forces a framerate as input for the next filter. Its mainly used for testing."),
      .type =     BG_PLUGIN_FILTER_VIDEO,
      .flags =    BG_PLUGIN_FILTER_1,
      .create =   create_framerate,
      .destroy =   destroy_framerate,
      .get_parameters =   get_parameters_framerate,
      .set_parameter =    set_parameter_framerate,
      .priority =         1,
    },
    
    .connect_input_port = connect_input_port_framerate,
    
    .set_input_format = set_input_format_framerate,
    .get_output_format = get_output_format_framerate,

    .read_video = read_video_framerate,
    .need_restart = need_restart_framerate,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;