/**
 * RAW recording. Similar to lv_rec, with some different internals:
 * 
 * - buffering: group the frames in 32GB contiguous chunks, to maximize writing speed
 * - edmac_copy_rectangle: we can crop the image and trim the black borders!
 * - edmac operation done outside the LV task
 * - on buffer overflow, it stops or skips frames (user-selected)
 * - using generic raw routines, no hardcoded stuff (should be easier to port)
 * - only for RAW in a single file (do one thing and do it well)
 * - goal: 1920x1080 on 1000x cards
 * 
 * Usage:
 * - enable modules in Makefile.user (CONFIG_MODULES = y, CONFIG_TCC = y, CONFIG_PICOC = n, CONFIG_CONSOLE = y)
 * - run "make" from modules/raw_rec to compile this module and the DNG converter
 * - run "make install" from platform dir to copy the modules on the card
 * - from Module menu: Load modules now
 * - look in Movie menu
 */

#define CONFIG_CONSOLE

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include "../lv_rec/lv_rec.h"
#include "edmac.h"
#include "../file_man/file_man.h"

/* camera-specific tricks */
/* todo: maybe add generic functions like is_digic_v, is_5d2 or stuff like that? */
static int cam_eos_m = 0;
static int cam_5d2 = 0;
static int cam_50d = 0;
static int cam_5d3 = 0;
static int cam_6d = 0;

/**
 * resolution should be multiple of 16 horizontally
 * see http://www.magiclantern.fm/forum/index.php?topic=5839.0
 * use roughly 10% increments
 **/

static int resolution_presets_x[] = {  640,  704,  768,  864,  960,  1152,  1280,  1344,  1472,  1600,  1728,  1856,  1920,  2048,  2240,  2560,  2880,  3584 };
#define  RESOLUTION_CHOICES_X CHOICES("640","704","768","864","960","1152","1280","1344","1472","1600","1728","1856","1920","2048","2240","2560","2880","3584")

static int aspect_ratio_presets_num[]      = {   5,    4,    3,       8,      25,     239,     235,      22,    2,     185,     16,    5,    3,    4,    1,    1 };
static int aspect_ratio_presets_den[]      = {   1,    1,    1,       3,      10,     100,     100,      10,    1,     100,      9,    3,    2,    3,    1,    2 };
static const char * aspect_ratio_choices[] = {"5:1","4:1","3:1","2.67:1","2.50:1","2.39:1","2.35:1","2.20:1","2:1","1.85:1", "16:9","5:3","3:2","4:3","1:1","1:2"};

/* config variables */

static CONFIG_INT("raw.video.enabled", raw_video_enabled, 0);

static CONFIG_INT("raw.res.x", resolution_index_x, 12);
static CONFIG_INT("raw.aspect.ratio", aspect_ratio_index, 10);
static CONFIG_INT("raw.write.spd", measured_write_speed, 0);
static CONFIG_INT("raw.skip.frames", allow_frame_skip, 0);
static CONFIG_INT("raw.sound", sound_rec, 2);

static CONFIG_INT("raw.framing", framing_mode, 0);
#define FRAMING_CENTER (framing_mode == 0)
#define FRAMING_LEFT (framing_mode == 1)
#define FRAMING_PANNING (framing_mode == 2)

static CONFIG_INT("raw.preview", preview_mode, 0);
#define PREVIEW_AUTO (preview_mode == 0)
#define PREVIEW_CANON (preview_mode == 1)
#define PREVIEW_ML (preview_mode == 2)
#define PREVIEW_HACKED (preview_mode == 3)

static CONFIG_INT("raw.memory.hack", memory_hack, 0);

/* state variables */
static int res_x = 0;
static int res_y = 0;
static int max_res_x = 0;
static int max_res_y = 0;
static int shave_right = 0;
static float squeeze_factor = 0;
int frame_size = 0;
int skip_x = 0;
int skip_y = 0;

static int frame_offset_x = 0;
static int frame_offset_y = 0;
static int frame_offset_delta_x = 0;
static int frame_offset_delta_y = 0;

#define RAW_IDLE      0
#define RAW_PREPARING 1
#define RAW_RECORDING 2
#define RAW_FINISHING 3

static int raw_recording_state = RAW_IDLE;
static int raw_playing = 0;
static int raw_previewing = 0;

#define RAW_IS_IDLE      (raw_recording_state == RAW_IDLE)
#define RAW_IS_PREPARING (raw_recording_state == RAW_PREPARING)
#define RAW_IS_RECORDING (raw_recording_state == RAW_RECORDING)
#define RAW_IS_FINISHING (raw_recording_state == RAW_FINISHING)

struct buff
{
    void* ptr;
    int size;
    int used;
};

static struct memSuite * mem_suite = 0;           /* memory suite for our buffers */
static void * fullsize_buffers[2];                /* original image, before cropping, double-buffered */
static struct buff buffers[20];                   /* our recording buffers */
static int buffer_count = 0;                      /* how many buffers we could allocate */
static int capturing_buffer_index = 0;            /* in which buffer we are capturing */
static int saving_buffer_index = 0;               /* from which buffer we are saving to card */
static int capture_offset = 0;                    /* position of capture pointer inside the buffer (0-32MB) */
static int fullsize_buffer_pos = 0;               /* which of the full size buffers (double buffering) is currently in use */
static int frame_count = 0;                       /* how many frames we have processed */
static int frame_skips = 0;                       /* how many frames were dropped/skipped */
static char* movie_filename = 0;                  /* file name for current (or last) movie */
static char* chunk_filename = 0;                  /* file name for current movie chunk */
static uint32_t written = 0;                      /* how many KB we have written in this movie */
static int writing_time = 0;                      /* time spent by raw_video_rec_task in FIO_WriteFile calls */
static int idle_time = 0;                         /* time spent by raw_video_rec_task doing something else */

/* interface to other modules:
 *
 *    unsigned int raw_rec_skip_frame(unsigned char *frame_data)
 *      This function is called on every single raw frame that is received from sensor with a pointer to frame data as parameter.
 *      If the return value is zero, the frame will get save into the saving buffers, else it is skipped
 *      Default: Do not skip frame (0)
 *
 *    unsigned int raw_rec_save_buffer(unsigned int used, unsigned int buffer_count)
 *      This function is called whenever the writing loop is checking if it has data to save to card.
 *      The parameters are the number of used buffers and the total buffer count
 *      Default: Save buffer (1)
 *
 *    unsigned int raw_rec_skip_buffer(unsigned int buffer_index, unsigned int buffer_count);
 *      Whenever the buffers are full, this function is called with the buffer index that is subject to being dropped, the number of frames in this buffer and the total buffer count.
 *      If it returns zero, this buffer will not get thrown away, but the next frame will get dropped.
 *      Default: Do not throw away buffer, but throw away incoming frame (0)
 */
extern WEAK_FUNC(ret_0) unsigned int raw_rec_cbr_starting();
extern WEAK_FUNC(ret_0) unsigned int raw_rec_cbr_stopping();
extern WEAK_FUNC(ret_0) unsigned int raw_rec_cbr_skip_frame(unsigned char *frame_data);
extern WEAK_FUNC(ret_1) unsigned int raw_rec_cbr_save_buffer(unsigned int used, unsigned int buffer_index, unsigned int frame_count, unsigned int buffer_count);
extern WEAK_FUNC(ret_0) unsigned int raw_rec_cbr_skip_buffer(unsigned int buffer_index, unsigned int frame_count, unsigned int buffer_count);

