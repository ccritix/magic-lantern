/**
 * RAW Diagnostics
 * 
 * Technical analysis of the raw image data.
 * 
 * (C) 2014 a1ex. GPL.
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <raw.h>
#include <lens.h>
#include <math.h>

static CONFIG_INT("enabled", raw_diag_enabled, 0);
static CONFIG_INT("analysis", analysis_type, 0);
static CONFIG_INT("screenshot", auto_screenshot, 0);
static volatile int raw_diag_running = 0;

#define ANALYSIS_OPTICAL_BLACK 0
#define ANALYSIS_DARKFRAME 1
#define ANALYSIS_SNR_CURVE 2

/* a float version of the routine from raw.c (should be more accurate) */
static void autodetect_black_level_calc(int x1, int x2, int y1, int y2, int dx, int dy, float* out_mean, float* out_stdev)
{
    int64_t black = 0;
    int num = 0;

    /* compute average level */
    for (int y = y1; y < y2; y += dy)
    {
        for (int x = x1; x < x2; x += dx)
        {
            int p = raw_get_pixel(x, y);
            if (p == 0) continue;               /* bad pixel */
            black += p;
            num++;
        }
    }

    float mean = (float)black / num;

    /* compute standard deviation */
    float stdev = 0;
    for (int y = y1; y < y2; y += dy)
    {
        for (int x = x1; x < x2; x += dx)
        {
            int p = raw_get_pixel(x, y);
            if (p == 0) continue;
            float dif = p - mean;
            stdev += dif * dif;
        }
    }
    
    stdev = sqrtf(stdev / num);
    
    *out_mean = mean;
    *out_stdev = stdev;
}

/* a slower version of the one from raw.c */
/* scans more pixels and does not use a safety margin */
static int autodetect_white_level()
{
    int white = 0;
    int max = 0;
    int confirms = 0;

    int raw_height = raw_info.active_area.y2 - raw_info.active_area.y1;
    for (int y = raw_info.active_area.y1 + raw_height/10; y < raw_info.active_area.y2 - raw_height/10; y ++)
    {
        int pitch = raw_info.width/8*14;
        int row = (intptr_t) raw_info.buffer + y * pitch;
        int row_crop_start = row + raw_info.active_area.x1/8*14;
        int row_crop_end = row + raw_info.active_area.x2/8*14;
        
        for (struct raw_pixblock * p = (void*)row_crop_start; (void*)p < (void*)row_crop_end; p ++)
        {
            /* todo: should probably also look at the other pixels from one pixblock */
            if (p->a > max)
            {
                max = p->a;
                confirms = 1;
            }
            else if (p->a == max)
            {
                confirms++;
                if (confirms > 5)
                {
                    white = max;
                }
            }
        }
    }
    
    /* no confirmed max? use the unconfirmed one */
    if (white == 0)
        white = max;

    return white;
}

static void ob_mean_stdev(float* mean, float* stdev)
{
    int x1 = 16;
    int x2 = raw_info.active_area.x1 - 24;
    int y1 = raw_info.active_area.y1 + 20;
    int y2 = raw_info.active_area.y2 - 20;

    autodetect_black_level_calc(x1, x2, y1, y2, 1, 1, mean, stdev);
}

