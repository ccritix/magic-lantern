/** \file
 * Bitrate
 */
#include "dryos.h"
#include "bmp.h"
#include "tasks.h"
#include "debug.h"
#include "menu.h"
#include "property.h"
#include "config.h"
#include "gui.h"
#include "lens.h"

//----------------begin qscale-----------------
CONFIG_INT( "h264.qscale.plus16", qscale_plus16, 16-8 );
CONFIG_INT( "h264.bitrate-mode", bitrate_mode, 1 ); // off, CBR, VBR
CONFIG_INT( "h264.bitrate-factor", bitrate_factor, 10 );
#if defined(CONFIG_7D_MINIMAL)
CONFIG_INT( "time.indicator", time_indicator, 1); // 0 = off, 1 = current clip length
#else
CONFIG_INT( "time.indicator", time_indicator, 3); // 0 = off, 1 = current clip length, 2 = time remaining until filling the card, 3 = time remaining until 4GB
#endif
CONFIG_INT( "bitrate.indicator", bitrate_indicator, 0);
#ifdef CONFIG_600D
CONFIG_INT( "hibr.wav.record", cfg_hibr_wav_record, 0);
#endif
int time_indic_x =  720 - 160;
int time_indic_y = 0;
int time_indic_width = 160;
int time_indic_height = 20;
int time_indic_warning = 120;
static int time_indic_font  = FONT(FONT_MED, COLOR_RED, COLOR_BLACK );

int measured_bitrate = 0; // mbps
//~ int free_space_32k = 0;
int movie_bytes_written_32k = 0;

#define qscale (((int)qscale_plus16) - 16)

int bitrate_dirty = 0;

// don't call those outside vbr_fix / vbr_set
void mvrFixQScale(uint16_t *);    // only safe to call when not recording
void mvrSetDefQScale(int16_t *);  // when recording, only change qscale by 1 at a time
// otherwise ther appears a nice error message which shows the shutter count [quote AlinS] :)

static struct mvr_config mvr_config_copy;
void cbr_init()
{
    memcpy(&mvr_config_copy, &mvr_config, sizeof(mvr_config_copy));
}

void vbr_fix(uint16_t param)
{
    if (!lv) return;
    if (!is_movie_mode()) return; 
    if (recording) return; // err70 if you do this while recording

    mvrFixQScale(&param);
}

// may be dangerous if mvr_config and numbers are incorrect
void opt_set(int num, int den)
{
    int i, j;
    

    for (i = 0; i < MOV_RES_AND_FPS_COMBINATIONS; i++) // 7 combinations of resolution / fps
    {
#ifdef CONFIG_500D
#define fullhd_30fps_opt_size_I fullhd_20fps_opt_size_I
#define fullhd_30fps_gop_opt_0 fullhd_20fps_gop_opt_0
#endif

#ifdef CONFIG_5D2
#define fullhd_30fps_opt_size_I v1920_30fps_opt_size_I
#endif
        for (j = 0; j < MOV_OPT_NUM_PARAMS; j++)
        {
            int* opt0 = (int*) &(mvr_config_copy.fullhd_30fps_opt_size_I) + i * MOV_OPT_STEP + j;
            int* opt = (int*) &(mvr_config.fullhd_30fps_opt_size_I) + i * MOV_OPT_STEP + j;
            if (*opt0 < 10000) { bmp_printf(FONT_LARGE, 0, 50, "opt_set: err %d %d %d ", i, j, *opt0); return; }
            (*opt) = (*opt0) * num / den;
        }
        for (j = 0; j < MOV_GOP_OPT_NUM_PARAMS; j++)
        {
            int* opt0 = (int*) &(mvr_config_copy.fullhd_30fps_gop_opt_0) + i * MOV_GOP_OPT_STEP + j;
            int* opt = (int*) &(mvr_config.fullhd_30fps_gop_opt_0) + i * MOV_GOP_OPT_STEP + j;
            if (*opt0 < 10000) { bmp_printf(FONT_LARGE, 0, 50, "gop_set: err %d %d %d ", i, j, *opt0); return; }
            (*opt) = (*opt0) * num / den;
        }
    }
}

