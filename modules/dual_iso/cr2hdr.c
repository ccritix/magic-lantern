/**
 * Post-process CR2 images obtained with the Dual ISO module
 * (deinterlace, blend the two exposures, output a 16-bit DNG with much cleaner shadows)
 * 
 * Technical details: https://dl.dropboxusercontent.com/u/4124919/bleeding-edge/isoless/dual_iso.pdf
 */
/*
 * Copyright (C) 2013 Magic Lantern Team
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

/* choose interpolation method (define only one of these) */
#define INTERP_MEAN23
#undef INTERP_MEAN_6
#undef INTERP_MEAN_4_OUT_OF_6
#undef INTERP_MEAN_5_OUT_OF_6
#undef INTERP_MEDIAN_6
#undef INTERP_MEAN23_EDGE

/* post interpolation enhancements */
//~ #define VERTICAL_SMOOTH
//~ #define MEAN7_SMOOTH
//~ #define MEDIAN7_SMOOTH
#define CHROMA_SMOOTH
#define CONTRAST_BLEND

/* minimizes aliasing while ignoring the other factors (e.g. shadow noise, banding) */
/* useful for debugging */
//~ #define FULLRES_ONLY

#if defined(VERTICAL_SMOOTH) || defined(MEAN7_SMOOTH) || defined(MEDIAN7_SMOOTH) || defined(CHROMA_SMOOTH)
#define SMOOTH
#endif

#ifdef FULLRES_ONLY
#undef CONTRAST_BLEND /* this will only waste CPU cycles */
/* it will still benefit from chroma smoothing, so leave it on */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "../../src/raw.h"
#include "qsort.h"  /* much faster than standard C qsort */

#include "optmed.h"

#define FAIL(fmt,...) { fprintf(stderr, "Error: "); fprintf(stderr, fmt, ## __VA_ARGS__); fprintf(stderr, "\n"); exit(1); }
#define CHECK(ok, fmt,...) { if (!ok) FAIL(fmt, ## __VA_ARGS__); }

#define COERCE(x,lo,hi) MAX(MIN((x),(hi)),(lo))
#define COUNT(x)        ((int)(sizeof(x)/sizeof((x)[0])))

#define MIN(a,b) \
   ({ typeof ((a)+(b)) _a = (a); \
      typeof ((a)+(b)) _b = (b); \
     _a < _b ? _a : _b; })

#define MAX(a,b) \
   ({ typeof ((a)+(b)) _a = (a); \
       typeof ((a)+(b)) _b = (b); \
     _a > _b ? _a : _b; })

#define ABS(a) \
   ({ __typeof__ (a) _a = (a); \
     _a > 0 ? _a : -_a; })

#define CAM_COLORMATRIX1                       \
 6722, 10000,     -635, 10000,    -963, 10000, \
-4287, 10000,    12460, 10000,    2028, 10000, \
 -908, 10000,     2162, 10000,    5668, 10000

struct raw_info raw_info = {
    .api_version = 1,
    .bits_per_pixel = 16,
    .black_level = 2048,
    .white_level = 15000,
    .cfa_pattern = 0x02010100,          // Red  Green  Green  Blue
    .calibration_illuminant1 = 1,       // Daylight
    .color_matrix1 = {CAM_COLORMATRIX1},
};

static int hdr_check();
static int hdr_interpolate();
static int black_subtract(int left_margin, int top_margin);
static int black_subtract_simple(int left_margin, int top_margin);
static int white_detect();

static inline int raw_get_pixel16(int x, int y) {
    unsigned short * buf = raw_info.buffer;
    int value = buf[x + y * raw_info.width];
    return value;
}

static inline int raw_set_pixel16(int x, int y, int value)
{
    unsigned short * buf = raw_info.buffer;
    buf[x + y * raw_info.width] = value;
    return value;
}

int raw_get_pixel(int x, int y) {
    return raw_get_pixel16(x,y);
}

/* from 14 bit to 16 bit */
int raw_get_pixel_14to16(int x, int y) {
    return (raw_get_pixel16(x,y) << 2) & 0xFFFF;
}

static int startswith(char* str, char* prefix)
{
    char* s = str;
    char* p = prefix;
    for (; *p; s++,p++)
        if (*s != *p) return 0;
    return 1;
}

static void reverse_bytes_order(char* buf, int count)
{
    unsigned short* buf16 = (unsigned short*) buf;
    int i;
    for (i = 0; i < count/2; i++)
    {
        unsigned short x = buf16[i];
        buf[2*i+1] = x;
        buf[2*i] = x >> 8;
    }
}

int main(int argc, char** argv)
{
    int k;
    int r;
    for (k = 1; k < argc; k++)
    {
        char* filename = argv[k];

        printf("\nInput file     : %s\n", filename);

        char dcraw_cmd[100];
        snprintf(dcraw_cmd, sizeof(dcraw_cmd), "dcraw -v -i -t 0 \"%s\" > tmp.txt", filename);
        int exit_code = system(dcraw_cmd);
        CHECK(exit_code == 0, "%s", filename);
        
        FILE* t = fopen("tmp.txt", "rb");
        CHECK(t, "tmp.txt");
        int raw_width = 0, raw_height = 0;
        int out_width = 0, out_height = 0;
        
        char line[100];
        while (fgets(line, sizeof(line), t))
        {
            if (startswith(line, "Full size: "))
            {
                r = sscanf(line, "Full size: %d x %d\n", &raw_width, &raw_height);
                CHECK(r == 2, "sscanf");
            }
            else if (startswith(line, "Output size: "))
            {
                r = sscanf(line, "Output size: %d x %d\n", &out_width, &out_height);
                CHECK(r == 2, "sscanf");
            }
        }
        fclose(t);

        printf("Full size      : %d x %d\n", raw_width, raw_height);
        printf("Active area    : %d x %d\n", out_width, out_height);
        
        int left_margin = raw_width - out_width;
        int top_margin = raw_height - out_height;

        snprintf(dcraw_cmd, sizeof(dcraw_cmd), "dcraw -4 -E -c -t 0 \"%s\" > tmp.pgm", filename);
        exit_code = system(dcraw_cmd);
        CHECK(exit_code == 0, "%s", filename);
        
        FILE* f = fopen("tmp.pgm", "rb");
        CHECK(f, "tmp.pgm");
        
        char magic0, magic1;
        r = fscanf(f, "%c%c\n", &magic0, &magic1);
        CHECK(r == 2, "fscanf");
        CHECK(magic0 == 'P' && magic1 == '5', "pgm magic");
        
        int width, height;
        r = fscanf(f, "%d %d\n", &width, &height);
        CHECK(r == 2, "fscanf");
        CHECK(width == raw_width, "pgm width");
        CHECK(height == raw_height, "pgm height");
        
        int max_value;
        r = fscanf(f, "%d\n", &max_value);
        CHECK(r == 1, "fscanf");
        CHECK(max_value == 65535, "pgm max");

        void* buf = malloc(width * (height+1) * 2); /* 1 extra line for handling GBRG easier */
        fseek(f, -width * height * 2, SEEK_END);
        int size = fread(buf, 1, width * height * 2, f);
        CHECK(size == width * height * 2, "fread");
        fclose(f);

        /* PGM is big endian, need to reverse it */
        reverse_bytes_order(buf, width * height * 2);

        raw_info.buffer = buf;
        
        /* did we read the PGM correctly? (right byte order etc) */
        //~ int i;
        //~ for (i = 0; i < 10; i++)
            //~ printf("%d ", raw_get_pixel16(i, 0));
        //~ printf("\n");
        
        raw_info.width = width;
        raw_info.height = height;
        raw_info.pitch = width * 2;
        raw_info.frame_size = raw_info.height * raw_info.pitch;

        raw_info.active_area.x1 = left_margin;
        raw_info.active_area.x2 = raw_info.width;
        raw_info.active_area.y1 = top_margin;
        raw_info.active_area.y2 = raw_info.height;
        raw_info.jpeg.x = 0;
        raw_info.jpeg.y = 0;
        raw_info.jpeg.width = raw_info.width - left_margin;
        raw_info.jpeg.height = raw_info.height - top_margin;

        if (hdr_check())
        {
            white_detect();
            if (!black_subtract(left_margin, top_margin))
                printf("Black subtract didn't work\n");

            /* will use 16 bit processing and output, instead of 14 */
            raw_info.black_level *= 4;
            raw_info.white_level *= 4;

            if (hdr_interpolate())
            {
                char out_filename[1000];
                snprintf(out_filename, sizeof(out_filename), "%s", filename);
                int len = strlen(out_filename);
                out_filename[len-3] = 'D';
                out_filename[len-2] = 'N';
                out_filename[len-1] = 'G';

                /* run a second black subtract pass, to fix whatever our funky processing may do to blacks */
                black_subtract_simple(left_margin, top_margin);

                reverse_bytes_order(raw_info.buffer, raw_info.frame_size);

                printf("Output file    : %s\n", out_filename);
                save_dng(out_filename);

                char exif_cmd[100];
                snprintf(exif_cmd, sizeof(exif_cmd), "exiftool -tagsFromFile \"%s\" -all:all \"%s\" -overwrite_original", filename, out_filename);
                int r = system(exif_cmd);
                if (r != 0)
                    printf("Exiftool didn't work\n");
            }
            else
            {
                printf("ISO blending didn't work\n");
            }

            raw_info.black_level /= 4;
            raw_info.white_level /= 4;
        }
        else
        {
            printf("Doesn't look like interlaced ISO\n");
        }
        
        unlink("tmp.pgm");
        unlink("tmp.txt");
        
        free(buf);
    }
    
    return 0;
}

