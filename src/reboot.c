/** \file
 * Reboot into the hacked firmware.
 *
 * This program is very simple: attempt to reboot into the normal
 * firmware RAM image after startup.
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>


#include "sd_direct.h"
#include "disp_direct.h"
#include "font_direct.h"
#include "led_dump.h"
#include "prop_diag.h"

#include "compiler.h"
#include "consts.h"
#include "fullfat.h"
#include "md5.h"
#include "atag.h"

#define MEM(x) (*(volatile uint32_t *)(x))

asm(
    ".text\n"
    ".globl _start\n"
    "_start:\n"

    /* if used in a .fir file, there is a 0x120 byte address offset.
       so cut the first 0x120 bytes off autoexec.bin before embedding into .fir
     */
    "B       skip_fir_header\n"
    ".incbin \"version.bin\"\n" // this must have exactly 0x11C (284) bytes
    "skip_fir_header:\n"

    "MRS     R0, CPSR\n"
    "BIC     R0, R0, #0x3F\n"   // Clear I,F,T
    "ORR     R0, R0, #0xD3\n"   // Set I,T, M=10011 == supervisor
    "MSR     CPSR, R0\n"
    "B       cstart\n"
    
    /* return */
    ".globl _vec_data_abort\n"
    "_vec_data_abort:\n"
    "STMFD SP!, {R0-R1}\n"
    "ADR R0, _dat_data_abort\n"
    "LDR R1, [R0]\n"
    "ADD R1, R1, #1\n"
    "STR R1, [R0]\n"
    "LDMFD SP!, {R0-R1}\n"
    "SUBS PC, R14, #4\n"

    ".globl _dat_data_abort\n"
    "_dat_data_abort:\n"
    ".word 0x00000000\n"
);

static void busy_wait(int n)
{
    int i,j;
    static volatile int k = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 100000; j++)
            k++;
}


void led_on()
{
#if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
    MEM(CARD_LED_ADDRESS) = (LEDON);
#endif
}

void led_off()
{
#if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
    MEM(CARD_LED_ADDRESS) = (LEDOFF);
#endif
}

static void blink(int n)
{
#if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
    *(volatile int*) (CARD_LED_ADDRESS) = (LEDON);
    busy_wait(n);
    *(volatile int*)(CARD_LED_ADDRESS) = (LEDOFF);
    busy_wait(n);
#endif
}

static void fail()
{
    while (1)
    {
        blink(20);
    }
}

/* file I/O stubs */
static int (*boot_fileopen)(int drive, char *filename, int mode) = 0; // read=1, write=2
static int (*boot_filewrite)(int drive, int handle, void* address, unsigned long size) = 0;
static int (*boot_fileclose)(int drive, int handle) = 0;
static int (*boot_card_init)() = 0;

enum { DRIVE_CF, DRIVE_SD } boot_drive;
enum { MODE_READ=1, MODE_WRITE=2 } boot_access_mode;

static void dump_md5(int drive, char* filename, uint32_t addr, int size)
{
    uint8_t md5_bin[16];
    char md5_ascii[34];
    md5((void *) addr, size, md5_bin);
    snprintf(md5_ascii, sizeof(md5_ascii),
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
        md5_bin[0],  md5_bin[1],  md5_bin[2],  md5_bin[3],
        md5_bin[4],  md5_bin[5],  md5_bin[6],  md5_bin[7],
        md5_bin[8],  md5_bin[9],  md5_bin[10], md5_bin[11],
        md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15]
    );
    char file_base[10];
    snprintf(file_base, sizeof(file_base), "%s", filename);
    file_base[strlen(file_base)-4] = 0;
    char md5file[10];
    snprintf(md5file, sizeof(md5file), "%s.MD5", file_base);
    printf(" - MD5: %s\n", md5_ascii);

    int f = boot_fileopen(drive, md5file, MODE_WRITE);
    if (f != -1)
    {
        boot_filewrite(drive, f, (void*) md5_ascii, 33);
        boot_fileclose(drive, f);
    }
}
static void boot_dump(int drive, char* filename, uint32_t addr, int size)
{
    /* check whether our stubs were initialized */
    if (!boot_fileopen) fail();
    if (!boot_filewrite) fail();
    if (!boot_fileclose) fail();
    
    /* turn on the LED */
    #ifdef CARD_LED_ADDRESS
    MEM(CARD_LED_ADDRESS) = (LEDON);
    #endif

    /* save the file */
    int f = boot_fileopen(drive, filename, MODE_WRITE);
    if (f != -1)
    {
        boot_filewrite(drive, f, (void*) addr, size);
        boot_fileclose(drive, f);
        
        dump_md5(drive, filename, addr, size);
    }
    else
    {
        fail();
    }

    /* turn off the LED */
    #ifdef CARD_LED_ADDRESS
    MEM(CARD_LED_ADDRESS) = (LEDOFF);
    #endif
}

