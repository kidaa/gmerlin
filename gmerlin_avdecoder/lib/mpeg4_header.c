/*****************************************************************
 * gmerlin-avdecoder - a general purpose multimedia decoding library
 *
 * Copyright (c) 2001 - 2008 Members of the Gmerlin project
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

#include <avdec_private.h>
#include <mpeg4_header.h>

/* log2 ripped from ffmpeg (maybe move to central place?) */

const uint8_t log2_tab[256]={
        0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

static inline int bgav_log2(unsigned int v)
{
    int n = 0;
    if (v & 0xffff0000) {
        v >>= 16;
        n += 16;
    }
    if (v & 0xff00) {
        v >>= 8;
        n += 8;
    }
    n += log2_tab[v];

    return n;
}


int bgav_mpeg4_get_start_code(const uint8_t * data)
  {
  if(data[3] <= 0x1f)
    return MPEG4_CODE_VO_START;
  else if(data[3] <= 0x2f)
    return MPEG4_CODE_VOL_START;
  else if(data[3] == 0xb6)
    return MPEG4_CODE_VOP_START;
  return 0;
  }

int bgav_mpeg4_vol_header_read(const bgav_options_t * opt,
                               bgav_mpeg4_vol_header_t * ret,
                               const uint8_t * buffer, int len)
  {
  bgav_bitstream_t b;
  int dummy;

  bgav_bitstream_init(&b, buffer, len);

  if(!bgav_bitstream_get(&b, &ret->random_accessible_vol, 1) ||
     !bgav_bitstream_get(&b, &ret->video_object_type_indication, 8) ||
     !bgav_bitstream_get(&b, &ret->is_object_layer_identifier, 1))
    return 0;

  if(ret->is_object_layer_identifier)
    {
    if(!bgav_bitstream_get(&b, &ret->video_object_layer_verid, 4) ||
       !bgav_bitstream_get(&b, &ret->video_object_layer_priority, 3))
      return 0;
    }

  if(!bgav_bitstream_get(&b, &ret->aspect_ratio_info, 4))
    return 0;
  
  if(ret->aspect_ratio_info == 15) // extended PAR
    {
    if(!bgav_bitstream_get(&b, &ret->par_width, 8) ||
       !bgav_bitstream_get(&b, &ret->par_height, 8))
      return 0;
    }

  if(!bgav_bitstream_get(&b, &ret->vol_control_parameters, 1))
    return 0;
  if(ret->vol_control_parameters)
    {
    if(!bgav_bitstream_get(&b, &ret->chroma_format, 2) ||
       !bgav_bitstream_get(&b, &ret->low_delay, 1) ||
       !bgav_bitstream_get(&b, &ret->vbv_parameters, 1))
      return 0;

    if(ret->vbv_parameters)
      {
      if(!bgav_bitstream_get(&b, &ret->first_half_bit_rate, 15) ||
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_bit_rate, 15) || 
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->first_half_vbv_buffer_size, 15) || 
         !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_vbv_buffer_size, 3) ||
         !bgav_bitstream_get(&b, &ret->first_half_vbv_occupancy, 11) ||
         !bgav_bitstream_get(&b, &dummy, 1) ||  /* int marker_bit; */
         !bgav_bitstream_get(&b, &ret->latter_half_vbv_occupancy, 15) ||
         !bgav_bitstream_get(&b, &dummy, 1)) /* int marker_bit; */
        return 0;
      }
    
    }
  
  if(!bgav_bitstream_get(&b, &ret->video_object_layer_shape, 2))      // 2
    return 0;
  
  if((ret->video_object_layer_shape == 3) && // "grayscale"
     (ret->video_object_layer_verid != 1))
    {
    if(!bgav_bitstream_get(&b, &ret->video_object_layer_shape_extension, 2))      // 2
      return 0;
    }

  if(!bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
     !bgav_bitstream_get(&b, &ret->vop_time_increment_resolution, 16) ||
     !bgav_bitstream_get(&b, &dummy, 1) || /* int marker_bit; */
     !bgav_bitstream_get(&b, &ret->fixed_vop_rate, 1))
    return 0;

  if(ret->fixed_vop_rate)
    {
    int bits = bgav_log2(ret->vop_time_increment_resolution - 1) + 1;
    if(!bgav_bitstream_get(&b, &ret->fixed_vop_time_increment, bits))
      return 0;
    }
  
  return 1;
  }

void bgav_mpeg4_vol_header_dump(bgav_mpeg4_vol_header_t * h)
  {
  
  bgav_dprintf("random_accessible_vol:              %d\n", h->random_accessible_vol);
  bgav_dprintf("video_object_type_indication:       %d\n", h->video_object_type_indication);
  bgav_dprintf("is_object_layer_identifier:         %d\n", h->is_object_layer_identifier);
  if (h->is_object_layer_identifier) {
  bgav_dprintf("video_object_layer_verid:           %d\n", h->video_object_layer_verid);
  bgav_dprintf("video_object_layer_priority:        %d\n", h->video_object_layer_priority);
  }
  bgav_dprintf("aspect_ratio_info:                  %d\n", h->aspect_ratio_info);
  if(h->aspect_ratio_info == 15) {
  bgav_dprintf("par_width:                          %d\n", h->par_width);
  bgav_dprintf("par_height:                         %d\n", h->par_height);
  }
  bgav_dprintf("vol_control_parameters:             %d\n", h->vol_control_parameters);

  if (h->vol_control_parameters) {
  bgav_dprintf("chroma_format:                      %d\n", h->chroma_format);
  bgav_dprintf("low_delay:                          %d\n", h->low_delay);
  bgav_dprintf("vbv_parameters:                     %d\n", h->vbv_parameters);
  if (h->vbv_parameters) {
  bgav_dprintf("first_half_bit_rate:                %d\n", h->first_half_bit_rate);
  bgav_dprintf("latter_half_bit_rate:               %d\n", h->latter_half_bit_rate);
  bgav_dprintf("first_half_vbv_buffer_size:         %d\n", h->first_half_vbv_buffer_size);
  bgav_dprintf("latter_half_vbv_buffer_size:        %d\n", h->latter_half_vbv_buffer_size);
  bgav_dprintf("first_half_vbv_occupancy:           %d\n", h->first_half_vbv_occupancy);
  bgav_dprintf("latter_half_vbv_occupancy:          %d\n", h->latter_half_vbv_occupancy);
  }
  }
  bgav_dprintf("video_object_layer_shape:           %d\n", h->video_object_layer_shape);
  if ((h->video_object_layer_shape == 3) &&
      (h->video_object_layer_verid != 1)) {
  bgav_dprintf("video_object_layer_shape_extension: %d\n", h->video_object_layer_shape_extension);
  }
  bgav_dprintf("vop_time_increment_resolution:      %d\n", h->vop_time_increment_resolution);
  bgav_dprintf("fixed_vop_rate:                     %d\n", h->fixed_vop_rate);
  if (h->fixed_vop_rate) {
  bgav_dprintf("fixed_vop_time_increment:           %d\n", h->fixed_vop_time_increment);
  }
  
  }
                               