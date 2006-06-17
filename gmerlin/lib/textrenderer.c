/*****************************************************************
 
  textrenderer.c
 
  Copyright (c) 2006 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de
 
  http://gmerlin.sourceforge.net
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 
*****************************************************************/

/* System includes */

#include <stdlib.h>
#include <pthread.h>

/* Fontconfig */

#include <fontconfig/fontconfig.h>

/* Gmerlin */

#include <bgfreetype.h>

#include <parameter.h>
#include <textrenderer.h>
#include <utils.h>
#include <charset.h>

/* Text alignment */

#define JUSTIFY_CENTER 0
#define JUSTIFY_LEFT   1
#define JUSTIFY_RIGHT  2

#define JUSTIFY_TOP    1
#define JUSTIFY_BOTTOM 2

static bg_parameter_info_t parameters[] =
  {
    {
      name:       "color",
      long_name:  "Text color",
      type:       BG_PARAMETER_COLOR_RGBA,
      val_default: { val_color: (float[]){ 1.0, 1.0, 1.0, 1.0 } },
    },
#ifdef FT_STROKER_H
    {
      name:       "border_color",
      long_name:  "Border color",
      type:       BG_PARAMETER_COLOR_RGB,
      val_default: { val_color: (float[]){ 0.0, 0.0, 0.0, 1.0 } },
    },
    {
      name:       "border_width",
      long_name:  "Border width",
      type:       BG_PARAMETER_FLOAT,
      val_min:     { val_f: 0.0 },
      val_max:     { val_f: 10.0 },
      val_default: { val_f: 2.0 },
      num_digits:  2,
    },
#endif    
    {
      name:       "font",
      long_name:  "Font",
      type:       BG_PARAMETER_FONT,
      val_default: { val_str: "Sans-20" }
    },
    {
      name:       "justify_h",
      long_name:  "Horizontal justify",
      type:       BG_PARAMETER_STRINGLIST,
      val_default: { val_str: "center" },
      multi_names:  (char*[]){ "center", "left", "right", (char*)0 },
      multi_labels: (char*[]){ "Center", "Left", "Right", (char*)0  },
            
    },
    {
      name:       "justify_v",
      long_name:  "Vertical justify",
      type:       BG_PARAMETER_STRINGLIST,
      val_default: { val_str: "bottom" },
      multi_names:  (char*[]){ "center", "top", "bottom", (char*)0  },
      multi_labels: (char*[]){ "Center", "Top", "Bottom", (char*)0 },
    },
    {
      name:        "cache_size",
      long_name:   "Cache size",
      type:        BG_PARAMETER_INT,
      val_min:     { val_i: 1     },
      val_max:     { val_i: 65535 },
      val_default: { val_i: 255   },
      
      help_string: "Specify, how many different characters are cached for faster rendering. For European languages, this never needs to be larger than 255",
    },
    {
      name:        "border_left",
      long_name:   "Left border",
      type:        BG_PARAMETER_INT,
      val_min:     { val_i: 0     },
      val_max:     { val_i: 65535 },
      val_default: { val_i: 10    },
      help_string: "Distance from the left text border to the image border",
    },
    {
      name:        "border_right",
      long_name:   "Left border",
      type:        BG_PARAMETER_INT,
      val_min:     { val_i: 0     },
      val_max:     { val_i: 65535 },
      val_default: { val_i: 10    },
      help_string: "Distance from the right text border to the image border",
    },
    {
      name:        "border_top",
      long_name:   "Top border",
      type:        BG_PARAMETER_INT,
      val_min:     { val_i: 0     },
      val_max:     { val_i: 65535 },
      val_default: { val_i: 10    },
      help_string: "Distance from the top text border to the image border",
    },
    {
      name:        "border_bottom",
      long_name:   "Bottom border",
      type:        BG_PARAMETER_INT,
      val_min:     { val_i: 0     },
      val_max:     { val_i: 65535 },
      val_default: { val_i: 10    },
      help_string: "Distance from the bottom text border to the image border",
    },
    {
      name:        "ignore_linebreaks",
      long_name:   "Ignore linebreaks",
      type:        BG_PARAMETER_CHECKBUTTON,
      help_string: "Ignore linebreaks in subtitles."
    },
    { /* End of parameters */ },
  };

typedef struct
  {
  int xmin, xmax, ymin, ymax;
  } bbox_t;

typedef struct
  {
  uint32_t unicode;
  FT_Glyph glyph;
#ifdef FT_STROKER_H
  FT_Glyph glyph_stroke;
#endif
  int advance_x, advance_y; /* Advance in integer pixels */
  bbox_t bbox;
  } cache_entry_t;

