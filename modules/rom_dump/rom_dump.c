/**
 * ROM Dumper using video dot detection
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
#include <zebra.h>
#include <shoot.h>
#include <fps.h>
#include <focus.h>
#include <beep.h>
#include <histogram.h>
#include <console.h>
#include <imgconv.h>
#include "../trace/trace.h"
#include "unpack_block.h"

typedef struct
{
    uint16_t x;
    uint16_t y;
} pixel_info_t;


uint32_t trace_ctx = TRACE_ERROR;

/* first control dots get handshaken */
pixel_info_t control_dots[16];

/* number of dots on screen. sent using control dots */
uint32_t dots_x = 0;
uint32_t dots_y = 0;

/* here are coordinates of the data dots */
pixel_info_t *dots = NULL;

uint8_t *data = NULL;
uint32_t data_length = 0;

/* detection margin is calculated using the control dots' brightness */
uint32_t detection_margin = 0;
uint32_t dark_level = 0;

/* helper macro to access dots by x/y */
#define DOTPOS(x,y) ((y)*dots_x + (x))

/* draw a dot on screen, bmp coordinates, used for marking detected dots. only works for even sizes */
static void mark_block(uint32_t x, uint32_t y, uint32_t color, int32_t size)
{
    uint8_t *const bvram = bmp_vram();
    uint8_t *const bvram_mirror = get_bvram_mirror();
    
    if(!bvram || !bvram_mirror)
    {
        return;
    }
    
    for(int32_t dy = -size/2; dy < size/2; dy++)
    {
        for(int32_t dx = -size/2; dx < size/2; dx++)
        {
            bvram[BM(x+dx,y+dy)] = color;
            bvram_mirror[BM(x+dx,y+dy)] = color;
        }
    }
}

/* simple fill function, bmp coordinates */
static void fill_rect(uint32_t x, uint32_t y, int32_t w, int32_t h, uint32_t color)
{
    uint8_t *const bvram = bmp_vram();
    uint8_t *const bvram_mirror = get_bvram_mirror();
    
    if(!bvram || !bvram_mirror)
    {
        return;
    }
    
    for(int32_t dy = 0; dy < h; dy++)
    {
        for(int32_t dx = 0; dx < w; dx++)
        {
            bvram[BM(x+dx,y+dy)] = color;
            bvram_mirror[BM(x+dx,y+dy)] = color;
        }
    }
}

/* wrapper to mark a dot specified by raw x/y coordinates */
static void mark_dot(uint32_t x, uint32_t y, uint32_t color)
{
    mark_block(RAW2BM_X(x), RAW2BM_Y(y), color, 2);
}

/* return pixel value of given raw coordinate */
static uint32_t get_pixel_value(uint32_t x, uint32_t y)
{
    return (raw_get_pixel(x,y) + raw_get_pixel(x+1,y) + raw_get_pixel(x,y+1) + raw_get_pixel(x+1,y+1)) / 4;
}

/* checks pixel at raw coordinate if it is set, uses margin as decision level */
static uint32_t is_set(uint32_t margin, uint32_t x, uint32_t y)
{
    uint32_t value = get_pixel_value(x,y);
    
    if(value > margin)
    {
        return 1;
    }
    
    return 0;
}

/* paint a large representation of the raw content as bmp */
static void paint_large(uint32_t margin, uint32_t x_raw, uint32_t y_raw)
{
    int32_t width = 40;
    
    for(int32_t y = -width/2; y < width/2; y++)
    {
        for(int32_t x = -width/2; x < width/2; x++)
        {
            uint32_t color = COLOR_BLACK;
            
            if(is_set(margin, x_raw + x, y_raw + y))
            {
                color = COLOR_RED;
            }
            mark_block(720/2 + 10*x, 480/2 + 10*y, color, 10);
        }
    }
    mark_block(720/2, 480/2, COLOR_GREEN2, 10);
}