static int calc_res_y(int res_x, int num, int den, float squeeze)
{
    if (squeeze != 1.0f)
    {
        /* image should be enlarged vertically in post by a factor equal to "squeeze" */
        return (int)(roundf(res_x * den / num / squeeze) + 1) & ~1;
    }
    else
    {
        /* assume square pixels */
        return (res_x * den / num + 1) & ~1;
    }
}

static void update_cropping_offsets()
{
    int sx = raw_info.active_area.x1 + (max_res_x - res_x) / 2;
    int sy = raw_info.active_area.y1 + (max_res_y - res_y) / 2;

    if (FRAMING_PANNING)
    {
        sx += frame_offset_x;
        sy += frame_offset_y;
    }
    else if (FRAMING_LEFT)
    {
        sx = raw_info.active_area.x1;
    }
    else if (FRAMING_CENTER && lv_dispsize > 1)
    {
        /* try to center the recording window on the YUV frame */
        int delta_x, delta_y;
        int ok = focus_box_get_raw_crop_offset(&delta_x, &delta_y);
        if (ok)
        {
            sx = COERCE(sx - delta_x, raw_info.active_area.x1, raw_info.active_area.x2 - res_x);
            sy = COERCE(sy - delta_y, raw_info.active_area.y1, raw_info.active_area.y2 - res_y);
        }
    }

    skip_x = sx;
    skip_y = sy;
}

static void update_resolution_params()
{
    /* max res X */
    /* make sure we don't get dead pixels from rounding */
    int left_margin = (raw_info.active_area.x1 + 7) / 8 * 8;
    int right_margin = (raw_info.active_area.x2 + shave_right) / 8 * 8;
    int max = (right_margin - left_margin) & ~15;
    while (max % 16) max--;
    max_res_x = max;
    
    /* max res Y */
    max_res_y = raw_info.jpeg.height & ~1;

    /* squeeze factor */
    if (video_mode_resolution == 1 && lv_dispsize == 1 && is_movie_mode()) /* 720p, image squeezed */
    {
        /* assume the raw image should be 16:9 when de-squeezed */
        int correct_height = max_res_x * 9 / 16;
        squeeze_factor = (float)correct_height / max_res_y;
    }
    else squeeze_factor = 1.0f;

    /* res X */
    res_x = MIN(resolution_presets_x[resolution_index_x], max_res_x);

    /* res Y */
    int num = aspect_ratio_presets_num[aspect_ratio_index];
    int den = aspect_ratio_presets_den[aspect_ratio_index];
    res_y = MIN(calc_res_y(res_x, num, den, squeeze_factor), max_res_y);

    /* frame size */
    /* should be multiple of 512, so there's no write speed penalty (see http://chdk.setepontos.com/index.php?topic=9970 ; confirmed by benchmarks) */
    /* should be multiple of 4096 for proper EDMAC alignment */
    frame_size = (res_x * res_y * 14/8 + 4095) & ~4095;
    
    update_cropping_offsets();
}

static char* guess_aspect_ratio(int res_x, int res_y)
{
    static char msg[20];
    int best_num = 0;
    int best_den = 0;
    float ratio = (float)res_x / res_y;
    float minerr = 100;
    /* common ratios that are expressed as integer numbers, e.g. 3:2, 16:9, but not 2.35:1 */
    static int common_ratios_x[] = {1, 2, 3, 3, 4, 16, 5, 5};
    static int common_ratios_y[] = {1, 1, 1, 2, 3, 9,  4, 3};
    for (int i = 0; i < COUNT(common_ratios_x); i++)
    {
        int num = common_ratios_x[i];
        int den = common_ratios_y[i];
        float err = ABS((float)num / den - ratio);
        if (err < minerr)
        {
            minerr = err;
            best_num = num;
            best_den = den;
        }
    }
    
    if (minerr < 0.05)
    {
        int h = calc_res_y(res_x, best_num, best_den, squeeze_factor);
        /* if the difference is 1 pixel, consider it exact */
        char* qualifier = ABS(h - res_y) > 1 ? "almost " : "";
        snprintf(msg, sizeof(msg), "%s%d:%d", qualifier, best_num, best_den);
    }
    else if (ratio > 1)
    {
        int r = (int)roundf(ratio * 100);
        /* is it 2.35:1 or 2.353:1? */
        int h = calc_res_y(res_x, r, 100, squeeze_factor);
        char* qualifier = ABS(h - res_y) > 1 ? "almost " : "";
        if (r%100) snprintf(msg, sizeof(msg), "%s%d.%02d:1", qualifier, r/100, r%100);
    }
    else
    {
        int r = (int)roundf((1/ratio) * 100);
        int h = calc_res_y(res_x, 100, r, squeeze_factor);
        char* qualifier = ABS(h - res_y) > 1 ? "almost " : "";
        if (r%100) snprintf(msg, sizeof(msg), "%s1:%d.%02d", qualifier, r/100, r%100);
    }
    return msg;
}

/* how many frames it's likely to get at some write speed? */
static int sim_frames(int write_speed)
{

    /* how many frames we can have in RAM */
    int total = 0;
    int used = 0;
    for (int i = 0; i < buffer_count; i++)
        total += buffers[i].size / frame_size;

    int f = 0;
    int k = 0;
    float wt = 0;
    float fps = fps_get_current_x1000() / 1000.0;
    float t = 0;
    while (used < total && f < 10000)
    {
        t += 1/fps;
        f++;
        used++;
        
        int current_buf_cap = buffers[k].size / frame_size;

        /* can we write a chunk? enough data and previous write finished */
        if (used > current_buf_cap && t >= wt)
        {
            if (wt > 0)
            {
                /* we just wrote a chunk to card, so we have more free memory now */
                used -= current_buf_cap;
                k = (k + 1) % buffer_count;
            }
            else
            {
                /* first file write starts here */
                wt = t;
            }
            /* new write process starts now, wt is when it will finish */
            wt += (float)(frame_size * current_buf_cap) / write_speed;
        }
    }
    return f;
}

/* how many frames can we record with current settings, without dropping? */
static char* guess_how_many_frames()
{
    if (!measured_write_speed) return "";
    if (!buffer_count) return "";
    
    int write_speed_lo = measured_write_speed * 1024 * 1024 / 10 - 1 * 1024 * 1024;
    int write_speed_hi = measured_write_speed * 1024 * 1024 / 10 + 1 * 1024 * 1024;
    
    int f_lo = sim_frames(write_speed_lo);
    int f_hi = sim_frames(write_speed_hi);
    
    static char msg[50];
    if (f_lo < 5000)
    {
        if (f_lo != f_hi)
            snprintf(msg, sizeof(msg), "Expect %d-%d frames at %d-%dMB/s.", f_lo, f_hi, write_speed_lo / 1024 / 1024, write_speed_hi / 1024 / 1024);
        else
            snprintf(msg, sizeof(msg), "Expect around %d frames at %d-%dMB/s.", f_lo, write_speed_lo / 1024 / 1024, write_speed_hi / 1024 / 1024);
    }
    else
    {
        snprintf(msg, sizeof(msg), "Continuous recording OK.");
    }
    
    return msg;
}

