/*****************************************************************
 
  msgqueue.c
 
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

#include <inttypes.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#include <parameter.h>
#include <streaminfo.h>
#include <msgqueue.h>

#include <utils.h>
#include <bgsocket.h>

#define TYPE_INT            0
#define TYPE_FLOAT          1
#define TYPE_POINTER        2
#define TYPE_POINTER_NOCOPY 3
#define TYPE_TIME           4

#define FLOAT_FRAC_BITS     16
#define FLOAT_FRAC_FACTOR   ((float)(1<<(FLOAT_FRAC_BITS-1)))

struct bg_msg_s
  {
  uint32_t id;

  struct
    {
    union
      {
      int val_i;
      float val_f;
      void * val_ptr;
      gavl_time_t val_time;
      } value;
    uint8_t type;
    uint32_t size;
    } args[BG_MSG_MAX_ARGS];

  int num_args;

  sem_t produced;
  
  bg_msg_t * prev;
  bg_msg_t * next;
  };

void bg_msg_set_id(bg_msg_t * msg, int id)
  {
  msg->id = id;
  msg->num_args = 0;

  /* Zero everything */

  memset(&(msg->args), 0, sizeof(msg->args));
  
  }

static int check_arg(int arg)
  {
  if(arg < 0)
    return 0;
  if(arg > BG_MSG_MAX_ARGS-1)
    {
    fprintf(stderr, "Increase BG_MSG_MAX_ARGS in msgqueue.h to %d\n",
            arg+1);
    return 0;
    }
  return 1;
  }

/* Set argument to a basic type */

void bg_msg_set_arg_int(bg_msg_t * msg, int arg, int value)
  {
  if(!check_arg(arg))
    return;
  msg->args[arg].value.val_i = value;
  msg->args[arg].type = TYPE_INT;
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  }

void bg_msg_set_arg_time(bg_msg_t * msg, int arg, gavl_time_t value)
  {
  if(!check_arg(arg))
    return;
  msg->args[arg].value.val_time = value;
  msg->args[arg].type = TYPE_TIME;
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  }


void * bg_msg_set_arg_ptr(bg_msg_t * msg, int arg, int len)
  {
  if(!check_arg(arg))
    return (char*)0;
  
  msg->args[arg].value.val_ptr = calloc(1, len);
  msg->args[arg].size = len;
  msg->args[arg].type = TYPE_POINTER;
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  return msg->args[arg].value.val_ptr;
  }

void bg_msg_set_arg_ptr_nocopy(bg_msg_t * msg, int arg, void * value)
  {
  if(!check_arg(arg))
    return;
  msg->args[arg].value.val_ptr = value;
  msg->args[arg].type = TYPE_POINTER_NOCOPY;
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  }


void bg_msg_set_arg_string(bg_msg_t * msg, int arg, const char * value)
  {
  int length;
  void * dst;
  if(!value)
    return;
  length = strlen(value)+1;
  dst = bg_msg_set_arg_ptr(msg, arg, length);
  memcpy(dst, value, length);
  
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  }

void bg_msg_set_arg_float(bg_msg_t * msg, int arg, float value)
  {
  if(!check_arg(arg))
    return;
  msg->args[arg].value.val_f = value;
  msg->args[arg].type = TYPE_FLOAT;
  if(arg+1 > msg->num_args)
    msg->num_args = arg + 1;
  }


/* Get basic types */

int bg_msg_get_id(bg_msg_t * msg)
  {
  return msg->id;
  }

int bg_msg_get_arg_int(bg_msg_t * msg, int arg)
  {
  if(!check_arg(arg))
    return 0;
  return msg->args[arg].value.val_i;
  }

gavl_time_t bg_msg_get_arg_time(bg_msg_t * msg, int arg)
  {
  if(!check_arg(arg))
    return 0;
  return msg->args[arg].value.val_time;
  }

float bg_msg_get_arg_float(bg_msg_t * msg, int arg)
  {
  if(!check_arg(arg))
    return 0.0;
  return msg->args[arg].value.val_f;
  }

void * bg_msg_get_arg_ptr(bg_msg_t * msg, int arg, int * length)
  {
  void * ret;
  
  if(!check_arg(arg))
    return (void*)0;

  ret = msg->args[arg].value.val_ptr;
  msg->args[arg].value.val_ptr = (void*)0;
  if(length)
    *length = msg->args[arg].size;
  return ret;
  }