struct sd_ctx sd_ctx;
uint32_t print_x = 0;
uint32_t print_y = 0;
char print_tmp[2];

void print_char()
{
    print_tmp[0] = (char)MEM(0x4080FFF8);
    print_tmp[1] = 0;
    
    font_draw(&print_x, &print_y, COLOR_WHITE, 1, print_tmp, 1);
    
    if(print_y > 480)
    {
        print_y = 480 - 8;
        disp_direct_scroll_up(8);
    }
}

void print_line_ext(uint32_t color, uint32_t scale, char *txt, uint32_t count)
{
    font_draw(&print_x, &print_y, color, scale, txt, count);
    
    if(print_y > 480)
    {
        print_y = 480 - 8;
        disp_direct_scroll_up(8);
    }
}

void print_line(uint32_t color, uint32_t scale, char *txt)
{
    print_line_ext(color, scale, txt, 0);
}

int printf(const char * fmt, ...)
{
    va_list            ap;
    char buf[128];
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf)-1, fmt, ap );
    va_end( ap );
    print_line(COLOR_CYAN, 2, buf);
    return 0;
}

void print_err(FF_ERROR err)
{
    char *message = (char*)FF_GetErrMessage(err);
    
    if(message)
    {
        print_line(COLOR_RED, 1, "   Error code:\n");
        print_line(COLOR_RED, 1, message);
    }
    else
    {
        char text[32];
        
        snprintf(text, 32, "   Error code: 0x%08X\n", err);
        print_line(COLOR_RED, 1, text);
    }
}

static unsigned long FF_blk_read(unsigned char *buffer, unsigned long sector, unsigned short count, void *priv)
{
    uint32_t ret = sd_read(&sd_ctx, sector, count, buffer);
    
    if(ret)
    {
        char text[64];
        
        snprintf(text, 64, "   SD read error: 0x%02X, sector: 0x%08X, count: %d\n", ret, sector, count);
        print_line(COLOR_RED, 1, text);
    }
    
    return ret ? FF_ERR_DRIVER_FATAL_ERROR : 0;
}

static unsigned long FF_blk_write(unsigned char *buffer, unsigned long sector, unsigned short count, void *priv)
{
    uint32_t ret = sd_write(&sd_ctx, sector, count, buffer);
    
    if(ret)
    {
        char text[64];
        
        snprintf(text, 64, "   SD write error: 0x%02X, sector: 0x%08X, count: %d\n", ret, sector, count);
        print_line(COLOR_RED, 1, text);
    }
    
    return ret ? FF_ERR_DRIVER_FATAL_ERROR : 0;
}

void halt_blink_code(uint32_t speed, uint32_t code)
{
    while(1)
    {
        for(uint32_t num = 0; num < code; num++)
        {
            blink(speed);
        }
        busy_wait(speed);
    }
}

FF_IOMAN * fat_init()
{
    FF_IOMAN *ioman = NULL;
    FF_ERROR err = FF_ERR_NONE;
    
    ioman = FF_CreateIOMAN(NULL, 0x4000, 0x200, &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to init driver\n");
        print_err(err);
        halt_blink_code(10, 2);
    }
    
    FF_RegisterBlkDevice(ioman, 0x200, (FF_WRITE_BLOCKS) &FF_blk_write, (FF_READ_BLOCKS) &FF_blk_read, &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to register driver\n");
        print_err(err);
        halt_blink_code(10, 3);
    }
    
    err = FF_MountPartition(ioman, 0);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to mount partition\n");
        print_err(err);
        halt_blink_code(10, 4);
    }
    
    return ioman;
}

void fat_deinit(FF_IOMAN *ioman)
{
    FF_ERROR err = FF_ERR_NONE;
    
    err = FF_UnmountPartition(ioman);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to unmount partition\n");
        print_err(err);
        halt_blink_code(10, 8);
    }
    
    err = FF_DestroyIOMAN(ioman);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to deinit driver\n");
        print_err(err);
        halt_blink_code(10, 9);
    }
}

