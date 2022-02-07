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
#include <wirth.h>
#include <screenshot.h>
#include <picstyle.h>
#include <zebra.h>
#include <beep.h>
#include <fileprefix.h>
#include <shoot.h>
#include <powersave.h>

extern WEAK_FUNC(ret_0) void raw_lv_request();
extern WEAK_FUNC(ret_0) void raw_lv_release();

static CONFIG_INT("enabled", raw_diag_enabled, 0);
static CONFIG_INT("screenshot", auto_screenshot, 1);
static CONFIG_INT("dump_raw", dump_raw, 0);
static volatile int raw_diag_running = 0;

/* analysis types */
static CONFIG_INT("analysis.ob_dr", analysis_ob_dr, 1);   /* optical black and dynamic range */
static CONFIG_INT("analysis.dark.noise", analysis_darkframe_noise, 0);
static CONFIG_INT("analysis.dark.gray", analysis_darkframe_grayscale, 0);
static CONFIG_INT("analysis.dark.fpn", analysis_darkframe_fpn, 0);
static CONFIG_INT("analysis.dark.fpn.xcov", analysis_darkframe_fpn_xcov, 0);
static CONFIG_INT("analysis.snr", analysis_snr_curve, 0);
static CONFIG_INT("analysis.snr.x2", analysis_snr_curve_2_shots, 0);
static CONFIG_INT("analysis.noise.x2", analysis_noise_curve_2_shots, 0);
static CONFIG_INT("analysis.jpg", analysis_jpg_curve, 0);
static CONFIG_INT("analysis.cmp", analysis_compare_2_shots, 0);
static CONFIG_INT("analysis.cmp.hl", analysis_compare_2_shots_highlights, 0);
static CONFIG_INT("analysis.ob_zones", analysis_ob_zones, 0);

static char* cam_model;
static char* video_mode_name;

static int should_keep_going()
{
    return lv || LV_PAUSED || gui_state == GUISTATE_QR;
}

/* a float version of the routine from raw.c (should be more accurate) */
static void FAST autodetect_black_level_calc(
    int x1, int x2, int y1, int y2,         /* crop window (including x1,x2,y1,y2) */
    int dx, int dy,                         /* increments (starting from x1 and y1) */
    float* out_mean, float* out_stdev,      /* output results */
    void* raw_delta_buffer                  /* optional: a raw buffer to be subtracted from the main one */
)
{
    int64_t black = 0;
    int num = 0;

    /* compute average level */
    for (int y = y1; y <= y2; y += dy)
    {
        for (int x = x1; x <= x2; x += dx)
        {
            int p = raw_get_pixel(x, y);
            if (p == 0) continue;               /* bad pixel */
            if (raw_delta_buffer) p -= raw_get_pixel_ex(raw_delta_buffer, x, y);
            black += p;
            num++;
        }
    }

    float mean = (float)black / num;

    /* compute standard deviation */
    float stdev = 0;
    for (int y = y1; y <= y2; y += dy)
    {
        for (int x = x1; x <= x2; x += dx)
        {
            int p = raw_get_pixel(x, y);
            if (p == 0) continue;
            if (raw_delta_buffer) p -= raw_get_pixel_ex(raw_delta_buffer, x, y);
            float dif = p - mean;
            stdev += dif * dif;
        }
    }
    
    stdev = sqrtf(stdev / num);
    
    *out_mean = mean;
    *out_stdev = stdev;
}

/* a slower version of the one from raw.c (crop_rec_4k) */
/* scans more pixels (every line, 2 pixels from every raw_pixblock)
 * and does not use a lower bound for white level */
/* optionally compute overexposure percentage */
static int autodetect_white_level(int * overexposure_percentage_x100)
{
    int initial_guess = 0;
    int white = initial_guess;

    /* build a temporary histogram for the entire image */
    int * hist = malloc(16384 * sizeof(hist[0]));
    if (!hist)
    {
        /* oops */
        ASSERT(0);
        return initial_guess;
    }
    memset(hist, 0, 16384 * sizeof(hist[0]));

    int raw_height = raw_info.active_area.y2 - raw_info.active_area.y1;
    for (int y = raw_info.active_area.y1 + raw_height/10; y < raw_info.active_area.y2 - raw_height/10; y++)
    {
        int pitch = raw_info.width/8*14;
        int row = (intptr_t) raw_info.buffer + y * pitch;
        int skip_5p = ((raw_info.active_area.x2 - raw_info.active_area.x1) * 6/128)/8*14; /* skip 5% */
        int row_crop_start = row + raw_info.active_area.x1/8*14 + skip_5p;
        int row_crop_end = row + raw_info.active_area.x2/8*14 - skip_5p;
        
        for (struct raw_pixblock * p = (void*)row_crop_start; (void*)p < (void*)row_crop_end; p++)
        {
            /* a is red or green, b is green or blue */
            int a = p->a;
            int b = p->h;
            hist[a]++;
            hist[b]++;
        }
    }

    /* compute the cdf (replace the histogram) */
    int * cdf = hist;
    int acc = 0;
    for (int i = 0; i < 16384; i++)
    {
        acc += hist[i];
        cdf[i] = acc;
    }

    int total = cdf[16383];

    if (0)
    {
        /* for debugging */
        char fn[100];
        get_numbered_file_name("CDF%05d.bin", 99999, fn, sizeof(fn));
        FILE * f = FIO_CreateFile(fn);
        FIO_WriteFile(f, cdf, 16384 * sizeof(cdf[0]));
        FIO_CloseFile(f);
    }

    int last = 16383;
    float peak = 0;
    for (int i = 16382; i >= raw_info.black_level + 256; i--)
    {
        /* attempt to find the steepest slope on the right side, */
        /* relative to what's on the left side (heuristic) */

        /* right side of the peak; allow at least 10 overexposed pixels */
        int possibly_clipped = total - cdf[i];
        while (last > i + 1 && cdf[last] > total - possibly_clipped / 100 - 10)
        {
            last--;
        }

        /* left side of the peak */
        /* include at least 200 px, or twice as big as the peak itself */
        int first = i - (last - i) * 2 - 200;
        if (first < 0) break;

        float slope_right = (float) (cdf[last] - cdf[i]) / (last - i);
        float slope_left = (float) (cdf[i] - cdf[first]) / (i - first);
        if (slope_left < 1e-5) continue;
        float slope_ratio = slope_right / slope_left;

        int p_x100 = (uint64_t) cdf[i] * (uint64_t) 10000 / total;
        qprintf("%d: %s%d.%02d %d-%d %d/%d %d\n", i, FMT_FIXEDPOINT2(p_x100), first, last, (int)(slope_left * 100), (int)(slope_right * 100), (int)(slope_ratio * 100));

        if (slope_ratio > peak)
        {
            peak = slope_ratio;

            /* use a small margin when choosing the white level, as the autodetection might not be very exact */
            /* 0.005 EV would give about 100 units at W=16382 and B=2048. */
            white = (i - raw_info.black_level) * 16270 / 16384 + raw_info.black_level;
        }
    }

    int clipped = total - cdf[white];

    printf("WL: %d (%d/%d clipped pixels)\n", white, clipped, total);

    if (overexposure_percentage_x100)
    {
        *overexposure_percentage_x100 = (uint64_t) clipped * (uint64_t) 10000 / total;
    }
    free(hist);
    return white;
}

static void FAST ob_mean_stdev(float* mean, float* stdev)
{
    /* try looking at the left black bar first */
    int x1 = 16;
    int x2 = raw_info.active_area.x1 - 24;
    int y1 = raw_info.active_area.y1 + 20;
    int y2 = raw_info.active_area.y2 - 20;

    if (x1 >= x2)
    {
        /* left bar too small? try the top one */
        x2 = raw_info.active_area.x2 - 16;
        y1 = 4;
        y2 = raw_info.active_area.y1 - 8;
    }

    autodetect_black_level_calc(x1, x2, y1, y2, 1, 1, mean, stdev, 0);
}

static void FAST quick_analysis_lv()
{
    /* white level & overexposure autodetection */
    int over_x100 = -1;
    int white = autodetect_white_level(&over_x100);

    /* black and noise stdev */
    /* read 5 times (from different LiveView frames) and use the median value */
    /* this is slow, but the estimated DR is a lot more consistent */
    int blacks[5], noises[5];
    for (int i = 0; i < COUNT(blacks); i++)
    {
        float b, n;
        ob_mean_stdev(&b, &n);
        blacks[i] = b * 1000;
        noises[i] = n * 1000;
        msleep(100);
    }
    float black = median_int_wirth(blacks, COUNT(blacks)) / 1000.0;
    float noise = median_int_wirth(noises, COUNT(noises)) / 1000.0;

    /* dynamic range approximation, without considering noise model */
    int dr_x100 = (int)roundf((log2f(white - black) - log2f(noise)) * 100.0);

    bmp_printf(
        FONT_MED | FONT_ALIGN_RIGHT | FONT_ALIGN_FILL,
        720, 50,
        "  %d..%d\n"
        "  %s%d.%02d%% over\n"
        "  ~ %s%d.%02d EV",
        (int) black, white,
        FMT_FIXEDPOINT2(over_x100), 0,
        FMT_FIXEDPOINT2(dr_x100)
    );
}

