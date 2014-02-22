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
static CONFIG_INT("dump_raw", dump_raw, 0);
static volatile int raw_diag_running = 0;

#define ANALYSIS_OPTICAL_BLACK 0
#define ANALYSIS_DARKFRAME_NOISE 1
#define ANALYSIS_DARKFRAME_FPN 2
#define ANALYSIS_DARKFRAME_FPN_XCOV 3
#define ANALYSIS_SNR_CURVE 4
#define ANALYSIS_JPG_CURVE 5

/* a float version of the routine from raw.c (should be more accurate) */
static void FAST autodetect_black_level_calc(int x1, int x2, int y1, int y2, int dx, int dy, float* out_mean, float* out_stdev)
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

static void FAST ob_mean_stdev(float* mean, float* stdev)
{
    int x1 = 16;
    int x2 = raw_info.active_area.x1 - 24;
    int y1 = raw_info.active_area.y1 + 20;
    int y2 = raw_info.active_area.y2 - 20;

    autodetect_black_level_calc(x1, x2, y1, y2, 1, 1, mean, stdev);
}

/* large histogram of the left optical black area */
static void FAST black_histogram(int ob)
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
    int canon_white = shamem_read(0xC0F12054) >> 16;
    
    if (ob)
    {
        bmp_printf(FONT_MED, 0, 0, "Optical black: mean %d, stdev %s%d.%02d ", mean_r, FMT_FIXEDPOINT2(stdev_r));
        int dr = (int)roundf((log2f(white - mean) - log2f(stdev)) * 1000.0);
        bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, "White level: %d (%d) ", white, canon_white);
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
        "%s\n"
        "ISO %d %s "SYM_F_SLASH"%s%d.%d",
        get_picstyle_name(lens_info.raw_picstyle),
        lens_get_from_other_picstyle_sharpness(i),
        lens_get_from_other_picstyle_contrast(i),
        ABS(lens_get_from_other_picstyle_saturation(i)) < 10 ? lens_get_from_other_picstyle_saturation(i) : 0,
        ABS(lens_get_from_other_picstyle_color_tone(i)) < 10 ? lens_get_from_other_picstyle_color_tone(i) : 0,
        camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    int x1 = raw_info.active_area.x1;
    int y1 = raw_info.active_area.y1;
    int x2 = raw_info.active_area.x2;
    int y2 = raw_info.active_area.y2;

    uint32_t* lv = (uint32_t*) get_yuv422_vram()->vram;

    int colors[4] = {COLOR_RED, COLOR_GREEN1, COLOR_GREEN1, COLOR_LIGHT_BLUE};
    for (int k = 0; k < 100000 && gui_state == GUISTATE_QR; k++)
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

    /* any 100-megapixel cameras out there? */
    float* fpn; int fpn_size = 10000 * sizeof(fpn[0]);
    fpn = malloc(fpn_size);
    memset(fpn, 0, fpn_size);
    
    compute_fpn_v(fpn);
    
    bmp_fill(COLOR_BG_DARK, 0, 0, 720, 230+20);
    float* fpn_x1 = fpn + raw_info.active_area.x1;
    int fpn_N = raw_info.active_area.x2 - raw_info.active_area.x1;
    plot(fpn_x1, fpn_N, 10, 25, 700, 220);
    int s = (int) roundf(std(fpn_x1, fpn_N) * 100.0);
    bmp_printf(FONT_MED, 15, 30, "Vertical FPN: stdev=%s%d.%02d", FMT_FIXEDPOINT2(s));

    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 300, "Please wait...\n(crunching numbers)");

    memset(fpn, 0, fpn_size);
    compute_fpn_h(fpn);

    bmp_fill(COLOR_BG_DARK, 0, 250, 720, 230);
    float* fpn_y1 = fpn + raw_info.active_area.y1;
    fpn_N = raw_info.active_area.y2 - raw_info.active_area.y1;
    plot(fpn_y1, fpn_N, 10, 255, 700, 220);
    s = (int) roundf(std(fpn_y1, fpn_N) * 100.0);
    bmp_printf(FONT_MED, 15, 260, "Horizontal FPN: stdev=%s%d.%02d", FMT_FIXEDPOINT2(s));

    bmp_printf(FONT_MED, 0, 0, 
        "Fixed-pattern noise"
    );

    bmp_printf(FONT_MED | FONT_ALIGN_RIGHT, 720, 0, 
        "%s\n"
        "  ISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    free(fpn);
}

