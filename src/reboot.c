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

/* various settings */
#undef PAGE_SCROLL
#define PRINTF_FONT_SIZE 2
#define PRINTF_FONT_COLOR COLOR_CYAN

#ifdef CONFIG_CPUINFO
#define PAGE_SCROLL
#undef  PRINTF_FONT_SIZE
#define PRINTF_FONT_SIZE 1
#undef  PRINTF_FONT_COLOR
#define PRINTF_FONT_COLOR COLOR_WHITE
#endif

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
#include "asm.h"

#define MEM(x) (*(volatile uint32_t *)(x))

/* we need this ASM block to be the first thing in the file */
#pragma GCC optimize ("-fno-reorder-functions")

asm(
    ".text\n"
    ".globl _start\n"
    "_start:\n"
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
    led_on();
    busy_wait(n);
    led_off();
    busy_wait(n);
}

static void fail()
{
    while (1)
    {
        blink(20);
    }
}

/* file I/O stubs */
static int (*boot_open_write)(int drive, char* filename, void* buf, uint32_t size) = 0;
static int (*boot_card_init)() = 0;

enum { DRIVE_CF, DRIVE_SD } boot_drive;

/* for intercepting Canon messages */
static int (*boot_putchar)(int ch) = 0;

static void save_file(int drive, char* filename, void* addr, int size)
{
    /* check whether our stubs were initialized */
    if (!boot_open_write)
    {
        fail();
    }

    if (boot_open_write(drive, filename, (void*) addr, size) == -1)
    {
        fail();
    }
}

static void dump_md5(int drive, char* filename, uint32_t addr, int size)
{
    uint8_t md5_bin[16];
    char md5_ascii[50];
    md5((void *) addr, size, md5_bin);
    snprintf(md5_ascii, sizeof(md5_ascii),
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x  %s\n",
        md5_bin[0],  md5_bin[1],  md5_bin[2],  md5_bin[3],
        md5_bin[4],  md5_bin[5],  md5_bin[6],  md5_bin[7],
        md5_bin[8],  md5_bin[9],  md5_bin[10], md5_bin[11],
        md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15],
        filename
    );
    char file_base[10];
    snprintf(file_base, sizeof(file_base), "%s", filename);
    file_base[strlen(file_base)-4] = 0;
    char md5file[10];
    snprintf(md5file, sizeof(md5file), "%s.MD5", file_base);
    md5_ascii[32] = 0;
    printf(" - MD5: %s\n", md5_ascii);
    md5_ascii[32] = ' ';
    
    save_file(drive, md5file, (void*) md5_ascii, strlen(md5_ascii));
}

static void boot_dump(int drive, char* filename, uint32_t addr, int size)
{
    /* turn on the LED */
    led_on();

    /* save the file and the MD5 checksum */
    save_file(drive, filename, (void*) addr, size);
    dump_md5(drive, filename, addr, size);

    /* turn off the LED */
    led_off();
}

struct sd_ctx sd_ctx;
uint32_t print_x = 0;
uint32_t print_y = 20;

void print_line(uint32_t color, uint32_t scale, char *txt)
{
    int height = scale * (FONTH + 2);
    
    if (print_y + height >= 480)
    {
#ifdef PAGE_SCROLL
        /* wait for user to read the current page */
        /* (no controls available; display a countdown instead) */
        /* note: adjust the timings if running with cache disabled */
        for (int i = 9; i > 0; i--)
        {
            uint32_t x = 710; uint32_t y = 0;
            char msg[] = "0"; msg[0] = '0' + i;
            font_draw(&x, &y, COLOR_WHITE, 1, (char*) &msg);
            busy_wait(100);
        }
        
        /* animation */
        while (print_y > 0)
        {
            disp_direct_scroll_up(10);
            print_y -= 10;
        }
        print_y = 0;
#else
        disp_direct_scroll_up(height);
        print_y -= height;
#endif
    }

    font_draw(&print_x, &print_y, color, scale, txt);
}

int printf(const char * fmt, ...)
{
    va_list            ap;
    char buf[128];
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf)-1, fmt, ap );
    va_end( ap );
    print_line(PRINTF_FONT_COLOR, PRINTF_FONT_SIZE, buf);
    return 0;
}

#ifdef CONFIG_FULLFAT
static void print_err(FF_ERROR err)
{
    char *message = (char*)FF_GetErrMessage(err);
    
    if(message)
    {
        print_line(COLOR_RED, 1, "   Error code:");
        print_line(COLOR_RED, 1, message);
    }
    else
    {
        char text[32];
        
        snprintf(text, 32, "   Error code: 0x%08X", err);
        print_line(COLOR_RED, 1, text);
    }
}

