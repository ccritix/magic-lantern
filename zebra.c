/** \file
 * Zebra stripes, contrast edge detection and crop marks.
 *
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
 * Edge detection code by Robert Thiel <rthiel@gmail.com>
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

#include "dryos.h"
#include "bmp.h"
#include "version.h"
#include "config.h"
#include "menu.h"
#include "property.h"
#include "gui.h"
#include "lens.h"

void waveform_init();
void histo_init();
void do_disp_mode_change();
void show_overlay();
void transparent_overlay_from_play();

//~ static struct bmp_file_t * cropmarks_array[3] = {0};
static struct bmp_file_t * cropmarks = 0;

#define hist_height			64
#define hist_width			128
#define WAVEFORM_MAX_HEIGHT			240
#define WAVEFORM_MAX_WIDTH			360
#define WAVEFORM_HALFSIZE (waveform_draw == 1)
#define WAVEFORM_WIDTH (WAVEFORM_HALFSIZE ? WAVEFORM_MAX_WIDTH/2 : WAVEFORM_MAX_WIDTH)
#define WAVEFORM_HEIGHT (WAVEFORM_HALFSIZE ? WAVEFORM_MAX_HEIGHT/2 : WAVEFORM_MAX_HEIGHT)

#define BVRAM_MIRROR_SIZE (BMPPITCH*540)

CONFIG_INT("disp.mode", disp_mode, 0);
CONFIG_INT("disp.mode.aaa", disp_mode_a, 0x285041);
CONFIG_INT("disp.mode.bbb", disp_mode_b, 0x295001);
CONFIG_INT("disp.mode.ccc", disp_mode_c,  0x88090);
CONFIG_INT("disp.mode.xxx", disp_mode_x, 0x2c5051);

CONFIG_INT( "transparent.overlay", transparent_overlay, 0);
CONFIG_INT( "transparent.overlay.x", transparent_overlay_offx, 0);
CONFIG_INT( "transparent.overlay.y", transparent_overlay_offy, 0);
CONFIG_INT( "livev.playback", livev_playback, 0);
CONFIG_INT( "global.draw", global_draw, 1 );
CONFIG_INT( "zebra.draw",	zebra_draw,	2 );
CONFIG_INT( "zebra.level-hi",	zebra_level_hi,	245 );
CONFIG_INT( "zebra.level-lo",	zebra_level_lo,	10 );
CONFIG_INT( "zebra.nrec",	zebra_nrec,	0 );
CONFIG_INT( "crop.draw",	crop_draw,	1 ); // index of crop file
CONFIG_INT( "crop.movieonly", cropmark_movieonly, 1);
CONFIG_INT( "falsecolor.draw", falsecolor_draw, 0);
CONFIG_INT( "falsecolor.palette", falsecolor_palette, 0);
CONFIG_INT( "zoom.overlay.mode", zoom_overlay_mode, 2);
CONFIG_INT( "zoom.overlay.size", zoom_overlay_size, 4);
CONFIG_INT( "zoom.overlay.pos", zoom_overlay_pos, 1);
CONFIG_INT( "zoom.overlay.split", zoom_overlay_split, 0);
CONFIG_INT( "zoom.overlay.split.zerocross", zoom_overlay_split_zerocross, 1);
int get_zoom_overlay_mode() { return zoom_overlay_mode; }
int get_zoom_overlay_z() { return zoom_overlay_mode == 1 || zoom_overlay_mode == 2; }

int zoom_overlay = 0;
int zoom_overlay_countdown = 0;
int get_zoom_overlay() { return zoom_overlay; }

CONFIG_INT( "focus.peaking", focus_peaking, 0);
CONFIG_INT( "focus.peaking.thr", focus_peaking_pthr, 10); // 1%
CONFIG_INT( "focus.peaking.color", focus_peaking_color, 7); // R,G,B,C,M,Y,cc1,cc2

//~ CONFIG_INT( "focus.graph", focus_graph, 0);
//~ int get_crop_black_border() { return crop_black_border; }

//~ CONFIG_INT( "edge.draw",	edge_draw,	0 );
CONFIG_INT( "hist.draw",	hist_draw,	1 );
CONFIG_INT( "hist.x",		hist_x,		720 - hist_width - 4 );
CONFIG_INT( "hist.y",		hist_y,		100 );
CONFIG_INT( "waveform.draw",	waveform_draw,	0 );
//~ CONFIG_INT( "waveform.x",	waveform_x,	720 - WAVEFORM_WIDTH );
//~ CONFIG_INT( "waveform.y",	waveform_y,	480 - 50 - WAVEFORM_WIDTH );
CONFIG_INT( "waveform.bg",	waveform_bg,	0x26 ); // solid black

CONFIG_INT( "clear.preview", clearscreen, 1); // 2 is always
CONFIG_INT( "clear.preview.delay", clearscreen_delay, 1000); // ms

CONFIG_INT( "spotmeter.size",		spotmeter_size,	5 );
CONFIG_INT( "spotmeter.draw",		spotmeter_draw, 1 );
CONFIG_INT( "spotmeter.formula",		spotmeter_formula, 0 ); // 0 percent, 1 IRE AJ, 2 IRE Piers

CONFIG_INT( "unified.loop", unified_loop, 2); // temporary; on/off/auto
CONFIG_INT( "zebra.density", zebra_density, 0); 
CONFIG_INT( "hd.vram", use_hd_vram, 0); 


int crop_dirty = 0;
int clearscreen_countdown = 20;

void ChangeColorPaletteLV(int x)
{
	if (!lv_drawn()) return;
	if (!bmp_is_on()) return;
	ChangeColorPalette(x);
}

static void
unified_loop_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"UnifLoop (experim): %s",
		unified_loop == 0 ? "OFF" : unified_loop == 1 ? "ON" : "Auto"
	);
	menu_draw_icon(x, y, MNI_BOOL_AUTO(unified_loop), 0);
}

static void
zebra_mode_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Zebra Density: %d", zebra_density);
}

static void
use_hd_vram_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Use HD VRAM: %s", use_hd_vram?"yes":"no");
}

int recording = 0;

uint8_t false_colour[5][256] = {
	{0x0E, 0x0E, 0x0E, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x72, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6E, 0x6F},
	{0x0E, 0x0E, 0x0E, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x26, 0x26, 0x27, 0x27, 0x27, 0x27, 0x27, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x29, 0x29, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x39, 0x39, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43, 0x43, 0x43, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x46, 0x46, 0x46, 0x47, 0x47, 0x47, 0x47, 0x47, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x49, 0x49, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4A, 0x4B, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4C, 0x4D, 0x4D, 0x4D, 0x4D, 0x4D, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0x4E, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0x6F},
	{0x0E, 0x0E, 0x0E, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13, 0x13,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0x6F},
	{0x0E, 0x0E, 0x0E, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52, 0x52,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0x6F},
	{0x0E, 0x4F, 0x4E, 0x4D, 0x4C, 0x4B, 0x4A, 0x49, 0x48, 0x48, 0x47, 0x47, 0x46, 0x46, 0x45, 0x45, 0x45, 0x44, 0x44, 0x44, 0x44, 0x43, 0x43, 0x43, 0x42, 0x42, 0x42, 0x42, 0x41, 0x41, 0x41, 0x41, 0x41, 0x40, 0x40, 0x40, 0x40, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3B, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x34, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x32, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2B, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x27, 0x26, 0x26, 0x26, 0x26, 0x26, 0x6F},
};

void crop_set_dirty(int value)
{
	crop_dirty = MAX(crop_dirty, value);
}

int ext_monitor_rca = 0;
int ext_monitor_hdmi = 0;
#define EXT_MONITOR_CONNECTED (ext_monitor_hdmi | ext_monitor_rca)

PROP_HANDLER(PROP_USBRCA_MONITOR)
{
	ext_monitor_rca = buf[0];
	crop_set_dirty(40);
	return prop_cleanup( token, property );
}
PROP_HANDLER(PROP_HDMI_CHANGE)
{
	ext_monitor_hdmi = buf[0];
	crop_set_dirty(40);
	return prop_cleanup( token, property );
}

int video_mode_crop = 0;
int video_mode_fps = 0;
int video_mode_resolution = 0; // 0 if full hd, 1 if 720p, 2 if 480p
PROP_HANDLER(PROP_VIDEO_MODE)
{
	video_mode_crop = buf[0];
	video_mode_fps = buf[2];
	video_mode_resolution = buf[1];
	return prop_cleanup( token, property );
}

/*int gui_state;
PROP_HANDLER(PROP_GUI_STATE) {
	gui_state = buf[0];
	if (gui_state == GUISTATE_IDLE) crop_set_dirty(40);
	return prop_cleanup( token, property );
}*/

PROP_HANDLER( PROP_LV_AFFRAME ) {
	afframe_set_dirty();
	return prop_cleanup( token, property );
}


// how to use a config setting in more than one file?!
//extern int* p_cfg_draw_meters;

int get_global_draw()
{
	return global_draw;
}
void set_global_draw(int g)
{
	global_draw = g;
}

struct vram_info * get_yuv422_hd_vram()
{
	static struct vram_info _vram_info;
	_vram_info.vram = (uint8_t*)YUV422_HD_BUFFER_DMA_ADDR;
	_vram_info.width = recording ? (video_mode_resolution == 0 ? 1720 : 
									video_mode_resolution == 1 ? 1280 : 
									video_mode_resolution == 2 ? 640 : 0)
								  : lv_dispsize > 1 ? 1024
								  : shooting_mode != SHOOTMODE_MOVIE ? 1056
								  : (video_mode_resolution == 0 ? 1056 : 
								  	video_mode_resolution == 1 ? 1024 :
									 video_mode_resolution == 2 ? (video_mode_crop? 640:1024) : 0);
	_vram_info.pitch = _vram_info.width << 1; 
	_vram_info.height = recording ? (video_mode_resolution == 0 ? 974 : 
									video_mode_resolution == 1 ? 580 : 
									video_mode_resolution == 2 ? 480 : 0)
								  : lv_dispsize > 1 ? 680
								  : shooting_mode != SHOOTMODE_MOVIE ? 704
								  : (video_mode_resolution == 0 ? 704 : 
								  	video_mode_resolution == 1 ? 680 :
									 video_mode_resolution == 2 ? (video_mode_crop? 480:680) : 0);

	return &_vram_info;
}


void* get_fastrefresh_422_buf()
{
	switch (YUV422_LV_BUFFER_DMA_ADDR)
	{
		case 0x40d07800:
			return (void*) 0x4c233800;
		case 0x4c233800:
			return (void*) 0x4f11d800;
		case 0x4f11d800:
			return (void*) 0x40d07800;
	}
	return 0;
}

void* get_write_422_buf()
{
	switch (YUV422_LV_BUFFER_DMA_ADDR)
	{
		case 0x40d07800:
			return (void*) 0x40d07800;
		case 0x4c233800:
			return (void*) 0x4c233800;
		case 0x4f11d800:
			return (void*) 0x4f11d800;
	}
	return 0;
}

int vram_width = 720;
int vram_height = 480;
PROP_HANDLER(PROP_VRAM_SIZE_MAYBE)
{
	vram_width = buf[1];
	vram_height = buf[2];
	return prop_cleanup(token, property);
}

struct vram_info * get_yuv422_vram()
{
	static struct vram_info _vram_info;
	_vram_info.vram = get_fastrefresh_422_buf();
	if (gui_state == GUISTATE_PLAYMENU) _vram_info.vram = (void*) YUV422_LV_BUFFER_DMA_ADDR;

	_vram_info.width = vram_width;
	_vram_info.height = vram_height;
	_vram_info.pitch = _vram_info.width * 2;

	//~ bmp_printf(FONT_LARGE, 100, 100, "%d x %d", _vram_info.width, _vram_info.height);

	return &_vram_info;
}

/** Sobel edge detection */
/*
static int32_t
edge_detect(
	uint32_t *		buf,
	uint32_t		pitch
)
{	
	const uint32_t		pixel1	= buf[0];
	const int32_t		p00	= (pixel1 & 0xFFFF);
	const int32_t		p01	= pixel1 >> 16;
	const uint32_t		pixel2	= buf[1];
	const int32_t		p02	= (pixel2 & 0xFFFF);
	const uint32_t		pixel3	= buf[pitch];
	const int32_t		p10	= (pixel3 & 0xFFFF);
	const int32_t		p11	= pixel3 >> 16;
	const uint32_t		pixel4	= buf[pitch+1];
	const int32_t		p12	= (pixel4 & 0xFFFF);
	
	int32_t sx1 = p00 - p11;
	int32_t sy1 = p01 - p10;
	
	int32_t sx2 = p01 - p12;
	int32_t sy2 = p02 - p11;

	// abs value
	sx1 = ( sx1 ^ (sx1 >> 15) ) - (sx1 >> 15);
	sy1 = ( sy1 ^ (sy1 >> 15) ) - (sy1 >> 15);
	
	sx2 = ( sx2 ^ (sx2 >> 15) ) - (sx2 >> 15);
	sy2 = ( sy2 ^ (sy2 >> 15) ) - (sy2 >> 15);
	
	return (((sx2 + sy2) >> 1 ) & 0xFF00 ) | ((sx1 + sy1) >> 9);
}


static unsigned
check_edge(
	unsigned		x,
	unsigned		y,
	uint16_t *		b_row,
	uint32_t *		v_row,
	unsigned		vram_pitch
)
{
	const unsigned dx = x/2;
	// Check for contrast
	uint32_t grad = edge_detect(
		&v_row[dx],
		vram_pitch
	);
	
	// Check for any high gradients in either pixel
	if( (grad & 0xF8F8) == 0 )
		return 0;

	// Color coding (using the blue colors starting at 0x70)
	b_row[dx] = 0x7070 | ((grad & 0xF8F8) >> 3) ;			
	return 1;
	
}


static unsigned
check_zebra(
	unsigned		x,
	unsigned		y,
	uint16_t *		b_row,
	uint32_t *		v_row,
	unsigned		vram_pitch, // unused?
	uint16_t * 		m_row
)
{
	// Determine if we are a zig or a zag line
	if (((y >> 3) ^ (x >> 3)) & 1) return 0;

	uint32_t pixel = v_row[x/2];
	uint32_t p0 = ((pixel >> 16) & 0xFF00) >> 8; // odd bytes are luma
	uint32_t p1 = ((pixel >>  0) & 0xFF00) >> 8;

	// If neither pixel is overexposed or underexposed, ignore it
	if( p0 <= zebra_level_hi && p1 <= zebra_level_hi && p0 >= zebra_level_lo && p1 >= zebra_level_lo)
		return 0;

 	uint8_t zebra_color_1 = 12; // red
   if (p0 < zebra_level_lo || p1 < zebra_level_lo)
        zebra_color_1 = 13; // blue 

	b_row[x/2] = (zebra_color_1<<8) | (zebra_color_1<<0);
	m_row[x/2] = (zebra_color_1<<8) | (zebra_color_1<<0);
	return 1;
}


static unsigned
check_crop(
	unsigned		x,
	unsigned		y,
	uint16_t *		b_row,
	uint32_t *		v_row,
	unsigned		vram_pitch,
	uint16_t * m_row
)
{
	if( !cropmarks )
		return 0;

	uint8_t * pixbuf = &cropmarks->image[
		x + cropmarks->width * (cropmarks->height - y)
	];
	uint16_t pix = *(uint16_t*) pixbuf;
	if( pix == 0 )
		return 0;

	b_row[ x/2 ] = pix;
	m_row[ x/2 ] = pix;
	return 1;
}
*/

