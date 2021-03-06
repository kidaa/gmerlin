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
#include <stdio.h>
#include <string.h>

#include <wctype.h>
#include <errno.h>

/* For stat/opendir */

#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>


#include <config.h>

#include <gmerlin/tree.h>
#include <treeprivate.h>

#include <gmerlin/charset.h>

#include <gmerlin/utils.h>
#include <gmerlin/translation.h>



#include <gmerlin/log.h>
#define LOG_DOMAIN "album"

#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#endif

#include <gavl/metatags.h>

/*
 *  This must be called, whenever the tracks in an album
 *  change.
 */

static void delete_shuffle_list(bg_album_t * album)
  {
  bg_shuffle_list_destroy(album->com->shuffle_list);
  album->com->shuffle_list = NULL;
  }

static char * new_filename(bg_album_t * album)
  {
  /*
   *  Album filenames are constructed like "aXXXXXXXX.xml",
   *  where XXXXXXXX is a hexadecimal unique identifier
   */
  char * template = NULL;
  char * path = NULL;
  char * ret = NULL;
  char * pos;
  
  template = bg_sprintf("%s/a%%08x.xml", album->com->directory);

  path = bg_create_unique_filename(template);

  if(!path)
    goto fail;

  pos = strrchr(path, '/');

  pos++;
  ret = gavl_strdup(pos);
  free(path);
    
  fail:
  if(template)
    free(template);
  
  return ret;
  }

void bg_album_set_default_location(bg_album_t * album)
  {
  if(!album->xml_file)
    {
    album->xml_file = new_filename(album);
    }
  }

static void entry_from_track_info(bg_album_common_t * com,
                                  bg_album_entry_t * entry,
                                  bg_track_info_t  * track_info,
                                  int update_name)
  {
  int i;
  int name_set = 0;
  entry->num_audio_streams = track_info->num_audio_streams;

  entry->num_video_streams = 0;
  entry->num_still_streams = 0;

  for(i = 0; i < track_info->num_video_streams; i++)
    {
    if(track_info->video_streams[i].format.framerate_mode ==
       GAVL_FRAMERATE_STILL)
      entry->num_still_streams++;
    else
      entry->num_video_streams++;
    }
  
  entry->num_subtitle_streams =
    track_info->num_text_streams +
    track_info->num_overlay_streams;
  
  if(!entry->name || update_name)
    {
    if(entry->name)
      {
      free(entry->name);
      entry->name = NULL;
      }

    if(entry->name_w)
      {
      free(entry->name_w);
      entry->name_w = NULL;
      entry->len_w = 0;
      }

    /* Track info has a name */
    
    if(com && com->use_metadata && com->metadata_format)
      {
      entry->name = bg_create_track_name(&track_info->metadata,
                                         com->metadata_format);
      if(entry->name)
        name_set = 1;
      }
    
    if(!name_set)
      {
      const char * name;

      if((name = gavl_metadata_get(&track_info->metadata, GAVL_META_STATION)) ||
         (name = gavl_metadata_get(&track_info->metadata, GAVL_META_LABEL)))
        entry->name = gavl_strrep(entry->name, name);
      /* Take filename minus extension */
      else
        {
        entry->name =
          bg_get_track_name_default(entry->location,
                                    entry->index, entry->total_tracks);
        }
      }
    }

  if(!gavl_metadata_get_long(&track_info->metadata,
                             GAVL_META_APPROX_DURATION, &entry->duration))
    entry->duration = GAVL_TIME_UNDEFINED;
  entry->flags &= ~BG_ALBUM_ENTRY_ERROR;
  
  if(track_info->url)
    {
    entry->location = gavl_strrep(entry->location, track_info->url);
    entry->index = 0;
    entry->total_tracks = 1;
    entry->flags = BG_ALBUM_ENTRY_REDIRECTOR;
    }
  
  }

void bg_album_update_entry(bg_album_t * album,
                           bg_album_entry_t * entry,
                           bg_track_info_t  * track_info,
                           int callback, int update_name)
  {
  entry_from_track_info(album->com, entry, track_info, update_name);
  if(callback)
    bg_album_entry_changed(album, entry);
  }

bg_album_entry_t *
bg_album_entry_create_from_track_info(bg_track_info_t * track_info,
                                      const char * url)
  {
  bg_album_entry_t * ret;
  ret = bg_album_entry_create();
  ret->location = gavl_strrep(ret->location, url);
  entry_from_track_info(NULL, ret, track_info, 1);
  return ret;
  }

bg_album_t * bg_album_create(bg_album_common_t * com, bg_album_type_t type,
                             bg_album_t * parent)
  {
  bg_album_t * ret = calloc(1, sizeof(*ret));
  ret->com = com;
  ret->parent = parent;
  ret->type = type;
#ifdef HAVE_INOTIFY
  ret->inotify_wd = -1;
#endif
  return ret;
  }

char * bg_album_get_name(bg_album_t * a)
  {
  return a->name;
  }

int bg_album_get_num_entries(bg_album_t * a)
  {
  int ret = 0;
  bg_album_entry_t * entry;
  entry = a->entries;

  while(entry)
    {
    ret++;
    entry = entry->next;
    }
  return ret;
  }

bg_album_entry_t * bg_album_get_entry(bg_album_t * a, int i)
  {
  bg_album_entry_t * ret;
  ret = a->entries;

  while(i--)
    {
    if(!ret)
      return NULL;
    ret = ret->next;
    }
  return ret;
  }


/* Add items */

static void insertion_done(bg_album_t * album, int start, int num)
  {
  switch(album->type)
    {
    case BG_ALBUM_TYPE_INCOMING:
    case BG_ALBUM_TYPE_FAVOURITES:
      break;
    case BG_ALBUM_TYPE_REGULAR:
    case BG_ALBUM_TYPE_TUNER:
      if(!album->xml_file)
        album->xml_file = new_filename(album);
      break;
    case BG_ALBUM_TYPE_REMOVABLE:
    case BG_ALBUM_TYPE_PLUGIN:
      break;
    }
  delete_shuffle_list(album);
  if(album->insert_callback)
    album->insert_callback(album, start, num, album->insert_callback_data);
  }

void bg_album_insert_entries_after(bg_album_t * album,
                                   bg_album_entry_t * new_entries,
                                   bg_album_entry_t * before)
  {
  bg_album_entry_t * last_new_entry;
  int start, num;
  
  if(!new_entries)
    return;
  
  last_new_entry = new_entries;

  num = 1;
  while(last_new_entry->next)
    {
    last_new_entry = last_new_entry->next;
    num++;
    }

  if(!before)
    {
    last_new_entry->next = album->entries;
    album->entries = new_entries;
    start = 0;
    }
  else
    {
    start = bg_album_get_index(album, before) + 1;
    last_new_entry->next = before->next;
    before->next = new_entries;
    }

  insertion_done(album, start, num);
  
  }

void bg_album_insert_entries_before(bg_album_t * album,
                                    bg_album_entry_t * new_entries,
                                    bg_album_entry_t * after)
  {
  bg_album_entry_t * before;
  bg_album_entry_t * last_new_entry;
  int start, num;

  if(!new_entries)
    return;
  
  last_new_entry = new_entries;
  
  num = 1;
  while(last_new_entry->next)
    {
    last_new_entry = last_new_entry->next;
    num++;
    }
  
  /* Fill empty album */
  
  if(!album->entries)
    {
    album->entries = new_entries;
    start = 0;
    }
  
  /* Append as first item */
  
  else if(after == album->entries)
    {
    last_new_entry->next = album->entries;
    album->entries = new_entries;
    start = 0;
    }
  else
    {
    before = album->entries;
    start = 1;
    while(before->next != after)
      {
      before = before->next;
      start++;
      }
    before->next = new_entries;
    last_new_entry->next = after;
    }
  
  insertion_done(album, start, num);

  }

void bg_album_insert_urls_before(bg_album_t * a,
                                 char ** locations,
                                 const char * plugin,
                                 int prefer_edl,
                                 bg_album_entry_t * after)
  {
  int i = 0;
  bg_album_entry_t * new_entries;

  while(locations[i])
    {
    new_entries = bg_album_load_url(a, locations[i], plugin, prefer_edl);
    bg_album_insert_entries_before(a, new_entries, after);
    //    bg_album_changed(a);
    i++;
    }
  }

void bg_album_insert_file_before(bg_album_t * a,
                                 char * file,
                                 const char * plugin,
                                 int prefer_edl,
                                 bg_album_entry_t * after,
                                 time_t mtime)
  {
  bg_album_entry_t * new_entries;
  bg_album_entry_t * e;
  
  new_entries = bg_album_load_url(a, file, plugin, prefer_edl);
  e = new_entries;
  while(e)
    {
    e->mtime = mtime;
    e->flags |= BG_ALBUM_ENTRY_SYNC;
    e = e->next;
    }
  bg_album_insert_entries_before(a, new_entries, after);
  //    bg_album_changed(a);
    
  }


