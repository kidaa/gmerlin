/*****************************************************************
 
  fv_decimate.c
 
  Copyright (c) 2007 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

#include <stdlib.h>
#include <string.h>

#include <config.h>
#include <translation.h>
#include <plugin.h>
#include <utils.h>
#include <log.h>

#include <gavl/gavldsp.h>

#define LOG_DOMAIN "fv_decimate"

typedef struct decimate_priv_s decimate_priv_t;

struct decimate_priv_s
  {
  gavl_dsp_context_t * dsp_ctx;
  gavl_dsp_funcs_t   * dsp_funcs;

  gavl_video_frame_t * frame;
  gavl_video_format_t format;

  gavl_video_frame_t * b1;
  gavl_video_frame_t * b2;

  bg_read_video_func_t read_func;
  void * read_data;
  int read_stream;

  float threshold_block;
  float threshold_total;
  int do_log;
  int skip_max;

  int have_frame;

  int num_planes;
  int sub_h;
  int sub_v;
  float scale_factors[GAVL_MAX_PLANES];
  int width_mul;

  float (*diff_block)(struct decimate_priv_s*, 
                      int width, int height);

  int (*sad_func)(uint8_t * src_1, uint8_t * src_2, 
                  int stride_1, int stride_2, 
                  int w, int h);
  };

static float diff_block_i(decimate_priv_t * vp, 
                          int width, int height)
  {
  int i;
  float ret = 0.0, tmp;
  
  ret = vp->sad_func(vp->b1->planes[0], vp->b2->planes[0],
                     vp->b1->strides[0], vp->b2->strides[0],
                     width * vp->width_mul, height);

  ret *= vp->scale_factors[0];
  
  width  /= vp->sub_h;
  height /= vp->sub_v;
  
  for(i = 1; i < vp->num_planes; i++)
    {
    tmp = vp->sad_func(vp->b1->planes[i], vp->b2->planes[i],
                       vp->b1->strides[i], vp->b2->strides[i],
                       width, height);
    tmp *= vp->scale_factors[i];
    ret += tmp;
    }
  return ret;
  }

static float diff_block_f(decimate_priv_t * vp, 
                          int width, int height)
  {
  float ret = 0.0;
  
  ret = vp->dsp_funcs->sad_f(vp->b1->planes[0], vp->b2->planes[0],
                           vp->b1->strides[0], vp->b2->strides[0],
                           width * vp->width_mul, height);

  ret *= vp->scale_factors[0];

  /* No planar float formats yet */
  return ret;
  }

static void * create_decimate()
  {
  decimate_priv_t * ret;
  ret = calloc(1, sizeof(*ret));
  ret->b1 = gavl_video_frame_create(NULL);
  ret->b2 = gavl_video_frame_create(NULL);
  ret->dsp_ctx = gavl_dsp_context_create();
  ret->dsp_funcs = gavl_dsp_context_get_funcs(ret->dsp_ctx);
  return ret;
  }

static void destroy_decimate(void * priv)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;
  if(vp->frame)
    gavl_video_frame_destroy(vp->frame);
  gavl_video_frame_null(vp->b1);
  gavl_video_frame_null(vp->b2);

  gavl_video_frame_destroy(vp->b1);
  gavl_video_frame_destroy(vp->b2);
  
  free(vp);
  }

static bg_parameter_info_t parameters[] =
  {
    {
      gettext_domain: PACKAGE,
      gettext_directory: LOCALE_DIR,
      name: "threshold_block",
      long_name: TRS("Block threshold"),
      type: BG_PARAMETER_SLIDER_FLOAT,
      val_min: { val_f: 0.0 },
      val_max: { val_f: 1.0 },
      num_digits: 2,
      flags: BG_PARAMETER_SYNC,
      help_string: TRS("Specifies how much a block may differ from the last non-skipped block. 0 means identical blocks, 1 means completely different blocks. Note that the meaning of \"completely different\" depends on the colorspace.")
    },
    {
      name: "threshold_total",
      long_name: TRS("Total threshold"),
      type: BG_PARAMETER_SLIDER_FLOAT,
      val_min: { val_f: 0.0 },
      val_max: { val_f: 1.0 },
      num_digits: 2,
      flags: BG_PARAMETER_SYNC,
      help_string: TRS("Specifies how much a frame may differ from the last non-skipped frame. 0 means identical frames,  1 means completely different frames. Note that the meaning of \"completely different\" depends on the colorspace.")
    },
    {
      name: "skip_max",
      long_name: TRS("Maximum skipped frames"),
      type: BG_PARAMETER_INT,
      val_min:     { val_i: 1 },
      val_max:     { val_i: 500 },
      val_default: { val_i: 10 },
      flags: BG_PARAMETER_SYNC,
      help_string: TRS("Maximum number of consecutive skipped frames .")
    },
    {
      name: "do_log",
      long_name: TRS("Report results"),
      type: BG_PARAMETER_CHECKBUTTON,
      flags: BG_PARAMETER_SYNC,
      help_string: TRS("Log reports about skipped frames"),
    },
    { /* End of parameters */ },
  };