/** Store the waveform data for each of the WAVEFORM_WIDTH bins with
 * 128 levels
 */
static uint32_t** waveform = 0;

/** Store the histogram data for each of the "hist_width" bins */
static uint32_t* hist = 0;
static uint32_t* hist_r = 0;
static uint32_t* hist_g = 0;
static uint32_t* hist_b = 0;

/** Maximum value in the histogram so that at least one entry fills
 * the box */
static uint32_t hist_max;


/** Generate the histogram data from the YUV frame buffer.
 *
 * Walk the frame buffer two pixels at a time, in 32-bit chunks,
 * to avoid err70 while recording.
 *
 * Average two adjacent pixels to try to reduce noise slightly.
 *
 * Update the hist_max for the largest number of bin entries found
 * to scale the histogram to fit the display box from top to
 * bottom.
 */
void
hist_build(void* vram, int width, int pitch)
{
	uint32_t * 	v_row = (uint32_t*) vram;
	int x,y;

	histo_init();
	if (!hist) return;
	if (!hist_r) return;
	if (!hist_g) return;
	if (!hist_b) return;

	hist_max = 0;
	for( x=0 ; x<hist_width ; x++ )
	{
		hist[x] = 0;
		hist_r[x] = 0;
		hist_g[x] = 0;
		hist_b[x] = 0;
	}

	if (waveform_draw)
	{
		waveform_init();
		for( y=0 ; y<WAVEFORM_WIDTH ; y++ )
			for( x=0 ; x<WAVEFORM_HEIGHT ; x++ )
				waveform[y][x] = 0;
	}

	for( y=1 ; y<480; y++, v_row += (pitch/4) )
	{
		for( x=0 ; x<width ; x += 2 )
		{
			// Average each of the two pixels
			uint32_t pixel = v_row[x/2];
			uint32_t p1 = (pixel >> 16) & 0xFF00;
			uint32_t p2 = (pixel >>  0) & 0xFF00;
			int Y = ((p1+p2) / 2) >> 8;

			if (hist_draw == 2)
			{
				int8_t U = (pixel >>  0) & 0xFF;
				int8_t V = (pixel >> 16) & 0xFF;
				int R = Y + 1437 * V / 1024;
				int G = Y -  352 * U / 1024 - 731 * V / 1024;
				int B = Y + 1812 * U / 1024;
				uint32_t R_level = COERCE(( R * hist_width ) / 256, 0, hist_width-1);
				uint32_t G_level = COERCE(( G * hist_width ) / 256, 0, hist_width-1);
				uint32_t B_level = COERCE(( B * hist_width ) / 256, 0, hist_width-1);
				hist_r[R_level]++;
				hist_g[G_level]++;
				hist_b[B_level]++;
			}

			uint32_t hist_level = COERCE(( Y * hist_width ) / 0xFF, 0, hist_width-1);

			// Ignore the 0 bin.  It generates too much noise
			unsigned count = ++ (hist[ hist_level ]);
			if( hist_level && count > hist_max )
				hist_max = count;
			
			// Update the waveform plot
			if (waveform_draw) waveform[ COERCE((x * WAVEFORM_WIDTH) / width, 0, WAVEFORM_WIDTH-1)][ COERCE((Y * WAVEFORM_HEIGHT) / 0xFF, 0, WAVEFORM_HEIGHT-1) ]++;
		}
	}
}

void get_under_and_over_exposure(uint32_t thr_lo, uint32_t thr_hi, int* under, int* over)
{
	*under = -1;
	*over = -1;
	struct vram_info * vramstruct = get_yuv422_vram();
	if (!vramstruct) return;

	*under = 0;
	*over = 0;
	void* vram = vramstruct->vram;
	int width = vramstruct->width;
	int pitch = vramstruct->pitch;
	uint32_t * 	v_row = (uint32_t*) vram;
	int x,y;
	for( y=1 ; y<480; y++, v_row += (pitch/4) )
	{
		for( x=0 ; x<width ; x += 2 )
		{
			uint32_t pixel = v_row[x/2];
			uint32_t p1 = (pixel >> 16) & 0xFFFF;
			uint32_t p2 = (pixel >>  0) & 0xFFFF;
			uint32_t p = ((p1+p2) / 2) >> 8;
			if (p < thr_lo) (*under)++;
			if (p > thr_hi) (*over)++;
		}
	}
}

static int hist_rgb_color(int y, int sizeR, int sizeG, int sizeB)
{
	switch ((y > sizeR ? 0 : 1) |
			(y > sizeG ? 0 : 2) |
			(y > sizeB ? 0 : 4))
	{
		case 0b000: return COLOR_BLACK;
		case 0b001: return COLOR_RED;
		case 0b010: return 7; // green
		case 0b100: return 9; // strident blue
		case 0b011: return COLOR_YELLOW;
		case 0b110: return 5; // cyan
		case 0b101: return 14; // magenta
		case 0b111: return COLOR_WHITE;
	}
	return 0;
}

#define ZEBRA_COLOR_WORD_SOLID(x) ( (x) | (x)<<8 | (x)<<16 | (x)<<24 )
static int zebra_rgb_color(int underexposed, int clipR, int clipG, int clipB, int y)
{
	if (underexposed) return zebra_color_word_row(79, y);
	
	switch ((clipR ? 0 : 1) |
			(clipG ? 0 : 2) |
			(clipB ? 0 : 4))
	{
		case 0b000: return zebra_color_word_row(COLOR_BLACK, y);
		case 0b001: return zebra_color_word_row(COLOR_RED,1);
		case 0b010: return zebra_color_word_row(7, 1); // green
		case 0b100: return zebra_color_word_row(9, 1); // strident blue
		case 0b011: return y&2 ? 0 : ZEBRA_COLOR_WORD_SOLID(COLOR_YELLOW);
		case 0b110: return y&2 ? 0 : ZEBRA_COLOR_WORD_SOLID(5); // cyan
		case 0b101: return y&2 ? 0 : ZEBRA_COLOR_WORD_SOLID(14); // magenta
		case 0b111: return 0;
	}
	return 0;
}


/** Draw the histogram image into the bitmap framebuffer.
 *
 * Draw one pixel at a time; it seems to be ok with err70.
 * Since there is plenty of math per pixel this doesn't
 * swamp the bitmap framebuffer hardware.
 */
static void
hist_draw_image(
	unsigned		x_origin,
	unsigned		y_origin
)
{
	if (!PLAY_MODE)
	{
		if (!expsim) return;
	}
	uint8_t * const bvram = bmp_vram();

	// Align the x origin, just in case
	x_origin &= ~3;

	uint8_t * row = bvram + x_origin + y_origin * BMPPITCH;
	if( hist_max == 0 )
		hist_max = 1;

	unsigned i, y;

	for( i=0 ; i < hist_width ; i++ )
	{
		// Scale by the maximum bin value
		const uint32_t size = (hist[i] * hist_height) / hist_max;
		const uint32_t sizeR = (hist_r[i] * hist_height) / hist_max;
		const uint32_t sizeG = (hist_g[i] * hist_height) / hist_max;
		const uint32_t sizeB = (hist_b[i] * hist_height) / hist_max;

		uint8_t * col = row + i;
		// vertical line up to the hist size
		for( y=hist_height ; y>0 ; y-- , col += BMPPITCH )
		{
			if (hist_draw == 2) // RGB
				*col = hist_rgb_color(y, sizeR, sizeG, sizeB);
			else
				*col = y > size ? COLOR_BG : (falsecolor_draw ? false_colour[falsecolor_palette][(i * 256 / hist_width) & 0xFF]: COLOR_WHITE);
		}
	}

	hist_max = 0;
}


/** Draw the waveform image into the bitmap framebuffer.
 *
 * Draw one pixel at a time; it seems to be ok with err70.
 * Since there is plenty of math per pixel this doesn't
 * swamp the bitmap framebuffer hardware.
 */

static void
waveform_draw_image(
	unsigned		x_origin,
	unsigned		y_origin
)
{
	if (!PLAY_MODE)
	{
		if (!expsim) return;
	}
	waveform_init();
	// Ensure that x_origin is quad-word aligned
	x_origin &= ~3;

	uint8_t * const bvram = bmp_vram();
	unsigned pitch = BMPPITCH;
	uint8_t * row = bvram + x_origin + y_origin * pitch;
	if( hist_max == 0 )
		hist_max = 1;

	unsigned i, y;

	// vertical line up to the hist size
	for( y=WAVEFORM_HEIGHT-1 ; y>0 ; y-- )
	{
		uint32_t pixel = 0;

		for( i=0 ; i<WAVEFORM_WIDTH ; i++ )
		{

			uint32_t count = waveform[ i ][ y ];
			// Scale to a grayscale
			count = (count * 42) / 128;
			if( count > 42 )
				count = 0x0F;
			else
			if( count >  0 )
				count += 0x26;
			else
			// Draw a series of colored scales
			if( y == (WAVEFORM_HEIGHT*1)/4 )
				count = COLOR_BLUE;
			else
			if( y == (WAVEFORM_HEIGHT*2)/4 )
				count = 0xE; // pink
			else
			if( y == (WAVEFORM_HEIGHT*3)/4 )
				count = COLOR_BLUE;
			else
				count = waveform_bg; // transparent

			pixel |= (count << ((i & 3)<<3));

			if( (i & 3) != 3 )
				continue;

			// Draw the pixel, rounding down to the nearest
			// quad word write (and then nop to avoid err70).
			*(uint32_t*)( row + (i & ~3)  ) = pixel;
			pixel = 0;
			asm( "nop" );
			asm( "nop" );
			asm( "nop" );
			asm( "nop" );
		}

		row += pitch;
	}
}


/** Master video overlay drawing code.
 *
 * This routine controls the display of the zebras, histogram,
 * edge detection, cropmarks and so on.
 *
 * The stacking order of the overlays is:
 *
 * - Histogram
 * - Cropping bitmap
 * - Zebras
 * - Edge detection
 *
 * This should be done with a proper OO controller that allows modules
 * to register new drawing functions, but for right now they are hardcoded.
 */

static FILE * g_aj_logfile = INVALID_PTR;
unsigned int aj_create_log_file( char * name)
{
   FIO_RemoveFile( name );
   g_aj_logfile = FIO_CreateFile( name );
   if ( g_aj_logfile == INVALID_PTR )
   {
      bmp_printf( FONT_SMALL, 120, 40, "FCreate: Err %s", name );
      return( 0 );  // FAILURE
   }
   return( 1 );  // SUCCESS
}

void aj_close_log_file( void )
{
   if (g_aj_logfile == INVALID_PTR)
      return;
   FIO_CloseFile( g_aj_logfile );
   g_aj_logfile = INVALID_PTR;
}

void dump_seg(uint32_t start, uint32_t size, char* filename)
{
    DEBUG();
    aj_create_log_file(filename);
    FIO_WriteFile( g_aj_logfile, (const void *) start, size );
    aj_close_log_file();
    DEBUG();
}

void dump_big_seg(int k, char* filename)
{
    DEBUG();
    aj_create_log_file(filename);
    
    int i;
    for (i = 0; i < 16; i++)
    {
		DEBUG();
		uint32_t start = (k << 28 | i << 24);
		bmp_printf(FONT_LARGE, 50, 50, "DUMP %x %8x ", i, start);
		FIO_WriteFile( g_aj_logfile, (const void *) start, 0x1000000 );
	}
    
    aj_close_log_file();
    DEBUG();
}

int tic()
{
	struct tm now;
	LoadCalendarFromRTC(&now);
	return now.tm_sec + now.tm_min * 60 + now.tm_hour * 3600 + now.tm_mday * 3600 * 24;
}
/*
void card_benchmark_wr(int bufsize, int K, int N)
{
	FIO_RemoveFile("B:/bench.tmp");
	msleep(1000);
	int n = 0x10000000 / bufsize;
	{
		FILE* f = FIO_CreateFile("B:/bench.tmp");
		int t0 = tic();
		int i;
		for (i = 0; i < n; i++)
		{
			uint32_t start = UNCACHEABLE(i * bufsize);
			bmp_printf(FONT_LARGE, 0, 0, "[%d/%d] Writing: %d/100 (buf=%dK)... ", K, N, i * 100 / n, bufsize/1024);
			FIO_WriteFile( f, (const void *) start, bufsize );
		}
		FIO_CloseFile(f);
		int t1 = tic();
		int speed = 2560 / (t1 - t0);
		console_printf("Write speed (buffer=%dk):\t %d.%d MB/s\n", bufsize/1024, speed/10, speed % 10);
	}
	SW1(1,100);
	SW1(0,100);
	msleep(1000);
	if (bufsize > 1024*1024) console_printf("read test skipped: buffer=%d\n", bufsize);
	else
	{
		void* buf = AllocateMemory(bufsize);
		if (buf)
		{
			FILE* f = FIO_Open("B:/bench.tmp", O_RDONLY | O_SYNC);
			int t0 = tic();
			int i;
			for (i = 0; i < n; i++)
			{
				bmp_printf(FONT_LARGE, 0, 0, "[%d/%d] Reading: %d/100 (buf=%dK)... ", K, N, i * 100 / n, bufsize/1024);
				FIO_ReadFile(f, UNCACHEABLE(buf), bufsize );
			}
			FIO_CloseFile(f);
			FreeMemory(buf);
			int t1 = tic();
			int speed = 2560 / (t1 - t0);
			console_printf("Read speed (buffer=%dk):\t %d.%d MB/s\n", bufsize/1024, speed/10, speed % 10);
		}
		else
		{
			console_printf("malloc error: buffer=%d\n", bufsize);
		}
	}

	FIO_RemoveFile("B:/bench.tmp");
	msleep(1000);
	SW1(1,100);
	SW1(0,100);
}

void card_benchmark()
{
	console_printf("Card benchmark starting...\n");
	card_benchmark_wr(16384, 1, 3);
	card_benchmark_wr(131072, 2, 3);
	card_benchmark_wr(16777216, 3, 3);
	console_printf("Card benchmark done.\n");
	console_show();
}

int card_benchmark_start = 0;
void card_benchmark_schedule()
{
	gui_stop_menu();
	card_benchmark_start = 1;
}*/