void bitrate_set()
{
    if (!lv) return;
    if (!is_movie_mode()) return; 
    if (gui_menu_shown()) return;
    if (recording) return; 
    
    if (bitrate_mode == 0)
    {
        if (!bitrate_dirty) return;
        vbr_fix(0);
        opt_set(1,1);
    }
    else if (bitrate_mode == 1) // CBR
    {
        if (bitrate_factor == 10 && !bitrate_dirty) return;
        vbr_fix(0);
        opt_set(bitrate_factor, 10);
    }
    else if (bitrate_mode == 2) // QScale
    {
        vbr_fix(1);
        opt_set(1,1);
        int16_t q = qscale;
        mvrSetDefQScale(&q);
    }
    bitrate_dirty = 1;
}

static void
bitrate_print(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    int fnt = selected ? MENU_FONT_SEL : MENU_FONT;

    if (bitrate_mode == 0)
    {
        bmp_printf( fnt, x, y, "Bit Rate      : FW default%s", bitrate_dirty ? "(reboot)" : "");
        menu_draw_icon(x, y, bitrate_dirty ? MNI_WARNING : MNI_OFF, 0);
    }
    else if (bitrate_mode == 1)
    {
        if (bitrate_factor > 10) fnt = FONT(fnt, bitrate_factor > 14 ? COLOR_RED : COLOR_YELLOW, FONT_BG(fnt));
        if (bitrate_factor < 7) fnt = FONT(fnt, bitrate_factor < 4 ? COLOR_RED : COLOR_YELLOW, FONT_BG(fnt));
        bmp_printf( fnt, x, y, "Bit Rate (CBR): %s%d.%dx%s", bitrate_factor>10 ? "up to " : "", bitrate_factor/10, bitrate_factor%10, bitrate_dirty || bitrate_factor != 10 ? "" : " (FW default)");
        menu_draw_icon(x, y, bitrate_dirty || bitrate_factor != 10 ? MNI_PERCENT : MNI_OFF, bitrate_factor * 100 / 30);
    }
    else if (bitrate_mode == 2)
    {
        fnt = FONT(fnt, COLOR_RED, FONT_BG(fnt));
        bmp_printf( fnt, x, y, "Bit Rate (VBR): QScale %d", qscale);
        menu_draw_icon(x, y, MNI_PERCENT, -(qscale-16) * 100 / 32);
    }
}

void bitrate_mvr_log(char* mvr_logfile_buffer)
{
    if (bitrate_mode == 1)
    {
        MVR_LOG_APPEND (
            "Bit Rate (CBR) : %d.%dx", bitrate_factor/10, bitrate_factor%10
        );
    }
    else if (bitrate_mode == 2)
    {
        MVR_LOG_APPEND (
            "Bit Rate (VBR) : QScale %d", qscale
        );
    }
}

static void
cbr_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf( selected ? MENU_FONT_SEL : MENU_FONT, x, y, "CBR factor    : %d.%dx", bitrate_factor/10, bitrate_factor%10);
    if (bitrate_mode == 1) menu_draw_icon(x, y, MNI_PERCENT, bitrate_factor * 100 / 30);
    else menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "CBR mode inactive => CBR setting not used.");
}

static void
qscale_display(
    void *          priv,
    int         x,
    int         y,
    int         selected
)
{
    bmp_printf( selected ? MENU_FONT_SEL : MENU_FONT, x, y, "QScale factor : %d", qscale);
    if (bitrate_mode == 2) menu_draw_icon(x, y, MNI_PERCENT, -(qscale-16) * 100 / 32);
    else menu_draw_icon(x, y, MNI_WARNING, (intptr_t) "VBR mode inactive => QScale setting not used.");
}


static void 
bitrate_factor_toggle(void* priv, int delta)
{
    if (recording) return;
    bitrate_factor = mod(bitrate_factor + delta - 1, 30) + 1;
}

static void 
bitrate_qscale_toggle(void* priv, int delta)
{
    if (recording) return;
    qscale_plus16 = mod(qscale_plus16 - delta, 33);
}

static void 
bitrate_toggle_mode(void* priv, int delta)
{
    if (recording) return;
    menu_ternary_toggle(priv, delta);
}

static void 
bitrate_toggle(void* priv, int delta)
{
    if (bitrate_mode == 1) bitrate_factor_toggle(priv, delta);
    else if (bitrate_mode == 2) bitrate_qscale_toggle(priv, delta);
}


int movie_elapsed_time_01s = 0;   // seconds since starting the current movie * 10

PROP_INT(PROP_CLUSTER_SIZE, cluster_size);
PROP_INT(PROP_FREE_SPACE, free_space_raw);
#define free_space_32k (free_space_raw * (cluster_size>>10) / (32768>>10))


