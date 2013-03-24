/** \file
 * Lens focus and zoom related things
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


#include "dryos.h"
#include "lens.h"
#include "property.h"
#include "bmp.h"
#include "config.h"
#include "menu.h"
#include "math.h"
#include "version.h"

// for movie logging
static char* mvr_logfile_buffer = 0;
/* delay to be waited after mirror is locked */
CONFIG_INT("mlu.lens.delay", lens_mlu_delay, 7);

static void update_stuff();

//~ extern struct semaphore * bv_sem;
void bv_update_lensinfo();
void bv_auto_update();
static void lensinfo_set_aperture(int raw);
static void bv_expsim_shift();


static CONFIG_INT("movie.log", movie_log, 0);
#ifdef CONFIG_FULLFRAME
#define SENSORCROPFACTOR 10
#define crop_info 0
#elif defined(CONFIG_600D)
static PROP_INT(PROP_DIGITAL_ZOOM_RATIO, digital_zoom_ratio);
#define DIGITAL_ZOOM ((is_movie_mode() && video_mode_crop && video_mode_resolution == 0) ? digital_zoom_ratio : 100)
#define SENSORCROPFACTOR (16 * DIGITAL_ZOOM / 100)
CONFIG_INT("crop.info", crop_info, 0);
#else
#define SENSORCROPFACTOR 16
CONFIG_INT("crop.info", crop_info, 0);
#endif

//~ static struct semaphore * lens_sem;
static struct semaphore * focus_done_sem;
//~ static struct semaphore * job_sem;


struct lens_info lens_info = {
    .name        = "NO LENS NAME"
};


/** Compute the depth of field for the current lens parameters.
 *
 * This relies heavily on:
 *     http://en.wikipedia.org/wiki/Circle_of_confusion
 * The CoC value given there is 0.019 mm, but we need to scale things
 */
static void
calc_dof(
    struct lens_info * const info
)
{
    #ifdef CONFIG_FULLFRAME
    const uint64_t        coc = 29; // 1/1000 mm
    #else
    const uint64_t        coc = 19; // 1/1000 mm
    #endif
    const uint64_t        fd = info->focus_dist * 10; // into mm
    const uint64_t        fl = info->focal_len; // already in mm

    // If we have no aperture value then we can't compute any of this
    // Not all lenses report the focus distance
    if( fl == 0 || info->aperture == 0 )
    {
        info->dof_near        = 0;
        info->dof_far        = 0;
        info->hyperfocal    = 0;
        return;
    }

    const uint64_t        fl2 = fl * fl;

    // The aperture is scaled by 10 and the CoC by 1000,
    // so scale the focal len, too.  This results in a mm measurement
    const uint64_t H = ((1000 * fl2) / (info->aperture  * coc)) * 10;
    info->hyperfocal = H;

    // If we do not have the focus distance, then we can not compute
    // near and far parameters
    if( fd == 0 )
    {
        info->dof_near        = 0;
        info->dof_far        = 0;
        return;
    }

    // fd is in mm, H is in mm, but the product of H * fd can
    // exceed 2^32, so we scale it back down before processing
    info->dof_near = (H * fd) / ( H + fd ); // in mm
    if( fd >= H )
        info->dof_far = 1000 * 1000; // infinity
    else
    {
        info->dof_far = (H * fd) / ( H - fd ); // in mm
    }
}

/*
const char *
lens_format_dist(
    unsigned        mm
)
{
    static char dist[ 32 ];

    if( mm > 100000 ) // 100 m
        snprintf( dist, sizeof(dist),
            "%d.%1dm",
            mm / 1000,
            (mm % 1000) / 100
        );
    else
    if( mm > 10000 ) // 10 m
        snprintf( dist, sizeof(dist),
            "%2d.%02dm",
            mm / 1000,
            (mm % 1000) / 10
        );
    else
    if( mm >  1000 ) // 1 m
        snprintf( dist, sizeof(dist),
            "%1d.%03dm",
            mm / 1000,
            (mm % 1000)
        );
    else
        snprintf( dist, sizeof(dist),
            "%dcm",
            mm / 10
        );

    return dist;
}*/

const char * lens_format_dist( unsigned mm)
{
   static char dist[ 32 ];

   if( mm > 100000 ) // 100 m
   {
      snprintf( dist, sizeof(dist), "Inf.");
   }
   else if( mm > 10000 ) // 10 m
   {
      snprintf( dist, sizeof(dist), "%2dm", mm / 1000);
   }
   else    if( mm >  1000 ) // 1 m 
   {
      snprintf( dist, sizeof(dist), "%1d.%1dm", mm / 1000, (mm % 1000)/100 );
   }
   else
   {
      snprintf( dist, sizeof(dist),"%2dcm", mm / 10 );
   }

   return (dist);
} /* end of aj_lens_format_dist() */

void
update_lens_display(int top, int bottom)
{
    extern int menu_upside_down; // don't use double buffer in this mode
    int double_buffering = !menu_upside_down && !is_canon_bottom_bar_dirty();

    if (top) draw_ml_topbar(double_buffering, 1);
    if (bottom) draw_ml_bottombar(double_buffering, 1);
}

int should_draw_bottom_bar()
{
    if (gui_menu_shown()) return 1;
    if (!get_global_draw()) return 0;
    //~ if (EXT_MONITOR_CONNECTED) return 1;
    if (canon_gui_front_buffer_disabled()) return 1;
    if (is_canon_bottom_bar_dirty())
    {
        crop_set_dirty(5);
        afframe_set_dirty();
        return 0;
    }
    if (lv_disp_mode == 0) return 1;
    return 0;
}

int raw2shutter_ms(int raw_shutter)
{
    if (!raw_shutter) return 0;
    return (int) roundf(powf(2.0, (56.0f - raw_shutter)/8.0f) * 1000.0f);
}
int shutter_ms_to_raw(int shutter_ms)
{
    if (shutter_ms == 0) return 160;
    return (int) roundf(56.0f - log2f((float)shutter_ms / 1000.0f) * 8.0f);
}
int shutterf_to_raw(float shutterf)
{
    if (shutterf == 0) return 160;
    return (int) roundf(56.0f - log2f(shutterf) * 8.0f);
}

// this one attempts to round in the same way as with previous call
// !! NOT thread safe !!
// !! ONLY call it once per iteration !!
// for example:      [0.4 0.5 0.6 0.4 0.8 0.7 0.6 0.5 0.4 0.3 0.2 0.1 0.3 0.5] =>
// flick-free round: [  0   0   0   0   1   1   1   1   1   1   0   0   0   0] => 2 transitions
// normal rounding:  [  0   1   1   0   1   1   1   1   0   0   0   0   0   1] => 5 transitions 
int round_noflicker(float value)
{
    static float rounding_correction = 0;

    float roundedf = roundf(value + rounding_correction);
    
    // if previous rounded value was smaller than float value (like 0.4 => 0),
    // then the rounding threshold should be moved at 0.8 => round(x - 0.3)
    // otherwise, rounding threshold should be moved at 0.2 => round(x + 0.3)
    
    rounding_correction = (roundedf < value ? -0.3 : 0.3);
    
    return (int) roundedf;
}

/*
void round_noflicker_test()
{
    float values[] = {0.4, 0.5, 0.6, 0.4, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.3, 0.5, 1, 2, 3, 3.5, 3.49, 3.51, 3.49, 3.51};
    char msg[100] = "";
    for (int i = 0; i < COUNT(values); i++)
    {
        int r = round_noflicker(values[i]);
        STR_APPEND(msg, "%d", r);
    }
    NotifyBox(5000, msg);
}*/

// NOT thread safe, see above!
// useful for bulb ramping
int shutterf_to_raw_noflicker(float shutterf)
{
    if (shutterf == 0) return 160;
    return round_noflicker(56.0f - log2f(shutterf) * 8.0f);
}

float raw2shutterf(int raw_shutter)
{
    if (!raw_shutter) return 0.0;
    return powf(2.0, (56.0 - raw_shutter)/8.0);
}

int raw2iso(int raw_iso)
{
    int iso = (int) roundf(100.0f * powf(2.0f, (raw_iso - 72.0f)/8.0f));
    if (iso >= 100 && iso <= 6400)
        iso = values_iso[raw2index_iso(raw_iso)];
    else if (raw_iso == 123)
        iso = 8000;
    else if (raw_iso == 125)
        iso = 10000;
    else if (raw_iso == 131)
        iso = 16000;
    else if (raw_iso == 133)
        iso = 20000;
    else if (iso > 100000)
        iso = ((iso+500)/1000) * 1000;
    else if (iso > 10000)
        iso = ((iso+50)/100) * 100;
    else if (iso >= 70 && iso < 80)
        iso = ((iso+5)/10) * 10;
    else if (iso >= 15)
        iso = ((iso+2)/5) * 5;
    else
        iso = iso&~1;
    return iso;
}

//~ void shave_color_bar(int x0, int y0, int w, int h, int shaved_color);

static void double_buffering_start(int ytop, int height)
{
    #ifdef CONFIG_500D // err70
    return;
    #endif
    // use double buffering to avoid flicker
    bmp_vram(); // make sure parameters are up to date
    ytop = MIN(ytop, BMP_H_PLUS - height);
    memcpy(bmp_vram_idle() + BM(0,ytop), bmp_vram_real() + BM(0,ytop), height * BMPPITCH);
    bmp_draw_to_idle(1);
}

static void double_buffering_end(int ytop, int height)
{
    #ifdef CONFIG_500D // err70
    return;
    #endif
    // done drawing, copy image to main BMP buffer
    bmp_draw_to_idle(0);
    bmp_vram(); // make sure parameters are up to date
    ytop = MIN(ytop, BMP_H_PLUS - height);
    memcpy(bmp_vram_real() + BM(0,ytop), bmp_vram_idle() + BM(0,ytop), height * BMPPITCH);
    bzero32(bmp_vram_idle() + BM(0,ytop), height * BMPPITCH);
}

static void ml_bar_clear(int ytop, int height)
{
    uint8_t* B = bmp_vram();
    uint8_t* M = (uint8_t *)get_bvram_mirror();
    if (!B) return;
    if (!M) return;
    int menu = gui_menu_shown();
    int ymax = MIN(ytop + height, BMP_H_PLUS);
   
    for (int y = ytop; y < ymax; y++)
    {
        for (int x = BMP_W_MINUS; x < BMP_W_PLUS; x+=4)
        {
            uint32_t p = *(uint32_t*)&B[BM(x,y)];
            uint32_t m = *(uint32_t*)&M[BM(x,y)];
            uint32_t target = 0;
           
            if (recording && y < 100)
            {
                for(int byte_pos = 0; byte_pos < 4; byte_pos++)
                {
                    uint8_t val = (p >> (8*byte_pos));
                   
                    /* is that one red? */
                    if((val & 0xFF) == COLOR_RED)
                    {
                        /* mask out cropmark */
                        m &= ~(0x80<<(8*byte_pos));
                    }
                }
            }
           
            if (menu)
            {
                target = (COLOR_BLACK<<24) | (COLOR_BLACK<<16) | (COLOR_BLACK<<8) | (COLOR_BLACK<<0);
            }
            else
            {
                for(int byte_pos = 0; byte_pos < 4; byte_pos++)
                {
                    uint8_t val = (m >> (8*byte_pos));
                   
                    /* is that one to draw? */
                    if(val & 0x80)
                    {
                        val &= 0x7F;
                        target |= (val<<(8*byte_pos));
                    }
                }
            }
            *(uint32_t*)&B[BM(x,y)] = target;
        }
    }
}