static void dump_vram()
{
	dump_big_seg(0, "B:/0.bin");
	//dump_big_seg(4, "B:/4.bin");
	//~ dump_seg(0x44000080, 1920*1080*2, "B:/hd.bin");
	//~ dump_seg(YUV422_IMAGE_BUFFER, 1920*1080*2, "B:/VRAM.BIN");
}

static uint8_t* bvram_mirror = 0;

void spotmeter_step();

int fps_ticks = 0;

void fail(char* msg)
{
	bmp_printf(FONT_LARGE, 30, 100, msg);
	while(1) msleep(1);
}
void waveform_init()
{
	if (!waveform)
	{
		waveform = AllocateMemory(WAVEFORM_MAX_WIDTH * sizeof(uint32_t*));
		if (!waveform) fail("Waveform malloc failed");
		int i;
		for (i = 0; i < WAVEFORM_MAX_WIDTH; i++) {
			waveform[i] = AllocateMemory(WAVEFORM_MAX_HEIGHT * sizeof(uint32_t));
			if (!waveform[i]) fail("Waveform malloc failed");
		}
	}
}

void histo_init()
{
	if (!hist) hist = AllocateMemory(hist_width * sizeof(uint32_t*));
	if (!hist) fail("Hist malloc failed");

	if (!hist_r) hist_r = AllocateMemory(hist_width * sizeof(uint32_t*));
	if (!hist_r) fail("HistR malloc failed");

	if (!hist_g) hist_g = AllocateMemory(hist_width * sizeof(uint32_t*));
	if (!hist_g) fail("HistG malloc failed");

	if (!hist_b) hist_b = AllocateMemory(hist_width * sizeof(uint32_t*));
	if (!hist_b) fail("HistB malloc failed");
}

static void bvram_mirror_init()
{
	if (!bvram_mirror)
	{
		bvram_mirror = AllocateMemory(BVRAM_MIRROR_SIZE);
		if (!bvram_mirror) 
		{	
			bmp_printf(FONT_MED, 30, 30, "Failed to allocate BVRAM mirror");
			return;
		}
		bzero32(bvram_mirror, BVRAM_MIRROR_SIZE);
	}
}

int get_focus_color(int thr, int d)
{
	return
		focus_peaking_color == 0 ? COLOR_RED :
		focus_peaking_color == 1 ? 7 :
		focus_peaking_color == 2 ? COLOR_BLUE :
		focus_peaking_color == 3 ? 5 :
		focus_peaking_color == 4 ? 14 :
		focus_peaking_color == 5 ? 15 :
		focus_peaking_color == 6 ? 	(thr > 50 ? COLOR_RED :
									 thr > 40 ? 19 /*orange*/ :
									 thr > 30 ? 15 /*yellow*/ :
									 thr > 20 ? 5 /*cyan*/ : 
									 9 /*light blue*/) :
		focus_peaking_color == 7 ? ( d > 50 ? COLOR_RED :
									 d > 40 ? 19 /*orange*/ :
									 d > 30 ? 15 /*yellow*/ :
									 d > 20 ? 5 /*cyan*/ : 
									 9 /*light blue*/) : 1;
}

static void little_cleanup(void* BP, void* MP)
{
	uint8_t* bp = BP; uint8_t* mp = MP;
	if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
	bp++; mp++;
	if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
	bp++; mp++;
	if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
	bp++; mp++;
	if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
}


int zebra_color_word_row(int c, int y)
{
	if (!c) return 0;
	
	uint32_t cw = 0;
	switch(y % 4)
	{
		case 0:
			cw  = c  | c  << 8;
			break;
		case 1:
			cw  = c << 8 | c << 16;
			break;
		case 2:
			cw = c  << 16 | c << 24;
			break;
		case 3:
			cw  = c  << 24 | c ;
			break;
	}
	return cw;
}

int zebra_color_word_row_thick(int c, int y)
{
	//~ return zebra_color_word_row(c,y);
	if (!c) return 0;
	
	uint32_t cw = 0;
	switch(y % 4)
	{
		case 0:
			cw  = c  | c  << 8 | c << 16;
			break;
		case 1:
			cw  = c << 8 | c << 16 | c << 24;
			break;
		case 2:
			cw = c  << 16 | c << 24 | c;
			break;
		case 3:
			cw  = c  << 24 | c | c << 8;
			break;
	}
	return cw;
}

// thresholded edge detection
static void draw_zebra_and_focus_unified( void )
{
	if (!global_draw) return;
	
	fps_ticks++;
	
	bvram_mirror_init();

	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	if (!bvram_mirror) return;
	if (lv_dispsize != 1) return; // zoom not handled, better ignore it

	int x,y;
	int zd = zebra_draw && (expsim || PLAY_MODE) && (!zebra_nrec || !recording); // when to draw zebras
	if (focus_peaking || zd) {
  		// clear previously written pixels
  		#define MAX_DIRTY_PIXELS 5000
  		static int* dirty_pixels = 0;
  		if (!dirty_pixels) dirty_pixels = AllocateMemory(MAX_DIRTY_PIXELS * sizeof(int));
  		if (!dirty_pixels) return;
  		static int dirty_pixels_num = 0;
  		//~ static int very_dirty = 0;
  		bmp_ov_loc_size_t os;
  		calc_ov_loc_size(&os);
  		struct vram_info * _vram;
  		
  		if(use_hd_vram) { 
  			_vram=get_yuv422_hd_vram();
		} else {
			_vram=get_yuv422_vram();
		}
		if (gui_state == GUISTATE_PLAYMENU) _vram->vram = (void*) YUV422_LV_BUFFER_DMA_ADDR;

  		uint8_t * const vram = /*UNCACHEABLE*/(_vram->vram);
		int vr_width  = _vram->width<<1;
  		int vr_height = _vram->height;
		int vr_pitch =  _vram->pitch;

		int bm_lv_y = 0;

		if(shooting_mode == SHOOTMODE_MOVIE) {
			bm_lv_y = (os.bmp_ex_y-os.bmp_ex_x*9/16);
			if(((ext_monitor_hdmi || ext_monitor_rca) && !recording ) || (!ext_monitor_hdmi && !ext_monitor_rca)){
				bm_lv_y>>=1;
			}
		}

		os.bmp_ex_x>>=2; //reduce x size to quad pixels 
		os.bmp_of_x>>=2; //reduce offset to quad pixels
		
		int vr_x_of_corr = 0;
		int vr_x_ex_corr = 0;
		int vr_y_of_corr = 0;
		int vr_y_ex_corr = 0;
		
		if(use_hd_vram) {
			vr_y_of_corr = recording ? bm_lv_y:0;
			vr_y_ex_corr = recording ? bm_lv_y+os.bmp_of_y:0;
			if(!ext_monitor_hdmi && !ext_monitor_rca) {
				vr_y_ex_corr<<=1;
			}
		} else {
			vr_x_of_corr = os.bmp_of_x; // number of double pixels we go left
			vr_x_ex_corr = os.bmp_of_x<<((ext_monitor_hdmi || ext_monitor_rca)&&recording?2:3);
			
			vr_y_of_corr=-os.bmp_of_y;
			vr_height=os.lv_ex_y;
			if(ext_monitor_hdmi && video_mode_resolution) {
				vr_height>>=1;
				if(video_mode_crop) { // FIXME crop mode with external displays
					vr_x_of_corr<<=2;
					vr_height>>=1;
					vr_width>>=2;
				}
			}
		}
		
//		bmp_printf(FONT_MED, 30, 100, "HD %dx%dp:%d vxc:%d vxo:%d vyc:%d vyo:%d byo:%d blvy:%d", vr_width>>1, vr_height, vr_pitch, vr_x_ex_corr, vr_x_of_corr, vr_y_ex_corr, vr_y_of_corr, os.bmp_of_y,  bm_lv_y);

  		int ymin = os.bmp_of_y + bm_lv_y;
  		int ymax = os.bmp_ex_y + os.bmp_of_y - bm_lv_y;
  		int xmin = os.bmp_of_x;
  		int xmax = os.bmp_ex_x + os.bmp_of_x;

 		ymin=COERCE(ymin, 0, 540);
  		ymax=COERCE(ymax, 0, 540);
  		xmin=COERCE(xmin, 0, 960);
  		xmax=COERCE(xmax, 0, 960);

  		static int16_t* xcalc = 0;
  		if (!xcalc) xcalc = AllocateMemory(960 * 2);
  		if (!xcalc) return;
  		static int xcalc_done=0;
  		
  		if(!xcalc_done || crop_dirty) {
	  		for (x = xmin; x < xmax; x++) {
  				xcalc[x]=(x-os.bmp_of_x+vr_x_of_corr)*((vr_width>>2)-vr_x_ex_corr)/os.bmp_ex_x;
			}
			xcalc_done=1;
		}

		uint32_t zlh = zebra_level_hi << 8;
		uint32_t zll = zebra_level_lo << 8;
		if(focus_peaking) {
			int i;
			for (i = 0; i < dirty_pixels_num; i++) {
				dirty_pixels[i] = COERCE(dirty_pixels[i], 0, 960*540);
				#define B1 *(uint16_t*)(bvram + dirty_pixels[i])
				#define B2 *(uint16_t*)(bvram + dirty_pixels[i] + BMPPITCH)
				#define M1 *(uint16_t*)(bvram_mirror + dirty_pixels[i])
				#define M2 *(uint16_t*)(bvram_mirror + dirty_pixels[i] + BMPPITCH)
				if ((B1 == 0 || B1 == M1) && (B2 == 0 || B2 == M2))
					B1 = B2 = M1 = M2 = 0;
				#undef B1
				#undef B2
				#undef M1
				#undef M2
			}
			dirty_pixels_num = 0;
  		}
  
  		static int thr = 50;
  		int n_over = 0;
  		
  		
		for( y = ymin; y < ymax; y+=2 ) {
			uint32_t * const vr_row = (uint32_t*)( vram + (y-os.bmp_of_y-vr_y_of_corr) * vr_height/(os.bmp_ex_y-vr_y_ex_corr) * vr_pitch ); // 2 pixels
			int b_row_off = y * BMPPITCH;
			uint32_t * const b_row = (uint32_t*)( bvram + b_row_off );   // 4 pixels
			uint32_t * const m_row = (uint32_t*)( bvram_mirror + b_row_off );   // 4 pixels
  
			for ( x = xmin; x < xmax; x++ ) {
				#define BP (b_row[x])
				#define MP (m_row[x])
				#define BN (b_row[x + (BMPPITCH>>2)])
				#define MN (m_row[x + (BMPPITCH>>2)])

				uint32_t pixel = vr_row[xcalc[x]];

				uint32_t bp = BP;
				uint32_t mp = MP;
				uint32_t bn = BN;
				uint32_t mn = MN;
				
//				BP=(pixel&0xff)>>8 | (pixel&0xff000000)>>24;

				if (zd) {
					int zebra_done = 0;
					if (bp != 0 && bp != mp) { little_cleanup(b_row + x, m_row + x); zebra_done = 1; }
					if (bn != 0 && bn != mn) { little_cleanup(b_row + x + (BMPPITCH>>1), m_row + x + (BMPPITCH>>1)); zebra_done = 1; }

					if(!zebra_done) {
						uint32_t p0 = pixel & 0xFF00;
						int color = 0;
					
						if (p0 > zlh) {
							color = COLOR_RED;
						} else if (p0 < zll) {
							color = COLOR_BLUE;
						}
						
						switch(zebra_density) {
							case 0:
								BP = MP = color;
								BN = MN = color<<16;
								break;
							case 1:
								BP = MP = color<<8 | color;
								BN = MN = color<<24 | color<<16;
								break;
							case 2:
								if(!(y&2)) {
										BP = MP = color<<8 | color;
										BN = MN = color<<16 | color<<8;
								} else {
										BP = MP = color<<16 | color<<24;
										BN = MN = color<<24 | color;
								}
								break;
						}
					}
  				}

				if(focus_peaking) {
					int32_t p0 = (pixel >> 24) & 0xFF;
					int32_t p1 = (pixel >>  8) & 0xFF;
					int32_t d = ABS(p0-p1);
					if (d < thr) continue;
					n_over++;
					// executed for 1% of pixels
					if (n_over > MAX_DIRTY_PIXELS) { // threshold too low, abort
						thr = MIN(thr+2, 255);
						continue;
					}

					int color = get_focus_color(thr, d);
					color = (color << 8) | color;   
					if ((bp == 0 || bp == mp) && (bn == 0 || bn == mn)) { // safe to draw
						BP = BN = MP = MN = color;
						if (dirty_pixels_num < MAX_DIRTY_PIXELS) {
							dirty_pixels[dirty_pixels_num++] = (x<<2) + b_row_off;
						}
					}
  				}
  				#undef MP
  				#undef BP
				#undef BN
				#undef MN
  			}
  		}
		int yy=250 * n_over / (os.bmp_ex_x * (os.bmp_ex_y - (bm_lv_y<<1)));
		//~ bmp_printf(FONT_LARGE, 10, 50, "%d ", thr);
//		bmp_printf(FONT_LARGE, 10, 50, "%d %d %d>%d ", thr, n_over, yy, focus_peaking_pthr);
		if ( yy > (int)focus_peaking_pthr) {
			thr++;
		} else {
			thr--;
		}
		int thr_min = (lens_info.iso > 1600 ? 15 : 10);
		thr = COERCE(thr, thr_min, 255);
  	}
}