static MENU_UPDATE_FUNC(write_speed_update)
{
    int fps = fps_get_current_x1000();
    int speed = (res_x * res_y * 14/8 / 1024) * fps / 100 / 1024;
    int ok = speed < measured_write_speed; 

    if (frame_size % 512)
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Frame size not multiple of 512 bytes!");
    }
    else
    {
        if (!measured_write_speed)
            MENU_SET_WARNING(ok ? MENU_WARN_INFO : MENU_WARN_ADVICE, 
                "Write speed needed: %d.%d MB/s at %d.%03d fps.",
                speed/10, speed%10, fps/1000, fps%1000
            );
        else
            MENU_SET_WARNING(ok ? MENU_WARN_INFO : MENU_WARN_ADVICE, 
                "%d.%d MB/s at %d.%03dp. %s",
                speed/10, speed%10, fps/1000, fps%1000,
                guess_how_many_frames()
            );
    }
}

static void update_shave()
{
    shave_right = FRAMING_LEFT ? (raw_info.width + shave_right - res_x - skip_x) / 8 * 8 : 0;
    raw_lv_shave_right(shave_right);
}

static void refresh_raw_settings(int force)
{
    if (RAW_IS_IDLE && !raw_playing && !raw_previewing)
    {
        /* autodetect the resolution (update 4 times per second) */
        static int aux = INT_MIN;
        if (force || should_run_polling_action(250, &aux))
        {
            if (raw_update_params())
            {
                update_resolution_params();
                update_shave();
            }
        }
    }
}

static MENU_UPDATE_FUNC(raw_main_update)
{
    if (!raw_video_enabled) return;
    
    refresh_raw_settings(0);

    if (auto_power_off_time)
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "\"Auto power off\" is enabled in Canon menu. Video may stop.");

    if (is_custom_movie_mode() && !is_native_movie_mode())
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "You are recording video in photo mode. Use expo override.");
    }

    if (!RAW_IS_IDLE)
    {
        MENU_SET_VALUE(RAW_IS_RECORDING ? "Recording..." : RAW_IS_PREPARING ? "Starting..." : RAW_IS_FINISHING ? "Stopping..." : "err");
        MENU_SET_ICON(MNI_RECORD, 0);
    }
    else
    {
        MENU_SET_VALUE("ON, %dx%d", res_x, res_y);
    }

    write_speed_update(entry, info);
}

static MENU_UPDATE_FUNC(aspect_ratio_update_info)
{
    if (squeeze_factor == 1.0f)
    {
        char* ratio = guess_aspect_ratio(res_x, res_y);
        MENU_SET_HELP("%dx%d (%s)", res_x, res_y, ratio);
    }
    else
    {
        int num = aspect_ratio_presets_num[aspect_ratio_index];
        int den = aspect_ratio_presets_den[aspect_ratio_index];
        int sq100 = (int)roundf(squeeze_factor*100);
        int res_y_corrected = calc_res_y(res_x, num, den, 1.0f);
        MENU_SET_HELP("%dx%d. Stretch by %s%d.%02dx to get %dx%d (%s) in post.", res_x, res_y, FMT_FIXEDPOINT2(sq100), res_x, res_y_corrected, aspect_ratio_choices[aspect_ratio_index]);
    }
}

static MENU_UPDATE_FUNC(resolution_update)
{
    if (!raw_video_enabled || !lv)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Enable RAW video first.");
        MENU_SET_VALUE("N/A");
        return;
    }
    
    refresh_raw_settings(1);

    int selected_x = resolution_presets_x[resolution_index_x];

    MENU_SET_VALUE("%dx%d", res_x, res_y);
    
    if (selected_x > max_res_x)
    {
        MENU_SET_HELP("%d is not possible in current video mode (max %d).", selected_x, max_res_x);
    }
    else
    {
        aspect_ratio_update_info(entry, info);
    }

    write_speed_update(entry, info);
}

static MENU_UPDATE_FUNC(aspect_ratio_update)
{
    if (!raw_video_enabled || !lv)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Enable RAW video first.");
        MENU_SET_VALUE("N/A");
        return;
    }
    
    refresh_raw_settings(0);

    int num = aspect_ratio_presets_num[aspect_ratio_index];
    int den = aspect_ratio_presets_den[aspect_ratio_index];
    int selected_y = calc_res_y(res_x, num, den, squeeze_factor);
    
    if (selected_y > max_res_y + 2)
    {
        char* ratio = guess_aspect_ratio(res_x, res_y * squeeze_factor);
        MENU_SET_VALUE(ratio);
        MENU_SET_HELP("Could not get %s. Max vertical resolution: %d.", aspect_ratio_choices[aspect_ratio_index], res_y);
    }
    else
    {
        aspect_ratio_update_info(entry, info);
    }
    write_speed_update(entry, info);
}

/* add a footer to given file handle to  */
static unsigned int lv_rec_save_footer(FILE *save_file)
{
    lv_rec_file_footer_t footer;
    
    strcpy((char*)footer.magic, "RAWM");
    footer.xRes = res_x;
    footer.yRes = res_y;
    footer.frameSize = frame_size;
    footer.frameCount = frame_count - 1; /* last frame is usually gibberish */
    footer.frameSkip = 1;
    
    footer.sourceFpsx1000 = fps_get_current_x1000();
    footer.raw_info = raw_info;

    int written = FIO_WriteFile(save_file, &footer, sizeof(lv_rec_file_footer_t));
    
    return written == sizeof(lv_rec_file_footer_t);
}

static unsigned int lv_rec_read_footer(FILE *f)
{
    lv_rec_file_footer_t footer;

    /* get current position in file, seek to footer, read and go back where we were */
    unsigned int old_pos = FIO_SeekFile(f, 0, 1);
    FIO_SeekFile(f, -sizeof(lv_rec_file_footer_t), SEEK_END);
    int read = FIO_ReadFile(f, &footer, sizeof(lv_rec_file_footer_t));
    FIO_SeekFile(f, old_pos, SEEK_SET);

    /* check if the footer was read */
    if(read != sizeof(lv_rec_file_footer_t))
    {
        bmp_printf(FONT_MED, 30, 190, "File position mismatch. Read %d", read);
        beep();
        msleep(1000);
    }
    
    /* check if the footer is in the right format */
    if(strncmp(footer.magic, "RAWM", 4))
    {
        bmp_printf(FONT_MED, 30, 190, "Footer format mismatch");
        beep();
        msleep(1000);
        return 0;
    }
        
    /* update global variables with data from footer */
    res_x = footer.xRes;
    res_y = footer.yRes;
    frame_count = footer.frameCount + 1;
    frame_size = footer.frameSize;
    // raw_info = footer.raw_info;
    raw_info.white_level = footer.raw_info.white_level;
    raw_info.black_level = footer.raw_info.black_level;
    
    return 1;
}