void free_space_show()
{
    if (!get_global_draw()) return;
    if (gui_menu_shown()) return;
    if (recording && time_indicator) return;
    int fsg = free_space_32k >> 15;
    int fsgr = free_space_32k - (fsg << 15);
    int fsgf = (fsgr * 10) >> 15;

    // trick to erase the old text, if any (problem due to shadow fonts)
    bmp_printf(
        FONT(FONT_MED, COLOR_WHITE, TOPBAR_BGCOLOR),
        time_indic_x + 160 - 6 * font_med.width,
        time_indic_y,
        "      "
    );

    bmp_printf(
        FONT(SHADOW_FONT(FONT_MED), COLOR_WHITE, COLOR_BLACK),
        time_indic_x + 160 - 6 * font_med.width,
        time_indic_y,
        "%d.%dGB",
        fsg,
        fsgf
    );
}

void fps_show()
{
    if (!get_global_draw()) return;
    if (gui_menu_shown()) return;
    if (!is_movie_mode() || recording) return;
    //~ if (hdmi_code == 5) return; // workaround
    int screen_layout = get_screen_layout();
    if (screen_layout > SCREENLAYOUT_3_2_or_4_3) return;
    
/*    bmp_printf(
        SHADOW_FONT(FONT_MED),
        time_indic_x + 160 - (video_mode_resolution == 0 ? 7 : 6) * font_med.width,
        time_indic_y + font_med.height - 3,
        "%d%s%s", 
        video_mode_fps, 
        video_mode_crop ? "+" : "p",
        video_mode_resolution == 0 ? "1080" :
        video_mode_resolution == 1 ? "720" : "VGA"
    );*/

    int f = fps_get_current_x1000();
    bmp_printf(
        SHADOW_FONT(FONT_MED),
        time_indic_x + 160 - 6 * font_med.width,
        time_indic_y + font_med.height - 3,
        "%2d.%03d", 
        f / 1000, f % 1000
    );
}

void free_space_show_photomode()
{
    int fsg = free_space_32k >> 15;
    int fsgr = free_space_32k - (fsg << 15);
    int fsgf = (fsgr * 10) >> 15;
    
#if defined(CONFIG_7D)    
    int x = DISPLAY_CLOCK_POS_X;
    int y = DISPLAY_CLOCK_POS_Y - font_med.height - 14;
    
    if(fsg < 10)
    {
        bmp_printf(FONT(SHADOW_FONT(FONT_LARGE), COLOR_FG_NONLV, bmp_getpixel(x,y)), x, y, "%d.%d G", fsg, fsgf );
    }
    else if(fsg < 100)
    {
        bmp_printf(FONT(SHADOW_FONT(FONT_LARGE), COLOR_FG_NONLV, bmp_getpixel(x,y)), x, y, "%d.%dG", fsg, fsgf );
    }
    else
    {
        bmp_printf(FONT(SHADOW_FONT(FONT_LARGE), COLOR_FG_NONLV, bmp_getpixel(x,y)), x, y, "%d GB", fsg, fsgf );
    }
#else
    int x = time_indic_x + 2 * font_med.width;
    int y =  452;
    bmp_printf(
        FONT(SHADOW_FONT(FONT_LARGE), COLOR_FG_NONLV, bmp_getpixel(x-10,y+10)),
        x, y,
        "%d.%dGB",
        fsg,
        fsgf
    );
#endif
}