void bg_album_insert_urls_after(bg_album_t * a,
                                char ** locations,
                                const char * plugin,
                                int prefer_edl,
                                bg_album_entry_t * before)
  {
  int i = 0;
  bg_album_entry_t * new_entries;

  while(locations[i])
    {
    new_entries = bg_album_load_url(a, locations[i], plugin, prefer_edl);
    bg_album_insert_entries_after(a, new_entries, before);

    before = new_entries;
    if(before)
      {
      while(before->next)
        before = before->next;
      }
    //    bg_album_changed(a);
    i++;
    }
  }

/* Inserts a string of the type text/uri-list into the album */

void bg_album_insert_urilist_after(bg_album_t * a, const char * str,
                                   int len, bg_album_entry_t * before)
  {
  char ** uri_list;
  
  uri_list = bg_urilist_decode(str, len);
  
  if(!uri_list)
    return;

  bg_album_insert_urls_after(a, uri_list, NULL, 0, before);

  bg_urilist_free(uri_list);
  }

void bg_album_insert_urilist_before(bg_album_t * a, const char * str,
                                    int len, bg_album_entry_t * after)
  {
  char ** uri_list;
  
  uri_list = bg_urilist_decode(str, len);

  if(!uri_list)
    return;

  bg_album_insert_urls_before(a, uri_list, NULL, 0, after);
  
  bg_urilist_free(uri_list);
  }

static char * get_playlist_location(const char * orig,
                                    int strip_leading,
                                    const char * prefix)
  {
  int i;
  char * pos;
  if(!strncmp(orig, "file://", 7))
    orig += 7;
  
  if((*orig == '/') && strip_leading)
    {
    for(i = 0; i < strip_leading; i++)
      {
      pos = strchr(orig+1, '/');
      if(pos)
        orig = pos;
      else
        return NULL;
      }
    }
  if(prefix)
    return bg_sprintf("%s%s", prefix, orig);
  else
    return gavl_strdup(orig);
  }

int bg_album_entries_save_extm3u(bg_album_entry_t * e, const char * name,
                                 int strip_leading, const char * prefix)
  {
  FILE * out;
  char * tmp_string;
  
  if(!e)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Not exporting empty album");
    return 0;
    }
  out = fopen(name, "w");
  if(!out)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Could not open %s: %s", name, strerror(errno));
    return 0;
    }

  fprintf(out, "#EXTM3U\r\n");

  while(e)
    {
    tmp_string = get_playlist_location(e->location,
                                       strip_leading, prefix);
    if(!tmp_string)
      {
      e = e->next;
      continue;
      }
    
    fprintf(out, "#EXTINF:%d,%s\r\n",
            (int)(e->duration / GAVL_TIME_SCALE),
            e->name);

    
    fprintf(out, "%s\r\n", tmp_string);
    free(tmp_string);
    e = e->next;
    }
  fclose(out);
  return 1;
  }

int bg_album_entries_save_pls(bg_album_entry_t * e, const char * name,
                              int strip_leading, const char * prefix)
  {
  FILE * out;
  int count = 1;
  char * tmp_string;

  if(!e)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Not exporting empty album");
    return 0;
    }
  out = fopen(name, "w");
  if(!out)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Could not open %s: %s", name, strerror(errno));
    return 0;
    }
  fprintf(out, "[Playlist]\r\n");

  while(e)
    {
    tmp_string = get_playlist_location(e->location,
                                       strip_leading, prefix);
    if(!tmp_string)
      {
      e = e->next;
      continue;
      }
    fprintf(out, "File%d=%s\r\n", count, tmp_string);
    fprintf(out, "Title%d=%s\r\n",count, e->name);
    fprintf(out, "Length%d=%d\r\n",count, (int)(e->duration / GAVL_TIME_SCALE));
    free(tmp_string);
    e = e->next;
    count++;
    }
  // Footer
  fprintf(out, "NumberOfEntries=%d\r\n", count-1);
  fprintf(out, "Version=2\r\n");
  fclose(out);
  return 1;
  
  }

/* Open / close */

static int open_device(bg_album_t * a)
  {
  bg_track_info_t * track_info;
  int i, j;
  int num_tracks;
  bg_input_plugin_t * plugin;
  bg_album_entry_t * new_entry;
  
  a->handle = bg_plugin_load(a->com->plugin_reg, a->plugin_info);

  bg_plugin_lock(a->handle);

  plugin = (bg_input_plugin_t*)a->handle->plugin;
  
  /* Open the plugin */

  if(!plugin->open(a->handle->priv, a->device))
    {
    bg_plugin_unlock(a->handle);
    return 0;
    }

  if(plugin->get_disc_name)
    {
    a->disc_name = gavl_strrep(a->disc_name,
                             plugin->get_disc_name(a->handle->priv));
    if(!a->disc_name || (*a->disc_name == '\0'))
      a->disc_name = gavl_strrep(a->disc_name, TR("Unnamed disc"));
    }
  
  if(plugin->eject_disc)
    a->flags |= BG_ALBUM_CAN_EJECT;
  
  /* Get number of tracks */

  num_tracks = plugin->get_num_tracks(a->handle->priv);

  for(i = 0; i < num_tracks; i++)
    {
    track_info = plugin->get_track_info(a->handle->priv, i);
    
    new_entry = calloc(1, sizeof(*new_entry));

    new_entry->index = i;
    new_entry->total_tracks = num_tracks;
    new_entry->name   = gavl_strdup(gavl_metadata_get(&track_info->metadata, GAVL_META_LABEL));
    new_entry->plugin = gavl_strdup(a->handle->info->name);
    
    new_entry->location = gavl_strrep(new_entry->location,
                                      a->device);
    new_entry->num_audio_streams = track_info->num_audio_streams;

    new_entry->num_still_streams = 0;
    new_entry->num_video_streams = 0;
    for(j = 0; j < track_info->num_video_streams; j++)
      {
      if(track_info->video_streams[j].format.framerate_mode ==
         GAVL_FRAMERATE_STILL)
        new_entry->num_still_streams++;
      else
        new_entry->num_video_streams++;
      }
    new_entry->num_subtitle_streams = track_info->num_text_streams +
      track_info->num_overlay_streams;


    if(!gavl_metadata_get_long(&track_info->metadata,
                               GAVL_META_APPROX_DURATION, &new_entry->duration))
      new_entry->duration = GAVL_TIME_UNDEFINED;

    bg_album_insert_entries_before(a, new_entry, NULL);
    }
  bg_plugin_unlock(a->handle);
  return 1;
  }

bg_album_type_t bg_album_get_type(bg_album_t * a)
  {
  return a->type;
  }

static bg_album_entry_t *
find_next_with_location(bg_album_t * a, char * filename, bg_album_entry_t * before)
  {
  if(!before)
    before = a->entries;
  else
    before = before->next;
  while(before)
    {
    if(!strcmp(before->location, filename))
      break;
    before = before->next;
    }
  return before;
  }

static void sync_dir_add(bg_album_t * a, char * filename, time_t mtime)
  {
  bg_album_insert_file_before(a,
                              filename,
                              NULL,
                              0,
                              NULL,
                              mtime);
  }

static void sync_dir_modify(bg_album_t * a, char * filename, time_t mtime)
  {
  bg_album_delete_with_file(a, filename);
  sync_dir_add(a, filename, mtime);
  }

int bg_album_inotify(bg_album_t * a, uint8_t * event1)
  {
#ifdef HAVE_INOTIFY
  char * filename;
  bg_album_t * child;
  struct stat stat_buf;
  
  if(a->inotify_wd >= 0)
    {
    struct inotify_event *event =
      ( struct inotify_event * ) event1;

    if(event->wd == a->inotify_wd)
      {
      switch(event->mask)
        {
        case IN_DELETE:
        case IN_MOVED_FROM:
          filename = bg_sprintf("%s/%s", a->watch_dir,
                                event->name);
          bg_log(BG_LOG_INFO, LOG_DOMAIN,
                 "%s disappeared, updating album",
                 filename);
          bg_album_delete_with_file(a, filename);
          free(filename);
          break;
        case IN_CLOSE_WRITE:
        case IN_MOVED_TO:
          filename = bg_sprintf("%s/%s", a->watch_dir,
                                event->name);
          bg_log(BG_LOG_INFO, LOG_DOMAIN,
                 "%s appeared, updating album",
                 filename);
          if(stat(filename, &stat_buf))
            {
            free(filename);
            return 1;
            }
          sync_dir_add(a, filename, stat_buf.st_mtime);
          free(filename);
          break;
        }
      return 1;
      }
    }

  /* Not our event, try children */
  child = a->children;
  while(child)
    {
    if(bg_album_inotify(child, event1))
      return 1;
    child = child->next;
    }
  return 0;
  
#else
  return 0;
#endif
  }


