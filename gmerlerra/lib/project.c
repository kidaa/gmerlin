#include <gavl/gavl.h>

#include <gmerlin/cfg_registry.h>
#include <gmerlin/translation.h>
#include <gmerlin/utils.h>

#include <track.h>
#include <project.h>


bg_nle_project_t * bg_nle_project_create(const char * file,
                                         bg_plugin_registry_t * plugin_reg)
  {
  bg_nle_project_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->end_visible = GAVL_TIME_SCALE * 10;
  ret->start_selection = 0;
  ret->end_selection = -1;
  ret->media_list = bg_nle_media_list_create(plugin_reg);
  ret->plugin_reg = plugin_reg;
  return ret;
  }

void bg_nle_project_destroy(bg_nle_project_t * p)
  {
  int i;

  /* Free tracks */
  for(i = 0; i < p->num_audio_tracks; i++)
    bg_nle_track_destroy(p->audio_tracks[i]);

  if(p->audio_tracks)
    free(p->audio_tracks);

  for(i = 0; i < p->num_video_tracks; i++)
    bg_nle_track_destroy(p->video_tracks[i]);

  if(p->video_tracks)
    free(p->video_tracks);

  if(p->tracks)
    free(p->tracks);
  
  free(p);
  }

bg_nle_track_t * bg_nle_project_add_audio_track(bg_nle_project_t * p)
  {
  bg_nle_track_t * track;
  char * tmp_string;
  
  if(p->num_audio_tracks+1 > p->audio_tracks_alloc)
    {
    p->audio_tracks_alloc += 16;
    p->audio_tracks = realloc(p->audio_tracks,
                              sizeof(*p->audio_tracks) *
                              (p->audio_tracks_alloc));
    }
  if(p->num_tracks+1 > p->tracks_alloc)
    {
    p->tracks_alloc += 16;
    p->tracks = realloc(p->tracks,
                        sizeof(*p->tracks) *
                        (p->tracks_alloc));
    }
  
  track = bg_nle_track_create(BG_NLE_TRACK_AUDIO);
  
  p->audio_tracks[p->num_audio_tracks] = track;
  p->tracks[p->num_tracks] = track;

  p->num_audio_tracks++;
  p->num_tracks++;

  tmp_string = bg_sprintf("Audio track %d", p->num_audio_tracks);
  bg_nle_track_set_name(track, tmp_string);
  free(tmp_string);
  
  p->changed_flags |= BG_NLE_PROJECT_TRACKS_CHANGED;
  
  return track;
  }

bg_nle_track_t * bg_nle_project_add_video_track(bg_nle_project_t * p)
  {
  bg_nle_track_t * track;
  char * tmp_string;

  if(p->num_video_tracks+1 > p->video_tracks_alloc)
    {
    p->video_tracks_alloc += 16;
    p->video_tracks = realloc(p->video_tracks,
                              sizeof(*p->video_tracks) *
                              (p->video_tracks_alloc));
    }
  if(p->num_tracks+1 > p->tracks_alloc)
    {
    p->tracks_alloc += 16;
    p->tracks = realloc(p->tracks,
                        sizeof(*p->tracks) *
                        (p->tracks_alloc));
    }

  track = bg_nle_track_create(BG_NLE_TRACK_VIDEO);
  p->video_tracks[p->num_video_tracks] = track;
  p->tracks[p->num_tracks] = track;

  p->num_video_tracks++;
  p->num_tracks++;

  tmp_string = bg_sprintf("Video track %d", p->num_audio_tracks);
  bg_nle_track_set_name(track, tmp_string);
  free(tmp_string);

  p->changed_flags |= BG_NLE_PROJECT_TRACKS_CHANGED;
  
  return track;
  }

void bg_nle_project_append_track(bg_nle_project_t * p, bg_nle_track_t * t)
  {
  if(p->num_tracks+1 > p->tracks_alloc)
    {
    p->tracks_alloc += 16;
    p->tracks = realloc(p->tracks,
                        sizeof(*p->tracks) *
                        (p->tracks_alloc));
    }
  p->tracks[p->num_tracks] = t;
  p->num_tracks++;
  
  switch(t->type)
    {
    case BG_NLE_TRACK_AUDIO:
      if(p->num_audio_tracks+1 > p->audio_tracks_alloc)
        {
        p->audio_tracks_alloc += 16;
        p->audio_tracks = realloc(p->audio_tracks,
                                  sizeof(*p->audio_tracks) *
                                  (p->audio_tracks_alloc));
        }
      p->tracks[p->num_audio_tracks] = t;
      p->num_audio_tracks++;
      break;
    case BG_NLE_TRACK_VIDEO:
      if(p->num_video_tracks+1 > p->video_tracks_alloc)
        {
        p->video_tracks_alloc += 16;
        p->video_tracks = realloc(p->video_tracks,
                                  sizeof(*p->video_tracks) *
                                  (p->video_tracks_alloc));
        }
      p->tracks[p->num_video_tracks] = t;
      p->num_video_tracks++;
      break;
    case BG_NLE_TRACK_NONE:
      break;
    }
  
  }


