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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>



#include <config.h>
#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/utils.h>

#include "v4l2_common.h"

#define TIME_DIV 1000

#include <gmerlin/log.h>
#define LOG_DOMAIN "i_v4l2"

#ifdef HAVE_V4LCONVERT
#include "convert.h"
#endif

#define CLEAR(x) memset (&(x), 0, sizeof (x))

/* Input module */

typedef struct
  {
  void *                  start;
  size_t                  length;
  } buffer_t;

typedef struct
  {
  bg_parameter_info_t * parameters;
  char *           device;
  bgv4l2_io_method_t	io;
  int              fd;
  buffer_t *         buffers;
  unsigned int     n_buffers;
  gavl_video_frame_t * frame;
  gavl_video_format_t format;
  
  uint32_t v4l2_pixelformat;
  
  int user_width;
  int user_height;
  int user_resolution;

  int width;
  int height;
  
  struct v4l2_format fmt;

  struct v4l2_queryctrl * controls;
  int num_controls;
  gavl_timer_t * timer;
#ifdef HAVE_V4LCONVERT
  bg_v4l2_convert_t * converter;
#endif
  } v4l2_t;


static int
init_read(v4l2_t * v4l)
  {
  v4l->buffers = calloc (1, sizeof (*v4l->buffers));
  
  if (!v4l->buffers)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Out of memory");
    return 0;
    }
  
  v4l->buffers[0].length = v4l->fmt.fmt.pix.sizeimage;
  v4l->buffers[0].start = malloc(v4l->fmt.fmt.pix.sizeimage);
  
  if (!v4l->buffers[0].start)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Out of memory");
    return 0;
    }
  return 1;
  }

static int
init_mmap(v4l2_t * v4l)
  {
  struct v4l2_requestbuffers req;
  int i;
  enum v4l2_buf_type type;
  
  CLEAR (req);
  
  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_MMAP;
  
  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_REQBUFS, &req))
    {
    if (EINVAL == errno)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "%s does not support "
               "memory mapping", v4l->device);
      return 0;
      }
    else
      {
      return 0;
      }
    }
  
  if (req.count < 2)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Insufficient buffer memory on %s",
             v4l->device);
    return 0;
    }
  
  v4l->buffers = calloc (req.count, sizeof (*v4l->buffers));

  if (!v4l->buffers)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Out of memory");
    return 0;
    }
  
  for (v4l->n_buffers = 0; v4l->n_buffers < req.count; ++v4l->n_buffers)
    {
    struct v4l2_buffer buf;
    
    CLEAR (buf);
    
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = v4l->n_buffers;
    
    if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_QUERYBUF, &buf))
      return 0;
    
    v4l->buffers[v4l->n_buffers].length = buf.length;
    v4l->buffers[v4l->n_buffers].start =
      mmap (NULL /* start anywhere */,
            buf.length,
            PROT_READ | PROT_WRITE /* required */,
            MAP_SHARED /* recommended */,
            v4l->fd, buf.m.offset);
    
    if (MAP_FAILED == v4l->buffers[v4l->n_buffers].start)
      return 0;
    }

  for (i = 0; i < v4l->n_buffers; ++i)
    {
    struct v4l2_buffer buf;
    CLEAR (buf);
    
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = i;
    
    if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_QBUF, &buf))
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "VIDIOC_QBUF failed: %s", strerror(errno));
      return 0;
      }
    }
  
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_STREAMON, &type))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "VIDIOC_STREAMON failed: %s", strerror(errno));
    return 0;
    }
  return 1;
  }

static int
init_userp(v4l2_t * v4l, unsigned int		buffer_size)
  {
  struct v4l2_requestbuffers req;
  unsigned int page_size;

  page_size = getpagesize ();
  buffer_size = (buffer_size + page_size - 1) & ~(page_size - 1);

  CLEAR (req);

  req.count               = 4;
  req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory              = V4L2_MEMORY_USERPTR;

  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_REQBUFS, &req))
    {
    if (EINVAL == errno)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "%s does not support "
               "user pointer i/o", v4l->device);
      return 0;
      }
    else
      {
      return 0;
      }
    }
  
  v4l->buffers = calloc (4, sizeof (*v4l->buffers));

  if (!v4l->buffers)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Out of memory");
    return 0;
    }
  
  for (v4l->n_buffers = 0; v4l->n_buffers < 4; ++v4l->n_buffers)
    {
    v4l->buffers[v4l->n_buffers].length = buffer_size;
    v4l->buffers[v4l->n_buffers].start = memalign (/* boundary */ page_size,
                                                   buffer_size);
    
    if (!v4l->buffers[v4l->n_buffers].start)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Out of memory");
      return 0;
      }
    }
  return 1;
  }

