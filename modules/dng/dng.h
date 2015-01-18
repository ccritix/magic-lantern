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

#ifndef _dng_h_
#define _dng_h_

struct dng_info
{
    char camera_name[32];
    char camera_serial[32];
    
    uint32_t shutter; /* microseconds */
    
    int32_t fps_numerator;
    int32_t fps_denominator;
    int32_t frame_number;
    
    struct raw_info * raw_info;
    struct lens_info * lens_info;
    struct tm * tm;
};

struct dng_info * dng_get_info(struct raw_info * raw_info, int use_frame_shutter);
void dng_free(struct dng_info * dng_info);
int dng_save(char* filename, struct dng_info * dng_info);

#endif