void * bg_msg_get_arg_ptr_nocopy(bg_msg_t * msg, int arg)
  {
  if(!check_arg(arg))
    return NULL;
  return msg->args[arg].value.val_ptr;
  }

char * bg_msg_get_arg_string(bg_msg_t * msg, int arg)
  {
  char * ret;
  if(!check_arg(arg))
    return (char *)0;
  ret = msg->args[arg].value.val_ptr;
  msg->args[arg].value.val_ptr = (char*)0;
  return ret;
  }


/* Get/Set routines for structures */

static inline uint8_t * get_8(uint8_t * data, uint32_t * val)
  {
  *val = *data;
  data++;
  return data;
  }

static inline uint8_t * get_16(uint8_t * data, uint32_t * val)
  {
  *val = ((data[0] << 8) | data[1]);
  data+=2;
  return data;
  }

static inline uint8_t * get_32(uint8_t * data, uint32_t * val)
  {
  *val = ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
  data+=4;
  return data;
  }

static inline uint8_t * get_str(uint8_t * data, char ** val)
  {
  uint32_t len;
  uint8_t * ret;

  ret = get_32(data, &len);

  if(len)
    {
    *val = malloc(len+1);
    memcpy(*val, ret, len);
    (*val)[len] = '\0';
    }
  return ret + len;
  }

static inline uint8_t * set_8(uint8_t * data, uint8_t val)
  {
  *data = val;
  data++;
  return data;
  }

static inline uint8_t * set_16(uint8_t * data, uint16_t val)
  {
  data[0] = (val & 0xff00) >> 8;
  data[1] = (val & 0xff);
  data+=2;
  return data;
  }

static inline uint8_t * set_32(uint8_t * data, uint32_t val)
  {
  data[0] = (val & 0xff000000) >> 24;
  data[1] = (val & 0xff0000) >> 16;
  data[2] = (val & 0xff00) >> 8;
  data[3] = (val & 0xff);
  data+=4;
  return data;
  }

static int str_len(const char * str)
  {
  int ret = 4;
  if(str)
    ret += strlen(str);
  return ret;
  }

static inline uint8_t * set_str(uint8_t * data, const char * val)
  {
  uint32_t len;
  if(val)
    len = strlen(val);
  else
    len = 0;
  
  data = set_32(data, len);
  if(len)
    memcpy(data, val, len);
  return data + len;
  }

/*
  int samples_per_frame; 
  int samplerate;
  int num_channels;
  gavl_sample_format_t   sample_format;
  gavl_interleave_mode_t interleave_mode;
  gavl_channel_setup_t   channel_setup;
  int lfe;
*/

void bg_msg_set_arg_audio_format(bg_msg_t * msg, int arg,
                                 const gavl_audio_format_t * format)
  {
  uint8_t * ptr;
  uint8_t * pos;
  int i;
    
  ptr = bg_msg_set_arg_ptr(msg, arg, 24 + 8 * format->num_channels);
  
  pos = ptr;
  
  pos = set_32(pos, format->samples_per_frame);
  pos = set_32(pos, format->samplerate);
  pos = set_32(pos, format->num_channels);
  pos = set_8(pos, format->sample_format);
  pos = set_8(pos, format->interleave_mode);

  pos = set_32(pos, (int32_t)(format->center_level*1.0e6));
  pos = set_32(pos, (int32_t)(format->rear_level*1.0e6));
  
  for(i = 0; i < format->num_channels; i++)
    pos = set_8(pos, format->channel_locations[i]);

  //  msg->args[arg];
  
  }

void bg_msg_get_arg_audio_format(bg_msg_t * msg, int arg,
                                 gavl_audio_format_t * format)
  {
  uint8_t * ptr;
  uint8_t * pos;
  int i;
  uint32_t tmp;
  
  
  ptr = bg_msg_get_arg_ptr(msg, arg, NULL);
  
  pos = ptr;
  
  pos = get_32(pos, &(tmp));
  format->samples_per_frame = tmp;

  pos = get_32(pos, &(tmp));
  format->samplerate = tmp;
  
  pos = get_32(pos, &(tmp));
  format->num_channels = tmp;
  
  pos = get_8(pos,  &(tmp));
  format->sample_format = tmp;
  
  pos = get_8(pos,  &(tmp));
  format->interleave_mode = tmp;
  
  pos = get_32(pos, &tmp);
  format->center_level = (float)(tmp)*1.0e-6;

  pos = get_32(pos, &tmp);
  format->rear_level = (float)(tmp)*1.0e-6;
    
  for(i = 0; i < format->num_channels; i++)
    {
    pos = get_8(pos, &(tmp));
    format->channel_locations[i] = tmp;
    }
  
  free(ptr);
  }