static unsigned long FF_blk_read(unsigned char *buffer, unsigned long sector, unsigned short count, void *priv)
{
    uint32_t ret = sd_read(&sd_ctx, sector, count, buffer);
    
    if(ret)
    {
        char text[64];
        
        snprintf(text, 64, "   SD read error: 0x%02X, sector: 0x%08X, count: %d", ret, sector, count);
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
        
        snprintf(text, 64, "   SD write error: 0x%02X, sector: 0x%08X, count: %d", ret, sector, count);
        print_line(COLOR_RED, 1, text);
    }
    
    return ret ? FF_ERR_DRIVER_FATAL_ERROR : 0;
}

static void halt_blink_code(uint32_t speed, uint32_t code)
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

static FF_IOMAN * fat_init()
{
    FF_IOMAN *ioman = NULL;
    FF_ERROR err = FF_ERR_NONE;
    
    ioman = FF_CreateIOMAN(NULL, 0x4000, 0x200, &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to init driver");
        print_err(err);
        halt_blink_code(10, 2);
    }
    
    FF_RegisterBlkDevice(ioman, 0x200, (FF_WRITE_BLOCKS) &FF_blk_write, (FF_READ_BLOCKS) &FF_blk_read, &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to register driver");
        print_err(err);
        halt_blink_code(10, 3);
    }
    
    err = FF_MountPartition(ioman, 0);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to mount partition");
        print_err(err);
        halt_blink_code(10, 4);
    }
    
    return ioman;
}

static void fat_deinit(FF_IOMAN *ioman)
{
    FF_ERROR err = FF_ERR_NONE;
    
    err = FF_UnmountPartition(ioman);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to unmount partition");
        print_err(err);
        halt_blink_code(10, 8);
    }
    
    err = FF_DestroyIOMAN(ioman);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to deinit driver");
        print_err(err);
        halt_blink_code(10, 9);
    }
}

static void dump_rom(FF_IOMAN *ioman)
{
    FF_ERROR err = FF_ERR_NONE;
    
    print_line(COLOR_CYAN, 1, "   Creating dump file");
    
    FF_FILE *file = FF_Open(ioman, "\\ROM.BIN", FF_GetModeBits("w"), &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to create dump file");
        print_err(err);
        halt_blink_code(10, 5);
    }
    
    print_line(COLOR_CYAN, 1, "   Dumping...");
    
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
            print_line(COLOR_RED, 1, "   Failed to write dump file");
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
        print_line(COLOR_RED, 1, "   Failed to close dump file");
        print_err(err);
        halt_blink_code(10, 7);
    }
    
    print_line(COLOR_CYAN, 1, "   Building checksum file");
    
    unsigned char md5_output[16];
    md5((void *)0xF8000000, 0x01000000, md5_output);
    
    file = FF_Open(ioman, "\\ROM.MD5", FF_GetModeBits("w"), &err);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to create MD5 file");
        print_err(err);
        halt_blink_code(10, 5);
    }
    
    err = FF_Write(file, 16, 1, md5_output);
    if(err <= 0)
    {
        print_line(COLOR_RED, 1, "   Failed to write MD5 file");
        print_err(err);
        FF_Close(file);
        FF_UnmountPartition(ioman);
        halt_blink_code(10, 6);
    }
    
    err = FF_Close(file);
    if(err)
    {
        print_line(COLOR_RED, 1, "   Failed to close MD5 file");
        print_err(err);
        halt_blink_code(10, 7);
    }
    
    print_line(COLOR_CYAN, 1, "   Finished, cleaning up");
    
    FF_UnmountPartition(ioman);
}
#endif

void malloc_init(void *ptr, uint32_t size);
void _vec_data_abort();
extern uint32_t _dat_data_abort;

