/** \file
 * Minimal test code for DIGIC 7 & 8
 * ROM dumper & other experiments
 */

#include "dryos.h"

extern void dump_file(char* name, uint32_t addr, uint32_t size);
extern void memmap_info(void);
extern void malloc_info(void);
extern void sysmem_info(void);
extern void smemShowFix(void);

static void led_blink(int times, int delay_on, int delay_off)
{
    for (int i = 0; i < times; i++)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(delay_on);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
        msleep(delay_off);
    }
}

static void DUMP_ASM dump_task()
{
    /* print memory info on QEMU console */
    memmap_info();
    malloc_info();
    sysmem_info();
    smemShowFix();

    /* LED blinking test */
    led_blink(5, 500, 500);

    /* dump ROM */
    dump_file("ROM0.BIN", 0xE0000000, 0x02000000);
  //dump_file("ROM1.BIN", 0xF0000000, 0x02000000); // - might be slow

    /* dump RAM */
  //dump_file("RAM4.BIN", 0x40000000, 0x40000000); // - large file; decrease size if it locks up
  //dump_file("DF00.BIN", 0xDF000000, 0x00010000); // - size unknown; try increasing
 
    /* attempt to take a picture */
    call("Release");
    msleep(2000);

    /* save a diagnostic log */
    call("dumpf");
}

/* called before Canon's init_task */
void boot_pre_init_task(void)
{
    /* nothing to do */
}

/* called right after Canon's init_task, while their initialization continues in background */
void boot_post_init_task(void)
{
    msleep(1000);

    task_create("dump", 0x1e, 0x1000, dump_task, 0 );
}

/* used by font_draw */
/* we don't have a valid display buffer yet */
void disp_set_pixel(int x, int y, int c)
{
}