/* large histogram of the left optical black area */
static void black_histogram(int ob)
{
    int x1 = ob ? 16                            : raw_info.active_area.x1;
    int x2 = ob ? raw_info.active_area.x1 - 24  : raw_info.active_area.x2;
    int y1 = ob ? raw_info.active_area.y1 + 20  : raw_info.active_area.y1;
    int y2 = ob ? raw_info.active_area.y2 - 20  : raw_info.active_area.y2;
    int dx = 1;
    int dy = 1;

    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 240, "Please wait...\n(crunching numbers)");

    float mean, stdev;
    autodetect_black_level_calc(x1, x2, y1, y2, dx, dy, &mean, &stdev);

    if (mean == 0 || stdev == 0)
    {
        /* something's wrong with our minions */
        bmp_printf(FONT_MED, 0, 0, "OB error");
        return;
    }

    /* print basic statistics */
    int mean_r = (int)roundf(mean);
    int stdev_r = (int)roundf(stdev * 100);
    int white = autodetect_white_level();
    
    if (ob)
    {
        bmp_printf(FONT_MED, 0, 0, "Optical black: mean %d, stdev %s%d.%02d ", mean_r, FMT_FIXEDPOINT2(stdev_r));
        int dr = (int)roundf((log2f(white - mean) - log2f(stdev)) * 1000.0);
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "White level: %d ", white);
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, font_med.height, "Dyn. range: %s%d.%03d EV ", FMT_FIXEDPOINT3(dr));
        bmp_printf(FONT_MED, 0, font_med.height, "%s, ISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture));
    }
    else
    {
        bmp_printf(FONT_MED, 0, 0, "Entire image: mean %d, stdev %s%d.%02d ", mean_r, FMT_FIXEDPOINT2(stdev_r));
        bmp_printf(FONT_MED, 0, font_med.height, "(assumming it's a dark frame)");
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "%s\nISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture));
    }

    /* build a small histogram of the OB noise around the mean */
    int hist[256];
    int offset = COUNT(hist)/2;
    memset(hist, 0, sizeof(hist));
    int max = 0;
    int i_lo = MAX(0, offset - 5*stdev);
    int i_hi = MIN(COUNT(hist)-1, offset + 5*stdev);
    for (int y = y1; y < y2; y++)
    {
        for (int x = x1; x < x2; x++)
        {
            int p = raw_get_pixel(x, y);
            p = COERCE(p - mean_r + offset, i_lo, i_hi);
            hist[p]++;
            max = MAX(max, hist[p]);
        }
    }
    
    /* draw the histogram */
    bmp_fill(COLOR_BG_DARK, 0, 50, 720, 420);
    int w = 700 / (i_hi - i_lo);
    int text_xlim = 0;
    for (int i = i_lo; i <= i_hi; i++)
    {
        int h = 380 * hist[i] / max;
        int x = 10 + (i-i_lo) * w;
        int y = 60 + 380 - h;
        
        bmp_fill(i == offset ? COLOR_CYAN : COLOR_WHITE, x, y, w-1, h);
        
        /* xlabel */
        int raw = i + mean_r - offset;
        if (raw % 2) continue;
        char msg[10];
        snprintf(msg, sizeof(msg), "%d", raw);
        int text_width = bmp_string_width(FONT_SMALL, msg);
        if (x - text_width/2 > text_xlim)
        {
            bmp_printf(FONT_SMALL | FONT_ALIGN_CENTER, x, 60+380, msg);
            text_xlim = x + text_width/2 + 20;
        }
    }

    /* show the 3*sigma interval */
    int i_lo3 = MAX(0, offset - 3*stdev);
    int i_hi3 = MIN(COUNT(hist), offset + 3*stdev);
    int x_lo3 = 10 + (i_lo3-i_lo) * w;
    int x_hi3 = 10 + (i_hi3-i_lo) * w;
    bmp_fill(COLOR_YELLOW, x_lo3, 460, x_hi3-x_lo3, 2);
}

/* compute mean and stdev for a random patch in the middle of the image; */
/* assuming the image is grossly out of focus, this should be a fairly good indication for local SNR */

/* todo:
 * - reject patches that clearly don't look like noise
 * - compensate for low-frequency variations (instead of just subtracting the mean)
 * - use full-res sampling for green channel?
 */
 static void patch_mean_stdev(int x, int y, float* out_mean, float* out_stdev)
{
    autodetect_black_level_calc(
        x - 8, x + 8,
        y - 8, y + 8,
        2, 2,               /* sample from the same color channel */
        out_mean, out_stdev
    );
}

/* estimate the SNR curve from a defocused image */
static void snr_graph()
{
    float black, noise;
    ob_mean_stdev(&black, &noise);

    clrscr();
    bmp_fill(COLOR_BG_DARK, 0, 00, 720, 480);

    float full_well = 14;
    
    /* horizontal grid */
    const int y_step = 35;
    const int y_origin = 35*9;
    for (int y = 0; y < 480; y += y_step)
    {
        draw_line(0, y, 720, y, COLOR_WHITE);
        if (y == y_origin)
        {
            draw_line(0, y+1, 720, y+1, COLOR_WHITE);
            draw_line(0, y+2, 720, y+2, COLOR_WHITE);
        }
        bmp_printf(FONT_SMALL | FONT_ALIGN_RIGHT, 720, y-12, "%dEV", -(y-y_origin)/y_step);
    }
    
    /* vertical grid */
    for (int signal = 0; signal < full_well; signal++)
    {
        int bx = COERCE(signal * 720 / full_well, 0, 719);
        draw_line(bx, 0, bx, 480/35*35, COLOR_WHITE);
        bmp_printf(FONT_SMALL | FONT_ALIGN_CENTER, bx, 480-font_small.height, "%dEV", signal);
    }

    bmp_fill(COLOR_BG_DARK, 0, 0, 308, y_step * 2 - 1);
    bmp_printf(FONT_MED, 0, 0, 
        "SNR curve (noise profile)\n"
        "%s\n"
        "ISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;
    
    for (int k = 0; k < 10000 && gui_state == GUISTATE_QR; k++)
    {
        /* choose random points in the image */
        int x = ((uint32_t) rand() % (x2 - x1 - 100)) + x1 + 50;
        int y = ((uint32_t) rand() % (y2 - y1 - 100)) + y1 + 50;
        
        /* not overexposed, please */
        int p = raw_get_pixel(x, y);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x-8, y-8);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x+8, y+8);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x-8, y+8);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x+8, y-8);
        if (p >= raw_info.white_level) continue;

        /* compute local average and stdev */
        float mean, stdev;
        patch_mean_stdev(x, y, &mean, &stdev);
        
        /* assumming the image is grossly out of focus, these numbers are a good estimation of the local SNR */
        float signal = log2f(MAX(1, mean - black));
        float noise = log2f(stdev);
        float snr = signal - noise;
        
        /* draw the data point */
        int bx = COERCE(signal * 720 / full_well, 0, 719);
        int by = COERCE(y_origin - snr * y_step, 0, 479);
        bmp_putpixel(bx, by, COLOR_LIGHT_BLUE);
        
        /* relax */
        if (k%128 == 0)
            msleep(10);
    }
}