static void plot_xcov(float* X, float* Y, int n, int x0, int y0, int w, int h)
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
        int xc = x0 + (0 - min) * w / (max - min);
        int yc = y0 + h - (0 - min) * h / (max - min);
        draw_line(x0, yc, x0+w, yc, COLOR_GRAY(50));
        draw_line(xc, y0, xc, y0+h, COLOR_GRAY(50));
    }
    
    for (int i = 0; i < n; i++)
    {
        int px = x0 + (X[i] - min) * w / (max - min);
        int py = y0 + h - (Y[i] - min) * h / (max - min);
        bmp_putpixel(px, py, COLOR_LIGHT_BLUE);
    }
    
    /* todo: compute the cross-covariance and print it */
}

static void darkframe_fpn_xcov()
{
    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please wait...\n(crunching numbers)");

    /* any 100-megapixel cameras out there? */
    int fpn_size = 10000 * sizeof(float);
    float* fpnv = malloc(2*fpn_size);
    float* fpnh = fpnv + 10000;
    memset(fpnv, 0, fpn_size);
    memset(fpnh, 0, fpn_size);

    /* data from previous picture is taken from a file */
    static char prev_filename[100];
    snprintf(prev_filename, sizeof(prev_filename), "%sFPN.DAT", MODULE_CARD_DRIVE);
    float* prev_fpnv = malloc(2*fpn_size);
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
        plot_xcov(prev_fpnv+x1, fpnv+x1, N, 0, 120, 360, 360);
    }

    compute_fpn_h(fpnh);
    if (ok)
    {
        bmp_fill(COLOR_BG_DARK, 360, 0, 360, 480);
        bmp_printf(FONT_MED, 360, 90, "Horizontal FPN");

        int y1 = raw_info.active_area.y1;
        int N = raw_info.active_area.y2 - raw_info.active_area.y1;
        plot_xcov(prev_fpnh+y1, fpnh+y1, N, 360, 120, 360, 360);
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
        "%s\n"
        "  ISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
    );

    /* save data from this picture, to be used with the next one */
    dump_seg(fpnv, 2*fpn_size, prev_filename);
    
    /* cleanup */
    free(fpnv);
    free(prev_fpnv);
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
    
    switch (analysis_type)
    {
        case ANALYSIS_OPTICAL_BLACK:
            black_histogram(1);
            break;
        case ANALYSIS_DARKFRAME_NOISE:
            black_histogram(0);
            break;
        case ANALYSIS_DARKFRAME_FPN:
            darkframe_fpn();
            break;
        case ANALYSIS_DARKFRAME_FPN_XCOV:
            darkframe_fpn_xcov();
            break;
        case ANALYSIS_SNR_CURVE:
            snr_graph();
            break;
        case ANALYSIS_JPG_CURVE:
            jpg_curve();
            break;
    }
    
    if (auto_screenshot)
    {
        take_screenshot(0);
    }
    
    if (dump_raw)
    {
        char filename[100];
        snprintf(filename, sizeof(filename), "%sML/LOGS/raw_diag.dng", MODULE_CARD_DRIVE);
        bmp_printf(FONT_MED, 0, 50, "Saving %s...", filename);
        save_dng(filename, &raw_info);
        bmp_printf(FONT_MED, 0, 50, "Saved %s.   ", filename);
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
                .max = 5,
                .choices = CHOICES("Optical black noise", "Dark frame noise", "Dark frame FPN", "Dark frame FPN xcov", "SNR curve", "JPEG curve"),
                .help  = "Choose the type of analysis you wish to run:",
                .help2 = "Optical black noise: mean, stdev, histogram.\n"
                         "Dark frame noise: same as OB noise, but from the entire image.\n"
                         "Dark frame FPN: fixed-pattern noise (banding) analysis.\n"
                         "Dark frame FPN xcov: how much FPN changes between 2 shots?\n"
                         "SNR curve: take a defocused picture to see the noise profile.\n"
                         "JPEG curve: plot the RAW to JPEG curve used by current PicStyle.\n"
            },
            {
                .name = "Auto screenshot",
                .priv = &auto_screenshot,
                .max = 1,
                .help = "Auto-save a screenshot for every test image."
            },
            {
                .name = "Dump RAW buffer",
                .priv = &dump_raw,
                .max = 1,
                .help = "Save a DNG file (ML/LOGS/raw_diag.dng) after analysis."
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
    MODULE_CONFIG(dump_raw)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, raw_diag_poll, 0)
MODULE_CBRS_END()
