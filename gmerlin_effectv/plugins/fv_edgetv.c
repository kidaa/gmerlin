/*
 * EffecTV - Realtime Digital Video Effector
 * Copyright (C) 2001-2006 FUKUCHI Kentaro
 *
 * EdgeTV - detects edge and display it in good old computer way. 
 * Copyright (C) 2001-2002 FUKUCHI Kentaro
 *
 * The idea of EdgeTV is taken from Adrian Likins's effector script for GIMP,
 * `Predator effect.'
 *
 * The algorithm of the original script pixelizes the image at first, then
 * it adopts the edge detection filter to the image. It also adopts MaxRGB
 * filter to the image. This is not used in EdgeTV.
 * This code is highly optimized and employs many fake algorithms. For example,
 * it devides a value with 16 instead of using sqrt() in line 132-134. It is
 * too hard for me to write detailed comment in this code in English.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gmerlin_effectv.h>
#include <utils.h>

static int start(effect*);
static int stop(effect*);
static int draw(effect*, RGB32 *src, RGB32 *dest);


typedef struct
  {
  int stat;
  RGB32 *map;
  int map_width;
  int map_height;
  int video_width_margin;
  } edge_t;

static effect *edgeRegister(void)
  {
  effect *entry;

  edge_t * priv;

  priv = calloc(1, sizeof(*priv));
  
  entry = calloc(1, sizeof(effect));
  if(entry == NULL)
    {
    return NULL;
    }
  entry->priv = priv;
  entry->start = start;
  entry->stop = stop;
  entry->draw = draw;
  return entry;
  }

static int start(effect * e)
  {
  edge_t * priv = e->priv;
  memset(priv->map, 0, priv->map_width * priv->map_height * PIXEL_SIZE * 2);

  priv->map_width = e->video_width / 4;
  priv->map_height = e->video_height / 4;
  priv->video_width_margin = e->video_width - priv->map_width * 4;

  priv->map = malloc(priv->map_width*priv->map_height*PIXEL_SIZE*2);
  if(priv->map == NULL)
    {
    return 0;
    }
  priv->stat = 1;
  return 1;
  }

static int stop(effect * e)
  {
  edge_t * priv = e->priv;
  free(priv->map);
  priv->stat = 0;
  return 0;
  }

static int draw(effect * e, RGB32 *src, RGB32 *dest)
  {
  int x, y;
  int r, g, b;
  RGB32 p, q;
  RGB32 v0, v1, v2, v3;
  edge_t * priv = e->priv;
  
  src += e->video_width*4+4;
  dest += e->video_width*4+4;
  for(y=1; y<priv->map_height-1; y++) {
  for(x=1; x<priv->map_width-1; x++) {
  p = *src;
  q = *(src - 4);

  /* difference between the current pixel and right neighbor. */
  r = ((int)(p & 0xff0000) - (int)(q & 0xff0000))>>16;
  g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00))>>8;
  b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
  r *= r; /* Multiply itself and divide it with 16, instead of */
  g *= g; /* using abs(). */
  b *= b;
  r = r>>5; /* To lack the lower bit for saturated addition,  */
  g = g>>5; /* devide the value with 32, instead of 16. It is */
  b = b>>4; /* same as `v2 &= 0xfefeff' */
  if(r>127) r = 127;
  if(g>127) g = 127;
  if(b>255) b = 255;
  v2 = (r<<17)|(g<<9)|b;

  /* difference between the current pixel and upper neighbor. */
  q = *(src - e->video_width*4);
  r = ((int)(p & 0xff0000) - (int)(q & 0xff0000))>>16;
  g = ((int)(p & 0x00ff00) - (int)(q & 0x00ff00))>>8;
  b = ((int)(p & 0x0000ff) - (int)(q & 0x0000ff));
  r *= r;
  g *= g;
  b *= b;
  r = r>>5;
  g = g>>5;
  b = b>>4;
  if(r>127) r = 127;
  if(g>127) g = 127;
  if(b>255) b = 255;
  v3 = (r<<17)|(g<<9)|b;

  v0 = priv->map[(y-1)*priv->map_width*2+x*2];
  v1 = priv->map[y*priv->map_width*2+(x-1)*2+1];
  priv->map[y*priv->map_width*2+x*2] = v2;
  priv->map[y*priv->map_width*2+x*2+1] = v3;
  r = v0 + v1;
  g = r & 0x01010100;
  dest[0] = r | (g - (g>>8));
  r = v0 + v3;
  g = r & 0x01010100;
  dest[1] = r | (g - (g>>8));
  dest[2] = v3;
  dest[3] = v3;
  r = v2 + v1;
  g = r & 0x01010100;
  dest[e->video_width] = r | (g - (g>>8));
  r = v2 + v3;
  g = r & 0x01010100;
  dest[e->video_width+1] = r | (g - (g>>8));
  dest[e->video_width+2] = v3;
  dest[e->video_width+3] = v3;
  dest[e->video_width*2] = v2;
  dest[e->video_width*2+1] = v2;
  dest[e->video_width*3] = v2;
  dest[e->video_width*3+1] = v2;

  src += 4;
  dest += 4;
  }
  src += e->video_width*3+8+priv->video_width_margin;
  dest += e->video_width*3+8+priv->video_width_margin;
  }

  return 0;
  }

static void * create_edgetv()
  {
  return bg_effectv_create(edgeRegister, 0);
  }

const bg_fv_plugin_t the_plugin = 
  {
    .common =
    {
      BG_LOCALE,
      .name =      "fv_edgetv",
      .long_name = TRS("EdgeTV"),
      .description = TRS("Detects edges and display it like good old low resolution computer way. Ported from EffecTV (http://effectv.sourceforge.net)."),
      .type =     BG_PLUGIN_FILTER_VIDEO,
      .flags =    BG_PLUGIN_FILTER_1,
      .create =   create_edgetv,
      .destroy =   bg_effectv_destroy,
      //      .get_parameters =   get_parameters_invert,
      //      .set_parameter =    set_parameter_invert,
      .priority =         1,
    },
    
    .connect = bg_effectv_connect,
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