int focus_peaking_debug = 0;

// thresholded edge detection
static void
draw_zebra_and_focus( int Z, int F )
{
	if (lv_dispsize != 1) return;

/*	int Zd = should_draw_zoom_overlay();
	static int Zdp = 0;
	if (Zd && !Zdp) clrscr_mirror();
	Zdp = Zd;
	if (Zd) msleep(100); // reduce frame rate when zoom overlay is active
	*/
	
	if (unified_loop == 1) { draw_zebra_and_focus_unified(); return; }
	if (unified_loop == 2 && (ext_monitor_hdmi || ext_monitor_rca || (shooting_mode == SHOOTMODE_MOVIE && video_mode_resolution != 0)))
		{ draw_zebra_and_focus_unified(); return; }
	
	if (!global_draw) return;
	
	fps_ticks++;
	
	// HD to LV coordinate transform:
	// non-record: 1056 px: 1.46 ratio (yuck!)
	// record: 1720: 2.38 ratio (yuck!)
	
	// How to scan?
	// Scan the HD vram and do ratio conversion only for the 1% pixels displayed

	bvram_mirror_init();

	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	if (!bvram_mirror) return;
	//~ int BMPPITCH = bmp_pitch();
	int y;

	if (F && focus_peaking)
	{
		// clear previously written pixels
		#define MAX_DIRTY_PIXELS 5000
  		static int* dirty_pixels = 0;
  		if (!dirty_pixels) dirty_pixels = AllocateMemory(MAX_DIRTY_PIXELS * sizeof(int));
  		if (!dirty_pixels) return;
		static int dirty_pixels_num = 0;
		//~ static int very_dirty = 0;
		int i;
		for (i = 0; i < dirty_pixels_num; i++)
		{
			dirty_pixels[i] = COERCE(dirty_pixels[i], 0, 950*540);
			#define B1 *(uint16_t*)(bvram + dirty_pixels[i])
			#define B2 *(uint16_t*)(bvram + dirty_pixels[i] + BMPPITCH)
			#define M1 *(uint16_t*)(bvram_mirror + dirty_pixels[i])
			#define M2 *(uint16_t*)(bvram_mirror + dirty_pixels[i] + BMPPITCH)
			if ((B1 == 0 || B1 == M1) && (B2 == 0 || B2 == M2))
				B1 = B2 = M1 = M2 = 0;
			#undef B1
			#undef B2
			#undef M1
			#undef M2
		}
		dirty_pixels_num = 0;
		
		int bm_pitch = (ext_monitor_hdmi && !recording) ? 960 : 720; // or other value for ext monitor
		int bm_width = bm_pitch;  // 8-bit palette image
		int bm_height = (ext_monitor_hdmi && !recording) ? 540 : 480;
		
		struct vram_info * hd_vram = get_yuv422_hd_vram();
		uint8_t * const hdvram = UNCACHEABLE(hd_vram->vram);
		int hd_pitch  = hd_vram->pitch;
		int hd_height = hd_vram->height;
		int hd_width  = hd_vram->width;
		
		//~ bmp_printf(FONT_MED, 30, 100, "HD %dx%d ", hd_width, hd_height);
		
		int bm_skipv = 50;
		int bm_skiph = 100;
		int hd_skipv = bm_skipv * hd_height / bm_height;
		int hd_skiph = bm_skiph * hd_width / bm_width;
		
		static int thr = 50;
		
		int n_over = 0;
		int n_total = 0;
		// look in the HD buffer

		int rec_off = (recording ? 90 : 0);
		int step = (focus_peaking == 1) 
						? (recording ? 2 : 1)
						: (recording ? 4 : 2);
		for( y = hd_skipv; y < hd_height - hd_skipv; y += 2 )
		{
			uint32_t * const hd_row = (uint32_t*)( hdvram + y * hd_pitch ); // 2 pixels
			uint32_t * const hd_row_end = hd_row + hd_width/2 - hd_skiph/2;
			
			uint32_t* hdp; // that's a moving pointer
			for (hdp = hd_row + hd_skiph/2 ; hdp < hd_row_end ; hdp += step )
			{
				#define PX_AB (*hdp)        // current pixel group
				#define PX_CD (*(hdp + 1))  // next pixel group
				#define a ((int32_t)(PX_AB >>  8) & 0xFF)
				#define b ((int32_t)(PX_AB >> 24) & 0xFF)
				#define c ((int32_t)(PX_CD >>  8) & 0xFF)
				#define d ((int32_t)(PX_CD >> 24) & 0xFF)
				
				#define mBC MIN(b,c)
				#define AE MIN(a,b)
				#define BE MIN(a, mBC)
				#define CE MIN(mBC, d)

				#define MBC MAX(b,c)
				#define AD MAX(a,b)
				#define BD MAX(a, MBC)
				#define CD MAX(MBC, d)

				#define BED MAX(AE,MAX(BE,CE))
				#define BDE MIN(AD,MIN(BD,CD))

				#define SIGNBIT(x) (x & (1<<31))
				#define CHECKSIGN(a,b) (SIGNBIT(a) ^ SIGNBIT(b) ? 0 : 0xFF)
				#define D1 (b-a)
				#define D2 (c-b)
				#define D3 (d-c)

				#define e_morph (ABS(b - ((BDE + BED) >> 1)) << 3)
				//~ #define e_opposite_sign (MAX(0, - (c-b)*(b-a)) >> 3)
				//~ #define e_sign3 CHECKSIGN(D1,D3) & CHECKSIGN(D1,-D2) & ((ABS(D1) + ABS(D2) + ABS(D3)) >> 1)

				int e = (focus_peaking == 1) ? ABS(D1) :
						(focus_peaking == 2) ? e_morph : 0;
				#undef a
				#undef b
				#undef c
				#undef d
				#undef z
				
				/*if (focus_peaking_debug)
				{
					int b_row_off = COERCE((y + rec_off) * bm_width / hd_width, 0, 539) * BMPPITCH;
					uint16_t * const b_row = (uint16_t*)( bvram + b_row_off );   // 2 pixels
					int x = 2 * (hdp - hd_row) * bm_width / hd_width;
					x = COERCE(x, 0, 960);
					int c = (COERCE(e, 0, thr*2) * 41 / thr / 2) + 38;
					b_row[x/2] = c | (c << 8);
				}*/
				
				n_total++;
				if (e < thr) continue;
				// else
				{ // executed for 1% of pixels
					n_over++;
					if (n_over > MAX_DIRTY_PIXELS) // threshold too low, abort
					{
						thr = MIN(thr+2, 255);
						return;
					}

					int color = get_focus_color(thr, e);
					//~ int color = COLOR_RED;
					color = (color << 8) | color;   
					int b_row_off = COERCE((y + rec_off) * bm_width / hd_width, 0, 539) * BMPPITCH;
					uint16_t * const b_row = (uint16_t*)( bvram + b_row_off );   // 2 pixels
					uint16_t * const m_row = (uint16_t*)( bvram_mirror + b_row_off );   // 2 pixels
					
					int x = 2 * (hdp - hd_row) * bm_width / hd_width;
					x = COERCE(x, 0, 960);
					
					uint16_t pixel = b_row[x/2];
					uint16_t mirror = m_row[x/2];
					uint16_t pixel2 = b_row[x/2 + BMPPITCH/2];
					uint16_t mirror2 = m_row[x/2 + BMPPITCH/2];
					if ((pixel == 0 || pixel == mirror) && (pixel2 == 0 || pixel2 == mirror2)) // safe to draw
					{
						b_row[x/2] = color;
						b_row[x/2 + BMPPITCH/2] = color;
						m_row[x/2] = color;
						m_row[x/2 + BMPPITCH/2] = color;
						if (dirty_pixels_num < MAX_DIRTY_PIXELS)
						{
							dirty_pixels[dirty_pixels_num++] = x + b_row_off;
						}
					}
				}
			}
		}
		//~ bmp_printf(FONT_LARGE, 10, 50, "%d ", thr);
		if (1000 * n_over / n_total > (int)focus_peaking_pthr) thr++;
		else thr--;
		
		int thr_min = (lens_info.iso > 1600 ? 15 : 10);
		thr = COERCE(thr, thr_min, 255);
	}
	
	int zd = Z && zebra_draw && (expsim || PLAY_MODE) && (!zebra_nrec || !recording); // when to draw zebras
	if (zd)
	{
		int zlh = zebra_level_hi;
		int zll = zebra_level_lo;

		uint8_t * const lvram = get_yuv422_vram()->vram;
		int lvpitch = YUV422_LV_PITCH;
		for( y = 40; y < 440; y += 2 )
		{
			uint32_t color_over = zebra_color_word_row(COLOR_RED, y);
			uint32_t color_under = zebra_color_word_row(COLOR_BLUE, y);
			uint32_t color_over_2 = zebra_color_word_row(COLOR_RED, y+1);
			uint32_t color_under_2 = zebra_color_word_row(COLOR_BLUE, y+1);
			
			uint32_t * const v_row = (uint32_t*)( lvram + y * lvpitch );          // 2 pixels
			uint32_t * const b_row = (uint32_t*)( bvram + y * BMPPITCH);          // 4 pixels
			uint32_t * const m_row = (uint32_t*)( bvram_mirror + y * BMPPITCH );  // 4 pixels
			
			uint32_t* lvp; // that's a moving pointer through lv vram
			uint32_t* bp;  // through bmp vram
			uint32_t* mp;  // through mirror

			for (lvp = v_row, bp = b_row, mp = m_row ; lvp < v_row + YUV422_LV_PITCH/4 ; lvp += 2, bp++, mp++)
			{
				#define BP (*bp)
				#define MP (*mp)
				#define BN (*(bp + BMPPITCH/4))
				#define MN (*(mp + BMPPITCH/4))
				if (BP != 0 && BP != MP) { little_cleanup(bp, mp); continue; }
				if (BN != 0 && BN != MN) { little_cleanup(bp + BMPPITCH/4, mp + BMPPITCH/4); continue; }
				
				if (zebra_draw == 2) // rgb
				{
					uint32_t pixel = *lvp;
					uint32_t p1 = (pixel >> 24) & 0xFF;
					uint32_t p2 = (pixel >>  8) & 0xFF;
					int Y = (p1+p2) / 2;
					int8_t U = (pixel >>  0) & 0xFF;
					int8_t V = (pixel >> 16) & 0xFF;
					int R = Y + 1437 * V / 1024;
					int G = Y -  352 * U / 1024 - 731 * V / 1024;
					int B = Y + 1812 * U / 1024;
					
					//~ bmp_printf(FONT_SMALL, 0, 0, "%d %d %d %d   ", Y, R, G, B);

					BP = MP = zebra_rgb_color(Y < zll, R > zlh, G > zlh, B > zlh, y);
					BN = MN = zebra_rgb_color(Y < zll, R > zlh, G > zlh, B > zlh, y+1);
				}
				else
				{
					int p0 = (*lvp) >> 8 & 0xFF;
					if (p0 > zlh)
					{
						BP = MP = color_over;
						BN = MN = color_over_2;
					}
					else if (p0 < zll)
					{
						BP = MP = color_under;
						BN = MN = color_under_2;
					}
					else if (BP)
						BN = MN = BP = MP = 0;
				}
					
				#undef MP
				#undef BP
			}
		}
	}
}


// clear only zebra, focus assist and whatever else is in BMP VRAM mirror
void
clrscr_mirror( void )
{
	if (!lv_drawn()) return;
	if (!global_draw) return;

	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	if (!bvram_mirror) return;

	int y;
	for( y = 0; y < 480; y++ )
	{
		uint32_t * const b_row = (uint32_t*)( bvram + y * BMPPITCH);
		uint32_t * const m_row = (uint32_t*)( bvram_mirror + y * BMPPITCH );
		
		uint32_t* bp;  // through bmp vram
		uint32_t* mp;  // through mirror

		for (bp = b_row, mp = m_row ; bp < b_row + BMPPITCH / 4; bp++, mp++)
		{
			#define BP (*bp)
			#define MP (*mp)
			if (BP != 0)
			{ 
				if (BP == MP) BP = MP = 0;
				else little_cleanup(bp, mp);
			}			
			#undef MP
			#undef BP
		}
	}
}

/*
void
draw_false( void )
{
	if (!global_draw) return;
	bvram_mirror_init();
	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	if (!bvram_mirror) return;

	uint32_t x,y;
	uint8_t * const lvram = CACHEABLE(YUV422_LV_BUFFER);
	int lvpitch = YUV422_LV_PITCH;
	for( y = 100; y < 480-100; y++ )
	{
		uint16_t * const v_row = (uint16_t*)( lvram + y * lvpitch );        // 1 pixel
		uint8_t * const b_row = (uint8_t*)( bvram + y * BMPPITCH);          // 1 pixel
		uint8_t * const m_row = (uint8_t*)( bvram_mirror + y * BMPPITCH );  // 1 pixel
		
		uint16_t* lvp; // that's a moving pointer through lv vram
		uint8_t* bp;  // through bmp vram
		uint8_t* mp;  // through mirror

		for (lvp = v_row + 100, bp = b_row + 100, mp = m_row + 100; lvp < v_row + 720-100 ; lvp++, bp++, mp++)
		{
			if (*bp != 0 && *bp != *mp) continue;
			*mp = *bp = false_colour[(*lvp) >> 8];
		}
	}
}*/

void
draw_false_downsampled( void )
{
	if (!PLAY_MODE)
	{
		if (!expsim) return;
	}
	bvram_mirror_init();
	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	if (!bvram_mirror) return;

	int y;
	uint8_t * const lvram = get_yuv422_vram()->vram;
	int lvpitch = YUV422_LV_PITCH;
	for( y = 40; y < 440; y++ )
	{
		uint32_t * const v_row = (uint32_t*)( lvram + y * lvpitch );        // 2 pixel
		uint16_t * const b_row = (uint16_t*)( bvram + y * BMPPITCH);          // 2 pixel
		uint16_t * const m_row = (uint16_t*)( bvram_mirror + y * BMPPITCH );  // 2 pixel
		
		uint32_t* lvp; // that's a moving pointer through lv vram
		uint16_t* bp;  // through bmp vram
		uint16_t* mp;  // through mirror

		for (lvp = v_row, bp = b_row, mp = m_row; lvp < v_row + 720 ; lvp++, bp++, mp++)
		{
			if (*bp != 0 && *bp != *mp) continue;
			int16_t c = false_colour[falsecolor_palette][((*lvp) >> 8) & 0xFF];
			*mp = *bp = c | (c << 8);
		}
	}
}