/* try to find the center. quite unintelligent but fast */
static uint32_t fine_tune_dot(uint32_t margin, uint32_t *x, uint32_t *y, uint32_t *w, uint32_t *h)
{
    uint32_t x_min = *x;
    uint32_t y_min = *y;
    uint32_t x_max = *x;
    uint32_t y_max = *y;
    
    /* no margin specified - use the current position's brightness as reference */
    if(!margin)
    {
        margin = (uint32_t)get_pixel_value(*x, *y) * 80 / 100;
    }
    
    /* first center horizontally as good as possible at the current X pos */
    while(x_min > 0 && is_set(margin, x_min - 1, *y))
    {
        x_min--;
    }
    while(x_max < (uint32_t)raw_info.width - 1 && is_set(margin, x_max + 1, *y))
    {
        x_max++;
    }
    
    *w = x_max - x_min;
    *x = x_min + (*w / 2);
    
    /* now find Y center */
    while(y_min > 0 && is_set(margin, *x, y_min - 1))
    {
        y_min--;
    }
    while(y_max < (uint32_t)raw_info.height - 1 && is_set(margin, *x, y_max + 1))
    {
        y_max++;
    }
    
    *h = y_max - y_min;
    *y = y_min + (*h / 2);
    
    /* rerun X as an incorrect Y could have caused incorrect center detection for X before */
    x_min = *x;
    x_max = *x;
    
    while(x_min > 0 && is_set(margin, x_min - 1, *y))
    {
        x_min--;
    }
    while(x_max < (uint32_t)raw_info.width - 1 && is_set(margin, x_max + 1, *y))
    {
        x_max++;
    }
    
    *w = x_max - x_min;
    *x = x_min + (*w / 2);
    
    return 1;
}

/* returns the brightest location and its coordinates */
static uint32_t find_brightest(uint32_t margin, uint32_t *x_ret, uint32_t *y_ret, uint32_t *w_ret, uint32_t *h_ret, uint32_t x_start, uint32_t y_start, uint32_t width, uint32_t height)
{
    int step = 4;
    uint32_t brightness = margin;
    
    /* a maximum width and height can be specified, or the maximum will be used if the value is zero or invalid */
    if(width == 0 || width > raw_info.width - x_start - 2)
    {
        width = raw_info.width - x_start - 2;
    }
    
    if(height == 0 || height > raw_info.height - y_start - 2)
    {
        height = raw_info.height - y_start - 2;
    }
    
    /* go through all pixels and check if they are brighter than the margin */
    for(uint32_t y = y_start; y < y_start + height; y += step)
    {
        for(uint32_t x = x_start; x < x_start + width; x += step)
        {
            uint32_t value = get_pixel_value(x, y);
            
            /* we have a low margin value anyway (black or white, no grayscales), so this does work good enough */
            if(value > brightness)
            {
                brightness = value;
                *x_ret = x;
                *y_ret = y;
            }
        }
    }
    
    /* now fine tune to make sure we find the center */
    uint32_t dot_width = 0;
    uint32_t dot_height = 0;
    
    fine_tune_dot(margin, x_ret, y_ret, &dot_width, &dot_height);

    /* if a dot size return size is wanted, return it */
    if(w_ret && h_ret)
    {
        *w_ret = dot_height;
        *h_ret = dot_height;
    }
    
    return brightness;
}

/* return what the control dots currently tell us */
static uint32_t read_control()
{
    uint32_t retval = 0;
    uint32_t bitnum = 0;
    
    while(bitnum < COUNT(control_dots))
    {
        uint32_t state = is_set(detection_margin, control_dots[bitnum].x, control_dots[bitnum].y);
        retval <<= 1;
        retval |= state;
        bitnum++;
    }
    
    return retval;
}