static int setup_buffers()
{
    /* allocate the entire memory, but only use large chunks */
    /* yes, this may be a bit wasteful, but at least it works */
    
    if (memory_hack) { PauseLiveView(); msleep(200); }
    
    mem_suite = shoot_malloc_suite(0);
    
    if (memory_hack) { ResumeLiveView(); msleep(500); }
    
    if (!mem_suite) return 0;
    
    /* allocate memory for double buffering */
    int buf_size = (raw_info.width + shave_right) * raw_info.height * 14/8 * 33/32; /* leave some margin, just in case */

    /* find the smallest chunk that we can use for buf_size */
    fullsize_buffers[0] = 0;
    struct memChunk * chunk = GetFirstChunkFromSuite(mem_suite);
    int waste = INT_MAX;
    while(chunk)
    {
        int size = GetSizeOfMemoryChunk(chunk);
        if (size >= buf_size)
        {
            if (size - buf_size < waste)
            {
                waste = size - buf_size;
                fullsize_buffers[0] = GetMemoryAddressOfMemoryChunk(chunk);
            }
        }
        chunk = GetNextMemoryChunk(mem_suite, chunk);
    }
    if (fullsize_buffers[0] == 0) return 0;
    
    /* reuse Canon's buffer */
    fullsize_buffers[1] = UNCACHEABLE(raw_info.buffer);
    if (fullsize_buffers[1] == 0) return 0;

    /* use all chunks larger than 16MB for recording */
    chunk = GetFirstChunkFromSuite(mem_suite);
    buffer_count = 0;
    int total = 0;
    while(chunk && buffer_count < COUNT(buffers))
    {
        int size = GetSizeOfMemoryChunk(chunk);
        if (size >= 16*1024*1024)
        {
            void* ptr = GetMemoryAddressOfMemoryChunk(chunk);
            if (ptr != fullsize_buffers[0])
            {
                /* make sure our buffers are aligned at 4K */
                buffers[buffer_count].ptr = (void*)(((intptr_t)ptr + 4095) & ~4095);
                buffers[buffer_count].size = size - 8192;
                buffers[buffer_count].used = 0;
                buffer_count++;
                total += size;
            }
        }
        chunk = GetNextMemoryChunk(mem_suite, chunk);
    }
    
    /* try to recycle the waste */
    if (waste >= 16*1024*1024 + 8192)
    {
        buffers[buffer_count].ptr = (void*)(((intptr_t)(fullsize_buffers[0] + buf_size) + 4095) & ~4095);
        buffers[buffer_count].size = waste - 8192;
        buffers[buffer_count].used = 0;
        buffer_count++;
        total += waste;
    }
    
    char msg[100];
    snprintf(msg, sizeof(msg), "Alloc: ");
    for (int i = 0; i < buffer_count; i++)
    {
        STR_APPEND(msg, "%dM", (buffers[i].size / 1024 + 512) / 1024);
        if (i < buffer_count-1) STR_APPEND(msg, "+");
    }
    bmp_printf(FONT_MED, 30, 90, msg);
    
    /* we need at least two buffers */
    if (buffer_count < 2) return 0;
    
    return 1;
}

static void free_buffers()
{
    if (mem_suite) shoot_free_suite(mem_suite);
    mem_suite = 0;
}

static void show_buffer_status()
{
    if (!liveview_display_idle()) return;
    
    int free_buffers = mod(saving_buffer_index - capturing_buffer_index, buffer_count); /* how many free slots do we have? */
    if (free_buffers == 0) free_buffers = buffer_count; /* saving task waiting for capturing task */
    
    /* could use a nicer display, but stars should be fine too */
    char buffer_status[10];
    for (int i = 0; i < (buffer_count-free_buffers); i++)
        buffer_status[i] = '*';
    for (int i = (buffer_count-free_buffers); i < buffer_count; i++)
        buffer_status[i] = '.';
    buffer_status[buffer_count] = 0;

    if(frame_skips > 0)
    {
        bmp_printf(FONT(FONT_MED, COLOR_RED, COLOR_BLACK), 30, 50, "Buffer usage: <%s>, %d skipped frames", buffer_status, frame_skips);
    }
    else
    {
        bmp_printf(FONT_MED, 30, 50, "Buffer usage: <%s>", buffer_status);
    }
}

static unsigned int raw_rec_should_preview(unsigned int ctx);

static void cropmark_draw()
{
    if (lv_dispsize > 1) return;
    
    int x = RAW2BM_X(skip_x);
    int y = RAW2BM_Y(skip_y);
    int w = RAW2BM_DX(res_x);
    int h = RAW2BM_DY(res_y);
    int p = raw_rec_should_preview(0);
    static int prev_x = 0;
    static int prev_y = 0;
    static int prev_w = 0;
    static int prev_h = 0;
    static int prev_p = 0;

    /* window changed? erase the old cropmark */
    if (prev_x != x || prev_y != y || prev_w != w || prev_h != h || prev_p != p)
    {
        bmp_draw_rect(0, prev_x, prev_y, prev_w, prev_h);
        bmp_draw_rect(0, prev_x-1, prev_y-1, prev_w+2, prev_h+2);
    }
    
    prev_x = x;
    prev_y = y;
    prev_w = w;
    prev_h = h;

    if (!p)
    {
        /* display a simple cropmark */
        bmp_draw_rect(COLOR_WHITE, x, y, w, h);
        bmp_draw_rect(COLOR_BLACK, x-1, y-1, w+2, h+2);
    }
}

static void panning_update()
{
    if (!FRAMING_PANNING) return;

    int sx = raw_info.active_area.x1 + (max_res_x - res_x) / 2;
    int sy = raw_info.active_area.y1 + (max_res_y - res_y) / 2;

    frame_offset_x = COERCE(
        frame_offset_x + frame_offset_delta_x, 
        raw_info.active_area.x1 - sx,
        raw_info.active_area.x1 + max_res_x - res_x - sx
    );
    
    frame_offset_y = COERCE(
        frame_offset_y + frame_offset_delta_y, 
        raw_info.active_area.y1 - sy,
        raw_info.active_area.y1 + max_res_y - res_y - sy
    );

    update_cropping_offsets();
}

static void raw_video_enable()
{
    /* toggle the lv_save_raw flag from raw.c */
    raw_lv_request();

    /* EOS-M needs this hack. Please don't use it unless there's no other way. */
    if (cam_eos_m) set_custom_movie_mode(1);
    
    msleep(50);
}

static void raw_video_disable()
{
    raw_lv_release();
    raw_lv_shave_right(0);
    if (cam_eos_m) set_custom_movie_mode(0);
}

static void raw_lv_request_update()
{
    static int raw_lv_requested = 0;

    if (raw_video_enabled && (is_movie_mode() || cam_eos_m))  /* exception: EOS-M needs to record in photo mode */
    {
        if (!raw_lv_requested)
        {
            raw_video_enable();
            raw_lv_requested = 1;
        }
    }
    else
    {
        if (raw_lv_requested)
        {
            raw_video_disable();
            raw_lv_requested = 0;
        }
    }
}


static unsigned int raw_rec_polling_cbr(unsigned int unused)
{
    raw_lv_request_update();
    
    if (!raw_video_enabled)
        return 0;
    
    if (!is_movie_mode())
        return 0;

    /* refresh cropmark (faster when panning, slower when idle) */
    static int aux = INT_MIN;
    if (frame_offset_delta_x || frame_offset_delta_y || should_run_polling_action(500, &aux))
    {
        if (liveview_display_idle())
        {
            BMP_LOCK( cropmark_draw(); )
        }
    }

    /* update settings when changing video modes (outside menu) */
    if (RAW_IS_IDLE && !gui_menu_shown())
    {
        refresh_raw_settings(0);
    }
    
    /* update status messages */
    static int auxrec = INT_MIN;
    if (RAW_IS_RECORDING && liveview_display_idle() && should_run_polling_action(1000, &auxrec))
    {
        int fps = fps_get_current_x1000();
        int t = (frame_count * 1000 + fps/2) / fps;
        bmp_printf( FONT_MED, 30, 70, 
            "%02d:%02d, %d frames...",
            t/60, t%60,
            frame_count
        );

        show_buffer_status();

        /* how fast are we writing? does this speed match our benchmarks? */
        if (writing_time)
        {
            int speed = written * 10 / writing_time * 1000 / 1024; // MB/s x10
            int idle_percent = idle_time * 100 / (writing_time + idle_time);
            measured_write_speed = speed;

            char msg[50];
            snprintf(msg, sizeof(msg),
                "%s: %d MB, %d.%d MB/s",
                chunk_filename + 17, /* skip A:/DCIM/100CANON/ */
                written / 1024,
                speed/10, speed%10
            );
            if (idle_time)
            {
                if (idle_percent) { STR_APPEND(msg, ", %d%% idle ", idle_percent); }
                else { STR_APPEND(msg, ", %dms idle ", idle_time); }
            }
            bmp_printf( FONT_MED, 30, 90, "%s", msg);
        }
    }

    return 0;
}