/*
static void
draw_zebra( void )
{
	if (!lv_drawn()) return;
	
	uint8_t * const bvram = bmp_vram();
    uint32_t a = 0;
    
	bvram_mirror_init();

    DebugMsg(DM_MAGIC, 3, "***************** draw_zebra() **********************");
    DebugMsg(DM_MAGIC, 3, "zebra_draw = %d, cfg_draw_meters = %x", zebra_draw, ext_cfg_draw_meters() );
    //~ dump_seg(0x40D07800, 1440*480, "B:/vram1.dat");
    
	// If we don't have a bitmap vram yet, nothing to do.
	if( !bvram )
	{
		DebugMsg( DM_MAGIC, 3, "draw_zebra() no bvram, fail");
		return;
	}
	if (!bvram_mirror)
	{
		DebugMsg( DM_MAGIC, 3, "draw_zebra() no bvram_mirror, fail");
		return;
	}

	struct vram_info * vram = get_yuv422_vram();
    if (hist_draw)
    {
        // something is fishy here => camera refused to boot due to this function...
        hist_build(vram->vram, vram->width, vram->pitch);
    }

    DebugMsg(DM_MAGIC, 3, "yay!");

	// skip the audio meter at the top and the bar at the bottom
	// hardcoded; should use a constant based on the type of display
	// 33 is the bottom of the meters; 55 is the crop mark
	uint32_t x,y;
	int cfg_draw_meters = ext_cfg_draw_meters();
	//int BMPPITCH = bmp_pitch();
	
	int zd = (zebra_draw == 1) || (zebra_draw == 2 && recording == 0);  // when to draw zebras
	for( y=1 ; y < 480; y++ )
	{
        // if audio meters are enabled, don't draw in this area
        if (y < 33 && (cfg_draw_meters == 1 || (cfg_draw_meters == 2 && shooting_mode == SHOOTMODE_MOVIE))) continue;
        
		uint32_t * const v_row = (uint32_t*)( vram->vram + y * vram->pitch );
		uint16_t * const b_row = (uint16_t*)( bvram + y * BMPPITCH );
		uint16_t * const m_row = (uint16_t*)( bvram_mirror + y * BMPPITCH );

		//~ bmp_printf(FONT_MED, 30, 50, "Row: %8x/%8x", b_row, m_row);
		//~ bmp_printf(FONT_MED, 30, 70, "Pixel: %8x/%8x", b_row[8], m_row[8]);

		// Iterate over the pixels in the scan row
		// two at a time to read the pixel buf in 32 bit chunks
		// otherwise we get err70 aborts while drawing regions
		// in the bitmap vram.
		for( x=2 ; x < vram->width-2 ; x+=2 ) // width = 720
		{
			// Abort as soon as the new menu is drawn
			if( gui_menu_task || !lv_drawn() )
				return;

			uint16_t pixel = b_row[x/2];
			uint16_t mirror = m_row[x/2];

			// cropmarks: black border in movie mode
			if (crop_black_border && (pixel == 0 || pixel == (COLOR_BG << 8 | COLOR_BG)) && shooting_mode == SHOOTMODE_MOVIE && (y < 40 || y > 440))
			{
				b_row[x/2] = (2 << 8 | 2); // black borders by default
				if( crop_draw) check_crop( x, y, b_row, v_row, vram->pitch, m_row);
			}

			if (pixel != 0 && pixel != mirror)
			{
				continue; // Canon code has drawn here, do not overwrite
			}
				
			// Ignore the regions where the histogram will be drawn
			if( hist_draw
			&&  y >= hist_y
			&&  y <  hist_y + hist_height
			&&  x >= hist_x
			&&  x <  hist_x + hist_width + 4
			)
				continue; // histogram does not overwrite any Canon stuff => no problem!

			// Ignore the regions where the waveform will be drawn
			//~ if( waveform_draw
			//~ &&  y >= waveform_y
			//~ &&  y <  waveform_y + WAVEFORM_HEIGHT
			//~ &&  x >= waveform_x
			//~ &&  x <  waveform_x + WAVEFORM_WIDTH
			//~ )
				//~ continue;

			// Ignore the timecode region
			//~ if( y >= timecode_y
			//~ &&  y <  timecode_y + timecode_height
			//~ &&  x >= timecode_x
			//~ &&  x <  timecode_x + timecode_width
			//~ )
				//~ continue;

			if( crop_draw && check_crop( x, y, b_row, v_row, vram->pitch, m_row) )
			{
				//~ m_row[x/2] = b_row[x/2];
				continue;
			}
			//~ if( edge_draw && check_edge( x, y, b_row, v_row, vram->pitch ) )
				//~ continue;

			if( zd && check_zebra( x, y, b_row, v_row, vram->pitch, m_row) )
			{
				//~ m_row[x/2] = b_row[x/2];
				continue;
			}

			// Nobody drew on it, make it clear
			if (pixel) b_row[x/2] = 0;
			//m_row[x/2] = 0;
		}
	}

	if( hist_draw )
		hist_draw_image( hist_x, hist_y );

	if( spotmeter_draw)
		spotmeter_step();
	//~ if( waveform_draw )
		//~ waveform_draw_image( waveform_x, waveform_y );
    DebugMsg(DM_MAGIC, 3, "***************** draw_zebra done **********************");
}*/

static void
zebra_lo_toggle( void * priv )
{
	zebra_level_lo = mod(zebra_level_lo + 1, 50);
}

static void
zebra_lo_toggle_reverse( void * priv )
{
	zebra_level_lo = mod(zebra_level_lo - 1, 50);
}

static void
zebra_hi_toggle( void * priv )
{
	zebra_level_hi = 200 + mod(zebra_level_hi - 200 + 1, 56);
}

static void
zebra_hi_toggle_reverse( void * priv )
{
	zebra_level_hi = 200 + mod(zebra_level_hi - 200 - 1, 56);
}

static void global_draw_toggle(void* priv)
{
	menu_binary_toggle(priv);
	if (!global_draw && lv_drawn()) bmp_fill(0, 0, 0, 720, 480);
}

#define MAX_CROP_NAME_LEN 15
#define MAX_CROPMARKS 9
int num_cropmarks = 0;
char cropmark_names[MAX_CROPMARKS][MAX_CROP_NAME_LEN];

// Cropmark sorting code contributed by Nathan Rosenquist
void sort_cropmarks()
{
	int i = 0;
	int j = 0;
	
	char aux[MAX_CROP_NAME_LEN];
	
	for (i=0; i<num_cropmarks; i++)
	{
		for (j=i+1; j<num_cropmarks; j++)
		{
			if (strcmp(cropmark_names[i], cropmark_names[j]) > 0)
			{
				strcpy(aux, cropmark_names[i]);
				strcpy(cropmark_names[i], cropmark_names[j]);
				strcpy(cropmark_names[j], aux);
			}
		}
	}
}

static void find_cropmarks()
{
	struct fio_file file;
	struct fio_dirent * dirent = FIO_FindFirstEx( "B:/CROPMKS/", &file );
	if( IS_ERROR(dirent) )
	{
		bmp_printf( FONT_LARGE, 40, 40, "CROPMKS dir missing" );
		return;
	}
	int k = 0;
	do {
		int n = strlen(file.name);
		if ((n > 4) && (streq(file.name + n - 4, ".BMP") || streq(file.name + n - 4, ".bmp")) && (file.name[0] != '.'))
		{
			if (k >= MAX_CROPMARKS)
			{
				bmp_printf(FONT_LARGE, 0, 50, "TOO MANY CROPMARKS (max=%d)", MAX_CROPMARKS);
				break;
			}
			snprintf(cropmark_names[k], MAX_CROP_NAME_LEN, "%s", file.name);
			k++;
		}
	} while( FIO_FindNextEx( dirent, &file ) == 0);
	num_cropmarks = k;
	sort_cropmarks();
}
static void reload_cropmark(int i)
{
	static int old_i = -1;
	if (i == old_i) return; 
	old_i = i;
	//~ bmp_printf(FONT_LARGE, 0, 100, "reload crop: %d", i);

	if (cropmarks)
	{
		FreeMemory(cropmarks);
		cropmarks = 0;
	}
	
	i = COERCE(i, 0, num_cropmarks);
	if (i)
	{
		char bmpname[100];
		snprintf(bmpname, sizeof(bmpname), "B:/CROPMKS/%s", cropmark_names[i-1]);
		cropmarks = bmp_load(bmpname);
		if (!cropmarks) bmp_printf(FONT_LARGE, 0, 50, "LOAD ERROR %d:%s   ", i, bmpname);
	}
}

static void
crop_toggle( int sign )
{
	crop_draw = mod(crop_draw + sign, num_cropmarks + 1);  // 0 = off, 1..num_cropmarks = cropmarks
	reload_cropmark(crop_draw);
}

static void crop_toggle_forward(void* priv)
{
	crop_toggle(1);
}

static void crop_toggle_reverse(void* priv)
{
	crop_toggle(-1);
}

/*
static void
zebra_hi_display( void * priv, int x, int y, int selected )
{
	bmp_printf( 
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"ZebraThrHI  : %d   ",
		*(unsigned*) priv
	);
}

static void
zebra_lo_display( void * priv, int x, int y, int selected )
{
	bmp_printf( 
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"ZebraThrLO  : %d   ",
		*(unsigned*) priv
	);
}*/


static void
zebra_draw_display( void * priv, int x, int y, int selected )
{
	unsigned z = *(unsigned*) priv;
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Zebras      : %s, %d..%d",
		z == 1 ? "Luma" : (z == 2 ? "RGB" : "OFF"),
		zebra_level_lo, zebra_level_hi
	);
	menu_draw_icon(x, y, MNI_BOOL_GDR(z), 0);
}

static void
falsecolor_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"False Color : %s",
		falsecolor_draw ? "ON" : "OFF"
	);
	int i;
	for (i = 0; i < 256; i++)
	{
		draw_line(x + 364 + i, y + 2, x + 364 + i, y + font_large.height - 2, false_colour[falsecolor_palette][i]);
	}
	menu_draw_icon(x, y, MNI_BOOL_GDR(falsecolor_draw), 0);
}

static void
falsecolor_palette_toggle(void* priv)
{
	falsecolor_palette = mod(falsecolor_palette+1, COUNT(false_colour));
}
/*
static void
focus_debug_display( void * priv, int x, int y, int selected )
{
	unsigned fc = *(unsigned*) priv;
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"FPeak debug : %s",
		focus_peaking_debug ? "ON" : "OFF"
	);
}*/

static void
focus_peaking_display( void * priv, int x, int y, int selected )
{
	unsigned f = *(unsigned*) priv;
	if (f)
		bmp_printf(
			selected ? MENU_FONT_SEL : MENU_FONT,
			x, y,
			"Focus Peak  : %s,%d.%d,%s", 
			focus_peaking == 1 ? "HDIF" : 
			focus_peaking == 2 ? "MORF" : "?",
			focus_peaking_pthr / 10, focus_peaking_pthr % 10, 
			focus_peaking_color == 0 ? "R" :
			focus_peaking_color == 1 ? "G" :
			focus_peaking_color == 2 ? "B" :
			focus_peaking_color == 3 ? "C" :
			focus_peaking_color == 4 ? "M" :
			focus_peaking_color == 5 ? "Y" :
			focus_peaking_color == 6 ? "cc1" :
			focus_peaking_color == 7 ? "cc2" : "err"
		);
	else
		bmp_printf(
			selected ? MENU_FONT_SEL : MENU_FONT,
			x, y,
			"Focus Peak  : OFF"
		);
	menu_draw_icon(x, y, MNI_BOOL_GDR(f), 0);
}

static void focus_peaking_adjust_thr(void* priv)
{
	if (focus_peaking)
	{
		focus_peaking_pthr += (focus_peaking_pthr < 10 ? 1 : 5);
		if (focus_peaking_pthr > 50) focus_peaking_pthr = 1;
	}
}
static void focus_peaking_adjust_color(void* priv)
{
	if (focus_peaking)
		focus_peaking_color = mod(focus_peaking_color + 1, 8);
}
static void
crop_display( void * priv, int x, int y, int selected )
{
	//~ extern int retry_count;
	int index = crop_draw;
	index = COERCE(index, 0, num_cropmarks);
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Cropmks(%d/%d): %s%s",
		 index, num_cropmarks,
		 index  ? cropmark_names[index-1] : "OFF",
		 (cropmarks || !index) ? "" : "!" // ! means error
	);
	//~ int h = font_large.height;
	//~ int w = h * 720 / 480;
	//~ bmp_draw_scaled_ex(cropmarks, x + 572, y, w, h, 0, 0);
	if (index && cropmark_movieonly && shooting_mode != SHOOTMODE_MOVIE)
		menu_draw_icon(x, y, MNI_WARNING, 0);
	menu_draw_icon(x, y, MNI_BOOL_GDR(index), 0);
}

/*
static void
focus_graph_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Focus Graph : %s",
		*(unsigned*) priv ? "ON " : "OFF"
	);
}*/

/*
static void
edge_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Edgedetect  : %s",
		*(unsigned*) priv ? "ON " : "OFF"
	);
}*/

static void
hist_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Histo/Wavefm: %s/%s",
		hist_draw == 1 ? "Luma" : hist_draw == 2 ? "RGB" : "OFF",
		waveform_draw == 1 ? "Small" : waveform_draw == 2 ? "Large" : "OFF"
	);
	//~ bmp_printf(FONT_MED, x + 460, y+5, "[SET/Q]");
	menu_draw_icon(x, y, MNI_BOOL_GDR(hist_draw || waveform_draw), 0);
}

static void
global_draw_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Global Draw : %s",
		global_draw ? "ON " : "OFF"
	);
}

static void
waveform_display( void * priv, int x, int y, int selected )
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Waveform    : %s",
		*(unsigned*) priv ? "ON " : "OFF"
	);
	menu_draw_icon(x, y, MNI_BOOL_GDR(*(unsigned*) priv), 0);
}
static void 
waveform_toggle(void* priv)
{
	waveform_draw = mod(waveform_draw+1, 3);
	bmp_fill(0, 360, 240-50, 360, 240);
}