int FAST get_ml_bottombar_pos()
{
    unsigned bottom = 480;
    int screen_layout = get_screen_layout();
    
    if (screen_layout == SCREENLAYOUT_3_2_or_4_3) bottom = os.y_max;
    else if (screen_layout == SCREENLAYOUT_16_9) bottom = os.y_max - os.off_169;
    else if (screen_layout == SCREENLAYOUT_16_10) bottom = os.y_max - os.off_1610;
    else if (screen_layout == SCREENLAYOUT_UNDER_3_2) bottom = MIN(os.y_max + 54, 480);
    else if (screen_layout == SCREENLAYOUT_UNDER_16_9) bottom = MIN(os.y_max - os.off_169 + 54, 480);

    if (gui_menu_shown())
        bottom = 480 + (hdmi_code == 5 ? 40 : 0); // force it at the bottom of menu

    return bottom - 35;
}
void draw_ml_bottombar(int double_buffering, int clear)
{
    //~ beep();
    if (!should_draw_bottom_bar()) return;
    
    struct lens_info *    info = &lens_info;

    int bg = COLOR_BLACK;
    if (is_movie_mode() || gui_menu_shown()) bg = COLOR_BLACK;
    
    int bar_height = 35;
    int ytop = get_ml_bottombar_pos();
    int bottom = ytop + bar_height;


    unsigned int x_origin = MAX(os.x0 + os.x_ex/2 - 360 + 50, 0);
    unsigned int y_origin = bottom - 30;
    unsigned text_font = SHADOW_FONT(FONT(FONT_LARGE, COLOR_WHITE, bg));

    // start drawing to mirror buffer to avoid flicker
    if (double_buffering)
        double_buffering_start(ytop, bar_height);

    if (clear)
    {
        ml_bar_clear(ytop, bar_height);
    }

    // mark the BV mode somehow
    if(CONTROL_BV)
    {
        bmp_fill(18, x_origin + 70, y_origin-1, 284, 32);
        //~ bmp_draw_rect(COLOR_RED, x_origin + 70, y_origin - 4, 284, 35);
        //~ bmp_draw_rect(COLOR_RED, x_origin + 71, y_origin - 3, 284-2, 35-2);
    }

        // MODE
            bmp_printf( FONT(text_font, canon_gui_front_buffer_disabled() ? COLOR_YELLOW : COLOR_WHITE, FONT_BG(text_font)), x_origin - 50, y_origin,
                "%s",
                is_movie_mode() ? "Mv" : 
                shooting_mode == SHOOTMODE_P ? "P " :
                shooting_mode == SHOOTMODE_M ? "M " :
                shooting_mode == SHOOTMODE_TV ? "Tv" :
                shooting_mode == SHOOTMODE_AV ? "Av" :
                shooting_mode == SHOOTMODE_CA ? "CA" :
                shooting_mode == SHOOTMODE_ADEP ? "AD" :
                shooting_mode == SHOOTMODE_AUTO ? "[]" :
                shooting_mode == SHOOTMODE_LANDSCAPE ? "LD" :
                shooting_mode == SHOOTMODE_PORTRAIT ? ":)" :
                shooting_mode == SHOOTMODE_NOFLASH ? "NF" :
                shooting_mode == SHOOTMODE_MACRO ? "MC" :
                shooting_mode == SHOOTMODE_SPORTS ? "SP" :
                shooting_mode == SHOOTMODE_NIGHT ? "NI" :
                shooting_mode == SHOOTMODE_BULB ? "B " :
                shooting_mode == SHOOTMODE_C ? "C " :
                shooting_mode == SHOOTMODE_C2 ? "C2" :
                shooting_mode == SHOOTMODE_C3 ? "C3" :
                "?"
            );

      /*******************
      * FOCAL & APERTURE *
      *******************/
      
      if (info->name[0]) // for chipped lenses only
      {
          text_font = FONT(SHADOW_FONT(FONT_LARGE),COLOR_WHITE,bg);
          unsigned med_font = FONT(SHADOW_FONT(FONT_MED),COLOR_WHITE,bg);

          static char focal[32];
          snprintf(focal, sizeof(focal), "%d",
                   crop_info ? (info->focal_len * SENSORCROPFACTOR + 5) / 10 : info->focal_len);

          //~ int IS_font = FONT(text_font, lens_info.IS ? COLOR_YELLOW : COLOR_WHITE, bg );
          int IS_font_med = FONT(med_font,
                    lens_info.IS == 0 ? COLOR_WHITE    :  // IS off
                    lens_info.IS == 4 ? COLOR_CYAN     :  // IS active, but not engaged
                    lens_info.IS == 8 ? COLOR_BLACK    :  // IS disabled on sigma lenses?
                    lens_info.IS == 0xC ? COLOR_YELLOW :  // IS starting?
                    lens_info.IS == 0xE ? COLOR_ORANGE :  // IS active and kicking
                    COLOR_RED,                            // unknown
                    bg
                );
          bmp_printf( text_font, x_origin - 5, y_origin, focal );

          if (info->aperture)
          {
                if (info->aperture < 100)
                {
                      bmp_printf( text_font, 
                                  x_origin + 74 + font_med.width + font_large.width - 7, 
                                  y_origin, 
                                  ".");
                      bmp_printf( text_font, 
                                  x_origin + 74 + font_med.width  , 
                                  y_origin, 
                                  "%d", info->aperture / 10);
                      bmp_printf( text_font, 
                                  x_origin + 74 + font_med.width + font_large.width * 2 - 14, 
                                  y_origin, 
                                  "%d", info->aperture % 10);
                }
                else
                      bmp_printf( text_font, 
                                  x_origin + 74 + font_med.width  , 
                                  y_origin, 
                                  "%d    ", info->aperture / 10) ;
          }

          bmp_printf( med_font, 
                      x_origin + font_large.width * strlen(focal) - 3 - 5, 
                      bottom - font_med.height, 
                      crop_info ? "eq" : "mm");

          if (lens_info.IS)
          bmp_printf( IS_font_med, 
                      x_origin + font_large.width * strlen(focal) - 3 - 5 + 1, 
                      y_origin - 3, 
                      "IS");

          bmp_printf( med_font, 
                      x_origin + 74 + 2  , 
                      y_origin - 2, 
                      "f") ;
      }
  
      /*******************
      *  SHUTTER         *
      *******************/


      int shutter_x10 = raw2shutter_ms(info->raw_shutter) / 100;
      int shutter_reciprocal = info->raw_shutter ? (int) roundf(4000.0f / powf(2.0f, (152 - info->raw_shutter)/8.0f)) : 0;

      // in movie mode we can get the exact value from Canon timers
      if (is_movie_mode()) 
      {
           int sr_x1000 = get_current_shutter_reciprocal_x1000();
           shutter_reciprocal = (sr_x1000+500)/1000;
           shutter_x10 = ((100000 / sr_x1000) + 5) / 10;
      }
      
      if (shutter_reciprocal > 100) shutter_reciprocal = 10 * ((shutter_reciprocal+5) / 10);
      if (shutter_reciprocal > 1000) shutter_reciprocal = 100 * ((shutter_reciprocal+50) / 100);
      static char shutter[32];
      if (is_bulb_mode()) snprintf(shutter, sizeof(shutter), "BULB");
      else if (info->raw_shutter == 0) snprintf(shutter, sizeof(shutter), "    ");
      else if (shutter_reciprocal >= 10000) snprintf(shutter, sizeof(shutter), "%dK ", shutter_reciprocal/1000);
      else if (shutter_x10 <= 3) snprintf(shutter, sizeof(shutter), "%d  ", shutter_reciprocal);
      else if (shutter_x10 % 10 && shutter_x10 < 30) snprintf(shutter, sizeof(shutter), "%d.%d\"", shutter_x10 / 10, shutter_x10 % 10);
      else snprintf(shutter, sizeof(shutter), "%d\" ", (shutter_x10+5) / 10);

      int fgs = COLOR_CYAN; // blue (neutral)
      int shutter_degrees = -1;
      if (is_movie_mode()) // check 180 degree rule
      {
           shutter_degrees = 360 * video_mode_fps / shutter_reciprocal;
           if (ABS(shutter_degrees - 180) < 10)
              fgs = FONT(FONT_LARGE,COLOR_GREEN1,bg);
           else if (shutter_degrees > 190)
              fgs = FONT(FONT_LARGE,COLOR_RED,bg);
           else if (shutter_degrees < 45)
              fgs = FONT(FONT_LARGE,COLOR_RED,bg);
      }
      else if (info->aperture && !is_bulb_mode()) // rule of thumb: shutter speed should be roughly equal to focal length
      {
           int focal_35mm = (info->focal_len * SENSORCROPFACTOR + 5) / 10;
           if (lens_info.IS) focal_35mm /= 4; // assume 2-stop effectiveness for IS
           if (shutter_reciprocal > focal_35mm * 15/10) 
              fgs = FONT(FONT_LARGE,COLOR_GREEN1,bg); // very good
           else if (shutter_reciprocal < focal_35mm / 2) 
              fgs = FONT(FONT_LARGE,COLOR_RED,bg); // you should have really steady hands
           else if (shutter_reciprocal < focal_35mm) 
              fgs = FONT(FONT_LARGE,COLOR_YELLOW,bg); // OK, but be careful
      }

    text_font = SHADOW_FONT(FONT(FONT_LARGE,fgs,bg));
    /*if (is_movie_mode() && shutter_display_degrees)
    {
        snprintf(shutter, sizeof(shutter), "%d  ", shutter_degrees);
        bmp_printf( text_font, 
                    x_origin + 143 + font_med.width*2  , 
                    y_origin, 
                    shutter);

        text_font = FONT(SHADOW_FONT(FONT_MED),fgs,bg);

        bmp_printf( text_font, 
                    x_origin + 143 + font_med.width*2 + (strlen(shutter) - 2) * font_large.width, 
                    y_origin, 
                    "o");
    }*/
 /*   else if (is_movie_mode() && is_hard_shutter_override_active())
    {
        int d = get_shutter_override_degrees_x10();
        int q = d/10;
        int r = d%10;
        int cr = r + '0';
        snprintf(shutter, sizeof(shutter), "%d%s%s  ", q, r ? "." : "", r ? (char*)&cr : "");
        bmp_printf( FONT(text_font,COLOR_ORANGE,bg), 
                    x_origin + 143 + font_med.width*2  , 
                    y_origin, 
                    shutter);

        text_font = FONT(SHADOW_FONT(FONT_MED),COLOR_ORANGE,bg);

        bmp_printf( text_font, 
                    x_origin + 143 + font_med.width*2 + (strlen(shutter) - 2) * font_large.width, 
                    y_origin, 
                    "o");
    }
    else*/
    {
        bmp_printf( text_font, 
                x_origin + (shutter_x10 <= 3 ? 143 : 123) + font_med.width*2  , 
                y_origin, 
                shutter);

        text_font = FONT(SHADOW_FONT(FONT_MED),fgs,bg);

        if (shutter_x10 <= 3 && !is_bulb_mode())
            bmp_printf( text_font, 
                x_origin + 143 + 1  , 
                y_origin - 2, 
                "1/");
    }

      /*******************
      *  ISO             *
      *******************/

      // good iso = 160 320 640 1250  - according to bloom video  
      //  http://www.youtube.com/watch?v=TNNqUm_nSXk&NR=1

      text_font = FONT(
      SHADOW_FONT(FONT_LARGE), 
      is_native_iso(lens_info.iso) ? COLOR_YELLOW :
      is_lowgain_iso(lens_info.iso) ? COLOR_GREEN2 : COLOR_RED,
      bg);
      
        if (hdr_video_enabled())
        {
            int iso_low, iso_high;
            hdr_get_iso_range(&iso_low, &iso_high);
            iso_low = raw2iso(get_effective_hdr_iso_for_display(iso_low));
            iso_high = raw2iso(get_effective_hdr_iso_for_display(iso_high));
            bmp_printf( FONT(FONT_MED, COLOR_WHITE, bg),
                      x_origin + 245  , 
                      y_origin + 5, 
                      "%4d/%d", iso_low, iso_high) ;
        }
        else if (info->iso)
        {

            text_font = FONT(
                SHADOW_FONT(FONT_LARGE),
                info->raw_iso > MAX_ANALOG_ISO ? COLOR_RED :
                info->iso_equiv_raw < info->raw_iso ? COLOR_GREEN1 :
                info->iso_equiv_raw > info->raw_iso ? COLOR_RED :
                info->iso_digital_ev < 0 ? COLOR_GREEN1 : info->iso_digital_ev > 0 ? COLOR_RED : COLOR_YELLOW,
                bg
            );
            char msg[10];
            int iso = raw2iso(info->iso_equiv_raw);
            snprintf(msg, sizeof(msg), "%d   ", iso >= 10000 ? iso/100 : iso);
            bmp_printf( text_font, 
                      x_origin + 250  , 
                      y_origin, 
                      msg);
            
            static char msg2[5];
            msg2[0] = '\0';
            if (CONTROL_BV && lens_info.iso_equiv_raw == lens_info.raw_iso) { STR_APPEND(msg2, "ov"); }
            else if (lens_info.iso_equiv_raw != lens_info.raw_iso) { STR_APPEND(msg2, "eq"); }
            if (get_htp()) { STR_APPEND(msg2, msg2[0] ? "+" : "D+"); }
            
            bmp_printf( FONT(SHADOW_FONT(FONT_MED), FONT_FG(text_font), bg), 
                      x_origin + 250 + font_large.width * (strlen(msg)-3) - 2, 
                      bottom - font_med.height, 
                      msg2
                      );
            if (iso >= 10000)
                bmp_printf( FONT(SHADOW_FONT(FONT_MED), FONT_FG(text_font), bg), 
                          x_origin + 250 + font_large.width * (strlen(msg)-3) - 2, 
                          y_origin - 2, 
                          "00");
        }
        else if (info->iso_auto)
            bmp_printf( text_font, 
                      x_origin + 250  , 
                      y_origin, 
                      "A%d   ", info->iso_auto);
        else
            bmp_printf( text_font, 
                      x_origin + 250  , 
                      y_origin, 
                      "Auto ");

      if (ISO_ADJUSTMENT_ACTIVE) goto end;
      
        // kelvins
      text_font = FONT(
      SHADOW_FONT(FONT_LARGE), 
      0x13, // orange
      bg);

        if( info->wb_mode == WB_KELVIN )
            bmp_printf( text_font, x_origin + 360 + (info->kelvin >= 10000 ? 0 : font_large.width), y_origin,
                info->kelvin >= 10000 ? "%5dK " : "%4dK ",
                info->kelvin
            );
        else
            bmp_printf( text_font, x_origin + 360, y_origin,
                "%s ",
                (uniwb_is_active()      ? " UniWB" :
                (lens_info.wb_mode == 0 ? "AutoWB" : 
                (lens_info.wb_mode == 1 ? " Sunny" :
                (lens_info.wb_mode == 2 ? "Cloudy" : 
                (lens_info.wb_mode == 3 ? "Tungst" : 
                (lens_info.wb_mode == 4 ? "Fluor." : 
                (lens_info.wb_mode == 5 ? " Flash" : 
                (lens_info.wb_mode == 6 ? "Custom" : 
                (lens_info.wb_mode == 8 ? " Shade" :
                 "unk")))))))))
            );
        
        int gm = lens_info.wbs_gm;
        int ba = lens_info.wbs_ba;
        if (gm) 
            bmp_printf(
                FONT(ba ? FONT_MED : FONT_LARGE, COLOR_WHITE, gm > 0 ? COLOR_GREEN2 : 14 /* magenta */),
                x_origin + 360 + font_large.width * 6, y_origin + (ba ? -3 : 0), 
                "%d", ABS(gm)
            );

        if (ba) 
            bmp_printf(
                FONT(gm ? FONT_MED : FONT_LARGE, COLOR_WHITE, ba > 0 ? 12 : COLOR_BLUE), 
                x_origin + 360 + font_large.width * 6, y_origin + (gm ? 14 : 0), 
                "%d", ABS(ba));


      /*******************
      *  Focus distance  *
      *******************/

      text_font = FONT(SHADOW_FONT(FONT_LARGE), is_manual_focus() ? COLOR_CYAN : COLOR_WHITE, bg );   // WHITE

      if(lens_info.focus_dist)
          bmp_printf( text_font, 
                  x_origin + 505  , 
                  y_origin, 
                  lens_format_dist( lens_info.focus_dist * 10 )
                );
        else
          bmp_printf( text_font, 
                  x_origin + 515  , 
                  y_origin, 
                  is_manual_focus() ? "MF" : "AF"
                );

      int ae = AE_VALUE;
      if (!ae) ae = lens_info.ae;
      if (ae)
      {
          text_font = FONT(SHADOW_FONT(FONT_LARGE), COLOR_CYAN, bg ); 

          bmp_printf( text_font, 
                      x_origin + 610 + font_large.width * 2 - 8, 
                      y_origin, 
                      ".");
          bmp_printf( text_font, 
                      x_origin + 610 - font_large.width, 
                      y_origin, 
                      " %s%d", 
                        ae < 0 ? "-" : ae > 0 ? "+" : " ",
                        ABS(ae) / 8
                      );
          bmp_printf( text_font, 
                      x_origin + 610 + font_large.width * 3 - 16, 
                      y_origin, 
                      "%d",
                        mod(ABS(ae) * 10 / 8, 10)
                      );
      }

        // battery indicator
        int xr = x_origin + 612 - font_large.width - 4;

    #ifdef CONFIG_BATTERY_INFO
        int bat = GetBatteryLevel();
    #else
        int bat = battery_level_bars == 0 ? 5 : battery_level_bars == 1 ? 30 : 100;
    #endif

        int col = battery_level_bars == 0 ? COLOR_RED :
                  battery_level_bars == 1 ? COLOR_YELLOW : 
                #ifdef CONFIG_BATTERY_INFO
                  bat <= 70 ? COLOR_WHITE : 
                #endif
                  COLOR_GREEN1;
        
        bat = bat * 22 / 100;
        bmp_draw_rect(COLOR_BLACK, xr+1, y_origin-4, 10, 4);
        bmp_draw_rect(COLOR_BLACK, xr-3, y_origin-1, 18, 31);
        //~ bmp_draw_rect(COLOR_BLACK, xr, y_origin+2, 11, 29);
        bmp_draw_rect(COLOR_BLACK, xr+1, y_origin + 24 - bat, 10, bat+2);
        bmp_fill(col, xr+2, y_origin-3, 8, 3);
        bmp_draw_rect(col, xr-2, y_origin, 16, 29);
        bmp_draw_rect(col, xr-1, y_origin + 1, 14, 27);
        bmp_fill(col, xr+2, y_origin + 25 - bat, 8, bat);

    //~ if (hdmi_code == 2) shave_color_bar(40,370,640,16,bg);
    //~ if (hdmi_code == 5) shave_color_bar(75,480,810,22,bg);
    
    // these have a black bar at the bottom => no problems
    //~ #if !defined(CONFIG_500D) && !defined(CONFIG_50D)
    //~ int y169 = os.y_max - os.off_169;

    //~ if (!gui_menu_shown() && (screen_layout == SCREENLAYOUT_16_9 || screen_layout == SCREENLAYOUT_16_10 || hdmi_code == 2 || ext_monitor_rca))
        //~ shave_color_bar(os.x0, ytop, os.x_ex, y169 - ytop + 1, bg);
    //~ #endif



end:

    if (double_buffering)
    {
        double_buffering_end(ytop, bar_height);
    }

    // this is not really part of the bottom bar, but it's close to it :)
    /*
    if (LVAE_DISP_GAIN)
    {
        text_font = FONT(FONT_LARGE, COLOR_WHITE, COLOR_BLACK ); 
        int gain_ev = gain_to_ev(LVAE_DISP_GAIN) - 10;
        bmp_printf( text_font, 
                  x_origin + 590, 
                  y_origin - font_large.height, 
                  "%s%dEV", 
                  gain_ev > 0 ? "+" : "-",
                  ABS(gain_ev));
    }*/
}
/*
void shave_color_bar(int x0, int y0, int w, int h, int shaved_color)
{
    // shave the bottom bar a bit :)
    int i,j;
    int new_color = bmp_getpixel_real(os.x0 + 123, y0-5);
    for (i = y0; i < y0 + h; i++)
    {
        //~ int new_color = 0;
        for (j = x0; j < x0+w; j++)
            if (bmp_getpixel(j,i) == shaved_color)
                bmp_putpixel(j,i,new_color);
        //~ bmp_putpixel(x0+5,i,COLOR_RED);
    }
}*/