void dump_rom(FF_IOMAN *ioman)
{
    FF_ERROR err = FF_ERR_NONE;
    
    print_line(COLOR_CYAN, 1, "   Creating dump file\n");
    
    FF_FILE *file = FF_Open(ioman, "\\ROM.BIN", FF_GetModeBits("w"), &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to create dump file\n");
        print_err(err);
        halt_blink_code(10, 5);
    }
    
    print_line(COLOR_CYAN, 1, "   Dumping...\n");
    
    uint32_t block_size = 0x2000;
    uint32_t block_count = 0x01000000 / block_size;
    uint32_t last_progress = 0;
    
    for(uint32_t block = 0; block < block_count; block++)
    {
        uint32_t progress = block * 255 / block_count;
        
        if(progress != last_progress)
        {
            last_progress = progress;
            disp_progress(progress);
        }
        
        err = FF_Write(file, block_size, 1, (FF_T_UINT8 *) (0xF8000000 + block * block_size));
        if(err <= 0)
        {
            print_line(COLOR_RED, 1, "   Failed to write dump file\n");
            print_err(err);
            FF_Close(file);
            FF_UnmountPartition(ioman);
            halt_blink_code(10, 6);
        }
    }
    disp_progress(255);
    
    
    err = FF_Close(file);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to close dump file\n");
        print_err(err);
        halt_blink_code(10, 7);
    }
    
    print_line(COLOR_CYAN, 1, "   Building checksum file\n");
    
    unsigned char md5_output[16];
    md5((void *)0xF8000000, 0x01000000, md5_output);
    
    file = FF_Open(ioman, "\\ROM.MD5", FF_GetModeBits("w"), &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to create MD5 file\n");
        print_err(err);
        halt_blink_code(10, 5);
    }
    
    err = FF_Write(file, 16, 1, md5_output);
    if(err <= 0)
    {
        print_line(COLOR_RED, 1, "   Failed to write MD5 file\n");
        print_err(err);
        FF_Close(file);
        FF_UnmountPartition(ioman);
        halt_blink_code(10, 6);
    }
    
    err = FF_Close(file);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to close MD5 file\n");
        print_err(err);
        halt_blink_code(10, 7);
    }
    
    print_line(COLOR_CYAN, 1, "   Finished, cleaning up\n");
    
    FF_UnmountPartition(ioman);
}

void malloc_init(void *ptr, uint32_t size);
void _vec_data_abort();
extern uint32_t _dat_data_abort;

uint32_t data_abort_occurred()
{
    uint32_t ret = 0;
    
    if(_dat_data_abort)
    {
        ret = 1;
    }
    _dat_data_abort = 0;
    
    return ret;
}

static void find_gaonisoy()
{
    for (uint32_t start = 0xFF000000; start < 0xFFFFFFF0; start += 4)
    {
        if (MEM(start+4) == 0x6e6f6167 && MEM(start+8) == 0x796f7369) // gaonisoy
        {
            printf(" - ROMBASEADDR: 0x%X\n", start);
        }
    }
}


struct model_data_s
{
    uint16_t id;
    const char *name;
};

static const struct model_data_s model_data[] =
{
    { 0x000, "ERROR"   },
    { 0x168, "10D"   },
    { 0x170, "300D"  },
    { 0x175, "20D"   },
    { 0x176, "450D"  },
    { 0x189, "350D"  },
    { 0x190, "40D"   },
    { 0x213, "5D"    },
    { 0x218, "5D2"   },
    { 0x234, "30D"   },
    { 0x236, "400D"  },
    { 0x250, "7D"    },
    { 0x252, "500D"  },
    { 0x254, "1000D" },
    { 0x261, "50D"   },
    { 0x270, "550D"  },
    { 0x285, "5D3"   },
    { 0x286, "600D"  },
    { 0x287, "60D"   },
    { 0x288, "1100D" },
    { 0x289, "7D2"   },
    { 0x301, "650D"  },
    { 0x302, "6D"    },
    { 0x325, "70D"   },
    { 0x331, "EOSM"  },
    { 0x346, "100D"  }
};