/* Video format */

/*
  int frame_width;
  int frame_height;
  int image_width;
  int image_height;
  int pixel_width;
  int pixel_height;
  gavl_pixelformat_t pixelformat;
  int framerate_num;
  int frame_duration;
  
  int free_framerate;
*/

void bg_msg_set_arg_video_format(bg_msg_t * msg, int arg,
                                 const gavl_video_format_t * format)
  {
  uint8_t * ptr;
  uint8_t * pos;

  ptr = bg_msg_set_arg_ptr(msg, arg, 39);
  
  pos = ptr;
  
  pos = set_32(pos, format->frame_width);
  pos = set_32(pos, format->frame_height);
  pos = set_32(pos, format->image_width);
  pos = set_32(pos, format->image_height);
  pos = set_32(pos, format->pixel_width);
  pos = set_32(pos, format->pixel_height);
  pos = set_32(pos, format->pixelformat);
  pos = set_32(pos, format->timescale);
  pos = set_32(pos, format->frame_duration);
  pos = set_8(pos, format->framerate_mode);
  pos = set_8(pos, format->interlace_mode);
  pos = set_8(pos, format->chroma_placement);
  
  }

void bg_msg_get_arg_video_format(bg_msg_t * msg, int arg,
                                 gavl_video_format_t * format)
  {
  uint32_t tmp;
  uint8_t * ptr;
  uint8_t * pos;

  ptr = bg_msg_get_arg_ptr(msg, arg, NULL);
  
  pos = ptr;

  pos = get_32(pos, &(tmp)); format->frame_width      = tmp;
  pos = get_32(pos, &(tmp)); format->frame_height     = tmp;
  pos = get_32(pos, &(tmp)); format->image_width      = tmp;
  pos = get_32(pos, &(tmp)); format->image_height     = tmp;
  pos = get_32(pos, &(tmp)); format->pixel_width      = tmp;
  pos = get_32(pos, &(tmp)); format->pixel_height     = tmp;
  pos = get_32(pos, &(tmp)); format->pixelformat       = tmp;
  pos = get_32(pos, &(tmp)); format->timescale        = tmp;
  pos = get_32(pos, &(tmp)); format->frame_duration   = tmp;
  pos = get_8(pos,  &(tmp)); format->framerate_mode   = tmp;
  pos = get_8(pos,  &(tmp)); format->interlace_mode   = tmp;
  pos = get_8(pos,  &(tmp)); format->chroma_placement = tmp;
  
  free(ptr);
  
  }

/*
  char * artist;
  char * title;
  char * album;
      
  int track;
  char * date;
  char * genre;
  char * comment;

  char * author;
  char * copyright;
*/


void bg_msg_set_arg_metadata(bg_msg_t * msg, int arg,
                             const bg_metadata_t * m)
  {
  uint8_t * ptr;
  uint8_t * pos;

  int len = 4;
  len += str_len(m->artist);
  len += str_len(m->title);
  len += str_len(m->album);
  len += str_len(m->date);
  len += str_len(m->genre);
  len += str_len(m->comment);
  len += str_len(m->author);
  len += str_len(m->copyright);
  
  ptr = bg_msg_set_arg_ptr(msg, arg, len);
  pos = ptr;

  pos = set_str(pos, m->artist);
  pos = set_str(pos, m->title);
  pos = set_str(pos, m->album);
  pos = set_str(pos, m->date);
  pos = set_str(pos, m->genre);
  pos = set_str(pos, m->comment);
  pos = set_str(pos, m->author);
  pos = set_str(pos, m->copyright);
  pos = set_32(pos, m->track);
  }

void bg_msg_get_arg_metadata(bg_msg_t * msg, int arg,
                             bg_metadata_t * m)
  {
  uint32_t tmp;
  uint8_t * ptr;
  uint8_t * pos;

  ptr = bg_msg_get_arg_ptr(msg, arg, NULL);
  
  pos = ptr;

  pos = get_str(pos, &(m->artist));
  pos = get_str(pos, &(m->title));
  pos = get_str(pos, &(m->album));
  pos = get_str(pos, &(m->date));
  pos = get_str(pos, &(m->genre));
  pos = get_str(pos, &(m->comment));
  pos = get_str(pos, &(m->author));
  pos = get_str(pos, &(m->copyright));
  pos = get_32(pos,  &(tmp));
  m->track = tmp;
  
  free(ptr);
  }