static int white_detect_brute_force()
{
    /* sometimes the white level is much lower than 15000; this would cause pink highlights */
    /* workaround: consider the white level as a little under the maximum pixel value from the raw file */
    /* caveat: bright and dark exposure may have different white levels, so we'll take the minimum value */
    /* side effect: if the image is not overexposed, it may get brightened a little; shouldn't hurt */
    
    /* ignore hot pixels when finding white level (at least 10 pixels should confirm it) */
    
    int white = raw_info.white_level * 5 / 6;
    int whites[4] = {white, white, white, white};
    int maxies[4] = {white, white, white, white};
    int counts[4] = {0, 0, 0, 0};

    int x,y;
    for (y = raw_info.active_area.y1; y < raw_info.active_area.y2; y ++)
    {
        for (x = raw_info.active_area.x1; x < raw_info.active_area.x2; x ++)
        {
            int pix = raw_get_pixel16(x, y);
            if (pix > maxies[y%4])
            {
                maxies[y%4] = pix;
                counts[y%4] = 1;
            }
            else if (pix == maxies[y%4])
            {
                counts[y%4]++;
                if (counts[y%4] > 10)
                {
                    whites[y%4] = maxies[y%4];
                }
            }
        }
    }
    
    white = MIN(MIN(whites[0], whites[1]), MIN(whites[2], whites[3]));
    raw_info.white_level = white - 100;
    printf("White level    : %d\n", raw_info.white_level);
    return 1;
}

static int white_detect()
{
    return white_detect_brute_force();

#if 0
    int w = raw_info.width;
    int p0 = raw_get_pixel16(0, 0);
    if (p0 < 10000) return 0;
    int x;
    for (x = 0; x < w; x++)
        if (raw_get_pixel16(x, 0) != p0)
            return 0;

    /* first line is white level, cool! */
    raw_info.white_level = p0 - 1000;       /* pink pixels at aggressive values */
    printf("White level    : %d\n", raw_info.white_level);
    return 1;
#endif
}


static int black_subtract(int left_margin, int top_margin)
{
    if (left_margin < 10) return 0;
    if (top_margin < 10) return 0;

#if 0
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("untouched.dng");
#endif

    printf("Black borders  : %d left, %d top\n", left_margin, top_margin);

    int w = raw_info.width;
    int h = raw_info.height;
    
    int* vblack = malloc(h * sizeof(int));
    int* hblack = malloc(w * sizeof(int));
    int* aux = malloc(MAX(w,h) * sizeof(int));
    unsigned short * blackframe = malloc(w * h * sizeof(unsigned short));
    
    CHECK(vblack, "malloc");
    CHECK(hblack, "malloc");
    CHECK(blackframe, "malloc");

    /* data above this may be gibberish */
    int ymin = (top_margin*3/4) & ~3;

    /* estimate vertical correction for each line */
    int x,y;
    for (y = 0; y < h; y++)
    {
        int avg = 0;
        int num = 0;
        for (x = 2; x < left_margin - 8; x++)
        {
            avg += raw_get_pixel16(x, y);
            num++;
        }
        vblack[y] = avg / num;
    }
    
    /* perform some slight filtering (averaging) so we don't add noise to the image */
    for (y = 0; y < h; y++)
    {
        int y2;
        int avg = 0;
        int num = 0;
        for (y2 = y - 10*4; y2 < y + 10*4; y2 += 4)
        {
            if (y2 < ymin) continue;
            if (y2 >= h) continue;
            avg += vblack[y2];
            num++;
        }
        if (num > 0)
        {
            avg /= num;
            aux[y] = avg;
        }
        else
        {
            aux[y] = vblack[y];
        }
    }
    
    memcpy(vblack, aux, h * sizeof(vblack[0]));

    /* update the dark frame */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            blackframe[x + y*w] = vblack[y];
    
    /* estimate horizontal drift for each channel */
    int k;
    for (k = 0; k < 4; k++)
    {
        int y0 = ymin + k;
        int offset = 0;
        {
            int num = 0;
            for (y = y0; y < top_margin-4; y += 4)
            {
                offset += blackframe[y*w];
                num++;
            }
            offset /= num;
        }

        for (x = 0; x < w; x++)
        {
            int num = 0;
            int avg = 0;
            for (y = y0; y < top_margin-4; y += 4)
            {
                avg += raw_get_pixel16(x, y) - offset;
                num++;
            }
            hblack[x] = avg / num;
        }
        
        /* perform some stronger filtering (averaging), since this data is a lot noisier */
        /* if we don't do that, we will add some strong FPN to the image */
        for (x = 0; x < w; x++)
        {
            int x2;
            int avg = 0;
            int num = 0;
            for (x2 = x - 1000; x2 < x + 1000; x2 ++)
            {
                if (x2 < 0) continue;
                if (x2 >= w) continue;
                avg += hblack[x2];
                num++;
            }
            avg /= num;
            aux[x] = avg;
        }
        memcpy(hblack, aux, w * sizeof(hblack[0]));

        /* update the dark frame */
        for (y = y0; y < h; y += 4)
            for (x = 0; x < w; x++)
                blackframe[x + y*w] += hblack[x];
    }

#if 0 /* for debugging only */
    void* old_buffer = raw_info.buffer;
    raw_info.buffer = blackframe;
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("black.dng");
    raw_info.buffer = old_buffer;
#endif

    /* "subtract" the dark frame, keeping the exif black level and preserving the white level */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int p = raw_get_pixel16(x, y);
            int black_delta = raw_info.black_level - blackframe[x + y*w];
            p += black_delta;
            p = p * raw_info.white_level / (raw_info.white_level + black_delta);
            p = COERCE(p, 0, 16383);
            raw_set_pixel16(x, y, p);
        }
    }

#if 0
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("subtracted.dng");
#endif

    free(vblack);
    free(hblack);
    free(blackframe);
    free(aux);
    return 1;
}


static int black_subtract_simple(int left_margin, int top_margin)
{
    if (left_margin < 10) return 0;
    if (top_margin < 10) return 0;
    
    int w = raw_info.width;
    int h = raw_info.height;

    /* average left bar */
    int x,y;
    long long avg = 0;
    int num = 0;
    for (y = 0; y < h; y++)
    {
        for (x = 2; x < left_margin - 8; x++)
        {
            int p = raw_get_pixel16(x, y);
            if (p > 0)
            {
                avg += p;
                num++;
            }
        }
    }
    
    int new_black = avg / num;
        
    int black_delta = raw_info.black_level - new_black;
    
    printf("Black adjust   : %d\n", (int)black_delta);

    /* "subtract" the dark frame, keeping the exif black level and preserving the white level */
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            double p = raw_get_pixel16(x, y);
            if (p > 0)
            {
                p += black_delta;
                p = p * raw_info.white_level / (raw_info.white_level + black_delta);
                p = COERCE(p, 0, 65535);
                raw_set_pixel16(x, y, p);
            }
        }
    }
    
    return 1;
}