/* large histogram of the left optical black area */
static void FAST black_histogram(int ob)
{
    int x1 = ob ? 16                            : raw_info.active_area.x1 + 20;
    int x2 = ob ? raw_info.active_area.x1 - 24  : raw_info.active_area.x2 - 20;
    int y1 = ob ? raw_info.active_area.y1 + 20  : raw_info.active_area.y1 + 20;
    int y2 = ob ? raw_info.active_area.y2 - 20  : raw_info.active_area.y2 - 20;
    int dx = 1;
    int dy = 1;

    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 240, "Please wait...\n(crunching numbers)");

    float mean, stdev;
    autodetect_black_level_calc(x1, x2, y1, y2, dx, dy, &mean, &stdev, 0);

    if (mean == 0 || stdev == 0)
    {
        /* something's wrong with our minions */
        bmp_printf(FONT_MED, 0, 0, "OB error");
        return;
    }

    /* print basic statistics */
    int mean_r = (int)roundf(mean);
    int stdev_r = (int)roundf(stdev * 100);
    int over_x100 = -1;
    int white = autodetect_white_level(&over_x100);
    //int canon_white = shamem_read(0xC0F12054) >> 16;
    
    if (ob)
    {
        bmp_printf(FONT_MED, 0, 0, "Optical black: mean %d, stdev %s%d.%02d ", mean_r, FMT_FIXEDPOINT2(stdev_r));
        int dr = (int)roundf((log2f(white - mean) - log2f(stdev)) * 1000.0);
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "White: %d (%s%d.%02d%% over) ", white, FMT_FIXEDPOINT2(over_x100));
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, font_med.height, "Dyn. range: %s%d.%03d EV ", FMT_FIXEDPOINT3(dr));
        bmp_printf(FONT_MED, 0, font_med.height, "%s, %s, ISO %d %s "SYM_F_SLASH"%s%d.%d", cam_model, video_mode_name, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture));
    }
    else
    {
        bmp_printf(FONT_MED, 0, 0, "Entire image: mean %d, stdev %s%d.%02d ", mean_r, FMT_FIXEDPOINT2(stdev_r));
        bmp_printf(FONT_MED, 0, font_med.height, "(assuming it's a dark frame)");
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "%s, %s\nISO %d %s "SYM_F_SLASH"%s%d.%d", cam_model, video_mode_name, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture));
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
/* optionally, pass a second raw buffer to sample the difference between them (ala Roger Clark) */

/* todo:
 * - reject patches that clearly don't look like noise
 * - compensate for low-frequency variations (instead of just subtracting the mean)
 * - use full-res sampling for green channel?
 */
static void patch_mean_stdev(int x, int y, float* out_mean, float* out_stdev, int radius, void* raw_delta_buffer)
{
    autodetect_black_level_calc(
        x - radius, x + radius,
        y - radius, y + radius,
        2, 2,               /* sample from the same color channel */
        out_mean, out_stdev,
        raw_delta_buffer
    );
}

static void snr_graph_init(char* graph_name, int clear_lines, float full_well, int y_step, int y_origin)
{
    clrscr();
    bmp_fill(COLOR_BG_DARK, 0, 00, 720, 480);

    /* horizontal grid */
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

    bmp_fill(COLOR_BG_DARK, 0, 0, 308, y_step * clear_lines - 1);
    bmp_printf(FONT_MED, 0, 0, 
        "%s\n"
        "%s, %s\n"
        "ISO %d %s "SYM_F_SLASH"%s%d.%d",
        graph_name,
        cam_model, video_mode_name,
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );
}

