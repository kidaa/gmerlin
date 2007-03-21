/*****************************************************************

  scale_bicubic_c.c

  Copyright (c) 2005 by Burkhard Plaum - plaum@ipf.uni-stuttgart.de

  http://gmerlin.sourceforge.net

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.

*****************************************************************/

#include <stdio.h>
#include <gavl/gavl.h>
#include <video.h>
#include <scale.h>

/* Packed formats for 15/16 colors (idea from libvisual) */

typedef struct {
	uint16_t b:5, g:6, r:5;
} color_16;

typedef struct {
	uint16_t b:5, g:5, r:5;
} color_15;

#ifdef NOCLIP
#define RECLIP_H(a,idx)
#define RECLIP_V(a,idx)
#define RECLIP_FLOAT(a)

#else

#define RECLIP_H(a,idx) \
  if(a < ctx->min_values_h[idx]) a = ctx->min_values_h[idx];    \
  if(a > ctx->max_values_h[idx]) a = ctx->max_values_h[idx]

#define RECLIP_V(a,idx) \
  if(a < ctx->min_values_v[idx]) a = ctx->min_values_v[idx];    \
  if(a > ctx->max_values_v[idx]) a = ctx->max_values_v[idx]

#define RECLIP_FLOAT(a) if(a < 0.0) a = 0.0; if(a > 1.0) a = 1.0

#endif

/* x-Direction */

#define FUNC_NAME scale_rgb_15_x_bicubic_c
#define TYPE color_15
#define INIT int64_t tmp;
#define SCALE                                                \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->r +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->r +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->r +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->r);   \
  RECLIP_H(tmp,0);                                                        \
  dst->r = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->g +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->g +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->g +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->g);   \
  RECLIP_H(tmp,1);                                                        \
  dst->g = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->b +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->b +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->b +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->b);   \
  RECLIP_H(tmp,2);                                                        \
  dst->b = tmp >> 16;                                                    \

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_rgb_16_x_bicubic_c
#define TYPE color_16
#define INIT int64_t tmp;
#define SCALE \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->r +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->r +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->r +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->r);       \
  RECLIP_H(tmp,0);                                                     \
  dst->r = tmp >> 16;                                               \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->g +      \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->g +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->g +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->g);       \
  RECLIP_H(tmp,1);                                                     \
  dst->g = tmp >> 16;                                               \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1->b + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2->b +    \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3->b +    \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4->b);\
  RECLIP_H(tmp,2);                                                     \
  dst->b = tmp >> 16;                                               \

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint8_x_1_x_bicubic_c
#define TYPE uint8_t
#define INIT int64_t tmp;
#define SCALE \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]);            \
  RECLIP_H(tmp,ctx->plane);                                               \
  dst[0] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint8_x_3_x_bicubic_c
#define TYPE uint8_t
#define INIT int64_t tmp;
#define SCALE \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]);       \
  RECLIP_H(tmp,0);                                                         \
  dst[0] = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[1]);       \
  RECLIP_H(tmp,1);                                                          \
  dst[1] = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[2]);       \
  RECLIP_H(tmp,2);                                                          \
  dst[2] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint8_x_4_x_bicubic_c
#define TYPE uint8_t
#define INIT int64_t tmp;
#define SCALE \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]);            \
  RECLIP_H(tmp,0);                                                         \
  dst[0] = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[1] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[1]);            \
  RECLIP_H(tmp,1);                                                          \
  dst[1] = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[2] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[2]);            \
  RECLIP_H(tmp,2);                                                          \
  dst[2] = tmp >> 16;                                                    \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[3] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[3] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[3] +            \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[3]);            \
  RECLIP_H(tmp,3);                                                          \
  dst[3] = tmp >> 16;
 
#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint16_x_1_x_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp;
#define SCALE                                                \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]); \
  RECLIP_H(tmp,ctx->plane);                                             \
  dst[0] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint16_x_3_x_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp;
#define SCALE                                                           \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] +   \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]); \
  RECLIP_H(tmp,0);                                                       \
  dst[0] = tmp >> 16; \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[1]); \
  RECLIP_H(tmp,1);                                               \
  dst[1] = tmp >> 16; \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[2]); \
  RECLIP_H(tmp,2);                                               \
  dst[2] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_uint16_x_4_x_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp;
