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

static CONFIG_INT("enabled", raw_diag_enabled, 0);
static CONFIG_INT("screenshot", auto_screenshot, 1);
static CONFIG_INT("dump_raw", dump_raw, 0);
static volatile int raw_diag_running = 0;

/* analysis types */
static CONFIG_INT("analysis.ob_dr", analysis_ob_dr, 1);   /* optical black and dynamic range */
static CONFIG_INT("analysis.dark.noise", analysis_darkframe_noise, 0);
static CONFIG_INT("analysis.dark.fpn", analysis_darkframe_fpn, 0);
static CONFIG_INT("analysis.dark.fpn.xcov", analysis_darkframe_fpn_xcov, 0);
static CONFIG_INT("analysis.snr", analysis_snr_curve, 0);
static CONFIG_INT("analysis.jpg", analysis_jpg_curve, 0);
static CONFIG_INT("analysis.cmp", analysis_compare_2_shots, 0);
static CONFIG_INT("analysis.cmp.hl", analysis_compare_2_shots_highlights, 0);

/* todo: move this functionality in core's take_screenshot and refactor it without dispcheck? */
static void custom_screenshot(char* filename)
{
    /* screenshots will be saved in one of these files (camera-specific) */
    FIO_RemoveFile("A:/VRAM0.BMP");
    FIO_RemoveFile("B:/VRAM0.BMP");
    FIO_RemoveFile("A:/TEST.BMP");
    FIO_RemoveFile("B:/TEST.BMP");
    
    /* this uses dispcheck, and the output file is not known (it's one from the above list) */
    take_screenshot(0);
    
    /* only one of these calls will succeed (of course, assuming no race conditions) */
    FIO_MoveFile("A:/VRAM0.BMP", filename);
    FIO_MoveFile("B:/VRAM0.BMP", filename);
    FIO_MoveFile("A:/TEST.BMP", filename);
    FIO_MoveFile("B:/TEST.BMP", filename);
}

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
    fpn = malloc(fpn_size); if (!fpn) return;
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
    float* fpnv = malloc(2*fpn_size); if (!fpnv) return;
    float* fpnh = fpnv + 10000;
    memset(fpnv, 0, fpn_size);
    memset(fpnh, 0, fpn_size);

    /* data from previous picture is taken from a file */
    char* prev_filename = "FPN.DAT";
    float* prev_fpnv = malloc(2*fpn_size); if (!prev_fpnv) return;
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
        "%s\n"
        "  ISO %d %s "SYM_F_SLASH"%s%d.%d", camera_model, lens_info.iso, lens_format_shutter(lens_info.raw_shutter), FMT_FIXEDPOINT1((int)lens_info.aperture)
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

static const char * get_numbered_file_name(char* pattern)
{
    static char filename[100];
    for (int num = 0; num < 10000; num++)
    {
        snprintf(filename, sizeof(filename), pattern, num);
        uint32_t size;
        if( FIO_GetFileSize( filename, &size ) != 0 ) return filename;
        if (size == 0) return filename;
    }

    snprintf(filename, sizeof(filename), pattern, 0);
    return filename;
}

static void compare_2_shots(int min_adu)
{
    clrscr();
    bmp_printf(FONT_MED | FONT_ALIGN_CENTER, 360, 200, "Please wait...\n(crunching numbers)");

    float black, noise;
    ob_mean_stdev(&black, &noise);

    /* get white level */
    int white = autodetect_white_level();

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
    struct test_pixel * this = malloc(data_size); if (!this) return;

    /* data from previous picture is taken from a file */
    char* prev_filename = "RAWSAMPL.DAT";
    struct test_pixel * prev = malloc(data_size); if (!prev) return;
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
        FILE* f = FIO_CreateFileEx(mfile);
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
            "%s\n"
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
            camera_model, prev_info, info,
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

/* name: 8 chars please */
static void screenshot_if_needed(const char* name)
{
    if (auto_screenshot)
    {
        char filename[100];
        snprintf(filename, sizeof(filename), "raw_diag/%s%04d/%s.bmp", get_file_prefix(), get_shooting_card()->file_number, name);
        custom_screenshot(filename);
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
    
    if (analysis_ob_dr)
    {
        black_histogram(1);
        screenshot_if_needed("ob-dr");
    }

    if (analysis_darkframe_noise)
    {
        black_histogram(0);
        screenshot_if_needed("darkhist");
    }

    if (analysis_darkframe_fpn)
    {
        darkframe_fpn();
        screenshot_if_needed("darkfpn");
    }

    if (analysis_darkframe_fpn_xcov)
    {
        darkframe_fpn_xcov();
        screenshot_if_needed("darkfpnx");
    }

    if (analysis_snr_curve)
    {
        snr_graph();
        screenshot_if_needed("snr");
    }

    if (analysis_jpg_curve)
    {
        jpg_curve();
        screenshot_if_needed("jpg");
    }

    if (analysis_compare_2_shots)
    {
        compare_2_shots(1);          /* show full shadow detail */
        screenshot_if_needed("cmp");
    }

    if (analysis_compare_2_shots_highlights)
    {
        compare_2_shots(1024);       /* trim the bottom 10 stops and zoom on highlight detail */
        screenshot_if_needed("cmp-hl");
    }
    
    if (dump_raw)
    {
        char filename[100];
        snprintf(filename, sizeof(filename), "raw_diag/%s%04d/raw.dng", get_file_prefix(), get_shooting_card()->file_number);
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

static void test_bracket()
{
    beep();
    msleep(5000);
    FIO_RemoveFile("RAWSAMPL.DAT");
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_50);
    call("Release");
    msleep(10000);

    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);
}

static void reference_shot()
{
    beep();
    msleep(5000);
    FIO_RemoveFile("RAWSAMPL.DAT");
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_50);
    call("Release");
    msleep(5000);
}

static void iso_experiment()
{
    /* shot 1: Canon 1/50 vs Canon 1/200 */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 0);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);

    /* shot 2: enable ADTG gain, but not CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);

    /* shot 3: enable ADTG gain and CMOS tweak too */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 1);
    menu_set_value_from_script("Debug", "ISO registers", 0);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);

    /* shot 4: enable ADTG gain, nonlinear gain, but disable CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 0);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);

    /* shot 5: enable ADTG gain, nonlinear gain and CMOS tweak */
    reference_shot();
    menu_set_value_from_script("Expo", "Mini ISO", 1);
    menu_set_value_from_script("Mini ISO", "CMOS tweak", 1);
    menu_set_value_from_script("Debug", "ISO registers", 1);
    lens_set_rawshutter(SHUTTER_1_25);
    call("Release");
    msleep(5000);
}