/* estimate the SNR curve from a defocused image */
static void snr_graph()
{
    float black, noise;
    ob_mean_stdev(&black, &noise);

    const float full_well = 14;
    const int y_step = 35;
    const int y_origin = 35*9;

    snr_graph_init("SNR curve (noise profile)", 2, full_well, y_step, y_origin);
    
    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;

    for (int k = 0; k < 10000 && should_keep_going(); k++)
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
        patch_mean_stdev(x, y, &mean, &stdev, 8, 0);
        
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

/* Model assumes constant additive read noise combined with Poisson shot noise */
/* Minimize the sum of absolute errors, similar to median */
static float check_noise_model(float read_noise, float gain, float* signal_points, float* noise_points, int num_points)
{
    float err = 0;
    for (int i = 0; i < num_points; i++)
    {
        float input_signal = signal_points[i];
        float measured_noise = noise_points[i];
        if (input_signal != 0 && measured_noise != 0)
        {
            float dn = powf(2, input_signal);
            float electrons = dn * gain;
            float shot_snr = sqrtf(electrons);
            float shot_noise = dn / shot_snr;
            float combined_noise = sqrtf(read_noise*read_noise + shot_noise*shot_noise);
            float delta = measured_noise - log2f(combined_noise);
            err += ABS(delta);
        }
    }
    return err;
}

/* random numbers between -1 and 1 */
static float frand()
{
    return ((rand() % 1001) - 500) / 500.0;
}

/* dumb minimization with random advancement step; feel free to implement something like Nelder-Mead ;) */
static void fit_noise_model(float ob_noise, float initial_gain, float* signal_points, float* noise_points, int num_points, float* out_read_noise, float* out_gain)
{
    float read_noise = ob_noise;
    float gain = initial_gain;
    
    float current_err = check_noise_model(read_noise, gain, signal_points, noise_points, num_points);
    
    for (int i = 0; i < 5000; i++)
    {
        float next_noise = read_noise + frand();
        float next_gain = gain + frand();
        
        float next_err = check_noise_model(next_noise, next_gain, signal_points, noise_points, num_points);
        if (next_err < current_err)
        {
            read_noise = next_noise;
            gain = next_gain;
            current_err = next_err;
        }
    }
    
    *out_read_noise = read_noise;
    *out_gain = gain;
}

/* estimate the SNR curve from the difference between two identical shots */
/* method inspired from Roger Clark, http://www.clarkvision.com/articles/evaluation-1d2/ */
static void snr_graph_2_shots(int noise_curve)
{
    float black, ob_noise;
    ob_mean_stdev(&black, &ob_noise);
    
    int white = autodetect_white_level(NULL);

    const float full_well = 14;
    const int y_step = 35;
    const int y_origin = 35 * (noise_curve ? 13 : 9);
    #define SNR_OR_NOISE(snr) (noise_curve ? signal - (snr) : (snr))

    snr_graph_init(
        noise_curve ? "Noise curve (EV)" : "SNR curve (noise profile)",
        6, full_well, y_step, y_origin
    );

    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;
    
    static void* second_buf = 0;
    static int raw_buffer_size = 0;

    if (!second_buf)
    {
        /* our buffer can't be larger than 32 MB :( */
        /* no big deal, we'll trim the analyzed image to fit in 32 M */

        /* if it fails, let's try some smaller buffer sizes */
        /* todo: update the memory backend to allow allocating the max possible contiguous chunk via malloc? */
        int buffer_sizes[] = {31, 28, 25, 20, 16, 10, 5 };
        for  (int buffer_size_index = 0; buffer_size_index < COUNT(buffer_sizes) && !second_buf; buffer_size_index++)
        {
            raw_buffer_size = MIN(raw_info.frame_size, buffer_sizes[buffer_size_index] * 1024 * 1024);
            second_buf = tmp_malloc(raw_buffer_size);   /* use shoot_malloc instead of srm_malloc */
        }

        if (second_buf)
        {
            memcpy(second_buf, raw_info.buffer, raw_buffer_size);
            
            bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, 
                "Please take one more test picture of the same static scene.\n"
                "Use the same settings, a solid tripod, and 10-second timer."
            );
        }
        else
        {
            bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, 
                "You may need to solder some RAM chips :("
            );
        }
        return;
    }
    
    /* data points for curve fitting */
    int num_points[28] = {0};
    int * data_points_x[28] = {0};
    int * data_points_y[28] = {0};
    float medians_x[28] = {0};
    float medians_y[28] = {0};

    for (int i = 0; i < COUNT(data_points_x); i++)
    {
        data_points_x[i] = malloc(5000 * sizeof(int));
        data_points_y[i] = malloc(5000 * sizeof(int));
        
        if (!data_points_x[i] || !data_points_y[i])
        {
            bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, 
                "You may need to solder some RAM chips :("
            );
            goto end;
        }
    }

    bmp_printf(FONT_MED, 0, font_med.height*3, 
        "Sampling data...\n"
    );

    for (int k = 0; k < 20000 && should_keep_going(); k++)
    {
        /* choose random points in the image */
        int x = ((uint32_t) rand() % (x2 - x1 - 100)) + x1 + 50;
        int y = ((uint32_t) rand() % (y2 - y1 - 100)) + y1 + 50;
        
        /* make sure we don't get outside the second buffer */
        if (y*raw_info.pitch + (x + 50) * 14/8 > raw_buffer_size)
            continue;
        
        /* not overexposed, please */
        int p = raw_get_pixel(x, y);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x-16, y-16);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x+16, y+16);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x-16, y+16);
        if (p >= raw_info.white_level) continue;
        p = raw_get_pixel(x+16, y-16);
        if (p >= raw_info.white_level) continue;

        /* compute local average (on the test image) and stdev (on the delta image) */
        float mean, stdev, unused;
        patch_mean_stdev(x, y, &mean, &unused, 16, 0);
        patch_mean_stdev(x, y, &unused, &stdev, 16, second_buf);
        
        /* when subtracting two test images of the same scene, taken with identical settings, 
         * the noise increases by sqrt(2); we need to undo this */
        stdev /= sqrtf(2);
        
        /* assumming the two test images represent the same scene, taken with identical settings,
         * these numbers a good estimation of the local SNR */
        float signal = log2f(MAX(1, mean - black));
        float noise = log2f(stdev);
        float snr = signal - noise;
        
        /* draw the data point */
        int bx = COERCE(signal * 720 / full_well, 0, 719);
        int by = COERCE(y_origin - SNR_OR_NOISE(snr) * y_step, 0, 479);
        bmp_putpixel(bx, by, COLOR_LIGHT_BLUE);
        
        /* group the data points every half-stop */
        int slot = COERCE((int)roundf(signal * 2), 0, COUNT(data_points_x) - 1);
        if (num_points[slot] < 5000)
        {
            data_points_x[slot][num_points[slot]] = (int)(signal * 10000.0f);
            data_points_y[slot][num_points[slot]] = (int)(noise * 10000.0f);
            num_points[slot]++;
        }

        /* relax */
        if (k%128 == 0)
            msleep(10);
    }

    bmp_printf(FONT_MED, 0, font_med.height*3, 
        "Fitting SNR curve...\n"
    );

    /* extract the median for each half-stop group */
    for (int i = 0; i < COUNT(data_points_x); i++)
    {
        if (num_points[i] > 100)
        {
            medians_x[i] = median_int_wirth(data_points_x[i], num_points[i]) / 10000.0f;
            medians_y[i] = median_int_wirth(data_points_y[i], num_points[i]) / 10000.0f;

            /* draw the data point */
            float signal = medians_x[i];
            float noise = medians_y[i];
            float snr = signal - noise;
            int bx = COERCE(signal * 720 / full_well, 0, 719);
            int by = COERCE(y_origin - SNR_OR_NOISE(snr) * y_step, 0, 479);
            fill_circle(bx, by, 3, COLOR_RED);
        }
        else
        {
            medians_x[i] = 0;
            medians_y[i] = 0;
        }
    }
    
    /* fit a model for read noise and gain (e/DN) */
    float read_noise, gain;
    fit_noise_model(ob_noise, 5, medians_x, medians_y, COUNT(medians_x), &read_noise, &gain);

    /* check the mathematical model */
    static float signal_zerocross = 0;
    for (float signal = 0; signal < log2f(white-black); signal += 0.01)
    {
        float dn = powf(2, signal);
        float electrons = dn * gain;
        float shot_snr = sqrtf(electrons);
        float shot_noise = dn / shot_snr;
        
        float combined_noise = sqrtf(read_noise*read_noise + shot_noise*shot_noise);
        float model_snr = signal - log2f(combined_noise);
        
        if (model_snr <= 0)
        {
            /* find the intersection point between our model and SNR=0 */
            signal_zerocross = signal;
        }

        int bx, by;

        /* plot the SNR curve contribution from read noise only*/
        float read_snr = signal - log2f(read_noise);
        bx = COERCE(signal * 720 / full_well, 0, 719);
        by = COERCE(y_origin - SNR_OR_NOISE(read_snr) * y_step, 0, 479);
        
        if (bx % 3 == 1)
        {
            bmp_putpixel(bx, by, COLOR_GRAY(50));
        }

        /* plot the SNR curve contribution from shot noise only*/
        bx = COERCE(signal * 720 / full_well, 0, 719);
        by = COERCE(y_origin - SNR_OR_NOISE(log2f(shot_snr)) * y_step, 0, 479);
        
        if (bx % 3 == 1)
        {
            bmp_putpixel(bx, by, COLOR_YELLOW);
        }

        /* plot the ideal curve */
        bx = COERCE(signal * 720 / full_well, 0, 719);
        by = COERCE(y_origin - SNR_OR_NOISE(model_snr) * y_step, 0, 479);
        bmp_putpixel(bx, by, COLOR_RED);
    }
    
    int full_well_electrons = (int)roundf(gain * (white - black));
    float dr = log2f(white - black) - signal_zerocross;
    int dr_x100 = (int)roundf(dr * 100);
    int ob_noise_x100 = (int)roundf(ob_noise * 100);
    int read_noise_x100 = (int)roundf(read_noise * 100);
    int gain_x100 = (int)roundf(gain * 100);
    int noise_electrons_x100 = (int)roundf(read_noise * gain * 100);

    big_bmp_printf(FONT_MED, 0, font_med.height*3, 
        "Measured OB noise: %s%d.%02d DN\n"
        "Model read noise: %s%d.%02d DN\n"
        "Model gain : %s%d.%02d e/DN\n"
        "Apparent read noise: %s%d.%02d e\n"
        "Full well capacity : %d e\n"
        "Dynamic range : %s%d.%02d EV\n",
        FMT_FIXEDPOINT2(ob_noise_x100),
        FMT_FIXEDPOINT2(read_noise_x100),
        FMT_FIXEDPOINT2(gain_x100),
        FMT_FIXEDPOINT2(noise_electrons_x100),
        full_well_electrons,
        FMT_FIXEDPOINT2(dr_x100)
    );

end:
    /* data points no longer needed */
    for (int i = 0; i < COUNT(data_points_x); i++)
    {
        if (data_points_x[i]) free(data_points_x[i]);
        if (data_points_y[i]) free(data_points_y[i]);
    }

    if (second_buf)
    {
        free(second_buf);
        second_buf = 0;
    }
}