#define SCALE                                                           \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[0] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[0]); \
  RECLIP_H(tmp,0);                                               \
  dst[0] = tmp >> 16; \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[1] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[1]); \
  RECLIP_H(tmp,1);                                               \
  dst[1] = tmp >> 16; \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[2] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[2]); \
  RECLIP_H(tmp,2);                                               \
  dst[2] = tmp >> 16; \
  tmp = ((int64_t)ctx->table_h.pixels[i].factor_i[0] * src_1[3] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[1] * src_2[3] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[2] * src_3[3] + \
         (int64_t)ctx->table_h.pixels[i].factor_i[3] * src_4[3]); \
  RECLIP_H(tmp,3);                                                       \
  dst[3] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_x.h"


#define FUNC_NAME scale_float_x_3_x_bicubic_c
#define TYPE float
#define SCALE                                                   \
  dst[0] = (ctx->table_h.pixels[i].factor_f[0] * src_1[0] + \
            ctx->table_h.pixels[i].factor_f[1] * src_2[0] +         \
            ctx->table_h.pixels[i].factor_f[2] * src_3[0] +         \
            ctx->table_h.pixels[i].factor_f[3] * src_4[0]);         \
  RECLIP_FLOAT(dst[0]);                                                 \
  dst[1] = (ctx->table_h.pixels[i].factor_f[0] * src_1[1] + \
            ctx->table_h.pixels[i].factor_f[1] * src_2[1] + \
            ctx->table_h.pixels[i].factor_f[2] * src_3[1] + \
            ctx->table_h.pixels[i].factor_f[3] * src_4[1]);         \
  RECLIP_FLOAT(dst[1]);                                                 \
  dst[2] = (ctx->table_h.pixels[i].factor_f[0] * src_1[2] + \
            ctx->table_h.pixels[i].factor_f[1] * src_2[2] + \
            ctx->table_h.pixels[i].factor_f[2] * src_3[2] + \
            ctx->table_h.pixels[i].factor_f[3] * src_4[2]);\
  RECLIP_FLOAT(dst[2]);                                        \


#define NUM_TAPS 4
#include "scale_x.h"

#define FUNC_NAME scale_float_x_4_x_bicubic_c
#define TYPE float
#define SCALE                                                           \
  dst[0] = (ctx->table_h.pixels[i].factor_f[0] * src_1[0] +         \
            ctx->table_h.pixels[i].factor_f[1] * src_2[0] +         \
            ctx->table_h.pixels[i].factor_f[2] * src_3[0] +         \
            ctx->table_h.pixels[i].factor_f[3] * src_4[0]);         \
  RECLIP_FLOAT(dst[0]);                                                 \
  dst[1] = (ctx->table_h.pixels[i].factor_f[0] * src_1[1] +         \
            ctx->table_h.pixels[i].factor_f[1] * src_2[1] +         \
            ctx->table_h.pixels[i].factor_f[2] * src_3[1] +         \
            ctx->table_h.pixels[i].factor_f[3] * src_4[1]);         \
  RECLIP_FLOAT(dst[1]);                                                 \
  dst[2] = (ctx->table_h.pixels[i].factor_f[0] * src_1[2] +         \
            ctx->table_h.pixels[i].factor_f[1] * src_2[2] +         \
            ctx->table_h.pixels[i].factor_f[2] * src_3[2] +         \
            ctx->table_h.pixels[i].factor_f[3] * src_4[2]);         \
  RECLIP_FLOAT(dst[2]);                                                 \
  dst[3] = (ctx->table_h.pixels[i].factor_f[0] * src_1[3] +         \
            ctx->table_h.pixels[i].factor_f[1] * src_2[3] +         \
            ctx->table_h.pixels[i].factor_f[2] * src_3[3] +         \
            ctx->table_h.pixels[i].factor_f[3] * src_4[3]);\
  RECLIP_FLOAT(dst[3]);                                        \

#define NUM_TAPS 4
#include "scale_x.h"

/* y-Direction */

#define FUNC_NAME scale_rgb_15_y_bicubic_c
#define TYPE color_15
#define INIT int64_t fac_1, fac_2, fac_3, fac_4;        \
  int64_t tmp; \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define NO_UINT8