static void sync_with_dir(bg_album_t * a)
  {
  //  char * tmp_string;
  DIR * dir;
  char filename[FILENAME_MAX];
  struct dirent * dent_ptr;
  struct stat stat_buf;
  bg_album_entry_t * e;

  struct
    {
    struct dirent d;
    char b[NAME_MAX]; /* Make sure there is enough memory */
    } dent;

  dir = opendir(a->watch_dir);
  if(!dir)
    return;

  while(!readdir_r(dir, &dent.d, &dent_ptr))
    {
    if(!dent_ptr)
      break;

    if(dent.d.d_name[0] == '.') /* Don't import hidden files */
      continue;
    
    sprintf(filename, "%s/%s", a->watch_dir, dent.d.d_name);
    
    if(stat(filename, &stat_buf))
      continue;
    
    /* Add directory as subalbum */
    if(S_ISDIR(stat_buf.st_mode))
      continue;
    else if(S_ISREG(stat_buf.st_mode))
      {
      e = find_next_with_location(a, filename, NULL);
      
      if(e)
        {
        while(e)
          {
          if(e->mtime != stat_buf.st_mtime)
            {
            sync_dir_modify(a, filename, stat_buf.st_mtime);
            break;
            }
          else
            e->flags |= BG_ALBUM_ENTRY_SYNC;
          e = find_next_with_location(a, filename, e);
          }
        }
      else
        sync_dir_add(a, filename, stat_buf.st_mtime);
      }
    }
  closedir(dir);

  bg_album_delete_unsync(a);
  
  }

int bg_album_open(bg_album_t * a)
  {
  char * tmp_filename;
  FILE * testfile;
  bg_input_plugin_t * plugin;
   
  if(a->open_count)
    {
    bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Album %s already open", a->name);
    a->open_count++;
    return 1;
    }

  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Opening album %s", a->name);
  
  
  a->cfg_section = bg_cfg_section_create(NULL);
  
  switch(a->type)
    {
    case BG_ALBUM_TYPE_REGULAR:
    case BG_ALBUM_TYPE_FAVOURITES:
    case BG_ALBUM_TYPE_INCOMING:
      if(a->xml_file)
        {
        tmp_filename = bg_sprintf("%s/%s", a->com->directory,
                                  a->xml_file);
        
        /* If the file cannot be opened, it was deleted earlier
           we exit quietly here */
        
        if(!(testfile = fopen(tmp_filename,"r")))
          {
          free(tmp_filename);
          break;
          }
        fclose(testfile);
        
        bg_album_load(a, tmp_filename);
        free(tmp_filename);
        }
      break;
    case BG_ALBUM_TYPE_TUNER:
      if(a->xml_file)
        {
        tmp_filename = bg_sprintf("%s/%s", a->com->directory,
                                  a->xml_file);
        
        /* If the file cannot be opened, it was deleted earlier
           we exit quietly here */
        
        if(!(testfile = fopen(tmp_filename,"r")))
          {
          free(tmp_filename);
          if(!open_device(a))
            return 0;
          break;
          }
        fclose(testfile);
        
        bg_album_load(a, tmp_filename);
        free(tmp_filename);
        a->handle = bg_plugin_load(a->com->plugin_reg, a->plugin_info);

        bg_plugin_lock(a->handle);

        plugin = (bg_input_plugin_t*)a->handle->plugin;
        
        /* Open the plugin */
        
        if(!plugin->open(a->handle->priv, a->device))
          {
          bg_plugin_unlock(a->handle);
          return 0;
          }
        bg_plugin_unlock(a->handle);
        }
      else
        if(!open_device(a))
          return 0;
      break;
    case BG_ALBUM_TYPE_REMOVABLE:
      /* Get infos from the plugin */
      if(!open_device(a))
        return 0;
      break;
    case BG_ALBUM_TYPE_PLUGIN:
      return 0; /* Cannot be opened */
      break;
    }
  a->open_count++;

  if((a->type == BG_ALBUM_TYPE_REGULAR) &&
     a->watch_dir) 
    {
    sync_with_dir(a);
#ifdef HAVE_INOTIFY
    a->inotify_wd = inotify_add_watch(a->com->inotify_fd,
                                      a->watch_dir,
                                      IN_CLOSE_WRITE | IN_DELETE |
                                      IN_MOVED_TO | IN_MOVED_FROM);
#endif
    }
  return 1;
  }


void bg_album_entry_destroy(bg_album_entry_t * entry)
  {
  if(entry->name)
    free(entry->name);
  if(entry->location)
    free(entry->location);
  if(entry->plugin)
    free(entry->plugin);
  free(entry);
  }

void bg_album_entries_destroy(bg_album_entry_t * entries)
  {
  bg_album_entry_t * tmp_entry;

  while(entries)
    {
    tmp_entry = entries->next;
    bg_album_entry_destroy(entries);
    entries = tmp_entry;
    }

  }

int bg_album_entries_count(bg_album_entry_t * e)
  {
  int ret = 0;
  while(e)
    {
    ret++;
    e = e->next;
    }
  return ret;
  }


bg_album_entry_t * bg_album_entry_create()
  {
  bg_album_entry_t * ret;
  ret = calloc(1, sizeof(*ret));
  return ret;
  }

void bg_album_close(bg_album_t *a )
  {
  //  char * tmp_filename;
  
  a->open_count--;

  if(a->open_count)
    {
    bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Not closing album %s (open_count > 0)", a->name);
    return;
    }
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Closing album %s", a->name);

  /* Tell the tree, if we are the current album */
  
  if((a == a->com->current_album) && a->com->set_current_callback)
    {
    a->com->set_current_callback(a->com->set_current_callback_data,
                                 NULL, NULL);
    }
  switch(a->type)
    {
    case BG_ALBUM_TYPE_REMOVABLE:
    case BG_ALBUM_TYPE_TUNER:
      a->flags &= ~BG_ALBUM_CAN_EJECT;
      bg_plugin_unref(a->handle);
      a->handle = NULL;
      if(a->disc_name)
        {
        free(a->disc_name);
        a->disc_name = NULL;
        }
      if(a->type == BG_ALBUM_TYPE_TUNER)
        bg_album_save(a, NULL);
      break;
    case BG_ALBUM_TYPE_REGULAR:
    case BG_ALBUM_TYPE_INCOMING:
    case BG_ALBUM_TYPE_FAVOURITES:
      bg_album_save(a, NULL);
      break;
    case BG_ALBUM_TYPE_PLUGIN:
      break;
    }
  
  /* Delete entries */

  bg_album_entries_destroy(a->entries);
  a->entries = NULL;

  /* Delete shuffle list */

  delete_shuffle_list(a);
  
  /* Destroy config data */
  if(a->cfg_section)
    {
    bg_cfg_section_destroy(a->cfg_section);
    a->cfg_section = NULL;
    }

#ifdef HAVE_INOTIFY
  if(a->inotify_wd >= 0)
    {
    inotify_rm_watch(a->com->inotify_fd, a->inotify_wd);
    a->inotify_wd = -1;
    }
#endif

  // if(a->watch_dir)
    
  }

void bg_album_set_expanded(bg_album_t * a, int expanded)
  {
  if(expanded)
    a->flags |= BG_ALBUM_EXPANDED;
  else
    {
    bg_album_t * child;
    a->flags &= ~BG_ALBUM_EXPANDED;
    
    /* Unexpand the children too */
    child = a->children;
    while(child)
      {
      bg_album_set_expanded(child, 0);
      child = child->next;
      }
    }
  }

int bg_album_get_expanded(bg_album_t * a)
  {
  if(a->flags & BG_ALBUM_EXPANDED)
    return 1;
  return 0;
  }

int bg_album_is_open(bg_album_t * a)
  {
  return (a->open_count) ? 1 : 0;
  }


void bg_album_destroy(bg_album_t * a)
  {
  //  char * tmp_filename;
  bg_album_t       * tmp_album;

  /* Things to do if an album was open */

  if(a->open_count)
    {
    bg_album_save(a, NULL);
    }
  if(a->name)
    free(a->name);
  if(a->xml_file)
    free(a->xml_file);
  if(a->device)
    free(a->device);
  
  if(a->disc_name)
    free(a->disc_name);
  if(a->cfg_section)
    bg_cfg_section_destroy(a->cfg_section);
  
  /* Free entries */

  bg_album_entries_destroy(a->entries);
  
  /* Free Children */

  while(a->children)
    {
    tmp_album = a->children->next;
    bg_album_destroy(a->children);
    a->children = tmp_album;
    }

  /* free rest */

  free(a);
  }

