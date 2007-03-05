#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <translation.h>

#include <pluginregistry.h>
#include <converters.h>


#include <utils.h>

#include <filters.h>

#include <log.h>
#define LOG_DOMAIN "filters"

/* Audio */

typedef struct
  {
  bg_plugin_handle_t * handle;
  bg_fa_plugin_t     * plugin;
  bg_audio_converter_t * cnv;
  int do_convert;
  } audio_filter_t;

struct bg_audio_filter_chain_s
  {
  int num_filters;
  audio_filter_t * filters;
  const bg_gavl_audio_options_t * opt;
  bg_plugin_registry_t * plugin_reg;
  
  bg_parameter_info_t * parameters;
  
  char * filter_string;
  int need_rebuild;
  };

int bg_audio_filter_chain_need_rebuild(bg_audio_filter_chain_t * ch)
  {
  return ch->need_rebuild;
  }


static int audio_filter_create(audio_filter_t * f, bg_audio_filter_chain_t * ch,
                                const char * name)
  {
  const bg_plugin_info_t * info;
  info = bg_plugin_find_by_name(ch->plugin_reg, name);
  if(!info)
    return 0;
  f->handle = bg_plugin_load(ch->plugin_reg, info);
  if(!f->handle)
    return 0;

  f->plugin = (bg_fa_plugin_t*)(f->handle->plugin);
  f->cnv = bg_audio_converter_create(ch->opt->opt);
  return 1;
  }

void bg_audio_filter_chain_lock(bg_audio_filter_chain_t * cnv)
  {
  if(cnv->num_filters && cnv->filters[cnv->num_filters-1].handle)
    bg_plugin_lock(cnv->filters[cnv->num_filters-1].handle);
  }

void bg_audio_filter_chain_unlock(bg_audio_filter_chain_t * cnv)
  {
  if(cnv->num_filters && cnv->filters[cnv->num_filters-1].handle)
    bg_plugin_unlock(cnv->filters[cnv->num_filters-1].handle);
  }


static void audio_filter_destroy(audio_filter_t * f)
  {
  bg_audio_converter_destroy(f->cnv);
  if(f->handle)
    bg_plugin_unref_nolock(f->handle);
  }

static void destroy_audio_chain(bg_audio_filter_chain_t * ch)
  {
  int i;
  
  /* Destroy previous filters */
  for(i = 0; i < ch->num_filters; i++)
    audio_filter_destroy(&ch->filters[i]);
  if(ch->filters)
    {
    free(ch->filters);
    ch->filters = (audio_filter_t*)0;
    }
  ch->num_filters = 0;
  }

static void build_audio_chain(bg_audio_filter_chain_t * ch)
  {
  int i;
  char ** filter_names;

  ch->need_rebuild = 0;
  filter_names = bg_strbreak(ch->filter_string, ',');
  
  destroy_audio_chain(ch);
  
  if(!filter_names)
    return;
  
  while(filter_names[ch->num_filters])
    ch->num_filters++;

  ch->filters = calloc(ch->num_filters, sizeof(*ch->filters));
  for(i = 0; i < ch->num_filters; i++)
    {
    audio_filter_create(&ch->filters[i], ch,
                        filter_names[i]);
    }
  bg_strbreak_free(filter_names);
  }

bg_audio_filter_chain_t *
bg_audio_filter_chain_create(const bg_gavl_audio_options_t * opt,
                             bg_plugin_registry_t * plugin_reg)
  {
  bg_audio_filter_chain_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->opt = opt;
  ret->plugin_reg = plugin_reg;


  return ret;
  }

static void create_audio_parameters(bg_audio_filter_chain_t * ch)
  {
  ch->parameters = calloc(2, sizeof(*ch->parameters));
  ch->parameters->name      = bg_strdup(NULL, "audio_filters");
  ch->parameters->long_name = bg_strdup(NULL, TRS("Audio Filters"));
  ch->parameters->gettext_domain = bg_strdup(NULL, PACKAGE);
  ch->parameters->gettext_directory = bg_strdup(NULL, LOCALE_DIR);
  ch->parameters->type = BG_PARAMETER_MULTI_CHAIN;
  ch->parameters->flags |= BG_PARAMETER_SYNC;
  bg_plugin_registry_set_parameter_info(ch->plugin_reg,
                                        BG_PLUGIN_FILTER_AUDIO,
                                        BG_PLUGIN_FILTER_1,
                                        ch->parameters);
  }