struct bg_text_renderer_s
  {
  gavl_rectangle_i_t max_bbox; /* Maximum bounding box the text may have */
  
  FT_Library library;
  FT_Face    face;
  
  int font_loaded;
  int font_changed;
  
  /* Configuration stuff */

  char * font;
  char * font_file;
  
  double font_size;
  
  float color[4];
  float alpha_f;
  int   alpha_i;

#ifdef FT_STROKER_H
  float color_stroke[4];
  int color_i[4];
  FT_Stroker stroker;
  float border_width;
#endif  
  
  /* Charset converter */

  bg_charset_converter_t * cnv;

  /* Glyph cache */

  cache_entry_t * cache;
  
  int cache_size;
  int cache_alloc;
  gavl_video_format_t overlay_format;
  gavl_video_format_t frame_format;
  gavl_video_format_t last_frame_format;

  int justify_h;
  int justify_v;
  int border_left, border_right, border_top, border_bottom;
  
  bbox_t bbox; /* Actual bounding box of the text */
  int ignore_linebreaks;

  int sub_h, sub_v; /* Chroma subsampling of the final destination frame */
  
  void (*render_func)(bg_text_renderer_t * r, cache_entry_t * glyph,
                      gavl_video_frame_t * frame,
                      int * dst_x, int * dst_y);
  pthread_mutex_t config_mutex;

  int config_changed;
  };

static void adjust_bbox(cache_entry_t * glyph, int dst_x, int dst_y, bbox_t * ret)
  {
  if(ret->xmin > dst_x + glyph->bbox.xmin)
    ret->xmin = dst_x + glyph->bbox.xmin;

  if(ret->ymin > dst_y + glyph->bbox.ymin)
    ret->ymin = dst_y + glyph->bbox.ymin;

  if(ret->xmax < dst_x + glyph->bbox.xmax)
    ret->xmax = dst_x + glyph->bbox.xmax;

  if(ret->ymax < dst_y + glyph->bbox.ymax)
    ret->ymax = dst_y + glyph->bbox.ymax;
  }

static void render_rgba_32(bg_text_renderer_t * r, cache_entry_t * glyph,
                           gavl_video_frame_t * frame,
                           int * dst_x, int * dst_y)
  {
  FT_BitmapGlyph bitmap_glyph;
  uint8_t * src_ptr, * dst_ptr, * src_ptr_start, * dst_ptr_start;
  int i, j, i_tmp;
#ifdef FT_STROKER_H
  int alpha_i;
#endif
 

#ifdef FT_STROKER_H
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph_stroke);
#else
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);
#endif
  
  if(!bitmap_glyph->bitmap.buffer)
    {
    // fprintf(stderr, "Bitmap missing\n");
    *dst_x += glyph->advance_x;
    *dst_y += glyph->advance_y;
    return;
    }

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 4;
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      i_tmp = ((int)*src_ptr * (int)r->alpha_i) >> 8;
      if(i_tmp > dst_ptr[3])
        dst_ptr[3] = i_tmp;
      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  // #if 0
#ifdef FT_STROKER_H
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 4;
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      if(*src_ptr)
        {
        alpha_i = *src_ptr;
        
        dst_ptr[0] = (int)dst_ptr[0] +
          ((alpha_i * ((int)(r->color_i[0]) - (int)dst_ptr[0])) >> 8);

        dst_ptr[1] = (int)dst_ptr[1] +
          ((alpha_i * ((int)(r->color_i[1]) - (int)dst_ptr[1])) >> 8);

        dst_ptr[2] = (int)dst_ptr[2] +
          ((alpha_i * ((int)(r->color_i[2]) - (int)dst_ptr[2])) >> 8);

        }

      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  
#endif
  
  *dst_x += glyph->advance_x;
  *dst_y += glyph->advance_y;
  }

static void render_rgba_64(bg_text_renderer_t * r, cache_entry_t * glyph,
                           gavl_video_frame_t * frame,
                           int * dst_x, int * dst_y)
  {
  FT_BitmapGlyph bitmap_glyph;
  uint8_t * src_ptr, * src_ptr_start, * dst_ptr_start;
  uint16_t * dst_ptr;
  int i, j, i_tmp;
#ifdef FT_STROKER_H
  int alpha_i;
#endif

#ifdef FT_STROKER_H
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph_stroke);
#else
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);
#endif
  
  if(!bitmap_glyph->bitmap.buffer)
    {
    // fprintf(stderr, "Bitmap missing\n");
    *dst_x += glyph->advance_x;
    *dst_y += glyph->advance_y;
    return;
    }

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 8;
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = (uint16_t*)dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      i_tmp = ((int)*src_ptr * (int)r->alpha_i) >> 8;
      if(i_tmp > dst_ptr[3])
        dst_ptr[3] = i_tmp;
      
      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  // #if 0