static void lv_unhack(int unused)
{
    while (!RAW_IS_IDLE) msleep(100);
    call("aewb_enableaewb", 1);
    PauseLiveView();
    ResumeLiveView();
}

static void hack_liveview()
{
    if (!PREVIEW_HACKED) return;
    
    int rec = RAW_IS_RECORDING;
    static int prev_rec = 0;
    int should_hack = 0;
    int should_unhack = 0;

    if (rec)
    {
        if (frame_count == 0)
            should_hack = 1;
        /*
        if (frame_count % 10 == 0)
            should_hack = 1;
        else if (frame_count % 10 == 9)
            should_unhack = 1;
        */
    }
    else if (prev_rec)
    {
        should_unhack = 1;
    }
    prev_rec = rec;
    
    if (should_hack)
    {
        call("aewb_enableaewb", 0);
        int y = 100;
        for (int channel = 0; channel < 32; channel++)
        {
            /* silence out the EDMACs used for HD and LV buffers */
            int pitch = edmac_get_length(channel) & 0xFFFF;
            if (pitch == vram_lv.pitch || pitch == vram_hd.pitch)
            {
                uint32_t reg = edmac_get_base(channel);
                bmp_printf(FONT_SMALL, 30, y += font_small.height, "Hack %x %dx%d ", reg, shamem_read(reg + 0x10) & 0xFFFF, shamem_read(reg + 0x10) >> 16);
                *(volatile uint32_t *)(reg + 0x10) = shamem_read(reg + 0x10) & 0xFFFF;
            }
        }
    }
    else if (should_unhack)
    {
        task_create("lv_unhack", 0x1e, 0x1000, lv_unhack, (void*)0);
    }
}

static int FAST process_frame()
{
    if (!lv) return 0;
    
    /* skip the first frame, it will be gibberish */
    if (frame_count == 0) { frame_count++; return 0; }
    
    /* copy current frame to our buffer and crop it to its final size */
    void* ptr = buffers[capturing_buffer_index].ptr + capture_offset;
    void* fullSizeBuffer = fullsize_buffers[(fullsize_buffer_pos+1) % 2];

    /* advance to next buffer for the upcoming capture */
    fullsize_buffer_pos = (fullsize_buffer_pos + 1) % 2;
    
    /* dont process this frame if a module wants to skip that */
    if(raw_rec_cbr_skip_frame(fullSizeBuffer))
    {
        return 0;
    }
    
    /* try a sync beep */
    if (sound_rec == 2 && frame_count == 1)
    {
        beep();
    }

    int ans = edmac_copy_rectangle_start(ptr, fullSizeBuffer, raw_info.pitch, (skip_x+7)/8*14, skip_y/2*2, res_x*14/8, res_y);

    /* advance to next frame */
    frame_count++;
    capture_offset += frame_size;
    
    return ans;
}

static unsigned int FAST raw_rec_vsync_cbr(unsigned int unused)
{
    static int dma_transfer_in_progress = 0;
    /* there may be DMA transfers started in process_frame, finish them */
    /* let's assume they are faster than LiveView refresh rate (well, they HAVE to be) */
    if (dma_transfer_in_progress)
    {
        edmac_copy_rectangle_finish();
        dma_transfer_in_progress = 0;
    }

    if (!raw_video_enabled) return 0;
    if (!is_movie_mode()) return 0;

    hack_liveview();
 
    /* panning window is updated when recording, but also when not recording */
    panning_update();

    if (!RAW_IS_RECORDING) return 0;
    if (!raw_lv_settings_still_valid()) { raw_recording_state = RAW_FINISHING; return 0; }
    if (!allow_frame_skip && frame_skips) return 0;

    /* double-buffering */
    raw_lv_redirect_edmac(fullsize_buffers[fullsize_buffer_pos % 2]);
    
    if (capture_offset + frame_size >= buffers[capturing_buffer_index].size)
    {
        /* this buffer is full, try next one */
        int next_buffer = mod(capturing_buffer_index + 1, buffer_count);
        if (next_buffer != saving_buffer_index)
        {
            buffers[capturing_buffer_index].used = capture_offset;
            capturing_buffer_index = next_buffer;
            capture_offset = 0;
        }
        else if (raw_rec_cbr_skip_buffer(next_buffer, buffers[next_buffer].size / frame_size, buffer_count))
        {
            /* our buffer list wrapped over, but this is intentional as an other module requests this mode of operation */
            buffers[capturing_buffer_index].used = capture_offset;
            capturing_buffer_index = next_buffer;
            capture_offset = 0;
            
            /* decrease frame count by the amount saved in skipped buffer */
            frame_count -= buffers[next_buffer].size / frame_size;
            saving_buffer_index = mod(saving_buffer_index + 1, buffer_count);
        }
        else
        {
            /* card too slow */
            frame_skips++;
            if (allow_frame_skip)
            {
                bmp_printf( FONT_MED, 30, 70, 
                    "Skipping frames...   "
                );
            }
            return 0;
        }
    }
    
    dma_transfer_in_progress = process_frame();

    return 0;
}

static char* get_next_raw_movie_file_name()
{
    static char filename[100];

    struct tm now;
    LoadCalendarFromRTC(&now);

    for (int number = 0 ; number < 100; number++)
    {
        /**
         * Get unique file names from the current date/time
         * last field gets incremented if there's another video with the same name
         */
        snprintf(filename, sizeof(filename), "%s/M%02d-%02d%02d.RAW", get_dcim_dir(), now.tm_mday, now.tm_hour, COERCE(now.tm_min + number, 0, 99));
        
        /* already existing file? */
        uint32_t size;
        if( FIO_GetFileSize( filename, &size ) != 0 ) break;
        if (size == 0) break;
    }
    
    return filename;
}

static char* get_next_chunk_file_name(char* base_name, int chunk)
{
    static char filename[100];

    /* change file extension, according to chunk number: RAW, R00, R01 and so on */
    snprintf(filename, sizeof(filename), "%s", base_name);
    int len = strlen(filename);
    snprintf(filename + len - 2, 3, "%02d", chunk-1);
    
    return filename;
}

static char* get_wav_file_name(char* movie_filename)
{
    /* same name as movie, but with wav extension */
    static char wavfile[100];
    snprintf(wavfile, sizeof(wavfile), movie_filename);
    int len = strlen(wavfile);
    wavfile[len-4] = '.';
    wavfile[len-3] = 'W';
    wavfile[len-2] = 'A';
    wavfile[len-1] = 'V';
    /* prefer SD card for saving WAVs (should be faster on 5D3) */
    if (is_dir("B:/")) wavfile[0] = 'B';
    return wavfile;
}