#define SCALE                                                           \
  tmp = (fac_1 * src_1->r + \
         fac_2 * src_2->r +    \
         fac_3 * src_3->r +                             \
         fac_4 * src_4->r);                        \
  RECLIP_V(tmp, 0);                                       \
  dst->r = tmp >> 16;                                    \
  tmp = (fac_1 * src_1->g +                             \
         fac_2 * src_2->g +                             \
         fac_3 * src_3->g +                             \
         fac_4 * src_4->g);                        \
  RECLIP_V(tmp, 1);                                       \
  dst->g = tmp >> 16;                                    \
  tmp = (fac_1 * src_1->b +                             \
         fac_2 * src_2->b +                             \
         fac_3 * src_3->b +                             \
         fac_4 * src_4->b);                        \
  RECLIP_V(tmp, 2);                                       \
  dst->b = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_rgb_16_y_bicubic_c
#define TYPE color_16
#define INIT int64_t fac_1, fac_2, fac_3, fac_4;\
  int64_t tmp;                                                  \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];


#define NO_UINT8

#define SCALE                                           \
  tmp = (fac_1 * src_1->r +                             \
         fac_2 * src_2->r +                             \
         fac_3 * src_3->r +                             \
         fac_4 * src_4->r);                             \
  RECLIP_V(tmp, 0);                                       \
  dst->r = tmp >> 16;                                     \
  tmp = (fac_1 * src_1->g +                             \
         fac_2 * src_2->g +                             \
         fac_3 * src_3->g +                             \
         fac_4 * src_4->g);                        \
  RECLIP_V(tmp, 1);                                       \
  dst->g = tmp >> 16;                                     \
  tmp = (fac_1 * src_1->b +                             \
         fac_2 * src_2->b +                             \
         fac_3 * src_3->b +                             \
         fac_4 * src_4->b);                        \
  RECLIP_V(tmp, 2);                                       \
  dst->b = tmp >> 16;                                     \

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_uint8_x_1_y_bicubic_c
#define TYPE uint8_t
#define INIT int64_t fac_1, fac_2, fac_3, fac_4, tmp;               \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define SCALE                  \
  tmp = (fac_1 * src_1[0] + \
         fac_2 * src_2[0] +    \
         fac_3 * src_3[0] +                     \
         fac_4 * src_4[0]);                     \
  RECLIP_V(tmp, ctx->plane);                      \
  dst[0] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_uint8_x_3_y_bicubic_c
#define TYPE uint8_t
#define INIT int64_t fac_1, fac_2, fac_3, fac_4, tmp;               \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define SCALE               \
  tmp = (fac_1 * src_1[0] +    \
         fac_2 * src_2[0] +                     \
         fac_3 * src_3[0] +                      \
         fac_4 * src_4[0]);                 \
  RECLIP_V(tmp, 0);                                \
  dst[0] = tmp >> 16;                             \
  tmp = (fac_1 * src_1[1] +                             \
         fac_2 * src_2[1] +                             \
         fac_3 * src_3[1] +                             \
         fac_4 * src_4[1]);                        \
  RECLIP_V(tmp, ctx->plane);                              \
  dst[1] = tmp >> 16;                                    \
  tmp = (fac_1 * src_1[2] +                             \
         fac_2 * src_2[2] +                             \
         fac_3 * src_3[2] +                             \
         fac_4 * src_4[2]);                        \
  RECLIP_V(tmp, ctx->plane);                              \
  dst[2] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_uint8_x_4_y_bicubic_c
#define TYPE uint8_t
#define INIT int64_t fac_1, fac_2, fac_3, fac_4, tmp;               \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define SCALE               \
  tmp = (fac_1 * src_1[0] +    \
         fac_2 * src_2[0] +                     \
         fac_3 * src_3[0] +                      \
         fac_4 * src_4[0]);                 \
  RECLIP_V(tmp, 0);                                \
  dst[0] = tmp >> 16;                             \
  tmp = (fac_1 * src_1[1] +                             \
         fac_2 * src_2[1] +                             \
         fac_3 * src_3[1] +                             \
         fac_4 * src_4[1]);                        \
  RECLIP_V(tmp, 1);                              \
  dst[1] = tmp >> 16;                                    \
  tmp = (fac_1 * src_1[2] +                             \
         fac_2 * src_2[2] +                             \
         fac_3 * src_3[2] +                             \
         fac_4 * src_4[2]);                        \
  RECLIP_V(tmp, 2);                                       \
  dst[2] = tmp >> 16;                                    \
  tmp = (fac_1 * src_1[3] +                             \
         fac_2 * src_2[3] +                             \
         fac_3 * src_3[3] +                             \
         fac_4 * src_4[3]);                        \
  RECLIP_V(tmp, 3);                              \
  dst[3] = tmp >> 16;