#ifdef FT_STROKER_H
  /* Render border */
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 8;
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = (uint16_t*)dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      if(*src_ptr)
        {
        //        alpha_i = ((int)*src_ptr * (int)r->color_i[3]) >> 8;
        alpha_i = *src_ptr;
        
        dst_ptr[0] = (int)dst_ptr[0] +
          ((alpha_i * ((int64_t)(r->color_i[0]) - (int64_t)dst_ptr[0])) >> 8);

        dst_ptr[1] = (int)dst_ptr[1] +
          ((alpha_i * ((int64_t)(r->color_i[1]) - (int64_t)dst_ptr[1])) >> 8);

        dst_ptr[2] = (int)dst_ptr[2] +
          ((alpha_i * ((int64_t)(r->color_i[2]) - (int64_t)dst_ptr[2])) >> 8);

        }
      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  
#endif
  
  *dst_x += glyph->advance_x;
  *dst_y += glyph->advance_y;

  }

static void render_rgba_float(bg_text_renderer_t * r, cache_entry_t * glyph,
                              gavl_video_frame_t * frame,
                              int * dst_x, int * dst_y)
  {
  FT_BitmapGlyph bitmap_glyph;
  uint8_t * src_ptr, * src_ptr_start, * dst_ptr_start;
  float * dst_ptr;
  int i, j;
  float f_tmp;
#ifdef FT_STROKER_H
  float alpha_f;
#endif

#ifdef FT_STROKER_H
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph_stroke);
#else
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);
#endif
  
  if(!bitmap_glyph->bitmap.buffer)
    {
    // fprintf(stderr, "Bitmap missing\n");
    *dst_x += glyph->advance_x;
    *dst_y += glyph->advance_y;
    return;
    }

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 4 * sizeof(float);
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = (float*)dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      f_tmp = ((float)*src_ptr * r->alpha_f) / 255.0;
      if(f_tmp > dst_ptr[3])
        dst_ptr[3] = f_tmp;
      
      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  // #if 0
#ifdef FT_STROKER_H
  /* Render border */
  bitmap_glyph = (FT_BitmapGlyph)(glyph->glyph);

  src_ptr_start = bitmap_glyph->bitmap.buffer;
  dst_ptr_start = frame->planes[0] + (*dst_y - bitmap_glyph->top) *
    frame->strides[0] + (*dst_x + bitmap_glyph->left) * 4 * sizeof(float);
  
  for(i = 0; i < bitmap_glyph->bitmap.rows; i++)
    {
    src_ptr = src_ptr_start;
    dst_ptr = (float*)dst_ptr_start;
    
    for(j = 0; j < bitmap_glyph->bitmap.width; j++)
      {
      if(*src_ptr)
        {
        alpha_f = (float)(*src_ptr) / 255.0;
        
        dst_ptr[0] = dst_ptr[0] + alpha_f * (r->color[0] - dst_ptr[0]);
        dst_ptr[1] = dst_ptr[1] + alpha_f * (r->color[1] - dst_ptr[1]);
        dst_ptr[2] = dst_ptr[2] + alpha_f * (r->color[2] - dst_ptr[2]);
        }
      src_ptr++;
      dst_ptr += 4;
      }
    src_ptr_start += bitmap_glyph->bitmap.pitch;
    dst_ptr_start += frame->strides[0];
    }
  
#endif
  
  *dst_x += glyph->advance_x;
  *dst_y += glyph->advance_y;

  }

static void alloc_glyph_cache(bg_text_renderer_t * r, int size)
  {
  int i;
  if(size > r->cache_alloc)
    {
    /* Enlarge */
    r->cache_alloc = size;
    r->cache = realloc(r->cache, r->cache_alloc * sizeof(*(r->cache)));
    }
  else if(size < r->cache_alloc)
    {
    /* Shrink */
    if(size < r->cache_size)
      {
      for(i = size; i < r->cache_size; i++)
        FT_Done_Glyph(r->cache[i].glyph);
      r->cache_size = size;
      }
    r->cache_alloc = size;
    r->cache = realloc(r->cache, r->cache_alloc * sizeof(*(r->cache)));
    }
  else
    return;
  }

static void clear_glyph_cache(bg_text_renderer_t * r)
  {
  int i;
  for(i = 0; i < r->cache_size; i++)
    {
    FT_Done_Glyph(r->cache[i].glyph);
#ifdef FT_STROKER_H
    FT_Done_Glyph(r->cache[i].glyph_stroke);
#endif
    }
  r->cache_size = 0;
  }

