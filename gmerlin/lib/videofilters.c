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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <gmerlin/translation.h>

#include <gmerlin/pluginregistry.h>
#include <gmerlin/converters.h>


#include <gmerlin/utils.h>

#include <gmerlin/filters.h>

#include <gmerlin/log.h>
#define LOG_DOMAIN "videofilters"


/* Video */

typedef struct
  {
  bg_plugin_handle_t * handle;
  bg_fv_plugin_t     * plugin;
  gavl_video_source_t * out_src;
  } video_filter_t;

struct bg_video_filter_chain_s
  {
  gavl_video_source_t * out_src;
  
  int num_filters;
  video_filter_t * filters;
  const bg_gavl_video_options_t * opt;
  bg_plugin_registry_t * plugin_reg;
  
  bg_parameter_info_t * parameters;

  char * filter_string;
  
  int need_rebuild;
  int need_restart;

  bg_video_converter_t * cnv_out;
  gavl_video_frame_t   * frame;
  gavl_video_format_t  out_format_1;
  gavl_video_format_t  in_format_1;
  gavl_video_format_t  in_format_2;

  gavl_video_format_t  out_format; /* Final output format */
  gavl_video_format_t  in_format;  /* Input format of first filter */
  
  bg_read_video_func_t in_func;
  void * in_data;
  int in_stream;

  bg_read_video_func_t read_func;
  void * read_data;
  int read_stream;
  
  pthread_mutex_t mutex;
  };

int bg_video_filter_chain_need_restart(bg_video_filter_chain_t * ch)
  {
  gavl_video_format_t test_format;

  if(!ch->need_restart)
    {
    gavl_video_format_copy(&test_format, &ch->in_format_1);
    bg_gavl_video_options_set_format(ch->opt, &ch->in_format_1, &test_format);
    if(!gavl_video_formats_equal(&test_format, &ch->in_format_2))
      ch->need_restart = 1;
    }
  
  return ch->need_restart || ch->need_rebuild;
  }

static int
video_filter_create(video_filter_t * f,
                    bg_video_filter_chain_t * ch,
                    const char * name)
  {
  const bg_plugin_info_t * info;
  info = bg_plugin_find_by_name(ch->plugin_reg, name);
  if(!info)
    return 0;
  f->handle = bg_plugin_load(ch->plugin_reg, info);
  if(!f->handle)
    return 0;

  f->plugin = (bg_fv_plugin_t*)(f->handle->plugin);
  return 1;
  }

static void video_filter_destroy(video_filter_t * f)
  {
  if(f->handle)
    bg_plugin_unref_nolock(f->handle);
  }

static void destroy_video_chain(bg_video_filter_chain_t * ch)
  {
  int i;
  
  /* Destroy previous filters */
  for(i = 0; i < ch->num_filters; i++)
    video_filter_destroy(&ch->filters[i]);
  if(ch->filters)
    {
    free(ch->filters);
    ch->filters = NULL;
    }
  ch->num_filters = 0;
  }

static void bg_video_filter_chain_rebuild(bg_video_filter_chain_t * ch)
  {
  int i;
  char ** filter_names;
  filter_names = bg_strbreak(ch->filter_string, ',');
  
  destroy_video_chain(ch);
  
  if(!filter_names)
    return;
  
  while(filter_names[ch->num_filters])
    ch->num_filters++;

  ch->filters = calloc(ch->num_filters, sizeof(*ch->filters));
  for(i = 0; i < ch->num_filters; i++)
    {
    video_filter_create(&ch->filters[i], ch,
                        filter_names[i]);
    }
  bg_strbreak_free(filter_names);
  ch->need_rebuild = 0;
  }

bg_video_filter_chain_t *
bg_video_filter_chain_create(const bg_gavl_video_options_t * opt,
                             bg_plugin_registry_t * plugin_reg)
  {
  bg_video_filter_chain_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->opt = opt;
  ret->plugin_reg = plugin_reg;

  ret->cnv_out = bg_video_converter_create(opt->opt);

  pthread_mutex_init(&ret->mutex, NULL);
  return ret;
  }

static void create_video_parameters(bg_video_filter_chain_t * ch)
  {
  ch->parameters = calloc(2, sizeof(*ch->parameters));
  ch->parameters->name      = bg_strdup(NULL, "video_filters");
  ch->parameters->long_name = bg_strdup(NULL, TRS("Video Filters"));
  ch->parameters->preset_path = bg_strdup(NULL, "videofilters");
  ch->parameters->gettext_domain = bg_strdup(NULL, PACKAGE);
  ch->parameters->gettext_directory = bg_strdup(NULL, LOCALE_DIR);
  ch->parameters->type = BG_PARAMETER_MULTI_CHAIN;
  ch->parameters->flags |= BG_PARAMETER_SYNC;
  bg_plugin_registry_set_parameter_info(ch->plugin_reg,
                                        BG_PLUGIN_FILTER_VIDEO,
                                        BG_PLUGIN_FILTER_1,
                                        ch->parameters);
  }