void bg_album_delete_selected(bg_album_t * album)
  {
  int num_selected = 0;
  bg_album_entry_t * cur;
  bg_album_entry_t * cur_next;
  bg_album_entry_t * new_entries_end = NULL;
  bg_album_entry_t * new_entries;
  int index, i;
  int * indices = NULL;
  
  if(!album->entries)
    return;

  cur = album->entries;

  num_selected = bg_album_num_selected(album);
  
  if(!num_selected)
    return;
  
  if(album->delete_callback)
    {
    indices = malloc((num_selected +1)*sizeof(*indices));
    }
  
  cur = album->entries;
  new_entries = NULL;
  index = 0;
  i = 0;
  
  while(cur)
    {
    cur_next = cur->next;

    if(cur->flags & BG_ALBUM_ENTRY_SELECTED)
      {
      if(cur == album->com->current_entry)
        {
        album->com->current_entry = NULL;
        album->com->current_album = NULL;
        }
      bg_album_entry_destroy(cur);
      if(indices)
        indices[i] = index;
      i++;
      }
    else
      {
      if(!new_entries)
        {
        new_entries = cur;
        new_entries_end = cur;
        }
      else
        {
        new_entries_end->next = cur;
        new_entries_end = new_entries_end->next;
        }
      }
    cur = cur_next;
    index++;
    }
  if(new_entries)
    new_entries_end->next = NULL;
  album->entries = new_entries;
  
  delete_shuffle_list(album);

  if(indices)
    {
    indices[i] = -1;
    album->delete_callback(album, indices, album->delete_callback_data);
    free(indices);
    }
  //  bg_album_changed(album);  
  }

void bg_album_delete_unsync(bg_album_t * album)
  {
  int num_selected = 0;
  bg_album_entry_t * cur;
  bg_album_entry_t * cur_next;
  bg_album_entry_t * new_entries_end = NULL;
  bg_album_entry_t * new_entries;
  int index, i;
  int * indices = NULL;
  
  if(!album->entries)
    return;

  cur = album->entries;

  num_selected = bg_album_num_unsync(album);
  
  if(!num_selected)
    return;
  
  if(album->delete_callback)
    {
    indices = malloc((num_selected +1)*sizeof(*indices));
    }
  
  cur = album->entries;
  new_entries = NULL;
  index = 0;
  i = 0;
  
  while(cur)
    {
    cur_next = cur->next;

    if(!(cur->flags & BG_ALBUM_ENTRY_SYNC))
      {
      if(cur == album->com->current_entry)
        {
        album->com->current_entry = NULL;
        album->com->current_album = NULL;
        }
      bg_album_entry_destroy(cur);
      if(indices)
        indices[i] = index;
      i++;
      }
    else
      {
      if(!new_entries)
        {
        new_entries = cur;
        new_entries_end = cur;
        }
      else
        {
        new_entries_end->next = cur;
        new_entries_end = new_entries_end->next;
        }
      }
    cur = cur_next;
    index++;
    }
  if(new_entries)
    new_entries_end->next = NULL;
  album->entries = new_entries;
  
  delete_shuffle_list(album);

  if(indices)
    {
    indices[i] = -1;
    album->delete_callback(album, indices, album->delete_callback_data);
    free(indices);
    }
  //  bg_album_changed(album);  
  }


void bg_album_delete_with_file(bg_album_t * album, const char * filename)
  {
  bg_album_entry_t * cur;
  bg_album_entry_t * cur_next;
  bg_album_entry_t * new_entries_end = NULL;
  bg_album_entry_t * new_entries;
  int index, i;
  int * indices = NULL;
  
  if(!album->entries)
    return;

  cur = album->entries;
    
  cur = album->entries;
  new_entries = NULL;
  index = 0;
  i = 0;
  
  while(cur)
    {
    cur_next = cur->next;

    if(!strcmp(cur->location, filename))
      {
      if(cur == album->com->current_entry)
        {
        album->com->current_entry = NULL;
        album->com->current_album = NULL;
        }
      bg_album_entry_destroy(cur);
      if(album->delete_callback)
        {
        indices = realloc(indices, (i+1) * sizeof(indices));
        indices[i] = index;
        }
      i++;
      }
    else
      {
      if(!new_entries)
        {
        new_entries = cur;
        new_entries_end = cur;
        }
      else
        {
        new_entries_end->next = cur;
        new_entries_end = new_entries_end->next;
        }
      }
    cur = cur_next;
    index++;
    }
  if(new_entries)
    new_entries_end->next = NULL;
  album->entries = new_entries;
  
  delete_shuffle_list(album);

  if(indices)
    {
    indices = realloc(indices, (i+1) * sizeof(indices));
    
    indices[i] = -1;
    album->delete_callback(album, indices, album->delete_callback_data);
    free(indices);
    }
  }


void bg_album_select_error_tracks(bg_album_t * album)
  {
  bg_album_entry_t * cur;
  cur = album->entries;
  while(cur)
    {
    if(cur->flags & BG_ALBUM_ENTRY_ERROR)
      cur->flags |= BG_ALBUM_ENTRY_SELECTED;
    else
      cur->flags &= ~BG_ALBUM_ENTRY_SELECTED;
    cur = cur->next;
    }
  bg_album_changed(album);
  }


static bg_album_entry_t * copy_selected(bg_album_t * album)
  {
  bg_album_entry_t * ret     = NULL;
  bg_album_entry_t * ret_end = NULL;
  bg_album_entry_t * tmp_entry;

  tmp_entry = album->entries;
  
  while(tmp_entry)
    {
    if(tmp_entry->flags & BG_ALBUM_ENTRY_SELECTED)
      {
      if(ret)
        {
        ret_end->next = bg_album_entry_copy(album, tmp_entry);
        ret_end = ret_end->next;
        }
      else
        {
        ret = bg_album_entry_copy(album, tmp_entry);
        ret_end = ret;
        }
      }
    tmp_entry = tmp_entry->next;
    }
  return ret;
  }

static bg_album_entry_t * extract_selected(bg_album_t * album)
  {
  bg_album_entry_t * selected_end = NULL;
  bg_album_entry_t * other_end = NULL;
  bg_album_entry_t * tmp_entry;
  
  bg_album_entry_t * other    = NULL;
  bg_album_entry_t * selected = NULL;
  
  while(album->entries)
    {
    tmp_entry = album->entries->next;

    if(album->entries->flags & BG_ALBUM_ENTRY_SELECTED)
      {
      if(!selected)
        {
        selected = album->entries;
        selected_end = selected;
        }
      else
        {
        selected_end->next = album->entries;
        selected_end = selected_end->next;
        }
      selected_end->next = NULL;
      }
    else
      {
      if(!other)
        {
        other = album->entries;
        other_end = other;
        }
      else
        {
        other_end->next = album->entries;
        other_end = other_end->next;
        }
      other_end->next = NULL;
      }
    album->entries = tmp_entry;
    }
  album->entries = other;
  return selected;
  }

void bg_album_move_selected_up(bg_album_t * album)
  {
  bg_album_entry_t * selected;
  selected = extract_selected(album);

  bg_album_insert_entries_after(album,
                                selected,
                                NULL);
  bg_album_changed(album);
  }

void bg_album_move_selected_down(bg_album_t * album)
  {
  bg_album_entry_t * selected;
  selected = extract_selected(album);

  bg_album_insert_entries_before(album,
                                 selected,
                                 NULL);
  bg_album_changed(album);
  }

typedef struct
  {
  bg_album_entry_t * entry;
  char * sort_string;
  } sort_entries_struct;

void bg_album_sort_entries(bg_album_t * album)
  {
  int num_entries;
  int i, j;
  char * tmp_string;
  int sort_string_len;
  sort_entries_struct * s_tmp;
  int keep_going;
  
  sort_entries_struct ** s;
  bg_album_entry_t * tmp_entry;
  
  /* 1. Count the entries */
  
  num_entries = 0;

  tmp_entry = album->entries;

  while(tmp_entry)
    {
    tmp_entry = tmp_entry->next;
    num_entries++;
    }

  if(!num_entries)
    return;
  
  /* Set up the album array */

  s = malloc(num_entries * sizeof(*s));

  tmp_entry = album->entries;
  
  for(i = 0; i < num_entries; i++)
    {
    s[i] = calloc(1, sizeof(*(s[i])));
    s[i]->entry = tmp_entry;

    /* Set up the sort string */

    tmp_string = bg_utf8_to_system(tmp_entry->name,
                                   strlen(tmp_entry->name));

    sort_string_len = strxfrm(NULL, tmp_string, 0);
    s[i]->sort_string = malloc(sort_string_len+1);
    strxfrm(s[i]->sort_string, tmp_string, sort_string_len+1);

    free(tmp_string);
    
    /* Advance */
    
    tmp_entry = tmp_entry->next;
    }

  /* Now, do a braindead bubblesort algorithm */

  for(i = 0; i < num_entries - 1; i++)
    {
    keep_going = 0;
    for(j = num_entries-1; j > i; j--)
      {
      if(strcmp(s[j]->sort_string, s[j-1]->sort_string) < 0)
        {
        s_tmp  = s[j];
        s[j]   = s[j-1];
        s[j-1] = s_tmp;
        keep_going = 1;
        }
      }
    if(!keep_going)
      break;
    }
  
  /* Rechain entries */

  album->entries = s[0]->entry;

  for(i = 0; i < num_entries-1; i++)
    {
    s[i]->entry->next = s[i+1]->entry;
    }
  
  s[num_entries-1]->entry->next = NULL;
  
  /* Free everything */

  for(i = 0; i < num_entries; i++)
    {
    free(s[i]->sort_string);
    free(s[i]);
    }
  free(s);
  bg_album_changed(album);
  }

