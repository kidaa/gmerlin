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

#include <config.h>

#define _GNU_SOURCE
#include <time.h>
#include <sqlite3.h>
#include <inttypes.h>

#include <gmerlin/pluginregistry.h>

/* Utilits functions */

#define MY_FREE(ptr) \
  if(ptr) \
    free(ptr);

#define SET_QUERY_STRING(col, val)   \
  if(!strcasecmp(azColName[i], col)) \
    ret->val = bg_strdup(ret->val, argv[i]);

#define SET_QUERY_INT(col, val)      \
  if(!strcasecmp(azColName[i], col) && argv[i]) \
    ret->val = strtoll(argv[i], NULL, 10);

int
bg_sqlite_exec(sqlite3 * db,                              /* An open database */
               const char *sql,                           /* SQL to be evaluated */
               int (*callback)(void*,int,char**,char**),  /* Callback function */
               void * data);                              /* 1st argument to callback */


#define BG_NMJ_TIME_STRING_LEN 20
time_t bg_nmj_string_to_time(const char * str);
void bg_nmj_time_to_string(time_t time, char * str);

char * bg_nmj_escape_string(const char * str);
char * bg_nmj_unescape_string(const char * str);

int64_t bg_nmj_string_to_id(sqlite3 * db,
                            const char * table,
                            const char * id_row,
                            const char * string_row,
                            const char * str);

char * bg_nmj_id_to_string(sqlite3 * db,
                           const char * table,
                           const char * string_row,
                           const char * id_row,
                           int64_t id);

int64_t bg_nmj_id_to_id(sqlite3 * db,
                        const char * table,
                        const char * dst_row,
                        const char * src_row,
                        int64_t id);
  
/* Directory scanning utility */

typedef struct
  {
  int checked;
  char * path;
  time_t time;
  int64_t size;
  } bg_nmj_file_t;

bg_nmj_file_t * bg_nmj_file_scan(const char * directory,
                                 const char * extensions, int64_t * size);

bg_nmj_file_t * bg_nmj_file_lookup(bg_nmj_file_t * files,
                                   const char * path);

void bg_nmj_file_destroy(bg_nmj_file_t * files);

/* Directory */

typedef struct
  {
  int64_t id;
  char * directory; // Relative path
  char * name;      // Absolute path??
  char * scan_time; // NULL?
  int64_t size;     // Sum of all file sizes in bytes
  int64_t category; // 40 for Music
  char * status;    // 3 (??)

  int found;
  } bg_nmj_dir_t;

void bg_nmj_dir_init(bg_nmj_dir_t*);
void bg_nmj_dir_free(bg_nmj_dir_t*);
void bg_nmj_dir_dump(bg_nmj_dir_t*);
int bg_nmj_dir_query(sqlite3*, bg_nmj_dir_t*);
int bg_nmj_dir_save(sqlite3*, bg_nmj_dir_t*);


/* Song structure */

typedef struct
  {
  /* SONGS */
  int64_t id;
  char * title;
  char * search_title;
  char * path;
  int64_t scan_dirs_id;
  int64_t folders_id;   // Unused? */
  char * runtime;
  char * format;
  char * lyric;
  int64_t rating;   // default: 0
  char * hash;      // Unused?
  int64_t size;
  char * bit_rate;
  int64_t track_position; // 1..
  char * release_date;    // YYYY-01-01
  char * create_time;     // 2012-03-21 23:43:16
  char * update_state;    // "2" or "5"
  char * filestatus;      // unused

  /* Secondary info */
  char * album;
  char * genre;
  char * artist;
  char * albumartist;

  int64_t album_id;
  int64_t artist_id;
  int64_t genre_id;
  
  int found;
  
  } bg_nmj_song_t;

void bg_nmj_song_free(bg_nmj_song_t * song);
void bg_nmj_song_init(bg_nmj_song_t * song);
void bg_nmj_song_dump(bg_nmj_song_t * song);

int bg_nmj_song_get_info(sqlite3 * db,
                         bg_plugin_registry_t * plugin_reg,
                         bg_nmj_dir_t * dir,
                         bg_nmj_file_t * file,
                         bg_nmj_song_t * song,
                         bg_nmj_song_t * old_song);

int bg_nmj_song_query(sqlite3 * db, bg_nmj_song_t * song);

int bg_nmj_song_add(sqlite3 * db, bg_nmj_song_t * song);
int bg_nmj_song_update(sqlite3 * db, bg_nmj_song_t * song);
int bg_nmj_song_delete(sqlite3 * db, bg_nmj_song_t * song);

/* Album */

typedef struct
  {
  int64_t id;
  char * title;
  char * search_title;
  char * total_item;
  char * release_date;
  char * update_state; // 3??
  } bg_nmj_album_t;

void bg_nmj_album_init(bg_nmj_album_t *);
void bg_nmj_album_free(bg_nmj_album_t *);
void bg_nmj_album_dump(bg_nmj_album_t *);
int bg_nmj_album_query(bg_nmj_album_t *);
int bg_nmj_album_save(bg_nmj_album_t *);

/* Master functions */

#define BG_NMJ_MEDIA_TYPE_AUDIO (1<<0)
#define BG_NMJ_MEDIA_TYPE_VIDEO (1<<1)
#define BG_NMJ_MEDIA_TYPE_IMAGE (1<<2)

int bg_nmj_add_directory(bg_plugin_registry_t * plugin_reg,
                         sqlite3 * db, const char * directory, int types);
int bg_nmj_remove_directory(sqlite3 * db, const char * directory, int types);