static cache_entry_t * get_glyph(bg_text_renderer_t * r, uint32_t unicode)
  {
  int i, index;
  cache_entry_t * entry;
  FT_BitmapGlyph bitmap_glyph;
      
  for(i = 0; i < r->cache_size; i++)
    {
    if(r->cache[i].unicode == unicode)
      return &r->cache[i];
    }

  /* No glyph found, try to load a new one into the cache */
  if(r->cache_size >= r->cache_alloc)
    {
    FT_Done_Glyph(r->cache[0].glyph);
    index = 0;
    }
  else
    {
    index = r->cache_size;
    r->cache_size++;
    }
  entry = &(r->cache[index]);
  
  /* Load the glyph */
  if(FT_Load_Char(r->face, unicode, FT_LOAD_DEFAULT))
    {
    fprintf(stderr, "Cannot load character for code %d\n", unicode);
    return (cache_entry_t*)0;
    }
  /* extract glyph image */
  if(FT_Get_Glyph(r->face->glyph, &(entry->glyph)))
    {
    fprintf(stderr, "Copying glyph failed\n");
    return (cache_entry_t*)0;
    }
#ifdef FT_STROKER_H
  /* Stroke glyph */
  entry->glyph_stroke = entry->glyph;
  FT_Glyph_StrokeBorder(&(entry->glyph_stroke), r->stroker, 0, 0);
  //  FT_Glyph_StrokeBorder(&(entry->glyph_stroke), r->stroker, 1, 0);
#endif
  
  /* Render glyph */
  if(FT_Glyph_To_Bitmap( &(entry->glyph),
                         FT_RENDER_MODE_NORMAL,
                         (FT_Vector*)0, 1 ))
    return (cache_entry_t*)0;

#ifdef FT_STROKER_H
  if(FT_Glyph_To_Bitmap( &(entry->glyph_stroke),
                         FT_RENDER_MODE_NORMAL,
                         (FT_Vector*)0, 1 ))
    return (cache_entry_t*)0;
#endif

  /* Get bounding box and advances */

#ifdef FT_STROKER_H
  bitmap_glyph = (FT_BitmapGlyph)(entry->glyph_stroke);
#else
  bitmap_glyph = (FT_BitmapGlyph)(entry->glyph);
#endif

  entry->bbox.xmin = bitmap_glyph->left;
  entry->bbox.ymin = -bitmap_glyph->top;
  entry->bbox.xmax = entry->bbox.xmin + bitmap_glyph->bitmap.width;
  entry->bbox.ymax = entry->bbox.ymin + bitmap_glyph->bitmap.rows;

  entry->advance_x = entry->glyph->advance.x>>16;
  entry->advance_y = entry->glyph->advance.y>>16;
  
  entry->unicode = unicode;
  return entry;
  }

static void unload_font(bg_text_renderer_t * r)
  {
  if(!r->font_loaded)
    return;

#ifdef FT_STROKER_H
  FT_Stroker_Done(r->stroker);
#endif

  clear_glyph_cache(r);
  FT_Done_Face(r->face);
  r->face = (FT_Face)0;
  r->font_loaded = 0;
  }

/* fontconfig stuff inspired from MPlayer code */

static int load_font(bg_text_renderer_t * r)
  {
  int err;
  FcPattern *fc_pattern, *fc_pattern_1;
  FcChar8 *filename;
  FcBool scalable;
  float sar, font_size_scaled;
  
  unload_font(r);

  if(r->font)
    {
    /* Get font file */
    FcInit();
    fc_pattern = FcNameParse((const FcChar8*)r->font);
    
    //  FcPatternPrint(fc_pattern);
    
    FcConfigSubstitute(0, fc_pattern, FcMatchPattern);
    
    //  FcPatternPrint(fc_pattern);
    
    FcDefaultSubstitute(fc_pattern);
    
    //  FcPatternPrint(fc_pattern);
    
    fc_pattern_1 = FcFontMatch((FcConfig*)0, fc_pattern, 0);
    
    //  FcPatternPrint(fc_pattern);
    
    FcPatternGetBool(fc_pattern_1, FC_SCALABLE, 0, &scalable);
    if(scalable != FcTrue)
      {
      fprintf(stderr, "Font %s not scalable, using built in default\n",
              r->font);
      FcPatternDestroy(fc_pattern);
      FcPatternDestroy(fc_pattern_1);
      
      fc_pattern = FcNameParse((const FcChar8*)"Sans-20");
      FcConfigSubstitute(0, fc_pattern, FcMatchPattern);
      FcDefaultSubstitute(fc_pattern);
      fc_pattern_1 = FcFontMatch(0, fc_pattern, 0);
      }
    // s doesn't need to be freed according to fontconfig docs
    
    FcPatternGetString(fc_pattern_1, FC_FILE, 0, &filename);
    FcPatternGetDouble(fc_pattern_1, FC_SIZE, 0, &(r->font_size));
    }
  else
    {
    filename = (FcChar8 *)r->font_file;
    }
  
  //  fprintf(stderr, "File: %s, Size: %f\n", filename, r->font_size);
  
  /* Load face */
  
  err = FT_New_Face(r->library, (char*)filename, 0, &r->face);
  
  if(err)
    {
    if(r->font)
      {
      FcPatternDestroy(fc_pattern);
      FcPatternDestroy(fc_pattern_1);
      }
    return 0;
    }

  if(r->font)
    {
    FcPatternDestroy(fc_pattern);
    FcPatternDestroy(fc_pattern_1);
    }
  
  /* Select Unicode */

  FT_Select_Charmap(r->face, FT_ENCODING_UNICODE );

#ifdef FT_STROKER_H
  /* Create stroker */
  FT_Stroker_New(r->face->memory, &(r->stroker));
  FT_Stroker_Set(r->stroker, (int)(r->border_width * 32.0 + 0.5), 
                 FT_STROKER_LINECAP_ROUND, 
                 FT_STROKER_LINEJOIN_ROUND, 0); 

#endif

  sar = (float)(r->frame_format.pixel_width) /
    (float)(r->frame_format.pixel_height);

  font_size_scaled = r->font_size * sar * (float)(r->frame_format.image_width) / 640.0;
  
  
  err = FT_Set_Char_Size(r->face,                     /* handle to face object           */
                         0,                           /* char_width in 1/64th of points  */
                         (int)(font_size_scaled*64.0+0.5),/* char_height in 1/64th of points */
                         (int)(72.0/sar+0.5),         /* horizontal device resolution    */
                         72 );                        /* vertical device resolution      */
  
  clear_glyph_cache(r);
  r->font_loaded = 1;
  return 1;
  }