bg_parameter_info_t *
bg_audio_filter_chain_get_parameters(bg_audio_filter_chain_t * ch)
  {
  if(!ch->parameters)
    create_audio_parameters(ch);
  return ch->parameters;
  }

void
bg_audio_filter_chain_set_parameter(void * data, char * name,
                                    bg_parameter_value_t * val)
  {
  bg_audio_filter_chain_t * ch;
  int i;
  char * pos;
  audio_filter_t * f;
  
  ch = (bg_audio_filter_chain_t *)data;
  
  if(!name)
    return;

  if(!strcmp(name, "audio_filters"))
    {
    if(!ch->filter_string && !val->val_str)
      return;
    
    if(ch->filter_string && val->val_str &&
       !strcmp(ch->filter_string, val->val_str))
      {
      return;
      }
    /* Rebuild chain */
    ch->filter_string = bg_strdup(ch->filter_string, val->val_str);
    ch->need_rebuild = 1;
    }
  else if(!strncmp(name, "audio_filters.", 14))
    {
    if(ch->need_rebuild)
      build_audio_chain(ch);

    pos = strchr(name, '.');
    pos++;
    i = atoi(pos);
    f = ch->filters + i;

    pos = strchr(pos, '.');
    if(!pos)
      return;
    pos++;
    if(f->plugin->common.set_parameter)
      f->plugin->common.set_parameter(f->handle->priv, pos, val);
    }
  }

int bg_audio_filter_chain_init(bg_audio_filter_chain_t * ch,
                               const gavl_audio_format_t * in_format,
                               gavl_audio_format_t * out_format)
  {
  int i;
  
  gavl_audio_format_t format_1;
  gavl_audio_format_t format_2;
  audio_filter_t * f;
  
  if(ch->need_rebuild)
    build_audio_chain(ch);
  
  if(!ch->num_filters)
    return 0;

  gavl_audio_format_copy(&format_1, in_format);
  
  f = ch->filters;
  
  /* Set user defined format */
  bg_gavl_audio_options_set_format(ch->opt,
                                   in_format,
                                   &format_1);

  if(ch->opt->force_float)
    format_1.sample_format = GAVL_SAMPLE_FLOAT;
  
  for(i = 0; i < ch->num_filters; i++)
    {
    gavl_audio_format_copy(&format_2, &format_1);
    
    if(!i && ch->opt->force_float)
      format_2.sample_format = GAVL_SAMPLE_FLOAT;
    
    f->plugin->set_input_format(f->handle->priv, &format_2, 0);
    
    if(!i)
      {
      f->do_convert =
        bg_audio_converter_init(f->cnv, in_format, &format_2);
      }
    else
      f->do_convert =
        bg_audio_converter_init(f->cnv, &format_1, &format_2);
    
    if(f->do_convert)
      {
      f->plugin->connect_input_port(f->handle->priv,
                                    bg_audio_converter_read,
                                    f->cnv, 0, 0);
      if(i)
        bg_audio_converter_connect_input(f->cnv,
                                         ch->filters[i-1].plugin->read_audio,
                                         ch->filters[i-1].handle->priv, 0);
      }
    else if(i)
      f->plugin->connect_input_port(f->handle->priv, ch->filters[i-1].plugin->read_audio,
                                    ch->filters[i-1].handle->priv, 0, 0);
    if(f->plugin->init)
      f->plugin->init(f->handle->priv);
    f->plugin->get_output_format(f->handle->priv, &format_1);

    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Initialized audio filter %s",
           TRD(f->handle->info->long_name,
               f->handle->info->gettext_domain));
    f++;
    }
  gavl_audio_format_copy(out_format, &format_1);
  return ch->num_filters;
  }

void bg_audio_filter_chain_connect_input(bg_audio_filter_chain_t * ch,
                                         bg_read_audio_func_t func,
                                         void * priv, int stream)
  {
  if(!ch->num_filters)
    return;

  if(ch->filters->do_convert)
    bg_audio_converter_connect_input(ch->filters->cnv,
                                     func, priv, stream);
  else
    ch->filters->plugin->connect_input_port(ch->filters->handle->priv,
                                            func, priv, stream, 0);
  }

