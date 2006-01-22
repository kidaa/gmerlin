/*****************************************************************
 
  player_ov.c
 
  Copyright (c) 2003-2004 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <keycodes.h>

#include <player.h>
#include <playerprivate.h>

struct bg_player_ov_context_s
  {
  bg_plugin_handle_t * plugin_handle;
  bg_ov_plugin_t     * plugin;
  void               * priv;
  bg_player_t        * player;
  int do_sync;
  bg_ov_callbacks_t callbacks;
  gavl_video_frame_t * frame;
  gavl_video_frame_t * still_frame;
  pthread_mutex_t     still_mutex;
  
  const char * error_msg;
  int still_shown;
  
  };

/* Callback functions */

static void key_callback(void * data, int key, int mask)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;
  //  fprintf(stderr, "Key callback %d, 0x%02x\n", key, mask);

  switch(key)
    {
    /* Take the keys, we can handle from here */
    case BG_KEY_LEFT:
      bg_player_seek_rel(ctx->player,   -2 * GAVL_TIME_SCALE );
      break;
    case BG_KEY_RIGHT:
      bg_player_seek_rel(ctx->player,   2 * GAVL_TIME_SCALE );
      break;
    case BG_KEY_UP:
      bg_player_set_volume_rel(ctx->player, 1.0);
      break;
    case BG_KEY_DOWN:
      bg_player_set_volume_rel(ctx->player,  -1.0);
      break;
    case BG_KEY_0:
      bg_player_seek(ctx->player, 0 );
      break;
    case BG_KEY_SPACE:
      bg_player_pause(ctx->player);
      break;
    default: /* Broadcast this */
      bg_player_key_pressed(ctx->player, key, mask);
      break;
    }

  }

static void button_callback(void * data, int x, int y, int button, int mask)
  {
  bg_player_ov_context_t * ctx = (bg_player_ov_context_t*)data;

  if(button == 4)
    bg_player_seek_rel(ctx->player, 2 * GAVL_TIME_SCALE );
  else if(button == 5)
    bg_player_seek_rel(ctx->player, - 2 * GAVL_TIME_SCALE );
    
  
  fprintf(stderr, "Button callback %d %d (Button %d)\n", x, y, button);
  }

/* Create frame */

void * bg_player_ov_create_frame(void * data)
  {
  gavl_video_frame_t * ret;
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t *)data;

  if(ctx->plugin->alloc_frame)
    {
    bg_plugin_lock(ctx->plugin_handle);
    ret = ctx->plugin->alloc_frame(ctx->priv);
    bg_plugin_unlock(ctx->plugin_handle);
    }
  else
    ret = gavl_video_frame_create(&(ctx->player->video_stream.output_format));

  //  fprintf(stderr, "gavl_video_frame_clear %d %d %d\n", ret->strides[0], ret->strides[1], ret->strides[2]);
  //  gavl_video_format_dump(&(ctx->player->video_stream.output_format));
  gavl_video_frame_clear(ret, &(ctx->player->video_stream.output_format));
  
  return (void*)ret;
  }

void bg_player_ov_destroy_frame(void * data, void * frame)
  {
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t *)data;
  
  if(ctx->plugin->free_frame)
    {
    bg_plugin_lock(ctx->plugin_handle);
    ctx->plugin->free_frame(ctx->priv, (gavl_video_frame_t*)frame);
    bg_plugin_unlock(ctx->plugin_handle);
    }
  else
    gavl_video_frame_destroy((gavl_video_frame_t*)frame);
  }

void bg_player_ov_create(bg_player_t * player)
  {
  bg_player_ov_context_t * ctx;
  ctx = calloc(1, sizeof(*ctx));
  ctx->player = player;

  pthread_mutex_init(&(ctx->still_mutex),(pthread_mutexattr_t *)0);
  
  /* Load output plugin */
  
  ctx->callbacks.key_callback    = key_callback;
  ctx->callbacks.button_callback = button_callback;
  ctx->callbacks.data = ctx;
  player->ov_context = ctx;
  }

void bg_player_ov_standby(bg_player_ov_context_t * ctx)
  {
  //  fprintf(stderr, "bg_player_ov_standby\n");
  
  if(!ctx->plugin_handle)
    return;

  bg_plugin_lock(ctx->plugin_handle);

  if(ctx->plugin->show_window)
    ctx->plugin->show_window(ctx->priv, 0);
  bg_plugin_unlock(ctx->plugin_handle);
  }


void bg_player_ov_set_plugin(bg_player_t * player, bg_plugin_handle_t * handle)
  {
  bg_player_ov_context_t * ctx;

  ctx = player->ov_context;

  if(ctx->plugin_handle)
    bg_plugin_unref(ctx->plugin_handle);
  

  ctx->plugin_handle = handle;

  bg_plugin_ref(ctx->plugin_handle);

  ctx->plugin = (bg_ov_plugin_t*)(ctx->plugin_handle->plugin);
  ctx->priv = ctx->plugin_handle->priv;

  bg_plugin_lock(ctx->plugin_handle);
  if(ctx->plugin->set_callbacks)
    ctx->plugin->set_callbacks(ctx->priv, &(ctx->callbacks));
  bg_plugin_unlock(ctx->plugin_handle);

  }