typedef struct
  {
  bg_album_t * child;
  char * sort_string;
  } sort_children_struct;

void bg_album_sort_children(bg_album_t * album)
  {
  int num_children;
  int i, j;
  char * tmp_string;
  int sort_string_len;
  sort_children_struct * s_tmp;
  int keep_going;
  
  sort_children_struct ** s;
  bg_album_t * tmp_child;
  
  /* 1. Count the children */
  
  num_children = 0;

  tmp_child = album->children;

  while(tmp_child)
    {
    tmp_child = tmp_child->next;
    num_children++;
    }

  if(!num_children)
    return;
  
  /* Set up the album array */

  s = malloc(num_children * sizeof(*s));

  tmp_child = album->children;
  
  for(i = 0; i < num_children; i++)
    {
    s[i] = calloc(1, sizeof(*(s[i])));
    s[i]->child = tmp_child;

    /* Set up the sort string */

    tmp_string = bg_utf8_to_system(tmp_child->name,
                                   strlen(tmp_child->name));

    sort_string_len = strxfrm(NULL, tmp_string, 0);
    s[i]->sort_string = malloc(sort_string_len+1);
    strxfrm(s[i]->sort_string, tmp_string, sort_string_len+1);

    free(tmp_string);
    
    /* Advance */
    
    tmp_child = tmp_child->next;
    }

  /* Now, do a braindead bubblesort algorithm */

  for(i = 0; i < num_children - 1; i++)
    {
    keep_going = 0;
    for(j = num_children-1; j > i; j--)
      {
      if(strcmp(s[j]->sort_string, s[j-1]->sort_string) < 0)
        {
        s_tmp  = s[j];
        s[j]   = s[j-1];
        s[j-1] = s_tmp;
        keep_going = 1;
        }
      }
    if(!keep_going)
      break;
    }
  
  /* Rechain children */

  album->children = s[0]->child;

  for(i = 0; i < num_children-1; i++)
    {
    s[i]->child->next = s[i+1]->child;
    }
  
  s[num_children-1]->child->next = NULL;
  
  /* Free everything */

  for(i = 0; i < num_children; i++)
    {
    free(s[i]->sort_string);
    free(s[i]);
    }
  free(s);
  }

/* END */

void bg_album_rename_track(bg_album_t * album,
                           const bg_album_entry_t * entry_c,
                           const char * name)
  {
  bg_album_entry_t * entry;

  entry = album->entries;

  while(entry)
    {
    if(entry == entry_c)
      break;
    entry = entry->next;
    }
  entry->name = gavl_strrep(entry->name, name);
  
  if(entry->name_w)
    {
    free(entry->name_w);
    entry->name_w = NULL;
    entry->len_w = 0;
    }
  bg_album_entry_changed(album, entry);

  }

void bg_album_rename(bg_album_t * a, const char * name)
  {
  a->name = gavl_strrep(a->name, name);

  if(((a->type == BG_ALBUM_TYPE_REMOVABLE) ||
      (a->type == BG_ALBUM_TYPE_TUNER)) &&
     a->plugin_info)
    {
    bg_plugin_registry_set_device_name(a->com->plugin_reg,
                                       a->plugin_info->name, a->device,
                                       name);
    }
  if(a->name_change_callback)
    a->name_change_callback(a, name, a->name_change_callback_data);

  }


bg_album_entry_t * bg_album_get_current_entry(bg_album_t * a)
  {
  return a->com->current_entry;
  }

int bg_album_next(bg_album_t * a, int wrap)
  {
  if(a->com->current_entry)
    {
    if(!a->com->current_entry->next)
      {
      if(wrap)
        {
        if(a->com->set_current_callback)
          a->com->set_current_callback(a->com->set_current_callback_data,
                                       a, a->entries);
        return 1;
        }
      else
        return 0;
      }
    else
      {
      if(a->com->set_current_callback)
        a->com->set_current_callback(a->com->set_current_callback_data,
                                     a, a->com->current_entry->next);
      return 1;
      }
    }
  else
    return 0;
  }


gavl_time_t bg_album_get_duration(bg_album_t * a)
  {
  gavl_time_t ret = 0;
  bg_album_entry_t * e;
  e = a->entries;
  while(e)
    {
    if(e->duration == GAVL_TIME_UNDEFINED)
      return GAVL_TIME_UNDEFINED;
    else
      ret += e->duration;
    e = e->next;
    }
  return ret;
  }


int bg_album_get_index(bg_album_t* a, const bg_album_entry_t * entry)
  {
  int index = 0;
  const bg_album_entry_t * e;
  e = a->entries;
  while(1)
    {
    if(e == entry)
      return index;

    index++;
    e = e->next;

    if(!e)
      break;
    }
  return -1;
  }


int bg_album_previous(bg_album_t * a, int wrap)
  {
  bg_album_entry_t * tmp_entry;
  
  if(!a->com->current_entry)
    return 0;
    
  if(a->com->current_entry == a->entries)
    {
    if(!wrap)
      return 0;
    tmp_entry = a->entries; 

    while(tmp_entry->next)
      tmp_entry = tmp_entry->next;
    if(a->com->set_current_callback)
      a->com->set_current_callback(a->com->set_current_callback_data,
                                   a, tmp_entry);
    return 1;
    }
  else
    {
    tmp_entry = a->entries; 
    while(tmp_entry->next != a->com->current_entry)
      tmp_entry = tmp_entry->next;
    if(a->com->set_current_callback)
      a->com->set_current_callback(a->com->set_current_callback_data,
                                   a, tmp_entry);
    return 1;    
    }
  }

void bg_album_set_change_callback(bg_album_t * a,
                                  void (*change_callback)(bg_album_t * a, void * data),
                                  void * change_callback_data)
  {
  a->change_callback      = change_callback;
  a->change_callback_data = change_callback_data;
  }

void bg_album_set_entry_change_callback(bg_album_t * a,
                                        void (*change_callback)(bg_album_t * a,
                                                                const bg_album_entry_t * e,
                                                                void * data),
                                        void * change_callback_data)
  {
  a->entry_change_callback      = change_callback;
  a->entry_change_callback_data = change_callback_data;
  }

void bg_album_set_current_change_callback(bg_album_t * a,
                                          void (*change_callback)(bg_album_t * a,
                                                                  const bg_album_entry_t * e,
                                                                  void * data),
                                          void * change_callback_data)
  {
  a->current_change_callback      = change_callback;
  a->current_change_callback_data = change_callback_data;
  }

void bg_album_set_delete_callback(bg_album_t * a,
                                  void (*delete_callback)(bg_album_t * current_album,
                                                          int * indices, void * data),
                                  void * delete_callback_data)
  {
  a->delete_callback      = delete_callback;
  a->delete_callback_data = delete_callback_data;
  }

void bg_album_set_insert_callback(bg_album_t * a,
                                  void (*insert_callback)(bg_album_t * current_album,
                                                          int start, int num, void * data),
                                  void * insert_callback_data)
  {
  a->insert_callback      = insert_callback;
  a->insert_callback_data = insert_callback_data;
  }



void bg_album_set_name_change_callback(bg_album_t * a,
                                       void (*name_change_callback)(bg_album_t * a,
                                                                    const char * name,
                                                                    void * data),
                                       void * name_change_callback_data)
  {
  a->name_change_callback      = name_change_callback;
  a->name_change_callback_data = name_change_callback_data;
  }

void bg_album_changed(bg_album_t * a)
  {
  if(a->change_callback)
    a->change_callback(a, a->change_callback_data);
  }

void bg_album_current_changed(bg_album_t * a)
  {
  if(a->current_change_callback)
    a->current_change_callback(a->com->current_album, a->com->current_entry,
                               a->current_change_callback_data);
  }

void bg_album_entry_changed(bg_album_t * a, const bg_album_entry_t * e)
  {
  if(a->entry_change_callback)
    a->entry_change_callback(a->com->current_album, e,
                             a->entry_change_callback_data);
  }

void bg_album_set_current(bg_album_t * a, const bg_album_entry_t * e)
  {
  bg_album_entry_t * tmp_entry;
    
  tmp_entry = a->entries;
  while(tmp_entry != e)
    tmp_entry = tmp_entry->next;
  
  if(a->com->set_current_callback)
    a->com->set_current_callback(a->com->set_current_callback_data,
                                 a, tmp_entry);
  //  bg_album_current_changed(a);
  }


void bg_album_play(bg_album_t * a)
  {
  if(a->com->play_callback)
    a->com->play_callback(a->com->play_callback_data);
  }