/* estimate the picture style curve */
static void jpg_curve()
{
    float black, noise;
    ob_mean_stdev(&black, &noise);

    clrscr();
    bmp_fill(COLOR_BG_DARK, 0, 00, 720, 480);

    float full_well = 14;
    
    /* horizontal grid */
    for (int luma = 0; luma <= 256; luma += 32)
    {
        int y = 455 - luma * 455 / 256;
        draw_line(0, y, 720, y, COLOR_WHITE);
        bmp_printf(FONT_SMALL | FONT_ALIGN_RIGHT, 700, MAX(y-12, 0), "%d", luma);
    }
    
    /* vertical grid */
    for (int signal = 0; signal < full_well; signal++)
    {
        int bx = COERCE(signal * 720 / full_well, 0, 719);
        draw_line(bx, 0, bx, 480/35*35, COLOR_WHITE);
        bmp_printf(FONT_SMALL | FONT_ALIGN_CENTER, bx, 480-font_small.height, "%dEV", signal);
    }

    bmp_fill(COLOR_BG_DARK, 0, 0, 308, 113);
    int i = lens_info.picstyle;
    bmp_printf(FONT_MED, 0, 0, 
        "JPEG curve\n"
        "%s %d,%d,%d,%d\n"
        "%s, %s\n"
        "ISO %d %s "SYM_F_SLASH"%s%d.%d\n",
        get_picstyle_name(lens_info.raw_picstyle),
        lens_get_from_other_picstyle_sharpness(i),
        lens_get_from_other_picstyle_contrast(i),
        ABS(lens_get_from_other_picstyle_saturation(i)) < 10 ? lens_get_from_other_picstyle_saturation(i) : 0,
        ABS(lens_get_from_other_picstyle_color_tone(i)) < 10 ? lens_get_from_other_picstyle_color_tone(i) : 0,
        cam_model, video_mode_name,
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;

    uint32_t* lv = (uint32_t*) get_yuv422_vram()->vram;

    int colors[4] = {COLOR_RED, COLOR_GREEN1, COLOR_GREEN1, COLOR_LIGHT_BLUE};
    for (int k = 0; k < 100000 && should_keep_going(); k++)
    {
        /* choose random points in the image */
        int x = ((uint32_t) rand() % (x2 - x1 - 100)) + x1 + 50;
        int y = ((uint32_t) rand() % (y2 - y1 - 100)) + y1 + 50;
        
        /* raw value */
        int p = raw_get_pixel(x, y);
        float signal = log2f(MAX(1, p - black));

        /* jpeg value */
        uint32_t uyvy = lv[RAW2LV(x,y)/4];
        uint32_t luma_x2 = ((uyvy >> 24) & 0xFF) + ((uyvy >> 8) & 0xFF);
        
        /* draw the data point */
        int bx = COERCE(signal * 720 / full_well, 0, 719);
        int by = 455 - luma_x2 * 455 / 512;
        int color = colors[x%2 + (y%2)*2];
        bmp_putpixel(bx, by, color);
    }
}

/* like octave mean(x) */
static float FAST mean(float* X, int N)
{
    float sum = 0;

    for (int i = 0; i < N; i++)
    {
        sum += X[i];
    }
    
    return sum / N;
}

/* like octave std(x) */
static float FAST std(float* X, int N)
{
    float m = mean(X, N);
    float stdev = 0;
    for (int i = 0; i < N; i++)
    {
        float dif = X[i] - m;
        stdev += dif * dif;
    }
    
    return sqrtf(stdev / N);
}

/* somewhat similar to octave plot(y) */
/* X assummed to be 1:n */
/* data stretched to fit the bounding box */
static void plot(float* Y, int n, int x0, int y0, int w, int h)
{
    bmp_draw_rect(COLOR_GRAY(50), x0, y0, w, h);
    
    float max = -INFINITY;
    float min = INFINITY;
    for (int i = 0; i < n; i++)
    {
        min = MIN(min, Y[i]);
        max = MAX(max, Y[i]);
    }
    
    if (min < 0 && max > 0)
    {
        int yc = y0 + h - (0 - min) * h / (max - min);
        draw_line(x0, yc, x0+w, yc, COLOR_GRAY(50));
    }
    
    int prev_x = 0;
    int prev_y = 0;
    for (int i = 0; i < n; i++)
    {
        int x = x0 + i * w / n;
        int y = y0 + h - (Y[i] - min) * h / (max - min);
        
        if (i > 0)
        {
            draw_line(prev_x, prev_y-1, x, y-1, COLOR_BLACK);
            draw_line(prev_x, prev_y+1, x, y+1, COLOR_BLACK);
            draw_line(prev_x, prev_y, x, y, COLOR_WHITE);
        }
        
        prev_x = x;
        prev_y = y;
    }
}

static void FAST compute_fpn_v(float* fpn)
{
    for (int x = raw_info.active_area.x1; x < raw_info.active_area.x2; x++)
    {
        int sum = 0;
        int num = raw_info.active_area.y2 - raw_info.active_area.y1;
        for (int y = raw_info.active_area.y1; y < raw_info.active_area.y2; y++)
        {
            int p = raw_get_pixel(x, y);
            sum += p;
        }
        fpn[x] = (float) sum / num;
    }
}

static void FAST compute_fpn_h(float* fpn)
{
    for (int y = raw_info.active_area.y1; y < raw_info.active_area.y2; y++)
    {
        int sum = 0;
        int num = raw_info.active_area.x2 - raw_info.active_area.x1;
        for (int x = raw_info.active_area.x1; x < raw_info.active_area.x2; x++)
        {
            int p = raw_get_pixel(x, y);
            sum += p;
        }
        fpn[y] = (float) sum / num;
    }
}

static void darkframe_fpn()
{
    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please wait...\n(crunching numbers)");

    float black, ob_noise;
    ob_mean_stdev(&black, &ob_noise);

    /* any 100-megapixel cameras out there? */
    float* fpn; int fpn_size = 10000 * sizeof(fpn[0]);
    fpn = malloc(fpn_size); if (!fpn) return;
    memset(fpn, 0, fpn_size);
    
    compute_fpn_v(fpn);
    
    bmp_fill(COLOR_BG_DARK, 0, 0, 720, 230+20);
    float* fpn_x1 = fpn + raw_info.active_area.x1;
    int fpn_N = raw_info.active_area.x2 - raw_info.active_area.x1;
    plot(fpn_x1, fpn_N, 10, 25, 700, 220);
    float s = std(fpn_x1, fpn_N);
    int s_rounded = (int) roundf(s * 100.0);
    int s_ratio = (int) roundf(s / ob_noise * 100.0);
    bmp_printf(FONT_MED, 15, 30, "Vertical FPN: stdev=%s%d.%02d ratio=%s%d.%02d", FMT_FIXEDPOINT2(s_rounded), FMT_FIXEDPOINT2(s_ratio));

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 300, "Please wait...\n(crunching numbers)");

    memset(fpn, 0, fpn_size);
    compute_fpn_h(fpn);

    bmp_fill(COLOR_BG_DARK, 0, 250, 720, 230);
    float* fpn_y1 = fpn + raw_info.active_area.y1;
    fpn_N = raw_info.active_area.y2 - raw_info.active_area.y1;
    plot(fpn_y1, fpn_N, 10, 255, 700, 220);
    s = std(fpn_y1, fpn_N);
    s_rounded = (int) roundf(s * 100.0);
    s_ratio = (int) roundf(s / ob_noise * 100.0);
    bmp_printf(FONT_MED, 15, 260, "Horizontal FPN: stdev=%s%d.%02d ratio=%s%d.%02d", FMT_FIXEDPOINT2(s_rounded), FMT_FIXEDPOINT2(s_ratio));

    bmp_printf(FONT_MED, 0, 0, 
        "Fixed-pattern noise"
    );

    bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, 
        "%s, %s\n"
        "  ISO %d %s "SYM_F_SLASH"%s%d.%d",
        cam_model, video_mode_name,
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    free(fpn);
}

static int scale_for_plot_dots_x(float x, float min, float max, int x0, int w)
{
    return COERCE(x0 + (x - min) * w / (max - min), x0, x0+w);
}

static int scale_for_plot_dots_y(float y, float min, float max, int y0, int h)
{
    return COERCE(y0 + h - (y - min) * h / (max - min), y0, y0+h);
}

static void plot_dots_grid(float min, float max, float step, int x0, int y0, int w, int h)
{
    for (float a = min; a < max; a += step)
    {
        int px = scale_for_plot_dots_x(a, min, max, x0, w);
        draw_line(px, y0, px, y0 + h, COLOR_GRAY(10));
        
        int py = scale_for_plot_dots_y(a, min, max, y0, h);
        draw_line(x0, py, x0 + w, py, COLOR_GRAY(10));
    }
}

static void plot_dots(float* X, float* Y, int n, int x0, int y0, int w, int h, int color)
{
    bmp_draw_rect(COLOR_GRAY(50), x0, y0, w, h);
    draw_line(x0+w, y0, x0, y0+h, COLOR_GRAY(10));

    float max = -INFINITY;
    float min = INFINITY;
    for (int i = 0; i < n; i++)
    {
        min = MIN(min, X[i]);
        max = MAX(max, X[i]);
        min = MIN(min, Y[i]);
        max = MAX(max, Y[i]);
    }

    if (min < 0 && max > 0)
    {
        int xc = scale_for_plot_dots_x(0, min, max, x0, w);
        int yc = scale_for_plot_dots_y(0, min, max, y0, h);
        draw_line(x0, yc, x0+w, yc, COLOR_GRAY(50));
        draw_line(xc, y0, xc, y0+h, COLOR_GRAY(50));
    }
    
    for (int i = 0; i < n; i++)
    {
        int px = scale_for_plot_dots_x(X[i], min, max, x0, w);
        int py = scale_for_plot_dots_y(Y[i], min, max, y0, h);
        bmp_putpixel(px, py, color);
    }
    
    /* todo: compute the cross-covariance and print it */
}