/* quick check to see if this looks like a HDR frame */
static int hdr_check()
{
    int black = raw_info.black_level;
    int white = raw_info.white_level;

    int w = raw_info.width;
    int h = raw_info.height;

    static double raw2ev[16384];
    
    int i;
    for (i = 0; i < 16384; i++)
        raw2ev[i] = log2(MAX(1, i - black));

    int x, y;
    double avg_ev = 0;
    int num = 0;
    for (y = 2; y < h-2; y ++)
    {
        for (x = 2; x < w-2; x ++)
        {
            int p = raw_get_pixel16(x, y);
            int p2 = raw_get_pixel16(x, y+2);
            if (p > black+32 && p2 > black+32 && p < white && p2 < white)
            {
                avg_ev += ABS(raw2ev[p2] - raw2ev[p]);
                num++;
            }
        }
    }
    
    avg_ev /= num;

    if (avg_ev > 0.5)
        return 1;
    
    return 0;
}

static int median_short(unsigned short* x, int n)
{
    unsigned short* aux = malloc(n * sizeof(x[0]));
    CHECK(aux, "malloc");
    memcpy(aux, x, n * sizeof(aux[0]));
    //~ qsort(aux, n, sizeof(aux[0]), compare_short);
    #define short_lt(a,b) ((*a)<(*b))
    QSORT(unsigned short, aux, n, short_lt);
    int ans = aux[n/2];
    free(aux);
    return ans;
}

static int estimate_iso(unsigned short* dark, unsigned short* bright, double* corr_ev, double* black_delta)
{
    /* guess ISO - find the factor and the offset for matching the bright and dark images */
    /* method: for each X (dark image) level, compute median of corresponding Y (bright image) values */
    /* then do a straight line fitting */

    int black = raw_info.black_level;
    int white = raw_info.white_level;

    int w = raw_info.width;
    int h = raw_info.height;
    
    int* order = malloc(w * h * sizeof(order[0]));
    CHECK(order, "malloc");
    
    int i, j, k;
    for (i = 0; i < w * h; i++)
        order[i] = i;
    
    /* sort the low ISO tones and process them as RLE */
    #define darkidx_lt(a,b) (dark[(*a)]<dark[(*b)])
    QSORT(int, order, w*h, darkidx_lt);
    
    int* medians_x = malloc(white * sizeof(medians_x[0]));
    int* medians_y = malloc(white * sizeof(medians_y[0]));
    int num_medians = 0;

    for (i = 0; i < w*h; )
    {
        int ref = dark[order[i]];
        for (j = i+1; j < w*h && dark[order[j]] == ref; j++);

        /* same dark value from i to j (without j) */
        int num = (j - i);
        
        if (num > 1000 && ref > black + 32)
        {
            unsigned short* aux = malloc(num * sizeof(aux[0]));
            for (k = 0; k < num; k++)
                aux[k] = bright[order[k+i]];
            int m = median_short(aux, num);
            if (m > black + 32 && m < white - 1000)
            {
                medians_x[num_medians] = ref - black;
                medians_y[num_medians] = m - black;
                num_medians++;
            }
            free(aux);
            
            /* no more useful data beyond this point */
            if (m >= white - 1000)
                break;
        }
        
        i = j;
    }

#if 0
    FILE* f = fopen("iso-curve.m", "w");

    fprintf(f, "x = [");
    for (i = 0; i < num_medians; i++)
        fprintf(f, "%d ", medians_x[i]);
    fprintf(f, "];\n");
    
    fprintf(f, "y = [");
    for (i = 0; i < num_medians; i++)
        fprintf(f, "%d ", medians_y[i]);
    fprintf(f, "];\n");

    fprintf(f, "plot(x, y);\n");
    fclose(f);
    
    system("octave --persist iso-curve.m");
#endif

    /**
     * plain least squares
     * y = ax + b
     * a = (mean(xy) - mean(x)mean(y)) / (mean(x^2) - mean(x)^2)
     * b = mean(y) - a mean(x)
     */
    
    double mx = 0, my = 0, mxy = 0, mx2 = 0;
    for (i = 0; i < num_medians; i++)
    {
        mx += medians_x[i];
        my += medians_y[i];
        mxy += medians_x[i] * medians_y[i];
        mx2 += medians_x[i] * medians_x[i];
    }
    mx /= num_medians;
    my /= num_medians;
    mxy /= num_medians;
    mx2 /= num_medians;
    double a = (mxy - mx*my) / (mx2 - mx*mx);
    double b = my - a * mx;

    free(medians_x);
    free(medians_y);
    free(order);

    double factor = a;
    if (factor < 1.2 || !isfinite(factor))
    {
        printf("Doesn't look like interlaced ISO\n");
        return 0;
    }
    
    *corr_ev = log2(factor);
    *black_delta = - b / a;

    printf("ISO difference : %.2f EV (%d)\n", log2(factor), (int)round(factor*100));
    printf("Black delta    : %d\n", (int)round(*black_delta));

    return 1;
}

#ifdef INTERP_MEAN_5_OUT_OF_6
#define interp6 mean5outof6
#define INTERP_METHOD_NAME "mean5/6"

/* mean of 5 numbers out of 6 (with one outlier removed) */
static int mean5outof6(int a, int b, int c, int d, int e, int f, int white)
{
    int x[6] = {a,b,c,d,e,f};

    /* compute median */
    int aux;
    int i,j;
    for (i = 0; i < 5; i++)
        for (j = i+1; j < 6; j++)
            if (x[i] > x[j])
                aux = x[i], x[i] = x[j], x[j] = aux;
    int median = (x[2] + x[3]) / 2;
    
    /* remove 1 outlier */
    int l = 0;
    int r = 5;
    if (median - x[l] > x[r] - median) l++;
    else r--;
    
    /* mean of remaining numbers */
    int sum = 0;
    for (i = l; i <= r; i++)
    {
        if (x[i] >= white) return white;
        sum += x[i];
    }
    return sum / (r - l + 1);
}
#endif

#ifdef INTERP_MEAN_4_OUT_OF_6
#define INTERP_METHOD_NAME "mean4/6"
#define interp6 mean4outof6
/* mean of 4 numbers out of 6 (with two outliers removed) */
static int mean4outof6(int a, int b, int c, int d, int e, int f, int white)
{
    int x[6] = {a,b,c,d,e,f};

    /* compute median */
    int aux;
    int i,j;
    for (i = 0; i < 5; i++)
        for (j = i+1; j < 6; j++)
            if (x[i] > x[j])
                aux = x[i], x[i] = x[j], x[j] = aux;
    int median = (x[2] + x[3]) / 2;
    
    /* remove 2 outliers */
    int l = 0;
    int r = 5;
    if (median - x[l] > x[r] - median) l++;
    else r--;
    if (median - x[l] > x[r] - median) l++;
    else r--;
    
    /* mean of remaining numbers */
    int sum = 0;
    for (i = l; i <= r; i++)
    {
        if (x[i] >= white) return white;
        sum += x[i];
    }
    return sum / (r - l + 1);
}
#endif

#ifdef INTERP_MEAN_6
#define INTERP_METHOD_NAME "mean6"
#define interp6 mean6
static int mean6(int a, int b, int c, int d, int e, int f, int white)
{
    return (a + b + c + d + e + f) / 6;
}
#endif

#ifdef INTERP_MEDIAN_6
#define INTERP_METHOD_NAME "median6"
#define interp6 median6
/* median of 6 numbers */
static int median6(int a, int b, int c, int d, int e, int f, int white)
{
    int x[6] = {a,b,c,d,e,f};

    /* compute median */
    int aux;
    int i,j;
    for (i = 0; i < 5; i++)
        for (j = i+1; j < 6; j++)
            if (x[i] > x[j])
                aux = x[i], x[i] = x[j], x[j] = aux;
    if ((x[2] >= white) || (x[3] >= white))
        return white;
    int median = (x[2] + x[3]) / 2;
    return median;
}
#endif