/* read all data dots */
static uint32_t read_data(uint8_t *buffer)
{
    uint8_t data_byte = 0;
    uint32_t bits_read = 0;
    uint32_t bytes_read = 0;
    
    for(uint32_t dot_y = 0; dot_y < dots_y; dot_y++)
    {
        for(uint32_t dot_x = 0; dot_x < dots_x; dot_x++)
        {
            uint32_t bitval = is_set(detection_margin, dots[DOTPOS(dot_x,dot_y)].x, dots[DOTPOS(dot_x,dot_y)].y) ? 1 : 0;
            
            data_byte <<= 1;
            data_byte |= bitval;
            
            bits_read++;
            bits_read %= 8;
            
            /* read 8 bits? write byte */
            if(bits_read == 0)
            {
                buffer[bytes_read] = data_byte;
                bytes_read++;
                data_byte = 0;
            }
        }
    }
    
    return 1;
}

/* try to find the control dots on screen */
static uint32_t detect_control_dots()
{
    uint32_t last_x = 0;
    uint32_t last_y = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t bitnum = 0;
    uint32_t avg_level = 0;
    
    while(bitnum < COUNT(control_dots))
    {
        uint32_t level = find_brightest(detection_margin, &x, &y, &w, &h, last_x, last_y - h, 2*w, 2*h);
        
        bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "C%d", bitnum);
        
        if(level > detection_margin)
        {
            printf("C%d at %d/%d (%dx%d)\n", bitnum, x, y, w, h);
            control_dots[bitnum].x = x;
            control_dots[bitnum].y = y;
            bitnum++;
            
            /* average their brightness to use it as detection margin */
            avg_level += (level - dark_level);
            
            paint_large(detection_margin, x, y);
            
            last_x = x;
            last_y = y;
            
            mark_dot(x, y, COLOR_RED);
        
            /* wait till it disappears */
            while(is_set(detection_margin, x, y))
            {
                msleep(100);
            }
        }
        else            
        {
            msleep(1);
        }
    }
    
    fill_rect(0, 0, 720, 480, COLOR_EMPTY);
    
    /* set margin at 50% of the average center brightness */
    detection_margin = dark_level + avg_level / 32;
    
    /* now wait for control dots telling us the data field dimension */
    bmp_printf(FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "SETUP");
    printf("waiting for SEQAAAA\n");
    while(read_control() != 0xAAAA)
    {
        msleep(1);
    }
    printf("waiting for SEQ5555\n");
    while(read_control() != 0x5555)
    {
        msleep(1);
    }
    printf("waiting for SEQFFFF\n");
    while(read_control() != 0xFFFF)
    {
        msleep(1);
    }
    
    printf("waiting for idle\n");
    while(read_control() & 0xFF00)
    {
        msleep(1);
    }
    printf("waiting for XRES\n");
    while(!(read_control() & 0xFF00))
    {
        msleep(1);
    }
    dots_x = read_control() & 0xFF;
    
    printf("waiting for idle\n");
    while(read_control() & 0xFF00)
    {
        msleep(1);
    }
    printf("waiting for YRES\n");
    while(!(read_control() & 0xFF00))
    {
        msleep(1);
    }
    dots_y = read_control() & 0xFF;
    
    printf("waiting for idle\n");
    while(read_control())
    {
        msleep(1);
    }
    
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "DONE");

    return 1;
}

/* just setup buffers etc */
static uint32_t init_dots()
{
    data_length = dots_x * dots_y / 8;
    printf("Data length: %d byte (%dx%d)\n", data_length, dots_x, dots_y);
    
    dots = malloc(dots_x * dots_y * sizeof(pixel_info_t));
    if(!dots)
    {
        return 0;
    }
    
    data = fio_malloc(data_length);
    if(!data)
    {
        return 0;
    }
    
    return 1;
}