bg_text_renderer_t * bg_text_renderer_create()
  {
  bg_text_renderer_t * ret = calloc(1, sizeof(*ret));
#ifdef GAVL_PROCESSOR_LITTLE_ENDIAN
  ret->cnv = bg_charset_converter_create("UTF-8", "UCS-4LE");
#else
  ret->cnv = bg_charset_converter_create("UTF-8", "UCS-4BE");
#endif
  pthread_mutex_init(&(ret->config_mutex),(pthread_mutexattr_t *)0);
  /* Initialize freetype */
  FT_Init_FreeType(&ret->library);

  return ret;
  }

void bg_text_renderer_destroy(bg_text_renderer_t * r)
  {
  bg_charset_converter_destroy(r->cnv);

  unload_font(r);
    
  if(r->cache)
    free(r->cache);

  if(r->font)
    free(r->font);
  if(r->font_file)
    free(r->font_file);
  
  FT_Done_FreeType(r->library);
  pthread_mutex_destroy(&(r->config_mutex));
  free(r);
  }

bg_parameter_info_t * bg_text_renderer_get_parameters()
  {
  return parameters;
  }

void bg_text_renderer_set_parameter(void * data, char * name,
                                    bg_parameter_value_t * val)
  {
  bg_text_renderer_t * r;
  r = (bg_text_renderer_t *)data;

  if(!name)
    return;

  pthread_mutex_lock(&(r->config_mutex));

  /* General text renderer */
  if(!strcmp(name, "font"))
    {
    if(!r->font || strcmp(val->val_str, r->font))
      {
      //      fprintf(stderr, "Set font %s -> %s\n", r->font, val->val_str);
      r->font = bg_strdup(r->font, val->val_str);
      r->font_changed = 1;
      }
    }
  /* OSD */
  else if(!strcmp(name, "font_file"))
    {
    if(!r->font_file || strcmp(val->val_str, r->font_file))
      {
      //      fprintf(stderr, "Set font %s -> %s\n", r->font_file, val->val_str);
      r->font_file = bg_strdup(r->font_file, val->val_str);
      r->font_changed = 1;
      }
    }
  else if(!strcmp(name, "font_size"))
    {
    if(r->font_size != val->val_f)
      {
      //      fprintf(stderr, "Set font %s -> %s\n", r->font_file, val->val_str);
      r->font_size = val->val_f;
      r->font_changed = 1;
      }
    }
  
  /* */
  else if(!strcmp(name, "color"))
    {
    r->color[0] = val->val_color[0];
    r->color[1] = val->val_color[1];
    r->color[2] = val->val_color[2];
    r->color[3] = 0.0;
    r->alpha_f  = val->val_color[3];
    }
#ifdef FT_STROKER_H
  else if(!strcmp(name, "border_color"))
    {
    r->color_stroke[0] = val->val_color[0];
    r->color_stroke[1] = val->val_color[1];
    r->color_stroke[2] = val->val_color[2];
    r->color_stroke[3] = 0.0;
    }
  else if(!strcmp(name, "border_width"))
    {
    r->border_width = val->val_f;
    }
#endif
  else if(!strcmp(name, "cache_size"))
    {
    alloc_glyph_cache(r, val->val_i);
    }
  else if(!strcmp(name, "justify_h"))
    {
    if(!strcmp(val->val_str, "left"))
      r->justify_h = JUSTIFY_LEFT;
    else if(!strcmp(val->val_str, "right"))
      r->justify_h = JUSTIFY_RIGHT;
    else if(!strcmp(val->val_str, "center"))
      r->justify_h = JUSTIFY_CENTER;
    }
  else if(!strcmp(name, "justify_v"))
    {
    if(!strcmp(val->val_str, "top"))
      r->justify_v = JUSTIFY_TOP;
    else if(!strcmp(val->val_str, "bottom"))
      r->justify_v = JUSTIFY_BOTTOM;
    else if(!strcmp(val->val_str, "center"))
      r->justify_v = JUSTIFY_CENTER;
    }
  else if(!strcmp(name, "border_left"))
    {
    r->border_left = val->val_i;
    }
  else if(!strcmp(name, "border_right"))
    {
    r->border_right = val->val_i;
    }
  else if(!strcmp(name, "border_top"))
    {
    r->border_top = val->val_i;
    }
  else if(!strcmp(name, "border_bottom"))
    {
    r->border_bottom = val->val_i;
    }
  else if(!strcmp(name, "ignore_linebreaks"))
    {
    r->ignore_linebreaks = val->val_i;
    }
  r->config_changed = 1;
  pthread_mutex_unlock(&(r->config_mutex));
  }