int bg_audio_filter_chain_read(void * priv, gavl_audio_frame_t* frame,
                               int stream, int num_samples)
  {
  int ret;
  audio_filter_t * f;
  bg_audio_filter_chain_t * ch;
  ch = (bg_audio_filter_chain_t *)priv;

  bg_audio_filter_chain_lock(ch);
  f = ch->filters + ch->num_filters - 1;
  ret = f->plugin->read_audio(f->handle->priv, frame, 0, num_samples);
  bg_audio_filter_chain_unlock(ch);
  return ret;
  }

void bg_audio_filter_chain_destroy(bg_audio_filter_chain_t * ch)
  {
  if(ch->parameters)
    bg_parameter_info_destroy_array(ch->parameters);

  if(ch->filter_string)
    free(ch->filter_string);

  destroy_audio_chain(ch);
  
  free(ch);
  }

/* Video */

typedef struct
  {
  bg_plugin_handle_t * handle;
  bg_fv_plugin_t     * plugin;
  bg_video_converter_t * cnv;
  int do_convert;
  } video_filter_t;

struct bg_video_filter_chain_s
  {
  int num_filters;
  video_filter_t * filters;
  const bg_gavl_video_options_t * opt;
  bg_plugin_registry_t * plugin_reg;
  
  bg_parameter_info_t * parameters;

  char * filter_string;
  
  int need_rebuild;
  };

int bg_video_filter_chain_need_rebuild(bg_video_filter_chain_t * ch)
  {
  return ch->need_rebuild;
  }


static int
video_filter_create(video_filter_t * f, bg_video_filter_chain_t * ch,
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
  f->cnv = bg_video_converter_create(ch->opt->opt);
  return 1;
  }

static void video_filter_destroy(video_filter_t * f)
  {
  bg_video_converter_destroy(f->cnv);
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
    ch->filters = (video_filter_t*)0;
    }
  ch->num_filters = 0;
  }

static void build_video_chain(bg_video_filter_chain_t * ch)
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


  return ret;
  }

static void create_video_parameters(bg_video_filter_chain_t * ch)
  {
  ch->parameters = calloc(2, sizeof(*ch->parameters));
  ch->parameters->name      = bg_strdup(NULL, "video_filters");
  ch->parameters->long_name = bg_strdup(NULL, TRS("Video Filters"));
  ch->parameters->gettext_domain = bg_strdup(NULL, PACKAGE);
  ch->parameters->gettext_directory = bg_strdup(NULL, LOCALE_DIR);
  ch->parameters->type = BG_PARAMETER_MULTI_CHAIN;
  ch->parameters->flags |= BG_PARAMETER_SYNC;
  bg_plugin_registry_set_parameter_info(ch->plugin_reg,
                                        BG_PLUGIN_FILTER_VIDEO,
                                        BG_PLUGIN_FILTER_1,
                                        ch->parameters);
  }


bg_parameter_info_t * bg_video_filter_chain_get_parameters(bg_video_filter_chain_t * ch)
  {
  if(!ch->parameters)
    create_video_parameters(ch);
  return ch->parameters;
  }

void bg_video_filter_chain_set_parameter(void * data, char * name, bg_parameter_value_t * val)
  {
  int i;
  char * pos;
  video_filter_t * f;
  
  bg_video_filter_chain_t * ch;
  ch = (bg_video_filter_chain_t *)data;
  
  if(!name)
    {
    
    }
  else if(!strcmp(name, "video_filters"))
    {
    if(!ch->filter_string && !val->val_str)
      return;
    
    if(ch->filter_string && val->val_str &&
       !strcmp(ch->filter_string, val->val_str))
      {
      return;
      }
    /* Rebuild chain */
    ch->filter_string = bg_strdup(ch->filter_string, val->val_str);
    ch->need_rebuild = 1;
    }
  else if(!strncmp(name, "video_filters.", 14))
    {
    if(ch->need_rebuild)
      build_video_chain(ch);
    
    pos = strchr(name, '.');
    pos++;
    i = atoi(pos);
    f = ch->filters + i;

    pos = strchr(pos, '.');
    if(!pos)
      return;
    pos++;
    if(f->plugin->common.set_parameter)
      f->plugin->common.set_parameter(f->handle->priv, pos, val);
    }
  }