int FAST get_ml_topbar_pos()
{
    int screen_layout = get_screen_layout();

    int y = 0;
    if (gui_menu_shown())
    {
        y = (hdmi_code == 5 ? 40 : 0); // force it at the top of menu
    }
    else
    {
        if (screen_layout == SCREENLAYOUT_3_2_or_4_3) y = os.y0; // just above the 16:9 frame
        else if (screen_layout == SCREENLAYOUT_16_9) y = os.y0 + os.off_169; // meters just below 16:9 border
        else if (screen_layout == SCREENLAYOUT_16_10) y = os.y0 + os.off_1610; // meters just below 16:9 border
        else if (screen_layout == SCREENLAYOUT_UNDER_3_2) y = MIN(os.y_max, 480 - 54);
        else if (screen_layout == SCREENLAYOUT_UNDER_16_9) y = MIN(os.y_max - os.off_169, 480 - 54);
    }
    return y;
}

void draw_ml_topbar(int double_buffering, int clear)
{
    if (!get_global_draw()) return;
    
    unsigned font    = FONT(SHADOW_FONT(FONT_MED), COLOR_WHITE, COLOR_BLACK);
    
    int x = MAX(os.x0 + os.x_ex/2 - 360, 0);
    int y = get_ml_topbar_pos();

    int screen_layout = get_screen_layout();
    if (screen_layout >= 3 && !should_draw_bottom_bar())
        return; // top bar drawn at bottom, may interfere with canon info

    // fixme: draw them right from the first try
    extern int time_indic_x, time_indic_y; // for bitrate indicators
    if (time_indic_x != os.x_max - 160 || time_indic_y != (int)y) redraw();
    time_indic_x = os.x_max - 160;
    time_indic_y = y;
    
    if (time_indic_y > BMP_H_PLUS - 30) time_indic_y = BMP_H_PLUS - 30;

    if (audio_meters_are_drawn() && !get_halfshutter_pressed()) return;
    
    if (double_buffering)
        double_buffering_start(y, 35);

    if (clear)
        ml_bar_clear(y, font_med.height+1);

    struct tm now;
    LoadCalendarFromRTC( &now );
    bmp_printf(font, x, y, "%02d:%02d", now.tm_hour, now.tm_min);

    x += 80;

    bmp_printf( font, x, y,
        "DISP%d", get_disp_mode()
    );

    x += 70;

    int raw = pic_quality & 0x60000;
    int jpg = pic_quality & 0x10000;
    int rawsize = pic_quality & 0xF;
    int jpegtype = pic_quality >> 24;
    int jpegsize = (pic_quality >> 8) & 0xFF;
    bmp_printf( font, x, y, "%s%s%s",
        rawsize == 1 ? "mRAW" : rawsize == 2 ? "sRAW" : rawsize == 7 ? "sRAW1" : rawsize == 8 ? "sRAW2" : raw ? "RAW" : "",
        jpg == 0 ? "" : (raw ? "+" : "JPG-"),
        jpg == 0 ? "" : (
            jpegsize == 0 ? (jpegtype == 3 ? "L" : "l") : 
            jpegsize == 1 ? (jpegtype == 3 ? "M" : "m") : 
            jpegsize == 2 ? (jpegtype == 3 ? "S" : "s") :
            jpegsize == 0x0e ? (jpegtype == 3 ? "S1" : "s1") :
            jpegsize == 0x0f ? (jpegtype == 3 ? "S2" : "s2") :
            jpegsize == 0x10 ? (jpegtype == 3 ? "S3" : "s3") :
            "err"
        )
    );

    x += 80;
    int alo = get_alo();
    bmp_printf( font, x, y,
        get_htp() ? "HTP" :
        alo == ALO_LOW ? "alo" :
        alo == ALO_STD ? "Alo" :
        alo == ALO_HIGH ? "ALO" : "   "
    );

    x += 45;
    #ifdef FEATURE_PICSTYLE
    bmp_printf( font, x, y, (char*)get_picstyle_shortname(lens_info.raw_picstyle));
    #endif

    x += 70;
    #ifdef CONFIG_BATTERY_INFO
        bmp_printf( font, x, y,"T=%dC BAT=%d", EFIC_CELSIUS, GetBatteryLevel());
    #else
        bmp_printf( font, x, y,"T=%dC", EFIC_CELSIUS);
    #endif

    x += 160;
    bmp_printf( font, x, y,
        is_movie_mode() ? "MVI-%04d" : "[%d]",
        is_movie_mode() ? file_number : avail_shot
    );

    free_space_show(); 
    fps_show();

    if (double_buffering)
        double_buffering_end(y, 35);
}