static int get_pixelformat(int fd, uint32_t * ret)
  {
  int index = 0;
  struct v4l2_fmtdesc desc;
  desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; 
  
  while(1)
    {
    desc.index = index;
    if(-1 == bgv4l2_ioctl (fd, VIDIOC_ENUM_FMT, &desc))
      return 0;
#if 0
    fprintf(stderr, "Cam pixelformat %c%c%c%c\n",
            desc.pixelformat & 0xff,
            (desc.pixelformat >> 8) & 0xff,
            (desc.pixelformat >> 16) & 0xff,
            (desc.pixelformat >> 24) & 0xff);
#endif

    /* Return first pixelformat even if we don't support that */
    if(!desc.index)
      *ret = desc.pixelformat;
    
    if(bgv4l2_pixelformat_v4l2_2_gavl(desc.pixelformat) !=
       GAVL_PIXELFORMAT_NONE)
      {
      *ret = desc.pixelformat;
      return 1;
      }
    index++;
    }
  return 0;
  }


static int open_v4l(void * priv,
                    gavl_audio_format_t * audio_format,
                    gavl_video_format_t * format)
  {
  v4l2_t * v4l;
  struct v4l2_capability cap;
  //  struct v4l2_cropcap cropcap;
  //  struct v4l2_crop crop;
  //  unsigned int min;
  unsigned int i;
  enum v4l2_buf_type type;

  v4l = priv;
  gavl_timer_set(v4l->timer, 0);
  gavl_timer_start(v4l->timer);

  v4l->fd = bgv4l2_open_device(v4l->device, V4L2_CAP_VIDEO_CAPTURE,
                               &cap);
  
  //  create_card_parameters(v4l->fd);

  if(v4l->controls)
    free(v4l->controls);
  
  v4l->controls =
    bgv4l2_create_device_controls(v4l->fd, &v4l->num_controls);
  
  bg_log(BG_LOG_DEBUG, LOG_DOMAIN, "Device name: %s", cap.card);
  
  if (cap.capabilities & V4L2_CAP_STREAMING)
    {
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Trying mmap i/o");
    v4l->io = BGV4L2_IO_METHOD_MMAP;
    }
  else if(cap.capabilities & V4L2_CAP_READWRITE)
    {
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Trying read i/o");
    v4l->io = BGV4L2_IO_METHOD_RW;
    }
  
  /* Select video input, video standard and tune here. */

#if 0
  CLEAR (cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (0 == bgv4l2_ioctl (v4l->fd, VIDIOC_CROPCAP, &cropcap))
    {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */
    
    if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_S_CROP, &crop))
      {
      switch (errno)
        {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
        }
      }
    }
  else
    {	
    /* Errors ignored. */
    }
#endif
  
  if(!get_pixelformat(v4l->fd, &v4l->v4l2_pixelformat))
    {
#ifdef HAVE_V4LCONVERT
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Trying v4lconvert");
    v4l->converter = bg_v4l2_convert_create(v4l->fd,
                                            &v4l->v4l2_pixelformat,
                                            &format->pixelformat, v4l->width,
                                            v4l->height);
    if(!v4l->converter)
      {
      return 0;
      }
#else
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Got no supported pixelformat");
    return 0;
#endif
    }
  else
    {
    format->pixelformat =
      bgv4l2_pixelformat_v4l2_2_gavl(v4l->v4l2_pixelformat);
    }
  
  v4l->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  
  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_G_FMT, &v4l->fmt))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "VIDIOC_G_FMT failed: %s", strerror(errno));
    return 0;
    }
  
  // CLEAR (fmt);

  v4l->fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  v4l->fmt.fmt.pix.width       = v4l->width; 
  v4l->fmt.fmt.pix.height      = v4l->height;
  v4l->fmt.fmt.pix.pixelformat = v4l->v4l2_pixelformat;
  
  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_S_FMT, &v4l->fmt))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "VIDIOC_S_FMT failed: %s", strerror(errno));
    return 0;
    }
  /* Note VIDIOC_S_FMT may change width and height. */

#if 0  
  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;