#ifdef MEDIAN7_SMOOTH
/* median of 7 numbers */
static int median7(int a, int b, int c, int d, int e, int f, int g, int white)
{
    int x[7] = {a,b,c,d,e,f,g};

    /* compute median */
    int aux;
    int i,j;
    for (i = 0; i < 6; i++)
        for (j = i+1; j < 7; j++)
            if (x[i] > x[j])
                aux = x[i], x[i] = x[j], x[j] = aux;
    return x[3];
}
#endif

#ifdef INTERP_MEAN23
#define INTERP_METHOD_NAME "mean23"
#endif

#ifdef INTERP_MEAN23_EDGE
#define INTERP_METHOD_NAME "mean23-edge"

#define interp6 median6
/* median of 6 numbers */
static int median6(int a, int b, int c, int d, int e, int f, int white)
{
    int x[6] = {a,b,c,d,e,f};

    /* compute median */
    int aux;
    int i,j;
    for (i = 0; i < 5; i++)
        for (j = i+1; j < 6; j++)
            if (x[i] > x[j])
                aux = x[i], x[i] = x[j], x[j] = aux;
    if ((x[2] >= white) || (x[3] >= white))
        return white;
    int median = (x[2] + x[3]) / 2;
    return median;
}
#endif

#if defined(INTERP_MEAN23) || defined(INTERP_MEAN23_EDGE)
static int mean2(int a, int b, int white, int* err)
{
    if (a >= white || b >= white)
        return white;
    
    int m = (a + b) / 2;

    if (err)
        *err = MAX(ABS(a - m), ABS(b - m));

    return m;
}

static int mean3(int a, int b, int c, int white, int* err)
{
    if (a >= white || b >= white || c >= white)
        return white;

    int m = (a + b + c) / 3;

    if (err)
        *err = MAX(MAX(ABS(a - m), ABS(b - m)), ABS(c - m));

    return m;
}
#endif

#define EV_RESOLUTION 32768

static int hdr_interpolate()
{
    int black = raw_info.black_level;
    int white = raw_info.white_level;

    int w = raw_info.width;
    int h = raw_info.height;

    int x, y;

    /* for fast EV - raw conversion */
    static int raw2ev[65536];   /* EV x EV_RESOLUTION */
    static int ev2raw_0[24*EV_RESOLUTION];
    
    /* handle sub-black values (negative EV) */
    int* ev2raw = ev2raw_0 + 10*EV_RESOLUTION;
    
    /* RGGB or GBRG? */
    double rggb_err = 0;
    double gbrg_err = 0;
    for (y = 2; y < h-2; y += 2)
    {
        for (x = 2; x < w-2; x += 2)
        {
            int tl = raw_get_pixel16(x, y);
            int tr = raw_get_pixel16(x+1, y);
            int bl = raw_get_pixel16(x, y+1);
            int br = raw_get_pixel16(x+1, y+1);
            int pl = raw_get_pixel16(x, y-1);
            int pr = raw_get_pixel16(x+1, y-1);
            rggb_err += MIN(ABS(tr-bl), ABS(tr-pl));
            gbrg_err += MIN(ABS(tl-br), ABS(tl-pr));
        }
    }
    
    /* which one looks more likely? */
    int rggb = (rggb_err < gbrg_err);
    
    if (!rggb) /* this code assumes RGGB, so we need to skip one line */
        raw_info.buffer += raw_info.pitch;

    int i;
    for (i = 0; i < 65536; i++)
    {
        double signal = MAX(i/4.0 - black/4.0, -1023);
        if (signal > 0)
            raw2ev[i] = (int)round(log2(1+signal) * EV_RESOLUTION);
        else
            raw2ev[i] = -(int)round(log2(1-signal) * EV_RESOLUTION);
    }

    for (i = -10*EV_RESOLUTION; i < 0; i++)
    {
        ev2raw[i] = COERCE(black+4 - round(4*pow(2, ((double)-i/EV_RESOLUTION))), 0, black);
    }

    for (i = 0; i < 14*EV_RESOLUTION; i++)
    {
        ev2raw[i] = COERCE(black-4 + round(4*pow(2, ((double)i/EV_RESOLUTION))), black, white);
        
        if (i >= raw2ev[white])
        {
            ev2raw[i] = white;
        }
    }
    
    /* keep "bad" pixels, if any */
    ev2raw[raw2ev[0]] = 0;
    ev2raw[raw2ev[0]] = 0;
    
    /* check raw <--> ev conversion */
    //~ printf("%d %d %d %d %d %d %d *%d* %d %d %d %d %d\n", raw2ev[0], raw2ev[1000], raw2ev[2000], raw2ev[8188], raw2ev[8189], raw2ev[8190], raw2ev[8191], raw2ev[8192], raw2ev[8193], raw2ev[8194], raw2ev[8195], raw2ev[8196], raw2ev[8200]);
    //~ printf("%d %d %d %d %d %d %d *%d* %d %d %d %d %d\n", ev2raw[raw2ev[0]], ev2raw[raw2ev[1000]], ev2raw[raw2ev[2000]], ev2raw[raw2ev[8188]], ev2raw[raw2ev[8189]], ev2raw[raw2ev[8190]], ev2raw[raw2ev[8191]], ev2raw[raw2ev[8192]], ev2raw[raw2ev[8193]], ev2raw[raw2ev[8194]], ev2raw[raw2ev[8195]], ev2raw[raw2ev[8196]], ev2raw[raw2ev[8200]]);

    /* first we need to know which lines are dark and which are bright */
    /* the pattern is not always the same, so we need to autodetect it */

    /* it may look like this */                       /* or like this */
    /*
               ab cd ef gh  ab cd ef gh               ab cd ef gh  ab cd ef gh
                                       
            0  RG RG RG RG  RG RG RG RG            0  rg rg rg rg  rg rg rg rg
            1  gb gb gb gb  gb gb gb gb            1  gb gb gb gb  gb gb gb gb
            2  rg rg rg rg  rg rg rg rg            2  RG RG RG RG  RG RG RG RG
            3  GB GB GB GB  GB GB GB GB            3  GB GB GB GB  GB GB GB GB
            4  RG RG RG RG  RG RG RG RG            4  rg rg rg rg  rg rg rg rg
            5  gb gb gb gb  gb gb gb gb            5  gb gb gb gb  gb gb gb gb
            6  rg rg rg rg  rg rg rg rg            6  RG RG RG RG  RG RG RG RG
            7  GB GB GB GB  GB GB GB GB            7  GB GB GB GB  GB GB GB GB
            8  RG RG RG RG  RG RG RG RG            8  rg rg rg rg  rg rg rg rg
    */

    double acc_bright[4] = {0, 0, 0, 0};
    for (y = 2; y < h-2; y ++)
    {
        for (x = 2; x < w-2; x ++)
        {
            acc_bright[y % 4] += raw_get_pixel16(x, y);
        }
    }
    double avg_bright = (acc_bright[0] + acc_bright[1] + acc_bright[2] + acc_bright[3]) / 4;
    int is_bright[4] = { acc_bright[0] > avg_bright, acc_bright[1] > avg_bright, acc_bright[2] > avg_bright, acc_bright[3] > avg_bright};

    printf("ISO pattern    : %c%c%c%c %s\n", is_bright[0] ? 'B' : 'd', is_bright[1] ? 'B' : 'd', is_bright[2] ? 'B' : 'd', is_bright[3] ? 'B' : 'd', rggb ? "RGGB" : "GBRG");
    
    if (is_bright[0] + is_bright[1] + is_bright[2] + is_bright[3] != 2)
    {
        printf("Bright/dark detection error\n");
        return 0;
    }

    if (is_bright[0] == is_bright[2] || is_bright[1] == is_bright[3])
    {
        printf("Interlacing method not supported\n");
        return 0;
    }

    #define BRIGHT_ROW (is_bright[y % 4])

    /* dark and bright exposures, interpolated */
    unsigned short* dark   = malloc(w * h * sizeof(unsigned short));
    CHECK(dark, "malloc");
    unsigned short* bright = malloc(w * h * sizeof(unsigned short));
    CHECK(bright, "malloc");
    memset(dark, 0, w * h * sizeof(unsigned short));
    memset(bright, 0, w * h * sizeof(unsigned short));
    
    /* fullres image (minimizes aliasing) */
    unsigned short* fullres = malloc(w * h * sizeof(unsigned short));
    CHECK(fullres, "malloc");
    memset(fullres, 0, w * h * sizeof(unsigned short));
    unsigned short* fullres_smooth = 0;

    #ifdef CONTRAST_BLEND
    unsigned short* contrast = malloc(w * h * sizeof(unsigned short));
    CHECK(contrast, "malloc");
    unsigned short* overexposed = malloc(w * h * sizeof(unsigned short));
    CHECK(overexposed, "malloc");
    memset(contrast, 0, w * h * sizeof(unsigned short));
    memset(overexposed, 0, w * h * sizeof(unsigned short));
    #endif

    printf("Estimating ISO difference...\n");
    /* use a simple interpolation in 14-bit space (the 16-bit one will trick the algorithm) */
    for (y = 2; y < h-2; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 0; x < w; x ++)
        {
            interp[x + y * w] = (raw_get_pixel(x, y+2) + raw_get_pixel(x, y-2)) / 2;
            native[x + y * w] = raw_get_pixel(x, y);
        }
    }
    /* estimate ISO and black difference between bright and dark exposures */
    double corr_ev = 0;
    double black_delta = 0;
    
    /* don't forget that estimate_iso only works on 14-bit data, but we are working on 16 */
    raw_info.black_level /= 4;
    raw_info.white_level /= 4;
    int ok = estimate_iso(dark, bright, &corr_ev, &black_delta);
    raw_info.black_level *= 4;
    raw_info.white_level *= 4;
    if (!ok) goto err;
    
    printf("Interpolation  : %s\n", INTERP_METHOD_NAME
        #ifdef VERTICAL_SMOOTH
        "-vsmooth3"
        #endif
        #ifdef MEAN7_SMOOTH
        "-msmooth7"
        #endif
        #ifdef MEDIAN7_SMOOTH
        "-medsmooth7"
        #endif
        #ifdef CHROMA_SMOOTH
        "-chroma5x5"
        #endif
        #ifdef CONTRAST_BLEND
        "-contrast"
        #endif
    );