static volatile int lv_focus_done = 1;
static volatile int lv_focus_error = 0;

PROP_HANDLER( PROP_LV_FOCUS_DONE )
{
    lv_focus_done = 1;
    if (buf[0] & 0x1000) 
    {
        NotifyBox(1000, "Focus: soft limit reached");
        lv_focus_error = 1;
    }
}

static void
lens_focus_wait( void )
{
    for (int i = 0; i < 100; i++)
    {
        msleep(10);
        if (lv_focus_done) return;
        if (!lv) return;
        if (is_manual_focus()) return;
    }
    NotifyBox(1000, "Focus error :(");
    lv_focus_error = 1;
    //~ NotifyBox(1000, "Press PLAY twice or reboot");
}

// this is compatible with all cameras so far, but allows only 3 speeds
int
lens_focus(
    int num_steps, 
    int stepsize, 
    int wait,
    int extra_delay
)
{
    if (!lv) return 0;
    if (is_manual_focus()) return 0;
    if (lens_info.job_state) return 0;

    if (num_steps < 0)
    {
        num_steps = -num_steps;
        stepsize = -stepsize;
    }

    stepsize = COERCE(stepsize, -3, 3);
    int focus_cmd = stepsize;
    if (stepsize < 0) focus_cmd = 0x8000 - stepsize;
    
    for (int i = 0; i < num_steps; i++)
    {
        lv_focus_done = 0;
        info_led_on();
        if (lv && !mirror_down && DISPLAY_IS_ON && lens_info.job_state == 0)
            prop_request_change(PROP_LV_LENS_DRIVE_REMOTE, &focus_cmd, 4);
        if (wait)
        {
            lens_focus_wait(); // this will sleep at least 10ms
            if (extra_delay > 10) msleep(extra_delay - 10); 
        }
        else
        {
            msleep(extra_delay);
        }
        info_led_off();
    }

    #ifdef FEATURE_MAGIC_ZOOM
    if (get_zoom_overlay_trigger_by_focus_ring()) zoom_overlay_set_countdown(300);
    #endif

    idle_wakeup_reset_counters(-10);
    lens_display_set_dirty();
    
    if (lv_focus_error) { msleep(200); lv_focus_error = 0; return 0; }
    return 1;
}

static PROP_INT(PROP_ICU_UILOCK, uilock);

void lens_wait_readytotakepic(int wait)
{
    int i;
    for (i = 0; i < wait * 20; i++)
    {
        if (ml_shutdown_requested) return;
        if (sensor_cleaning) { msleep(50); continue; }
        if (job_state_ready_to_take_pic() && burst_count > 0 && ((uilock & 0xFF) == 0)) break;
        msleep(50);
        if (!recording) info_led_on();
    }
    if (!recording) info_led_off();
}

static int mirror_locked = 0;
int mlu_lock_mirror_if_needed() // called by lens_take_picture; returns 0 if success, 1 if camera took a picture instead of locking mirror
{
    #ifdef CONFIG_5DC
    if (get_mlu()) set_mlu(0); // can't trigger shutter with MLU active, so just turn it off
    return 0;
    #endif
    
    if (drive_mode == DRIVE_SELFTIMER_2SEC || drive_mode == DRIVE_SELFTIMER_REMOTE || drive_mode == DRIVE_SELFTIMER_CONTINUOUS)
        return 0;
    
    if (get_mlu() && CURRENT_DIALOG_MAYBE)
    {
        SetGUIRequestMode(0);
        int iter = 20;
        while (iter-- && !display_idle())
            msleep(50); 
        msleep(500);
    }

    //~ NotifyBox(1000, "MLU locking");
    if (get_mlu() && !lv)
    {
        if (!mirror_locked)
        {
            int fn = file_number;
            
            #if defined(CONFIG_5D2) || defined(CONFIG_50D)
            SW1(1,50);
            SW2(1,250);
            SW2(0,50);
            SW1(0,50);
            #elif defined(CONFIG_40D)
            call("FA_Release");
            #else
            call("Release");
            #endif
            
            msleep(500);
            if (file_number != fn) // Heh... camera took a picture instead. Cool.
                return 1;

            if (lv) // we have somehow got into LiveView, where MLU does nothing... so, no need to wait
                return 0;

            mirror_locked = 1;
            
            msleep(MAX(0, get_mlu_delay(lens_mlu_delay) - 500));
        }
    }
    //~ NotifyBox(1000, "MLU locked");
    return 0;
}

#define AF_BUTTON_NOT_MODIFIED 100
static int orig_af_button_assignment = AF_BUTTON_NOT_MODIFIED;

// to preview AF patterns
void assign_af_button_to_halfshutter()
{
    if (ml_shutdown_requested) return;
    if (orig_af_button_assignment == AF_BTN_HALFSHUTTER) return;
    //~ take_semaphore(lens_sem, 0);
    lens_wait_readytotakepic(64);
    if (ml_shutdown_requested) return;
    if (orig_af_button_assignment == AF_BUTTON_NOT_MODIFIED) orig_af_button_assignment = cfn_get_af_button_assignment();
    cfn_set_af_button(AF_BTN_HALFSHUTTER);
    //~ give_semaphore(lens_sem);
}

// to prevent AF
void assign_af_button_to_star_button()
{
    if (ml_shutdown_requested) return;
    if (orig_af_button_assignment == AF_BTN_STAR) return;
    //~ take_semaphore(lens_sem, 0);
    lens_wait_readytotakepic(64);
    if (ml_shutdown_requested) return;
    if (orig_af_button_assignment == AF_BUTTON_NOT_MODIFIED) orig_af_button_assignment = cfn_get_af_button_assignment();
    cfn_set_af_button(AF_BTN_STAR);
    //~ give_semaphore(lens_sem);
}

void restore_af_button_assignment()
{
    if (orig_af_button_assignment != AF_BUTTON_NOT_MODIFIED)
        orig_af_button_assignment = COERCE(orig_af_button_assignment, 0, 10); // just in case, so we don't read invalid values from config file
    
    if (orig_af_button_assignment == AF_BUTTON_NOT_MODIFIED) return;
    //~ take_semaphore(lens_sem, 0);
    lens_wait_readytotakepic(64);
    cfn_set_af_button(orig_af_button_assignment);
    msleep(100);
    if (cfn_get_af_button_assignment() == (int)orig_af_button_assignment)
        orig_af_button_assignment = AF_BUTTON_NOT_MODIFIED; // success
    //~ give_semaphore(lens_sem);
}

// keep retrying until it succeeds, or until the 3-second timeout expires
void restore_af_button_assignment_at_shutdown()
{
    for (int i = 0; i < 30; i++)
    {
        if (orig_af_button_assignment == AF_BUTTON_NOT_MODIFIED)
            break;
        restore_af_button_assignment();
        info_led_blink(1,50,50);
    }
}

int ml_taking_pic = 0;

int lens_setup_af(int should_af)
{
    ASSERT(should_af != AF_DONT_CHANGE);
    
    if (!is_manual_focus())
    {
        if (should_af == AF_ENABLE) assign_af_button_to_halfshutter();
        else if (should_af == AF_DISABLE) assign_af_button_to_star_button();
        else return 0;
        
        return 1;
    }
    return 0;
}
void lens_cleanup_af()
{
    restore_af_button_assignment();
}

int
lens_take_picture(
    int wait, 
    int should_af
)
{
    if (ml_taking_pic) return -1;
    ml_taking_pic = 1;

    if (should_af != AF_DONT_CHANGE) lens_setup_af(should_af);
    //~ take_semaphore(lens_sem, 0);
    lens_wait_readytotakepic(64);
    
    // in some cases, the MLU setting is ignored; if ML can't detect this properly, this call will actually take a picture
    // if it happens (e.g. with LV active, but camera in QR mode), that's it, we won't try taking another one
    // side effects should be minimal
    int took_pic = mlu_lock_mirror_if_needed();
    if (took_pic) goto end;

    #if defined(CONFIG_5D2) || defined(CONFIG_50D)
    if (get_mlu())
    {
        SW1(1,50);
        SW2(1,250);
        SW2(0,50);
        SW1(0,50);
    }
    else
    {
        #ifdef CONFIG_5D2
        int status = 0;
        PtpDps_remote_release_SW1_SW2_worker(&status);
        #else
        call("Release");
        #endif
    }
    #elif defined(CONFIG_5DC)
    call("rssRelease");
    #elif defined(CONFIG_40D)
    call("FA_Release");
    #else
    call("Release");
    #endif
    
    #if defined(CONFIG_7D)
    /* on EOS 7D the code to trigger SW1/SW2 is buggy that the metering somehow locks up when exposure time is >1.x seconds.
     * This causes the camera not to shut down when the card door is opened.
     * There is a workaround: Just wait until shooting is possible again and then trigger SW1 for a short time.
     * Then the camera will shut down clean.
     */
    lens_wait_readytotakepic(64);
    SW1(1,50);
    SW1(0,50);
    SW1(1,50);
    SW1(0,50);
    #endif

end:
    if( !wait )
    {
        //~ give_semaphore(lens_sem);
        if (should_af != AF_DONT_CHANGE) lens_cleanup_af();
        ml_taking_pic = 0;
        return 0;
    }
    else
    {
        msleep(200);

        if (drive_mode == DRIVE_SELFTIMER_2SEC) msleep(2000);
        if (drive_mode == DRIVE_SELFTIMER_REMOTE || drive_mode == DRIVE_SELFTIMER_CONTINUOUS) msleep(10000);

        lens_wait_readytotakepic(wait);
        //~ give_semaphore(lens_sem);
        if (should_af != AF_DONT_CHANGE) lens_cleanup_af();
        ml_taking_pic = 0;
        return lens_info.job_state;
    }
}