bg_msg_t * bg_msg_create()
  {
  bg_msg_t * ret;
  ret = calloc(1, sizeof(*ret));
  
  sem_init(&(ret->produced), 0, 0);
    
  return ret;
  }

void bg_msg_free(bg_msg_t * m)
  {
  int i;
  for(i = 0; i < m->num_args; i++)
    {
    if((m->args[i].type == TYPE_POINTER) &&
       (m->args[i].value.val_ptr))
      {
      free(m->args[i].value.val_ptr);
      m->args[i].value.val_ptr = (void*)0;
      }
    }
  }

void bg_msg_destroy(bg_msg_t * m)
  {
  bg_msg_free(m);
  sem_destroy(&(m->produced));
  free(m);
  }


struct bg_msg_queue_s
  {
  bg_msg_t * msg_input;
  bg_msg_t * msg_output;
  bg_msg_t * msg_last;

  pthread_mutex_t chain_mutex;
  pthread_mutex_t write_mutex;
  
  int num_messages; /* Number of total messages */
  };

bg_msg_queue_t * bg_msg_queue_create()
  {
  bg_msg_queue_t * ret;
  ret = calloc(1, sizeof(*ret));
  
  /* Allocate at least 2 messages */
  
  ret->msg_output       = bg_msg_create();
  ret->msg_output->next = bg_msg_create();
  
  /* Set in- and output messages */

  ret->msg_input = ret->msg_output;
  ret->msg_last  = ret->msg_output->next;
  
  /* Initialize chain mutex */

  pthread_mutex_init(&(ret->chain_mutex),(pthread_mutexattr_t *)0);
  pthread_mutex_init(&(ret->write_mutex),(pthread_mutexattr_t *)0);
  
  return ret;
  }

void bg_msg_queue_destroy(bg_msg_queue_t * m)
  {
  bg_msg_t * tmp_message;
  while(m->msg_output)
    {
    tmp_message = m->msg_output->next;
    bg_msg_destroy(m->msg_output);
    m->msg_output = tmp_message;
    }
  free(m);
  }

/* Lock message queue for reading, block until something arrives */

bg_msg_t * bg_msg_queue_lock_read(bg_msg_queue_t * m)
  {
  sem_wait(&(m->msg_output->produced)); 
  return m->msg_output;

  // sem_ret->msg_output
  }

bg_msg_t * bg_msg_queue_try_lock_read(bg_msg_queue_t * m)
  {
  if(!sem_trywait(&(m->msg_output->produced)))
    return m->msg_output;
  else
    return (bg_msg_t*)0;
  }

int bg_msg_queue_peek(bg_msg_queue_t * m, uint32_t * id)
  {
  int sem_val;
  sem_getvalue(&(m->msg_output->produced), &sem_val);
  if(sem_val)
    {
    if(id)
      *id = m->msg_output->id;
    return 1;
    }
  else
    return 0;
  }

void bg_msg_queue_unlock_read(bg_msg_queue_t * m)
  {

  bg_msg_t * old_out_message;

  pthread_mutex_lock(&(m->chain_mutex));
  old_out_message = m->msg_output;
  
  bg_msg_free(old_out_message);
  
  m->msg_output = m->msg_output->next;
  m->msg_last->next = old_out_message;
  m->msg_last = m->msg_last->next;
  m->msg_last->next = (bg_msg_t*)0;

  pthread_mutex_unlock(&(m->chain_mutex));
  }

/*
 *  Lock queue for writing
 */

bg_msg_t * bg_msg_queue_lock_write(bg_msg_queue_t * m)
  {
  pthread_mutex_lock(&(m->write_mutex));
  return m->msg_input;
  }

void bg_msg_queue_unlock_write(bg_msg_queue_t * m)
  {
  bg_msg_t * message = m->msg_input;
    
  pthread_mutex_lock(&(m->chain_mutex));
  if(!m->msg_input->next)
    {
    m->msg_input->next = bg_msg_create();
    m->msg_last = m->msg_input->next;
    }
  
  m->msg_input = m->msg_input->next;
  sem_post(&(message->produced));
  pthread_mutex_unlock(&(m->chain_mutex));
  pthread_mutex_unlock(&(m->write_mutex));

  }

typedef struct list_entry_s
  {
  bg_msg_queue_t * q;
  struct list_entry_s * next;
  } list_entry_t;

struct bg_msg_queue_list_s
  {
  list_entry_t * entries;
  
  pthread_mutex_t mutex;
  };

