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

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include "../lv_rec/lv_rec.h"
#include "edmac.h"

/* when enabled, it hooks shortcut keys */
static int raw_video_enabled = 0;

/**
 * resolution should be multiple of 64x32 or 128x16
 * this way, we get frame size multiple of 512, so there's no write speed penalty
 * (see http://chdk.setepontos.com/index.php?topic=9970 ; confirmed by benchmarks)
 * mod16 request: http://www.magiclantern.fm/forum/index.php?topic=5839.0
 **/

static int resolution_presets_x[] = {  640,  768,  960,  1280,  1344,  1472,  1600,  1728,  1856,  1920,  2048,  2560,  2880,  3584 };
#define  RESOLUTION_CHOICES_X CHOICES("640","768","960","1280","1344","1472","1600","1728","1856","1920","2048","2560","2880","3584")

static int aspect_ratio_presets_num[]      = {    3,       8,      25,     239,     235,      22,    2,     185,     16,    5,    3,    4,    1};
static int aspect_ratio_presets_den[]      = {    1,       3,      10,     100,     100,      10,    1,     100,      9,    3,    2,    3,    1};
static const char * aspect_ratio_choices[] = { "3:1","2.67:1","2.50:1","2.39:1","2.35:1","2.20:1","2:1","1.85:1", "16:9","5:3","3:2","4:3","1:1"};

//~ static CONFIG_INT("raw.res.x", resolution_index_x, 2);
//~ static CONFIG_INT("raw.res.y", resolution_index_y, 4);
//~ static CONFIG_INT("raw.write.spd", measured_write_speed, 0);

/* no config options yet */
static int resolution_index_x = 9;
static int aspect_ratio_index = 8;
static int measured_write_speed = 0;
static int stop_on_buffer_overflow = 1;
static int sound_rec = 2;
static int panning_enabled = 0;
static int hacked_mode = 0;

#define RAW_IDLE      0
#define RAW_PREPARING 1
#define RAW_RECORDING 2
#define RAW_FINISHING 3

static int raw_recording_state = RAW_IDLE;

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
static struct buff buffers[10];                   /* our recording buffers */
static int buffer_count = 0;                      /* how many buffers we could allocate */
static int capturing_buffer_index = 0;            /* in which buffer we are capturing */
static int saving_buffer_index = 0;               /* from which buffer we are saving to card */
static int capture_offset = 0;                    /* position of capture pointer inside the buffer (0-32MB) */
static int frame_count = 0;                       /* how many frames we have processed */
static int frame_skips = 0;                       /* how many frames were dropped/skipped */
static char* movie_filename = 0;                  /* file name for current (or last) movie */

static float get_squeeze_factor()
{
    if (video_mode_resolution == 1 && lv_dispsize == 1 && is_movie_mode()) /* 720p, image squeezed */
    {
        /* assume the raw image should be 16:9 when de-squeezed */
        int correct_height = raw_info.jpeg.width * 9 / 16;
        return (float)correct_height / raw_info.jpeg.height;
    }
    return 1.0f;
}

static int get_res_x()
{
    /* make sure we don't get dead pixels from rounding */
    int left_margin = (raw_info.active_area.x1 + 7) / 8 * 8;
    int right_margin = (raw_info.active_area.x2) / 8 * 8;
    
    ASSERT(resolution_presets_x[resolution_index_x] % 64 == 0);
    
    int max = (right_margin - left_margin) & ~15;
    while (max % 16 || (max * 14/8) % 16) max--;

    return MIN(resolution_presets_x[resolution_index_x], max);
}

static int calc_res_y(int res_x, int num, int den, float squeeze)
{
    int rounding_mask = res_x % 128 ? 31 : 15;
    if (squeeze != 1.0f)
    {
        /* image should be enlarged vertically in post by a factor equal to "squeeze" */
        return (int)(roundf(res_x * den / num / squeeze) + rounding_mask) & ~rounding_mask;
    }
    else
    {
        /* assume square pixels */
        return (res_x * den / num + rounding_mask) & ~rounding_mask;
    }
}