static struct menu_entry raw_diag_menu[] =
{
    {
        .name = "RAW Diagnostics", 
        .priv = &raw_diag_enabled, 
        .max = 1,
        .submenu_width = 710,
        .help  = "Show technical analysis of raw image data.",
        .help2 = "Enable this and take a picture with RAW image quality.",
        .children =  (struct menu_entry[]) {
            {
                .name = "Auto screenshot",
                .priv = &auto_screenshot,
                .max = 1,
                .help = "Auto-save a screenshot for every analysis."
            },
            {
                .name  = "Optical black + DR",
                .priv  = &analysis_ob_dr,
                .max   = 1,
                .help  = "From OB: mean, stdev, histogram. Also white levels and DR.",
                .help2 = "You need something overexposed in the image (e.g. a light bulb).",
            },
            {
                .name  = "Dark frame noise",
                .priv  = &analysis_darkframe_noise,
                .max   = 1,
                .help  = "Compute mean, stdev and histogram from the entire image.",
                .help2 = "Make sure you take a dark frame (with lens cap on).",
            },
            {
                .name  = "Dark frame FPN",
                .priv  = &analysis_darkframe_fpn,
                .max   = 1,
                .help  = "Fixed-pattern noise (banding) analysis.",
                .help2 = "Make sure you take a dark frame (with lens cap on).",
            },
            {
                .name  = "Dark frame FPN xcov",
                .priv  = &analysis_darkframe_fpn_xcov,
                .max   = 1,
                .help  = "Compare FPN between two successive dark frames.",
                .help2 = "You will have to take two dark frames (with lens cap on).",
            },
            {
                .name  = "SNR curve",
                .priv  = &analysis_snr_curve,
                .max   = 1,
                .help  = "Estimate the SNR curve (noise profile) from a defocused image.",
                .help2 = "If the image is focused, detail will be misinterpreted as noise.",
            },
            {
                .name  = "JPEG curve",
                .priv  = &analysis_jpg_curve,
                .max   = 1,
                .help  = "Plot the RAW to JPEG curves (R,G,B) used by current picture style.",
                .help2 = "Color in input image should not change (e.g. lamp on a white wall).",
            },
            {
                .name  = "Compare 2 shots",
                .priv  = &analysis_compare_2_shots,
                .max   = 1,
                .help  = "Compare 2 test images of the same static scene (solid tripod!)",
                .help2 = "=> exposure difference (median brightness, or from clipping point).",
            },
            {
                .name  = "Compare 2 shots HL",
                .priv  = &analysis_compare_2_shots_highlights,
                .max   = 1,
                .help  = "Same analysis, but zoom on the highlights when plotting (top 4 EV).",
                .help2 = "You need something overexposed in the image (e.g. a light bulb).",
            },
            {
                .name = "Dump RAW buffer",
                .priv = &dump_raw,
                .max = 1,
                .help = "Save a DNG file (raw_diag/IMG_nnnn/raw.dng) after analysis."
            },
            {
                .name = "Test bracket",
                .priv = &test_bracket,
                .select = (void (*)(void*,int))run_in_separate_task,
                .help = "Shot 1: 1/50 with iso_regs and mini_iso turned off.",
                .help2 = "Shot 2: 1/25 with iso_regs and mini_iso turned on, if loaded."
            },
            {
                .name = "ISO experiment",
                .priv = &iso_experiment,
                .select = (void (*)(void*,int))run_in_separate_task,
                .help = "1: Canon 1/25 vs 1/50. 2: mini_iso ADTG gain; 3: also CMOS tweak.",
                .help2 = "4: enable iso_regs, disable CMOS tweak. 5: re-enable CMOS tweak."
            },
            MENU_EOL,
        },
    },
};

static unsigned int raw_diag_init()
{
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
    MODULE_CONFIG(analysis_darkframe_fpn)
    MODULE_CONFIG(analysis_darkframe_fpn_xcov)
    MODULE_CONFIG(analysis_snr_curve)
    MODULE_CONFIG(analysis_jpg_curve)
    MODULE_CONFIG(analysis_compare_2_shots)
    MODULE_CONFIG(analysis_compare_2_shots_highlights)
MODULE_CONFIGS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, raw_diag_poll, 0)
MODULE_CBRS_END()
