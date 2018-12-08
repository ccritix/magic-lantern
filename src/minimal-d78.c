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

#define BACKUP_BLOCKSIZE 0x0010000

#undef malloc
#undef free
#undef _alloc_dma_memory
#define malloc _alloc_dma_memory
#define free _free_dma_memory
extern void * _alloc_dma_memory(size_t);
extern void _free_dma_memory(void *);

#define FIO_CreateFile _FIO_CreateFile
extern FILE* _FIO_CreateFile(const char* filename );

#define FIO_GetFileSize _FIO_GetFileSize
extern int _FIO_GetFileSize(const char * filename, uint32_t * size);

#define FIO_WriteFile _FIO_WriteFile
extern int _FIO_WriteFile( FILE* stream, const void* ptr, size_t count );

extern int FIO_Flush(const char * filename);

static void backup_region(char *file, uint32_t base, uint32_t length)
{
    FILE *handle = NULL;
    uint32_t size = 0;
    uint32_t pos = 0;
    
    /* already backed up that region? */
    if((FIO_GetFileSize( file, &size ) == 0) && (size == length) )
    {
        return;
    }
    
    /* no, create file and store data */

    void* buf = malloc(BACKUP_BLOCKSIZE);
    if (!buf) return;

    handle = FIO_CreateFile(file);
    if (handle)
    {
      while(pos < length)
      {
         uint32_t blocksize = BACKUP_BLOCKSIZE;
        
          if(length - pos < blocksize)
          {
              blocksize = length - pos;
          }
          
          /* copy to RAM before saving, because ROM is slow and may interfere with LiveView */
          memcpy(buf, &((uint8_t*)base)[pos], blocksize);
          
          FIO_WriteFile(handle, buf, blocksize);
          pos += blocksize;

          /* throttle to prevent freezing */
          msleep(10);
      }
      FIO_CloseFile(handle);
      FIO_Flush(file);
    }
    
    free(buf);
}

static void DUMP_ASM dump_task()
{
    /* LED blinking test */
    led_blink(2, 500, 500);

    /* print memory info on QEMU console */
    memmap_info();
    malloc_info();
    sysmem_info();
    smemShowFix();


    /* dump ROM (this method gives garbage on M50, why?) */
    //dump_file("ROM0.BIN", 0xE0000000, 0x02000000);
    //dump_file("ROM1.BIN", 0xF0000000, 0x02000000); // - might be slow

    /* dump ROM */
    backup_region("B:/ROM0.BIN", 0xE0000000, 0x02000000);
    backup_region("B:/ROM1.BIN", 0xF0000000, 0x01000000);
 
    /* dump RAM */
    //dump_file("RAM4.BIN", 0x40000000, 0x40000000); // - large file; decrease size if it locks up
    //dump_file("DF00.BIN", 0xDF000000, 0x00010000); // - size unknown; try increasing

    /* attempt to take a picture */
    //call("Release");
    //msleep(2000);

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
