#include <stdlib.h>
#include <renderer.h>

#include <gmerlin/translation.h>
#include <gmerlin/bggavl.h>

bg_nle_outstream_t *
bg_nle_outstream_create(bg_nle_track_type_t type)
  {
  bg_nle_outstream_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->type = type;
  ret->section = bg_cfg_section_create("");
  return ret;
  }

void bg_nle_outstream_destroy(bg_nle_outstream_t * s)
  {
  free(s);
  }

#define PARAM_NAME \
  { \
  .name = "name", \
  .long_name = "Name", \
  .type = BG_PARAMETER_STRING, \
  }

static const bg_parameter_info_t video_parameters[] =
  {
    PARAM_NAME,
    BG_GAVL_PARAM_PIXELFORMAT,
    BG_GAVL_PARAM_FRAMESIZE,
    { /* End */ },
  };

static const bg_parameter_info_t audio_parameters[] =
  {
    PARAM_NAME,
    BG_GAVL_PARAM_CHANNEL_SETUP,
    { /* End */ },
  };

const bg_parameter_info_t * bg_nle_outstream_get_parameters(bg_nle_outstream_t * t)
  {
  switch(t->type)
    {
    
    case BG_NLE_TRACK_AUDIO:
      return audio_parameters;
      break;
    case BG_NLE_TRACK_VIDEO:
      return video_parameters;
      break;
    default:
      return NULL;
    }
  }

void bg_nle_outstream_set_name(bg_nle_outstream_t * t, const char * name)
  {
  bg_cfg_section_set_parameter_string(t->section, "name", name);
  }

const char * bg_nle_outstream_get_name(bg_nle_outstream_t * t)
  { 
  const char * ret;
  bg_cfg_section_get_parameter_string(t->section, "name", &ret);
  return ret;
  }