#endif

  if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_G_FMT, &v4l->fmt))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "VIDIOC_G_FMT failed: %s", strerror(errno));
    return 0;
    }
 
  format->pixel_width  = 1;
  format->pixel_height = 1;
  format->image_width  = v4l->fmt.fmt.pix.width;
  format->image_height = v4l->fmt.fmt.pix.height;
  format->frame_width  = v4l->fmt.fmt.pix.width;
  format->frame_height = v4l->fmt.fmt.pix.height;
  format->timescale = GAVL_TIME_SCALE / TIME_DIV;
  format->frame_duration = 0;
  format->framerate_mode = GAVL_FRAMERATE_VARIABLE;

  gavl_video_format_copy(&v4l->format, format);
  //  gavl_video_format_dump(&v4l->format);

  //  fprintf(stderr, "Bytesperline: %d, sizeimage: %d\n",
  //          v4l->fmt.fmt.pix.bytesperline, v4l->fmt.fmt.pix.sizeimage);

  gavl_video_frame_set_strides(v4l->frame, &v4l->format);
  
  switch (v4l->io)
    {
    case BGV4L2_IO_METHOD_RW:
      if(!init_read (v4l))
        return 0;
      break;
      
    case BGV4L2_IO_METHOD_MMAP:
      if(!init_mmap (v4l))
        return 0;
      break;
      
    case BGV4L2_IO_METHOD_USERPTR:
      if(!init_userp (v4l, v4l->fmt.fmt.pix.sizeimage))
        return 0;
      break;
    }
  
  /* start_capturing  */

  switch (v4l->io)
    {
    case BGV4L2_IO_METHOD_RW:
      /* Nothing to do. */
      break;
      
    case BGV4L2_IO_METHOD_MMAP:
      
      break;
      
    case BGV4L2_IO_METHOD_USERPTR:
      for (i = 0; i < v4l->n_buffers; ++i)
        {
        struct v4l2_buffer buf;
        
        CLEAR (buf);
        
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_USERPTR;
        buf.index       = i;
        buf.m.userptr	= (unsigned long) v4l->buffers[i].start;
        buf.length      = v4l->buffers[i].length;
        
        if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_QBUF, &buf))
          return 0;
        }
      
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_STREAMON, &type))
        return 0;
      
      break;
    }
  
  return 1;
  }

static void close_v4l(void * priv)
  {
  v4l2_t * v4l;
  enum v4l2_buf_type type;
  unsigned int i;
  v4l = priv;
  gavl_timer_stop(v4l->timer);

  switch (v4l->io)
    {
    case BGV4L2_IO_METHOD_RW:
      if(v4l->buffers && v4l->buffers[0].start)
        free (v4l->buffers[0].start);
      break;

    case BGV4L2_IO_METHOD_MMAP:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_STREAMOFF, &type))
        return;

      for (i = 0; i < v4l->n_buffers; ++i)
        if (-1 == munmap (v4l->buffers[i].start, v4l->buffers[i].length))
          return;
      break;

    case BGV4L2_IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_STREAMOFF, &type))
        return;
      
      for (i = 0; i < v4l->n_buffers; ++i)
        free (v4l->buffers[i].start);
      break;
    }
  if(v4l->buffers)
    free (v4l->buffers);

  if(v4l->fd >= 0)
    close(v4l->fd);
  v4l->fd = -1;

  if(v4l->controls)
    {
    free(v4l->controls);
    v4l->controls = NULL;
    }
  }

static void process_image(v4l2_t * v4l, void * data,
                          gavl_video_frame_t * frame)
  {
#ifdef HAVE_V4LCONVERT
  if(v4l->converter)
    {
    bg_v4l2_convert_convert(v4l->converter, data, v4l->fmt.fmt.pix.sizeimage,
                            frame);
    }
  else
    {
#endif
  gavl_video_frame_set_planes(v4l->frame, &v4l->format, data);
  gavl_video_frame_copy(&v4l->format, frame, v4l->frame);
#ifdef HAVE_V4LCONVERT
    }
#endif
  frame->timestamp = gavl_timer_get(v4l->timer) / TIME_DIV;
  }