static uint32_t get_model_id()
{
    uint32_t *model_ptr = (uint32_t *)0xF8002014;
    
    if(*model_ptr && *model_ptr < 0x00000FFF)
    {
        return *model_ptr;
    }
    
    /* newer models */
    model_ptr = (uint32_t *)0xF8020014;
    if(*model_ptr && *model_ptr < 0x00000FFF)
    {
        return *model_ptr;
    }
    
    return 0;
}

static const char *get_model_string()
{
    uint32_t model = get_model_id();
    
    for(int pos = 0; pos < COUNT(model_data); pos++)
    {
        if(model_data[pos].id == model)
        {
            return model_data[pos].name;
        }
    }
    
    return "unknown";
}

static void print_model()
{
    printf(" - Camera: '%s'\n", get_model_string());
}

/** Shadow copy of the NVRAM boot flags stored at 0xF8000000 */
#define NVRAM_BOOTFLAGS     ((void*) 0xF8000000)
struct boot_flags
{
    uint32_t        firmware;   // 0x00
    uint32_t        bootdisk;   // 0x04
    uint32_t        ram_exe;    // 0x08
    uint32_t        update;     // 0x0c
};

static struct boot_flags * const    boot_flags = NVRAM_BOOTFLAGS;;

static void print_bootflags()
{
    printf(" - Boot flags: "
        "FIR=%d "
        "BOOT=%d "
        "RAM=%d "
        "UPD=%d\n",
        boot_flags->firmware,
        boot_flags->bootdisk,
        boot_flags->ram_exe,
        boot_flags->update
    );

}

void dump_rom_with_canon_routines()
{
    /* file I/O routines are called from "Open file for write: %s\n" */
    
    const char* cam = get_model_string();

    if (strcmp(cam, "5D3") == 0)
    {
        boot_fileopen  = (void*) 0xFFFE9114;
        boot_filewrite = (void*) 0xFFFE9570;
        boot_fileclose = (void*) 0xFFFE927C;
        boot_card_init = (void*) 0xFFFE34B0;
    }

    if (strcmp(cam, "60D") == 0)
    {
        boot_fileopen  = (void*) 0xffff9ce8;
        boot_filewrite = (void*) 0xffffa0f8;
        boot_fileclose = (void*) 0xffff9e38;
    }

    if (strcmp(cam, "550D") == 0)
    {
        boot_fileopen  = (void*) 0xFFFF9D30;
        boot_filewrite = (void*) 0xFFFFA140;
        boot_fileclose = (void*) 0xFFFF9E80;
    }
    
    if (strcmp(cam, "600D") == 0)
    {
        boot_fileopen  = (void*) 0xFFFF9D80;
        boot_filewrite = (void*) 0xFFFFA190;
        boot_fileclose = (void*) 0xFFFF9ED0;
    }

    /* are we calling the right stubs? */
    
    if (!boot_fileopen || !boot_filewrite || !boot_fileclose)
    {
        print_line(COLOR_RED, 2, " - Boot file I/O stubs not set.\n");
        fail();
    }
    
    if (MEM(boot_fileopen)  != 0xe92d41f0 ||
        MEM(boot_filewrite) != 0xe92d4fff ||
        MEM(boot_fileclose) != 0xe92d41f0)
    {
        print_line(COLOR_RED, 2, " - Boot file I/O stubs incorrect.\n");
        fail();
    }

    if (boot_card_init)
    {
        /* not all cameras need this, but some do */
        printf(" - Init SD...\n");
        boot_card_init();
    }

    printf(" - Dumping ROM0...\n");
    boot_dump(DRIVE_SD, "ROM0.BIN", 0xF0000000, 0x01000000);
    printf(" - Dumping ROM1...\n");
    boot_dump(DRIVE_SD, "ROM1.BIN", 0xF8000000, 0x01000000);
}

static void dump_rom_with_fullfat()
{
#if defined(CONFIG_600D) || defined(CONFIG_5D3)
    /* file I/O only known to work on these cameras */
    malloc_init((void *)0x42000000, 0x02000000);
    
    printf(" - Init SD/CF\n");
    sd_init(&sd_ctx);
    
    printf(" - Init FAT\n");
    FF_IOMAN *ioman = fat_init();

    printf(" - Dump ROM\n");
    dump_rom(ioman);
    
    printf(" - Umount FAT\n");
    fat_deinit(ioman);
#endif
}