#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_uint16_x_1_y_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp; \
  int64_t fac_1, fac_2, fac_3, fac_4;                           \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];   \
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];   \
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];   \
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define NO_UINT8

#define SCALE                                  \
  tmp = (fac_1 * src_1[0] + \
         fac_2 * src_2[0] + \
         fac_3 * src_3[0] + \
         fac_4 * src_4[0]); \
  RECLIP_V(tmp, ctx->plane);                    \
  dst[0] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"


#define FUNC_NAME scale_uint16_x_3_y_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp;                      \
  int64_t fac_1, fac_2, fac_3, fac_4;                           \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define NO_UINT8

#define SCALE                                  \
  tmp = (fac_1 * src_1[0] +                    \
         fac_2 * src_2[0] +                    \
         fac_3 * src_3[0] +                    \
         fac_4 * src_4[0]);                    \
  RECLIP_V(tmp, 0);                              \
  dst[0] = tmp >> 16;                          \
  tmp = (fac_1 * src_1[1] +                    \
         fac_2 * src_2[1] +                    \
         fac_3 * src_3[1] +                    \
         fac_4 * src_4[1]);                    \
  RECLIP_V(tmp, 1);                              \
  dst[1] = tmp >> 16;                          \
  tmp = (fac_1 * src_1[2] +                    \
         fac_2 * src_2[2] +                    \
         fac_3 * src_3[2] +                    \
         fac_4 * src_4[2]);                    \
  RECLIP_V(tmp, 2);                              \
  dst[2] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_uint16_x_4_y_bicubic_c