#ifdef FEATURE_MOVIE_LOGGING

/** Write the current lens info into the logfile */
static void
mvr_update_logfile(
    struct lens_info *    info,
    int            force
)
{
    if( mvr_logfile_buffer == 0 )
        return;

    static unsigned last_iso;
    static unsigned last_shutter;
    static unsigned last_aperture;
    static unsigned last_focal_len;
    static unsigned last_focus_dist;
    static int last_second;

    // Check if nothing changed and not forced.  Do not write.
    if( !force
    &&  last_iso        == info->iso
    &&  last_shutter    == info->shutter
    &&  last_aperture    == info->aperture
    &&  last_focal_len    == info->focal_len
    &&  last_focus_dist    == info->focus_dist
    )
        return;
    
    // Don't update more often than once per second
    if (!force
    && last_second == get_seconds_clock()
    )
        return;

    // Record the last settings so that we know if anything changes
    last_iso    = info->iso;
    last_shutter    = info->shutter;
    last_aperture    = info->aperture;
    last_focal_len    = info->focal_len;
    last_focus_dist    = info->focus_dist;
    last_second = get_seconds_clock();

    struct tm now;
    LoadCalendarFromRTC( &now );

    MVR_LOG_APPEND (
        "%02d:%02d:%02d,%d,%d,%d.%d,%d,%d\n",
        now.tm_hour,
        now.tm_min,
        now.tm_sec,
        info->iso,
        info->shutter,
        info->aperture / 10,
        info->aperture % 10,
        info->focal_len,
        info->focus_dist
    );
}

/** Create a logfile for each movie.
 * Record a logfile with the lens info for each movie.
 */
static void
mvr_create_logfile(
    unsigned        event
)
{
    if (!movie_log) return;

    if( event == 0 )
    {
        // Movie stopped - write the log file
        char name[100];
        snprintf(name, sizeof(name), "%s/MVI_%04d.LOG", get_dcim_dir(), file_number);

        FILE * mvr_logfile = mvr_logfile = FIO_CreateFileEx( name );
        if( mvr_logfile == INVALID_PTR )
        {
            bmp_printf( FONT_MED, 0, 40,
                "Unable to create movie log! fd=%x\n%s",
                (unsigned) mvr_logfile,
                name
            );
            return;
        }

        FIO_WriteFile( mvr_logfile, mvr_logfile_buffer, strlen(mvr_logfile_buffer) );

        FIO_CloseFile( mvr_logfile );
        
        free_dma_memory(mvr_logfile_buffer);
        mvr_logfile_buffer = 0;
        return;
    }

    if( event != 2 )
        return;

    // Movie starting
    mvr_logfile_buffer = alloc_dma_memory(MVR_LOG_BUF_SIZE);

    snprintf( mvr_logfile_buffer, MVR_LOG_BUF_SIZE,
        "# Magic Lantern %s\n\n",
        build_version
    );

    struct tm now;
    LoadCalendarFromRTC( &now );

    MVR_LOG_APPEND (
        "Start          : %4d/%02d/%02d %02d:%02d:%02d\n",
        now.tm_year + 1900,
        now.tm_mon + 1,
        now.tm_mday,
        now.tm_hour,
        now.tm_min,
        now.tm_sec
    );

    MVR_LOG_APPEND (
        "Lens name      : %s\n", lens_info.name 
    );

    int sr_x1000 = get_current_shutter_reciprocal_x1000();

    MVR_LOG_APPEND (
        "ISO            : %d%s\n"
        "Shutter        : 1/%d.%03ds\n"
        "Aperture       : f/%d.%d\n"
        "Focal length   : %d mm\n"
        "Focus distance : %d mm\n",
        lens_info.iso, get_htp() ? " D+" : "",
        sr_x1000/1000, sr_x1000%1000,
        lens_info.aperture / 10, lens_info.aperture % 10,
        lens_info.focal_len,
        lens_info.focus_dist * 10
    );

    MVR_LOG_APPEND (
        "White Balance  : %d%s, %s %d, %s %d\n",
        lens_info.wb_mode == WB_KELVIN ? lens_info.kelvin : lens_info.wb_mode,
        lens_info.wb_mode == WB_KELVIN ? "K" : 
        lens_info.wb_mode == 0 ? " - Auto" : 
        lens_info.wb_mode == 1 ? " - Sunny" :
        lens_info.wb_mode == 2 ? " - Cloudy" : 
        lens_info.wb_mode == 3 ? " - Tungsten" : 
        lens_info.wb_mode == 4 ? " - Fluorescent" : 
        lens_info.wb_mode == 5 ? " - Flash" : 
        lens_info.wb_mode == 6 ? " - Custom" : 
        lens_info.wb_mode == 8 ? " - Shade" : " - unknown",
        lens_info.wbs_gm > 0 ? "Green" : "Magenta", ABS(lens_info.wbs_gm), 
        lens_info.wbs_ba > 0 ? "Amber" : "Blue", ABS(lens_info.wbs_ba)
        );

    #ifdef FEATURE_PICSTYLE
    MVR_LOG_APPEND (
        "Picture Style  : %s (%d,%d,%d,%d)\n", 
        get_picstyle_name(lens_info.raw_picstyle), 
        lens_get_sharpness(),
        lens_get_contrast(),
        ABS(lens_get_saturation()) < 10 ? lens_get_saturation() : 0,
        ABS(lens_get_color_tone()) < 10 ? lens_get_color_tone() : 0
        );
    #endif

    fps_mvr_log(mvr_logfile_buffer);
    hdr_mvr_log(mvr_logfile_buffer);
    bitrate_mvr_log(mvr_logfile_buffer);
    
    MVR_LOG_APPEND (
        "\n\nCSV data:\n%s\n",
        "Time,ISO,Shutter,Aperture,Focal_Len,Focus_Dist"
    );

    // Force the initial values to be written
    mvr_update_logfile( &lens_info, 1 );
}
#endif

static inline uint16_t
bswap16(
    uint16_t        val
)
{
    return ((val << 8) & 0xFF00) | ((val >> 8) & 0x00FF);
}

PROP_HANDLER( PROP_MVR_REC_START )
{
    mvr_rec_start_shoot(buf[0]);
    
    #ifdef FEATURE_MOVIE_LOGGING
    mvr_create_logfile( *(unsigned*) buf );
    #endif
}


PROP_HANDLER( PROP_LENS_NAME )
{
    if( len > sizeof(lens_info.name) )
        len = sizeof(lens_info.name);
    memcpy( (char*)lens_info.name, buf, len );
}