static void raw_video_rec_task()
{
    /* init stuff */
    raw_recording_state = RAW_PREPARING;
    buffer_count = 0;
    capturing_buffer_index = 0;
    saving_buffer_index = 0;
    fullsize_buffer_pos = 0;
    capture_offset = 0;
    frame_count = 0;
    frame_skips = 0;
    frame_offset_delta_x = 0;
    frame_offset_delta_y = 0;
    FILE* f = 0;
    written = 0; /* in KB */
    uint32_t written_chunk = 0; /* in bytes, for current chunk */

    /* disable canon graphics (gains a little speed) */
    int canon_gui = !canon_gui_front_buffer_disabled();
    canon_gui_disable_front_buffer();

    /* create a backup file, to make sure we can save the file footer even if the card is full */
    char backup_filename[100];
    snprintf(backup_filename, sizeof(backup_filename), "%s/backup.raw", get_dcim_dir());
    FILE* bf = FIO_CreateFileEx(backup_filename);
    if (bf == INVALID_PTR)
    {
        bmp_printf( FONT_MED, 30, 50, "File create error");
        goto cleanup;
    }
    FIO_WriteFile(bf, (void*)0x40000000, 512*1024);
    FIO_CloseFile(bf);
    
    
    /* create output file */
    int chunk = 0;
    movie_filename = get_next_raw_movie_file_name();
    chunk_filename = movie_filename;
    f = FIO_CreateFileEx(movie_filename);
    if (f == INVALID_PTR)
    {
        bmp_printf( FONT_MED, 30, 50, "File create error");
        goto cleanup;
    }
    
    /* detect raw parameters (geometry, black level etc) */
    if (!raw_update_params())
    {
        bmp_printf( FONT_MED, 30, 50, "Raw detect error");
        goto cleanup;
    }

    /* allocate memory */
    if (!setup_buffers())
    {
        bmp_printf( FONT_MED, 30, 50, "Memory error");
        goto cleanup;
    }

    if (sound_rec == 1)
    {
        char* wavfile = get_wav_file_name(movie_filename);
        bmp_printf( FONT_MED, 30, 90, "Sound: %s%s", wavfile + 17, wavfile[0] == 'B' && movie_filename[0] == 'A' ? " on SD card" : "");
        bmp_printf( FONT_MED, 30, 90, "%s", wavfile);
        WAV_StartRecord(wavfile);
    }
    
    /* this will enable the vsync CBR and the other task(s) */
    raw_recording_state = RAW_RECORDING;

    /* signal that we are starting */
    raw_rec_cbr_starting();
    
    writing_time = 0;
    idle_time = 0;
    int last_write_timestamp = 0;
    
    /* fake recording status, to integrate with other ml stuff (e.g. hdr video */
    recording = -1;
    
    /* main recording loop */
    while (RAW_IS_RECORDING && lv)
    {
        if (!allow_frame_skip && frame_skips)
            goto abort;
            
        /* do we have any buffers completely filled with data, that we can save? */
        int used_buffers = (capturing_buffer_index + buffer_count - saving_buffer_index) % buffer_count;
        int ext_gating = raw_rec_cbr_save_buffer(used_buffers, saving_buffer_index, buffers[saving_buffer_index].size / frame_size, buffer_count);

        /* ask an optional external routine if this buffer should get saved now. if none registered, it will return 1 */
        if(used_buffers > 0 && ext_gating)
        {
            void* ptr = buffers[saving_buffer_index].ptr;
            int size_used = buffers[saving_buffer_index].used;

            int t0 = get_ms_clock_value();
            if (!last_write_timestamp) last_write_timestamp = t0;
            idle_time += t0 - last_write_timestamp;
            int r = FIO_WriteFile(f, ptr, size_used);
            last_write_timestamp = get_ms_clock_value();

            if (r != size_used) /* 4GB limit or card full? */
            {
                /* it failed right away? card must be full */
                if (written == 0) goto abort;

                if (r == -1)
                {
                    /* 4GB limit? it stops after writing 4294967295 bytes, but FIO_WriteFile may return -1 */
                    //if ((uint64_t)written_chunk + size_used > 4294967295)
                    if (1) // renato says it's not working?!
                    {
                        r = 4294967295 - written_chunk;
                        
                        /* 5D2 does not write anything if the call failed, but 5D3 writes exactly 4294967295 */
                        /* this one should cover both cases in a portable way */
                        /* on 5D2 will succeed, on 5D3 should fail right away */
                        FIO_WriteFile(f, ptr, r);
                    }
                    else /* idk */
                    {
                        r = 0;
                    }
                }
                
                /* try to create a new chunk */
                chunk_filename = get_next_chunk_file_name(movie_filename, ++chunk);
                FILE* g = FIO_CreateFileEx(chunk_filename);
                if (g == INVALID_PTR) goto abort;
                
                /* write the remaining data in the new chunk */
                int r2 = FIO_WriteFile(g, ptr + r, size_used - r);
                if (r2 == size_used - r) /* new chunk worked, continue with it */
                {
                    FIO_CloseFile(f);
                    f = g;
                    written += size_used / 1024;
                    written_chunk = r2;
                }
                else /* new chunk didn't work, card full */
                {
                    /* let's hope we can still save the footer in the current chunk (don't create a new one) */
                    FIO_CloseFile(g);
                    FIO_RemoveFile(chunk_filename);
                    chunk--;
                    goto abort;
                }
            }
            else
            {
                /* all fine */
                written += size_used / 1024;
                written_chunk += size_used;
            }
            
            writing_time += last_write_timestamp - t0;
            saving_buffer_index = mod(saving_buffer_index + 1, buffer_count);
        }
        else
        {
            /* to be verified if it is okay to sleep when the buffers are empty */
            msleep(20);
        }

        /* error handling */
        if (0)
        {
abort:
            bmp_printf( FONT_MED, 30, 90, 
                "Movie recording stopped automagically"
            );
            beep_times(2);
            break;
        }
    }
    
    /* signal that we are stopping */
    raw_rec_cbr_stopping();
    
    /* done, this will stop the vsync CBR and the copying task */
    raw_recording_state = RAW_FINISHING;

    /* wait until the other tasks calm down */
    msleep(500);

    recording = 0;

    if (sound_rec == 1)
    {
        WAV_StopRecord();
    }

    bmp_printf( FONT_MED, 30, 70, 
        "Frames captured: %d    ", 
        frame_count - 1
    );

    /* write remaining frames */
    while (saving_buffer_index != capturing_buffer_index)
    {
        show_buffer_status();
        written += FIO_WriteFile(f, buffers[saving_buffer_index].ptr, buffers[saving_buffer_index].used) / 1024;
        saving_buffer_index = mod(saving_buffer_index + 1, buffer_count);
    }
    written += FIO_WriteFile(f, buffers[capturing_buffer_index].ptr, capture_offset) / 1024;

    /* remove the backup file, to make sure we can save the footer even if card is full */
    msleep(500);
    FIO_RemoveFile(backup_filename);
    msleep(500);

    if (written && f)
    {
        /* write footer (metadata) */
        int footer_ok = lv_rec_save_footer(f);
        if (!footer_ok)
        {
            /* try to save footer in a new chunk */
            FIO_CloseFile(f); f = 0;
            chunk_filename = get_next_chunk_file_name(movie_filename, ++chunk);
            FILE* g = FIO_CreateFileEx(chunk_filename);
            if (g != INVALID_PTR)
            {
                footer_ok = lv_rec_save_footer(g);
                FIO_CloseFile(g);
            }
        }

        /* still didn't succeed? */
        if (!footer_ok)
        {
            bmp_printf( FONT_MED, 30, 110, 
                "Footer save error"
            );
            beep_times(3);
            msleep(2000);
        }
    }
    else
    {
        bmp_printf( FONT_MED, 30, 110, 
            "Nothing saved, card full maybe."
        );
        beep_times(3);
        msleep(2000);
    }

cleanup:
    if (f) FIO_CloseFile(f);
    if (!written) { FIO_RemoveFile(movie_filename); movie_filename = 0; }
    FIO_RemoveFile(backup_filename);
    free_buffers();
    if (canon_gui) canon_gui_enable_front_buffer(0);
    redraw();
    raw_recording_state = RAW_IDLE;
}