static void darkframe_fpn_xcov()
{
    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please wait...\n(crunching numbers)");

    /* any 100-megapixel cameras out there? */
    int fpn_size = 10000 * sizeof(float);
    float* fpnv = fio_malloc(2*fpn_size); if (!fpnv) return;
    float* fpnh = fpnv + 10000;
    memset(fpnv, 0, fpn_size);
    memset(fpnh, 0, fpn_size);

    /* data from previous picture is taken from a file */
    char* prev_filename = "FPN.DAT";
    float* prev_fpnv = fio_malloc(2*fpn_size); if (!prev_fpnv) return;
    float* prev_fpnh = prev_fpnv + 10000;
    int read_size = read_file(prev_filename, prev_fpnv, 2*fpn_size);
    int ok = (read_size == 2*fpn_size);
    
    compute_fpn_v(fpnv);
    if (ok)
    {
        bmp_fill(COLOR_BG_DARK, 0, 0, 360, 480);
        bmp_printf(FONT_MED, 15, 90, "Vertical FPN");

        int x1 = raw_info.active_area.x1;
        int N = raw_info.active_area.x2 - raw_info.active_area.x1;
        plot_dots(prev_fpnv+x1, fpnv+x1, N, 0, 120, 360, 360, COLOR_LIGHT_BLUE);
    }

    compute_fpn_h(fpnh);
    if (ok)
    {
        bmp_fill(COLOR_BG_DARK, 360, 0, 360, 480);
        bmp_printf(FONT_MED, 360, 90, "Horizontal FPN");

        int y1 = raw_info.active_area.y1;
        int N = raw_info.active_area.y2 - raw_info.active_area.y1;
        plot_dots(prev_fpnh+y1, fpnh+y1, N, 360, 120, 360, 360, COLOR_LIGHT_BLUE);
    }
    else
    {
        clrscr();
        bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please take one more dark frame.");
    }

    bmp_printf(FONT_MED, 0, 0, 
        "FPN cross-covariance"
    );

    bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, 
        "%s, %s\n"
        "  ISO %d %s "SYM_F_SLASH"%s%d.%d",
        cam_model, video_mode_name,
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    /* save data from this picture, to be used with the next one */
    dump_seg(fpnv, 2*fpn_size, prev_filename);
    
    /* cleanup */
    free(fpnv);
    free(prev_fpnv);
}

struct test_pixel
{
    int16_t x;
    int16_t y;
    int16_t pixel;
} __attribute__((packed));

static void compare_2_shots(int min_adu)
{
    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please wait...\n(crunching numbers)");

    float black, noise;
    ob_mean_stdev(&black, &noise);

    /* get white level */
    int white = autodetect_white_level(NULL);

    /* values from previous shot */
    static float black_prev;
    static int white_prev;
    static float noise_prev;
    
    /* exposure difference */
    float expo_delta_median = 0;
    float expo_delta_median_normalized = 0;
    float expo_delta_clip = 0;

    int N = 30000;
    int data_size = N * sizeof(struct test_pixel);
    struct test_pixel * this = fio_malloc(data_size); if (!this) return;

    /* data from previous picture is taken from a file */
    char* prev_filename = "RAWSAMPL.DAT";
    struct test_pixel * prev = fio_malloc(data_size); if (!prev) return;
    int read_size = read_file(prev_filename, prev, data_size);
    int ok = (read_size == data_size);

    /* initialize some random sampling points */
    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;
    for (int k = 0; k < N; k++)
    {
        if (ok)
        {
            /* if a previous image is loaded, keep the sampling points from there */
            this[k].x = prev[k].x;
            this[k].y = prev[k].y;
        }
        else
        {
            /* choose random points in the image */
            int x = ((uint32_t) rand() % (x2 - x1 - 100)) + x1 + 50;
            int y = ((uint32_t) rand() % (y2 - y1 - 100)) + y1 + 50;
            
            /* force color channel */
            if (k < N/3)
            {
                /* force red */
                x &= ~1;
                y &= ~1;
            }
            else if (k < 2*N/3)
            {
                /* force green */
                x &= ~1;
                y &= ~1;
                if (k%2) x++; else y++;
            }
            else
            {
                /* force blue */
                x |= 1;
                y |= 1;
            }
            
            this[k].x = x;
            this[k].y = y;
        }
    }

    /* sample current image */
    for (int k = 0; k < N; k++)
    {
        int x = this[k].x;
        int y = this[k].y;
        this[k].pixel = raw_get_pixel(x, y) - black;
    }

    if (ok)
    {
        /* plot the graph */
        bmp_fill(COLOR_BG_DARK, 0, 0, 480, 480);
        bmp_fill(COLOR_BLACK, 480, 0, 720-480, 480);
        
        float* X = malloc(N * sizeof(float)); if (!X) return;
        float* Y = malloc(N * sizeof(float)); if (!Y) return;

        /* extract the data and convert to EV */
        for (int i = 0; i < N; i++)
        {
            X[i] = log2f(MAX(prev[i].pixel, min_adu));
            Y[i] = log2f(MAX(this[i].pixel, min_adu));
        }
        
        /* estimate median exposure difference */
        int* deltas = malloc(N * sizeof(int)); if (!deltas) return;
        for (int i = 0; i < N; i++)
        {
            float delta = log2f(MAX(this[i].pixel, 1)) - log2f(MAX(prev[i].pixel, 1));
            deltas[i] = (int)roundf(1000.0f * delta);
        }
        
        /* raw median, assumming identical black and white levels */
        expo_delta_median = median_int_wirth(deltas, N) / 1000.0f;
        
        /* normalized, considering the difference in black and white levels */
        expo_delta_median_normalized = expo_delta_median - log2f(white - black) + log2f(white_prev - black_prev);
        
        free(deltas); deltas = 0;
        
        /* estimate exposure difference from clipping point */
        int* clipping_points = malloc(N * sizeof(int)); if (!clipping_points) return;
        int nclip = 0;
        
        for (int i = 0; i < N; i++)
        {
            /* if the point is nearly saturated, check its horizontal position */
            if (this[i].pixel > white - black - 200 && this[i].pixel < white - black - 100)
            {
                clipping_points[nclip++] = prev[i].pixel;
            }
        }
        int clipping_point = nclip ? median_int_wirth(clipping_points, nclip) : 0;
        expo_delta_clip = clipping_point ? log2f(white_prev - black_prev) - log2f(clipping_point) : 0;
        free(clipping_points); clipping_points = 0;

        /* enforce min/max limits to trick auto-scaling */
        X[0] = log2f(min_adu); X[1] = 14;
        Y[0] = log2f(min_adu); Y[1] = 14;
        X[N/3] = log2f(min_adu); X[N/3+1] = 14;
        Y[N/3] = log2f(min_adu); Y[N/3+1] = 14;
        X[2*N/3] = log2f(min_adu); X[2*N/3+1] = 14;
        Y[2*N/3] = log2f(min_adu); Y[2*N/3+1] = 14;
        
        /* plot the grid */
        plot_dots_grid(X[0], X[1], 1, 0, 0, 480, 480);
        
        /* plot the curves */
        plot_dots(X, Y, N/3, 0, 0, 480, 480, COLOR_RED);
        plot_dots(X+N/3, Y+N/3, N/3, 0, 0, 480, 480, COLOR_GREEN1);
        plot_dots(X+2*N/3, Y+2*N/3, N/3, 0, 0, 480, 480, COLOR_BLUE);

        /* show white levels */
        int xw = scale_for_plot_dots_x(log2f(white_prev - black_prev), X[0], X[1], 0, 480);
        draw_line(xw, 0, xw, 480, COLOR_GRAY(50));
        int yw = scale_for_plot_dots_y(log2f(white - black), Y[0], Y[1], 0, 480);
        draw_line(0, yw, 480, yw, COLOR_GRAY(50));
        
        /* show clipping point */
        if (clipping_point)
        {
            int xclip = scale_for_plot_dots_x(log2f(clipping_point), X[0], X[1], 0, 480);
            draw_circle(xclip, yw, 10, COLOR_WHITE);
        }

        /* save the data to a Octave script */
        /* run it with: octave --persist RCURVEnn.M */
        int size = 1024*1024;
        char* msg = fio_malloc(size); if (!msg) return;
        msg[0] = 0;
        int len = 0;

        len += snprintf(msg+len, size-len, "a = [");
        for (int i = 0; i < N; i++)
            len += snprintf(msg+len, size-len, "%d ", prev[i].pixel);
        len += snprintf(msg+len, size-len, "];\n");

        len += snprintf(msg+len, size-len, "b = [");
        for (int i = 0; i < N; i++)
            len += snprintf(msg+len, size-len, "%d ", this[i].pixel);
        len += snprintf(msg+len, size-len, "];\n");

        len += snprintf(msg+len, size-len, "x = [");
        for (int i = 0; i < N; i++)
            len += snprintf(msg+len, size-len, "%d ", this[i].x);
        len += snprintf(msg+len, size-len, "];\n");

        len += snprintf(msg+len, size-len, "y = [");
        for (int i = 0; i < N; i++)
            len += snprintf(msg+len, size-len, "%d ", this[i].y);
        len += snprintf(msg+len, size-len, "];\n");

        len += snprintf(msg+len, size-len, 
            "R = mod(x,2) == 0 & mod(y,2) == 0;\n"
            "G = mod(x,2) ~= mod(y,2);\n"
            "B = mod(x,2) == 1 & mod(y,2) == 1;\n"
            "plot(log2(a(R)), log2(b(R)), '.r'); hold on;\n"
            "plot(log2(a(B)), log2(b(B)), '.b')\n"
            "plot(log2(a(G)), log2(b(G)), '.g')\n"
            "wa = prctile(a(:), 99);\n"
            "wb = prctile(b(:), 99);\n"
            "disp(sprintf('Exposure difference (median): %%.2f EV', median(real(log2(b/wb)-log2(a/wa)))))\n"
            "clip = median(a(b>wb-200 & b<wb-100));\n"
            "plot(log2(clip), log2(wb),'or', 'MarkerSize', 20);\n"
            "disp(sprintf('Exposure difference (clip): %%.2f EV', log2(wa)-log2(clip)))\n"
        );

        char mfile[100];
        snprintf(mfile, sizeof(mfile), "raw_diag/%s%04d/rcurve.m", get_file_prefix(), get_shooting_card()->file_number);
        FILE* f = FIO_CreateFile(mfile);
        FIO_WriteFile(f, msg, len);
        FIO_CloseFile(f);
        fio_free(msg);

        for (int i = 2; i < N; i++)
        {
            X[i] = log2f(MAX(prev[i].pixel, min_adu));
            Y[i] = log2f(MAX(this[i].pixel, min_adu));
        }
        
        free(X);
        free(Y);
    }
    else
    {
        clrscr();
        bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, 
            "Please take one more test picture of the same static scene.\n"
            "Use different settings, a solid tripod, and 10-second timer."
        );
    }

    static char prev_info[200] = "";
    char info[200];

    snprintf(info, sizeof(info),
        "ISO %d %s "SYM_F_SLASH"%s%d.%d",
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );
    
    if (ok)
    {
        int exp_med = (int)roundf(expo_delta_median * 100);
        int exp_med_nor = (int)roundf(expo_delta_median_normalized * 100);
        int exp_clip = (int)roundf(expo_delta_clip * 100);
        int noi = (int)roundf(noise * 100);
        int noi_prev = (int)roundf(noise_prev * 100);
        big_bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, 
            "2-shot comparison\n"
            "%s, %s\n"
            "X: %s\n"
            "Y: %s\n"
            "Black level X: %d\n"
            "White level X: %d\n"
            "Noise stdev X: %s%d.%02dEV\n"
            "Black level Y: %d\n"
            "White level Y: %d\n"
            "Noise stdev Y: %s%d.%02dEV\n"
            "Expo diff (med): %s%d.%02dEV\n"
            "Normalized: %s%d.%02dEV\n"
            "Expo diff (clip): %s%d.%02dEV\n"
            "Grid from %d to %d EV.\n",
            cam_model, video_mode_name,
            prev_info, info,
            (int)roundf(black_prev), white_prev,
            FMT_FIXEDPOINT2(noi_prev),
            (int)roundf(black), white,
            FMT_FIXEDPOINT2(noi),
            FMT_FIXEDPOINT2(exp_med), FMT_FIXEDPOINT2(exp_med_nor), FMT_FIXEDPOINT2(exp_clip),
            (int)roundf(log2f(min_adu)), 14
        );
    }

    snprintf(prev_info, sizeof(prev_info), "%s", info);
    black_prev = black;
    white_prev = white;
    noise_prev = noise;

    /* save data from this picture, to be used with the next one */
    dump_seg(this, data_size, prev_filename);
    
    /* cleanup */
    free(this);
    free(prev);
}