static void
clearscreen_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	int mode = *(int*) priv;
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Display     : %s",
		mode == 0 ? "No special action" : 
		mode == 1 ? "Clr on HalfShutter" : 
		mode == 2 ? "Clr when idle" :
		mode == 3 ? "Turn OFF when idle" :
		"Error"
	);
}

static void
zoom_overlay_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Magic Zoom  : %s%s%s",
		zoom_overlay_mode == 0 ? "OFF" :
		zoom_overlay_mode == 1 ? "Zrec, " :
		zoom_overlay_mode == 2 ? "Zr+F, " :
		zoom_overlay_mode == 3 ? "(+), " : "err",

		zoom_overlay_mode == 0 ? "" :
			zoom_overlay_size == 0 ? "Small, " :
			zoom_overlay_size == 1 ? "Med, " :
			zoom_overlay_size == 2 ? "Large, " :
			zoom_overlay_size == 3 ? "SmallX2, " :
			zoom_overlay_size == 4 ? "MedX2, " :
			zoom_overlay_size == 5 ? "LargeX2, " :  "err",
		zoom_overlay_mode == 0 ? "" :
			zoom_overlay_pos == 0 ? "AFF" :
			zoom_overlay_pos == 1 ? "NW" :
			zoom_overlay_pos == 2 ? "NE" :
			zoom_overlay_pos == 3 ? "SE" :
			zoom_overlay_pos == 4 ? "SW" : "err"
	);
	menu_draw_icon(x, y, MNI_BOOL_GDR(zoom_overlay_mode), 0);
}

static void
split_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Split Screen: %s%s",
		zoom_overlay_split ? "ON" : "OFF",
		zoom_overlay_split && zoom_overlay_split_zerocross ? ", zerocross" : ""
	);
	menu_draw_icon(x, y, MNI_BOOL_GDR(zoom_overlay_split), 0);
}

static void split_zerocross_toggle(void* priv)
{
	zoom_overlay_split_zerocross = !zoom_overlay_split_zerocross;
}

static void
spotmeter_menu_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{

	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"Spotmeter   : %s",
		spotmeter_draw == 0 ? "OFF" : (spotmeter_formula == 0 ? "Percent" : spotmeter_formula == 1 ? "IRE -1..101" : "IRE 0..108")
	);
	menu_draw_icon(x, y, MNI_BOOL_GDR(spotmeter_draw), 0);
}

static void 
spotmeter_formula_toggle(void* priv)
{
	spotmeter_formula = mod(spotmeter_formula + 1, 3);
}



void get_spot_yuv(int dx, int* Y, int* U, int* V)
{
	struct vram_info *	vram = get_yuv422_vram();

	if( !vram->vram )
		return;
	const uint16_t*		vr = (void*) YUV422_LV_BUFFER_DMA_ADDR;
	const unsigned		width = vram->width;
	//~ const unsigned		pitch = vram->pitch;
	const unsigned		height = vram->height;
	unsigned		x, y;

	draw_line(width/2 - dx, height/2 - dx, width/2 + dx, height/2 - dx, COLOR_WHITE);
	draw_line(width/2 + dx, height/2 - dx, width/2 + dx, height/2 + dx, COLOR_WHITE);
	draw_line(width/2 + dx, height/2 + dx, width/2 - dx, height/2 + dx, COLOR_WHITE);
	draw_line(width/2 - dx, height/2 + dx, width/2 - dx, height/2 - dx, COLOR_WHITE);
	
	unsigned sy = 0;
	int32_t su = 0, sv = 0; // Y is unsigned, U and V are signed
	// Sum the values around the center
	for( y = height/2 - dx ; y <= height/2 + dx ; y++ )
	{
		for( x = width/2 - dx ; x <= width/2 + dx ; x++ )
		{
			uint16_t p = vr[ x + y * width ];
			sy += (p >> 8) & 0xFF;
			if (x % 2) sv += (int8_t)(p & 0x00FF); else su += (int8_t)(p & 0x00FF);
		}
	}

	sy /= (2 * dx + 1) * (2 * dx + 1);
	su /= (dx + 1) * (2 * dx + 1);
	sv /= (dx + 1) * (2 * dx + 1);

	*Y = sy;
	*U = su;
	*V = sv;
}

int get_spot_motion(int dx, int draw)
{
	struct vram_info *	vram = get_yuv422_vram();

	if( !vram->vram )
		return 0;
	const uint16_t*		vr1 = (void*)YUV422_LV_BUFFER_DMA_ADDR;
	const uint16_t*		vr2 = get_fastrefresh_422_buf();
	uint8_t * const		bm = bmp_vram();
	const unsigned		width = vram->width;
	//~ const unsigned		pitch = vram->pitch;
	const unsigned		height = vram->height;
	unsigned		x, y;

	draw_line(width/2 - dx, height/2 - dx, width/2 + dx, height/2 - dx, COLOR_WHITE);
	draw_line(width/2 + dx, height/2 - dx, width/2 + dx, height/2 + dx, COLOR_WHITE);
	draw_line(width/2 + dx, height/2 + dx, width/2 - dx, height/2 + dx, COLOR_WHITE);
	draw_line(width/2 - dx, height/2 + dx, width/2 - dx, height/2 - dx, COLOR_WHITE);
	
	unsigned D = 0;
	for( y = height/2 - dx ; y <= height/2 + dx ; y++ )
	{
		for( x = width/2 - dx ; x <= width/2 + dx ; x++ )
		{
			int p1 = (vr1[ x + y * width ] >> 8) & 0xFF;
			int p2 = (vr2[ x + y * width ] >> 8) & 0xFF;
			int dif = ABS(p1 - p2);
			D += dif;
			if (draw) bm[x + y * BMPPITCH] = false_colour[4][dif & 0xFF];
		}
	}
	
	D = D * 2;
	D /= (2 * dx + 1) * (2 * dx + 1);
	return D;
}

int get_spot_focus(int dx)
{
	struct vram_info *	vram = get_yuv422_vram();

	if( !vram->vram )
		return 0;
	const uint32_t*		vr = (uint32_t*) vram->vram; // 2px
	const unsigned		width = vram->width;
	//~ const unsigned		pitch = vram->pitch;
	const unsigned		height = vram->height;
	unsigned		x, y;
	
	unsigned sf = 0;
	unsigned br = 0;
	// Sum the absolute difference of values around the center
	for( y = height/2 - dx ; y <= height/2 + dx ; y++ )
	{
		for( x = width/2 - dx ; x <= width/2 + dx ; x += 2 )
		{
			uint32_t p = vr[ x/2 + y * width/2 ];
			int32_t p0 = (p >> 24) & 0xFF;
			int32_t p1 = (p >>  8) & 0xFF;
			sf += ABS(p1 - p0);
			br += p1 + p0;
		}
	}
	return sf / (br >> 14);
}

extern int lv_disp_mode;

void spotmeter_step()
{
    //~ if (!lv_drawn()) return;
	struct vram_info *	vram = get_yuv422_vram();

	if( !vram->vram )
		return;
	
	const uint16_t*		vr = (uint16_t*) vram->vram;
	const unsigned		width = vram->width;
	//~ const unsigned		pitch = vram->pitch;
	const unsigned		height = vram->height;
	const unsigned		dx = spotmeter_size;
	unsigned		sum = 0;
	unsigned		x, y;

	// Sum the values around the center
	for( y = height/2 - dx ; y <= height/2 + dx ; y++ )
	{
		for( x = width/2 - dx ; x <= width/2 + dx ; x++ )
			sum += (vr[ x + y * width]) & 0xFF00;
	}

	sum /= (2 * dx + 1) * (2 * dx + 1);

	// Scale to 100%
	const unsigned		scaled = (100 * sum) / 0xFF00;
	
	// spotmeter color: 
	// black on transparent, if brightness > 60%
	// white on transparent, if brightness < 50%
	// previous value otherwise
	
	// if false color is active, draw white on semi-transparent gray
	
	static int fg = 0;
	if (scaled > 60) fg = COLOR_BLACK;
	if (scaled < 50 || falsecolor_draw) fg = COLOR_WHITE;
	int bg = falsecolor_draw ? COLOR_BG : 0;

	int xc = (ext_monitor_hdmi && !recording) ? 480 : 360;
	int yc = (ext_monitor_hdmi && !recording) ? 270 : 240;
	bmp_draw_rect(fg, xc - dx, yc - dx, 2*dx+1, 2*dx+1);
	yc += dx + 20;
	yc -= font_med.height/2;
	xc -= 2 * font_med.width;

	if (spotmeter_formula == 0)
	{
		bmp_printf(
			FONT(FONT_MED, fg, bg),
			xc, yc, 
			"%3d%%",
			scaled
		);
	}
	else
	{
		int ire_aj = (((int)sum >> 8) - 2) * 102 / 253 - 1; // formula from AJ: (2...255) -> (-1...101)
		int ire_piers = ((int)sum >> 8) * 108/255;           // formula from Piers: (0...255) -> (0...108)
		int ire = (spotmeter_formula == 1) ? ire_aj : ire_piers;
		
		bmp_printf(
			FONT(FONT_MED, fg, bg),
			xc, yc, 
			"%s%3d", // why does %4d display garbage?!
			ire < 0 ? "-" : " ",
			ire < 0 ? -ire : ire
		);
		bmp_printf(
			FONT(FONT_SMALL, fg, 0),
			xc + font_med.width*4, yc,
			"IRE\n%s",
			spotmeter_formula == 1 ? "-1..101" : "0..108"
		);
	}
}

void hdmi_test_toggle(void* priv)
{
	ext_monitor_hdmi = !ext_monitor_hdmi;
}


void zoom_overlay_main_toggle(void* priv)
{
	zoom_overlay_mode = mod(zoom_overlay_mode + 1, 4);
}

void zoom_overlay_size_toggle(void* priv)
{
	zoom_overlay_size = mod(zoom_overlay_size + 1, 5);
}

CONFIG_INT("lv.disp.profiles", disp_profiles_0, 1);

static void
disp_profiles_0_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	bmp_printf(
		selected ? MENU_FONT_SEL : MENU_FONT,
		x, y,
		"DISP profiles  : %d", 
		disp_profiles_0 + 1
	);
}

/*
static void
transparent_overlay_display(
	void *			priv,
	int			x,
	int			y,
	int			selected
)
{
	if (transparent_overlay && (transparent_overlay_offx || transparent_overlay_offy))
		bmp_printf(
			selected ? MENU_FONT_SEL : MENU_FONT,
			x, y,
			"Ghost Image : ON, dx=%d, dy=%d", 
			transparent_overlay_offx, 
			transparent_overlay_offy
		);
	else
		bmp_printf(
			selected ? MENU_FONT_SEL : MENU_FONT,
			x, y,
			"Ghost Image : %s", 
			transparent_overlay ? "ON" : "OFF"
		);
	menu_draw_icon(x, y, MNI_BOOL_GDR(transparent_overlay), 0);
}*/

void transparent_overlay_offset(int dx, int dy)
{
	transparent_overlay_offx = COERCE((int)transparent_overlay_offx + dx, -650, 650);
	transparent_overlay_offy = COERCE((int)transparent_overlay_offy + dy, -400, 400);
	BMP_SEM( show_overlay(); )
}

void transparent_overlay_offset_clear(int dx, int dy)
{
	transparent_overlay_offx = transparent_overlay_offy = 0;
}

struct menu_entry zebra_menus[] = {
	{
		.priv		= &global_draw,
		.select		= global_draw_toggle,
		.display	= global_draw_display,
		.help = "Enable/disable ML overlay graphics (zebra, cropmarks...)"

	},
	{
		.priv		= &hist_draw,
		.select		= menu_ternary_toggle,
		.select_auto = waveform_toggle,
		.display	= hist_display,
		.help = "Histogram [SET] and Waveform [Q] for evaluating exposure."
	},
	{
		.priv		= &zebra_draw,
		.select		= menu_ternary_toggle,
		.select_reverse = zebra_lo_toggle, 
		.select_auto = zebra_hi_toggle,
		.display	= zebra_draw_display,
		.help = "Zebra stripes: show overexposed or underexposed areas."
	},
	{
		.priv		= &falsecolor_draw,
		.display	= falsecolor_display,
		.select		= menu_binary_toggle,
		.select_auto = falsecolor_palette_toggle,
		.help = "Shows brightness level as color-coded. [Q]: change palette."
	},
	{
		.priv = &crop_draw,
		.display	= crop_display,
		.select		= crop_toggle_forward,
		.select_reverse		= crop_toggle_reverse,
		.help = "Cropmarks for framing. Usually shown only in Movie mode."
	},
	/*{
		.priv = &transparent_overlay, 
		.display = transparent_overlay_display, 
		.select = menu_binary_toggle,
		.select_auto = transparent_overlay_offset_clear,
		.help = "Overlay any image in LiveView. In PLAY mode, press LV btn."
	},*/
	{
		.priv			= &spotmeter_draw,
		.select			= menu_binary_toggle,
		.select_auto	= spotmeter_formula_toggle,
		.display		= spotmeter_menu_display,
		.help = "Measure brightness in the frame center. [Q]: Percent/IRE."
	},
	{
		.priv			= &focus_peaking,
		.display		= focus_peaking_display,
		.select			= menu_ternary_toggle,
		.select_reverse = focus_peaking_adjust_color, 
		.select_auto    = focus_peaking_adjust_thr,
		.help = "Show tiny dots on focused edges. Params: method,thr,color."
	},
	{
		.priv = &zoom_overlay_pos,
		.display = zoom_overlay_display,
		.select = zoom_overlay_main_toggle,
		.select_reverse = zoom_overlay_size_toggle,
		.select_auto = menu_quinternary_toggle,
		.help = "Zoom box for focusing. Can be used while recording."
	},
	{
		.priv = &zoom_overlay_split,
		.display = split_display, 
		.select = menu_binary_toggle,
		.select_auto = split_zerocross_toggle,
		.help = "Magic Zoom will be split when image is out of focus. [Q]:ZC"
	},
	{
		.priv			= &clearscreen,
		.display		= clearscreen_display,
		.select			= menu_quaternary_toggle,
		.select_reverse	= menu_quaternary_toggle_reverse,
		.help = "Clear BMP overlay or turn display off."
	},
	/*{
		.priv			= &focus_graph,
		.display		= focus_graph_display,
		.select			= menu_binary_toggle,
	},*/
	//~ {
		//~ .display		= crop_off_display,
		//~ .select			= crop_off_toggle,
		//~ .select_reverse = crop_off_toggle_rev, 
	//~ },
	