PROP_HANDLER(PROP_LENS)
{
    uint8_t* info = (uint8_t *) buf;
    #ifdef CONFIG_5DC
    lens_info.raw_aperture_min = info[2];
    lens_info.raw_aperture_max = info[3];
    #else
    lens_info.raw_aperture_min = info[1];
    lens_info.raw_aperture_max = info[2];
    #endif
    
    if (lens_info.raw_aperture < lens_info.raw_aperture_min || lens_info.raw_aperture > lens_info.raw_aperture_max)
    {
        int raw = COERCE(lens_info.raw_aperture, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        lensinfo_set_aperture(raw); // valid limits changed
    }
    
    //~ bv_update_lensinfo();
}

PROP_HANDLER(PROP_LV_LENS_STABILIZE)
{
    //~ NotifyBox(2000, "%x ", buf[0]);
    lens_info.IS = (buf[0] & 0x000F0000) >> 16; // not sure, but lower word seems to be AF/MF status
}

// it may be slow; if you need faster speed, replace this with a binary search or something better
#define RAWVAL_FUNC(param) \
int raw2index_##param(int raw) \
{ \
    int i; \
    for (i = 0; i < COUNT(codes_##param); i++) \
        if(codes_##param[i] >= raw) return i; \
    return 0; \
}\
\
int val2raw_##param(int val) \
{ \
    unsigned i; \
    for (i = 0; i < COUNT(codes_##param); i++) \
        if(values_##param[i] >= val) return codes_##param[i]; \
    return -1; \
}

RAWVAL_FUNC(iso)
RAWVAL_FUNC(shutter)
RAWVAL_FUNC(aperture)

#define RAW2VALUE(param,rawvalue) ((int)values_##param[raw2index_##param(rawvalue)])
#define VALUE2RAW(param,value) ((int)val2raw_##param(value))

static void lensinfo_set_iso(int raw)
{
    lens_info.raw_iso = raw;
    lens_info.iso = RAW2VALUE(iso, raw);
    update_stuff();
}

static void lensinfo_set_shutter(int raw)
{
    //~ bmp_printf(FONT_MED, 600, 100, "liss %d %d ", raw, caller);
    lens_info.raw_shutter = raw;
    lens_info.shutter = RAW2VALUE(shutter, raw);
    update_stuff();
}

static void lensinfo_set_aperture(int raw)
{
    if (raw)
    {
        if (lens_info.raw_aperture_min && lens_info.raw_aperture_max)
            raw = COERCE(raw, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        lens_info.raw_aperture = raw;
        lens_info.aperture = RAW2VALUE(aperture, raw);
    }
    else
    {
        lens_info.aperture = lens_info.raw_aperture = 0;
    }
    //~ BMP_LOCK( lens_info.aperture = (int)roundf(10.0 * sqrtf(powf(2.0, (raw-8.0)/8.0))); )
    update_stuff();
}

extern int bv_auto;
static CONFIG_INT("bv.needed.by.iso", bv_auto_needed_by_iso, 0);
static CONFIG_INT("bv.needed.by.tv", bv_auto_needed_by_shutter, 0);
static CONFIG_INT("bv.needed.by.av", bv_auto_needed_by_aperture, 0);

static int iso_ack = -1;
PROP_HANDLER( PROP_ISO )
{
    if (!CONTROL_BV) lensinfo_set_iso(buf[0]);
    #ifdef FEATURE_EXPO_OVERRIDE
    else if (buf[0] && !gui_menu_shown() && ISO_ADJUSTMENT_ACTIVE
        #ifdef CONFIG_500D
        && !is_movie_mode()
        #endif
    )
    {
        bv_set_rawiso(buf[0]);
        bv_auto_needed_by_iso = 0;
    }
    bv_auto_update();
    #endif
    lens_display_set_dirty();
    iso_ack = buf[0];
}

void iso_auto_restore_hack()
{
    if (iso_ack == 0) lensinfo_set_iso(0);
}

PROP_HANDLER( PROP_ISO_AUTO )
{
    uint32_t raw = *(uint32_t *) buf;

    #if defined(FRAME_ISO) && !defined(CONFIG_500D) // FRAME_ISO not known
    if (lv && is_movie_mode()) raw = (uint8_t)FRAME_ISO;
    #endif

    lens_info.raw_iso_auto = raw;
    lens_info.iso_auto = RAW2VALUE(iso, raw);

    update_stuff();
}

#if defined(FRAME_ISO) && !defined(CONFIG_500D) // FRAME_ISO not known
PROP_HANDLER( PROP_BV ) // camera-specific
{
    if (lv && is_movie_mode())
    {
        uint32_t raw_iso = (uint8_t)FRAME_ISO;

        if (raw_iso)
        {
            lens_info.raw_iso_auto = raw_iso;
            lens_info.iso_auto = RAW2VALUE(iso, raw_iso);
            update_stuff();
        }
    }
}
#endif

static int shutter_was_set_from_ml = 0;
static int shutter_ack = -1;
PROP_HANDLER( PROP_SHUTTER )
{
    if (!CONTROL_BV) 
    {
        if (shooting_mode != SHOOTMODE_AV && shooting_mode != SHOOTMODE_P)
            lensinfo_set_shutter(buf[0]);
    }
    #ifdef FEATURE_EXPO_OVERRIDE
    else if (buf[0]  // sync expo override to Canon values
            && (!shutter_was_set_from_ml || ABS(buf[0] - lens_info.raw_shutter) > 3) // some cameras may attempt to round shutter value to 1/2 or 1/3 stops
                                                       // especially when pressing half-shutter

        #ifdef CONFIG_500D
        && !is_movie_mode()
        #endif

        )
    {
        bv_set_rawshutter(buf[0]);
        bv_auto_needed_by_shutter = 0;
        shutter_was_set_from_ml = 0;
    }
    bv_auto_update();
    #endif
    lens_display_set_dirty();
    shutter_ack = buf[0];
}

static int aperture_ack = -1;
PROP_HANDLER( PROP_APERTURE2 )
{
    //~ NotifyBox(2000, "%x %x %x %x ", buf[0], CONTROL_BV, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
    if (!CONTROL_BV) lensinfo_set_aperture(buf[0]);
    #ifdef FEATURE_EXPO_OVERRIDE
    else if (buf[0] && !gui_menu_shown()
        #ifdef CONFIG_500D
        && !is_movie_mode()
        #endif
    )
    {
        bv_set_rawaperture(COERCE(buf[0], lens_info.raw_aperture_min, lens_info.raw_aperture_max));
        bv_auto_needed_by_aperture = 0;
    }
    bv_auto_update();
    #endif
    lens_display_set_dirty();
    aperture_ack = buf[0];
}

PROP_HANDLER( PROP_APERTURE ) // for Tv mode
{
    if (!CONTROL_BV) lensinfo_set_aperture(buf[0]);
    lens_display_set_dirty();
}

static int shutter_also_ack = -1;
PROP_HANDLER( PROP_SHUTTER_ALSO ) // for Av mode
{
    if (!CONTROL_BV)
    {
        if (ABS(buf[0] - lens_info.raw_shutter) > 3) 
            lensinfo_set_shutter(buf[0]);
    }
    lens_display_set_dirty();
    shutter_also_ack = buf[0];
}

static int ae_ack = 12345;
PROP_HANDLER( PROP_AE )
{
    const uint32_t value = *(uint32_t *) buf;
    lens_info.ae = (int8_t)value;
    update_stuff();
    ae_ack = (int8_t)buf[0];
}

PROP_HANDLER( PROP_WB_MODE_LV )
{
    const uint32_t value = *(uint32_t *) buf;
    lens_info.wb_mode = value;
}

PROP_HANDLER(PROP_WBS_GM)
{
    const int8_t value = *(int8_t *) buf;
    lens_info.wbs_gm = value;
}

PROP_HANDLER(PROP_WBS_BA)
{
    const int8_t value = *(int8_t *) buf;
    lens_info.wbs_ba = value;
}

PROP_HANDLER( PROP_WB_KELVIN_LV )
{
    const uint32_t value = *(uint32_t *) buf;
    lens_info.kelvin = value;
}

#if !defined(CONFIG_5DC) && !defined(CONFIG_40D)
static uint16_t custom_wb_gains[128];
PROP_HANDLER(PROP_CUSTOM_WB)
{
    ASSERT(len <= sizeof(custom_wb_gains));
    memcpy(custom_wb_gains, buf, len);
    const uint16_t * gains = (uint16_t *) buf;
    lens_info.WBGain_R = gains[16];
    lens_info.WBGain_G = gains[18];
    lens_info.WBGain_B = gains[19];
}
#endif

void lens_set_custom_wb_gains(int gain_R, int gain_G, int gain_B)
{
#if !defined(CONFIG_5DC) && !defined(CONFIG_40D)
    // normalize: green gain should be always 1
    //~ gain_G = COERCE(gain_G, 4, 32000);
    //~ gain_R = COERCE(gain_R * 1024 / gain_G, 128, 32000);
    //~ gain_B = COERCE(gain_B * 1024 / gain_G, 128, 32000);
    //~ gain_G = 1024;

    gain_G = COERCE(gain_G, 128, 8192);
    gain_R = COERCE(gain_R, 128, 8192);
    gain_B = COERCE(gain_B, 128, 8192);

    // round off a bit to get nice values in menu
    gain_R = ((gain_R + 8) / 16) * 16;
    gain_B = ((gain_B + 8) / 16) * 16;
    
    custom_wb_gains[16] = gain_R;
    custom_wb_gains[18] = gain_G;
    custom_wb_gains[19] = gain_B;
    prop_request_change(PROP_CUSTOM_WB, custom_wb_gains, 0);

    int mode = WB_CUSTOM;
    prop_request_change(PROP_WB_MODE_LV, &mode, 4);
    prop_request_change(PROP_WB_MODE_PH, &mode, 4);
#endif
}

#define LENS_GET(param) \
int lens_get_##param() \
{ \
    return lens_info.param; \
} 

//~ LENS_GET(iso)
//~ LENS_GET(shutter)
//~ LENS_GET(aperture)
LENS_GET(ae)
//~ LENS_GET(kelvin)
//~ LENS_GET(wbs_gm)
//~ LENS_GET(wbs_ba)

#define LENS_SET(param) \
void lens_set_##param(int value) \
{ \
    int raw = VALUE2RAW(param,value); \
    if (raw >= 0) lens_set_raw##param(raw); \
}

LENS_SET(iso)
LENS_SET(shutter)
LENS_SET(aperture)

PROP_INT(PROP_WB_KELVIN_PH, wb_kelvin_ph);

void
lens_set_kelvin(int k)
{
    k = COERCE(k, KELVIN_MIN, KELVIN_MAX);
    int mode = WB_KELVIN;

    if (k > 10000 || k < 2500) // workaround for 60D; out-of-range values are ignored in photo mode
    {
        int lim = k > 10000 ? 10000 : 2500;
        if ((k > 10000 && (int)wb_kelvin_ph < lim) || (k < 2500 && (int)wb_kelvin_ph > lim))
        {
            prop_request_change(PROP_WB_KELVIN_PH, &lim, 4);
            msleep(20);
        }
    }

    prop_request_change(PROP_WB_MODE_LV, &mode, 4);
    prop_request_change(PROP_WB_KELVIN_LV, &k, 4);
    prop_request_change(PROP_WB_MODE_PH, &mode, 4);
    prop_request_change(PROP_WB_KELVIN_PH, &k, 4);
    msleep(20);
}

void
lens_set_kelvin_value_only(int k)
{
    k = COERCE(k, KELVIN_MIN, KELVIN_MAX);

    if (k > 10000 || k < 2500) // workaround for 60D; out-of-range values are ignored in photo mode
    {
        int lim = k > 10000 ? 10000 : 2500;
        prop_request_change(PROP_WB_KELVIN_PH, &lim, 4);
        msleep(10);
    }

    prop_request_change(PROP_WB_KELVIN_LV, &k, 4);
    prop_request_change(PROP_WB_KELVIN_PH, &k, 4);
    msleep(10);
}

void split_iso(int raw_iso, unsigned int* analog_iso, int* digital_gain)
{
    if (!raw_iso) { *analog_iso = 0; *digital_gain = 0; return; }
    int rounded = ((raw_iso+3)/8) * 8;
    if (get_htp()) rounded -= 8;
    *analog_iso = COERCE(rounded, 72, MAX_ANALOG_ISO); // analog ISO range: 100-3200 (100-25600 on 5D3)
    *digital_gain = raw_iso - *analog_iso;
}

void iso_components_update()
{
    split_iso(lens_info.raw_iso, &lens_info.iso_analog_raw, &lens_info.iso_digital_ev);

    lens_info.iso_equiv_raw = lens_info.raw_iso;

    int digic_gain = get_digic_iso_gain_movie();
    if (lens_info.iso_equiv_raw && digic_gain != 1024 && is_movie_mode())
    {
        lens_info.iso_equiv_raw = lens_info.iso_equiv_raw + (gain_to_ev_scaled(digic_gain, 8) - 80);
    }
}

static void update_stuff()
{
    calc_dof( &lens_info );
    //~ if (gui_menu_shown()) lens_display_set_dirty();
    
    #ifdef FEATURE_MOVIE_LOGGING
    if (movie_log) mvr_update_logfile( &lens_info, 0 ); // do not force it
    #endif
    
    iso_components_update();
}

#ifdef CONFIG_EOSM
PROP_HANDLER( PROP_LV_FOCAL_DISTANCE )
{
#ifdef FEATURE_MAGIC_ZOOM
    if (get_zoom_overlay_trigger_by_focus_ring()) zoom_overlay_set_countdown(300);
#endif
    
    idle_wakeup_reset_counters(-11);
    lens_display_set_dirty();
    
#ifdef FEATURE_LV_ZOOM_SETTINGS
    zoom_focus_ring_trigger();
#endif
}
#else
PROP_HANDLER( PROP_LV_LENS )
{
    const struct prop_lv_lens * const lv_lens = (void*) buf;
    lens_info.focal_len    = bswap16( lv_lens->focal_len );
    lens_info.focus_dist    = bswap16( lv_lens->focus_dist );
    
    if (lens_info.focal_len > 1000) // bogus values
        lens_info.focal_len = 0;

    //~ uint32_t lrswap = SWAP_ENDIAN(lv_lens->lens_rotation);
    //~ uint32_t lsswap = SWAP_ENDIAN(lv_lens->lens_step);

    //~ lens_info.lens_rotation = *((float*)&lrswap);
    //~ lens_info.lens_step = *((float*)&lsswap);
    
    static unsigned old_focus_dist = 0;
    static unsigned old_focal_len = 0;
    if (lv && (old_focus_dist && lens_info.focus_dist != old_focus_dist) && (old_focal_len && lens_info.focal_len == old_focal_len))
    {
        #ifdef FEATURE_MAGIC_ZOOM
        if (get_zoom_overlay_trigger_by_focus_ring()) zoom_overlay_set_countdown(300);
        #endif
        
        idle_wakeup_reset_counters(-11);
        lens_display_set_dirty();
        
        #ifdef FEATURE_LV_ZOOM_SETTINGS
        zoom_focus_ring_trigger();
        #endif
    }
    old_focus_dist = lens_info.focus_dist;
    old_focal_len = lens_info.focal_len;

    update_stuff();
}
#endif

/**
 * This tells whether the camera is ready to take a picture (or not)
 * 5D2: the sequence is: 0 11 10 8 0
 *      that means: 0 = idle, 11 = very busy (exposing), 10 = exposed, but processing (can take the next picture), 8 = done processing, just saving to card
 *      also, when job state is 11, we can't change camera settings, but when it's 10, we can
 * 5D3: the sequence is: 0 0x16 0x14 0x10 0
 * other cameras may have different values
 * 
 * => hypothesis: the general sequence is:
 * 
 *   0 max something_smaller something_even_smaller and so on
 * 
 *   so, we only want to avoid the situation when job_state == max_job_state
 * 
 */

static int max_job_state = 0;

int job_state_ready_to_take_pic()
{
    if (max_job_state == 0) return 1;
    return (int)lens_info.job_state < max_job_state;
}

PROP_HANDLER( PROP_LAST_JOB_STATE )
{
    const uint32_t state = *(uint32_t*) buf;
    lens_info.job_state = state;
    
    if (max_job_state == 0 && state != 0)
        max_job_state = state;
    
    ASSERT((int)state <= max_job_state);
    
    if (max_job_state && (int)state == max_job_state)
    {
        mirror_locked = 0;
        hdr_flag_picture_was_taken();
    }

    #ifdef CONFIG_JOB_STATE_DEBUG
    static char jmsg[100] = "";
    STR_APPEND(jmsg, "%d ", state);
    bmp_printf(FONT_MED,0,0, jmsg);
    #endif
}

static int fae_ack = 12345;
PROP_HANDLER(PROP_STROBO_AECOMP)
{
    lens_info.flash_ae = (int8_t) buf[0];
    fae_ack = (int8_t) buf[0];
}

int lens_set_flash_ae(int fae)
{
    fae = COERCE(fae, FLASH_MIN_EV * 8, FLASH_MAX_EV * 8);
    fae &= 0xFF;
    prop_request_change(PROP_STROBO_AECOMP, &fae, 4);

    fae_ack = 12345;
    for (int i = 0; i < 10; i++) { if (fae_ack != 12345) break; msleep(20); }
    return fae_ack == (int8_t)fae;
}

PROP_HANDLER(PROP_HALF_SHUTTER)
{
    update_stuff();
    lens_display_set_dirty();
    //~ bv_auto_update();
}

static struct menu_entry lens_menus[] = {
    #ifdef FEATURE_MOVIE_LOGGING
    {
        .name = "Movie Logging",
        .priv = &movie_log,
        .max = 1,
        .help = "Save metadata for each movie, e.g. MVI_1234.LOG",
        .depends_on = DEP_MOVIE_MODE,
    },
    #endif
};

#ifndef CONFIG_FULLFRAME

static struct menu_entry tweak_menus[] = {
    {
        .name = "Crop Factor Display",
        .priv = &crop_info,
        .max  = 1,
        .choices = CHOICES("OFF", "ON,35mm eq."),
        .help = "Display the 35mm equiv. focal length including crop factor.",
        .depends_on = DEP_LIVEVIEW | DEP_CHIPPED_LENS,
    }
};
#endif

// hack to show this at the end of prefs menu
void
crop_factor_menu_init()
{
#ifndef CONFIG_FULLFRAME
    menu_add("Prefs", tweak_menus, COUNT(tweak_menus));
#endif
}

static void
lens_init( void* unused )
{
    focus_done_sem = create_named_semaphore( "focus_sem", 1 );
#ifndef CONFIG_5DC
    menu_add("Movie Tweaks", lens_menus, COUNT(lens_menus));
#endif
}

INIT_FUNC( "lens", lens_init );


// picture style, contrast...
// -------------------------------------------

PROP_HANDLER(PROP_PICTURE_STYLE)
{
    const uint32_t raw = *(uint32_t *) buf;
    lens_info.raw_picstyle = raw;
    lens_info.picstyle = get_prop_picstyle_index(raw);
}

extern struct prop_picstyle_settings picstyle_settings[];

// get contrast/saturation/etc from the current picture style

#define LENS_GET_FROM_PICSTYLE(param) \
int \
lens_get_##param() \
{ \
    int i = lens_info.picstyle; \
    if (!i) return -10; \
    return picstyle_settings[i].param; \
} \

#define LENS_GET_FROM_OTHER_PICSTYLE(param) \
int \
lens_get_from_other_picstyle_##param(int picstyle_index) \
{ \
    return picstyle_settings[picstyle_index].param; \
} \

// set contrast/saturation/etc in the current picture style (change is permanent!)
#define LENS_SET_IN_PICSTYLE(param,lo,hi) \
void \
lens_set_##param(int value) \
{ \
    if (value < lo || value > hi) return; \
    int i = lens_info.picstyle; \
    if (!i) return; \
    picstyle_settings[i].param = value; \
    prop_request_change(PROP_PICSTYLE_SETTINGS(i), &picstyle_settings[i], 24); \
} \

LENS_GET_FROM_PICSTYLE(contrast)
LENS_GET_FROM_PICSTYLE(sharpness)
LENS_GET_FROM_PICSTYLE(saturation)
LENS_GET_FROM_PICSTYLE(color_tone)

LENS_GET_FROM_OTHER_PICSTYLE(contrast)
LENS_GET_FROM_OTHER_PICSTYLE(sharpness)
LENS_GET_FROM_OTHER_PICSTYLE(saturation)
LENS_GET_FROM_OTHER_PICSTYLE(color_tone)

LENS_SET_IN_PICSTYLE(contrast, -4, 4)
LENS_SET_IN_PICSTYLE(sharpness, 0, 7)
LENS_SET_IN_PICSTYLE(saturation, -4, 4)
LENS_SET_IN_PICSTYLE(color_tone, -4, 4)


void SW1(int v, int wait)
{
    //~ int unused;
    //~ ptpPropButtonSW1(v, 0, &unused);
    prop_request_change(PROP_REMOTE_SW1, &v, 0);
    if (wait) msleep(wait);
}

void SW2(int v, int wait)
{
    //~ int unused;
    //~ ptpPropButtonSW2(v, 0, &unused);
    prop_request_change(PROP_REMOTE_SW2, &v, 0);
    if (wait) msleep(wait);
}

/** exposure primitives (the "clean" way, via properties) */

static int prop_set_rawaperture(unsigned aperture)
{
    // Canon likes only numbers in 1/3 or 1/2-stop increments
    int r = aperture % 8;
    if (r != 0 && r != 4 && r != 3 && r != 5 
        && aperture != lens_info.raw_aperture_min && aperture != lens_info.raw_aperture_max)
        return 0;

    lens_wait_readytotakepic(64);
    aperture = COERCE(aperture, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
    //~ aperture_ack = -1;
    prop_request_change( PROP_APERTURE, &aperture, 4 );
    for (int i = 0; i < 10; i++) { if (aperture_ack == (int)aperture) return 1; msleep(20); }
    //~ NotifyBox(1000, "%d=%d ", aperture_ack, aperture);
    return 0;
}

static int prop_set_rawaperture_approx(unsigned new_av)
{

    // Canon likes only numbers in 1/3 or 1/2-stop increments
    new_av = COERCE(new_av, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
    if (!expo_value_rounding_ok(new_av)) // try to change it by a small amount, so Canon firmware will accept it
    {
        int new_av_plus1  = COERCE(new_av + 1, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        int new_av_minus1 = COERCE(new_av - 1, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        int new_av_plus2  = COERCE(new_av + 2, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        int new_av_minus2 = COERCE(new_av - 2, lens_info.raw_aperture_min, lens_info.raw_aperture_max);
        
        if (expo_value_rounding_ok(new_av_plus1)) new_av = new_av_plus1;
        else if (expo_value_rounding_ok(new_av_minus1)) new_av = new_av_minus1;
        else if (expo_value_rounding_ok(new_av_plus2)) new_av = new_av_plus2;
        else if (expo_value_rounding_ok(new_av_minus2)) new_av = new_av_minus2;
    }
    
    if (ABS((int)new_av - (int)lens_info.raw_aperture) <= 3) // nothing to do :)
        return 1;

    lens_wait_readytotakepic(64);
    aperture_ack = -1;
    prop_request_change( PROP_APERTURE, &new_av, 4 );
    for (int i = 0; i < 20; i++) { if (aperture_ack != -1) break; msleep(20); }
    return ABS(aperture_ack - (int)new_av) <= 3;
}

static int prop_set_rawshutter(unsigned shutter)
{
    // Canon likes numbers in 1/3 or 1/2-stop increments
    if (is_movie_mode())
    {
        int r = shutter % 8;
        if (r != 0 && r != 4 && r != 3 && r != 5)
            return 0;
    }
    
    if (shutter < 16) return 0;
    if (shutter > FASTEST_SHUTTER_SPEED_RAW) return 0;
    
    //~ bmp_printf(FONT_MED, 100, 100, "%d...", shutter);
    lens_wait_readytotakepic(64);
    //~ shutter = COERCE(shutter, 16, FASTEST_SHUTTER_SPEED_RAW); // 30s ... 1/8000 or 1/4000
    shutter_ack = -1;
    int s0 = shutter;
    prop_request_change( PROP_SHUTTER, &shutter, 4 );
    for (int i = 0; i < 5; i++) { if (shutter_ack != -1) break; msleep(20); }
    //~ for (int i = 0; i < 5; i++) { if (shutter_also_ack == s0) return 1; msleep(20); }
    //~ bmp_printf(FONT_MED, 100, 100, "%d %s ", shutter, shutter_ack == s0 ? ":)" : ":(");
    return shutter_ack == s0;
}

static int prop_set_rawshutter_approx(unsigned shutter)
{
    lens_wait_readytotakepic(64);
    shutter = COERCE(shutter, 16, FASTEST_SHUTTER_SPEED_RAW); // 30s ... 1/8000 or 1/4000
    shutter_ack = -1;
    prop_request_change( PROP_SHUTTER, &shutter, 4 );
    for (int i = 0; i < 5; i++) { if (shutter_ack != -1) break; msleep(20); }
    return ABS(shutter_ack - shutter) <= 3;
}

static int prop_set_rawiso(unsigned iso)
{
    lens_wait_readytotakepic(64);
    if (iso) iso = COERCE(iso, MIN_ISO, MAX_ISO); // ISO 100-25600
    iso_ack = -1;
    prop_request_change( PROP_ISO, &iso, 4 );
    for (int i = 0; i < 10; i++) { if (iso_ack != -1) break; msleep(20); }
    return iso_ack == (int)iso;
}

/** Exposure primitives (the "dirty" way, via BV control, bypasses protections) */

#ifdef FEATURE_EXPO_OVERRIDE

extern int bv_iso;
extern int bv_tv;
extern int bv_av;

void bv_update_lensinfo()
{
    if (CONTROL_BV) // sync lens info and camera properties with overriden values
    {
        lensinfo_set_iso(bv_iso + (get_htp() ? 8 : 0));
        lensinfo_set_shutter(bv_tv);
        lensinfo_set_aperture(bv_av);
    }
}

int bv_set_rawshutter(unsigned shutter)
{
    //~ bmp_printf(FONT_MED, 600, 300, "bvsr %d ", shutter);
    CONTROL_BV_TV = bv_tv = shutter;
    bv_update_lensinfo();
    bv_expsim_shift();
    //~ NotifyBox(2000, "%d > %d?", raw2shutter_ms(shutter), 1000/video_mode_fps); msleep(400);
    if (is_movie_mode() && raw2shutter_ms(shutter+1) > 1000/video_mode_fps) return 0;
    return shutter != 0;
}

int bv_set_rawiso(unsigned iso) 
{ 
    if (iso == 0) return 0;
    if (iso >= MIN_ISO && iso <= MAX_ISO_BV)
    {
        if (get_htp()) iso -= 8; // quirk: with exposure override and HTP, image is brighter by 1 stop than with Canon settings
        CONTROL_BV_ISO = bv_iso = iso; 
        bv_update_lensinfo();
        bv_expsim_shift();
        return 1;
    }
    else
    {
        return 0;
    }
}
int bv_set_rawaperture(unsigned aperture) 
{ 
    if (aperture >= lens_info.raw_aperture_min && aperture <= lens_info.raw_aperture_max) 
    { 
        CONTROL_BV_AV = bv_av = aperture; 
        bv_update_lensinfo();
        bv_expsim_shift();
        return 1; 
    }
    else
    {
        return 0;
    }
}

static void bv_expsim_shift_try_iso(int newiso)
{
    #ifndef FEATURE_LV_DISPLAY_GAIN
    #error This requires FEATURE_LV_DISPLAY_GAIN.
    #endif
    
    #define MAX_GAIN_EV 6
    int e = 0;
    if (newiso < 72)
        e = 72 - newiso;
    else if (newiso > MAX_ISO_BV + MAX_GAIN_EV*8)
        e = MAX_ISO_BV + MAX_GAIN_EV*8 - newiso;
    e = e * 10/8;
    
    static int prev_e = 0;
    if (e != prev_e)
    {
        if (ABS(e) > 2) NotifyBox(2000, "Preview %sexposed by %d.%d EV", e > 0 ? "over" : "under", ABS(e)/10, ABS(e)%10);
        else NotifyBoxHide();
    }
    prev_e = e;

    int g = 1024;
    while (newiso > MAX_ISO_BV && g < (1024 << MAX_GAIN_EV))
    {
        g *= 2;
        newiso -= 8;
    }

    CONTROL_BV_ISO = COERCE(newiso, 72, MAX_ISO_BV);
    set_photo_digital_iso_gain_for_bv(g);
}
static void bv_expsim_shift()
{
    set_photo_digital_iso_gain_for_bv(1024);
    if (!lv) return;
    if (!get_expsim()) return;
    if (!CONTROL_BV) return;
   
    if (!is_movie_mode())
    {
        int tv_fps_shift = fps_get_shutter_speed_shift(bv_tv);
        
        if (is_bulb_mode()) // try to perform expsim in bulb mode, based on bulb timer setting
        {
            int tv = get_bulb_shutter_raw_equiv() + tv_fps_shift;
            if (tv < 96)
            {
                int delta = 96 - tv;
                CONTROL_BV_TV = 96;
                bv_expsim_shift_try_iso(bv_iso + delta);
                return;
            }
            else
            {
                CONTROL_BV_TV = tv;
                CONTROL_BV_ISO = bv_iso;
                return;
            }
        }
        else
        {
            CONTROL_BV_TV = bv_tv;

            if (bv_tv < 96) // shutter speeds slower than 1/31 -> can't be obtained, raise ISO or open up aperture instead
            {
                int delta = 96 - bv_tv - tv_fps_shift;
                CONTROL_BV_TV = 96;
                bv_expsim_shift_try_iso(bv_iso + delta);
                return;
            }
            else if (tv_fps_shift) // FPS override enabled
            {
                bv_expsim_shift_try_iso(bv_iso - tv_fps_shift);
                return;
            }
        }
        // no shifting, make sure we use unaltered values
        CONTROL_BV_TV = bv_tv;
        CONTROL_BV_AV = bv_av;
        CONTROL_BV_ISO = bv_iso;
    }
    
    return;
}

static int bv_auto_should_enable()
{
    if (!bv_auto) return 0;
    if (!lv) return 0;

    extern int zoom_auto_exposure;
    if (zoom_auto_exposure && lv_dispsize > 1)
        return 0; // otherwise it would interfere with auto exposure
    
    extern int bulb_ramp_calibration_running; 
    if (bulb_ramp_calibration_running) 
        return 0; // temporarily disable BV mode to make sure display gain will work

    if (LVAE_DISP_GAIN) // compatibility problem, disable it
        return 0;

    if (bv_auto == 2) // only enabled when needed
    {
        // cameras without manual exposure control
        #if defined(CONFIG_50D)
        if (is_movie_mode() && shooting_mode == SHOOTMODE_M) return 1;
        else return 0;
        #endif
        #if defined(CONFIG_500D) || defined(CONFIG_1100D)
        if (is_movie_mode()) return 1;
        else return 0;
        #endif

        // extra ISO values in movie mode
        if (is_movie_mode() && lens_info.raw_iso==0) 
            return 0;
        
        if (is_movie_mode() && (bv_auto_needed_by_iso || bv_auto_needed_by_shutter || bv_auto_needed_by_aperture)) 
            return 1;
        
        // temporarily cancel it in photo mode
        //~ if (!is_movie_mode() && get_halfshutter_pressed())
            //~ return 0;
        
        // underexposure bug with manual lenses in M mode
        #if defined(CONFIG_60D) || defined(CONFIG_5D3)
        if (shooting_mode == SHOOTMODE_M && 
            !is_movie_mode() &&
            !lens_info.name[0] && 
            lens_info.raw_iso != 0
        )
            return 1;
        #endif
        
        // exposure simulation in Bulb mode
        if (is_bulb_mode() && get_expsim())
            return 1;
    }
    else if (bv_auto == 1) // always enable (except for situations where it's known to cause problems)
    {
        return 1; // tricky situations were handled before these if's
    }

    return 0;
}

void bv_auto_update()
{
    //~ take_semaphore(bv_sem, 0);
    
    if (!bv_auto) return;
    //~ take_semaphore(lens_sem, 0);
    if (bv_auto_should_enable()) bv_enable();
    else bv_disable();
    bv_expsim_shift();
    lens_display_set_dirty();
    //~ give_semaphore(lens_sem);
    //~ give_semaphore(bv_sem);
}

void bv_auto_update_startup()
{
    bv_auto_update();
    if (CONTROL_BV) shutter_was_set_from_ml = 1;
}
#endif

/** Camera control functions */
int lens_set_rawaperture( int aperture)
{
    bv_auto_needed_by_aperture = !prop_set_rawaperture(aperture); // first try to set via property
    #ifdef FEATURE_EXPO_OVERRIDE
    bv_auto_update(); // auto flip between "BV" or "normal"
    if (bv_auto_should_enable() || CONTROL_BV) return bv_set_rawaperture(aperture);
    #endif
    return !bv_auto_needed_by_aperture;
}

int lens_set_rawiso( int iso )
{
    bv_auto_needed_by_iso = !prop_set_rawiso(iso); // first try to set via property
    #ifdef FEATURE_EXPO_OVERRIDE
    bv_auto_update(); // auto flip between "BV" or "normal"
    if (bv_auto_should_enable() || CONTROL_BV) return bv_set_rawiso(iso);
    #endif
    return !bv_auto_needed_by_iso;
}

int lens_set_rawshutter( int shutter )
{
    #if defined(CONFIG_5D2) || defined(CONFIG_50D)
    lens_info.raw_shutter = 0; // force a refresh from prop handler(PROP_SHUTTER)
    #endif

    //~ bmp_printf(FONT_MED, 500, 300, "lsr %d ...", shutter);
    bv_auto_needed_by_shutter = !prop_set_rawshutter(shutter); // first try to set via property
    //~ bmp_printf(FONT_MED, 500, 300, "lsr %d %d  ", shutter, bv_auto_needed_by_shutter);
    #ifdef FEATURE_EXPO_OVERRIDE
    bv_auto_update(); // auto flip between "BV" or "normal"
    if (bv_auto_should_enable() || CONTROL_BV) { shutter_was_set_from_ml = 1; return bv_set_rawshutter(shutter); }
    #endif
    return !bv_auto_needed_by_shutter;
}


int lens_set_ae( int ae )
{
    ae_ack = 12345;
    ae = COERCE(ae, -MAX_AE_EV * 8, MAX_AE_EV * 8);
    prop_request_change( PROP_AE, &ae, 4 );
    for (int i = 0; i < 10; i++) { if (ae_ack != 12345) break; msleep(20); }
    return ae_ack == ae;
}

void lens_set_drivemode( int dm )
{
    lens_wait_readytotakepic(64);
    prop_request_change( PROP_DRIVE, &dm, 4 );
    msleep(10);
}

void lens_set_wbs_gm(int value)
{
    value = COERCE(value, -9, 9);
    prop_request_change(PROP_WBS_GM, &value, 4);
}

void lens_set_wbs_ba(int value)
{
    value = COERCE(value, -9, 9);
    prop_request_change(PROP_WBS_BA, &value, 4);
}

// Functions to change camera settings during bracketing
// They will check the operation and retry if necessary
// Used for HDR bracketing
static int hdr_set_something(int (*set_something)(int), int arg)
{
    // first try to set it a few times...
    for (int i = 0; i < 5; i++)
    {
        if (ml_shutdown_requested) return 0;

        if (set_something(arg))
            return 1;
    }

    // didn't work, let's wait for job state...
    lens_wait_readytotakepic(64);

    for (int i = 0; i < 5; i++)
    {
        if (ml_shutdown_requested) return 0;

        if (set_something(arg))
            return 1;
    }

    // now this is really extreme... okay, one final try
    while (lens_info.job_state) msleep(100);

    for (int i = 0; i < 5; i++)
    {
        if (ml_shutdown_requested) return 0;

        if (set_something(arg))
            return 1;
    }

    // I give up    
    return 0;
}

int hdr_set_rawiso(int iso)
{
    return hdr_set_something((int(*)(int))prop_set_rawiso, iso);
}

int hdr_set_rawshutter(int shutter)
{
    int ok = shutter < FASTEST_SHUTTER_SPEED_RAW && shutter > 13;
    return hdr_set_something((int(*)(int))prop_set_rawshutter_approx, shutter) && ok;
}

int hdr_set_rawaperture(int aperture)
{
    int ok = aperture < lens_info.raw_aperture_max && aperture > lens_info.raw_aperture_min;
    return hdr_set_something((int(*)(int))prop_set_rawaperture_approx, aperture) && ok;
}

int hdr_set_ae(int ae)
{
    int ok = ABS(ae) < MAX_AE_EV * 8;
    return hdr_set_something((int(*)(int))lens_set_ae, ae) && ok;
}

int hdr_set_flash_ae(int fae)
{
    int ok = fae < FLASH_MAX_EV * 8 && fae > FLASH_MIN_EV * 8;
    return hdr_set_something((int(*)(int))lens_set_flash_ae, fae) && ok;
}

/*
void gui_bump_rawshutter(int delta)
{
}

void gui_bump_rawiso(int delta)
{
}

void gui_bump_rawaperture(int delta)
{
}

void lens_task()
{
    while(1)
    {
    }
}

TASK_CREATE( "lens_task", lens_task, 0, 0x1a, 0x1000 );
*/