static int get_res_y()
{
    int res_x = get_res_x();
    int num = aspect_ratio_presets_num[aspect_ratio_index];
    int den = aspect_ratio_presets_den[aspect_ratio_index];
    float squeeze = get_squeeze_factor();
    int rounding_mask = res_x % 128 ? 31 : 15;
    return MIN(calc_res_y(res_x, num, den, squeeze), raw_info.jpeg.height & ~rounding_mask);
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
        int h = calc_res_y(res_x, best_num, best_den, get_squeeze_factor());
        char* qualifier = h != res_y ? "almost " : "";
        snprintf(msg, sizeof(msg), "%s%d:%d", qualifier, best_num, best_den);
    }
    else if (ratio > 1)
    {
        int r = (int)roundf(ratio * 100);
        int r2 = (int)roundf(ratio * 1000);
        /* is it 2.35:1 or 2.353:1? */
        char* qualifier = ABS(r2 - r * 10) >= 1 ? "almost " : "";
        if (r%100) snprintf(msg, sizeof(msg), "%s%d.%02d:1", qualifier, r/100, r%100);
    }
    else
    {
        int r = (int)roundf((1/ratio) * 100);
        int r2 = (int)roundf((1/ratio) * 1000);
        char* qualifier = ABS(r2 - r * 10) >= 1 ? "almost " : "";
        if (r%100) snprintf(msg, sizeof(msg), "%s1:%d.%02d", qualifier, r/100, r%100);
    }
    return msg;
}

static MENU_UPDATE_FUNC(write_speed_update)
{
    int res_x = get_res_x();
    int res_y = get_res_y();
    int fps = fps_get_current_x1000();
    int speed = (res_x * res_y * 14/8 / 1024) * fps / 100 / 1024;
    int ok = speed < measured_write_speed; 

    if ((res_x * res_y * 14/8) % 512)
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Frame size not multiple of 512 bytes!");
    }
    else
    {
        MENU_SET_WARNING(ok ? MENU_WARN_INFO : MENU_WARN_ADVICE, 
            "Write speed needed: %d.%d MB/s at %d.%03d fps.",
            speed/10, speed%10, fps/1000, fps%1000
        );
    }
}

static void refresh_raw_settings()
{
    if (RAW_IS_IDLE)
    {
        /* autodetect the resolution (update every second) */
        static int aux = INT_MIN;
        if (should_run_polling_action(1000, &aux))
        {
            raw_update_params();
        }
    }
}
static MENU_UPDATE_FUNC(raw_main_update)
{
    if (!raw_video_enabled) return;
    
    refresh_raw_settings();
    
    if (!RAW_IS_IDLE)
    {
        MENU_SET_VALUE(RAW_IS_RECORDING ? "Recording..." : RAW_IS_PREPARING ? "Starting..." : RAW_IS_FINISHING ? "Stopping..." : "err");
        MENU_SET_ICON(MNI_RECORD, 0);
    }
    else
    {
        MENU_SET_VALUE("ON, %dx%d", get_res_x(), get_res_y());
    }

    write_speed_update(entry, info);
}