/* Copied from gavl */

#define r_float_to_y  0.29900
#define g_float_to_y  0.58700
#define b_float_to_y  0.11400

#define r_float_to_u  (-0.16874)
#define g_float_to_u  (-0.33126)
#define b_float_to_u   0.50000

#define r_float_to_v   0.50000
#define g_float_to_v  (-0.41869)
#define b_float_to_v  (-0.08131)

#define Y_FLOAT_TO_8(val) (int)(val * 219.0) + 16;
#define UV_FLOAT_TO_8(val) (int)(val * 224.0) + 128;

#define RGB_FLOAT_TO_Y_8(r, g, b, y)                              \
  y_tmp = r_float_to_y * r + g_float_to_y * g + b_float_to_y * b; \
  y = Y_FLOAT_TO_8(y_tmp);

#define RGB_FLOAT_TO_YUV_8(r, g, b, y, u, v)                      \
  RGB_FLOAT_TO_Y_8(r, g, b, y)                                    \
  u_tmp = r_float_to_u * r + g_float_to_u * g + b_float_to_u * b; \
  v_tmp = r_float_to_v * r + g_float_to_v * g + b_float_to_v * b; \
  u = UV_FLOAT_TO_8(u_tmp);                                        \
  v = UV_FLOAT_TO_8(v_tmp);


static
void init_nolock(bg_text_renderer_t * r)
  {
  int bits;
#ifdef FT_STROKER_H 
  float y_tmp, u_tmp, v_tmp;
#endif  

  if((r->frame_format.image_width  != r->last_frame_format.image_width) ||
     (r->frame_format.image_height != r->last_frame_format.image_height) ||
     !r->last_frame_format.pixel_width || !r->frame_format.pixel_height ||
     (r->frame_format.pixel_width  * r->last_frame_format.pixel_height !=
      r->frame_format.pixel_height * r->last_frame_format.pixel_width))
    r->font_changed = 1;

  gavl_video_format_copy(&r->last_frame_format, &r->frame_format);
  
  
  /* Load font if necessary */
  if(r->font_changed || !r->face)
    load_font(r);
  
  /* Copy formats */

  gavl_video_format_copy(&r->overlay_format, &r->frame_format);
  
  /* Decide about overlay format */

  gavl_pixelformat_chroma_sub(r->frame_format.pixelformat,
                              &r->sub_h, &r->sub_v);
  
  if(gavl_pixelformat_is_rgb(r->frame_format.pixelformat))
    {
    bits = 8*gavl_pixelformat_bytes_per_pixel(r->frame_format.pixelformat);
    if(bits <= 32)
      {
      r->overlay_format.pixelformat = GAVL_RGBA_32;
      r->alpha_i = (int)(r->alpha_f*255.0+0.5);
#ifdef FT_STROKER_H 
      r->color_i[0] = (int)(r->color[0]*255.0+0.5);
      r->color_i[1] = (int)(r->color[1]*255.0+0.5);
      r->color_i[2] = (int)(r->color[2]*255.0+0.5);
      r->color_i[3] = (int)(r->color[3]*255.0+0.5);
#endif
      r->render_func = render_rgba_32;
      }
    else if(bits <= 64)
      {
      r->overlay_format.pixelformat = GAVL_RGBA_64;
      r->alpha_i = (int)(r->alpha_f*65535.0+0.5);
#ifdef FT_STROKER_H 
      r->color_i[0] = (int)(r->color[0]*65535.0+0.5);
      r->color_i[1] = (int)(r->color[1]*65535.0+0.5);
      r->color_i[2] = (int)(r->color[2]*65535.0+0.5);
      r->color_i[3] = (int)(r->color[3]*65535.0+0.5);
#endif
      r->render_func = render_rgba_64;
      }
    else
      {
      r->overlay_format.pixelformat = GAVL_RGBA_FLOAT;
      r->render_func = render_rgba_float;
      }
    }
  else
    {
    r->overlay_format.pixelformat = GAVL_YUVA_32;
    r->alpha_i = (int)(r->alpha_f*255.0+0.5);
#ifdef FT_STROKER_H 

    RGB_FLOAT_TO_YUV_8(r->color[0],
                       r->color[1],
                       r->color[2],
                       r->color_i[0],
                       r->color_i[1],
                       r->color_i[2]);
    r->color_i[3] = (int)(r->color[3]*255.0+0.5);
#endif
    r->render_func = render_rgba_32;
    }

  /* */
  
  gavl_rectangle_i_set_all(&(r->max_bbox), &(r->overlay_format));

  gavl_rectangle_i_crop_left(&(r->max_bbox), r->border_left);
  gavl_rectangle_i_crop_right(&(r->max_bbox), r->border_right);
  gavl_rectangle_i_crop_top(&(r->max_bbox), r->border_top);
  gavl_rectangle_i_crop_bottom(&(r->max_bbox), r->border_bottom);
  
  }