bg_msg_queue_list_t * bg_msg_queue_list_create()
  {
  bg_msg_queue_list_t * ret = calloc(1, sizeof(*ret));
  pthread_mutex_init(&(ret->mutex),(pthread_mutexattr_t *)0);
  return ret;
  }

void bg_msg_queue_list_destroy(bg_msg_queue_list_t * l)
  {
  list_entry_t * tmp_entry;

  while(l->entries)
    {
    tmp_entry = l->entries->next;
    free(l->entries);
    l->entries = tmp_entry;
    }
  free(l);
  }

void 
bg_msg_queue_list_send(bg_msg_queue_list_t * l,
                       void (*set_message)(bg_msg_t * message,
                                           const void * data),
                       const void * data)
  {
  bg_msg_t * msg;
  list_entry_t * entry;
  
  pthread_mutex_lock(&(l->mutex));
  entry = l->entries;
  
  while(entry)
    {
    msg = bg_msg_queue_lock_write(entry->q);
    set_message(msg, data);
    bg_msg_queue_unlock_write(entry->q);
    entry = entry->next;
    }
  pthread_mutex_unlock(&(l->mutex));
  }

void bg_msg_queue_list_add(bg_msg_queue_list_t * list,
                        bg_msg_queue_t * queue)
  {
  list_entry_t * new_entry;

  new_entry = calloc(1, sizeof(*new_entry));
  
  pthread_mutex_lock(&(list->mutex));

  new_entry->next = list->entries;
  new_entry->q = queue;
  list->entries = new_entry;
  
  pthread_mutex_unlock(&(list->mutex));
  }

void bg_msg_queue_list_remove(bg_msg_queue_list_t * list,
                           bg_msg_queue_t * queue)
  {
  list_entry_t * tmp_entry;
  list_entry_t * entry_before;
  
  pthread_mutex_lock(&(list->mutex));

  if(list->entries->q == queue)
    {
    tmp_entry = list->entries->next;
    free(list->entries);
    list->entries = tmp_entry;
    }
  else
    {
    entry_before = list->entries;

    while(entry_before->next->q != queue)
      entry_before = entry_before->next;

    tmp_entry = entry_before->next;
    entry_before->next = tmp_entry->next;
    free(tmp_entry);
    }
  pthread_mutex_unlock(&(list->mutex));
  }

/*
 *  This on will be used for remote controls,
 *  return FALSE on error
 */

/* This is how a message is passed through pipes and sockets:
 *
 * content [   id  |nargs|<arg1><arg2>....
 * bytes   |0|1|2|3|  4  |5....
 *
 * An int argument is encoded as:
 * content | type | value |
 * bytes   | 0    |1|2|3|4|
 *
 * Type is TYPE_INT, byte order is big endian (network byte order)
 *
 *  Floating point arguments are coded with type TYPE_FLOAT and the
 *  value as an integer
 *
 *  String arguments are coded as:
 *  content |length |string including final '\0'|
 *  bytes   |0|1|2|3|4 .. 4 + len               |
 *
 *  Pointer messages cannot be transmited!
 */

static int read_uint32(int fd, uint32_t * ret, int milliseconds)
  {
  uint8_t buf[4];

  if(bg_socket_read_data(fd, buf, 4, milliseconds) < 4)
    return 0;
  
  //  bg_hexdump(buf, 4);
    
  *ret = (buf[0]<<24) | (buf[1]<<16) |
    (buf[2]<<8) | buf[3];
  return 1;
  }

static int read_time(int fd, gavl_time_t * ret, int milliseconds)
  {
  uint8_t buf[8];

  if(bg_socket_read_data(fd, buf, 8, milliseconds) < 4)
    return 0;

  *ret =
    ((gavl_time_t)(buf[0])<<56) |
    ((gavl_time_t)(buf[1])<<48) |
    ((gavl_time_t)(buf[2])<<40) |
    ((gavl_time_t)(buf[3])<<32) |
    ((gavl_time_t)(buf[4])<<24) |
    ((gavl_time_t)(buf[5])<<16) |
    ((gavl_time_t)(buf[6])<<8) |
    ((gavl_time_t)(buf[7]));
  return 1;
  }

static int write_uint32(int fd, uint32_t * val)
  {
  uint8_t buf[4];
  
  buf[0] = (*val & 0xff000000) >> 24;
  buf[1] = (*val & 0x00ff0000) >> 16;
  buf[2] = (*val & 0x0000ff00) >> 8;
  buf[3] = (*val & 0x000000ff);

  //  bg_hexdump(buf, 4);
    
  if(bg_socket_write_data(fd, buf, 4) < 4)
    return 0;
  return 1;
  }