static int read_frame(v4l2_t * v4l, gavl_video_frame_t * frame)
  {
  struct v4l2_buffer buf;
  unsigned int i;

  switch (v4l->io)
    {
    case BGV4L2_IO_METHOD_RW:
      if (-1 == read (v4l->fd, v4l->buffers[0].start, v4l->buffers[0].length))
        {
        switch (errno)
          {
          case EAGAIN:
            return 0;
            
          case EIO:
            /* Could ignore EIO, see spec. */
            
            /* fall through */
            
          default:
            return -1;
          }
        }
      process_image (v4l, v4l->buffers[0].start, frame);

      break;

    case BGV4L2_IO_METHOD_MMAP:
      CLEAR (buf);
      
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_DQBUF, &buf))
        {
        switch (errno)
          {
          case EAGAIN:
            return 0;
            
          case EIO:
            /* Could ignore EIO, see spec. */
            
            /* fall through */
            
          default:
            return -1;
          }
        }
      
      //      assert (buf.index < n_buffers);
      
      process_image (v4l, v4l->buffers[buf.index].start, frame);
      
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_QBUF, &buf))
        return -1;
      
      break;
      
    case BGV4L2_IO_METHOD_USERPTR:
      CLEAR (buf);
      
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_DQBUF, &buf))
        {
        switch (errno)
          {
          case EAGAIN:
            return -1;
            
          case EIO:
            /* Could ignore EIO, see spec. */
            
            /* fall through */
            
          default:
            return 0;
          }
        }
      
      for (i = 0; i < v4l->n_buffers; ++i)
        if (buf.m.userptr == (unsigned long) v4l->buffers[i].start
            && buf.length == v4l->buffers[i].length)
          break;
      
      //      assert (i < n_buffers);
      
      process_image (v4l, (void *) buf.m.userptr, frame);
      
      if (-1 == bgv4l2_ioctl (v4l->fd, VIDIOC_QBUF, &buf))
        return -1;
      
      break;
    }
  
  return 1;
  }

static int read_frame_v4l(void * priv, gavl_video_frame_t * frame, int stream)
  {
  v4l2_t * v4l;
  v4l = priv;

  
  for (;;)
    {
    fd_set fds;
    struct timeval tv;
    int r;
    
    FD_ZERO (&fds);
    FD_SET (v4l->fd, &fds);

    /* Timeout. */
    tv.tv_sec = 4;
    tv.tv_usec = 0;
    
    r = select (v4l->fd + 1, &fds, NULL, NULL, &tv);
    
    if (-1 == r)
      {
      if (EINTR == errno)
        continue;
      return 0;
      }
    
    if (0 == r)
      {
      bg_log(BG_LOG_ERROR, LOG_DOMAIN, "Select timeout");
      return 0;
      }

    r = read_frame (v4l, frame);

    if(r > 0)
      break;
    else if(!r)
      continue;
    else
      return 0;
    
    /* EAGAIN - continue select loop. */
    }
  
  return 1;
  }

static void * create_v4l()
  {
  v4l2_t * v4l;

  v4l = calloc(1, sizeof(*v4l));

  v4l->frame = gavl_video_frame_create(NULL);
  v4l->fd = -1;
  //  v4l->device = bg_strdup(v4l->device, "/dev/video4");
  v4l->timer = gavl_timer_create();
  return v4l;
  }

static void  destroy_v4l(void * priv)
  {
  v4l2_t * v4l;
  v4l = priv;
  gavl_video_frame_null(v4l->frame);
  gavl_video_frame_destroy(v4l->frame);
#if HAVE_V4LCONVERT
  if(v4l->converter)
    bg_v4l2_convert_destroy(v4l->converter);
#endif
  close_v4l(priv);
  gavl_timer_destroy(v4l->timer);
  
  if(v4l->parameters)
    bg_parameter_info_destroy_array(v4l->parameters);

  if(v4l->device)
    free(v4l->device);
  
  free(v4l);
  }


/* Configuration stuff */

static const bg_parameter_info_t parameters[] =
  {
    {
      .name =        "device_section",
      .long_name =   TRS("Device"),
      .type =        BG_PARAMETER_SECTION
    },
    {
      .name =        "device",
      .long_name =   TRS("V4L2 Device"),
      .type =        BG_PARAMETER_MULTI_MENU,
      .val_default = { .val_str = "/dev/video0" },
    },
    {
      .name =        "res",
      .long_name =   TRS("Resolution"),
      .type =        BG_PARAMETER_SECTION,
    },
    {
      .name =      "resolution",
      .long_name = TRS("Resolution"),
      .type =      BG_PARAMETER_STRINGLIST,
      .val_default = { .val_str = "QVGA (320x240)" },
      .multi_names =     (char const *[]){ "QSIF (160x112)",
                              "QCIF (176x144)", 
                              "QVGA (320x240)", 
                              "SIF(352x240)", 
                              "CIF (352x288)", 
                              "VGA (640x480)", 
                              "User defined",
                              NULL },
      .multi_labels =     (char const *[]){ TRS("QSIF (160x112)"),
                                   TRS("QCIF (176x144)"), 
                                   TRS("QVGA (320x240)"), 
                                   TRS("SIF(352x240)"), 
                                   TRS("CIF (352x288)"), 
                                   TRS("VGA (640x480)"), 
                                   TRS("User defined"),
                                   NULL },
    },
    {
      .name =        "user_width",
      .long_name =   TRS("User defined width"),
      .type =        BG_PARAMETER_INT,
      .val_default = { .val_i = 720 },
      .val_min =     { .val_i = 160 },
      .val_max =     { .val_i = 1024 },
    },
    {
      .name =        "user_height",
      .long_name =   TRS("User defined height"),
      .type =        BG_PARAMETER_INT,
      .val_default = { .val_i = 576 },
      .val_min =     { .val_i = 112 },
      .val_max =     { .val_i = 768 },
    },
    { /* End of parameters */ }
  };