/* timer interrupt variables */
volatile uint32_t timer_irq_count = 0;
volatile uint32_t timer_int_print_line = 0;
uint32_t timer_module = 2;
uint32_t timer_irq_id = 0x0A;


void timer_enable(uint32_t module)
{
    /* enable module clocks */
    MEM(0xC0400008) |= 0x400 << module;
    MEM(0xC0400010 | module * 4) = 0;
    MEM(0xC0203000) = 0x08;
}

void timer_reset(uint32_t module)
{
    /* set flag for initialization */
    MEM(0xC0210000 | module * 0x100 | 0x00) = 0x80000000;
    
    /* wait until module successfully initialized */
    while(MEM(0xC0210000 | module * 0x100 | 0x00) & 0x80000000)
    {
    }
}

void timer_start(uint32_t module, uint32_t target)
{
    /* enable module */
    MEM(0xC0210000 | module * 0x100 | 0x00) = 1;
    
    /* some init values and interrupts */
    MEM(0xC0210000 | module * 0x100 | 0x04) = 2;
    MEM(0xC0210000 | module * 0x100 | 0x14) = 3;
    MEM(0xC0210000 | module * 0x100 | 0x08) = target;
    MEM(0xC0210000 | module * 0x100 | 0x10) = 1;
}

uint32_t timer_get_value(uint32_t module)
{
    return MEM(0xC0210000 | module * 0x100 | 0x0C);
}

uint32_t timer_get_target(uint32_t module)
{
    return MEM(0xC0210000 | module * 0x100 | 0x08);
}

void print_irq()
{
    printf(" - IRQs: %06d (0x%08X 0x%08X)\n", timer_irq_count, timer_get_value(timer_module), timer_get_target(timer_module));
}


void int_enable(uint32_t id)
{
    MEM(0xC0201010) = id;
    volatile uint32_t status = MEM(0xC0201200);
}

void __attribute__((interrupt)) irq_handler()
{
    /* get interrupt id */
    uint32_t irq_id = MEM(0xC0201004) >> 2;
    
    /* check if timer counted up */
    if(irq_id == timer_irq_id)
    {
        timer_irq_count++;
    }
    
    /* reset/re-arm interrupt. works without, but firmware does so */
    int_enable(irq_id);
}

/*
 * from: https://github.com/hackndev/cocoboot/blob/master/arm/atag.c
 * Tag setup code and structures were originally from the GPL'd article:
 *
 * Booting ARM Linux
 * Copyright (C) 2004 Vincent Sanders
 * http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html
 *
 */
static void setup_core_tag(struct atag **params, uint32_t pagesize, uint32_t rootdev)
{
    (*params)->hdr.tag = ATAG_CORE; /* start with the core tag */
    (*params)->hdr.size = tag_size(atag_core); /* size the tag */
    (*params)->u.core.flags = 1; /* ensure read-only */
    (*params)->u.core.pagesize = pagesize;
    (*params)->u.core.rootdev = rootdev;
    
    *params = tag_next(*params); /* move pointer to next tag */
}

static void setup_ramdisk_tag(struct atag **params, uint32_t size)
{
    (*params)->hdr.tag = ATAG_RAMDISK; /* Ramdisk tag */
    (*params)->hdr.size = tag_size(atag_ramdisk); /* size tag */
    (*params)->u.ramdisk.flags = 0; /* Load the ramdisk */
    (*params)->u.ramdisk.size = size; /* Decompressed ramdisk size */
    (*params)->u.ramdisk.start = 0; /* Unused */
    *params = tag_next(*params); /* move pointer to next tag */
}

static void setup_initrd2_tag(struct atag **params, uint32_t start, uint32_t size)
{
	(*params)->hdr.tag = ATAG_INITRD2;         /* Initrd2 tag */
	(*params)->hdr.size = tag_size(atag_initrd2);  /* size tag */

	(*params)->u.initrd2.start = start;        /* physical start */
	(*params)->u.initrd2.size = size;          /* compressed ramdisk size */

	*params = tag_next(*params);              /* move pointer to next tag */
}

static void setup_mem_tag(struct atag **params, uint32_t start, uint32_t len)
{
	(*params)->hdr.tag = ATAG_MEM;             /* Memory tag */
	(*params)->hdr.size = tag_size(atag_mem);  /* size tag */

	(*params)->u.mem.start = start;            /* Start of memory area (physical address) */
	(*params)->u.mem.size = len;               /* Length of area */

	*params = tag_next(*params);              /* move pointer to next tag */
}