void bg_player_ov_destroy(bg_player_t * player)
  {
  bg_player_ov_context_t * ctx;
  
  ctx = player->ov_context;
  
  if(ctx->plugin_handle)
    bg_plugin_unref(ctx->plugin_handle);

  
  free(ctx);
  }

int bg_player_ov_init(bg_player_ov_context_t * ctx)
  {
  int result;
  
  gavl_video_format_copy(&(ctx->player->video_stream.output_format),
                         &(ctx->player->video_stream.input_format));

  bg_plugin_lock(ctx->plugin_handle);
  result = ctx->plugin->open(ctx->priv,
                             &(ctx->player->video_stream.output_format),
                             "Video output");
  if(result && ctx->plugin->show_window)
    ctx->plugin->show_window(ctx->priv, 1);

  else if(!result)
    {
    if(ctx->plugin->common.get_error)
      ctx->error_msg = ctx->plugin->common.get_error(ctx->priv);
    }
  
  bg_plugin_unlock(ctx->plugin_handle);
  return result;
  }

void bg_player_ov_update_still(bg_player_ov_context_t * ctx)
  {
  bg_fifo_state_t state;

  //  fprintf(stderr, "bg_player_ov_update_still\n");
  pthread_mutex_lock(&ctx->still_mutex);

  if(ctx->frame)
    bg_fifo_unlock_read(ctx->player->video_stream.fifo);
  //  fprintf(stderr, "bg_player_ov_update_still 1\n");

  ctx->frame = bg_fifo_lock_read(ctx->player->video_stream.fifo, &state);
  //  fprintf(stderr, "bg_player_ov_update_still 2 state: %d\n", state);

  if(!ctx->still_frame)
    {
    //      fprintf(stderr, "create_frame....");
    ctx->still_frame = bg_player_ov_create_frame(ctx);
    //      fprintf(stderr, "done\n");
    }
  
  if(ctx->frame)
    {
    gavl_video_frame_copy(&(ctx->player->video_stream.output_format),
                          ctx->still_frame, ctx->frame);
    //      fprintf(stderr, "Unlock read....%p", ctx->frame);
    bg_fifo_unlock_read(ctx->player->video_stream.fifo);
    ctx->frame = (gavl_video_frame_t*)0;
    //      fprintf(stderr, "Done\n");
    }
  else
    {
    //    fprintf(stderr, "update_still: Got no frame\n");
    gavl_video_frame_clear(ctx->still_frame, &(ctx->player->video_stream.output_format));
    }
  
  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->put_still(ctx->priv, ctx->still_frame);
  bg_plugin_unlock(ctx->plugin_handle);

  
  pthread_mutex_unlock(&ctx->still_mutex);
  }

void bg_player_ov_cleanup(bg_player_ov_context_t * ctx)
  {
  pthread_mutex_lock(&ctx->still_mutex);
  if(ctx->still_frame)
    {
    //      fprintf(stderr, "Destroy still frame...");
    bg_player_ov_destroy_frame(ctx, ctx->still_frame);
      //      fprintf(stderr, "done\n");
    ctx->still_frame = (gavl_video_frame_t*)0;
    }
  pthread_mutex_unlock(&ctx->still_mutex);

  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->close(ctx->priv);
  bg_plugin_unlock(ctx->plugin_handle);
  }

void bg_player_ov_sync(bg_player_t * p)
  {
  p->ov_context->do_sync  = 1;
  }


const char * bg_player_ov_get_error(bg_player_ov_context_t * ctx)
  {
  return ctx->error_msg;
  }

static void ping_func(void * data)
  {
  bg_player_ov_context_t * ctx;
  ctx = (bg_player_ov_context_t*)data;

  //  fprintf(stderr, "Ping func\n");  
  
  pthread_mutex_lock(&ctx->still_mutex);
  
  if(!ctx->still_shown)
    {
    if(ctx->frame)
      {
      //      fprintf(stderr, "create_frame....");
      ctx->still_frame = bg_player_ov_create_frame(data);
      //      fprintf(stderr, "done\n");
      
      gavl_video_frame_copy(&(ctx->player->video_stream.output_format),
                            ctx->still_frame, ctx->frame);
      //      fprintf(stderr, "Unlock read....%p", ctx->frame);
      bg_fifo_unlock_read(ctx->player->video_stream.fifo);
      ctx->frame = (gavl_video_frame_t*)0;
      //      fprintf(stderr, "Done\n");
      
      //      fprintf(stderr, "Put still...");
      bg_plugin_lock(ctx->plugin_handle);
      ctx->plugin->put_still(ctx->priv, ctx->still_frame);
      bg_plugin_unlock(ctx->plugin_handle);
      //      fprintf(stderr, "Done\n");
      }
    
    ctx->still_shown = 1;
    }
  //  fprintf(stderr, "handle_events...");
  bg_plugin_lock(ctx->plugin_handle);
  ctx->plugin->handle_events(ctx->priv);
  bg_plugin_unlock(ctx->plugin_handle);
  //  fprintf(stderr, "Done\n");
  
  pthread_mutex_unlock(&ctx->still_mutex);
  }