#if defined(INTERP_MEAN23) || defined(INTERP_MEAN23_EDGE)
    for (y = 2; y < h-2; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        int is_rg = (y % 2 == 0); /* RG or GB? */
        
        for (x = 2; x < w-3; x += 2)
        {
        
            /* red/blue: interpolate from (x,y+2) and (x,y-2) */
            /* green: interpolate from (x+1,y+1),(x-1,y+1),(x,y-2) or (x+1,y-1),(x-1,y-1),(x,y+2), whichever has the correct brightness */
            
            int s = (is_bright[y%4] == is_bright[(y+1)%4]) ? -1 : 1;
            
            if (is_rg)
            {
                int ra = raw_get_pixel_14to16(x, y-2);
                int rb = raw_get_pixel_14to16(x, y+2);
                int er=0; int ri = mean2(raw2ev[ra], raw2ev[rb], raw2ev[white], &er);
                
                int ga = raw_get_pixel_14to16(x+1+1, y+s);
                int gb = raw_get_pixel_14to16(x+1-1, y+s);
                int gc = raw_get_pixel_14to16(x+1, y-2*s);
                int eg=0; int gi = mean3(raw2ev[ga], raw2ev[gb], raw2ev[gc], raw2ev[white], &eg);

                interp[x   + y * w] = ev2raw[ri];
                interp[x+1 + y * w] = ev2raw[gi];

                #ifdef CONTRAST_BLEND
                //~ contrast[x   + y * w] = ABS(er);
                //~ contrast[x+1 + y * w] = ABS(eg);
                #endif
            }
            else
            {
                int ba = raw_get_pixel_14to16(x+1  , y-2);
                int bb = raw_get_pixel_14to16(x+1  , y+2);
                int eb=0; int bi = mean2(raw2ev[ba], raw2ev[bb], raw2ev[white], &eb);

                int ga = raw_get_pixel_14to16(x+1, y+s);
                int gb = raw_get_pixel_14to16(x-1, y+s);
                int gc = raw_get_pixel_14to16(x, y-2*s);
                int eg=0; int gi = mean3(raw2ev[ga], raw2ev[gb], raw2ev[gc], raw2ev[white], &eg);

                interp[x   + y * w] = ev2raw[gi];
                interp[x+1 + y * w] = ev2raw[bi];
                
                #ifdef CONTRAST_BLEND
                //~ contrast[x   + y * w] = ABS(eg);
                //~ contrast[x+1 + y * w] = ABS(eb);
                #endif
            }

            native[x   + y * w] = raw_get_pixel_14to16(x, y);
            native[x+1 + y * w] = raw_get_pixel_14to16(x+1, y);
        }
    }
#else
    for (y = 2; y < h-2; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 2; x < w-2; x ++)
        {
            int ra = raw_get_pixel_14to16(x, y-2);
            int rb = raw_get_pixel_14to16(x, y+2);
            int ral = raw_get_pixel_14to16(x-2, y-2);
            int rbl = raw_get_pixel_14to16(x-2, y+2);
            int rar = raw_get_pixel_14to16(x+2, y-2);
            int rbr = raw_get_pixel_14to16(x+2, y+2);

            int ri = ev2raw[
                interp6(
                    raw2ev[ra], 
                    raw2ev[rb],
                    raw2ev[ral],
                    raw2ev[rbl],
                    raw2ev[rar],
                    raw2ev[rbr],
                    raw2ev[white]
                )
            ];
            
            interp[x   + y * w] = ri;
            native[x   + y * w] = raw_get_pixel_14to16(x, y);
        }
    }
#endif

#ifdef INTERP_MEAN23_EDGE /* second step, for detecting edges */
    for (y = 2; y < h-2; y ++)
    {
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 2; x < w-2; x ++)
        {
            int Ra = raw_get_pixel_14to16(x, y-2);
            int Rb = raw_get_pixel_14to16(x, y+2);
            int Ral = raw_get_pixel_14to16(x-2, y-2);
            int Rbl = raw_get_pixel_14to16(x-2, y+2);
            int Rar = raw_get_pixel_14to16(x+2, y-2);
            int Rbr = raw_get_pixel_14to16(x+2, y+2);
            
            int rae = raw2ev[Ra];
            int rbe = raw2ev[Rb];
            int rale = raw2ev[Ral];
            int rble = raw2ev[Rbl];
            int rare = raw2ev[Rar];
            int rbre = raw2ev[Rbr];
            int whitee = raw2ev[white];

            /* median */
            int ri = ev2raw[interp6(rae, rbe, rale, rble, rare, rbre, whitee)];
            
            /* mean23, computed at previous step */
            int ri0 = interp[x + y * w];
            
            /* mean23 overexposed? use median without thinking twice */
            if (ri0 >= white)
            {
                interp[x + y * w] = ri;
                continue;
            }
            
            int ri0e = raw2ev[ri0];
            
            /* threshold for edges */
            int thr = EV_RESOLUTION / 8;
            
            /* detect diagonal edge patterns, where median looks best:
             *       
             *       . . *          . * *           * * *
             *         ?              ?               ?            and so on
             *       * * *          * * *           * . . 
             */
            
            if (rbe < ri0e - thr && rble < ri0e - thr && rbre < ri0e - thr) /* bottom dark */
            {
                if ((rale < ri0e - thr && rare > ri0e + thr) || (rare < ri0e - thr && rale > ri0e - thr))
                    interp[x + y * w] = ri;
            }
            else if (rbe > ri0e + thr && rble > ri0e + thr && rbre > ri0e + thr) /* bottom bright */
            {
                if ((rale > ri0e + thr && rare < ri0e - thr) || (rare > ri0e + thr && rale < ri0e - thr))
                    interp[x + y * w] = ri;
            }
            else if (rae < ri0e - thr && rale < ri0e - thr && rare < ri0e - thr) /* top dark */
            {
                if ((rble > ri0e + thr && rbre < ri0e - thr) || (rbre > ri0e + thr && rble < ri0e - thr))
                    interp[x + y * w] = ri;
            }
            else if (rae > ri0e + thr && rale > ri0e + thr && rare > ri0e + thr) /* top bright */
            {
                if ((rble > ri0e + thr && rbre < ri0e - thr) || (rbre > ri0e + thr && rble < ri0e - thr))
                    interp[x + y * w] = ri;
            }
        }
    }