/* main raw diagnostic task */
static void raw_diag_task(int corr)
{
    if (image_review_time == 0)
    {
        beep();
        NotifyBox(2000, "Enable image review from Canon menu");
        goto end;
    }

    while (!display_is_on())
        msleep(100);
    
    msleep(500);

    if (!raw_update_params())
    {
        NotifyBox(2000, "Raw error");
        goto end;
    }
    
    if (analysis_type == ANALYSIS_OPTICAL_BLACK)
    {
        black_histogram(1);
    }
    else if (analysis_type == ANALYSIS_DARKFRAME)
    {
        black_histogram(0);
    }
    else if (analysis_type == ANALYSIS_SNR_CURVE)
    {
        snr_graph();
    }
    
    if (auto_screenshot)
    {
        take_screenshot(0);
    }

end:
    raw_diag_running = 0;
}

static void raw_diag_run()
{
    if (raw_diag_running) return;
    raw_diag_running = 1;
    task_create("raw_diag_task", 0x1c, 0x1000, raw_diag_task, 0);
}

/* trigger raw diagnostics after taking a picture */
PROP_HANDLER(PROP_GUI_STATE)
{
    int* data = buf;
    if (data[0] == GUISTATE_QR)
    {
        if (raw_diag_enabled)
            raw_diag_run();
    }
}

static unsigned int raw_diag_poll(unsigned int unused)
{
    if (raw_diag_enabled && lv && get_halfshutter_pressed())
    {
        beep();
        NotifyBox(5000, "Raw diag...");
        while (get_halfshutter_pressed()) msleep(100);
        idle_globaldraw_dis();
        NotifyBoxHide();
        msleep(1000);
        raw_lv_request();
        raw_diag_run();
        raw_lv_release();
        msleep(5000);
        beep();
        clrscr();
        idle_globaldraw_en();
        redraw();
    }
    return CBR_RET_CONTINUE;
}

static struct menu_entry raw_diag_menu[] =
{
    {
        .name = "RAW Diagnostics", 
        .priv = &raw_diag_enabled, 
        .max = 1,
        .help  = "Show technical analysis of raw image data.",
        .help2 = "Enable this and take a picture with RAW image quality.\n",
        .children =  (struct menu_entry[]) {
            {
                .name = "Analysis",
                .priv = &analysis_type,
                .max = 2,
                .choices = CHOICES("Optical black", "Dark frame", "SNR curve"),
                .help  = "Choose the type of analysis you wish to run:",
                .help2 = "Optical black: mean, stdev, histogram.\n"
                         "Dark frame: same as optical black, but from the entire image.\n"
                         "SNR curve: take a defocused picture to see the noise profile.\n"
            },
            {
                .name = "Auto screenshot",
                .priv = &auto_screenshot,
                .max = 1,
                .help = "Auto-save a screenshot for every test image."
            },
            MENU_EOL,
        },
    },
};

static unsigned int raw_diag_init()
{
    menu_add("Debug", raw_diag_menu, COUNT(raw_diag_menu));
    return 0;
}

static unsigned int raw_diag_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(raw_diag_init)
    MODULE_DEINIT(raw_diag_deinit)
MODULE_INFO_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_GUI_STATE)
MODULE_PROPHANDLERS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(raw_diag_enabled)
    MODULE_CONFIG(analysis_type)
    MODULE_CONFIG(auto_screenshot)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, raw_diag_poll, 0)
MODULE_CBRS_END()
