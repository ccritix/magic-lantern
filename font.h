/** \file
 * Internal bitmap font representation
 *
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

#ifndef _font_h_
#define _font_h_

#include <stdint.h>



/** Raw in ROM representation of a Canon font.
 *
 * Stored in memory as;
 *	canon_font_t header
 *	uint32_t charmap[charmap_size/4]; (header + charmap_offset)
 *	uint32_t offsets[charmap_size/4]; (header + 2 * charmap_offset)
 *	canon_char_t chars[]; (header + 3 * charmap_offset)
 *
 * To find a character (slow way):
 * 	Search charmap for matching value to find index
 *	Retrieve offset = offsets[index]
 *	Retrieve char at chars + index;
 *
 * Fast way for ASCII: index == c - 0x20;
 *
 */

typedef struct
{
	uint32_t magic;
	uint16_t off_0x4; // 0xffe2 in most of them?
	uint16_t height; // in pixels, off_0x8;
	uint32_t charmap_offset; // off_0x8, typicaly 0x24
	uint32_t charmap_size; // off_0xc
	uint32_t bitmap_size; //
	const char name[16];
} __attribute__((packed))
canon_font_t;


#define CANON_FONT_MAGIC ((uint32_t) 0x544e46) // "FNT\0"

/** Raw in ROM representation of a Canon font bitmap */
typedef struct
{
        uint16_t width; // bitmap width in bits (round up to nearest byte)
        uint16_t height; // bitmap height in rows
        uint16_t display_width; // display width
        uint16_t xoff; // x offset
        uint16_t yoff; // y offset
        uint8_t bitmap[]; // ((w + 7) & ~7) * h bytes long
} __attribute__((packed))
canon_char_t;


/** Four fonts in the ROM1.bin file */
extern const canon_font_t font_gothic_24;
extern const canon_font_t font_gothic_30;
extern const canon_font_t font_gothic_36;
extern const canon_font_t font_mono_24;
extern const canon_font_t font_small;
extern const canon_font_t font_med;

extern void
canon_font_puts(
	const canon_font_t * font,
	unsigned * x,
	unsigned * y,
	const char * s
);

#endif
