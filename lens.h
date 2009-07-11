#ifndef _lens_h_
#define _lens_h_
/** \file
 * Lens and camera control
 *
 * These are Magic Lantern specific structures that control the camera's
 * shutter speed, ISO and lens aperture.  It also records the focal length,
 * distance and other parameters by hooking the different lens related
 * properties.
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
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

#include "property.h"

struct lens_info
{
	char 			name[ 32 ];
	int			focal_len;
	int			focus_dist;
	unsigned		aperture;
	unsigned		shutter;
	unsigned		iso;
	void *			token;
};

extern struct lens_info lens_info;


struct prop_lv_lens
{
	uint32_t		off_0x00;
	uint32_t		off_0x04;
	uint32_t		off_0x08;
	uint32_t		off_0x0c;
	uint32_t		off_0x10;
	uint32_t		off_0x14;
	uint32_t		off_0x18;
	uint32_t		off_0x1c;
	uint32_t		off_0x20;
	uint32_t		off_0x24;
	uint32_t		off_0x28;
	uint16_t		focal_len;	// off_0x2c;
	uint16_t		focus_dist;	// off_0x2e;
	uint32_t		off_0x30;
	uint32_t		off_0x34;
	uint16_t		off_0x38;
} __attribute__((packed));

SIZE_CHECK_STRUCT( prop_lv_lens, 58 );


/** Shutter values */
#define SHUTTER_30 96
#define SHUTTER_40 99
#define SHUTTER_50 101
#define SHUTTER_60 104
#define SHUTTER_80 107
#define SHUTTER_100 109
#define SHUTTER_125 112
#define SHUTTER_160 115
#define SHUTTER_200 117
#define SHUTTER_250 120
#define SHUTTER_320 123
#define SHUTTER_400 125
#define SHUTTER_500 128
#define SHUTTER_640 131
#define SHUTTER_800 133
#define SHUTTER_1000 136
#define SHUTTER_1250 139
#define SHUTTER_1600 141
#define SHUTTER_2000 144
#define SHUTTER_2500 147
#define SHUTTER_3200 149
#define SHUTTER_4000 152

/** Aperture values */
#define APERTURE_1_8 21
#define APERTURE_2_0 24
#define APERTURE_2_2 27
#define APERTURE_2_5 29
#define APERTURE_2_8 32
#define APERTURE_3_2 35
#define APERTURE_3_5 37
#define APERTURE_4_0 40
#define APERTURE_4_5 43
#define APERTURE_5_0 45
#define APERTURE_5_6 48
#define APERTURE_6_3 51
#define APERTURE_7_1 53
#define APERTURE_8_0 56
#define APERTURE_9_0 59
#define APERTURE_10 61
#define APERTURE_11 64
#define APERTURE_13 67
#define APERTURE_14 69
#define APERTURE_16 72
#define APERTURE_18 75
#define APERTURE_20 77
#define APERTURE_22 80
#define APERTURE_25 83
#define APERTURE_29 85
#define APERTURE_32 88

/** ISO values */
#define ISO_100 72
#define ISO_125 75
#define ISO_160 77
#define ISO_200 80
#define ISO_250 83
#define ISO_320 85
#define ISO_400 88
#define ISO_500 91
#define ISO_640 93
#define ISO_800 96
#define ISO_1000 99
#define ISO_1250 101
#define ISO_1600 104
#define ISO_2000 107
#define ISO_2500 109
#define ISO_3200 112
#define ISO_4000 115
#define ISO_5000 117
#define ISO_6400 120
#define ISO_12500 128


/** Camera control functions */
static inline void
lens_set_aperture(
	unsigned		aperture
)
{
	prop_request_change( PROP_APERTURE, &aperture, sizeof(aperture) );
}

static inline void
lens_set_iso(
	unsigned		iso
)
{
	prop_request_change( PROP_ISO, &iso, sizeof(iso) );
}

static inline void
lens_set_shutter(
	unsigned		shutter
)
{
	prop_request_change( PROP_SHUTTER, &shutter, sizeof(shutter) );
}

static inline void
lens_take_photo( void )
{
	unsigned value = 0;
	prop_request_change( PROP_SHUTTER_RELEASE, &value, sizeof(value) );
}

#endif