void time_indicator_show()
{
    if (!get_global_draw()) return;

    if (!recording) 
    {
        free_space_show();
        return;
    }
    
    // time until filling the card
    // in "movie_elapsed_time_01s" seconds, the camera saved "movie_bytes_written_32k"x32kbytes, and there are left "free_space_32k"x32kbytes
    int time_cardfill = movie_elapsed_time_01s * free_space_32k / movie_bytes_written_32k / 10;
    
    // time until 4 GB
    int time_4gb = movie_elapsed_time_01s * (4 * 1024 * 1024 / 32 - movie_bytes_written_32k) / movie_bytes_written_32k / 10;

    //~ bmp_printf(FONT_MED, 0, 300, "%d %d %d %d ", movie_elapsed_time_01s, movie_elapsed_ticks, rec_time_card, rec_time_4gb);

    // what to display
    int dispvalue = time_indicator == 1 ? movie_elapsed_time_01s / 10:
                    time_indicator == 2 ? time_cardfill :
                    time_indicator == 3 ? MIN(time_4gb, time_cardfill)
                    : 0;
    
    if (time_indicator)
    {
        bmp_printf(
            time_4gb < time_indic_warning ? time_indic_font : FONT(FONT_MED, COLOR_WHITE, TOPBAR_BGCOLOR),
            time_indic_x + 160 - 6 * font_med.width,
            time_indic_y,
            "%3d:%02d",
            dispvalue / 60,
            dispvalue % 60
        );
    }
    if (bitrate_indicator)
    {
        bmp_printf( FONT_SMALL,
            680 - font_small.width * 5,
            55,
            "A%3d ",
            movie_bytes_written_32k * 32 * 80 / 1024 / movie_elapsed_time_01s);

        bmp_printf(FONT_SMALL,
            680 - font_small.width * 5,
            55 + font_small.height,
            "B%3d ",
            measured_bitrate
        );

        int fnts = FONT(FONT_SMALL, COLOR_WHITE, mvr_config.actual_qscale_maybe == -16 ? COLOR_RED : COLOR_BLACK);
        bmp_printf(fnts,
            680,
            55 + font_small.height,
            " Q%s%02d",
            mvr_config.actual_qscale_maybe < 0 ? "-" : "+",
            ABS(mvr_config.actual_qscale_maybe)
        );
    }
    
    //~ if (flicker_being_killed()) // this also kills recording dot
    //~ {
        //~ maru(os.x_max - 28, os.y0 + 12, COLOR_RED);
    //~ }
}

void measure_bitrate() // called once / second
{
    static uint32_t prev_bytes_written = 0;
    uint32_t bytes_written = MVR_BYTES_WRITTEN;
    int bytes_delta = (((int)(bytes_written >> 1)) - ((int)(prev_bytes_written >> 1))) << 1;
    prev_bytes_written = bytes_written;
    movie_bytes_written_32k = bytes_written >> 15;
    measured_bitrate = (ABS(bytes_delta) / 1024) * 8 / 1024;
}

static void
time_indicator_display( void * priv, int x, int y, int selected )
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Time Indicator: %s",
        time_indicator == 1 ? "Elapsed" :
        time_indicator == 2 ? "Remain.Card" :
        time_indicator == 3 ? "Remain.4GB" : "OFF"
    );
    menu_draw_icon(x, y, MNI_BOOL_GDR(time_indicator));
}

/*static void
bitrate_indicator_display( void * priv, int x, int y, int selected )
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "Bitrate Info  : %s",
        bitrate_indicator ? "ON" : "OFF"
    );
    menu_draw_icon(x, y, MNI_BOOL_GDR(bitrate_indicator));
}*/

CONFIG_INT("buffer.warning.level", buffer_warning_level, 70);
static void
buffer_warning_level_display( void * priv, int x, int y, int selected )
{
    bmp_printf(
        selected ? MENU_FONT_SEL : MENU_FONT,
        x, y,
        "BuffWarnLevel : %d%%",
        buffer_warning_level
    );
    menu_draw_icon(x, y, MNI_PERCENT, buffer_warning_level);
}

static void buffer_warning_level_toggle(void* priv, int step)
{
    buffer_warning_level += step;
    if (buffer_warning_level > 100) buffer_warning_level = 30;
    if (buffer_warning_level < 30) buffer_warning_level = 100;
}

int warning = 0;
int is_mvr_buffer_almost_full() 
{
    if (recording == 0) return 0;
    if (recording == 1) return 1;
    // 2
    
    int ans = MVR_BUFFER_USAGE > (int)buffer_warning_level;
    if (ans) warning = 1;
    return warning;
}

void show_mvr_buffer_status()
{
    int fnt = warning ? FONT(FONT_SMALL, COLOR_WHITE, COLOR_RED) : FONT(FONT_SMALL, COLOR_WHITE, COLOR_GREEN2);
    if (warning) warning--;
    if (recording && get_global_draw() && !gui_menu_shown()) bmp_printf(fnt, 680, 55, " %3d%%", MVR_BUFFER_USAGE);
}

static void load_h264_ini()
{
    gui_stop_menu();
    call("IVAParamMode", CARD_DRIVE "ML/H264.ini");
    NotifyBox(2000, "%s", 0x4da10);
}