static int write_time(int fd, gavl_time_t val)
  {
  uint8_t buf[8];

  buf[0] = (val & 0xff00000000000000LL) >> 56;
  buf[1] = (val & 0x00ff000000000000LL) >> 48;
  buf[2] = (val & 0x0000ff0000000000LL) >> 40;
  buf[3] = (val & 0x000000ff00000000LL) >> 32;

  buf[4] = (val & 0x00000000ff000000LL) >> 24;
  buf[5] = (val & 0x0000000000ff0000LL) >> 16;
  buf[6] = (val & 0x000000000000ff00LL) >> 8;
  buf[7] = (val & 0x00000000000000ffLL);
  
  if(bg_socket_write_data(fd, buf, 8) < 8)
    return 0;
  return 1;
  }

int bg_message_read_socket(bg_msg_t * ret, int fd, int milliseconds)
  {
  int i;
  void * ptr;
  
  int32_t val_i;

  gavl_time_t val_time;

  uint8_t val_u8;
  memset(ret, 0, sizeof(*ret));
  
  /* Message ID */

  if(!read_uint32(fd, (uint32_t*)(&val_i), 0))
    return 0;

  //  fprintf(stderr, "Read ID: %d\n", val_i);
  
  ret->id = val_i;

  /* Number of arguments */

  if(!bg_socket_read_data(fd, &val_u8, 1, 1))
    return 0;

  ret->num_args = val_u8;

  for(i = 0; i < ret->num_args; i++)
    {
    if(!bg_socket_read_data(fd, &val_u8, 1, 1))
      return 0;
    ret->args[i].type = val_u8;

    switch(ret->args[i].type)
      {
      case TYPE_INT:
        if(!read_uint32(fd, (uint32_t*)(&val_i), 1))
          return 0;

        ret->args[i].value.val_i = val_i;
        break;
      case TYPE_FLOAT:
        if(!read_uint32(fd, (uint32_t*)(&val_i), 1))
          return 0;
        ret->args[i].value.val_f = (float)val_i/FLOAT_FRAC_FACTOR;
        break;
      case TYPE_POINTER:
        if(!read_uint32(fd, (uint32_t*)(&val_i), 1)) /* Length */
          return 0;
        ptr = bg_msg_set_arg_ptr(ret, i, val_i);
        if(bg_socket_read_data(fd, ret->args[i].value.val_ptr,
                               val_i, 1) < val_i)
          {
          //          fprintf(stderr, "Reading pointer failed\n");
          return 0;
          }
        break;
      case TYPE_TIME:
        if(!read_time(fd, &val_time, 1))
          return 0;
        ret->args[i].value.val_time = val_time;
        break;
        
      case TYPE_POINTER_NOCOPY:
        break;
        
      }

    }
    
  return 1;
  }

int bg_message_write_socket(bg_msg_t * msg, int fd)
  {
  int i;
  uint8_t val_u8;
  int32_t i_tmp;
    
  /* Message id */

  if(!write_uint32(fd, &msg->id))
    return 0;

  /* Number of arguments */
  val_u8 = msg->num_args;
  
  if(!bg_socket_write_data(fd, &val_u8, 1))
    return 0;

  /* Arguments */

  for(i = 0; i < msg->num_args; i++)
    {
    bg_socket_write_data(fd, &(msg->args[i].type), 1);

    switch(msg->args[i].type)
      {
      case TYPE_INT:
        if(!write_uint32(fd, (uint32_t*)(&msg->args[i].value.val_i)))
          return 0;
        break;
      case TYPE_TIME:
        if(!write_time(fd, msg->args[i].value.val_time))
          return 0;
        break;
      case TYPE_FLOAT:
        i_tmp = (int32_t)(msg->args[i].value.val_f *
                      FLOAT_FRAC_FACTOR + 0.5);
        
        if(!write_uint32(fd, (uint32_t*)(&i_tmp)))
          return 0;
        break;
      case TYPE_POINTER:
        if(!write_uint32(fd, &(msg->args[i].size)))
          return 0;
        if(bg_socket_write_data(fd, msg->args[i].value.val_ptr,
                 msg->args[i].size) < msg->args[i].size)
          return 0;
        break;
      case TYPE_POINTER_NOCOPY:
        break;
      }
    
    }
  return 1;
  }