static MENU_SELECT_FUNC(raw_start_stop)
{
    if (!RAW_IS_IDLE)
    {
        raw_recording_state = RAW_FINISHING;
    }
    else
    {
        raw_recording_state = RAW_PREPARING;
        gui_stop_menu();
        task_create("raw_rec_task", 0x19, 0x1000, raw_video_rec_task, (void*)0);
    }
}

static void raw_video_playback_task()
{
    void* buf = NULL;
    FILE* f = INVALID_PTR;

    /* prepare display */
    SetGUIRequestMode(1);
    msleep(1000);
    ui_lock(UILOCK_EVERYTHING & ~1); /* everything but shutter */
    clrscr();

    if (!movie_filename)
        goto cleanup;
    
    f = FIO_Open( movie_filename, O_RDONLY | O_SYNC );
    if( f == INVALID_PTR )
    {
        beep();
        bmp_printf(FONT_MED, 0, 0, "Failed to open file '%s' ", movie_filename);
        msleep(2000);
        goto cleanup;
    }
    
    /* read footer information and update global variables, will seek automatically */
    lv_rec_read_footer(f);

    shave_right = 0;
    raw_lv_shave_right(0);
    raw_set_geometry(res_x, res_y, 0, 0, 0, 0);
    
    /* don't use raw_info.frame_size, use the one from the footer instead
     * (which should be greater or equal, because of rounding) */
    ASSERT(raw_info.frame_size <= frame_size);
    
    buf = shoot_malloc(frame_size);
    if (!buf)
        goto cleanup;

    vram_clear_lv();
    
    for (int i = 0; i < frame_count-1; i++)
    {
        bmp_printf(FONT_MED, os.x_max - font_med.width*10, os.y_max - 20, "%d/%d", i+1, frame_count-1);
        bmp_printf(FONT_MED, 0, os.y_max - font_med.height, "%s: %dx%d", movie_filename, res_x, res_y);
        int r = FIO_ReadFile(f, buf, frame_size);
        if (r != frame_size)
            break;
        
        if (get_halfshutter_pressed())
            break;

        if (gui_state != GUISTATE_PLAYMENU)
            break;

        raw_info.buffer = buf;
        raw_set_geometry(res_x, res_y, 0, 0, 0, 0);
        raw_force_aspect_ratio_1to1();
        raw_preview_fast();
    }

cleanup:
    vram_clear_lv();
    if (f != INVALID_PTR) FIO_CloseFile(f);
    if (buf) shoot_free(buf);
    raw_playing = 0;
    SetGUIRequestMode(0);
    ui_lock(UILOCK_NONE);
}

static void raw_video_playback(char *filename)
{
    movie_filename = filename;
    raw_playing = 1;
    gui_stop_menu();
    
    task_create("raw_rec_task", 0x1e, 0x1000, raw_video_playback_task, (void*)0);
}

FILETYPE_HANDLER(raw_rec_filehandler)
{
    /* there is no header and clean interface yet */
    switch(cmd)
    {
        case FILEMAN_CMD_INFO:
            strcpy(data, "A 14-bit RAW Video");
            return 1;
        case FILEMAN_CMD_VIEW_OUTSIDE_MENU:
            raw_video_playback(filename);
            return 1;
    }
    return 0; /* command not handled */
}

static MENU_SELECT_FUNC(raw_playback_start)
{
    if (!raw_playing && RAW_IS_IDLE)
    {
        if (!movie_filename)
        {
            bmp_printf(FONT_MED, 20, 50, "Please record a movie first.");
            return;
        }
        raw_playing = 1;
        gui_stop_menu();
        task_create("raw_rec_task", 0x1e, 0x1000, raw_video_playback_task, (void*)0);
    }
}

static MENU_UPDATE_FUNC(raw_playback_update)
{
    if (movie_filename)
        MENU_SET_VALUE(movie_filename + 17);
    else
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Record a video clip first.");
}

static struct menu_entry raw_video_menu[] =
{
    {
        .name = "RAW video",
        .priv = &raw_video_enabled,
        .max = 1,
        .update = raw_main_update,
        .submenu_width = 710,
        .depends_on = DEP_LIVEVIEW | DEP_MOVIE_MODE,
        .help = "Record 14-bit RAW video. Press LiveView to start.",
        .children =  (struct menu_entry[]) {
            {
                .name = "Resolution",
                .priv = &resolution_index_x,
                .max = COUNT(resolution_presets_x) - 1,
                .update = resolution_update,
                .choices = RESOLUTION_CHOICES_X,
            },
            {
                .name = "Aspect ratio",
                .priv = &aspect_ratio_index,
                .max = COUNT(aspect_ratio_presets_num) - 1,
                .update = aspect_ratio_update,
                .choices = aspect_ratio_choices,
            },
            /*
            {
                .name = "Source ratio",
                .priv = &source_ratio,
                .max = 2,
                .choices = CHOICES("Square pixels", "16:9", "3:2"),
                .help  = "Choose aspect ratio of the source image (LiveView buffer).",
                .help2 = "Useful for video modes with squeezed image (e.g. 720p).",
            },
            */
            {
                .name = "Sound",
                .priv = &sound_rec,
                .max = 2,
                .choices = CHOICES("OFF", "Separate WAV", "Sync beep"),
                .help = "Sound recording options.",
            },
            {
                .name = "Frame skipping",
                .priv = &allow_frame_skip,
                .max = 1,
                .choices = CHOICES("OFF", "Allow"),
                .help = "Enable if you don't mind skipping frames (for slow cards).",
            },
            {
                .name = "Framing",
                .priv = &framing_mode,
                .max = 2,
                .choices = CHOICES("Center", "Force Left", "Dolly mode"),
                .help = "Choose how to frame recorded the image.",
                .help2 = "Center: most intuitive, but not the fastest.\n"
                         "Force Left: we can chop off the right side for higher speed.\n"
                         "Dolly: smooth panning of the recording window, with arrows."
            },
            {
                .name = "Preview",
                .priv = &preview_mode,
                .max = 3,
                .choices = CHOICES("Auto", "Canon", "ML Grayscale", "HaCKeD"),
                .help2 = "Auto: ML chooses what's best for each video mode\n"
                         "Canon: plain old LiveView. Framing is not always correct.\n"
                         "ML Grayscale: looks ugly, but at least framing is correct.\n"
                         "HaCKeD: try to squeeze a little speed by killing LiveView.\n"
            },
            {
                .name = "Memory hack",
                .priv = &memory_hack,
                .max = 1,
                .help = "Allocate memory with LiveView off. On 5D3 => 2x32M extra.",
            },
            {
                .name = "Playback",
                .select = raw_playback_start,
                .update = raw_playback_update,
                .icon_type = IT_ACTION,
                .help = "Play back the last raw video clip.",
            },
            MENU_EOL,
        },
    }
};