#ifdef CONFIG_600D
static void hibr_wav_record_select( void * priv, int x, int y, int selected ){
    menu_numeric_toggle(priv, 1, 0, 1);
    if (recording) return;
    int *onoff = (int *)priv;
    if(*onoff == 1){
        if (sound_recording_mode != 1){
            int mode  = 1; //disabled
            prop_request_change(PROP_MOVIE_SOUND_RECORD, &mode, 4);
            NotifyBox(2000,"Canon sound disabled");
            audio_configure(1);
        }
    }
}
static void hibr_wav_record_display( void * priv, int x, int y, int selected ){
    bmp_printf(selected ? MENU_FONT_SEL : MENU_FONT,
               x, y,
               "Sound rec     : %s", 
               (cfg_hibr_wav_record ? "Separate WAV" : "Normal")
               );
}
#endif

static struct menu_entry mov_menus[] = {
#if !defined(CONFIG_7D_MINIMAL)
    {
        .name = "Bit Rate",
        .priv = &bitrate_mode,
        .display    = bitrate_print,
        .select     = bitrate_toggle,
        .help = "Change H.264 bitrate. Be careful, recording may stop!",
        //.essential = 1,
        .edit_mode = EM_MANY_VALUES,
        .children =  (struct menu_entry[]) {
            {
                .name = "Mode",
                .priv = &bitrate_mode,
                .max = 2,
                .select = bitrate_toggle_mode,
                .choices = (const char *[]) {"FW default", "CBR", "VBR (QScale)"},
                .help = "Firmware default / CBR (recommended) / VBR (very risky)"
            },
            {
                .name = "CBR factor",
                .priv = &bitrate_factor,
                .select = bitrate_factor_toggle,
                .display = cbr_display,
                .help = "1.0x = Canon default, 0.4x = 30minutes, 1.4x = fast card."
            },
            {
                .name = "QScale",
                .priv = &qscale_plus16,
                .select = bitrate_qscale_toggle,
                .display = qscale_display,
                .help = "Quality factor (-16 = best quality). Try not to use it!"
            },
            {
                .name = "Bitrate Info",
                .priv       = &bitrate_indicator,
                .max = 1,
                .help = "A = average, B = instant bitrate, Q = instant QScale."
            },
            {
                .name = "BuffWarnLevel",
                .select     = buffer_warning_level_toggle,
                .display    = buffer_warning_level_display,
                .help = "ML will pause CPU-intensive graphics if buffer gets full."
            },
  #ifdef CONFIG_600D
            {
                .name = "Sound Record\b",
                .priv = &cfg_hibr_wav_record,
                .select = hibr_wav_record_select,
                .display    = hibr_wav_record_display,
                //                .choices = (const char *[]) {"Disabled", "Separate WAV"},
                //                .icon_type = IT_BOOL,
                .help = "Sound goes out of sync, so it has to be recorded separately.",
            },
  #endif
            MENU_EOL
        },
    },
#endif
    {
        .name = "Time Indicator",
        .priv       = &time_indicator,
#if !defined(CONFIG_7D_MINIMAL)
        .select     = menu_quaternary_toggle,
#else
        .select     = menu_binary_toggle,
#endif
        .display    = time_indicator_display,
        .help = "Time indicator while recording.",
        //.essential = 1,
        //~ .edit_mode = EM_MANY_VALUES,
    },
};

void bitrate_init()
{
    menu_add( "Movie", mov_menus, COUNT(mov_menus) );
}

INIT_FUNC(__FILE__, bitrate_init);

static void
bitrate_task( void* unused )
{
    cbr_init();

    TASK_LOOP
    {
        if (recording) wait_till_next_second(); // uses a bit of CPU, but it's precise
        else msleep(1000); // relax
        
        if (recording) 
        {
            movie_elapsed_time_01s += 10;
            measure_bitrate();
            BMP_LOCK( show_mvr_buffer_status(); )
        }
        else
        {
            movie_elapsed_time_01s = 0;
            if (movie_elapsed_time_01s % 10 == 0)
                bitrate_set();
        }
    }
}

void movie_indicators_show()
{
    if (recording)
    {
        BMP_LOCK( time_indicator_show(); )
    }
    else
    {
        BMP_LOCK(
            free_space_show(); 
            fps_show();
        )
    }
}

TASK_CREATE("bitrate_task", bitrate_task, 0, 0x1d, 0x1000 );