static int ob_zones_raw2bm_x(int x)
{
    /* 0...300: left OB, displayed on 0...600 */
    /* 300:end-50: active area, squashed on 600...620 */
    /* end-50...end: right OB, displayed on 620...720 */
    /* assume zoom 2x */
    
    if (x < 300)
    {
        return x * 2;
    }
    else if (x > raw_info.width - 50)
    {
        return 720 - (raw_info.width - x) * 2;
    }
    else
    {
        return 600 + (x - 300) * 20 / (raw_info.width - 50 - 300);
    }
}

static int ob_zones_raw2bm_y(int y)
{
    /* 0...190: top OB, displayed on 0...380 */
    /* 190:end-40: active area, squashed on 380...400 */
    /* end-40...end: bottom OB, displayed on 400...480 */
    /* assume zoom 2x */
    
    if (y < 190)
    {
        return y * 2;
    }
    else if (y > raw_info.height - 40)
    {
        return 480 - (raw_info.height - y) * 2;
    }
    else
    {
        return 380 + (y - 190) * 20 / (raw_info.height - 40 - 190);
    }
}

static void analyze_ob_zones()
{
    float black, noise;
    ob_mean_stdev(&black, &noise);
    int min = black - 2 * noise;
    int max = black + 2 * noise;

    int prev_bx = INT_MIN;
    int prev_by = INT_MIN;
    for (int y = 0; y < raw_info.height; y++)
    {
        int by = ob_zones_raw2bm_y(y);
        if (by == prev_by) continue;

        for (int x = 0; x < raw_info.width; x++)
        {
            int bx = ob_zones_raw2bm_x(x);
            if (bx == prev_bx) continue;

            /* draw without demosaicing */
            int p = raw_get_pixel(x, y);
            p = COERCE(p, min, max);
            int g = 100 * (p - min) / (max - min);
            int c = COLOR_GRAY(g);
            
            /* skip squashed areas */
            if (bx == prev_bx + 1 || by == prev_by + 1)
            {
                /* any nice and easy way to get a zig-zag pattern? */
                c = (bx/2 + by/2) % 10 == 0 ? COLOR_BLACK : COLOR_WHITE;
            }
            
            /* zoom 2x to keep things simple */
            bmp_putpixel(bx,   by,   c);
            bmp_putpixel(bx+1, by,   c);
            bmp_putpixel(bx,   by+1, c);
            bmp_putpixel(bx+1, by+1, c);
            
            prev_bx = bx;
        }
        prev_by = by;
    }
    
    /* show raw_info.active_area */
    int x1 = ob_zones_raw2bm_x(raw_info.active_area.x1);
    int x2 = ob_zones_raw2bm_x(raw_info.active_area.x2);
    int y1 = ob_zones_raw2bm_y(raw_info.active_area.y1);
    int y2 = ob_zones_raw2bm_y(raw_info.active_area.y2);
    bmp_draw_rect(COLOR_CYAN, x1, y1, x2-x1, y2-y1);

    /* round the values to 8-pixel multiples (area usable for raw video) */
    int x1r = ob_zones_raw2bm_x((raw_info.active_area.x1 + 7) & ~7);
    int x2r = ob_zones_raw2bm_x((raw_info.active_area.x2 + 0) & ~7);
    for (int y = y1; y < y2; y++)
    {
        if ((y / 4) % 2)
        {
            bmp_putpixel(x1r, y, COLOR_CYAN);
            bmp_putpixel(x2r, y, COLOR_CYAN);
        }
    }

    /* todo: define zones (camera-specific) and print some info about each zone */

    bmp_printf(FONT_MED | FONT_ALIGN_FILL, x1 + 50, y1 + 50,
        "OB zones\n"
        "%s, %s\n"
        "ISO %d %s "SYM_F_SLASH"%s%d.%d\n",
        cam_model, video_mode_name,
        lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );
    bmp_printf(FONT(FONT_MED, COLOR_CYAN, COLOR_BLACK) | FONT_ALIGN_FILL, x1 + 50, y1 + 50 + font_med.height*3,
        "Active area: %dx%d\n"
        "Usable for video: %d\n"
        "Skip L:%d R:%d T:%d B:%d",
        raw_info.active_area.x2 - raw_info.active_area.x1,
        raw_info.active_area.y2 - raw_info.active_area.y1,
        (raw_info.active_area.x2 & ~7) - ((raw_info.active_area.x1 + 7) & ~7),
        raw_info.active_area.x1, raw_info.width - raw_info.active_area.x2,
        raw_info.active_area.y1, raw_info.height - raw_info.active_area.y2
    );
}

/* todo: print stdev for overall dark frame and FPN components? */
static void FAST darkframe_grayscale()
{
    /* will downsample the entire raw buffer, line by line, and plot it progressively */
    
    /* assumming it's a dark frame, the stdev of a downsampled 22MPix to 720x480 would be 1/8 the stdev of the original image */
    /* for 18MPix, it's 1/7.2 */
    /* let's show around +/- 2*sigma; we'll compute the min and max in advance from OB because it's quick */
    float black, noise;
    ob_mean_stdev(&black, &noise);
    float min = black - noise/4;
    float max = black + noise/4;

    /* accumulators for each line */
    float* line = malloc(720 * sizeof(line[0]));
    int* num = malloc(720 * sizeof(num[0]));
    memset(line, 0, 720 * sizeof(line[0]));
    memset(num, 0, 720 * sizeof(num[0]));
    
    /* scan the raw buffer, stop if you get out of QR mode */
    int current_by = 0;
    for (int y = 0; y < raw_info.height && should_keep_going(); y++)
    {
        int by = RAW2BM_Y(y);
        if (by < 0 || by >= 480) continue;
        
        if (by != current_by)
        {
            /* plot previous line (completed) and reset the accumulators */
            for (int bx = 0; bx < 720; bx++)
            {
                float p = line[bx] / num[bx];
                p = COERCE(p, min, max);
                int g = 100 * (p - min) / (max - min);
                int c = COLOR_GRAY(g);
                bmp_putpixel(bx, current_by, c);
            }
            current_by = by;
            memset(line, 0, 720 * sizeof(line[0]));
            memset(num, 0, 720 * sizeof(num[0]));
        }

        /* average a few raw lines */
        for (int x = 0; x < raw_info.width; x++)
        {
            int bx = RAW2BM_X(x);
            if (bx < 0 || bx >= 720) continue;
            int p = raw_get_pixel(x, y);
            line[bx] += p;
            num[bx]++;
        }
    }
    free(line);
    free(num);
}