static uint32_t data_abort_occurred()
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
    { 0x000, "ERROR" },
    { 0x213, "5D"    },
    { 0x218, "5D2"   },
    { 0x285, "5D3"   },
    { 0x382, "5DS"   },
    { 0x401, "5DSR"  },
    { 0x349, "5D4"   },
    { 0x302, "6D"    },
    { 0x250, "7D"    },
    { 0x289, "7D2"   },
    { 0x168, "10D"   },
    { 0x175, "20D"   },
    { 0x234, "30D"   },
    { 0x190, "40D"   },
    { 0x261, "50D"   },
    { 0x287, "60D"   },
    { 0x325, "70D"   },
    { 0x350, "80D"   },
    { 0x170, "300D"  },
    { 0x189, "350D"  },
    { 0x236, "400D"  },
    { 0x176, "450D"  },
    { 0x252, "500D"  },
    { 0x270, "550D"  },
    { 0x286, "600D"  },
    { 0x301, "650D"  },
    { 0x326, "700D"  },
    { 0x393, "750D"  },
    { 0x347, "760D"  },
    { 0x346, "100D"  },
    { 0x254, "1000D" },
    { 0x288, "1100D" },
    { 0x327, "1200D" },
    { 0x404, "1300D" },
    { 0x331, "EOSM"  },
};

uint32_t get_model_id()
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

    /* even newer models */
    model_ptr = (uint32_t *)0xFC060014;
    if(*model_ptr && *model_ptr < 0x00000FFF)
    {
        return *model_ptr;
    }

    return 0;
}

uint32_t is_digic6()
{
    return get_model_id() == *(uint32_t *)0xFC060014;
}

uint32_t is_vxworks()
{
    /* check for Wind (from Wind River Systems, Inc.) */
    return MEM(0xFF81003C) == 0x646E6957;
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
    printf(" - Model ID: 0x%X %s\n", get_model_id(), get_model_string());
}

/** Shadow copy of the NVRAM boot flags stored at 0xF8000000
 * (or 0xFC040000 on DIGIC 6) */
struct boot_flags
{
    uint32_t        firmware;   // 0x00
    uint32_t        bootdisk;   // 0x04
    uint32_t        ram_exe;    // 0x08
    uint32_t        update;     // 0x0c
};

