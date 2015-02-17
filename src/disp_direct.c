

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "disp_direct.h"
#include "compiler.h"
#include "consts.h"

#define MEM(x) (*(volatile uint32_t *)(x))

uint8_t *disp_framebuf = (uint8_t *)0x44000000;

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

void disp_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    uint32_t yres = 480;
    uint32_t xres = 720;
    uint32_t pixnum = ((y * xres) + x) / 2;
    
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
    /* set frame buffer memory area */
    MEM(0xC0F140D0) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F140D4) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F140E0) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F140E4) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F14110) = (uint32_t)disp_framebuf & ~0x40000000;
    MEM(0xC0F14114) = (uint32_t)disp_framebuf & ~0x40000000;
    
    /* trigger a display update */
    MEM(0xC0F14000) = 1;
}

void disp_fill(uint32_t color)
{
    uint32_t yres = 480;
    uint32_t xres = 720;
    
    /* build a 32 bit word */
    uint32_t val = color;
    val |= val << 4;
    val |= val << 8;
    val |= val << 16;
    
    for(uint32_t ypos = 0; ypos < yres; ypos++)
    {
        for(uint32_t xpos = 0; xpos < xres; xpos += 4)
        {
            uint32_t pixnum = ((ypos * xres) + xpos);
            uint32_t *ptr = (uint32_t *)&disp_framebuf[pixnum];
            *ptr = val;
        }
    }
}


void disp_progress(uint32_t progress)
{
    uint32_t yres = 480;
    uint32_t xres = 720 / 2;
    uint32_t height = 20;
    
    if(!progress)
    {
        height = yres;
    }
    
    for(uint32_t ypos = (yres - height) / 2; ypos < (yres + height) / 2; ypos++)
    {
        for(uint32_t xpos = 0; xpos < xres; xpos++)
        {
            if(xpos * 255 / xres >= progress)
            {
                disp_set_pixel(xpos, ypos, COLOR_BLACK);
            }
            else
            {
                disp_set_pixel(xpos, ypos, COLOR_RED);
            }
        }
    }
    
    disp_update();
}

/* this is hardcoded for 600D */
void disp_init()
{
    disp_framebuf = (uint8_t *)0x44000000;

#if defined(CONFIG_600D)
    void (*fromutil_disp_init)(uint32_t) = 0xFFFF5EC8;
#endif
    
    /* first clear, then init */
    disp_fill(COLOR_BLACK);
    fromutil_disp_init(0);

    disp_set_palette();
    disp_fill(COLOR_BLACK);
    
    disp_update();
}