void * bg_player_ov_thread(void * data)
  {
  bg_player_ov_context_t * ctx;
  gavl_time_t diff_time, frame_time;
  gavl_time_t current_time;
  bg_fifo_state_t state;
  
  ctx = (bg_player_ov_context_t*)data;

  //  fprintf(stderr, "Starting ov thread\n");

  while(1)
    {
    if(!bg_player_keep_going(ctx->player, ping_func, ctx))
      {
      //      fprintf(stderr, "bg_player_keep_going returned 0\n");
      break;
      }
    if(ctx->frame)
      {
      //      fprintf(stderr, "Unlock_read %p...", ctx->frame);
      bg_fifo_unlock_read(ctx->player->video_stream.fifo);
      //      fprintf(stderr, "done\n");
      
      ctx->frame = (gavl_video_frame_t*)0;
      }

    pthread_mutex_lock(&ctx->still_mutex);
    if(ctx->still_frame)
      {
      //      fprintf(stderr, "Destroy still frame...");
      bg_player_ov_destroy_frame(data, ctx->still_frame);
      //      fprintf(stderr, "done\n");
      ctx->still_frame = (gavl_video_frame_t*)0;
      }
    pthread_mutex_unlock(&ctx->still_mutex);
    
    ctx->still_shown = 0;

    //    fprintf(stderr, "Lock read\n");
    ctx->frame = bg_fifo_lock_read(ctx->player->video_stream.fifo, &state);
    //    fprintf(stderr, "Lock read done %p\n", ctx->frame);
    if(!ctx->frame)
      {
      //      fprintf(stderr, "Got no frame\n");
      break;
      }
    bg_player_time_get(ctx->player, 1, &current_time);

    frame_time = gavl_time_unscale(ctx->player->video_stream.output_format.timescale,
                                   ctx->frame->time_scaled);
#if 0
    fprintf(stderr, "F: %f, C: %f\n",
            gavl_time_to_seconds(frame_time),
            gavl_time_to_seconds(current_time));
#endif

    
    
    if(!ctx->do_sync)
      {
      diff_time =  frame_time - current_time;
      
      /* Wait until we can display the frame */
      if(diff_time > 0)
        gavl_time_delay(&diff_time);
    
      /* TODO: Drop frames */
      else if(diff_time < -100000)
        {
        //        fprintf(stderr, "Warning, frame dropping not yet implemented\n");
        }
      }
    //    fprintf(stderr, "Frame time: %lld\n", frame->time);
    bg_plugin_lock(ctx->plugin_handle);
    //    fprintf(stderr, "Put video\n");
    ctx->plugin->put_video(ctx->priv, ctx->frame);
    ctx->plugin->handle_events(ctx->priv);
    
    if(ctx->do_sync)
      {
      bg_player_time_set(ctx->player, frame_time);
      //      fprintf(stderr, "OV Resync %lld\n", frame->time);
      ctx->do_sync = 0;
      }
    
    bg_plugin_unlock(ctx->plugin_handle);
    }
  
  //  fprintf(stderr, "ov thread finisheded\n");
  return NULL;
  }

void * bg_player_ov_still_thread(void *data)
  {
  bg_player_ov_context_t * ctx;
  gavl_time_t delay_time = gavl_seconds_to_time(0.02);
  
  ctx = (bg_player_ov_context_t*)data;
  
  /* Put the image into the window once and handle only events thereafter */
  //  fprintf(stderr, "Starting still loop\n");

  ctx->still_shown = 0;
  while(1)
    {
    if(!bg_player_keep_going(ctx->player, NULL, NULL))
      {
      //      fprintf(stderr, "bg_player_keep_going returned 0\n");
      break;
      }
    if(!ctx->still_shown)
      {
      bg_player_ov_update_still(ctx);
      ctx->still_shown = 1;
      }
    //    fprintf(stderr, "Handle events...");
    bg_plugin_lock(ctx->plugin_handle);
    ctx->plugin->handle_events(ctx->priv);
    bg_plugin_unlock(ctx->plugin_handle);
    //    fprintf(stderr, "Handle events done\n");
    gavl_time_delay(&delay_time);
    }
  //  fprintf(stderr, "still thread finished\n");
  return NULL;
  }