static void setup_cmdline_tag(struct atag **params, const char * line)
{
	int linelen = strlen(line);

	if(!linelen)
		return;                             /* do not insert a tag for an empty commandline */

	(*params)->hdr.tag = ATAG_CMDLINE;         /* Commandline tag */
	(*params)->hdr.size = (sizeof(struct atag_header) + linelen + 1 + 4) >> 2;

	strcpy((*params)->u.cmdline.cmdline,line); /* place commandline into tag */

	*params = tag_next(*params);              /* move pointer to next tag */
}

static void setup_end_tag(struct atag **params)
{
	(*params)->hdr.tag = ATAG_NONE;            /* Empty tag ends list */
	(*params)->hdr.size = 0;                   /* zero length */
}

void boot_linux()
{
    MEM(0x4080FFF4) = &print_char;
    MEM(0x4080FFFC) = &print_line_ext;
    
    struct atag *atags_ptr = (void*)0x4080F000;
    
    /* setup atags */  
    setup_core_tag(&atags_ptr, 4096, 0x100);
    setup_mem_tag(&atags_ptr, 0x41000000, 0x0EF20000);
    //setup_mem_tag(&atags_ptr, 0x50000000, 0x10000000);
    setup_ramdisk_tag(&atags_ptr, 4096);
    setup_cmdline_tag(&atags_ptr, "root=/dev/ram0 init=/bin/sh earlyprintk=1");
    setup_end_tag(&atags_ptr);
    
    asm(
        /* enter SVC mode */
        "mrs   r0, cpsr         \n"
        "bic   r0, r0, #0x1F    \n"
        "orr   r0, r0, #0xD3    \n"
        "msr   cpsr, r0         \n"
        
        /* setup registers for kernel boot */
        "ADR   R4,  parameters  \n"
        "MOV   R0,  #0x00       \n"
        "LDR   R1,  [R4, #0x00] \n"
        "LDR   R2,  [R4, #0x04] \n"
        "LDR   R3,  [R4, #0x08] \n"
        
        /* boot kernel */
        "BX    R3               \n"
        
        /* memory addresses */
        "parameters:\n"
        ".word   0xFFFFFFFF\n" /* machine ID */
        ".word   0x4080F000\n" /* ATAGs address */
        ".word   0x40810000\n" /* linux kernel address */
    );
}

void
__attribute__((noreturn))
cstart( void )
{
    /* install custom data abort handler */
    MEM(0x00000024) = (uint32_t)&_vec_data_abort;
    MEM(0x00000028) = (uint32_t)&_vec_data_abort;
    MEM(0x0000002C) = (uint32_t)&_vec_data_abort;
    MEM(0x00000030) = (uint32_t)&_vec_data_abort;
    MEM(0x00000038) = (uint32_t)&irq_handler;
    MEM(0x0000003C) = (uint32_t)&_vec_data_abort;
    
    /* allow interrupts */
    //sei(0);
    
    print_x = 0;
    print_y = 0;
    
    disp_init();
    boot_linux();
    
    print_line(COLOR_CYAN, 3, " Magic Lantern Rescue\n");
    print_line(COLOR_CYAN, 3, "----------------------------\n");

    printf(" - Booting linux...\n");
    
    /* allow timer interrupt to happen */
    int_enable(timer_irq_id);   
    timer_irq_count = 0; 

    /* enable module clocks, etc */
    timer_enable(timer_module);
    /* reset module */
    timer_reset(timer_module);
    /* start counting */
    timer_start(timer_module, 0xFFFF);

    
    timer_int_print_line = print_y;
    while(1)
    {
        print_y = timer_int_print_line;
        print_irq();
    }
    print_model();
    prop_diag();
    print_bootflags();
    find_gaonisoy();
    
    /* pick one method for dumping the ROM */
    dump_rom_with_canon_routines();
    //~ dump_rom_with_fullfat();
    
    printf(" - DONE!");
    while(1);
}

/** Include the relocatable shim code */
extern uint8_t blob_start;
extern uint8_t blob_end;

/* this will include the linux kernel at 0x40810000 */
asm(
    ".text\n"
    ".org 0x00010000\n"
    ".globl blob_start\n"
    "blob_start:\n"
    ".incbin \"../../xipImage\"\n"
    "blob_end:\n"
    ".globl blob_end\n"
);

