/** \file
 * Minimal ML - for debugging
 */

#include "dryos.h"
#include "vram.h"
#include "bmp.h"
#include "font_direct.h"
#include "imgconv.h"

#if 0
/* ROM dumper */
extern FILE* _FIO_CreateFile(const char* filename );

/* this cannot run from init_task */
static void run_test()
{
    /* change to A:/ for CF cards */
    FILE * f = _FIO_CreateFile("B:/FF000000.BIN");
    
    if (f != (void*) -1)
    {
        FIO_WriteFile(f, (void*) 0xFF000000, 0x1000000);
        FIO_CloseFile(f);
    }
}
#endif

static void hello_world()
{
    /* wait for display to initialize */
    while (!bmp_vram_raw())
    {
        msleep(100);
    }

    while(1)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(500);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
        msleep(500);
        
        font_draw(100, 75, COLOR_WHITE, 3, "Hello, World!");
    }
}

/* called before Canon's init_task */
void boot_pre_init_task(void)
{
    /* nothing to do */
}

/* called right after Canon's init_task, while their initialization continues in background */
void boot_post_init_task(void)
{
    task_create("run_test", 0x1e, 0x4000, hello_world, 0 );
}

/* used by font_draw */
void disp_set_pixel(int x, int y, int c)
{
    uint8_t * bmp = bmp_vram_raw();

#ifdef CONFIG_DIGIC_45
    bmp[x + y * BMPPITCH] = c;
#endif

#ifdef CONFIG_DIGIC_678
    struct MARV * MARV = bmp_marv();
    uint8_t * disp_framebuf = bmp;

    // UYVY display, must convert
    uint32_t color = 0xFFFFFF00;
    uint32_t uyvy = rgb2yuv422(color >> 24,
                              (color >> 16) & 0xff,
                              (color >> 8) & 0xff);

    if (MARV->opacity_data)
    {
        /* 80D, 200D */

        uint32_t disp_xres = MARV->width;
        uint32_t disp_yres = MARV->width;

        /* from names_are_hard, https://pastebin.com/Vt84t4z1 */
        uint8_t *pixel;
        if (x % 2)
        {
            pixel = disp_framebuf + (x*2 + y*2*disp_xres);
            *pixel = (uyvy >> 16) & 0xff;

            pixel = disp_framebuf + (x*2 + y*2*disp_xres + 1);
            *pixel = (uyvy >> 24) & 0xff;
        }
        else
        {
            pixel = disp_framebuf + (x*2 + y*2*disp_xres);
            *pixel = (uyvy >>  0) & 0xff;

            pixel = disp_framebuf + (x*2 + y*2*disp_xres + 1);
            *pixel = (uyvy >>  8) & 0xff;
        }

        /* FIXME: opacity buffer not updated */
    }
    else
    {
        /* 5D4, M50 */

        uint32_t buf_xres = MARV->width;
        uint32_t buf_yres = MARV->width;

        /* from https://bitbucket.org/chris_miller/ml-fork/src/d1f1cdf978acc06c6fd558221962c827a7dc28f8/src/minimal-d678.c?fileviewer=file-view-default#minimal-d678.c-175 */
        // VRAM layout is UYVYAA (each character is one byte) for pixel pairs
        uint8_t *offset = disp_framebuf + (x * 3 + y * 3 * buf_xres);
        uint8_t u = uyvy >>  0 & 0xff;
        uint8_t v = uyvy >> 16 & 0xff;
        uint8_t alpha = color & 0xff;
        if (!(x & 1)) {
            // First pixel in the pair, so we set U, Y1, V, A1
            *offset = u;
            *(offset + 1) = uyvy >> 8 & 0xff;
            *(offset + 2) = v;
            *(offset + 4) = alpha;
        } else {
            // Second pixel in the pair, so we set U, V, Y2, A2
            *(offset - 3) = u;
            *(offset - 1) = v;
            *offset = uyvy >> 24 & 0xff;
            *(offset + 2) = alpha;
        }
    }
#endif
}
