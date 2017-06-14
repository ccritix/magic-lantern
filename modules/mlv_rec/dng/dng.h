/*
 * Copyright (C) 2014 David Milligan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _dng_h
#define _dng_h

#include <sys/types.h>
#include <raw.h>
#include "../mlv.h"

/* all mlv block headers needed to generate a DNG frame plus extra parameters */
struct frame_info
{
    char * mlv_filename;
    char * dng_filename;
    double fps_override;

    /* flags */
    int deflicker_target;
    int vertical_stripes;
    int bad_pixels;
    int save_bpm;
    int dual_iso;
    int chroma_smooth;
    int pattern_noise;
    int show_progress;
    int compressed_raw;
    int pack_dng_bits;

    /* block headers */
    mlv_vidf_hdr_t vidf_hdr;
    mlv_file_hdr_t file_hdr;
    mlv_rtci_hdr_t rtci_hdr;
    mlv_idnt_hdr_t idnt_hdr;
    mlv_rawi_hdr_t rawi_hdr;
    mlv_expo_hdr_t expo_hdr;
    mlv_lens_hdr_t lens_hdr;
    mlv_wbal_hdr_t wbal_hdr;
};

/* buffers of DNG header and image data */
struct dng_data
{
    size_t header_size;
    size_t image_size;
    size_t image_size_packed;
    size_t image_size_bak;

    uint8_t * header_buf;
    uint16_t * image_buf;
    uint16_t * image_buf_packed;
    uint16_t * image_buf_bak;
};

size_t dng_get_header_data(struct frame_info * frame_info, uint8_t * output_buffer, off_t offset, size_t max_size);
size_t dng_get_header_size();
size_t dng_get_image_data(struct frame_info * frame_info, uint16_t * packed_bits, uint8_t * output_buffer, off_t offset, size_t max_size);
size_t dng_get_image_size(struct frame_info * frame_info, int flag);
size_t dng_get_size(struct frame_info * frame_info, int flag);

void dng_init_data(struct frame_info * frame_info, struct dng_data * dng_data);
void dng_process_data(struct frame_info * frame_info, struct dng_data * dng_data);
int dng_save_data(struct frame_info * frame_info, struct dng_data * dng_data);
void dng_free_data(struct frame_info * frame_info, struct dng_data * dng_data);

#endif