const bg_parameter_info_t *
bg_video_filter_chain_get_parameters(bg_video_filter_chain_t * ch)
  {
  if(!ch->parameters)
    create_video_parameters(ch);
  return ch->parameters;
  }

void
bg_video_filter_chain_set_parameter(void * data, const char * name,
                                    const bg_parameter_value_t * val)
  {
  int i;
  char * pos;
  video_filter_t * f;
  
  bg_video_filter_chain_t * ch;
  ch = (bg_video_filter_chain_t *)data;
    
  if(!name)
    {
    for(i = 0; i < ch->num_filters; i++)
      {
      if(ch->filters[i].plugin->common.set_parameter)
        ch->filters[i].plugin->common.set_parameter(ch->filters[i].handle->priv,
                                             NULL, NULL);
      }
    return;
    }

  
  if(!strcmp(name, "video_filters"))
    {
    if(!ch->filter_string && !val->val_str)
      goto the_end;
    
    if(ch->filter_string && val->val_str &&
       !strcmp(ch->filter_string, val->val_str))
      {
      goto the_end;
      }
    /* Rebuild chain */
    ch->filter_string = bg_strdup(ch->filter_string, val->val_str);
    ch->need_rebuild = 1;
    }
  else if(!strncmp(name, "video_filters.", 14))
    {
    if(ch->need_rebuild)
      bg_video_filter_chain_rebuild(ch);
    
    pos = strchr(name, '.');
    pos++;
    i = atoi(pos);
    f = ch->filters + i;

    pos = strchr(pos, '.');
    if(!pos)
      goto the_end;
    pos++;
    if(f->plugin->common.set_parameter)
      {
      f->plugin->common.set_parameter(f->handle->priv, pos, val);
      if(f->plugin->need_restart &&
         f->plugin->need_restart(f->handle->priv))
        ch->need_restart = 1;
      }
    }
  the_end:
  return;
  }

int bg_video_filter_chain_init(bg_video_filter_chain_t * ch,
                               const gavl_video_format_t * in_format,
                               gavl_video_format_t * out_format)
  {
#if 0
  int i;
  gavl_video_options_t * opt;
  gavl_video_format_t format_1;
  gavl_video_format_t format_2;
  video_filter_t * f;
  int do_convert;
  ch->need_restart = 0;

  if(ch->need_rebuild)
    bg_video_filter_chain_rebuild(ch);
  
  gavl_video_format_copy(&format_1, in_format);
  
  f = ch->filters;
  
  /* Set user defined format */
  bg_gavl_video_options_set_format(ch->opt,
                                   in_format,
                                   &format_1);

  gavl_video_format_copy(&ch->in_format_1, in_format);
  gavl_video_format_copy(&ch->in_format_2, &format_1);

  ch->read_func   = ch->in_func;
  ch->read_data   = ch->in_data;
  ch->read_stream = ch->in_stream;
  
  for(i = 0; i < ch->num_filters; i++)
    {
    gavl_video_format_copy(&format_2, &format_1);

    if(f->plugin->get_options)
      {
      opt = f->plugin->get_options(f->handle->priv);
      gavl_video_options_copy(opt, ch->opt->opt);
      }
    
    f->plugin->set_input_format(f->handle->priv, &format_2, 0);
    
    if(!i)
      {
      do_convert =
        bg_video_converter_init(f->cnv, in_format, &format_2);
      gavl_video_format_copy(&ch->in_format, &format_2);
      }
    else
      do_convert =
        bg_video_converter_init(f->cnv, &format_1, &format_2);
    
    if(do_convert)
      {
      bg_video_converter_connect_input(f->cnv,
                                       ch->read_func,
                                       ch->read_data,
                                       ch->read_stream);
      
      f->plugin->connect_input_port(f->handle->priv,
                                    bg_video_converter_read,
                                    f->cnv, 0, 0);
      }
    else
      f->plugin->connect_input_port(f->handle->priv,
                                    ch->read_func,
                                    ch->read_data,
                                    ch->read_stream, 0);
    
    ch->read_func   = f->plugin->read_video;
    ch->read_data   = f->handle->priv;
    ch->read_stream = 0;
    
    //    if(f->plugin->init)
    //      f->plugin->init(f->handle->priv);
    f->plugin->get_output_format(f->handle->priv, &format_1);

    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Initialized video filter %s",
           TRD(f->handle->info->long_name, f->handle->info->gettext_domain));
    f++;
    }
  gavl_video_format_copy(&ch->out_format, &format_1);
  gavl_video_format_copy(out_format, &format_1);
  if(ch->num_filters)
    gavl_video_format_copy(&ch->out_format_1, &format_1);
  else
    gavl_video_format_copy(&ch->out_format_1, in_format);