void bg_text_renderer_init(bg_text_renderer_t * r,
                           const gavl_video_format_t * frame_format,
                           gavl_video_format_t * overlay_format)
  {
  pthread_mutex_lock(&r->config_mutex);

  gavl_video_format_copy(&(r->frame_format), frame_format);

  init_nolock(r);

  gavl_video_format_copy(overlay_format, &(r->overlay_format));

  r->config_changed = 0;
  
  pthread_mutex_unlock(&r->config_mutex);
  }
  
static void flush_line(bg_text_renderer_t * r, gavl_video_frame_t * f,
                       cache_entry_t ** glyphs, int len, int * line_y)
  {
  int line_x, j;
  int line_width;
  //  fprintf(stderr, "flush_line, len: %d\n", len);

  if(!len)
    return;
  
  line_width = 0;
  line_width = -glyphs[0]->bbox.xmin;
  //  fprintf(stderr, "Line width: %d\n", line_width);
  /* Compute the length of the line */
  for(j = 0; j < len-1; j++)
    {
    line_width += glyphs[j]->advance_x;

    /* Adjust the y-position, if the glyph extends beyond the top
       border of the frame. This should happen only for the first
       line */

    if(*line_y < -glyphs[j]->bbox.ymin)
      *line_y = -glyphs[j]->bbox.ymin;
    }
  line_width += glyphs[len - 1]->bbox.xmax;

  if(*line_y < -glyphs[len - 1]->bbox.ymin)
    *line_y = -glyphs[len - 1]->bbox.ymin;
  
  switch(r->justify_h)
    {
    case JUSTIFY_CENTER:
      line_x = (r->max_bbox.w - line_width) >> 1;
      break;
    case JUSTIFY_LEFT:
      line_x = 0;
      break;
    case JUSTIFY_RIGHT:
      line_x = r->max_bbox.w - line_width;
      break;
    }

  line_x -= glyphs[0]->bbox.xmin;
  //  fprintf(stderr, "Flush line %d %d\n", line_x, line_y);
  
  for(j = 0; j < len; j++)
    {
    adjust_bbox(glyphs[j], line_x, *line_y, &r->bbox);
    r->render_func(r, glyphs[j], f, &line_x, line_y);
    }
  }