#endif

    /* border interpolation */
    for (y = 0; y < 3; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 0; x < w; x ++)
        {
            interp[x + y * w] = raw_get_pixel_14to16(x, y+2);
            native[x + y * w] = raw_get_pixel_14to16(x, y);
        }
    }

    for (y = h-2; y < h; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 0; x < w; x ++)
        {
            interp[x + y * w] = raw_get_pixel_14to16(x, y-2);
            native[x + y * w] = raw_get_pixel_14to16(x, y);
        }
    }

    for (y = 2; y < h; y ++)
    {
        unsigned short* native = BRIGHT_ROW ? bright : dark;
        unsigned short* interp = BRIGHT_ROW ? dark : bright;
        
        for (x = 0; x < 2; x ++)
        {
            interp[x + y * w] = raw_get_pixel_14to16(x, y-2);
            native[x + y * w] = raw_get_pixel_14to16(x, y);
        }

        for (x = w-3; x < w; x ++)
        {
            interp[x + y * w] = raw_get_pixel_14to16(x-2, y-2);
            native[x + y * w] = raw_get_pixel_14to16(x-2, y);
        }
    }

    printf("Matching brightness...\n");
    double corr = pow(2, corr_ev);
    int white_darkened = (white - black) / corr + black;
    for (y = 0; y < h; y ++)
    {
        for (x = 0; x < w; x ++)
        {
            {
                /* darken bright image so it looks like the low-ISO one */
                /* this is best done in linear space, to handle values under black level */
                /* but it's important to work in 16-bit or more, to minimize quantization errors */
                int b = bright[x + y*w];
                int bd = (b - black) / corr + black;
                bright[x + y*w] = bd;
            }
            {
                /* adjust the black level in the dark image, so it matches the high-ISO one */
                int d = dark[x + y*w];
                int da = d - black_delta*4;
                dark[x + y*w] = da;
            }
        }
    }

    /* reconstruct a full-resolution image (discard interpolated fields whenever possible) */
    /* this has full detail and lowest possible aliasing, but it has high shadow noise and color artifacts when high-iso starts clipping */

    printf("Full-res reconstruction...\n");
    for (y = 0; y < h; y ++)
    {
        for (x = 0; x < w; x ++)
        {
            if (BRIGHT_ROW)
            {
                int f = bright[x + y*w];
                /* if the brighter copy is overexposed, the guessed pixel for sure has higher brightness */
                fullres[x + y*w] = f < white_darkened ? f : MAX(f, dark[x + y*w]);
            }
            else
            {
                fullres[x + y*w] = dark[x + y*w]; 
            }
        }
    }
 
#ifdef SMOOTH
    printf("Alias filtering...\n");
    fullres_smooth = malloc(w * h * sizeof(unsigned short));
    CHECK(fullres_smooth, "malloc");

    memcpy(fullres_smooth, fullres, w * h * sizeof(unsigned short));
#endif

#ifdef CHROMA_SMOOTH
    for (y = 4; y < h-4; y += 2)
    {
        for (x = 2; x < w-2; x += 2)
        {
            int i,j;
            int k = 0;
            int med_r[25];
            int med_b[25];
            for (i = -4; i <= 4; i += 2)
            {
                for (j = -4; j <= 4; j += 2)
                {
                    int r  = fullres[x+i   +   (y+j) * w];
                    int g1 = fullres[x+i+1 +   (y+j) * w];
                    int g2 = fullres[x+i   + (y+j+1) * w];
                    int b  = fullres[x+i+1 + (y+j+1) * w];
                    
                    int ge = (raw2ev[g1] + raw2ev[g2]) / 2;
                    med_r[k] = raw2ev[r] - ge;
                    med_b[k] = raw2ev[b] - ge;
                    k++;
                }
            }
            int dr = opt_med25(med_r);
            int db = opt_med25(med_b);

            int g1 = fullres[x+1 +     y * w];
            int g2 = fullres[x   + (y+1) * w];
            int ge = (raw2ev[g1] + raw2ev[g2]) / 2;

            fullres_smooth[x   +     y * w] = ev2raw[COERCE(ge + dr, -10*EV_RESOLUTION, 14*EV_RESOLUTION)];
            fullres_smooth[x+1 + (y+1) * w] = ev2raw[COERCE(ge + db, -10*EV_RESOLUTION, 14*EV_RESOLUTION)];
        }
    }
#endif

#ifdef VERTICAL_SMOOTH
    for (y = 4; y < h-4; y ++)
    {
        for (x = 2; x < w-2; x ++)
        {
            int a = fullres[x + (y-2)*w];
            int b = fullres[x + (y  )*w];
            int c = fullres[x + (y+2)*w];
            fullres_smooth[x + y*w] = ev2raw[(raw2ev[a] + raw2ev[b] + raw2ev[b] + raw2ev[c]) / 4];
        }
    }
#endif

#ifdef MEAN7_SMOOTH
    for (y = 4; y < h-4; y ++)
    {
        for (x = 2; x < w-2; x ++)
        {
            int a = fullres[x + (y-2)*w];
            int b = fullres[x + (y  )*w];
            int c = fullres[x + (y+2)*w];

            int d = fullres[x-2 + (y-2)*w];
            int e = fullres[x+2 + (y-2)*w];
            int f = fullres[x-2 + (y+2)*w];
            int g = fullres[x+2 + (y+2)*w];
            fullres_smooth[x + y*w] = ev2raw[(raw2ev[a] + raw2ev[b] + raw2ev[b] + raw2ev[c] + raw2ev[d] + raw2ev[e] + raw2ev[f] + raw2ev[g]) / 8];
        }
    }
#endif

#ifdef MEDIAN7_SMOOTH
    for (y = 4; y < h-4; y ++)
    {
        for (x = 2; x < w-2; x ++)
        {
            int a = fullres[x + (y-2)*w];
            int b = fullres[x + (y  )*w];
            int c = fullres[x + (y+2)*w];

            int d = fullres[x-2 + (y-2)*w];
            int e = fullres[x+2 + (y-2)*w];
            int f = fullres[x-2 + (y+2)*w];
            int g = fullres[x+2 + (y+2)*w];
            fullres_smooth[x + y*w] = median7(a,b,c,d,e,f,g,white);
        }
    }
#endif

#if 0 /* for debugging only */
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("normal.dng");

    for (y = 3; y < h-2; y ++)
        for (x = 2; x < w-2; x ++)
            raw_set_pixel16(x, y, bright[x + y*w]);
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("bright.dng");

    for (y = 3; y < h-2; y ++)
        for (x = 2; x < w-2; x ++)
            raw_set_pixel16(x, y, dark[x + y*w]);
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("dark.dng");

    for (y = 3; y < h-2; y ++)
        for (x = 2; x < w-2; x ++)
            raw_set_pixel16(x, y, fullres[x + y*w]);
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("fullres.dng");

#ifdef SMOOTH
    for (y = 3; y < h-2; y ++)
        for (x = 2; x < w-2; x ++)
            raw_set_pixel16(x, y, fullres_smooth[x + y*w]);
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("fullres_smooth.dng");
#endif

    int yb = 0;
    int yd = h/2;
    for (y = 0; y < h; y ++)
    {
        if (BRIGHT_ROW)
        {
            for (x = 0; x < w; x ++)
                raw_set_pixel16(x, yb, bright[x + y*w]);
            yb++;
        }
        else
        {
            for (x = 0; x < w; x ++)
                raw_set_pixel16(x, yd, dark[x + y*w]);
            yd++;
        }
    }
    reverse_bytes_order(raw_info.buffer, raw_info.frame_size);
    save_dng("split.dng");
#endif