/* do a full screen search for a data dot and show on screen */
static uint32_t detect_dot(uint32_t dot_x, uint32_t dot_y)
{
    uint32_t last_time = get_ms_clock_value();
    
    while(get_ms_clock_value() - last_time < 5000)
    { 
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t w = 0;
        uint32_t h = 0;
        
        uint32_t level = find_brightest(detection_margin, &x, &y, &w, &h, 0, 0 ,0 ,0);
        
        if(level > detection_margin)
        {
            /* detected, fine tune */
            fine_tune_dot(0, &x, &y, &w, &h);
            
            printf("D%d/%d at %d/%d (%dx%d)\n", dot_x, dot_y, x, y, w, h);
            dots[DOTPOS(dot_x,dot_y)].x = x;
            dots[DOTPOS(dot_x,dot_y)].y = y;
            
            mark_dot(x,y, COLOR_RED);
            paint_large(detection_margin, x, y);
            
            /* wait till it disappears */
            while(is_set(detection_margin, x, y))
            {
            }
            
            return 1;
        }
    }
    
    return 0;
}

/* detect all data dots on screen */
static uint32_t detect_dots()
{
    printf("Detect dots...\n");
    trace_write(trace_ctx, "Enter detect_dots()");
    
    /* first find the three corner dots when they blink */
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "Ref 0");
    trace_write(trace_ctx, "Detect ref 0");
    if(!detect_dot(0, 0))
    {
        NotifyBox(5000, "Data calib dot 0 detection failed");
        return 0;
    }
    
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "Ref 1");
    trace_write(trace_ctx, "Detect ref 1");
    if(!detect_dot(dots_x-1, 0))
    {
        NotifyBox(5000, "Data calib dot 1 detection failed");
        return 0;
    }
    
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "Ref 2");
    trace_write(trace_ctx, "Detect ref 2");
    if(!detect_dot(0, dots_y-1))
    {
        NotifyBox(5000, "Data calib dot 2 detection failed");
        return 0;
    }
    
    /* wait till they come back */
    while(!is_set(detection_margin, dots[DOTPOS(0,0)].x, dots[DOTPOS(0,0)].y))
    {
        msleep(1);
    }
    /* plus a bit more to make sure everything is shown */
    msleep(50);
    
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "Ref Matrix");
    trace_write(trace_ctx, "Detect data ref done");
    trace_write(trace_ctx, "    (0,0) at %d %d", dots[DOTPOS(0,0)].x, dots[DOTPOS(0,0)].y);
    trace_write(trace_ctx, "    (x,0) at %d %d", dots[DOTPOS(dots_x-1,0)].x, dots[DOTPOS(dots_x-1,0)].y);
    trace_write(trace_ctx, "    (0,y) at %d %d", dots[DOTPOS(0,dots_y-1)].x, dots[DOTPOS(0,dots_y-1)].y);
    
    /* and determine base coordinates and dot distance. no skew or rotation compensation */
    uint32_t base_x = dots[DOTPOS(0,0)].x;
    uint32_t base_y = dots[DOTPOS(0,0)].y;
    uint32_t dist_x = (dots[DOTPOS(dots_x-1,0)].x - base_x) / (dots_x-1);
    uint32_t dist_y = (dots[DOTPOS(0,dots_y-1)].y - base_y) / (dots_y-1);
    
    /* we now know where the dots theoretically are. now fine tune all of them */
    for(uint32_t dot_y = 0; dot_y < dots_y; dot_y++)
    {
        msleep(1);
        
        for(uint32_t dot_x = 0; dot_x < dots_x; dot_x++)
        {
            uint32_t x = 0;
            uint32_t y = 0;
            uint32_t w = 0;
            uint32_t h = 0;
            
            bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "D: %d/%d    ", dot_x, dot_y);
            trace_write(trace_ctx, "Tuning dot %d %d", dot_x, dot_y);
        
            /* the expected x/y coordinates are determined depending on previous dots */
            if(dot_x == 0)
            {
                /* leftmost dot, there is none left of us */
                if(dot_y == 0)
                {
                    /* if there is none above us, its the base */
                    x = base_x;
                    trace_write(trace_ctx, "    x = base_x -> %d", x);
                }
                else
                {
                    /* else use X position of dot above */
                    x = dots[DOTPOS(dot_x,dot_y - 1)].x;
                    trace_write(trace_ctx, "    x = dots[DOTPOS(dot_x,dot_y - 1)].x -> %d", x);
                }
            }
            else
            {
                /* there are dots left of us, just add distance */
                x = dots[DOTPOS(dot_x - 1,dot_y)].x + dist_x;
                trace_write(trace_ctx, "    x = dots[DOTPOS(dot_x - 1,dot_y)].x + dist_x -> %d", x);
            }
            
            if(dot_y == 0)
            {
                /* topmost dot, there is none above us */
                if(dot_x == 0)
                {
                    /* if there is none above us, its the base */
                    y = base_y;
                    trace_write(trace_ctx, "    y = base_y -> %d", y);
                }
                else
                {
                    /* else use Y position of dot left of this one */
                    y = dots[DOTPOS(dot_x - 1,dot_y)].y;
                    trace_write(trace_ctx, "    y = dots[DOTPOS(dot_x - 1,dot_y)].y -> %d", y);
                }
            }
            else
            {
                /* there are dots above us, just add distance */
                y = dots[DOTPOS(dot_x,dot_y - 1)].y + dist_y;
                trace_write(trace_ctx, "    dots[DOTPOS(dot_x,dot_y - 1)].y + dist_y -> %d", y);
            }
            
            /* we expect to hit at least a white dot there... */
            if(!is_set(detection_margin, x, y))
            {
                /* uh oh, a bit off. maybe its 2 pixels away from the expected position? */
                uint32_t level = find_brightest(detection_margin, &x, &y, NULL, NULL, x-2, y-2, 4, 4);
                
                if(level < detection_margin)
                {
                    trace_write(trace_ctx, "    -> not set!");
                    paint_large(detection_margin, x, y);
                    msleep(5000);
                    NotifyBox(5000, "Calib failed %d/%d at %d/%d", dot_x, dot_y, x, y);
                    return 0;
                }
            }
            
            /* detected, fine tune. needed because we do not correct rotation */
            trace_write(trace_ctx, "    #0 at %d %d", x, y);
            fine_tune_dot(detection_margin, &x, &y, &w, &h);
            trace_write(trace_ctx, "    #1 at %d %d", x, y);
            
            /* it should now still be white :) */
            if(!is_set(detection_margin, x, y))
            {
                trace_write(trace_ctx, "    -> not set!");
                paint_large(detection_margin, x, y);
                msleep(5000);
                NotifyBox(5000, "Calib failed [tune] %d/%d at %d/%d,%d/%d", dot_x, dot_y, x, y, w, h);
                return 0;
            }
            
            /* and store position */
            dots[DOTPOS(dot_x,dot_y)].x = x;
            dots[DOTPOS(dot_x,dot_y)].y = y;
            trace_write(trace_ctx, "    SET: (%d,%d) at %d %d", dot_x, dot_y, dots[DOTPOS(dot_x,dot_y)].x, dots[DOTPOS(dot_x,dot_y)].y);
            
            mark_dot(x,y, COLOR_RED);
        }
    }
    trace_write(trace_ctx, "Finished detect_dots()");
    
    return 1;
}