#endif
  return ch->num_filters;
  }

void bg_video_filter_chain_set_input_format(bg_video_filter_chain_t * ch,
                                            const gavl_video_format_t * format)
  {
#if 0
  int do_convert;
  if(!ch->num_filters)
    {
    
    do_convert = bg_video_converter_init(ch->cnv_out,
                                         &ch->out_format_1,
                                         &ch->out_format);
    if(do_convert)
      {
      bg_video_converter_connect_input(ch->cnv_out,
                                       ch->in_func,
                                       ch->in_data,
                                       ch->in_stream);
      ch->read_func   = bg_video_converter_read;
      ch->read_data   = ch->cnv_out;
      ch->read_stream = 0;
      }
    }
  else
    {
    video_filter_t * f = ch->filters;

    do_convert =
      bg_video_converter_init(f->cnv, format, &ch->in_format);

    if(do_convert)
      {
      bg_video_converter_connect_input(f->cnv,
                                       ch->in_func, ch->in_data, ch->in_stream);
      
      f->plugin->connect_input_port(f->handle->priv,
                                    bg_video_converter_read,
                                    f->cnv, 0, 0);
      }
    else
      f->plugin->connect_input_port(f->handle->priv, ch->in_func, ch->in_data,
                                    ch->in_stream, 0);
    }
#endif
  }

void bg_video_filter_chain_connect_input(bg_video_filter_chain_t * ch,
                                         bg_read_video_func_t func, void * priv,
                                         int stream)
  {
  ch->in_func = func;
  ch->in_data = priv;
  ch->in_stream = stream;
  }

int bg_video_filter_chain_read(void * priv, gavl_video_frame_t* frame,
                               int stream)
  {
  bg_video_filter_chain_t * ch;
  int ret;
  ch = (bg_video_filter_chain_t *)priv;

  bg_video_filter_chain_lock(ch);
  ret = ch->read_func(ch->read_data, frame, ch->read_stream);
  bg_video_filter_chain_unlock(ch);
  return ret;
  }

int bg_video_filter_chain_set_out_format(bg_video_filter_chain_t * ch,
                                         const gavl_video_format_t * out_format)
  {
  int do_convert;
  do_convert = bg_video_converter_init(ch->cnv_out,
                                       &ch->out_format_1,
                                       out_format);
  if(do_convert)
    {
    bg_video_converter_connect_input(ch->cnv_out,
                                     ch->read_func,
                                     ch->read_data,
                                     ch->read_stream);
    ch->read_func = bg_video_converter_read;
    ch->read_data = ch->cnv_out;
    ch->read_stream = 0;
    }
  gavl_video_format_copy(&ch->out_format, out_format);
  return do_convert;
  }


void bg_video_filter_chain_destroy(bg_video_filter_chain_t * ch)
  {
  if(ch->parameters)
    bg_parameter_info_destroy_array(ch->parameters);
  if(ch->filter_string)
    free(ch->filter_string);
  destroy_video_chain(ch);
  pthread_mutex_destroy(&ch->mutex);
  bg_video_converter_destroy(ch->cnv_out);

  free(ch);
  }

void bg_video_filter_chain_lock(bg_video_filter_chain_t * cnv)
  {
  pthread_mutex_lock(&cnv->mutex);
  }

void bg_video_filter_chain_unlock(bg_video_filter_chain_t * cnv)
  {
  pthread_mutex_unlock(&cnv->mutex);
  }

void bg_video_filter_chain_reset(bg_video_filter_chain_t * ch)
  {
  int i;
  for(i = 0; i < ch->num_filters; i++)
    {
    if(ch->filters[i].plugin->reset)
      ch->filters[i].plugin->reset(ch->filters[i].handle->priv);
    gavl_video_source_reset(ch->filters[i].out_src);
    }
  gavl_video_source_reset(ch->out_src);
  }

gavl_video_source_t *
bg_video_filter_chain_connect(bg_video_filter_chain_t * ch,
                              gavl_video_source_t * src)
  {
  int i;
  
  for(i = 0; i < ch->num_filters; i++)
    {
    src =
      ch->filters[i].plugin->connect(ch->filters[i].handle->priv,
                                     src, ch->opt->opt);
    ch->filters[i].out_src = src;
    }

  return src;
  }