#ifdef CONTRAST_BLEND
    unsigned short* contrast_aux = malloc(w * h * sizeof(unsigned short));
    CHECK(contrast_aux, "malloc");

    /* build the contrast maps (for each channel) */
    for (y = 6; y < h-6; y ++)
    {
        for (x = 6; x < w-6; x ++)
        {
            int p0 = fullres[x + y*w];
            int p1 = fullres[x + (y+2)*w];
            int p2 = fullres[x + (y-2)*w];
            int p3 = fullres[x+2 + y*w];
            int p4 = fullres[x+2 + y*w];
            contrast[x + y*w] = MAX(MAX(ABS(p1-p0), ABS(p2-p0)), MAX(ABS(p3-p0), ABS(p4-p0)));
        }
    }

    memcpy(contrast_aux, contrast, w * h * sizeof(unsigned short));
    
    /* trial and error - too high = aliasing, too low = noisy */
    int CONTRAST_MAX = 2000;

#if 1
    printf("Dilating contrast map...\n");
    /* dilate the contrast maps (for each channel) */
    for (y = 6; y < h-6; y ++)
    {
        for (x = 6; x < w-6; x ++)
        {
            /* optimizing... the brute force way */
            int c0 = contrast[x+0 + (y+0) * w];
            int c1 = MAX(MAX(contrast[x+0 + (y-2) * w], contrast[x-2 + (y+0) * w]), MAX(contrast[x+2 + (y+0) * w], contrast[x+0 + (y+2) * w]));
            int c2 = MAX(MAX(contrast[x-2 + (y-2) * w], contrast[x+2 + (y-2) * w]), MAX(contrast[x-2 + (y+2) * w], contrast[x+2 + (y+2) * w]));
            int c3 = MAX(MAX(contrast[x+0 + (y-4) * w], contrast[x-4 + (y+0) * w]), MAX(contrast[x+4 + (y+0) * w], contrast[x+0 + (y+4) * w]));
            //~ int c4 = MAX(MAX(contrast[x-2 + (y-4) * w], contrast[x+2 + (y-4) * w]), MAX(contrast[x-4 + (y-2) * w], contrast[x+4 + (y-2) * w]));
            //~ int c5 = MAX(MAX(contrast[x-4 + (y+2) * w], contrast[x+4 + (y+2) * w]), MAX(contrast[x-2 + (y+4) * w], contrast[x+2 + (y+4) * w]));
            //~ int c = MAX(MAX(MAX(c0, c1), MAX(c2, c3)), MAX(c4, c5));
            int c = MAX(MAX(c0, c1), MAX(c2, c3));
            
            contrast_aux[x + y * w] = c;
        }
    }
#endif

    printf("Smoothing contrast map...\n");
    /* gaussian blur */
    for (y = 6; y < h-6; y ++)
    {
        for (x = 6; x < w-6; x ++)
        {

/* code generation
            const int blur[4][4] = {
                {1024,  820,  421,  139},
                { 820,  657,  337,  111},
                { 421,  337,  173,   57},
                { 139,  111,   57,    0},
            };
            const int blur_unique[] = {1024, 820, 657, 421, 337, 173, 139, 111, 57};

            int k;
            for (k = 0; k < COUNT(blur_unique); k++)
            {
                int dx, dy;
                int c = 0;
                printf("(");
                for (dy = -3; dy <= 3; dy++)
                {
                    for (dx = -3; dx <= 3; dx++)
                    {
                        c += contrast_aux[x + dx + (y + dy) * w] * blur[ABS(dx)][ABS(dy)] / 1024;
                        if (blur[ABS(dx)][ABS(dy)] == blur_unique[k])
                            printf("contrast_aux[x%+d + (y%+d) * w] + ", dx, dy);
                    }
                }
                printf("\b\b\b) * %d / 1024 + \n", blur_unique[k]);
            }
            exit(1);
*/
            /* optimizing... the brute force way */
            int c = 
                (contrast_aux[x+0 + (y+0) * w])+ 
                (contrast_aux[x+0 + (y-2) * w] + contrast_aux[x-2 + (y+0) * w] + contrast_aux[x+2 + (y+0) * w] + contrast_aux[x+0 + (y+2) * w]) * 820 / 1024 + 
                (contrast_aux[x-2 + (y-2) * w] + contrast_aux[x+2 + (y-2) * w] + contrast_aux[x-2 + (y+2) * w] + contrast_aux[x+2 + (y+2) * w]) * 657 / 1024 + 
                (contrast_aux[x+0 + (y-2) * w] + contrast_aux[x-2 + (y+0) * w] + contrast_aux[x+2 + (y+0) * w] + contrast_aux[x+0 + (y+2) * w]) * 421 / 1024 + 
                (contrast_aux[x-2 + (y-2) * w] + contrast_aux[x+2 + (y-2) * w] + contrast_aux[x-2 + (y-2) * w] + contrast_aux[x+2 + (y-2) * w] + contrast_aux[x-2 + (y+2) * w] + contrast_aux[x+2 + (y+2) * w] + contrast_aux[x-2 + (y+2) * w] + contrast_aux[x+2 + (y+2) * w]) * 337 / 1024 + 
                //~ (contrast_aux[x-2 + (y-2) * w] + contrast_aux[x+2 + (y-2) * w] + contrast_aux[x-2 + (y+2) * w] + contrast_aux[x+2 + (y+2) * w]) * 173 / 1024 + 
                //~ (contrast_aux[x+0 + (y-6) * w] + contrast_aux[x-6 + (y+0) * w] + contrast_aux[x+6 + (y+0) * w] + contrast_aux[x+0 + (y+6) * w]) * 139 / 1024 + 
                //~ (contrast_aux[x-2 + (y-6) * w] + contrast_aux[x+2 + (y-6) * w] + contrast_aux[x-6 + (y-2) * w] + contrast_aux[x+6 + (y-2) * w] + contrast_aux[x-6 + (y+2) * w] + contrast_aux[x+6 + (y+2) * w] + contrast_aux[x-2 + (y+6) * w] + contrast_aux[x+2 + (y+6) * w]) * 111 / 1024 + 
                //~ (contrast_aux[x-2 + (y-6) * w] + contrast_aux[x+2 + (y-6) * w] + contrast_aux[x-6 + (y-2) * w] + contrast_aux[x+6 + (y-2) * w] + contrast_aux[x-6 + (y+2) * w] + contrast_aux[x+6 + (y+2) * w] + contrast_aux[x-2 + (y+6) * w] + contrast_aux[x+2 + (y+6) * w]) * 57 / 1024;
                0;
            contrast[x + y * w] = COERCE(c, 0, CONTRAST_MAX);
        }
    }

    /* make it grayscale and lower it in dark areas where it's just noise */
    for (y = 2; y < h-2; y += 2)
    {
        for (x = 2; x < w-2; x += 2)
        {
            int a = contrast[x   +     y * w];
            int b = contrast[x+1 +     y * w];
            int c = contrast[x   + (y+1) * w];
            int d = contrast[x+1 + (y+1) * w];
            int C = MAX(MAX(a,b), MAX(c,d));
            
            C = MIN(C, CONTRAST_MAX);

            contrast[x   +     y * w] = 
            contrast[x+1 +     y * w] = 
            contrast[x   + (y+1) * w] = 
            contrast[x+1 + (y+1) * w] = C;
        }
    }

    /* where the image is overexposed? */
    for (y = 0; y < h; y ++)
    {
        for (x = 0; x < w; x ++)
        {
            overexposed[x + y * w] = bright[x + y * w] >= white_darkened || dark[x + y * w] >= white ? 100 : 0;
        }
    }
    
    /* "blur" the overexposed map */
    memcpy(contrast_aux, overexposed, w * h * sizeof(unsigned short));

    for (y = 3; y < h-3; y ++)
    {
        for (x = 3; x < w-3; x ++)
        {
            overexposed[x + y * w] = 
                (contrast_aux[x+0 + (y+0) * w])+ 
                (contrast_aux[x+0 + (y-1) * w] + contrast_aux[x-1 + (y+0) * w] + contrast_aux[x+1 + (y+0) * w] + contrast_aux[x+0 + (y+1) * w]) * 820 / 1024 + 
                (contrast_aux[x-1 + (y-1) * w] + contrast_aux[x+1 + (y-1) * w] + contrast_aux[x-1 + (y+1) * w] + contrast_aux[x+1 + (y+1) * w]) * 657 / 1024 + 
                //~ (contrast_aux[x+0 + (y-2) * w] + contrast_aux[x-2 + (y+0) * w] + contrast_aux[x+2 + (y+0) * w] + contrast_aux[x+0 + (y+2) * w]) * 421 / 1024 + 
                //~ (contrast_aux[x-1 + (y-2) * w] + contrast_aux[x+1 + (y-2) * w] + contrast_aux[x-2 + (y-1) * w] + contrast_aux[x+2 + (y-1) * w] + contrast_aux[x-2 + (y+1) * w] + contrast_aux[x+2 + (y+1) * w] + contrast_aux[x-1 + (y+2) * w] + contrast_aux[x+1 + (y+2) * w]) * 337 / 1024 + 
                //~ (contrast_aux[x-2 + (y-2) * w] + contrast_aux[x+2 + (y-2) * w] + contrast_aux[x-2 + (y+2) * w] + contrast_aux[x+2 + (y+2) * w]) * 173 / 1024 + 
                //~ (contrast_aux[x+0 + (y-3) * w] + contrast_aux[x-3 + (y+0) * w] + contrast_aux[x+3 + (y+0) * w] + contrast_aux[x+0 + (y+3) * w]) * 139 / 1024 + 
                //~ (contrast_aux[x-1 + (y-3) * w] + contrast_aux[x+1 + (y-3) * w] + contrast_aux[x-3 + (y-1) * w] + contrast_aux[x+3 + (y-1) * w] + contrast_aux[x-3 + (y+1) * w] + contrast_aux[x+3 + (y+1) * w] + contrast_aux[x-1 + (y+3) * w] + contrast_aux[x+1 + (y+3) * w]) * 111 / 1024 + 
                //~ (contrast_aux[x-2 + (y-3) * w] + contrast_aux[x+2 + (y-3) * w] + contrast_aux[x-3 + (y-2) * w] + contrast_aux[x+3 + (y-2) * w] + contrast_aux[x-3 + (y+2) * w] + contrast_aux[x+3 + (y+2) * w] + contrast_aux[x-2 + (y+3) * w] + contrast_aux[x+2 + (y+3) * w]) * 57 / 1024;
                0;
        }
    }

    free(contrast_aux);