	//~ {
		//~ .priv = "[debug] HDMI test", 
		//~ .display = menu_print, 
		//~ .select = hdmi_test_toggle,
	//~ }
	//~ {
		//~ .priv		= &edge_draw,
		//~ .select		= menu_binary_toggle,
		//~ .display	= edge_display,
	//~ },
	//~ {
		//~ .priv		= &waveform_draw,
		//~ .select		= menu_binary_toggle,
		//~ .display	= waveform_display,
	//~ },
};

struct menu_entry dbg_menus[] = {
	/*{
		.priv = "Card Benchmark",
		.select = card_benchmark_schedule,
		.display = menu_print,
	},
	{
		.priv = "Dump RAM",
		.display = menu_print, 
		.select = dump_vram,
	},*/
	{
		.priv		= &unified_loop,
		.select		= menu_ternary_toggle,
		.display	= unified_loop_display,
		.help = "Unique loop for zebra and FP. Used with HDMI and 720p."
	},
	/*{
		.priv		= &zebra_density,
		.select		= menu_ternary_toggle,
		.display	= zebra_mode_display,
	},
	{
		.priv		= &use_hd_vram,
		.select		= menu_binary_toggle,
		.display	= use_hd_vram_display,
	},
	{
		.priv = &focus_peaking_debug,
		.select = menu_binary_toggle, 
		.display = focus_debug_display,
	}*/
};

static struct menu_entry cfg_menus[] = {
	{
		.priv		= &disp_profiles_0,
		.select		= menu_quaternary_toggle,
		.select_reverse	= menu_quaternary_toggle_reverse,
		.display	= disp_profiles_0_display,
		.help = "Number of LiveV display presets. Switch them with ISO+DISP."
	},
};

PROP_HANDLER(PROP_LV_ACTION)
{
	zoom_overlay_countdown = 0;
	return prop_cleanup( token, property );
}

/*PROP_HANDLER(PROP_MVR_REC_START)
{
	if (buf[0] != 1) redraw();
	return prop_cleanup( token, property );
}*/

void 
cropmark_draw()
{
	ChangeColorPaletteLV(2);
	if (!get_global_draw()) return;
	if (transparent_overlay) show_overlay();
	if (cropmark_movieonly && shooting_mode != SHOOTMODE_MOVIE) return;
	reload_cropmark(crop_draw); // reloads only when changed
	clrscr_mirror();
	bmp_ov_loc_size_t os;
	calc_ov_loc_size(&os);
	bmp_draw_scaled_ex(cropmarks, os.bmp_of_x, os.bmp_of_y, os.bmp_ex_x, os.bmp_ex_y, bvram_mirror, 0);
}
static void
cropmark_redraw()
{
	cropmark_draw();
}

// those functions will do nothing if called multiple times (it's safe to do this)
// they might cause ERR80 if called while taking a picture

void wait_till_its_safe_to_mess_with_the_display()
{
	while (lens_info.job_state >= 10 || tft_status || recording == 1)
	{
		msleep(100);
	}
}

int _bmp_cleared = 0;
void bmp_on()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (_bmp_cleared) { call("MuteOff"); msleep(100); _bmp_cleared = 0;}
}
void bmp_on_force()
{
	_bmp_cleared = 1;
	bmp_on();
}
void bmp_off()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (!_bmp_cleared) { _bmp_cleared = 1; msleep(100); call("MuteOn");}
}
int bmp_is_on() { return !_bmp_cleared; }

int _lvimage_cleared = 0;
void lvimage_on()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (!_lvimage_cleared) call("MuteOffImage");
	_lvimage_cleared = 1;
}
void lvimage_off()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (_lvimage_cleared) call("MuteOnImage");
	_lvimage_cleared = 0;
}

int _display_is_off = 0;
void display_on()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (_display_is_off)
	{
		call("TurnOnDisplay");
		_display_is_off = 0;
	}
}
void display_on_force()
{
	_display_is_off = 1;
	display_on();
}
void display_off()
{
	wait_till_its_safe_to_mess_with_the_display();
	if (!_display_is_off)
	{
		call("TurnOffDisplay");
		_display_is_off = 1;
	}
}
void display_off_force()
{
	wait_till_its_safe_to_mess_with_the_display();
	_display_is_off = 0;
	display_off();
}
int display_is_on() { return !_display_is_off; }


void zoom_overlay_toggle()
{
	zoom_overlay = !zoom_overlay;
	if (!zoom_overlay)
	{
		zoom_overlay_countdown = 0;
		redraw();
	}
}

//~ void zoom_overlay_enable()
//~ {
	//~ zoom_overlay = 1;
//~ }

void zoom_overlay_disable()
{
	zoom_overlay = 0;
	zoom_overlay_countdown = 0;
}

void zoom_overlay_set_countdown(int x)
{
	zoom_overlay_countdown = x;
}

void yuvcpy_x2(uint32_t* dst, uint32_t* src, int num_pix)
{
	dst = (void*)((unsigned int)dst & 0xFFFFFFFC);
	src = (void*)((unsigned int)src & 0xFFFFFFFC);
	uint32_t* last_s = src + (num_pix>>1);
	for (; src < last_s; src++, dst += 2)
	{
		*(dst) = ((*src) & 0x00FFFFFF) | (((*src) & 0x0000FF00) << 16);
		*(dst+1) = ((*src) & 0xFFFF00FF) | (((*src) & 0xFF000000) >> 16);
	}
}

void draw_zoom_overlay()
{
	if (!lv_drawn()) return;
	if (!get_global_draw()) return;
	if (gui_menu_shown()) return;
	if (!bmp_is_on()) return;
	if (lv_dispsize != 1) return;
	//~ if (get_halfshutter_pressed() && clearscreen != 2) return;
	if (recording == 1) return;
	
	struct vram_info *	lv = get_yuv422_vram();
	struct vram_info *	hd = get_yuv422_hd_vram();

	if( !lv->vram )	return;
	if( !hd->vram )	return;

	uint16_t*		lvr = (uint16_t*) lv->vram;
	uint16_t*		hdr = (uint16_t*) hd->vram;
	
	if (!lvr) return;

	int hx0,hy0; 
	get_afframe_pos(hd->width, hd->height, &hx0, &hy0);
	
	int W = 240;
	int H = 240;
	
	switch(zoom_overlay_size)
	{
		case 0:
		case 3:
			W = 150;
			H = 150;
			break;
		case 1:
		case 4:
			W = 250;
			H = 200;
			break;
		case 2:
		case 5:
			W = 500;
			H = 350;
			break;
	}
	
	int x2 = (zoom_overlay_size > 2) ? 1 : 0;

	int x0,y0; 
	int xaf,yaf;
	get_afframe_pos(lv->width, lv->height, &xaf, &yaf);

	switch(zoom_overlay_pos)
	{
		case 0: // AFF
			x0 = xaf;
			y0 = yaf;
			break;
		case 1: // NW
			x0 = W/2 + 50;
			y0 = H/2 + 50;
			break;
		case 2: // NE
			x0 = 720 - W/2 - 50;
			y0 = H/2 + 50;
			break;
		case 3: // SE
			x0 = 720 - W/2 - 50;
			y0 = 480 - H/2 - 50;
			break;
		case 4: // SV
			x0 = W/2 + 50;
			y0 = 480 - H/2 - 50;
			break;
		default:
			return;
	}

	if (zoom_overlay_pos)
	{
		int w = W * lv->width / hd->width;
		int h = H * lv->width / hd->width;
		if (x2)
		{
			w >>= 1;
			h >>= 1;
		}
		memset(lvr + COERCE(xaf - (w>>1), 0, 720-w) + COERCE(yaf - (h>>1) - 1, 0, 480) * lv->width, 0,    w<<1);
		memset(lvr + COERCE(xaf - (w>>1), 0, 720-w) + COERCE(yaf - (h>>1) - 2, 0, 480) * lv->width, 0xFF, w<<1);
		memset(lvr + COERCE(xaf - (w>>1), 0, 720-w) + COERCE(yaf + (h>>1) + 1, 0, 480) * lv->width, 0xFF, w<<1);
		memset(lvr + COERCE(xaf - (w>>1), 0, 720-w) + COERCE(yaf + (h>>1) + 2, 0, 480) * lv->width, 0,    w<<1);
	}

	//~ draw_circle(x0,y0,45,COLOR_WHITE);
	int y;
	int x0c = COERCE(x0 - (W>>1), 0, 720-W);
	int y0c = COERCE(y0 - (H>>1), 0, 480-H);

	extern int focus_value;
	int rawoff = COERCE(80 - focus_value, 0, 100) >> 2;
	
	// reverse the sign of split when perfect focus is achieved
	static int rev = 0;
	static int poff = 0;
	if (rawoff != 0 && poff == 0) rev = !rev;
	poff = rawoff;
	if (!zoom_overlay_split_zerocross) rev = 0;

	if (x2)
	{
		uint16_t* d = lvr + x0c + (y0c + 2) * lv->width;
		uint16_t* s = hdr + (hy0 - (H>>2)) * hd->width + (hx0 - (W>>2));
		for (y = 2; y < H-2; y++)
		{
			int off = zoom_overlay_split ? (y < H/2 ? rawoff : -rawoff) : 0;
			if (rev) off = -off;
			yuvcpy_x2((uint32_t*)d, (uint32_t*)(s + off), W>>1);
			d += lv->width;
			if (y & 1) s += hd->width;
		}
	}
	else
	{
		uint16_t* d = lvr + x0c + (y0c + 2) * lv->width;
		uint16_t* s = hdr + (hy0 - (H>>1)) * hd->width + (hx0 - (W>>1));
		for (y = 2; y < H-2; y++)
		{
			int off = zoom_overlay_split ? (y < H/2 ? rawoff : -rawoff) : 0;
			if (rev) off = -off;
			memcpy(d, s + off, W<<1);
			d += lv->width;
			s += hd->width;
		}
	}
	memset(lvr + x0c + COERCE(0   + y0c, 0, 720) * lv->width, rawoff ? 0    : 0x80, W<<1);
	memset(lvr + x0c + COERCE(1   + y0c, 0, 720) * lv->width, rawoff ? 0xFF : 0x80, W<<1);
	memset(lvr + x0c + COERCE(H-1 + y0c, 0, 720) * lv->width, rawoff ? 0xFF : 0x80, W<<1);
	memset(lvr + x0c + COERCE(H   + y0c, 0, 720) * lv->width, rawoff ? 0    : 0x80, W<<1);	bmp_fill(0, x0c, y0c + 2, W, H - 4);
	//~ bmp_fill(rawoff ? COLOR_BLACK : COLOR_GREEN1, x0c, y0c, W, 1);
	//~ bmp_fill(rawoff ? COLOR_WHITE : COLOR_GREEN2, x0c+1, y0c, W, 1);
	//~ bmp_fill(rawoff ? COLOR_WHITE : COLOR_GREEN2, x0c, y0c + H - 1, W, 1);
	//~ bmp_fill(rawoff ? COLOR_BLACK : COLOR_GREEN1, x0c, y0c + H, W, 1);
}

//~ int zebra_paused = 0;
//~ void zebra_pause() { zebra_paused = 1; }
//~ void zebra_resume() { zebra_paused = 0; }

int liveview_display_idle()
{
	return
		lv_drawn() && 
		!gui_menu_shown() &&
		gui_state == GUISTATE_IDLE && 
		CURRENT_DIALOG_MAYBE <= 3 && 
		lv_dispsize == 1 &&
		lens_info.job_state < 10 &&
		!mirror_down &&
		!(clearscreen == 1 && get_halfshutter_pressed());
}
// when it's safe to draw zebras and other on-screen stuff
int zebra_should_run()
{
	return liveview_display_idle() && global_draw && bmp_is_on();
}

void zebra_sleep_when_tired()
{
	if (!zebra_should_run())
	{
		while (clearscreen == 1 && get_halfshutter_pressed()) msleep(100);
		if (zebra_should_run()) return;
		if (!gui_menu_shown()) ChangeColorPaletteLV(4);
		if (lv_drawn() && !gui_menu_shown()) redraw();
		while (!zebra_should_run()) msleep(100);
		ChangeColorPaletteLV(2);
		crop_set_dirty(40);
		//~ if (lv_drawn() && !gui_menu_shown()) redraw();
	}
	
	static int prev_recording = 0;
	if (prev_recording != recording)
	{
		msleep(3000);
		redraw();
		prev_recording = recording;
	}
}

void clear_this_message_not_available_in_movie_mode()
{
	static int fp = -1;
	int f = FLASH_BTN_MOVIE_MODE;
	if (f == fp) return; // clear the message only once
	fp = f;
	if (!f) return;
	
	bmp_fill(0, 0, 330, 720, 480-330);
	msleep(50);
	bmp_fill(0, 0, 330, 720, 480-330);
}

void draw_livev_for_playback()
{
	cropmark_redraw();

	if (spotmeter_draw)
		spotmeter_step();

	if (falsecolor_draw) 
	{
		draw_false_downsampled();
	}
	else
	{
		draw_zebra_and_focus(1,0);
	}

	if (hist_draw || waveform_draw)
	{
		struct vram_info * vram = get_yuv422_vram();
		hist_build(vram->vram, vram->width, vram->pitch);
	}
	
	if( hist_draw)
		hist_draw_image( hist_x, hist_y );
		
	if( waveform_draw)
		waveform_draw_image( 720 - WAVEFORM_WIDTH, 480 - WAVEFORM_HEIGHT - 50 );
}

/*
//this function is a mess... but seems to work
static void
zebra_task( void )
{
	DebugMsg( DM_MAGIC, 3, "Starting zebra_task");
	
	msleep(1000);
	
	find_cropmarks();
	int k;

	while(1)
	{
		k++;
		msleep(10); // safety msleep :)
		if (recording) msleep(100);
		
		if (lv_drawn() && disp_mode_change_request)
		{
			disp_mode_change_request = 0;
			do_disp_mode_change();
		}
		
		zebra_sleep_when_tired();
		
		if (get_global_draw())
		{
			draw_livev_stuff(k);
		}
	}
}


TASK_CREATE( "zebra_task", zebra_task, 0, 0x1f, 0x1000 ); */