static void print_bootflags()
{
    struct boot_flags * const boot_flags = (void*)(
        is_digic6() ? 0xFC040000 : 0xF8000000
    );
    
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

/* we replace boot_putchar with this one, to print Canon messages on the LCD */
static int my_putchar(int ch)
{
    /* Canon's puts assumes putchar will not change R3 */
    /* ARM calling convention says R3 is not preserved */
    /* workaround: push/pop it manually, and do the same for R1 and R2 just in case */

    asm("push {R1,R2,R3}");

    /* print each character */
    print_line(COLOR_WHITE, 1, (char*) &ch);
    
    asm("pop {R1,R2,R3}");

    return ch;
}

static void* find_boot_card_init()
{
    char * boot_card_strings[] = {
        "cf_ready (Not SD Detect High)\n",  /* most cameras */
        "cf_ready (Not CD Detect High)\n",  /* 5D2, 50D */
        "cf_ready (Not CF Detect High)\n",  /* 7D */
        "SD Detect High\n",                 /* 6D, 70D */
    };

    /* find a function matching one of those strings */
    /* fail if two matches */
    void* found = 0;
    for (int i = 0; i < COUNT(boot_card_strings); i++)
    {
        void* match = (void*) find_func_from_string(boot_card_strings[i], 0, 0x100);
        
        if (match)
        {
            if (found && found != match)
            {
                /* ambiguous match (matched two strings with different results) */
                printf("boot_card_init: found at %X and %X\n", found, match);
                return 0;
            }
            
            /* store this match */
            found = match;
        }
    }
    
    return found;
}

static void init_stubs()
{
    /* autodetect this one */
    boot_open_write = (void*) find_func_from_string("Open file for write : %s\n", 0, 0x50);
    boot_card_init = find_boot_card_init();
    
    const char* cam = get_model_string();

    if (strcmp(cam, "5D3") == 0)
    {
        /* from FFFF1738: routines from FFFE0000 to FFFEF408 are copied to 0x100000 */
        intptr_t RAM_OFFSET   =   0xFFFE0000 - 0x100000;
        boot_putchar    = (void*) 0xFFFEA8EC - RAM_OFFSET;
    }

    if (strcmp(cam, "60D") == 0)
    {
        /* from FFFF12EC: routines from FFFF2A3C to FFFFFBE4 are copied to 0x100000 */
        intptr_t RAM_OFFSET   =   0xFFFF2A3C - 0x100000;
        boot_putchar    = (void*) 0xFFFFB334 - RAM_OFFSET;
    }
    
    if (strcmp(cam, "700D") == 0)
    {
        /* from FFFF14E8: routines from FFFE0000 to FFFEFFB8 are copied to 0x100000 */
        intptr_t RAM_OFFSET   =   0xFFFE0000 - 0x100000;
        boot_putchar    = (void*) 0xFFFEB040 - RAM_OFFSET;
    }

    if (boot_putchar)
    {
        /* replace Canon's putchar with our own, to print their messages on the screen */
        MEM(boot_putchar) = B_INSTR(boot_putchar, &my_putchar);
    }
}

static void dump_rom_with_canon_routines()
{
    init_stubs();

    /* are we calling the right stubs? */
    
    if (!boot_open_write)
    {
        print_line(COLOR_RED, 2, " - Boot file write stub not set.\n");
        fail();
    }
    
    if (MEM(boot_open_write)  != 0xe92d47f0)
    {
        print_line(COLOR_RED, 2, " - Boot file write stub incorrect.\n");
        printf(" - Address: %X   Value: %X\n", boot_open_write, MEM(boot_open_write));
        fail();
    }

    if (!boot_card_init)
    {
        print_line(COLOR_YELLOW, 2, " - Card init stub not found.\n");
    }

    if ((((uint32_t)boot_open_write & 0xF0000000) == 0xF0000000) ||
        (((uint32_t)boot_card_init  & 0xF0000000) == 0xF0000000))
    {
        print_line(COLOR_YELLOW, 2, " - Boot file I/O stubs called from ROM.");
    }

    if (boot_card_init)
    {
        /* not all cameras need this, but some do */
        printf(" - Init SD... (%X)\n", boot_card_init);
        boot_card_init();
    }

    if (is_digic6())
    {
        printf(" - Dumping ROM1 (attempt 1)...\n");
        boot_dump(DRIVE_SD, "ROM1A.BIN", 0xFC000000, 0x02000000);
        printf(" - Dumping ROM1 (attempt 2)...\n");
        boot_dump(DRIVE_SD, "ROM1B.BIN", 0xFE000000, 0x02000000);
        printf(" - Dumping ROM1 (attempt 3)...\n");
        boot_dump(DRIVE_SD, "ROM1C.BIN", 0xFD000000, 0x02000000);
    }
    else
    {
        printf(" - Dumping ROM0 (attempt 1)...\n");
        boot_dump(DRIVE_SD, "ROM0A.BIN", 0xF0000000, 0x01000000);
        printf(" - Dumping ROM1 (attempt 1)...\n");
        boot_dump(DRIVE_SD, "ROM1A.BIN", 0xF8000000, 0x01000000);

        printf(" - Dumping ROM0 (attempt 2)...\n");
        boot_dump(DRIVE_SD, "ROM0B.BIN", 0xF0000000, 0x01000000);
        printf(" - Dumping ROM1 (attempt 2)...\n");
        boot_dump(DRIVE_SD, "ROM1B.BIN", 0xF8000000, 0x01000000);
    }
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

extern void cpuinfo_print(void);

void
__attribute__((noreturn))
cstart( void )
{
    /* install custom data abort handler */
    MEM(0x00000024) = (uint32_t)&_vec_data_abort;
    MEM(0x00000028) = (uint32_t)&_vec_data_abort;
    MEM(0x0000002C) = (uint32_t)&_vec_data_abort;
    MEM(0x00000030) = (uint32_t)&_vec_data_abort;
    MEM(0x00000038) = (uint32_t)&_vec_data_abort;
    MEM(0x0000003C) = (uint32_t)&_vec_data_abort;
    
    disp_init();

#ifdef CONFIG_CPUINFO
    printf("CHDK CPU info for 0x%X %s\n", get_model_id(), get_model_string());
    printf("------------------------------\n");
    cpuinfo_print();
#else
    /* disable caching (seems to interfere with ROM dumping) */
    //~ disable_dcache();
    //~ disable_icache();
    //~ disable_write_buffer();
    
    if (is_digic6())
    {
        disable_all_caches_d6();
    }
    else
    {
        disable_all_caches();
    }
    
    print_line(COLOR_CYAN, 3, "  Magic Lantern Rescue\n");
    print_line(COLOR_CYAN, 3, " ----------------------------\n");
    
    print_model();
    prop_diag();
    print_bootflags();
    find_gaonisoy();
    
    /* pick one method for dumping the ROM */
    dump_rom_with_canon_routines();
    //~ dump_rom_with_fullfat();
    
    printf(" - DONE!\n");
#endif

    printf("\n");
    print_line(COLOR_WHITE, 2, " You may now remove the battery.\n");
    
    while(1);
}