static MENU_UPDATE_FUNC(aspect_ratio_update_info)
{
    int res_x = get_res_x();
    int res_y = get_res_y();
    float squeeze = get_squeeze_factor();
    if (squeeze == 1.0f)
    {
        char* ratio = guess_aspect_ratio(res_x, res_y);
        MENU_SET_HELP("%dx%d (%s)", res_x, res_y, ratio);
    }
    else
    {
        int num = aspect_ratio_presets_num[aspect_ratio_index];
        int den = aspect_ratio_presets_den[aspect_ratio_index];
        int sq100 = (int)roundf(squeeze*100);
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
    
    refresh_raw_settings();

    int res_x = get_res_x();
    int res_y = get_res_y();
    int selected_x = resolution_presets_x[resolution_index_x];

    MENU_SET_VALUE("%dx%d", res_x, res_y);
    
    if (selected_x != res_x)
    {
        MENU_SET_HELP("%d is not possible in current video mode (max %d).", selected_x, res_x);
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
    
    refresh_raw_settings();

    int res_x = get_res_x();
    int res_y = get_res_y();
    int num = aspect_ratio_presets_num[aspect_ratio_index];
    int den = aspect_ratio_presets_den[aspect_ratio_index];
    float squeeze = get_squeeze_factor();
    int selected_y = calc_res_y(res_x, num, den, squeeze);
    
    if (selected_y != res_y)
    {
        char* ratio = guess_aspect_ratio(res_x, res_y * squeeze);
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
    footer.xRes = get_res_x();
    footer.yRes = get_res_y();
    footer.frameSize = footer.xRes * footer.yRes * 14/8;
    footer.frameCount = frame_count - 1; /* last frame is usually gibberish */
    footer.frameSkip = 1;
    
    footer.sourceFpsx1000 = fps_get_current_x1000();
    footer.raw_info = raw_info;

    int written = FIO_WriteFile(save_file, &footer, sizeof(lv_rec_file_footer_t));
    
    return written == sizeof(lv_rec_file_footer_t);
}

static int setup_buffers()
{
    /* allocate the entire memory, but only use large chunks */
    /* yes, this may be a bit wasteful, but at least it works */
    mem_suite = shoot_malloc_suite(0);
    if (!mem_suite) return 0;
    
    /* allocate memory for double buffering */
    int buf_size = raw_info.frame_size * 33/32; /* leave some margin, just in case */

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
    fullsize_buffers[1] = raw_info.buffer;
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

static void show_buffer_status(int adj)
{
    if (!liveview_display_idle()) return;
    
    int free_buffers = mod(saving_buffer_index - capturing_buffer_index, buffer_count); /* how many free slots do we have? */
    if (free_buffers == 0) free_buffers = 4; /* saving task waiting for capturing task */
    free_buffers += adj; /* when skipping frames, adj is -1, because capturing_buffer_index was not incremented yet */
    
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

static int frame_offset_x = 0;
static int frame_offset_y = 0;
static int frame_offset_delta_x = 0;
static int frame_offset_delta_y = 0;

static unsigned int raw_rec_should_preview(unsigned int ctx);

static void cropmark_draw()
{
    if (raw_rec_should_preview(0))
        raw_force_aspect_ratio_1to1();

    int res_x = get_res_x();
    int res_y = get_res_y();
    int skip_x = raw_info.active_area.x1 + (raw_info.jpeg.width - res_x) / 2;
    int skip_y = raw_info.active_area.y1 + (raw_info.jpeg.height - res_y) / 2;

    if (panning_enabled)
    {
        skip_x += frame_offset_x;
        skip_y += frame_offset_y;
    }
    
    int x = RAW2BM_X(skip_x);
    int y = RAW2BM_Y(skip_y);
    int w = RAW2BM_DX(res_x);
    int h = RAW2BM_DY(res_y);
    static int prev_x = 0;
    static int prev_y = 0;
    static int prev_w = 0;
    static int prev_h = 0;

    /* window changed? erase the old cropmark */
    if (prev_x != x || prev_y != y || prev_w != w || prev_h != h)
    {
        bmp_draw_rect(0, prev_x, prev_y, prev_w, prev_h);
        bmp_draw_rect(0, prev_x-1, prev_y-1, prev_w+2, prev_h+2);
    }
    
    prev_x = x;
    prev_y = y;
    prev_w = w;
    prev_h = h;

    /* display a simple cropmark */
    bmp_draw_rect(COLOR_WHITE, x, y, w, h);
    bmp_draw_rect(COLOR_BLACK, x-1, y-1, w+2, h+2);
}

static void panning_update()
{
    if (!panning_enabled) return;
    
    int res_x = get_res_x();
    int res_y = get_res_y();
    int skip_x = raw_info.active_area.x1 + (raw_info.jpeg.width - res_x) / 2;
    int skip_y = raw_info.active_area.y1 + (raw_info.jpeg.height - res_y) / 2;
    
    frame_offset_x = COERCE(
        frame_offset_x + frame_offset_delta_x, 
        raw_info.active_area.x1 - skip_x,
        raw_info.active_area.x2 - res_x - skip_x
    );
    
    frame_offset_y = COERCE(
        frame_offset_y + frame_offset_delta_y, 
        raw_info.active_area.y1 - skip_y,
        raw_info.active_area.y2 - res_y - skip_y
    );
}

static unsigned int raw_rec_polling_cbr(unsigned int unused)
{
    if (!raw_video_enabled)
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
        refresh_raw_settings();
    }

    return 0;
}

static void lv_unhack(int unused)
{
    call("aewb_enableaewb", 1);
    idle_globaldraw_en();
    PauseLiveView();
    ResumeLiveView();
}

static void hack_liveview()
{
    if (!hacked_mode) return;
    
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
        idle_globaldraw_dis();
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

static int process_frame()
{
    if (!lv) return 0;
    
    /* skip the first frame, it will be gibberish */
    if (frame_count == 0) { frame_count++; return 0; }
    
    /* copy current frame to our buffer and crop it to its final size */
    int res_x = get_res_x();
    int res_y = get_res_y();
    
    /* center crop */
    int skip_x = raw_info.active_area.x1 + (raw_info.jpeg.width - res_x) / 2;
    int skip_y = raw_info.active_area.y1 + (raw_info.jpeg.height - res_y) / 2;
    if (panning_enabled)
    {
        skip_x += frame_offset_x;
        skip_y += frame_offset_y;
    }
    
    /* start copying frame to our buffer */
    void* ptr = buffers[capturing_buffer_index].ptr + capture_offset;
    int ans = edmac_copy_rectangle_start(ptr, fullsize_buffers[(frame_count+1) % 2], raw_info.pitch, (skip_x+7)/8*14, skip_y/2*2, res_x*14/8, res_y);

    /* advance to next frame */
    frame_count++;
    capture_offset += res_x * res_y * 14/8;

    if (liveview_display_idle())
    {
        bmp_printf( FONT_MED, 30, 70, 
            "Capturing frame %d...", 
            frame_count
        );
    }
    
    return ans;
}

static unsigned int raw_rec_vsync_cbr(unsigned int unused)
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
    
    hack_liveview();
 
    /* panning window is updated when recording, but also when not recording */
    panning_update();

    if (!RAW_IS_RECORDING) return 0;
    if (!raw_lv_settings_still_valid()) { raw_recording_state = RAW_FINISHING; return 0; }
    if (stop_on_buffer_overflow && frame_skips) return 0;

    /* double-buffering */
    raw_lv_redirect_edmac(fullsize_buffers[frame_count % 2]);
    
    int res_x = get_res_x();
    int res_y = get_res_y();
    if (capture_offset + res_x * res_y * 14/8 >= buffers[capturing_buffer_index].size)
    {
        /* this buffer is full, try next one */
        int next_buffer = mod(capturing_buffer_index + 1, buffer_count);
        if (next_buffer != saving_buffer_index)
        {
            buffers[capturing_buffer_index].used = capture_offset;
            capturing_buffer_index = next_buffer;
            capture_offset = 0;
        }
        else
        {
            /* card too slow */
            frame_skips++;
            if (!stop_on_buffer_overflow)
            {
                bmp_printf( FONT_MED, 30, 70, 
                    "Skipping frames...   "
                );
            }
            show_buffer_status(-1);
            return 0;
        }
    }
    else
    {
        show_buffer_status(0);
    }
    
    dma_transfer_in_progress = process_frame();

    /* try a sync beep */
    if (sound_rec == 2 && frame_count == 1)
        beep();

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
    capture_offset = 0;
    frame_count = 0;
    frame_skips = 0;
    frame_offset_delta_x = 0;
    frame_offset_delta_y = 0;
    FILE* f = 0;
    uint32_t written = 0; /* in KB */
    uint32_t written_chunk = 0; /* in bytes, for current chunk */
    
    /* create a backup file, to make sure we can save the file footer even if the card is full */
    char backup_filename[100];
    snprintf(backup_filename, sizeof(backup_filename), "%s/backup.raw", get_dcim_dir());
    FILE* bf = FIO_CreateFileEx(backup_filename);
    if (bf == INVALID_PTR)
    {
        bmp_printf( FONT_MED, 30, 50, "File create error");
        goto cleanup;
    }
    FIO_WriteFile(bf, (void*)0x40000000, 32*1024);
    FIO_CloseFile(bf);
    
    
    /* create output file */
    int chunk = 0;
    movie_filename = get_next_raw_movie_file_name();
    char* chunk_filename = movie_filename;
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

    int t0 = 0;
    
    /* fake recording status, to integrate with other ml stuff (e.g. hdr video */
    recording = -1;
    
    /* main recording loop */
    while (RAW_IS_RECORDING && lv)
    {
        if (stop_on_buffer_overflow && frame_skips)
            goto abort;

        /* do we have any buffers completely filled with data, that we can save? */
        if (saving_buffer_index != capturing_buffer_index)
        {
            if (!t0) t0 = get_ms_clock_value();
            void* ptr = buffers[saving_buffer_index].ptr;
            int size_used = buffers[saving_buffer_index].used;
            int r = FIO_WriteFile(f, ptr, size_used);

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
                    written_chunk = 0;
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
            
            /* all fine */
            written += size_used / 1024;
            written_chunk += size_used;
            saving_buffer_index = mod(saving_buffer_index + 1, buffer_count);
        }

        /* how fast are we writing? does this speed match our benchmarks? */
        if (t0)
        {
            int t1 = get_ms_clock_value();
            int speed = written * 10 / (t1 - t0) * 1000 / 1024; // MB/s x10
            measured_write_speed = speed;
            if (liveview_display_idle()) bmp_printf( FONT_MED, 30, 90, 
                "%s: %d MB, %d.%d MB/s ",
                chunk_filename + 17, /* skip A:/DCIM/100CANON/ */
                written / 1024,
                speed/10, speed%10
            );
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
        written += FIO_WriteFile(f, buffers[saving_buffer_index].ptr, buffers[saving_buffer_index].used) / 1024;
        saving_buffer_index = mod(saving_buffer_index + 1, buffer_count);
    }
    written += FIO_WriteFile(f, buffers[capturing_buffer_index].ptr, capture_offset) / 1024;

    /* remove the backup file, to make sure we can save the footer even if card is full */
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

static MENU_SELECT_FUNC(raw_video_toggle)
{
    if (!RAW_IS_IDLE) return;
    
    raw_video_enabled = !raw_video_enabled;
    
    /* toggle the lv_save_raw flag from raw.c */
    if (raw_video_enabled)
        raw_lv_request();
    else
        raw_lv_release();
    msleep(50);
}

static int raw_playing = 0;
static void raw_video_playback_task()
{
    set_lv_zoom(1);
    PauseLiveView();
    
    int resx = get_res_x();
    int resy = get_res_y();
    raw_set_geometry(resx, resy, 0, 0, 0, 0);
    
    FILE* f = INVALID_PTR;
    void* buf = shoot_malloc(raw_info.frame_size);
    if (!buf)
        goto cleanup;

    bmp_printf(FONT_MED, 0, 0, "file '%s' ", movie_filename);
    msleep(100);

    if (!movie_filename)
        goto cleanup;

    f = FIO_Open( movie_filename, O_RDONLY | O_SYNC );
    if( f == INVALID_PTR )
        goto cleanup;

    clrscr();
    struct vram_info * lv_vram = get_yuv422_vram();
    memset(lv_vram->vram, 0, lv_vram->width * lv_vram->pitch);
    for (int i = 0; i < frame_count-1; i++)
    {
        bmp_printf(FONT_MED, 0, os.y_max - 20, "%d/%d", i+1, frame_count-1);
        bmp_printf(FONT_MED, os.x_max - font_med.width*9, os.y_max - font_med.height, "%dx%d", resx, resy);
        int r = FIO_ReadFile(f, buf, raw_info.frame_size);
        if (r != raw_info.frame_size)
            break;
        
        if (get_halfshutter_pressed())
            break;
        
        raw_info.buffer = buf;
        raw_set_geometry(resx, resy, 0, 0, 0, 0);
        raw_force_aspect_ratio_1to1();
        raw_preview_fast();
    }

cleanup:
    if (f != INVALID_PTR) FIO_CloseFile(f);
    if (buf) shoot_free(buf);
    raw_playing = 0;
    ResumeLiveView();
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
        .select = raw_video_toggle,
        .max = 1,
        .update = raw_main_update,
        .submenu_width = 710,
        .depends_on = DEP_LIVEVIEW,
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
                .priv = &stop_on_buffer_overflow,
                .max = 1,
                .choices = CHOICES("Allow", "OFF"),
                .icon_type = IT_BOOL_NEG,
                .help = "Enable if you don't mind skipping frames (for slow cards).",
            },
            {
                .name = "Panning mode",
                .priv = &panning_enabled,
                .max = 1,
                .help = "Smooth panning of the recording window (software dolly).",
                .help2 = "Use direction keys to move the window.",
            },
            {
                .name = "HaCKeD mode",
                .priv = &hacked_mode,
                .max = 1,
                .help = "Some extreme hacks for squeezing a little more speed.",
                .help2 = "Your camera will explode.",
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
    
    /* keys are only hooked in LiveView */
    if (!liveview_display_idle())
        return 1;
    
    /* start/stop recording with the LiveView key */
    if(key == MODULE_KEY_LV || key == MODULE_KEY_REC)
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
    if (panning_enabled)
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

static unsigned int raw_rec_should_preview(unsigned int ctx)
{
    /* enable preview in x5 mode, since framing doesn't match */
    /* keep x10 mode unaltered, for focusing */
    return raw_video_enabled && lv_dispsize == 5;
}

static unsigned int raw_rec_update_preview(unsigned int ctx)
{
    if (!raw_rec_should_preview(0))
        return 0;
    struct display_filter_buffers * buffers = (struct display_filter_buffers *) ctx;
    raw_force_aspect_ratio_1to1();
    raw_preview_fast_ex(raw_info.buffer, buffers->dst_buf, BM2LV_Y(os.y0), BM2LV_Y(os.y_max), !get_halfshutter_pressed());
    if (!RAW_IS_IDLE) msleep(500); /* be gentle with the CPU, save it for recording */
    return 1;
}

static unsigned int raw_rec_init()
{
    menu_add("Movie", raw_video_menu, COUNT(raw_video_menu));
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
    MODULE_CBR(CBR_DISPLAY_FILTER_ENABLED, raw_rec_should_preview, 0)
    MODULE_CBR(CBR_DISPLAY_FILTER_UPDATE, raw_rec_update_preview, 0)
MODULE_CBRS_END()