static void create_parameters(v4l2_t * v4l)
  {
  bg_parameter_info_t * info;
  v4l->parameters = bg_parameter_info_copy_array(parameters);
  
  info = v4l->parameters + 1;
  bgv4l2_create_device_selector(info, V4L2_CAP_VIDEO_CAPTURE);
  }

static const bg_parameter_info_t * get_parameters_v4l(void * priv)
  {
  v4l2_t * v4l;
  v4l = priv;
  if(!v4l->parameters)
    create_parameters(v4l);
  return v4l->parameters;
  }

static int get_parameter_v4l(void * priv, const char * name,
                             bg_parameter_value_t * val)
  {
  v4l2_t * v4l;
  v4l = priv;
  if(v4l->controls && (v4l->fd >= 0))
    {
    return bgv4l2_get_device_parameter(v4l->fd,
                                       v4l->controls,
                                       v4l->num_controls,
                                       name, val);
    }
  return 0;
  }

static void set_parameter_v4l(void * priv, const char * name,
                              const bg_parameter_value_t * val)
  {
  v4l2_t * v4l;
  v4l = priv;

  if(!name)
    {
    if(v4l->user_resolution)
      {
      v4l->width  = v4l->user_width;
      v4l->height = v4l->user_height;
      }
    return;
    }
  else if(!strcmp(name, "device"))
    {
    v4l->device = bg_strdup(v4l->device, val->val_str);
    }
  else if(!strcmp(name, "resolution"))
    {
    if(!strcmp(val->val_str, "QSIF (160x112)"))
      {
      v4l->width  = 160;
      v4l->height = 112;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "QCIF (176x144)"))
      {
      v4l->width  = 176;
      v4l->height = 144;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "QVGA (320x240)"))
      {
      v4l->width  = 320;
      v4l->height = 240;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "SIF(352x240)"))
      {
      v4l->width  = 352;
      v4l->height = 240;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "CIF (352x288)"))
      {
      v4l->width  = 352;
      v4l->height = 288;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "VGA (640x480)"))
      {
      v4l->width  = 640;
      v4l->height = 480;
      v4l->user_resolution = 0;
      }
    else if(!strcmp(val->val_str, "User defined"))
      {
      v4l->user_resolution = 1;
      }
    }
  else if(!strcmp(name, "user_width"))
    {
    v4l->user_width = val->val_i;
    }
  else if(!strcmp(name, "user_height"))
    {
    v4l->user_height = val->val_i;
    }

  else if(v4l->controls && (v4l->fd >= 0))
    {
    bgv4l2_set_device_parameter(v4l->fd,
                                v4l->controls,
                                v4l->num_controls,
                                name, val);
    }
  }

const bg_recorder_plugin_t the_plugin =
  {
    .common =
    {
      BG_LOCALE,
      .name =          "i_v4l2",
      .long_name =     TRS("V4L2"),
      .description =   TRS("video4linux 2 recording plugin. Supports only video and no tuner decives."),
      .type =          BG_PLUGIN_RECORDER_VIDEO,
      .flags =         BG_PLUGIN_RECORDER | BG_PLUGIN_DEVPARAM,
      .priority =      BG_PLUGIN_PRIORITY_MAX,
      .create =        create_v4l,
      .destroy =       destroy_v4l,

      .get_parameters = get_parameters_v4l,
      .set_parameter =  set_parameter_v4l,
      .get_parameter =  get_parameter_v4l,
    },
    
    .open =       open_v4l,
    .close =      close_v4l,
    .read_video = read_frame_v4l,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