#define TYPE uint16_t
#define INIT int64_t tmp;\
  int64_t fac_1, fac_2, fac_3, fac_4;\
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_i[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_i[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_i[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_i[3];

#define NO_UINT8

#define SCALE                                  \
  tmp = (fac_1 * src_1[0] + \
         fac_2 * src_2[0] + \
         fac_3 * src_3[0] + \
         fac_4 * src_4[0]); \
  RECLIP_V(tmp, 0);                              \
  dst[0] = tmp >> 16; \
  tmp = (fac_1 * src_1[1] + \
         fac_2 * src_2[1] + \
         fac_3 * src_3[1] + \
         fac_4 * src_4[1]); \
  RECLIP_V(tmp, 1);                              \
  dst[1] = tmp >> 16; \
  tmp = (fac_1 * src_1[2] + \
         fac_2 * src_2[2] + \
         fac_3 * src_2[2] + \
         fac_4 * src_4[2]); \
  RECLIP_V(tmp, 2);                              \
  dst[2] = tmp >> 16; \
  tmp = (fac_1 * src_1[3] + \
         fac_2 * src_2[3] + \
         fac_3 * src_3[3] + \
         fac_4 * src_4[3]); \
  RECLIP_V(tmp, 3);                              \
  dst[3] = tmp >> 16;

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_float_x_3_y_bicubic_c
#define TYPE float
#define INIT float fac_1, fac_2, fac_3, fac_4;\
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_f[0];\
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_f[1];\
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_f[2];\
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_f[3];

#define NO_UINT8
  
#define SCALE                  \
  dst[0] = (fac_1 * src_1[0] + \
            fac_2 * src_2[0] + \
            fac_3 * src_3[0] + \
            fac_4 * src_4[0]); \
  RECLIP_FLOAT(dst[0]);        \
  dst[1] = (fac_1 * src_1[1] + \
            fac_2 * src_2[1] + \
            fac_3 * src_3[1] + \
            fac_4 * src_4[1]); \
  RECLIP_FLOAT(dst[1]);        \
  dst[2] = (fac_1 * src_1[2] + \
            fac_2 * src_2[2] + \
            fac_3 * src_3[2] + \
            fac_4 * src_4[2]);\
  RECLIP_FLOAT(dst[2]);

#define NUM_TAPS 4
#include "scale_y.h"

#define FUNC_NAME scale_float_x_4_y_bicubic_c
#define TYPE float
#define INIT float fac_1, fac_2, fac_3, fac_4;                  \
  fac_1 = ctx->table_v.pixels[ctx->scanline].factor_f[0];   \
  fac_2 = ctx->table_v.pixels[ctx->scanline].factor_f[1];   \
  fac_3 = ctx->table_v.pixels[ctx->scanline].factor_f[2];   \
  fac_4 = ctx->table_v.pixels[ctx->scanline].factor_f[3];

#define NO_UINT8

#define SCALE                  \
  dst[0] = (fac_1 * src_1[0] + \
            fac_2 * src_2[0] + \
            fac_3 * src_3[0] + \
            fac_4 * src_4[0]); \
  RECLIP_FLOAT(dst[0]);        \
  dst[1] = (fac_1 * src_1[1] + \
            fac_2 * src_2[1] + \
            fac_3 * src_3[1] + \
            fac_4 * src_4[1]); \
  RECLIP_FLOAT(dst[1]);                           \
  dst[2] = (fac_1 * src_1[2] +                    \
            fac_2 * src_2[2] +                    \
            fac_3 * src_3[2] +                    \
            fac_4 * src_4[2]);                    \
  RECLIP_FLOAT(dst[2]);                           \
  dst[3] = (fac_1 * src_1[3] +                    \
            fac_2 * src_2[3] +                    \
            fac_3 * src_3[3] +                    \
            fac_4 * src_4[3]);                    \
  RECLIP_FLOAT(dst[3]);

#define NUM_TAPS 4
#include "scale_y.h"

#ifdef NOCLIP
void gavl_init_scale_funcs_bicubic_noclip_c(gavl_scale_funcs_t * tab)
#else
void gavl_init_scale_funcs_bicubic_c(gavl_scale_funcs_t * tab)
#endif
  {
  //  fprintf(stderr, "gavl_init_scale_funcs_bicubic_c\n");
  tab->funcs_x.scale_rgb_15 =     scale_rgb_15_x_bicubic_c;
  tab->funcs_x.scale_rgb_16 =     scale_rgb_16_x_bicubic_c;
  tab->funcs_x.scale_uint8_x_1_advance =  scale_uint8_x_1_x_bicubic_c;
  tab->funcs_x.scale_uint8_x_1_noadvance =  scale_uint8_x_1_x_bicubic_c;
  tab->funcs_x.scale_uint8_x_3 =  scale_uint8_x_3_x_bicubic_c;
  tab->funcs_x.scale_uint8_x_4 =  scale_uint8_x_4_x_bicubic_c;
  tab->funcs_x.scale_uint16_x_1 = scale_uint16_x_1_x_bicubic_c;
  tab->funcs_x.scale_uint16_x_3 = scale_uint16_x_3_x_bicubic_c;
  tab->funcs_x.scale_uint16_x_4 = scale_uint16_x_4_x_bicubic_c;
  tab->funcs_x.scale_float_x_3 =  scale_float_x_3_x_bicubic_c;
  tab->funcs_x.scale_float_x_4 =  scale_float_x_4_x_bicubic_c;
  tab->funcs_x.bits_rgb_15 = 16;
  tab->funcs_x.bits_rgb_16 = 16;
  tab->funcs_x.bits_uint8_advance  = 16;
  tab->funcs_x.bits_uint8_noadvance  = 16;
  tab->funcs_x.bits_uint16 = 16;

  tab->funcs_y.scale_rgb_15 =     scale_rgb_15_y_bicubic_c;
  tab->funcs_y.scale_rgb_16 =     scale_rgb_16_y_bicubic_c;
  tab->funcs_y.scale_uint8_x_1_advance =  scale_uint8_x_1_y_bicubic_c;
  tab->funcs_y.scale_uint8_x_1_noadvance =  scale_uint8_x_1_y_bicubic_c;
  tab->funcs_y.scale_uint8_x_3 =  scale_uint8_x_3_y_bicubic_c;
  tab->funcs_y.scale_uint8_x_4 =  scale_uint8_x_4_y_bicubic_c;
  tab->funcs_y.scale_uint16_x_1 = scale_uint16_x_1_y_bicubic_c;
  tab->funcs_y.scale_uint16_x_3 = scale_uint16_x_3_y_bicubic_c;
  tab->funcs_y.scale_uint16_x_4 = scale_uint16_x_4_y_bicubic_c;
  tab->funcs_y.scale_float_x_3 =  scale_float_x_3_y_bicubic_c;
  tab->funcs_y.scale_float_x_4 =  scale_float_x_4_y_bicubic_c;
  tab->funcs_y.bits_rgb_15 = 16;
  tab->funcs_y.bits_rgb_16 = 16;
  tab->funcs_y.bits_uint8_advance  = 16;
  tab->funcs_y.bits_uint8_noadvance  = 16;
  tab->funcs_y.bits_uint16 = 16;
  
  }