/* detect all dots, to be ran when the black screen shows up */
static uint32_t calib_dots()
{
    uint32_t x = 0;
    uint32_t y = 0;
    
    trace_write(trace_ctx, "Enter calib_dots()");
    
    /* first to a rough detection of the on/off margin */
    dark_level = find_brightest(0, &x, &y, NULL, NULL, 0, 0, 0, 0);
    detection_margin = dark_level + 2048;
    trace_write(trace_ctx, "Dark: %d", dark_level);

    /* then find our control dots which tell us screen dimensions and are used for flow control */
    if(!detect_control_dots())
    {
        trace_write(trace_ctx, "detect_control_dots() failed");
        NotifyBox(5000, "Control dot detection failed");
        return 0;
    }
    
    /* then alloc stuff */
    if(!init_dots())
    {
        trace_write(trace_ctx, "init_dots() failed");
        NotifyBox(5000, "Allocation failure");
        return 0;
    }
    
    /* and detect the data dots */
    if(!detect_dots())
    {
        trace_write(trace_ctx, "detect_dots() failed");
        return 0;
    }
    
    fill_rect(0, 0, 720, 480, COLOR_EMPTY);
    trace_write(trace_ctx, "Finished calib_dots()");
    return 1;
}

/* read data from screen and write to a file */
static void dump_memory()
{
    FILE *handle = FIO_CreateFile("DUMP.BIN");
    uint32_t last_time = get_ms_clock_value();
    uint32_t block = 0;
    uint32_t failed = 0;
    uint32_t address = 0;
    
    bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 20, "Dumping");
    while(1)
    {
        last_time = get_ms_clock_value();
        
        /* control 0xFF00 tells us the data is valid */
        while(read_control() != 0xFF00)
        {
            if(get_ms_clock_value() - last_time > 20000 || get_halfshutter_pressed())
            {
                bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 60, "Timeout 1   ");
                FIO_CloseFile(handle);
                return;
            }
        }

        /* it was set, so data on screen is assumed to be valid */
        read_data(data);
        
        /* check header CRC */
        if(!test_correct_header(data))
        {
            bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 140, "Header CRC Error  ");
            failed++;
        }
        /* and data CRC */
        else if(!test_correct_data(data))
        {
            bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 140, "Data CRC Error    ");
            failed++;
        }
        /* and read the block address */
        else if(!unpack_block(data, NULL, NULL, &address))
        {
            bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 140, "Unpack Error      ");
            failed++;
        }
        else
        {
            block++;
        }
        
        /* write in any case. checksums/errors can be brute forced later, which could take some time */
        FIO_WriteFile(handle, data, data_length);
        
        /* now wait till the control word says 0x00FF and tells us new data is about to come */
        last_time = get_ms_clock_value();
        while(read_control() != 0x00FF)
        {
            msleep(1);
            if(get_ms_clock_value() - last_time > 5000 || get_halfshutter_pressed())
            {
                bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 60, "Timeout 2   ");
                FIO_CloseFile(handle);
                return;
            }
        }
        
        /* hint: control words 0xFF00 and 0x00FF were chosen to safely detect set/unset states and not to trigger
           while a transition is happening. maybe it would be better to chose 0xFAA0 and 0x055F to cover more 
           bits of crosstalk also (8 crosstalking instead of only 1). but it is already stable enough */
        
        bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 60 + 0 * font_med.height, "Received #%d (%d byte)    ", block, block * data_length);
        bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 60 + 1 * font_med.height, "Failed   #%d      ", failed);
        bmp_printf( FONT(FONT_MED, COLOR_WHITE, COLOR_BG_DARK), 10, 60 + 2 * font_med.height, "Address 0x%08X    ", address);
    }
}

static void rom_dump_task()
{
    raw_lv_request();
    
    if (!raw_update_params())
    {
        return;
    }
    
    trace_write(trace_ctx, "Starting...");
    if(!calib_dots())
    {
        return;
    }
    dump_memory();
}

static struct menu_entry rom_dump_menu[] =
{
    {
        .name = "Dump memory",
        .select = run_in_separate_task,
        .priv = rom_dump_task,
    }
};

static unsigned int rom_dump_init()
{
    char filename[] = "rom_dump.log";
    trace_ctx = trace_start("rom_dump", filename);
    trace_set_flushrate(trace_ctx, 60000);
    trace_format(trace_ctx, TRACE_FMT_TIME_REL | TRACE_FMT_COMMENT, ' ');
    trace_write(trace_ctx, "ROM Dumper started");
    
    menu_add("Debug", rom_dump_menu, COUNT(rom_dump_menu));
    return 0;
}

static unsigned int rom_dump_deinit()
{
    menu_remove("Debug", rom_dump_menu, COUNT(rom_dump_menu));
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(rom_dump_init)
    MODULE_DEINIT(rom_dump_deinit)
MODULE_INFO_END()