bg_nle_outstream_t * bg_nle_project_add_video_outstream(bg_nle_project_t * p)
  {
  bg_nle_outstream_t * outstream;
  char * tmp_string;
  
  if(p->num_video_outstreams+1 > p->video_outstreams_alloc)
    {
    p->video_outstreams_alloc += 16;
    p->video_outstreams = realloc(p->video_outstreams,
                              sizeof(*p->video_outstreams) *
                              (p->video_outstreams_alloc));
    }
  if(p->num_outstreams+1 > p->outstreams_alloc)
    {
    p->outstreams_alloc += 16;
    p->outstreams = realloc(p->outstreams,
                        sizeof(*p->outstreams) *
                        (p->outstreams_alloc));
    }

  outstream = bg_nle_outstream_create(BG_NLE_TRACK_VIDEO);
  p->video_outstreams[p->num_video_outstreams] = outstream;
  p->outstreams[p->num_outstreams] = outstream;

  p->num_video_outstreams++;
  p->num_outstreams++;
  
  tmp_string = bg_sprintf("Video outstream %d", p->num_video_outstreams);
  bg_nle_outstream_set_name(outstream, tmp_string);
  free(tmp_string);

  p->changed_flags |= BG_NLE_PROJECT_OUTSTREAMS_CHANGED;
  
  return outstream;
  
  }

bg_nle_outstream_t * bg_nle_project_add_audio_outstream(bg_nle_project_t * p)
  {
  bg_nle_outstream_t * outstream;
  char * tmp_string;

  if(p->num_audio_outstreams+1 > p->audio_outstreams_alloc)
    {
    p->audio_outstreams_alloc += 16;
    p->audio_outstreams = realloc(p->audio_outstreams,
                              sizeof(*p->audio_outstreams) *
                              (p->audio_outstreams_alloc));
    }
  if(p->num_outstreams+1 > p->outstreams_alloc)
    {
    p->outstreams_alloc += 16;
    p->outstreams = realloc(p->outstreams,
                        sizeof(*p->outstreams) *
                        (p->outstreams_alloc));
    }

  outstream = bg_nle_outstream_create(BG_NLE_TRACK_AUDIO);
  p->audio_outstreams[p->num_audio_outstreams] = outstream;
  p->outstreams[p->num_outstreams] = outstream;

  p->num_audio_outstreams++;
  p->num_outstreams++;

  tmp_string = bg_sprintf("Audio outstream %d", p->num_audio_outstreams);
  bg_nle_outstream_set_name(outstream, tmp_string);
  free(tmp_string);
  p->changed_flags |= BG_NLE_PROJECT_OUTSTREAMS_CHANGED;
  
  return outstream;
  
  }

void bg_nle_project_append_outstream(bg_nle_project_t * p, bg_nle_outstream_t * t)
  {
  if(p->num_outstreams+1 > p->outstreams_alloc)
    {
    p->outstreams_alloc += 16;
    p->outstreams = realloc(p->outstreams,
                        sizeof(*p->outstreams) *
                        (p->outstreams_alloc));
    }
  p->outstreams[p->num_outstreams] = t;
  p->num_outstreams++;
  
  switch(t->type)
    {
    case BG_NLE_TRACK_AUDIO:
      if(p->num_audio_outstreams+1 > p->audio_outstreams_alloc)
        {
        p->audio_outstreams_alloc += 16;
        p->audio_outstreams = realloc(p->audio_outstreams,
                                  sizeof(*p->audio_outstreams) *
                                  (p->audio_outstreams_alloc));
        }
      p->outstreams[p->num_audio_outstreams] = t;
      p->num_audio_outstreams++;
      break;
    case BG_NLE_TRACK_VIDEO:
      if(p->num_video_outstreams+1 > p->video_outstreams_alloc)
        {
        p->video_outstreams_alloc += 16;
        p->video_outstreams = realloc(p->video_outstreams,
                                  sizeof(*p->video_outstreams) *
                                  (p->video_outstreams_alloc));
        }
      p->outstreams[p->num_video_outstreams] = t;
      p->num_video_outstreams++;
      break;
    case BG_NLE_TRACK_NONE:
      break;
    }
  
  }
