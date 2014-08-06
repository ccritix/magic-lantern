/*
 * Copyright (C) 2013 Magic Lantern Team
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

#ifndef __MLV_REC_H__
#define __MLV_REC_H__

/* interface to other modules:
 *
 *    uint32_t raw_rec_skip_frame(unsigned char *frame_data)
 *      This function is called on every single raw frame that is received from sensor with a pointer to frame data as parameter.
 *      If the return value is zero, the frame will get save into the saving buffers, else it is skipped
 *      Default: Do not skip frame (0)
 *
 *    uint32_t raw_rec_save_buffer(uint32_t used, uint32_t buffer_count)
 *      This function is called whenever the writing loop is checking if it has data to save to card.
 *      The parameters are the number of used buffers and the total buffer count
 *      Default: Save buffer (1)
 *
 *    uint32_t raw_rec_skip_buffer(uint32_t buffer_index, uint32_t buffer_count);
 *      Whenever the buffers are full, this function is called with the buffer index that is subject to being dropped, the number of frames in this buffer and the total buffer count.
 *      If it returns zero, this buffer will not get thrown away, but the next frame will get dropped.
 *      Default: Do not throw away buffer, but throw away incoming frame (0)
 */
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_starting();
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_started();
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_stopping();
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_stopped();
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_mlv_block(mlv_hdr_t *hdr);
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_skip_frame(unsigned char *frame_data);
extern WEAK_FUNC(ret_1) uint32_t raw_rec_cbr_save_buffer(uint32_t used, uint32_t buffer_index, uint32_t frame_count, uint32_t buffer_count);
extern WEAK_FUNC(ret_0) uint32_t raw_rec_cbr_skip_buffer(uint32_t buffer_index, uint32_t frame_count, uint32_t buffer_count);

void mlv_rec_queue_block(mlv_hdr_t *hdr);
void mlv_rec_set_rel_timestamp(mlv_hdr_t *hdr, uint64_t timestamp);
int32_t mlv_rec_get_free_slot();
void mlv_rec_get_slot_info(int32_t slot, uint32_t *size, void **address);
void mlv_rec_release_slot(int32_t slot, uint32_t write);

#endif
