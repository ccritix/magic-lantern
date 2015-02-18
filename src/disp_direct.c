

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "font_direct.h"
#include "disp_direct.h"
#include "compiler.h"
#include "consts.h"

#define MEM(x) (*(volatile uint32_t *)(x))
#define UYVY_PACK(u,y1,v,y2) ((u) & 0xFF) | (((y1) & 0xFF) << 8) | (((v) & 0xFF) << 16) | (((y2) & 0xFF) << 24);
 
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define COERCE(x,lo,hi) MAX(MIN((x),(hi)),(lo))

#define ABS(a) ({ __typeof__ (a) _a = (a); _a > 0 ? _a : -_a; })

uint8_t *disp_framebuf = (uint8_t *)0x44000000;
uint8_t *disp_yuvbuf = (uint8_t *)0x44800000;

uint32_t disp_yres = 480;
uint32_t disp_xres = 720;

void disp_set_palette()
{
    // transparent
    // 1 - red
    // 2 - green
    // 3 - blue
    // 4 - cyan
    // 5 - magenta
    // 6 - yellow
    // 7 - orange
    // 8 - transparent black
    // 9 - black
    // A - gray 1
    // B - gray 2
    // C - gray 3
    // D - gray 4
    // E - gray 5
    // F - white

    uint32_t palette_pb[16] = {0x00fc0000, 0x0346de7f, 0x036dcba1, 0x031a66ea, 0x03a42280, 0x03604377, 0x03cf9a16, 0x0393b94b, 0x00000000, 0x03000000, 0x03190000, 0x033a0000, 0x03750000, 0x039c0000, 0x03c30000, 0x03eb0000};

    for(uint32_t i = 0; i < 16; i++)
    {
        MEM(0xC0F14080 + i*4) = palette_pb[i];
    }
    MEM(0xC0F14078) = 1;
}

uint32_t rgb2yuv422(int R, int G, int B)
{
    int Y = COERCE(((217) * R + (732) * G + (73) * B) / 1024, 0, 255);
    int U = COERCE(((-117) * R + (-394) * G + (512) * B) / 1024, -128, 127);
    int V = COERCE(((512) * R + (-465) * G + (-46) * B) / 1024, -128, 127);
    return UYVY_PACK(U,Y,V,Y);
}

void disp_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t pixnum = ((y * disp_xres) + x) / 2;
    
    if(x & 1)
    {
        disp_framebuf[pixnum] = (disp_framebuf[pixnum] & 0x0F) | ((color & 0x0F)<<4);
    }
    else
    {
        disp_framebuf[pixnum] = (disp_framebuf[pixnum] & 0xF0) | (color & 0x0F);
    }
}

void disp_update()
{
    /* set frame buffer memory areas */
    MEM(0xC0F140D0) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F140E0) = (uint32_t)disp_yuvbuf & ~0x40000000;
    
    /* trigger a display update */
    MEM(0xC0F14000) = 1;
}

void disp_fill(uint32_t color)
{
    /* build a 32 bit word */
    uint32_t val = color;
    val |= val << 4;
    val |= val << 8;
    val |= val << 16;
    
    for(uint32_t ypos = 0; ypos < disp_yres; ypos++)
    {
        /* we are writing 8 pixels at once with a 32 bit word */
        for(uint32_t xpos = 0; xpos < disp_xres; xpos += 8)
        {
            /* get linear pixel number */
            uint32_t pixnum = ((ypos * disp_xres) + xpos);
            /* two pixels per byte */
            uint32_t *ptr = (uint32_t *)&disp_framebuf[pixnum / 2];
            
            *ptr = val;
        }
    }
}

void disp_fill_yuv(uint32_t color)
{
    for(uint32_t ypos = 0; ypos < disp_yres; ypos++)
    {
        /* we are writing 2 pixels at once with a 32 bit word */
        for(uint32_t xpos = 0; xpos < disp_xres; xpos += 2)
        {
            /* get linear pixel number */
            uint32_t pixnum = ((ypos * disp_xres) + xpos);
            /* two byte per pixel */
            uint32_t *ptr = (uint32_t *)&disp_yuvbuf[pixnum * 2];
            
            /* ok that is making things slow.... */
            if(color == 0xFFFFFFFF)
            {
                /* but we love funny patterns :) */
                *ptr = rgb2yuv422(xpos, ypos, ABS(xpos-ypos));
            }
            else
            {
                *ptr = color;
            }
        }
    }
}

void disp_progress(uint32_t progress)
{
    uint32_t width = 480;
    uint32_t height = 20;
    uint32_t paint = progress * width / 255;
    
    for(uint32_t ypos = (disp_yres - height) / 2; ypos < (disp_yres + height) / 2; ypos++)
    {
        for(uint32_t xofs = 0; xofs < width; xofs++)
        {
            uint32_t xpos = (disp_xres - width) / 2 + xofs;
            
            if(xofs >= paint)
            {
                disp_set_pixel(xpos, ypos, COLOR_BLACK);
            }
            else
            {
                disp_set_pixel(xpos, ypos, COLOR_RED);
            }
        }
    }
    
    /* print progress in percent */
    char text[32];
    
    snprintf(text, 32, "%d%%", progress * 100 / 255);
    font_draw(disp_xres / 2 - 28, (disp_yres - height) / 2 + 2, COLOR_WHITE, 2, text);
    
    /* present image */
    disp_update();
}

void disp_init_dummy (uint32_t buffer)
{
}

/* this is hardcoded for 600D */
void disp_init()
{
    disp_framebuf = (uint8_t *)0x44000000;
    void (*fromutil_disp_init)(uint32_t) = &disp_init_dummy;

#if defined(CONFIG_600D)
    fromutil_disp_init = (void (*)(uint32_t))0xFFFF5EC8;
#endif
    
    /* first clear, then init */
    disp_fill(COLOR_BLACK);
    
    /* this one initializes everyhting that is needed for display usage. PWM, PWR, GPIO, SIO and DISPLAY */
    fromutil_disp_init(0);

    /* we want our own palette */
    disp_set_palette();
    
    /* BMP foreground is transparent */
    disp_fill(COLOR_TRANSPARENT_BLACK);
    /* make a funny pattern in the YUV buffer*/
    disp_fill_yuv(0xFFFFFFFF);
    
    /* present new bitmap data */
    disp_update();
}