void clearscreen_wakeup()
{
	clearscreen_countdown = clearscreen == 3 ? 50 : 20;
}

static void
clearscreen_task( void* unused )
{
	int k = 0;
	for (;;k++)
	{
clearscreen_loop:
		msleep(100);
		if (!lv_drawn()) continue;
		
/*		if (k % 10 == 0)
		{
			bmp_printf(FONT_MED, 50, 50, "%d fps ", fps_ticks);
			fps_ticks = 0;
		}*/

		if (k % 50 == 0 && !display_is_on())
			card_led_blink(2, 50, 50);

		// clear overlays on shutter halfpress
		if (clearscreen == 1 && get_halfshutter_pressed())
		{
			BMP_SEM( clrscr_mirror(); )
			int i;
			for (i = 0; i < (int)clearscreen_delay/10; i++)
			{
				msleep(10);
				if (!get_halfshutter_pressed() || dofpreview)
					goto clearscreen_loop;
			}
			BMP_SEM( bmp_off(); )
			while (get_halfshutter_pressed()) msleep(100);
			BMP_SEM( bmp_on(); )
			redraw();
		}
		else if (clearscreen == 2 || clearscreen == 3)  // always clear overlays, or turn off display
		{
			if (!get_halfshutter_pressed() && liveview_display_idle())
			{
				if (clearscreen_countdown)
					clearscreen_countdown--;
			}
			else
			{
				clearscreen_wakeup();
			}
			
			if (get_lcd_sensor_shortcuts() && display_sensor && DISPLAY_SENSOR_POWERED)
				clearscreen_wakeup();
			
			if (clearscreen_countdown == 1)
			{
				if (clearscreen == 3)
				{
					bmp_printf(FONT_LARGE, 10, 40, "DISPLAY OFF");
					msleep(1000);
					display_off_force();
				}
				bmp_off();
			}
			else if (clearscreen_countdown > 1)
			{
				display_on();
				bmp_on();
			}
		}
		else
		{
			display_on();
			bmp_on();
		}
	}
}

TASK_CREATE( "cls_task", clearscreen_task, 0, 0x1e, 0x1000 );

void redraw()
{
	wait_till_its_safe_to_mess_with_the_display();
	BMP_SEM(
		/*static int x;
		msleep(500);
		bmp_printf(FONT_MED, 50, 50, "redraw %d ", x++);
		msleep(500);*/
		RedrawDisplay();
		crop_set_dirty(20);
		afframe_set_dirty();
	)
}

/*
static void
redraw_task( void )
{
	while(1)
	{
		msleep(50);
		if (redraw_requested)
		{
			bmp_enabled = 0;
			msleep(50);
			RedrawDisplay();
			bmp_enabled = 1;

			afframe_set_dirty();
			redraw_requested = 0;
			msleep(200);
			redraw_requested = 0;
		}
	}
}

TASK_CREATE( "redraw_task", redraw_task, 0, 0x1e, 0x1000 );*/

void test_fps(int* x)
{
	int x0 = 0;
	int F = 0;
	//~ int f = 0;
	bmp_printf(FONT_SMALL, 10, 100, "testing %x...", x);
	while(F < 500)
	{
		if (x0 != *x)
		{
			x0 = *x;
			fps_ticks++;
		}
		F++;
		msleep(1);
	}
	bmp_printf(FONT_SMALL, 10, 100, "testing done.");
	return;
}


int should_draw_zoom_overlay()
{
	if (zebra_should_run() && get_global_draw() && zoom_overlay_mode && (zoom_overlay || zoom_overlay_countdown)) return 1;
	if (lv_drawn() && get_halfshutter_pressed() && get_global_draw() && zoom_overlay_mode && (zoom_overlay || zoom_overlay_countdown)) return 1;
	return 0;
}

void false_color_toggle()
{
	falsecolor_draw = !falsecolor_draw;
	if (falsecolor_draw) zoom_overlay_disable();
}

int transparent_overlay_flag = 0;
void schedule_transparent_overlay()
{
	transparent_overlay_flag = 1;
}

// Items which need a high FPS
// Magic Zoom, Focus Peaking, zebra*, spotmeter*, false color*
// * = not really high FPS, but still fluent
static void
livev_hipriority_task( void* unused )
{
	msleep(1000);
	find_cropmarks();

	int k = 0;
	for (;;k++)
	{
		msleep(10);
		
		while (is_mvr_buffer_almost_full())
			msleep(100);

		zebra_sleep_when_tired();

		static int dirty = 0;
		if (should_draw_zoom_overlay())
		{
			if (dirty) { clrscr_mirror(); dirty = 0; }
			BMP_SEM( draw_zoom_overlay(); )
		}
		else
		{
			dirty = 1;
			if (falsecolor_draw)
			{
				if (k % 4 == 0)
					BMP_SEM( draw_false_downsampled(); )
			}
			else
			{
				BMP_SEM( draw_zebra_and_focus(k % (recording ? 2 : 1) == 0, 1); )
			}
			msleep(20);
		}

		if (spotmeter_draw && k % 4 == 0)
			BMP_SEM( spotmeter_step(); )

		if (crop_dirty)
		{
			//~ bmp_printf(FONT_MED, 100, 100, "%d ", crop_dirty);
			crop_dirty--;
			if (!crop_dirty)
			{
				BMP_SEM( cropmark_redraw(); )
			}
		}

		if (zoom_overlay_countdown)
		{
			zoom_overlay_countdown--;
			crop_set_dirty(40);
		}
		
		if (LV_BOTTOM_BAR_DISPLAYED || get_halfshutter_pressed())
			crop_set_dirty(25);
		
		static int rec = 0;
		if (rec != recording)
		{
			if (recording == 2 || recording == 0)
			{
				msleep(1000);
				redraw();
			}
		}
		rec = recording;
	}
}

void loprio_sleep()
{
	msleep(10);
	while (is_mvr_buffer_almost_full()) msleep(100);
}

// Items which do not need a high FPS, but are CPU intensive
// histogram, waveform...
static void
livev_lopriority_task( void* unused )
{
	while(1)
	{
		if (transparent_overlay_flag)
		{
			transparent_overlay_from_play();
			transparent_overlay_flag = 0;
		}
		
		loprio_sleep();
		if (!zebra_should_run()) continue;
		/*if (should_draw_zoom_overlay())
		{
			draw_zebra_and_focus(1,0); // when magic zoom is active, zebra can work at low priority
		}*/

		loprio_sleep();

		if ((hist_draw || waveform_draw) && zebra_should_run())
		{
			struct vram_info * vram = get_yuv422_vram();
			hist_build(vram->vram, vram->width, vram->pitch);
		}

		loprio_sleep();
		
		if( hist_draw && zebra_should_run())
			BMP_SEM( hist_draw_image( hist_x, hist_y ); )

		loprio_sleep();
		
		if( waveform_draw && zebra_should_run())
			BMP_SEM( waveform_draw_image( 720 - WAVEFORM_WIDTH, 480 - WAVEFORM_HEIGHT - 50 ); )
	}
}

TASK_CREATE( "livev_hiprio_task", livev_hipriority_task, 0, 0x1a, 0x1000 );
TASK_CREATE( "livev_loprio_task", livev_lopriority_task, 0, 0x1f, 0x1000 );

int unused = 0;
unsigned int * disp_mode_params[] = {&crop_draw, &zebra_draw, &hist_draw, &waveform_draw, &falsecolor_draw, &spotmeter_draw, &clearscreen, &focus_peaking, &zoom_overlay_split, &global_draw, &zoom_overlay_mode, &transparent_overlay};
int disp_mode_bits[] =    {4,          2,           2,          2,              2,                2,               2,             2,             1,                   1,            2,                   2};

void update_disp_mode_bits_from_params()
{
	int i;
	int off = 0;
	uint32_t bits = 0;
	for (i = 0; i < COUNT(disp_mode_params); i++)
	{
		int b = disp_mode_bits[i];
		bits = bits | (((*(disp_mode_params[i])) & ((1 << b) - 1)) << off);
		off += b;
	}
	
	if (disp_mode == 1) disp_mode_a = bits;
	else if (disp_mode == 2) disp_mode_b = bits;
	else if (disp_mode == 3) disp_mode_c = bits;
	else disp_mode_x = bits;
	
	unused++;
	//~ bmp_printf(FONT_MED, 0, 50, "mode: %d", disp_mode);
	//~ bmp_printf(FONT_MED, 0, 50, "a=%8x b=%8x c=%8x x=%8x", disp_mode_a, disp_mode_b, disp_mode_c, disp_mode_x);
}

int update_disp_mode_params_from_bits()
{
	uint32_t bits = disp_mode == 1 ? disp_mode_a : 
	                disp_mode == 2 ? disp_mode_b :
	                disp_mode == 3 ? disp_mode_c : disp_mode_x;
	
	static uint32_t old_bits = 0xffffffff;
	if (bits == old_bits) return 0;
	old_bits = bits;
	
	int i;
	int off = 0;
	for (i = 0; i < COUNT(disp_mode_bits); i++)
	{
		int b = disp_mode_bits[i];
		*(disp_mode_params[i]) = (bits >> off) & ((1 << b) - 1);
		off += b;
	}
	
	bmp_on();
	return 1;
}

int get_disp_mode() { return disp_mode; }

int toggle_disp_mode()
{
	clearscreen_wakeup();
	disp_mode = mod(disp_mode + 1, disp_profiles_0 + 1);
	BMP_SEM( do_disp_mode_change(); )
	redraw();
	return disp_mode == 0;
}
void do_disp_mode_change()
{
	if (gui_menu_shown()) { update_disp_mode_params_from_bits(); return; }
	
	display_on();
	bmp_on();
	clrscr();
	bmp_printf(FONT_LARGE, 10, 40, "DISP %d", disp_mode);
	update_disp_mode_params_from_bits();
	msleep(500);
	//~ crop_dirty = 1;
}

void livev_playback_toggle()
{
	livev_playback = !livev_playback;
	if (livev_playback)
	{
		draw_livev_for_playback();
	}
	else
	{
		redraw();
	}
}
void livev_playback_reset()
{
	livev_playback = 0;
}



static void zebra_init_menus()
{
    menu_add( "LiveV", zebra_menus, COUNT(zebra_menus) );
    menu_add( "Debug", dbg_menus, COUNT(dbg_menus) );
    //~ menu_add( "Movie", movie_menus, COUNT(movie_menus) );
    menu_add( "Config", cfg_menus, COUNT(cfg_menus) );
}

INIT_FUNC(__FILE__, zebra_init_menus);




void make_overlay()
{
	bvram_mirror_init();
	clrscr();

	bmp_printf(FONT_MED, 0, 0, "Saving overlay...");

	struct vram_info * vram = get_yuv422_vram();
	uint8_t * const lvram = vram->vram;
	int lvpitch = YUV422_LV_PITCH;
	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	#define BMPPITCH 960

	int y;
	for (y = 0; y < vram->height; y++)
	{
		//~ int k;
		uint16_t * const v_row = (uint16_t*)( lvram + y * lvpitch );        // 1 pixel
		uint8_t * const b_row = (uint8_t*)( bvram + y * BMPPITCH);          // 1 pixel
		uint8_t * const m_row = (uint8_t*)( bvram_mirror + y * BMPPITCH);   // 1 pixel
		uint16_t* lvp; // that's a moving pointer through lv vram
		uint8_t* bp;  // through bmp vram
		uint8_t* mp;  //through bmp vram mirror
		for (lvp = v_row, bp = b_row, mp = m_row; lvp < v_row + 720 ; lvp++, bp++, mp++)
			if ((y + (int)bp) % 2)
				*bp = *mp = ((*lvp) * 41 >> 16) + 38;
	}
	FIO_RemoveFile("B:/overlay.dat");
	FILE* f = FIO_CreateFile("B:/overlay.dat");
	FIO_WriteFile( f, (const void *) UNCACHEABLE(bvram_mirror), BVRAM_MIRROR_SIZE);
	FIO_CloseFile(f);
	bmp_printf(FONT_MED, 0, 0, "Overlay saved.  ");

	msleep(1000);
}

void show_overlay()
{
	bvram_mirror_init();
	struct vram_info * vram = get_yuv422_vram();
	//~ uint8_t * const lvram = vram->vram;
	//~ int lvpitch = YUV422_LV_PITCH;
	uint8_t * const bvram = bmp_vram();
	if (!bvram) return;
	#define BMPPITCH 960
	
	clrscr();

	FILE* f = FIO_Open("B:/overlay.dat", O_RDONLY | O_SYNC);
	if (f == INVALID_PTR) return;
	FIO_ReadFile(f, UNCACHEABLE(bvram_mirror), BVRAM_MIRROR_SIZE );
	FIO_CloseFile(f);

	int y;
	for (y = 0; y < vram->height; y++)
	{
		//~ int k;
		//~ uint16_t * const v_row = (uint16_t*)( lvram + y * lvpitch );        // 1 pixel
		uint8_t * const b_row = (uint8_t*)( bvram + y * BMPPITCH);          // 1 pixel
		uint8_t * const m_row = (uint8_t*)( bvram_mirror + (y - (int)transparent_overlay_offy) * BMPPITCH);   // 1 pixel
		uint8_t* bp;  // through bmp vram
		uint8_t* mp;  //through bmp vram mirror
		if (y - (int)transparent_overlay_offy < 0 || y - (int)transparent_overlay_offy > 480) continue;
		//~ int offm = 0;
		//~ int offb = 0;
		//~ if (transparent_overlay == 2) offm = 720/2;
		//~ if (transparent_overlay == 3) offb = 720/2;
		for (bp = b_row, mp = m_row - (int)transparent_overlay_offx; bp < b_row + 720 ; bp++, mp++)
			if (((y + (int)bp) % 2) && mp > m_row && mp < m_row + 720)
				*bp = *mp;
	}
	
	bzero32(bvram_mirror, BVRAM_MIRROR_SIZE);
}

void transparent_overlay_from_play()
{
	if (!PLAY_MODE) { fake_simple_button(BGMT_PLAY); msleep(1000); }
	make_overlay();
	get_out_of_play_mode();
	msleep(500);
	if (!lv_drawn()) { force_liveview(); msleep(500); }
	msleep(1000);
	BMP_SEM( show_overlay(); )
	transparent_overlay = 1;
}