static char * raw_diag_get_numbered_file_name(const char * name, const char * extension)
{
    static char filename[FIO_MAX_PATH_LENGTH];
    
    /* note: we use a custom routine because we want a slightly different naming
     * compared to ML's get_numbered_file_name
     */
    int filenum = 0;
    do
    {
        char short_name[8+1];
        char suffix[10];
        /* first file is not numbered */
        if (filenum) snprintf(suffix, sizeof(suffix), "-%d%s", filenum, extension);
        else snprintf(suffix, sizeof(suffix), "%s", extension);
        /* number the screenshots, but make sure we fit the file name in 8 characters, trimming the original name if needed */
        snprintf(short_name, sizeof(short_name) - strlen(suffix) + 4, "%s", name);
        snprintf(filename, sizeof(filename), "raw_diag/%s%04d/%s%s", get_file_prefix(), get_shooting_card()->file_number, short_name, suffix);
        filenum++;
    }
    while (is_file(filename));
    
    return filename;
}

/* name: 8 chars please */
static void screenshot_if_needed(const char* name)
{
    if (auto_screenshot)
    {
        char * filename = raw_diag_get_numbered_file_name(name, ".ppm");
        take_screenshot(filename, SCREENSHOT_BMP);

        bmp_printf(FONT_SMALL, 0, 480 - font_small.height, "Saved %s.", filename);
    }
}

