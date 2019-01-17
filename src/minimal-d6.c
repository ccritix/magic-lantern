/** \file
 * Minimal test code for DIGIC 6
 * ROM dumper & other experiments
 */

#include "dryos.h"
#include "log-d6.h"

extern void dump_file(char* name, uint32_t addr, uint32_t size);
extern void malloc_info(void);
extern void sysmem_info(void);
extern void smemShowFix(void);

static void DUMP_ASM dump_task()
{
#if 0
    /* print memory info on QEMU console */
    malloc_info();
    sysmem_info();
    smemShowFix();

    /* dump ROM1 */
    dump_file("ROM1.BIN", 0xFC000000, 0x02000000);

    /* dump RAM */
    dump_file("ATCM.BIN", 0x00000000, 0x00004000);
    dump_file("BTCM.BIN", 0x80000000, 0x00010000);
  //dump_file("RAM4.BIN", 0x40000000, 0x40000000);    - runs out of space in QEMU
    dump_file("BFE0.BIN", 0xBFE00000, 0x00200000);
    dump_file("DFE0.BIN", 0xDFE00000, 0x00200000);
  //dump_file("EE00.BIN", 0xEE000000, 0x02000000);    - unknown, may crash 
#endif

    /* save a diagnostic log */
    log_finish();
    call("dumpf");
}

/* called before Canon's init_task */
void boot_pre_init_task(void)
{
    log_start();
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

/* dummy */
int FIO_WriteFile( FILE* stream, const void* ptr, size_t count ) { };

void ml_assert_handler(char* msg, char* file, int line, const char* func) { };