#endif


    /* mix the two images */
    /* highlights:  keep data from dark image only */
    /* shadows:     keep data from bright image only */
    /* midtones:    mix data from both, to bring back the resolution */
    
    /* estimate ISO overlap */
    /*
      ISO 100:       ###...........  (11 stops)
      ISO 1600:  ####..........      (10 stops)
      Combined:  XX##..............  (14 stops)
    */
    double clipped_ev = corr_ev;
    double lowiso_dr = 11;
    double overlap = lowiso_dr - clipped_ev;

    /* you get better colors, less noise, but a little more jagged edges if we underestimate the overlap amount */
    /* maybe expose a tuning factor? (preference towards resolution or colors) */
    overlap -= MIN(2, overlap - 2);
    
    printf("ISO overlap    : %.1f EV (approx)\n", overlap);
    
    if (overlap < 0.5)
    {
        printf("Overlap error\n");
        goto err;
    }
    else if (overlap < 2)
    {
        printf("Overlap too small, use a smaller ISO difference for better results.\n");
    }

    /* mixing curves */
    double max_ev = log2(white/4 - black/4);
    static double mix_curve[65536];
    static double fullres_curve[65536];
    
    static double fullres_start = 3.5;
    static double fullres_transition = 2;
    
    for (i = 0; i < 65536; i++)
    {
        double ev = log2(MAX(i/4.0 - black/4.0, 1)) + corr_ev;
        double ev2 = log2(MAX(i/4.0 - black/4.0, 1));
        double c = -cos(MAX(MIN(ev-(max_ev-overlap),overlap),0)*M_PI/overlap);
        double c2 = -cos(COERCE(ev2 - fullres_start, 0, fullres_transition)*M_PI/fullres_transition);
        double k = (c+1) / 2;
        double f = (c2+1) / 2;

        mix_curve[i] = k;
        fullres_curve[i] = f;
    }

#if 0
    FILE* f = fopen("mix-curve.m", "w");
    fprintf(f, "x = 0:65535; \n");

    fprintf(f, "ev = [");
    for (i = 0; i < 65536; i++)
        fprintf(f, "%f ", log2(MAX(i/4.0 - black/4.0, 1)));
    fprintf(f, "];\n");
    
    fprintf(f, "k = [");
    for (i = 0; i < 65536; i++)
        fprintf(f, "%f ", mix_curve[i]);
    fprintf(f, "];\n");

    fprintf(f, "f = [");
    for (i = 0; i < 65536; i++)
        fprintf(f, "%f ", fullres_curve[i]);
    fprintf(f, "];\n");
    
    fprintf(f, "plot(ev, k, ev, f, 'r');\n");
    fclose(f);
    
    system("octave --persist mix-curve.m");
#endif
    
    int hot_pixels = 0;
    
    for (y = 0; y < h; y ++)
    {
        for (x = 0; x < w; x ++)
        {
            /* bright and dark source pixels  */
            /* they may be real or interpolated */
            /* they both have the same brightness (they were adjusted before this loop), so we are ready to mix them */ 
            int b = bright[x + y*w];
            int d  = dark[x + y*w];
            
            /* full-res image (non-interpolated, except where one ISO is blown out) */
            int fr = fullres[x + y*w];

            #ifdef SMOOTH
            /* full res with some smoothing applied to hide aliasing artifacts */
            int frs = fullres_smooth[x + y*w];
            #else
            int frs = fr;
            #endif

            /* go from linear to EV space */
            int bev = raw2ev[b];
            int dev = raw2ev[d];
            int frev = raw2ev[fr];
            int frsev = raw2ev[frs];

#ifdef FULLRES_ONLY 
            int output = frev;
#else
            /* blending factors */
            int k = mix_curve[b & 65535];
            int f = fullres_curve[b & 65535];
            
            #ifdef CONTRAST_BLEND
            int co = contrast[x + y*w];
            double c = COERCE(co / (double) CONTRAST_MAX, 0, 1);
            double ovf = COERCE(overexposed[x + y*w] / 200.0, 0, 1);
            c = MAX(c, ovf);
            k = MIN(MAX(k, ovf*5), 1);
            #else
            double c = 1;
            #endif

            double noisy_or_overexposed = MAX(ovf, 1-f);
            
            /* mix bright and dark exposures */
            double mixed = bev * (1-k) + dev * k;

            /* use data from both ISOs in high-detail areas, even if it's noisier (less aliasing) */
            f = MAX(f, c);
            
            /* use smoothing in noisy near-overexposed areas to hide color artifacts */
            double fev = noisy_or_overexposed * frsev + (1-noisy_or_overexposed) * frev;
            
            /* blend "mixed" and "full-res" images smoothly to avoid banding*/
            int output = mixed * (1-f) + fev * f;

            /* show full-res map (for debugging) */
            //~ output = f * 14*EV_RESOLUTION;
            
            /* show contrast map (for debugging) */
            //~ output = c * 14*EV_RESOLUTION;

            /* beware of hot pixels */
            int is_hot = ((k > 0 || f > 0) && !ovf && (ABS(dev - bev) > (2+c) * EV_RESOLUTION));
            if (is_hot)
            {
                hot_pixels++;
                output = bev;
            }
#endif
            /* safeguard */
            output = COERCE(output, -10*EV_RESOLUTION, 14*EV_RESOLUTION-1);
            
            /* back to linear space and commit */
            raw_set_pixel16(x, y, ev2raw[output]);
        }
    }
    
    if (hot_pixels)
        printf("Hot pixels     : %d\n", hot_pixels);

    if (!rggb) /* back to GBRG */
        raw_info.buffer -= raw_info.pitch;

    free(dark);
    free(bright);
    free(fullres);
    #ifdef CONTRAST_BLEND
    free(contrast);
    free(overexposed);
    #endif
    #ifdef SMOOTH
    free(fullres_smooth);
    #endif
    return 1;

err:
    free(dark);
    free(bright);
    free(fullres);
    #ifdef CONTRAST_BLEND
    free(contrast);
    free(overexposed);
    #endif
    #ifdef SMOOTH
    if (fullres_smooth) free(fullres_smooth);
    #endif
    return 0;
}