/* main raw diagnostic task */
static void raw_diag_task(int unused)
{
    int paused_lv = 0;
    
    if (image_review_time != 0xFF && !lv)
    {
        beep();
        NotifyBox(2000, "Set 'Image Review' to 'Hold' from Canon menu");
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
    
    /* get video mode name now, since we may change it later */
    video_mode_name = get_video_mode_name(0);
    
    if (lv)
    {
        /* run analyses with paused LiveView,
         * to make sure we look at a single image */
        PauseLiveView();
        paused_lv = 1;
    }
    
    if (analysis_ob_dr)
    {
        black_histogram(1);
        screenshot_if_needed("ob-dr");
        if (!should_keep_going()) goto end;
    }

    if (analysis_ob_zones)
    {
        analyze_ob_zones();
        screenshot_if_needed("ob-zones");
        if (!should_keep_going()) goto end;
    }

    if (analysis_darkframe_noise)
    {
        black_histogram(0);
        screenshot_if_needed("darkhist");
        if (!should_keep_going()) goto end;
    }
    
    if (analysis_darkframe_grayscale)
    {
        darkframe_grayscale();
        screenshot_if_needed("darkgray");
        if (!should_keep_going()) goto end;
    }

    if (analysis_darkframe_fpn)
    {
        darkframe_fpn();
        screenshot_if_needed("darkfpn");
        if (!should_keep_going()) goto end;
    }

    if (analysis_darkframe_fpn_xcov)
    {
        darkframe_fpn_xcov();
        screenshot_if_needed("darkfpnx");
        if (!should_keep_going()) goto end;
    }

    if (analysis_snr_curve)
    {
        snr_graph();
        screenshot_if_needed("snr");
        if (!should_keep_going()) goto end;
    }

    if (analysis_snr_curve_2_shots)
    {
        snr_graph_2_shots(0);
        screenshot_if_needed("snr2");
        if (!should_keep_going()) goto end;
    }

    if (analysis_noise_curve_2_shots)
    {
        snr_graph_2_shots(1);
        screenshot_if_needed("noise2");
        if (!should_keep_going()) goto end;
    }

    if (analysis_jpg_curve)
    {
        jpg_curve();
        screenshot_if_needed("jpg");
        if (!should_keep_going()) goto end;
    }

    if (analysis_compare_2_shots)
    {
        compare_2_shots(1);          /* show full shadow detail */
        screenshot_if_needed("cmp");
        if (!should_keep_going()) goto end;
    }

    if (analysis_compare_2_shots_highlights)
    {
        compare_2_shots(1024);       /* trim the bottom 10 stops and zoom on highlight detail */
        screenshot_if_needed("cmp-hl");
        if (!should_keep_going()) goto end;
    }
    
    if (dump_raw)
    {
        char * filename = raw_diag_get_numbered_file_name("raw", ".dng");

        /* make a copy of the raw buffer, because it's being updated while we are saving it */
        void* buf = malloc(raw_info.frame_size);
        ASSERT(buf);

        if (buf)
        {
            memcpy(buf, raw_info.buffer, raw_info.frame_size);
            struct raw_info local_raw_info = raw_info;
            local_raw_info.buffer = buf;
            save_dng(filename, &local_raw_info);
            free(buf);
            bmp_printf(FONT_MED, 0, 50, "Saved %s.   ", filename);
        }
    }

end:
    if (paused_lv && LV_PAUSED)
    {
        /* if we have paused LV, resume it */
        if (image_review_time)
        {
            /* image review setting from Canon menu */
            int preview_delay = image_review_time * 1000;
            int t0 = get_ms_clock();
            while (get_ms_clock() - t0 < preview_delay &&
                   !get_halfshutter_pressed())
            {
                msleep(10);
            }
        }
        else
        {
            msleep(2000);
        }
        ResumeLiveView();
    }
    raw_diag_running = 0;
}

static void raw_diag_run(int wait)
{
    if (raw_diag_running) return;
    raw_diag_running = 1;
    task_create("raw_diag_task", 0x1c, 0x1000, raw_diag_task, 0);
    if (wait)
    {
        while (raw_diag_running)
            msleep(100);
    }
}

/* trigger raw diagnostics after taking a picture */
PROP_HANDLER(PROP_GUI_STATE)
{
    uint32_t* data = buf;
    if (data[0] == GUISTATE_QR)
    {
        if (raw_diag_enabled)
            raw_diag_run(0);
    }
}

static unsigned int raw_diag_poll(unsigned int unused)
{
    if ((void*)&raw_lv_request == (void*)&ret_0)
    {
        /* no backend support for LiveView */
        return CBR_RET_CONTINUE;
    }

    /* quick analysis in LiveView */
    static int last_check = 0;
    if (raw_diag_enabled && lv &&                       /* enabled in LiveVew ... */
        !get_halfshutter_pressed() &&                   /* ... during idle times, i.e. when full analysis is not triggered */
        raw_lv_is_enabled() &&                          /* ... if raw video is enabled */
        !gui_menu_shown() &&                            /* ... outside ML menu (it's quickly erased otherwise) */
        should_run_polling_action(2000, &last_check))   /* ... no more often than once every 2 seconds */
    {
        raw_lv_request();
        if (raw_update_params())
        {
            quick_analysis_lv();
        }
        raw_lv_release();
    }

    if (raw_diag_enabled && lv && get_halfshutter_pressed())
    {
        msleep(500);
        if (!get_halfshutter_pressed())
        {
            return CBR_RET_CONTINUE;
        }
        beep();
        NotifyBox(5000, "Raw diag...");
        while (get_halfshutter_pressed()) msleep(100);
        idle_globaldraw_dis();
        NotifyBoxHide();
        msleep(1000);
        
        if (!lv)
        {
            beep();
            return CBR_RET_CONTINUE;
        }
        
        raw_lv_request();
        raw_diag_run(1);
        for (int i = 0; i < 200 && !get_halfshutter_pressed(); i++)
            msleep(100);
        raw_lv_release();
        beep();
        clrscr();
        idle_globaldraw_en();
        redraw();
    }
    return CBR_RET_CONTINUE;
}

static void test_shot()
{
    if (is_movie_mode())
    {
        msleep(1000);
        raw_lv_request();
        raw_diag_run(1);
        raw_lv_release();
        msleep(1000);
    }
    else
    {
        call("Release");
        msleep(10000);
    }
}

static void test_bracket()
{
    beep();
    msleep(5000);
    FIO_RemoveFile("RAWSAMPL.DAT");
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    menu_set_value_from_script("Debug", "ADTG Registers", 0);
    lens_set_rawshutter(SHUTTER_1_50);
    test_shot();

    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    menu_set_value_from_script("Debug", "ADTG Registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();
}

static void dummy_test_bracket()
{
    beep();
    msleep(5000);
    FIO_RemoveFile("RAWSAMPL.DAT");
    test_shot();
    test_shot();
}

static void reference_shot()
{
    beep();
    msleep(5000);
    FIO_RemoveFile("RAWSAMPL.DAT");
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_50);
    test_shot();
}

static void iso_experiment()
{
    /* shot 1: Canon 1/50 vs Canon 1/200 */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();

    /* shot 2: enable ADTG gain, but not CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();

    /* shot 3: enable ADTG gain and CMOS tweak too */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 1);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();

    /* shot 4: enable ADTG gain, nonlinear gain, but disable CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();

    /* shot 5: enable ADTG gain, nonlinear gain and CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 1);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    test_shot();
}

static void silent_zoom_bracket()
{
    if (!lv) return;
    beep();
    msleep(5000);
    menu_set_value_from_script("Debug", "RAW Diagnostics", 0);
    set_lv_zoom(1);
    SW1(1,300);
    SW1(0,0);
    msleep(2000);
    set_lv_zoom(5);
    msleep(2000);
    SW1(1,300);
    SW1(0,0);
    msleep(2000);
    set_lv_zoom(1);
    menu_set_value_from_script("Debug", "RAW Diagnostics", 1);
}


static struct menu_entry raw_diag_menu[] =
{
    {
        .name           = "RAW Diagnostics", 
        .priv           = &raw_diag_enabled, 
        .max            = 1,
        .submenu_width  = 710,
        .help           = "Show technical analysis of raw image data.",
        .help2          = "Enable this and take a picture with RAW image quality.",
        .children       = (struct menu_entry[])
        {
            {
                .name           = "Auto Screenshot",
                .priv           = &auto_screenshot,
                .max            = 1,
                .help           = "Auto-save a screenshot for every analysis.",
            },
            {
                .name           = "Optical Black Analyses",
                .select         = menu_open_submenu,
                .help           = "Raw analyses based on optical black areas.",
                .children       = (struct menu_entry[])
                {
                    {
                        .name           = "Optical Black + DR",
                        .priv           = &analysis_ob_dr,
                        .max            = 1,
                        .help           = "From OB: mean, stdev, histogram. Also white levels and DR.",
                        .help2          = "You need something overexposed in the image (e.g. a light bulb).",
                    },
                    {
                        .name           = "Optical Black Zones",
                        .priv           = &analysis_ob_zones,
                        .max            = 1,
                    },
                    MENU_EOL
                }
            },
            {
                .name           = "Dark Frame Analyses",
                .select         = menu_open_submenu,
                .help           = "Raw analyses based on dark frames (images taken with lens cap on).",
                .children       = (struct menu_entry[])
                {
                    {
                        .name           = "Dark Frame Noise",
                        .priv           = &analysis_darkframe_noise,
                        .max            = 1,
                        .help           = "Compute mean, stdev and histogram from the entire image.",
                        .help2          = "Make sure you take a dark frame (with lens cap on).",
                    },
                    {
                        .name           = "Dark Frame Grayscale",
                        .priv           = &analysis_darkframe_grayscale,
                        .max            = 1,
                        .help           = "Render the raw image as downsampled grayscale (reveals FPN).",
                        .help2          = "Make sure you take a dark frame (with lens cap on).",
                    },
                    {
                        .name           = "Dark Frame FPN",
                        .priv           = &analysis_darkframe_fpn,
                        .max            = 1,
                        .help           = "Fixed-pattern noise (banding) analysis.",
                        .help2          = "Make sure you take a dark frame (with lens cap on).",
                    },
                    {
                        .name           = "Dark Frame FPN xcov",
                        .priv           = &analysis_darkframe_fpn_xcov,
                        .max            = 1,
                        .help           = "Compare FPN between two successive dark frames.",
                        .help2          = "You will have to take two dark frames (with lens cap on).",
                    },
                    MENU_EOL,
                }
            },
            {
                .name           = "Curve-based Analyses",
                .select         = menu_open_submenu,
                .help           = "Raw analyses that attempts to identify response curves.",
                .children       = (struct menu_entry[])
                {
                    {
                        .name           = "SNR Curve",
                        .priv           = &analysis_snr_curve,
                        .max            = 1,
                        .help           = "Estimate the SNR curve (noise profile) from a defocused image.",
                        .help2          = "If the image is focused, detail will be misinterpreted as noise.",
                    },
                    {
                        .name           = "SNR Curve (2 shots)",
                        .priv           = &analysis_snr_curve_2_shots,
                        .max            = 1,
                        .help           = "Estimate the SNR curve (noise profile) from the difference between",
                        .help2          = "two identical test images of the same static scene (tripod required).",
                    },
                    {
                        .name           = "Noise Curve (2 shots)",
                        .priv           = &analysis_noise_curve_2_shots,
                        .max            = 1,
                        .help           = "Same as SNR curve (2 shots), but plot log2(noise_stdev) instead.",
                    },
                    {
                        .name           = "JPEG Curve",
                        .priv           = &analysis_jpg_curve,
                        .max            = 1,
                        .help           = "Plot the RAW to JPEG curves (R,G,B) used by current picture style.",
                        .help2          = "Color in input image should not change (e.g. lamp on a white wall).",
                    },
                    {
                        .name           = "Compare 2 Shots",
                        .priv           = &analysis_compare_2_shots,
                        .max            = 1,
                        .help           = "Compare 2 test images of the same static scene (solid tripod!)",
                        .help2          = "=> Exposure difference (median brightness, or from clipping point).",
                    },
                    {
                        .name           = "Compare 2 Shots HL",
                        .priv           = &analysis_compare_2_shots_highlights,
                        .max            = 1,
                        .help           = "Same analysis, but zoom on the highlights when plotting (top 4 EV).",
                        .help2          = "You need something overexposed in the image (e.g. a light bulb).",
                    },
                    MENU_EOL,
                }
            },
            {
                .name           = "Dump RAW Buffer",
                .priv           = &dump_raw,
                .max            = 1,
                .help           = "Save a DNG file (raw_diag/IMG_nnnn/raw.dng) after analysis.",
            },
            {
                .name           = "Bracket 1/50 vs 1/25 ADTG",
                .priv           = &test_bracket,
                .select         = (void (*)(void*,int))run_in_separate_task,
                .help           = "Shot 1: 1/50 with adtg_gui/iso_regs/mini_iso turned off, if loaded.",
                .help2          = "Shot 2: 1/25 with adtg_gui/iso_regs/mini_iso turned on, if loaded.",
            },
            {
                .name           = "Dummy Bracket",
                .priv           = &dummy_test_bracket,
                .select         = (void (*)(void*,int))run_in_separate_task,
                .help           = "Takes 2 shots with current settings.",
                .help2          = "Useful for tools that compare two test images of the same scene.",
            },
            {
                .name           = "Silent Zoom Bracket",
                .priv           = &silent_zoom_bracket,
                .select         = (void (*)(void*,int))run_in_separate_task,
                .help           = "Takes 2 silent pictures in LiveView, one normal, one with 5x zoom.",
                .help2          = "(this will not trigger any raw_diag analysis.)",
            },
            {
                .name           = "ISO Experiment",
                .priv           = &iso_experiment,
                .select         = (void (*)(void*,int))run_in_separate_task,
                .help           = "1: Canon 1/25 vs 1/50. 2: mini_iso ADTG gain. 3: also CMOS tweak.",
                .help2          = "4: enable iso_regs, disable CMOS tweak. 5: re-enable CMOS tweak.",
            },
            MENU_EOL,
        },
    },
};

static unsigned int raw_diag_init()
{
    /* skip "Canon" or "Canon EOS" from camera model name */
    cam_model = camera_model;
    if (strncmp(cam_model, "Canon ", 6) == 0) cam_model += 6;
    if (strncmp(cam_model, "EOS ", 4) == 0 && strlen(cam_model) >= 10) cam_model += 4;

    FIO_RemoveFile("RAWSAMPL.DAT");
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
    MODULE_CONFIG(auto_screenshot)
    MODULE_CONFIG(dump_raw)

    MODULE_CONFIG(analysis_ob_dr)
    MODULE_CONFIG(analysis_darkframe_noise)
    MODULE_CONFIG(analysis_darkframe_grayscale)
    MODULE_CONFIG(analysis_darkframe_fpn)
    MODULE_CONFIG(analysis_darkframe_fpn_xcov)
    MODULE_CONFIG(analysis_snr_curve)
    MODULE_CONFIG(analysis_snr_curve_2_shots)
    MODULE_CONFIG(analysis_noise_curve_2_shots)
    MODULE_CONFIG(analysis_jpg_curve)
    MODULE_CONFIG(analysis_compare_2_shots)
    MODULE_CONFIG(analysis_compare_2_shots_highlights)
    MODULE_CONFIG(analysis_ob_zones)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, raw_diag_poll, 0)
MODULE_CBRS_END()