bg_cfg_section_t * bg_album_get_cfg_section(bg_album_t * album)
  {
  return album->cfg_section;
  }


int bg_album_is_current(bg_album_t * a)
  {
  return (a == a->com->current_album) ? 1 : 0;
  }

int bg_album_entry_is_current(bg_album_t * a,
                              bg_album_entry_t * e)
  {
  return ((a == a->com->current_album) &&
          (e == a->com->current_entry)) ? 1 : 0; 
  }

bg_plugin_registry_t * bg_album_get_plugin_registry(bg_album_t * a)
  {
  return a->com->plugin_reg;
  }

void bg_album_get_times(bg_album_t * a,
                        gavl_time_t * duration_before,
                        gavl_time_t * duration_current,
                        gavl_time_t * duration_after)
  {
  bg_album_entry_t * e;

  if(a != a->com->current_album)
    {
    *duration_before = GAVL_TIME_UNDEFINED;
    *duration_current = GAVL_TIME_UNDEFINED;
    *duration_after = GAVL_TIME_UNDEFINED;
    return;
    }

  e = a->entries;
  *duration_before = 0;
  while(e != a->com->current_entry)
    {
    if(e->duration == GAVL_TIME_UNDEFINED)
      {
      *duration_before = GAVL_TIME_UNDEFINED;
      break;
      }
    *duration_before += e->duration;
    e = e->next;
    }

  *duration_current = a->com->current_entry->duration;
  
  *duration_after = 0;

  e = a->com->current_entry->next;
  while(e)
    {
    if(e->duration == GAVL_TIME_UNDEFINED)
      {
      *duration_after = GAVL_TIME_UNDEFINED;
      break;
      }
    *duration_after += e->duration;
    e = e->next;
    }
  }

void bg_album_set_error(bg_album_t * a, int err)
  {
  if(err)
    a->flags |= BG_ALBUM_ERROR;
  else
    a->flags &= ~BG_ALBUM_ERROR;
  }

int  bg_album_get_error(bg_album_t * a)
  {
  return !!(a->flags & BG_ALBUM_ERROR);
  }

void bg_album_append_child(bg_album_t * parent, bg_album_t * child)
  {
  bg_album_t * album_before;
  if(parent->children)
    {
    album_before = parent->children;
    while(album_before->next)
      album_before = album_before->next;
    album_before->next = child;
    }
  else
    parent->children = child;
  }

static void add_device(bg_album_t * album,
                       const char * device,
                       const char * name)
  {
  bg_album_t * device_album;
  bg_album_type_t type = BG_ALBUM_TYPE_REGULAR;
  if(album->plugin_info->flags & BG_PLUGIN_REMOVABLE)
    type = BG_ALBUM_TYPE_REMOVABLE;
  else if(album->plugin_info->flags & BG_PLUGIN_TUNER)
    type = BG_ALBUM_TYPE_TUNER;    	
  
  device_album = bg_album_create(album->com, type, album);
 
  device_album->device = gavl_strrep(device_album->device, device);
  if(name)
    {
    device_album->name = gavl_strrep(device_album->name,
                                   name);
    }
  else
    {
    device_album->name = gavl_strrep(device_album->name,
                                   device);
    }

  device_album->plugin_info = album->plugin_info;
  bg_album_append_child(album, device_album);
  }

void bg_album_add_device(bg_album_t * album,
                         const char * device,
                         const char * name)
  {
  add_device(album, device, name);
  bg_plugin_registry_add_device(album->com->plugin_reg,
                                album->plugin_info->name,
                                device, name);
  }

static bg_album_t *
remove_from_list(bg_album_t * list, bg_album_t * album, int * index)
  {
  bg_album_t * sibling_before;

  *index = 0;
  
  if(album == list)
    return album->next;
  else
    {
    sibling_before = list;
    (*index)++;
    while(sibling_before->next != album)
      {
      sibling_before = sibling_before->next;
      (*index)++;
      }
    sibling_before->next = album->next;
    return list;
    }
  }

void bg_album_remove_from_parent(bg_album_t * album)
  {
  int index;
  if(!album->parent)
    return;
  
  album->parent->children = remove_from_list(album->parent->children, album, &index);

  
  if(album->type == BG_ALBUM_TYPE_REMOVABLE)
    {
    bg_plugin_registry_remove_device(album->com->plugin_reg,
                                     album->plugin_info->name,
                                     album->plugin_info->devices[index].device,
                                     album->plugin_info->devices[index].name);
    }
  }

void bg_album_set_devices(bg_album_t * a)
  {
  bg_album_t * tmp_album;
  int j;

  /* Delete previous children */
  while(a->children)
    {
    tmp_album = a->children->next;
    bg_album_destroy(a->children);
    a->children = tmp_album;
    }
  
  if(a->plugin_info->devices && a->plugin_info->devices->device)
    {
    j = 0;
    
    while(a->plugin_info->devices[j].device)
      {
      add_device(a, a->plugin_info->devices[j].device,
                 a->plugin_info->devices[j].name);
      j++;
      } /* Device loop */
    }

  }

void bg_album_find_devices(bg_album_t * a)
  {
  bg_plugin_registry_find_devices(a->com->plugin_reg, a->plugin_info->name);
  bg_album_set_devices(a);
  }

static bg_album_entry_t * remove_redirectors(bg_album_t * album,
                                             bg_album_entry_t * entries)
  {
  bg_album_entry_t * before;
  bg_album_entry_t * e;
  bg_album_entry_t * new_entry, * end_entry;
  int done = 0;
  const bg_plugin_info_t * info;
  const char * name;
  
  done = 1;
  e = entries;
  
  while(e)
    {
    if(e->flags & BG_ALBUM_ENTRY_REDIRECTOR)
      {
      /* Load "real" url */
      if(e->plugin)
        {
        info = bg_plugin_find_by_name(album->com->plugin_reg,
                                      e->plugin);
          
        name = info->name;
        }
      else
        name = NULL;
        
      new_entry = bg_album_load_url(album,
                                    e->location,
                                    name, 0);
      if(new_entry)
        {
        /* Insert new entries into list */
        if(e != entries)
          {
          before = entries;
          while(before->next != e)
            before = before->next;
          before->next = new_entry;
          }
        else
          {
          entries = new_entry;
          }
        end_entry = new_entry;
        while(end_entry->next)
          {
          /* Set plugin so we don't have to set it next time */
          end_entry->plugin = gavl_strrep(end_entry->plugin, album->com->load_handle->info->name);
          end_entry = end_entry->next;
          }
        /* Set plugin so we don't have to set it next time */
        end_entry->plugin = gavl_strrep(end_entry->plugin, album->com->load_handle->info->name);
        end_entry->next = e->next;
        bg_album_entry_destroy(e);
        e = new_entry;
        }
      else
        {
        /* Remove e from list */
        if(e != entries)
          {
          before = entries;
          while(before->next != e)
            before = before->next;
          before->next = e->next;
          }
        else
          {
          entries = e->next;
          before = NULL;
          }
        bg_album_entry_destroy(e);
        e = (before) ? before->next : entries;
        }
      }
    else
      {
      /* Leave e as it is */
      e = e->next;
      }
    }

  return entries;
  }

static int is_blacklisted(bg_album_common_t * com,
                          const char * url)
  {
  const char * pos;
  if(!com->blacklist) // No blacklist
    return 0;
  if(strncmp(url, "file:", 5) && (*url != '/'))  // Remote file
    return 0;

  pos = strrchr(url, '.');
  if(pos)
    {
    pos++;
    if(bg_string_match(pos, com->blacklist))
      {
      bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Not loading %s (blacklisted extension)", url);
      return 1;
      }
    }
  
  pos = strrchr(url, '/');
  if(pos)
    {
    pos++;
    if(bg_string_match(pos, com->blacklist_files))
      {
      bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Not loading %s (blacklisted filename)", url);
      return 1;
      }
    }
  return 0;
  }