static unsigned int raw_rec_keypress_cbr(unsigned int key)
{
    if (!raw_video_enabled)
        return 1;

    if (!is_movie_mode())
        return 1;

    /* keys are only hooked in LiveView */
    if (!liveview_display_idle())
        return 1;
    
    /* start/stop recording with the LiveView key */
    int rec_key_pressed = (key == MODULE_KEY_LV || key == MODULE_KEY_REC);
    
    /* ... or SET on 5D2/50D */
    if (cam_50d || cam_5d2) rec_key_pressed = (key == MODULE_KEY_PRESS_SET);
    
    if (rec_key_pressed)
    {
        switch(raw_recording_state)
        {
            case RAW_IDLE:
            case RAW_RECORDING:
                raw_start_stop(0,0);
                break;
        }
        return 0;
    }
    
    /* panning (with arrow keys) */
    if (FRAMING_PANNING)
    {
        switch (key)
        {
            case MODULE_KEY_PRESS_LEFT:
                frame_offset_delta_x -= 8;
                return 0;
            case MODULE_KEY_PRESS_RIGHT:
                frame_offset_delta_x += 8;
                return 0;
            case MODULE_KEY_PRESS_UP:
                frame_offset_delta_y -= 2;
                return 0;
            case MODULE_KEY_PRESS_DOWN:
                frame_offset_delta_y += 2;
                return 0;
            case MODULE_KEY_PRESS_DOWN_LEFT:
                frame_offset_delta_y += 2;
                frame_offset_delta_x -= 8;
                return 0;
            case MODULE_KEY_PRESS_DOWN_RIGHT:
                frame_offset_delta_y += 2;
                frame_offset_delta_x += 8;
                return 0;
            case MODULE_KEY_PRESS_UP_LEFT:
                frame_offset_delta_y -= 2;
                frame_offset_delta_x -= 8;
                return 0;
            case MODULE_KEY_PRESS_UP_RIGHT:
                frame_offset_delta_y -= 2;
                frame_offset_delta_x += 8;
                return 0;
            case MODULE_KEY_JOY_CENTER:
                /* first click stop the motion, second click center the window */
                if (frame_offset_delta_x || frame_offset_delta_y)
                {
                    frame_offset_delta_y = 0;
                    frame_offset_delta_x = 0;
                }
                else
                {
                    frame_offset_y = 0;
                    frame_offset_x = 0;
                }
        }
    }
    
    return 1;
}

static int preview_dirty = 0;

static unsigned int raw_rec_should_preview(unsigned int ctx)
{
    if (!raw_video_enabled) return 0;
    if (!is_movie_mode()) return 0;

    /* keep x10 mode unaltered, for focusing */
    if (lv_dispsize == 10) return 0;
    
    if (PREVIEW_AUTO)
        /* enable preview in x5 mode, since framing doesn't match */
        return lv_dispsize == 5;

    else if (PREVIEW_CANON)
        return 0;
    
    else if (PREVIEW_ML)
        return 1;
    
    else if (PREVIEW_HACKED)
        return RAW_IS_RECORDING || get_halfshutter_pressed() || lv_dispsize == 5;
    
    return 0;
}

static unsigned int raw_rec_update_preview(unsigned int ctx)
{
    /* just say whether we can preview or not */
    if (ctx == 0)
    {
        int enabled = raw_rec_should_preview(ctx);
        if (!enabled && preview_dirty)
        {
            /* cleanup the mess, if any */
            raw_set_dirty();
            preview_dirty = 0;
        }
        return enabled;
    }
    
    struct display_filter_buffers * buffers = (struct display_filter_buffers *) ctx;

    raw_previewing = 1;
    raw_set_preview_rect(skip_x, skip_y, res_x, res_y);
    raw_force_aspect_ratio_1to1();
    raw_preview_fast_ex((void*)-1, PREVIEW_HACKED && RAW_RECORDING ? (void*)-1 : buffers->dst_buf, -1, -1, !get_halfshutter_pressed());
    raw_previewing = 0;

    if (!RAW_IS_IDLE)
    {
        /* be gentle with the CPU, save it for recording (especially if the buffer is almost full) */
        int free_buffers = mod(saving_buffer_index - capturing_buffer_index, buffer_count);
        if (free_buffers == 0) free_buffers = buffer_count;
        int used_buffers = buffer_count - free_buffers;
        msleep(free_buffers <= 2 ? 2000 : used_buffers > 1 ? 1000 : 100);
    }

    preview_dirty = 1;
    return 1;
}

static unsigned int raw_rec_init()
{
    cam_eos_m = streq(camera_model_short, "EOSM");
    cam_5d2 = streq(camera_model_short, "5D2");
    cam_50d = streq(camera_model_short, "50D");
    cam_5d3 = streq(camera_model_short, "5D3");
    cam_6d = streq(camera_model_short, "6D");
    
    for (struct menu_entry * e = raw_video_menu[0].children; !MENU_IS_EOL(e); e++)
    {
        /* customize menus for each camera here (e.g. hide what doesn't work) */
        
        /* 50D doesn't have sound and can't even beep */
        if (cam_50d && streq(e->name, "Sound"))
            e->shidden = 1;

        /* Memory hack confirmed to work only on 5D3 and 6D */
        if (streq(e->name, "Memory hack") && !(cam_5d3 || cam_6d))
            e->shidden = 1;
    }

    if (cam_5d2 || cam_50d)
       raw_video_menu[0].help = "Record 14-bit RAW video. Press SET to start.";

    menu_add("Movie", raw_video_menu, COUNT(raw_video_menu));
    fileman_register_type("RAW", "RAW Video", raw_rec_filehandler);
    
    return 0;
}

static unsigned int raw_rec_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(raw_rec_init)
    MODULE_DEINIT(raw_rec_deinit)
MODULE_INFO_END()

MODULE_STRINGS_START()
    MODULE_STRING("Description", "14-bit RAW video")
    MODULE_STRING("License", "GPL")
    MODULE_STRING("Author", "a1ex")
    MODULE_STRING("Credits", "g3gg0 (lv_rec)")
MODULE_STRINGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_VSYNC, raw_rec_vsync_cbr, 0)
    MODULE_CBR(CBR_KEYPRESS, raw_rec_keypress_cbr, 0)
    MODULE_CBR(CBR_SHOOT_TASK, raw_rec_polling_cbr, 0)
    MODULE_CBR(CBR_DISPLAY_FILTER, raw_rec_update_preview, 0)
MODULE_CBRS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(raw_video_enabled)
    MODULE_CONFIG(resolution_index_x)
    MODULE_CONFIG(aspect_ratio_index)
    MODULE_CONFIG(measured_write_speed)
    MODULE_CONFIG(allow_frame_skip)
    MODULE_CONFIG(sound_rec)
    MODULE_CONFIG(framing_mode)
    MODULE_CONFIG(preview_mode)
    MODULE_CONFIG(memory_hack)
MODULE_CONFIGS_END()