int bg_video_filter_chain_init(bg_video_filter_chain_t * ch,
                               const gavl_video_format_t * in_format,
                               gavl_video_format_t * out_format)
  {
  int i;
  
  gavl_video_format_t format_1;
  gavl_video_format_t format_2;
  video_filter_t * f;
  
  if(ch->need_rebuild)
    build_video_chain(ch);
  
  if(!ch->num_filters)
    return 0;

  gavl_video_format_copy(&format_1, in_format);
  
  f = ch->filters;
  
  /* Set user defined format */
  bg_gavl_video_options_set_format(ch->opt,
                                   in_format,
                                   &format_1);
  bg_gavl_video_options_set_rectangles(ch->opt,
                                       in_format,
                                       &format_1, 1);
  
  for(i = 0; i < ch->num_filters; i++)
    {
    gavl_video_format_copy(&format_2, &format_1);
    
    f->plugin->set_input_format(f->handle->priv, &format_2, 0);
    
    if(!i)
      {
      f->do_convert =
        bg_video_converter_init(f->cnv, in_format, &format_2);
      }
    else
      f->do_convert =
        bg_video_converter_init(f->cnv, &format_1, &format_2);
    
    if(f->do_convert)
      {
      f->plugin->connect_input_port(f->handle->priv,
                                    bg_video_converter_read, f->cnv, 0, 0);
      if(i)
        bg_video_converter_connect_input(f->cnv,
                                         ch->filters[i-1].plugin->read_video,
                                         ch->filters[i-1].handle->priv, 0);
      }
    else if(i)
      f->plugin->connect_input_port(f->handle->priv, ch->filters[i-1].plugin->read_video,
                                    ch->filters[i-1].handle->priv, 0, 0);
    if(f->plugin->init)
      f->plugin->init(f->handle->priv);
    f->plugin->get_output_format(f->handle->priv, &format_1);

    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Initialized video filter %s",
           TRD(f->handle->info->long_name, f->handle->info->gettext_domain));
    f++;
    }
  gavl_video_format_copy(out_format, &format_1);
  return ch->num_filters;
  }

void bg_video_filter_chain_connect_input(bg_video_filter_chain_t * ch,
                                      bg_read_video_func_t func, void * priv,
                                      int stream)
  {
  if(!ch->num_filters)
    return;

  if(ch->filters->do_convert)
    bg_video_converter_connect_input(ch->filters->cnv,
                                     func, priv, stream);
  else
    ch->filters->plugin->connect_input_port(ch->filters->handle->priv,
                                            func, priv, stream, 0);
  
  }

int bg_video_filter_chain_read(void * priv, gavl_video_frame_t* frame, int stream)
  {
  video_filter_t * f;
  bg_video_filter_chain_t * ch;
  int ret;
  ch = (bg_video_filter_chain_t *)priv;

  bg_video_filter_chain_lock(ch);
  f = ch->filters + ch->num_filters - 1;
  ret = f->plugin->read_video(f->handle->priv, frame, 0);
  bg_video_filter_chain_unlock(ch);
  return ret;
  }

void bg_video_filter_chain_destroy(bg_video_filter_chain_t * ch)
  {
  if(ch->parameters)
    bg_parameter_info_destroy_array(ch->parameters);
  if(ch->filter_string)
    free(ch->filter_string);
  destroy_video_chain(ch);
  free(ch);
  }

void bg_video_filter_chain_lock(bg_video_filter_chain_t * cnv)
  {
  if(cnv->num_filters && cnv->filters[cnv->num_filters-1].handle)
    bg_plugin_lock(cnv->filters[cnv->num_filters-1].handle);
  }

void bg_video_filter_chain_unlock(bg_video_filter_chain_t * cnv)
  {
  if(cnv->num_filters && cnv->filters[cnv->num_filters-1].handle)
    bg_plugin_unlock(cnv->filters[cnv->num_filters-1].handle);
  }