bg_album_entry_t * bg_album_load_url(bg_album_t * album,
                                     char * url,
                                     const char * plugin_name,
                                     int prefer_edl)
  {
  int i, num_entries;
  
  bg_album_entry_t * new_entry;
  bg_album_entry_t * end_entry = NULL;
  bg_album_entry_t * ret       = NULL;
  
  //  char * system_location;

  bg_input_plugin_t * plugin;
  bg_track_info_t * track_info;
  const bg_plugin_info_t * info;
  //  const char * file_plugin_name;

  if(is_blacklisted(album->com, url))
    {
    return NULL;
    }
  
  bg_log(BG_LOG_INFO, LOG_DOMAIN, "Loading %s", url);
  
  /* Load the appropriate plugin */

  if(plugin_name)
    {
    info = bg_plugin_find_by_name(album->com->plugin_reg,
                                  plugin_name);
    }
  else
    info = NULL;

  bg_album_common_prepare_callbacks(album->com, NULL);
  
  if(!bg_input_plugin_load(album->com->plugin_reg,
                           url, info,
                           &album->com->load_handle,
                           &album->com->input_callbacks, prefer_edl))
    {
    bg_log(BG_LOG_WARNING, LOG_DOMAIN, "Loading %s failed", url);
    return NULL;
    }
  plugin = (bg_input_plugin_t*)(album->com->load_handle->plugin);
  
  /* Open the track */
  
  if(!plugin->get_num_tracks)
    num_entries = 1;
  else
    num_entries = plugin->get_num_tracks(album->com->load_handle->priv);
  
  for(i = 0; i < num_entries; i++)
    {
    track_info = plugin->get_track_info(album->com->load_handle->priv, i);
    
    new_entry = bg_album_entry_create();
    //    new_entry->location = bg_system_to_utf8(url, strlen(url));
    new_entry->location = gavl_strrep(new_entry->location, url);
    new_entry->index = i;
    new_entry->total_tracks = num_entries;

    if(album->com->load_handle->edl)
      new_entry->flags |= BG_ALBUM_ENTRY_EDL;
    
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Loaded %s (track %d of %d)", url,
           new_entry->index+1, new_entry->total_tracks);
    
    bg_album_common_set_auth_info(album->com, new_entry);
    bg_album_update_entry(album, new_entry, track_info, 0, 1);

    new_entry->plugin = gavl_strrep(new_entry->plugin, plugin_name);
    
    if(ret)
      {
      end_entry->next = new_entry;
      end_entry = end_entry->next;
      }
    else
      {
      ret = new_entry;
      end_entry = ret;
      }
    
    }
  plugin->close(album->com->load_handle->priv);

  ret = remove_redirectors(album, ret);
  return ret;
  }

int bg_album_get_unique_id(bg_album_t * album)
  {
  album->com->highest_id++;
  return album->com->highest_id;
  }

static int refresh_entry(bg_album_t * album,
                         bg_album_entry_t * entry, gavl_edl_t * edl)
  {
  const bg_plugin_info_t * info;
  //  char * system_location;

  bg_input_plugin_t * plugin;
  bg_track_info_t * track_info;
  int i;
  /* Check, which plugin to use */

  if(entry->plugin)
    {
    info = bg_plugin_find_by_name(album->com->plugin_reg, entry->plugin);
    }
  else
    info = NULL;

  // system_location = bg_utf8_to_system(entry->location,
  //                                     strlen(entry->location));
  bg_album_common_prepare_callbacks(album->com, entry);
  if(!bg_input_plugin_load(album->com->plugin_reg,
                           entry->location,
                           info,
                           &album->com->load_handle, &album->com->input_callbacks,
                           !!(entry->flags & BG_ALBUM_ENTRY_EDL)))
    {
    entry->flags |= BG_ALBUM_ENTRY_ERROR;
    bg_album_entry_changed(album, entry);
    return 0;
    }
  
  plugin = (bg_input_plugin_t*)(album->com->load_handle->plugin);

  track_info = plugin->get_track_info(album->com->load_handle->priv,
                                      entry->index);

  bg_album_common_set_auth_info(album->com, entry);

  if(edl) /* Need timescales */
    {
    if(plugin->set_track)
      plugin->set_track(album->com->load_handle->priv, entry->index);

    if(plugin->set_audio_stream)
      {
      for(i = 0; i < track_info->num_audio_streams; i++)
        plugin->set_audio_stream(album->com->load_handle->priv,
                                 i, BG_STREAM_ACTION_DECODE);
      }
    if(plugin->set_video_stream)
      {
      for(i = 0; i < track_info->num_video_streams; i++)
        plugin->set_video_stream(album->com->load_handle->priv,
                                 i, BG_STREAM_ACTION_DECODE);
      }
    if(plugin->set_text_stream)
      {
      for(i = 0; i < track_info->num_text_streams; i++)
        plugin->set_text_stream(album->com->load_handle->priv,
                                i, BG_STREAM_ACTION_DECODE);
      }
    if(plugin->set_overlay_stream)
      {
      for(i = 0; i < track_info->num_overlay_streams; i++)
        plugin->set_overlay_stream(album->com->load_handle->priv,
                                   i, BG_STREAM_ACTION_DECODE);
      }
    
    if(plugin->start)
      plugin->start(album->com->load_handle->priv);
    bg_edl_append_track_info(edl, track_info, entry->location, entry->index,
                             entry->total_tracks, entry->name);
    }
  
  bg_album_update_entry(album, entry, track_info, 1, 1);
  plugin->close(album->com->load_handle->priv);
  bg_album_entry_changed(album, entry);
  return 1;
  }

void bg_album_refresh_selected(bg_album_t * album)
  {
  bg_album_entry_t * cur;
  cur = album->entries;
  while(cur)
    {
    if(cur->flags & BG_ALBUM_ENTRY_SELECTED)
      refresh_entry(album, cur, NULL);
    cur = cur->next;
    }
  }

gavl_edl_t * bg_album_selected_to_edl(bg_album_t * album)
  {
  gavl_edl_t * ret;
  bg_album_entry_t * cur;

  ret = gavl_edl_create();
  
  cur = album->entries;
  while(cur)
    {
    if(cur->flags & BG_ALBUM_ENTRY_SELECTED)
      refresh_entry(album, cur, ret);
    cur = cur->next;
    }
  return ret;
  }


void bg_album_copy_selected_to_favourites(bg_album_t * a)
  {
  int was_open;
  bg_album_entry_t * sel;
  sel = copy_selected(a);
  
  if(!bg_album_is_open(a->com->favourites))
    {
    bg_album_open(a->com->favourites);
    was_open = 0;
    }
  was_open = 1;
  
  bg_album_insert_entries_before(a->com->favourites, sel, NULL);
  
  if(!was_open)
    bg_album_close(a->com->favourites);
  }

void bg_album_move_selected_to_favourites(bg_album_t * a)
  {
  int was_open;
  bg_album_entry_t * sel;
  sel = extract_selected(a);
  
  if(!bg_album_is_open(a->com->favourites))
    {
    bg_album_open(a->com->favourites);
    was_open = 0;
    }
  was_open = 1;
  
  bg_album_insert_entries_before(a->com->favourites, sel, NULL);
  
  if(!was_open)
    bg_album_close(a->com->favourites);
  }

const char * bg_album_get_disc_name(bg_album_t * a)
  {
  return a->disc_name;
  }

char * bg_album_get_label(bg_album_t * a)
  {
  return a->disc_name ? a->disc_name : a->name;
  }


int bg_album_can_eject(bg_album_t * a)
  {
  /* Leave this disabled until ejecting really works */
  return !!(a->flags & BG_ALBUM_CAN_EJECT);
  //  return 0;
  }

void bg_album_eject(bg_album_t * a)
  {
  bg_input_plugin_t * plugin;
  bg_plugin_handle_t * handle;

  handle = bg_plugin_load(a->com->plugin_reg, a->plugin_info);
  plugin = (bg_input_plugin_t*)handle->plugin;
  
  if(!plugin->eject_disc(a->device))
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Ejecting disc failed");
  bg_plugin_unref(handle);
  }
   
char * bg_album_selected_to_string(bg_album_t * a)
  {
  char time_string[GAVL_TIME_STRING_LEN];
  bg_album_entry_t * entry;
  char * ret = NULL;
  char * tmp_string;
  int index = 1;
  entry = a->entries;
  while(entry)
    {
    if(entry->flags & BG_ALBUM_ENTRY_SELECTED)
      {
      if(ret)
        ret = gavl_strcat(ret, "\n");
      gavl_time_prettyprint(entry->duration, time_string);
      tmp_string = bg_sprintf("%d.\t%s\t%s", index, entry->name, time_string);
      ret = gavl_strcat(ret, tmp_string);
      free(tmp_string);
      }
    entry = entry->next;
    index++;
    }
  return ret;
  }

void bg_album_select_entry(bg_album_t * a, int entry)
  {
  bg_album_entry_t * e;
  e = bg_album_get_entry(a, entry);
  e->flags |= BG_ALBUM_ENTRY_SELECTED;
  }

void bg_album_unselect_entry(bg_album_t * a, int entry)
  {
  bg_album_entry_t * e;
  e = bg_album_get_entry(a, entry);
  e->flags &= ~BG_ALBUM_ENTRY_SELECTED;
  }

void bg_album_unselect_all(bg_album_t * a)
  {
  bg_album_entry_t * e;
  e = a->entries;
  while(e)
    {
    e->flags &= ~BG_ALBUM_ENTRY_SELECTED;
    e = e->next;
    }
  }

int bg_album_num_selected(bg_album_t * a)
  {
  bg_album_entry_t * e;
  int ret = 0;
  e = a->entries;
  while(e)
    {
    if(e->flags & BG_ALBUM_ENTRY_SELECTED)
      ret++;
    e = e->next;
    }
  return ret;
  }

