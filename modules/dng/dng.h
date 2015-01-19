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

#ifndef CONFIG_MAGICLANTERN
//compile for desktop
//subset of the ML lens_info struct definition (only fields needed by dng.c code)
struct lens_info
{
    char     name[32];
    uint32_t focal_len; // in mm
    uint32_t focus_dist; // in cm
    uint32_t aperture;
    uint32_t iso;
    uint32_t iso_auto;
    uint32_t hyperfocal; // in mm
    uint32_t dof_near; // in mm
    uint32_t dof_far; // in mm
    
    uint32_t wb_mode;
    uint32_t kelvin;
    uint32_t WBGain_R;
    uint32_t WBGain_G;
    uint32_t WBGain_B;
    int8_t   wbs_gm;
    int8_t   wbs_ba;
};
#endif

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

#ifdef CONFIG_MAGICLANTERN
struct dng_info * dng_get_info(struct raw_info * raw_info, int use_frame_shutter);
void dng_free(struct dng_info * dng_info);
#endif

int dng_save(char* filename, struct dng_info * dng_info);

#endif