static bg_parameter_info_t * get_parameters_decimate(void * priv)
  {
  return parameters;
  }

static void set_parameter_decimate(void * priv, char * name, bg_parameter_value_t * val)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;

  if(!name)
    return;

  if(!strcmp(name, "threshold_block"))
    vp->threshold_block = val->val_f;
  else if(!strcmp(name, "threshold_total"))
    vp->threshold_total = val->val_f;
  else if(!strcmp(name, "do_log"))
   vp->do_log = val->val_i;
  else if(!strcmp(name, "skip_max"))
   vp->skip_max = val->val_i;
  }

static void connect_input_port_decimate(void * priv,
                                    bg_read_video_func_t func,
                                    void * data, int stream, int port)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;

  if(!port)
    {
    vp->read_func = func;
    vp->read_data = data;
    vp->read_stream = stream;
    }
  }

static void reset_decimate(void * priv)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;
  vp->have_frame = 0;
  }

static void 
set_input_format_decimate(void * priv, 
                          gavl_video_format_t * format, int port)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;

  if(!port)
    {
    gavl_video_format_copy(&vp->format, format);
    vp->format.framerate_mode = GAVL_FRAMERATE_VARIABLE;
    if(vp->frame)
      {
      gavl_video_frame_destroy(vp->frame);
      vp->frame = (gavl_video_frame_t*)0;
      }
    gavl_pixelformat_chroma_sub(vp->format.pixelformat,
                                &vp->sub_h, &vp->sub_v);
    vp->num_planes = 
      gavl_pixelformat_num_planes(vp->format.pixelformat);

    vp->width_mul = 1;
    vp->diff_block = diff_block_i;

    switch(vp->format.pixelformat)
      {
      case GAVL_RGB_15:
      case GAVL_BGR_15:
        vp->scale_factors[0] = 1.0/(3.0*255.0);
        vp->sad_func = vp->dsp_funcs->sad_rgb15;
        break;
      case GAVL_RGB_16:
      case GAVL_BGR_16:
        vp->scale_factors[0] = 1.0/(3.0*255.0);
        vp->sad_func = vp->dsp_funcs->sad_rgb16;
        break;
      case GAVL_RGB_24:
      case GAVL_BGR_24:
        vp->scale_factors[0] = 1.0/(3.0*255.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        vp->width_mul = 3;
        break;
      case GAVL_RGB_32:
      case GAVL_BGR_32:
        vp->scale_factors[0] = 1.0/(3.0*255.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        vp->width_mul = 4;
        break;
      case GAVL_RGBA_32:
        vp->scale_factors[0] = 1.0/(4.0*255.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        vp->width_mul = 4;
        break;
      case GAVL_YUV_444_P:
      case GAVL_YUV_420_P:
      case GAVL_YUV_410_P:
      case GAVL_YUV_411_P:
      case GAVL_YUV_422_P:
        vp->scale_factors[0] = 1.0/((235.0 - 16.0)*3.0);
        vp->scale_factors[1] = 
          (float)(vp->sub_h * vp->sub_v)/((240.0 - 16.0)*3.0);
        vp->scale_factors[2] = 
          (float)(vp->sub_h * vp->sub_v)/((240.0 - 16.0)*3.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        break;
      case GAVL_YUV_444_P_16:
      case GAVL_YUV_422_P_16:
        vp->scale_factors[0] = 1.0/((235.0 - 16.0)*256.0*3.0);
        vp->scale_factors[1] = 
          (float)(vp->sub_h * vp->sub_v)/((240.0 - 16.0)*256.0*3.0);
        vp->scale_factors[2] = 
          (float)(vp->sub_h * vp->sub_v)/((240.0 - 16.0)*256.0*3.0);
        vp->sad_func = vp->dsp_funcs->sad_16;
        break;
      case GAVL_RGB_48:
      	vp->scale_factors[0] = 1.0/(3.0*65535.0);
        vp->sad_func = vp->dsp_funcs->sad_16;
        vp->width_mul = 3;
        break;
      case GAVL_RGBA_64:
      	vp->scale_factors[0] = 1.0/(4.0*65535.0);
        vp->sad_func = vp->dsp_funcs->sad_16;
        vp->width_mul = 4;
        break;
      case GAVL_RGB_FLOAT:
        vp->scale_factors[0] = 1.0/3.0;
        vp->diff_block = diff_block_f;
        vp->width_mul = 3;
        break;
      case GAVL_RGBA_FLOAT:
        vp->scale_factors[0] = 1.0/4.0;
        vp->diff_block = diff_block_f;
        vp->width_mul = 4;
        break;
      case GAVL_YUVA_32:
        vp->scale_factors[0] = 
          1.0/(235.0 - 16.0 + 2.0 * (240.0 - 16.0) + 255.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        vp->width_mul = 4;
        break;
      case GAVL_YUY2:
      case GAVL_UYVY:
        vp->scale_factors[0] = 
          1.0/(235.0 - 16.0 + 240.0 - 16.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        vp->width_mul = 2;
        break;
      case GAVL_YUVJ_420_P:
      case GAVL_YUVJ_422_P:
      case GAVL_YUVJ_444_P:
        vp->scale_factors[0] = 1.0/(3.0 * 255.0);
        vp->scale_factors[1] = 
          (float)(vp->sub_h * vp->sub_v)/(3.0 * 255.0);
        vp->scale_factors[2] = 
          (float)(vp->sub_h * vp->sub_v)/(3.0 * 255.0);
        vp->sad_func = vp->dsp_funcs->sad_8;
        break;
      case GAVL_PIXELFORMAT_NONE:
        break;
      }
    }
  vp->frame = gavl_video_frame_create(&vp->format);
  }

static void get_output_format_decimate(void * priv, gavl_video_format_t * format)
  {
  decimate_priv_t * vp;
  vp = (decimate_priv_t *)priv;
  
  gavl_video_format_copy(format, &vp->format);
  }

#define BLOCK_SIZE 16

static int do_skip(decimate_priv_t * vp, gavl_video_frame_t * frame)
  {
  int i, j, imax, jmax;
  float diff_block;
  float diff_total = 0.0;

  int threshold_total = 
    vp->threshold_total * 
    vp->format.image_width * 
    vp->format.image_height;

  gavl_rectangle_i_t rect;
  imax = (vp->format.image_height + BLOCK_SIZE - 1)/BLOCK_SIZE;
  jmax = (vp->format.image_width  + BLOCK_SIZE - 1)/BLOCK_SIZE;

  for(i = 0; i < imax; i++)
    {
    for(j = 0; j < jmax; j++)
      {
      rect.x = j * BLOCK_SIZE;
      rect.y = i * BLOCK_SIZE;
      rect.w = BLOCK_SIZE;
      rect.h = BLOCK_SIZE;
      gavl_rectangle_i_crop_to_format(&rect, &vp->format);
      gavl_video_frame_get_subframe(vp->format.pixelformat,
                                    frame, vp->b1, &rect);
      gavl_video_frame_get_subframe(vp->format.pixelformat,
                                    vp->frame, vp->b2, &rect);
      diff_block = vp->diff_block(vp, rect.w, rect.h);
      if(diff_block > vp->threshold_block * rect.w * rect.h)
        return 0;
      diff_total += diff_block;
      if(diff_total > threshold_total)
        return 0;
      }
    }
  return 1;
  }

static int read_video_decimate(void * priv, 
                               gavl_video_frame_t * frame, int stream)
  {
  decimate_priv_t * vp;
  int skipped = 0;
  vp = (decimate_priv_t *)priv;
  if(!vp->have_frame)
    {
    if(!vp->read_func(vp->read_data, frame, vp->read_stream))
      return 0;
    gavl_video_frame_copy(&vp->format, vp->frame, frame);
    vp->have_frame = 1;
    return 1;
    }
  
  while(1)
    {
    if(!vp->read_func(vp->read_data, frame, vp->read_stream))
      return 0;
    if((skipped >= vp->skip_max) || !do_skip(vp, frame))
      break;
    skipped++;
    }
  gavl_video_frame_copy(&vp->format, vp->frame, frame);
  
  /* Don't know when the next frame will come */
  frame->duration_scaled = -1; 
  if(vp->do_log && skipped)
    bg_log(BG_LOG_INFO, LOG_DOMAIN, "Skipped %d frames", skipped);  
  return 1;
  }

bg_fv_plugin_t the_plugin = 
  {
    common:
    {
      BG_LOCALE,
      name:      "fv_decimate",
      long_name: TRS("Decimate"),
      description: TRS("Skip almost identical frames"),
      type:     BG_PLUGIN_FILTER_VIDEO,
      flags:    BG_PLUGIN_FILTER_1,
      create:   create_decimate,
      destroy:   destroy_decimate,
      get_parameters:   get_parameters_decimate,
      set_parameter:    set_parameter_decimate,
      priority:         1,
    },
    
    connect_input_port: connect_input_port_decimate,
    
    set_input_format: set_input_format_decimate,
    get_output_format: get_output_format_decimate,

    read_video: read_video_decimate,
    
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;