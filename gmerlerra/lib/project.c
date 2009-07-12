#include <string.h>

#include <gavl/gavl.h>

#include <gmerlin/cfg_registry.h>
#include <gmerlin/translation.h>
#include <gmerlin/utils.h>

#include <track.h>
#include <project.h>
#include <editops.h>

static void bg_nle_edit_callback_stub(bg_nle_project_t * p,
                                      bg_nle_edit_op_t op,
                                      void * op_data,
                                      void * user_data)
  {

  }

bg_nle_project_t * bg_nle_project_create(bg_plugin_registry_t * plugin_reg)
  {
  bg_nle_project_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->visible.end = GAVL_TIME_SCALE * 10;
  ret->selection.end = -1;
  ret->media_list = bg_nle_media_list_create(plugin_reg);
  ret->plugin_reg = plugin_reg;

  ret->audio_track_section =
    bg_cfg_section_create_from_parameters("",
                                          bg_nle_track_audio_parameters);
  ret->video_track_section =
    bg_cfg_section_create_from_parameters("",
                                          bg_nle_track_video_parameters);
  ret->audio_outstream_section =
    bg_cfg_section_create_from_parameters("",
                                          bg_nle_outstream_audio_parameters);
  ret->video_outstream_section =
    bg_cfg_section_create_from_parameters("",
                                          bg_nle_outstream_video_parameters);

  ret->edit_callback = bg_nle_edit_callback_stub;
  
  return ret;
  }

void bg_nle_project_destroy(bg_nle_project_t * p)
  {
  int i;

  /* Free tracks */
  for(i = 0; i < p->num_tracks; i++)
    bg_nle_track_destroy(p->tracks[i]);
  
  if(p->tracks)
    free(p->tracks);

  for(i = 0; i < p->num_outstreams; i++)
    bg_nle_outstream_destroy(p->outstreams[i]);
  
  if(p->outstreams)
    free(p->outstreams);

  
  free(p);
  }

void bg_nle_project_set_edit_callback(bg_nle_project_t * p,
                                      bg_nle_edit_callback callback,
                                      void * callback_data)
  {
  p->edit_callback = callback;
  p->edit_callback_data = callback_data;
  }

void bg_nle_project_append_track(bg_nle_project_t * p,
                                     bg_nle_track_t * t)
  {
  bg_nle_project_insert_track(p, t, p->num_tracks);
  }

void bg_nle_project_insert_track(bg_nle_project_t * p,
                                     bg_nle_track_t * t, int pos)
  {
  if(p->num_tracks+1 > p->tracks_alloc)
    {
    p->tracks_alloc += 16;
    p->tracks = realloc(p->tracks,
                        sizeof(*p->tracks) *
                        (p->tracks_alloc));
    }

  if(pos < p->num_tracks)
    {
    memmove(p->tracks + pos + 1, p->tracks + pos,
            (p->num_tracks - pos)*sizeof(*p->tracks));
    }
  
  p->tracks[pos] = t;
  p->num_tracks++;

  t->p = p;
  
  }

void bg_nle_project_append_outstream(bg_nle_project_t * p,
                                     bg_nle_outstream_t * t)
  {
  bg_nle_project_insert_outstream(p, t, p->num_outstreams);
  }

void bg_nle_project_insert_outstream(bg_nle_project_t * p,
                                     bg_nle_outstream_t * t, int pos)
  {
  if(t->flags & BG_NLE_TRACK_PLAYBACK)
    {
    switch(t->type)
      {
      case BG_NLE_TRACK_AUDIO:
        p->current_audio_outstream = t;
        break;
      case BG_NLE_TRACK_VIDEO:
        p->current_video_outstream = t;
        break;
      case BG_NLE_TRACK_NONE:
        break;
      }
    }
  
  if(p->num_outstreams+1 > p->outstreams_alloc)
    {
    p->outstreams_alloc += 16;
    p->outstreams = realloc(p->outstreams,
                        sizeof(*p->outstreams) *
                        (p->outstreams_alloc));
    }

  if(pos < p->num_outstreams)
    {
    memmove(p->outstreams + pos + 1, p->outstreams + pos,
            (p->num_outstreams - pos)*sizeof(*p->outstreams));
    }
  
  p->outstreams[pos] = t;
  p->num_outstreams++;

  t->p = p;
  
  }


static void resolve_source_tracks(bg_nle_outstream_t * s,
                                  bg_nle_track_t ** tracks, int num_tracks)
  {
  int i, j;
  s->source_tracks = malloc(s->source_tracks_alloc *
                            sizeof(*s->source_tracks));

  for(i = 0; i < s->num_source_tracks; i++)
    {
    for(j = 0; j < num_tracks; j++)
      {
      if(s->source_track_ids[i] == tracks[j]->id)
        {
        s->source_tracks[i] = tracks[j];
        break;
        }
      }
    }
  }


void bg_nle_project_resolve_ids(bg_nle_project_t * p)
  {
  int i;
  /* Set the source tracks of the outstreams */

  for(i = 0; i < p->num_outstreams; i++)
    {
    resolve_source_tracks(p->outstreams[i],
                          p->tracks, p->num_tracks);
    }
  
  }

int bg_nle_project_outstream_index(bg_nle_project_t * p, bg_nle_outstream_t * outstream)
  {
  int i;
  for(i = 0; i < p->num_outstreams; i++)
    {
    if(p->outstreams[i] == outstream)
      return i;
    }
  return -1;
  }

int bg_nle_project_track_index(bg_nle_project_t * p, bg_nle_track_t * track)
  {
  int i;
  
  for(i = 0; i < p->num_tracks; i++)
    {
    if(p->tracks[i] == track)
      return i;
    }
  return -1;
  }