int bg_album_num_unsync(bg_album_t * a)
  {
  bg_album_entry_t * e;
  int ret = 0;
  e = a->entries;
  while(e)
    {
    if(!(e->flags & BG_ALBUM_ENTRY_SYNC))
      ret++;
    e = e->next;
    }
  return ret;
  }

int bg_album_entry_is_selected(bg_album_t * a, int entry)
  {
  bg_album_entry_t * e;
  e = bg_album_get_entry(a, entry);
  return !!(e->flags & BG_ALBUM_ENTRY_SELECTED);
  }


void bg_album_toggle_select_entry(bg_album_t * a, int entry)
  {
  bg_album_entry_t * e;
  e = bg_album_get_entry(a, entry);
  if(e->flags & BG_ALBUM_ENTRY_SELECTED)
    e->flags &= ~BG_ALBUM_ENTRY_SELECTED;
  else
    e->flags |= BG_ALBUM_ENTRY_SELECTED;
  }

void bg_album_select_entries(bg_album_t * a, int start, int end)
  {
  int i;
  int swp;
  bg_album_entry_t * e;

  if(end < start)
    {
    swp = end;
    end = start;
    start = swp;
    }
  
  e = bg_album_get_entry(a, start);
  for(i = start; i <= end; i++)
    {
    if(!e)
      {
      bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Invalid selection range given");
      return;
      }
    e->flags |= BG_ALBUM_ENTRY_SELECTED;
    e = e->next;
    }
  }

/*********************************
 * Seek support
 *********************************/

struct bg_album_seek_data_s
  {
  char * str;
  int ignore;
  int exact;
  
  int changed;

  struct
    {
    wchar_t * str;
    int len;
    int matched;
    } * substrings;
  int num_substrings;
  int substrings_alloc;
  
  int (*match_func)(const wchar_t *s1, const wchar_t *s2, size_t n);
  
  bg_charset_converter_t * cnv;
  };

bg_album_seek_data_t * bg_album_seek_data_create()
  {
  bg_album_seek_data_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->cnv = bg_charset_converter_create("UTF-8", "WCHAR_T");
  return ret;
  }

void bg_album_seek_data_set_string(bg_album_seek_data_t * d, const char * str)
  {
  if(d->str && !strcmp(d->str, str))
    return;
  d->str = gavl_strrep(d->str, str);
  d->changed = 1;
  }

void bg_album_seek_data_ignore_case(bg_album_seek_data_t * d, int ignore)
  {
  if(d->ignore == ignore)
    return;
  d->ignore = ignore;
  d->changed = 1;
  }

void bg_album_seek_data_exact_string(bg_album_seek_data_t * d, int exact)
  {
  if(d->exact == exact)
    return;
  d->exact = exact;
  d->changed = 1;
  }

static int match_string_ignore(const wchar_t *s1, const wchar_t *s2, size_t n)
  {
  int i;
  for(i = 0; i < n; i++)
    {
    if(!(*s1) || !(*s2) || (towlower(*s1) != towlower(*s2)))
      return 0;
    s1++;
    s2++;
    }
  return 1;
  }

static int match_string(const wchar_t *s1, const wchar_t *s2, size_t n)
  {
  int i;
  for(i = 0; i < n; i++)
    {
    if(!(*s1) || !(*s2) || (*s1 != *s2))
      return 0;
    s1++;
    s2++;
    }
  return 1;
  }

static void update_seek_data(bg_album_seek_data_t * d)
  {
  int i;
  char ** substrings;
  char ** substrings_d = NULL;
  
  char * substrings_s[2];

  
  if(d->exact)
    {
    d->num_substrings = 1;
    substrings_s[0] = d->str;
    substrings_s[1] = NULL;
    substrings = substrings_s;
    }
  else
    {
    substrings_d = bg_strbreak(d->str, ' ');
    d->num_substrings = 0;
    while(substrings_d[d->num_substrings])
      d->num_substrings++;
    substrings = substrings_d;
    }

  if(d->num_substrings > d->substrings_alloc)
    {
    d->substrings = realloc(d->substrings,
                            d->num_substrings * sizeof(*d->substrings));
    memset(d->substrings + d->substrings_alloc,
           0, sizeof(*d->substrings) * (d->num_substrings - d->substrings_alloc));
    d->substrings_alloc = d->num_substrings;
    }

  for(i = 0; i < d->num_substrings; i++)
    {
    if(d->substrings[i].str)
      free(d->substrings[i].str);
    d->substrings[i].str =
      (wchar_t*)bg_convert_string(d->cnv,
                                  substrings[i], -1,
                                  &d->substrings[i].len);
    d->substrings[i].len /= sizeof(wchar_t);
    }
  
  if(d->ignore)
    d->match_func = match_string_ignore;
  else
    d->match_func = match_string;

  if(substrings_d)
    bg_strbreak_free(substrings_d);
  d->changed = 0;
  }

static int entry_matches(bg_album_entry_t * entry, bg_album_seek_data_t * d)
  {
  int i, j, keep_going;
  wchar_t * ptr;
  if(d->changed)
    update_seek_data(d);
  
  if(!entry->name_w)
    {
    entry->name_w = (wchar_t*)bg_convert_string(d->cnv,
                                                entry->name, -1,
                                                &entry->len_w);
    entry->len_w /= sizeof(wchar_t);
    }

  ptr = entry->name_w;

  for(j = 0; j < d->num_substrings; j++)
    d->substrings[j].matched = 0;
  
  keep_going = 1;
  
  for(i = 0; i < entry->len_w-1; i++)
    {
    keep_going = 0;
    for(j = 0; j < d->num_substrings; j++)
      {
      if(!d->substrings[j].matched)
        d->substrings[j].matched = d->match_func(d->substrings[j].str,
                                                 ptr, d->substrings[j].len);
      
      if(!d->substrings[j].matched)
        keep_going = 1;
      }
    if(!keep_going)
      break;
    ptr++;
    }
  if(keep_going)
    return 0;
  return 1;
  }


bg_album_entry_t * bg_album_seek_entry_after(bg_album_t * a,
                                             bg_album_entry_t * e,
                                             bg_album_seek_data_t * d)
  {
  if(!e)
    e = a->entries;
  else
    e = e->next;
  
  while(e)
    {
    if(entry_matches(e, d))
      return e;
    e = e->next;
    }
  return e;
  }

bg_album_entry_t * bg_album_seek_entry_before(bg_album_t * a,
                                              bg_album_entry_t * e,
                                              bg_album_seek_data_t * d)
  {
  bg_album_entry_t * cur;
  
  bg_album_entry_t * match = NULL;
  
  if(!e)
    {
    e = a->entries;
    while(e->next)
      e = e->next;
    if(entry_matches(e, d))
      return e;
    }
  
  cur = a->entries;
  while(1)
    {
    if(cur == e)
      return match;
    
    if(entry_matches(cur, d))
      {
      if(cur->next == e)
        return cur;
      match = cur;
      }
    cur = cur->next;
    if(!cur)
      break;
    }
  return NULL;
  }

void bg_album_seek_data_destroy(bg_album_seek_data_t * d)
  {
  int i;
  bg_charset_converter_destroy(d->cnv);

  if(d->str)
    free(d->str);

  for(i = 0; i < d->substrings_alloc; i++)
    {
    if(d->substrings[i].str)
      free(d->substrings[i].str);
    }
  if(d->substrings)
    free(d->substrings);
  
  free(d);
  }

int bg_album_seek_data_changed(bg_album_seek_data_t * d)
  {
  return d->changed;
  }

bg_album_entry_t * bg_album_entry_copy(bg_album_t * a, bg_album_entry_t * e)
  {
  bg_album_entry_t * ret;
  /* Also sets unique ID */
  ret = bg_album_entry_create();

  ret->name = gavl_strrep(ret->name, e->name);
  ret->location = gavl_strrep(ret->location, e->location);
  ret->plugin = gavl_strrep(ret->plugin, e->plugin);
  ret->duration = e->duration;

  ret->num_audio_streams = e->num_audio_streams;
  ret->num_still_streams = e->num_still_streams;
  ret->num_video_streams = e->num_video_streams;
  ret->num_subtitle_streams = e->num_subtitle_streams;
  
  /*
   *  Track index for multi track files/plugins
   */
  
  ret->index = e->index; 
  ret->total_tracks = e->total_tracks;
                          
  /* Authentication data */

  ret->username = gavl_strrep(ret->username, e->username);
  ret->password = gavl_strrep(ret->password, e->password);
  
  ret->flags = e->flags;
  /* Clear selected bit */
  ret->flags &= ~(BG_ALBUM_ENTRY_SELECTED);
  
  /* The wchar stuff will be rebuilt on demand */
                                 
  return ret;
  }

void bg_album_set_watch_dir(bg_album_t * a,
                            const char * dir)
  {
  char * pos;
  a->watch_dir = gavl_strrep(a->watch_dir, dir);
  pos = a->watch_dir + strlen(a->watch_dir) - 1;
  if(*pos == '\n')
    *pos = '\0';
  }