void bg_text_renderer_render(bg_text_renderer_t * r, const char * string,
                             gavl_overlay_t * ovl)
  {
  cache_entry_t ** glyphs = (cache_entry_t **)0;
  uint32_t * string_unicode = (uint32_t *)0;
  int len, i;
  int pos_x, pos_y;
  int line_start, line_end;
  int line_width, line_end_y;
  int line_offset;
  int break_word; /* Linebreak within a (long) word */
  pthread_mutex_lock(&r->config_mutex);
  
  if(r->config_changed)
    init_nolock(r);
  
  //  fprintf(stderr, "bg_text_renderer_render: string:\n\n%s\n\n", string);
    
  r->bbox.xmin = r->overlay_format.image_width;
  r->bbox.ymin = r->overlay_format.image_height;

  r->bbox.xmax = 0;
  r->bbox.ymax = 0;

#ifdef FT_STROKER_H
  gavl_video_frame_fill(ovl->frame, &(r->overlay_format), r->color_stroke);
#else
  gavl_video_frame_fill(ovl->frame, &(r->overlay_format), r->color);
#endif
  
  /* Convert string */

  string_unicode = (uint32_t*)bg_convert_string(r->cnv, string, -1, &len);
  len /= 4;
  
  //  fprintf(stderr, "Len: %d\n", len);
  line_offset = r->face->size->metrics.height >> 6;
  
  glyphs = malloc(len * sizeof(*glyphs));

  if(r->ignore_linebreaks)
    {
    for(i = 0; i < len; i++)
      {
      if(string_unicode[i] == '\n')
        string_unicode[i] = ' ';
      }
    }
  for(i = 0; i < len; i++)
    {
    glyphs[i] = get_glyph(r, string_unicode[i]);
    if(!glyphs[i])
      glyphs[i] = get_glyph(r, '?');
    }
  //  fprintf(stderr, "pos_y: %d\n", pos_y);

  line_start = 0;
  line_end   = -1;
  break_word = 0;
  
  pos_x = 0;
  pos_y = r->face->size->metrics.ascender >> 6;
  line_width = 0;
  for(i = 0; i < len; i++)
    {
    if((string_unicode[i] == ' ') ||
       (string_unicode[i] == '\n') ||
       (i == len))
      {
      line_end = i;
      line_width = pos_x;
      line_end_y = pos_y;
      break_word = 0;
      }
    
    // fprintf(stderr, "Checking '%c', x: %d\n", string_unicode[i], pos_x);
    
    /* Linebreak */
    if((pos_x + (glyphs[i]->advance_x) > r->max_bbox.w) ||
       (string_unicode[i] == '\n'))
      {
      //      fprintf(stderr, "Linebreak\n");
      
      if(line_end < 0)
        {
        line_width = pos_x;
        line_end_y = pos_y;
        line_end = i;
        break_word = 1;
        }
      /* Render the line */
      
      flush_line(r, ovl->frame,
                 &glyphs[line_start], line_end - line_start,
                 &pos_y);
      
      pos_x -= line_width;
      
      pos_y += line_offset;
      
      if((pos_y + (r->face->size->metrics.descender >> 6)) > r->max_bbox.h)
        break;
      
      line_start = line_end;
      if(!break_word)
        line_start++;
      line_end = -1;
      }
    pos_x += glyphs[i]->advance_x;

    }
  if(len - line_start)
    {
    flush_line(r, ovl->frame,
               &glyphs[line_start], len - line_start,
               &pos_y);
    }
#if 0
  fprintf(stderr, "bounding_box: %d,%d -> %d,%d\n",
          r->bbox.xmin, r->bbox.ymin, r->bbox.xmax, r->bbox.ymax);
#endif
  
  ovl->ovl_rect.x = r->bbox.xmin;
  ovl->ovl_rect.y = r->bbox.ymin;

  ovl->ovl_rect.w = r->bbox.xmax - r->bbox.xmin;
  ovl->ovl_rect.h = r->bbox.ymax - r->bbox.ymin;

  /* Align to subsampling */
  ovl->ovl_rect.w += r->sub_h - (ovl->ovl_rect.w % r->sub_h);
  ovl->ovl_rect.h += r->sub_v - (ovl->ovl_rect.h % r->sub_v);
  
  switch(r->justify_h)
    {
    case JUSTIFY_LEFT:
    case JUSTIFY_CENTER:
      ovl->dst_x = ovl->ovl_rect.x + r->border_left;
      
      if(ovl->dst_x % r->sub_h)
        ovl->dst_x += r->sub_h - (ovl->dst_x % r->sub_h);
      
      break;
    case JUSTIFY_RIGHT:
      ovl->dst_x = r->overlay_format.image_width - r->border_right - ovl->ovl_rect.w;
      ovl->dst_x -= (ovl->dst_x % r->sub_h);
      break;
    }

  switch(r->justify_v)
    {
    case JUSTIFY_TOP:
      ovl->dst_y = r->border_top;
      ovl->dst_y += r->sub_v - (ovl->dst_y % r->sub_v);

      break;
    case JUSTIFY_CENTER:
      ovl->dst_y = r->border_top +
        ((r->overlay_format.image_height - ovl->ovl_rect.h)>>1);

      if(ovl->dst_y % r->sub_v)
        ovl->dst_y += r->sub_v - (ovl->dst_y % r->sub_v);

      break;
    case JUSTIFY_BOTTOM:
      ovl->dst_y = r->overlay_format.image_height - r->border_bottom - ovl->ovl_rect.h;
      ovl->dst_y -= (ovl->dst_y % r->sub_v);
      break;
    }
  if(glyphs)
    free(glyphs);
  if(string_unicode)
    free(string_unicode);

  pthread_mutex_unlock(&r->config_mutex);
  }

