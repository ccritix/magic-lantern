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

#if defined(CONFIG_BOOT_CPUINFO) && !defined(CONFIG_BOOT_DUMPER)
#define PAGE_SCROLL
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
#include "disp_dump.h"
#include "prop_diag.h"
#include "qemu-util.h"

#include "scnprintf.h"
#include "compiler.h"
#include "consts.h"
#include "fullfat.h"
#include "md5.h"
#include "asm.h"

/** Memory map
 * - available size: at least 64MB (1000D)
 * - Canon code is free to use anything from the RAM space
 * - we just hope a reasonably large area after 0x00800000 is unused
 * - 0x00000000 - 0x007FFFFF: area below AUTOEXEC.BIN (reserved for Canon routines)
 * - 0x00100000 - 0x00120000: Canon bootloader (128K)
 * - 0x00800000 - 0x01F00000: AUTOEXEC.BIN (max 24 MB; includes display buffers, serial flash buffer etc)
 * - 0x01F00000 - 0x02000000: our stack (1 MB)
 * - 0x02000000 - 0x04000000: our heap (32 MB)
 * - 0x10000000: uncacheable bit (VxWorks models)
 * - 0x40000000: uncacheable bit (all recent models)
 * - 0xC0000000 - 0xC1000000: MMIO (basic range)
 * - 0xC0000000 - 0xDFFFFFFF: MMIO (extended; some models have TCM at the top of this range)
 * - 0xE0000000 - 0xEFFFFFFF: ROM0, mirrored (DIGIC 7 & 8) [main ROM]
 * - 0xF0000000 - 0xFFFFFFFF: ROM1, mirrored (DIGIC 7 & 8) [optional, secondary ROM, very slow]
 * - 0xF0000000 - 0xF7FFFFFF: ROM0, mirrored (DIGIC <= 5)  [optional, secondary ROM]
 * - 0xF8000000 - 0xFFFFFFFF: ROM1, mirrored (DIGIC <= 5)  [main ROM]
 * - 0xFC000000 - 0xFFFFFFFF: ROM1, mirrored (DIGIC 6) [only one ROM?]
 */

#define MEM(x) (*(volatile uint32_t *)(x))

/* we need this ASM block to be the first thing in the file */
#pragma GCC optimize ("-fno-reorder-functions")

/* polyglot startup code that works if loaded as either ARM or Thumb */
asm(
    ".syntax unified\n"
    ".text\n"
    ".globl _start\n"
    "_start:\n"

    ".code 16\n"
    "NOP\n"                     /* as ARM, this is interpreted as: and r4, r11, r0, asr #13 (harmless) */
    "B loaded_as_thumb\n"       /* as Thumb, this jumps over the next ARM-specific block */

    ".code 32\n"
    "loaded_as_arm:\n"
    "MRS     R0, CPSR\n"
    "BIC     R0, R0, #0x3F\n"   // Clear I,F,T
    "ORR     R0, R0, #0xD3\n"   // Set I,T, M=10011 == supervisor
    "MSR     CPSR, R0\n"
    "MOV R0, #0\n"              // cstart(0) = loaded as ARM
    "LDR SP, _sp_addr\n"        // load stack pointer
    "LDR PC, _cstart_addr\n"    // long jump (into the uncacheable version, if linked that way)

    ".code 16\n"
    "loaded_as_thumb:\n"
    "MOVS R0, #1\n"             // cstart(1) = loaded as Thumb; MOV fails
    "LDR R1, _cstart_addr\n"    // long jump into ARM code (uncacheable, if linked that way)
    "LDR R2, _sp_addr\n"        // load stack pointer
    "MOV SP, R2\n"
    "BX R1\n"
    "NOP\n"
    "_cstart_addr:\n"
    ".word cstart\n"
    "_sp_addr:\n"
    ".word 0x02000000\n"        // see memory map

    /* return */
    ".code 32\n"
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

static int ml_loaded_as_thumb = 0;

/** Specified by the linker */
extern uint32_t __bss_start__[], __bss_end__[];

static inline void
zero_bss( void )
{
    qprintf("BSS %X - %X\n", __bss_start__, __bss_end__);
    if ((uint32_t)__bss_end__ > 0x01F00000)
    {
        qprintf("BSS overflow, please change the memory map.\n");
        while(1);
    }

    uint32_t *bss = __bss_start__;
    while( bss < __bss_end__ )
        *(bss++) = 0;
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
    { 0x406, "6D2"   },
    { 0x250, "7D"    },
    { 0x289, "7D2"   },
    { 0x168, "10D"   },
    { 0x175, "20D"   },
    { 0x234, "30D"   },
    { 0x190, "40D"   },
    { 0x261, "50D"   },
    { 0x287, "60D"   },
    { 0x325, "70D"   },
    { 0x408, "77D"   },
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
    { 0x405, "800D"  },
    { 0x346, "100D"  },
    { 0x417, "200D"  },
    { 0x254, "1000D" },
    { 0x288, "1100D" },
    { 0x327, "1200D" },
    { 0x404, "1300D" },
    { 0x432, "2000D" },
    { 0x422, "4000D" },
    { 0x331, "M"     },
    { 0x355, "M2"    },
    { 0x412, "M50"   },
    { 0x424, "R"     },
    { 0x805, "SX70"  },
    { 0x801, "SX740" },
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

    /* even newer models (DIGIC 6) */
    model_ptr = (uint32_t *)0xFC060014;
    if(*model_ptr && *model_ptr < 0x00000FFF)
    {
        return *model_ptr;
    }

    /* DIGIC 7 */
    model_ptr = (uint32_t *)0xE9FF9014;
    if(*model_ptr && *model_ptr < 0x00000FFF)
    {
        return *model_ptr;
    }

    /* DIGIC 4+ */
    model_ptr = (uint32_t *)0xF8001014;
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

uint32_t is_digic8()
{
    /* FIXME: generic method to tell apart DIGIC 8 from DIGIC 7 */
    switch (get_model_id())
    {
        case 0x412: /* M50 */
        case 0x424: /* R */
        case 0x801: /* SX740 HS */
        case 0x805: /* SX70 HS */
            return 1;

        default:
            return 0;
    }
}

uint32_t is_digic7()
{
    return is_digic8() ? 0 : ml_loaded_as_thumb;
}

uint32_t is_digic78()
{
    /* either DIGIC 7 or 8 */
    return ml_loaded_as_thumb;
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

static void busy_wait(int n)
{
    int i,j;
    static volatile int k = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 100000; j++)    /* 5000 with caching disabled, 100000 with it enabled */
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

/*------- cache routines ------------*/

extern void sync_caches_d6(void);
extern void disable_caches_region1_ram_d6(void);

static void sync_caches_portable()
{
    if (is_digic6() || is_digic78())
    {
        /* Cortex R4 (DIGIC 6) */
        /* Cortex A9 dual core (DIGIC 7/8) */
        /* will this be enough on DIGIC 7/8 when running on single core? */
        sync_caches_d6();
    }
    else
    {
        /* ARM946E-S */
        sync_caches();
    }
}
/*------- cache routines end --------*/

static char log_buffer[16384];
static unsigned int log_length = 0;

static int printf_font_size = 2;
static int printf_font_color = COLOR_CYAN;

static uint32_t print_x = 0;
static uint32_t print_y = 20;

void print_line(uint32_t color, uint32_t scale, char *txt)
{
    qprint(txt);

    /* copy the printed text into log buffer, except for backspaces */
    for (char * c = txt; *c && log_length < sizeof(log_buffer); c++)
    {
        switch (*c)
        {
            case '\b':
                if (log_length > 0 && log_buffer[log_length - 1] != '\n')
                {
                    log_buffer[--log_length] = '\0';
                }
                break;
            default:
                log_buffer[log_length++] = *c;
        }
    }

    int height = scale * (FONTH + 2);

    while (print_y + height >= disp_direct_get_yres())
    {
#ifdef PAGE_SCROLL
        /* wait for user to read the current page */
        /* (no controls available; display a countdown instead) */
        /* note: adjust the timings if running with cache disabled */
        for (int i = 9; i > 0; i--)
        {
            uint32_t x = disp_direct_get_xres() - 10; uint32_t y = 0;
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

    /* make sure the display DMA can pick the correct image */
    sync_caches_portable();
}

/* only for lines where '\n' was not yet printed */
static void clear_line()
{
    int max_chars = disp_direct_get_xres() / FONTW / printf_font_size - 1;
    for (int i = 0; i < max_chars; i++) {
        print_line(printf_font_color, printf_font_size, "\b");
    }
    for (int i = 0; i < max_chars; i++) {
        print_line(printf_font_color, printf_font_size, " ");
    }
    for (int i = 0; i < max_chars; i++) {
        print_line(printf_font_color, printf_font_size, "\b");
    }
}

int printf(const char * fmt, ...)
{
    va_list            ap;
    char buf[128];
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf)-1, fmt, ap );
    va_end( ap );
    print_line(printf_font_color, printf_font_size, buf);
    return 0;
}

static void fail()
{
    printf("\n");
    print_line(COLOR_RED, 2, " You may now remove the battery.\n");

    while (1)
    {
        blink(20);
    }
}

/* this is used with both FullFAT and Canon I/O routines */
static void * find_boot_card_init(int * ret_failed)
{
    *ret_failed = 0;
    char * boot_card_strings[] = {
        "cf_ready (Not SD Detect High)\n",  /* most cameras */
        "cf_ready (Not CD Detect High)\n",  /* 5D2, 50D */
        "cf_ready (Not CF Detect High)\n",  /* 7D */
        "SD Detect High\n",                 /* 6D, 70D */
    };

    uint32_t (*find_func_from_str)(const char *, uint32_t, uint32_t) = is_digic78() ? find_func_from_string_thumb : find_func_from_string;

    /* find a function matching one of those strings */
    /* fail if two matches */
    void* found = 0;
    for (int i = 0; i < COUNT(boot_card_strings); i++)
    {
        void* match = (void*) find_func_from_str(boot_card_strings[i], 0, 0x100);

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

    if (!found)
    {
        /* very old models (400D, 5D) */
        void * a = (void *) find_func_called_near_string_ref("cf_dir (cfata_init error)\n", 0, -32);
        void * b = (void *) find_func_called_near_string_ref("cf_read_dma (cfata_init error)\n", 0, -32);
        if (a == b)
        {
            *ret_failed = -1;
            return a;
        }
    }

    return found;
}

static void patch_init_card(uint32_t card_init_addr, int max_offset)
{
    /* some old models use a flag that prevents the card init routines from running twice */
    /* however, the firmware forgets to reset that flag on card deinit */
    /* result: after loading our code, Canon bootloader turns off the card,
     * and we cannot turn it back on just by calling the init function */
    /* let's try to reset that flag */
    for (int i = 0; i < max_offset; i++)
    {
        uint32_t addr = card_init_addr + i * 4;
        if ((MEM(addr) & 0xFFFCFFFF) == 0xE3500001)
        {
            /* CMP R[0..3], #1 -> change it into CMP Rn, #0 */
            printf(" - Patching %X from %x", addr, MEM(addr));
            MEM(addr) &= ~1;
            printf(" to %x\n", MEM(addr));
        }
    }
}

static void init_card()
{
    int card_init_failed = 0;
    int (*boot_card_init)(void) = find_boot_card_init(&card_init_failed);

    /* some models have a low-level routine called just before it */
    /* the procedure that finds it is not very robust and may come up with false positives */
    /* let's whitelist it on models known to require this */
    const char * cam = get_model_string();
    if (strcmp(cam, "5D2") == 0 ||
        strcmp(cam, "50D") == 0 ||
        strcmp(cam, "40D") == 0 ||
        strcmp(cam, "7D") == 0)
    {
        uint32_t call_addr = find_call_address(0x100000, 0x10000, (uint32_t)boot_card_init);
        int (*boot_card_init_ll)(void) = (void *) find_func_call(call_addr - 4, 4, 0, 0, 0, 0, NULL);

        /* double-check */
        uint32_t call_addr2 = find_call_address(call_addr + 4, 0x10000, (uint32_t)boot_card_init);
        int (*boot_card_init_ll2)(void) = (void *) find_func_call(call_addr2 - 4, 4, 0, 0, 0, 0, NULL);

        if (boot_card_init_ll && boot_card_init_ll == boot_card_init_ll2)
        {
            /* this one needs patching for sure, otherwise it does nothing */
            patch_init_card((uint32_t)boot_card_init_ll, 20);
            printf(" - %X Card low-level init...", boot_card_init_ll);
            int ans = boot_card_init_ll();
            printf("\b\b\b => %X\n", ans);
        }
    }

    if (boot_card_init)
    {
        /* not all cameras need this, but some do */
        printf(" - %X Card init...", boot_card_init);
        int ans = boot_card_init();
        printf("\b\b\b => %X\n", ans);
        if (ans == card_init_failed)
        {
            /* didn't seem to work */
            patch_init_card((uint32_t)boot_card_init, 10);
            printf(" - %X Card init #2...", boot_card_init);
            ans = boot_card_init();
            printf("\b\b\b => %X\n", ans);
        }
        if (ans == card_init_failed)
        {
            print_line(COLOR_YELLOW, 2, " - Card init error?\n");
            fail();
        }
    }
    else
    {
        print_line(COLOR_RED, 2, " - Card init stub not found.\n");
        fail();
    }
}

#ifdef CONFIG_BOOT_FULLFAT
/* using some external FAT library (FullFAT) */
/* we need to provide sector-level I/O routines */
/* this method plays nice with large cards, caches etc */

/* low-level I/O stubs */
static int (*boot_read_sector)(uint32_t sector_address, uint32_t num_sectors, void * buffer) = 0;
static int (*boot_write_sector)(uint32_t sector_address, uint32_t num_sectors, void * buffer) = 0;

/* some models use these instead */
static int (*boot_read_sector_4)(uint32_t drive, uint32_t sector_address, uint32_t num_sectors, void * buffer) = 0;
static int (*boot_write_sector_4)(uint32_t drive, uint32_t sector_address, uint32_t num_sectors, void * buffer) = 0;

/* drive is ignored on most models, but not all */
static int boot_drive = 1;

static int boot_read_sector_3to4(uint32_t sector_address, uint32_t num_sectors, void * buffer)
{
    return boot_read_sector_4(boot_drive, sector_address, num_sectors, buffer);
}

static int boot_write_sector_3to4(uint32_t sector_address, uint32_t num_sectors, void * buffer)
{
    return boot_write_sector_4(boot_drive, sector_address, num_sectors, buffer);
}

extern void * memmem(const void * haystack, size_t haystacklen, const void * needle, size_t needle_len);

static void init_sector_io_stubs()
{
    /* DIGIC 7 & 5D4 */
    uint32_t (*find_func_from_str)(const char *, uint32_t, uint32_t) = is_digic78() ? find_func_from_string_thumb : find_func_from_string;
    void (*get_read_sector)(int drive, void * result) = (void *) find_func_from_str("Read_Sector drive=%d FS1=%d FS2=%d FS3=%d\n", 0, 64);
    void (*get_write_sector)(int drive, void * result) = (void *) find_func_from_str("Write_Sector drive=%d FS1=%d FS2=%d FS3=%d\n", 0, 64);
    if (get_read_sector && get_write_sector)
    {
        get_read_sector(1, &boot_read_sector);
        get_write_sector(1, &boot_write_sector);
        printf(" - boot_read/write_sector %x %x\n", boot_read_sector, boot_write_sector);
        return;
    }

    if (!is_digic78())
    {
        void (*card_bootflags1)(int mode) = (void *) find_func_from_string("EOS_DEVELOP", 1, 1024);
        void (*card_bootflags2)(int mode) = (void *) find_func_from_string("BOOTDISK", 1, 1024);
        if (card_bootflags1 && card_bootflags1 == card_bootflags2)
        {
            printf(" - card_bootflags %x\n", card_bootflags1);

            /* some old models use 3 arguments for read/write_sector */
            /* if this is the case, first call to read_sector is preceded by "MOV R0, #0" (i.e. read sector 0) */
            uint32_t read_sector = find_func_call((uint32_t) card_bootflags1, 512, 0, 0, 0xE3A00000, 0, NULL);

            /* the call to write_sector is followed by "CMP R0, #0" */
            uint32_t bootdisk_ref = find_string_ref("BOOTDISK");
            if (bootdisk_ref < (uint32_t) card_bootflags1) fail();
            uint32_t write_sector = find_func_call(bootdisk_ref, 512, 0, 0, 0, 0xE3500000, NULL);

            if (read_sector)
            {
                /* using 3 arguments */
                boot_read_sector  = (void *) read_sector;
                boot_write_sector = (void *) write_sector;
            }
            else
            {
                /* using 4 arguments */
                /* this is called somewhere at the beginning of card_bootflags */
                /* first call would have the second argument set to 0 (i.e. read sector 0) */
                /* and the body of the function is located right before write_sector in memory */
                for (int i = 0; i < 5; i++)
                {
                    uint32_t call_address = 0;
                    read_sector = find_func_call((uint32_t) card_bootflags1, 512, i, 0, 0, 0, &call_address);

                    if (read_sector < write_sector &&               /* function body before write_sector */
                        read_sector > write_sector - 1024 &&        /* but not too far from it */
                        (MEM(call_address - 4) == 0xE3A01000 ||     /* MOV R1, #0 (read first sector) */
                         MEM(call_address - 8) == 0xE3A01000))      /* it might be right before the function, or a little earlier */
                    {
                        /* let's hope this is the one */
                        break;
                    }
                }

                boot_read_sector_4  = (void *) read_sector;
                boot_write_sector_4 = (void *) write_sector;
                boot_read_sector    = boot_read_sector_3to4;
                boot_write_sector   = boot_write_sector_3to4;
            }

            printf(" - boot_read/write_sector %x %x\n", read_sector, write_sector);

            /* some consistency checks */
            if (read_sector == write_sector) fail();
            if (read_sector  < 0x100000 || read_sector  > 0x110000) fail();
            if (write_sector < 0x100000 || write_sector > 0x110000) fail();
            if (write_sector < read_sector) fail();
            if (write_sector > read_sector + 1024) fail();

            if (strcmp(get_model_string(), "7D2") == 0)
            {
                /* https://www.magiclantern.fm/forum/index.php?topic=13746.msg205531#msg205531 */
                /* 1 = CF (not working); 2 = SD; 0 also works, why? */
                boot_drive = 2;
            }

            return;
        }
    }

    if (is_digic8())
    {
        /* M50, SX70 */
        uint16_t boot_rw_sector[] = { 0xB538, 0x4614, 0x460B, 0x2100, 0x4602, 0x9100, 0x2000, 0x4621 };

        printf(" - boot_read_sector ");
        boot_read_sector = memmem((void *) 0x100000, 0x10000, boot_rw_sector, 16);
        printf("%x\n", boot_read_sector);
        if (!boot_read_sector) fail();
        uint32_t insn = MEM((void *)boot_read_sector + 16);
        int is_bl = ((insn & 0xF800FC00) == 0xF800F000);
        if (!is_bl) fail();

        printf(" - boot_write_sector ");
        boot_write_sector = memmem((void *) boot_read_sector + 16, 32, boot_rw_sector, 16);
        printf("%x\n", boot_write_sector);
        if (!boot_read_sector) fail();

        uint32_t i = (uint32_t) boot_write_sector + 16;
        insn = MEM(i);
        is_bl = ((insn & 0xF800FC00) == 0xF800F000);
        if (!is_bl) fail();

        uint32_t pc = (i + 4);
        uint32_t imm10 = insn & 0x3FF;
        uint32_t imm11 = (insn >> 16) & 0x7FF;
        uint32_t func_offset = (imm11 << 1) | (imm10 << 12);
        uint32_t func_addr = pc + func_offset;
        printf(" - %x: BL %x\n", i, func_addr);
        if (func_addr < 0x100000 || func_addr > 0x110000) fail();

        /* add the Thumb bit */
        boot_read_sector  = (void *)((uintptr_t) boot_read_sector  | 1);
        boot_write_sector = (void *)((uintptr_t) boot_write_sector | 1);
    }
}

static void check_sector_io_stubs()
{
    /* check whether our stubs were initialized */
    if (!boot_read_sector)
    {
        print_line(COLOR_RED, 2, " - Boot read sector stub not set.\n");
        fail();
    }

    if (!boot_write_sector)
    {
        print_line(COLOR_RED, 2, " - Boot write sector stub not set.\n");
        fail();
    }
}

static void print_err(FF_ERROR err, char * our_msg)
{
    print_line(COLOR_RED, 2, "\n - ");
    print_line(COLOR_RED, 2, our_msg);
    print_line(COLOR_RED, 2, "\n   ");

    char *message = (char*)FF_GetErrMessage(err);

    if(message)
    {
        print_line(COLOR_RED, 2, message);
    }
    else
    {
        char text[32];
        snprintf(text, 32, "   Error code: 0x%08X", err);
        print_line(COLOR_RED, 2, text);
    }
    printf("\n");
}

static unsigned long FF_blk_read(unsigned char *buffer, unsigned long sector, unsigned short count, void *priv)
{
    /* FIXME: not sure what to do here, but this appears to work */
    sync_caches_portable();

    //printf(" - RD %08X %4X %08X", sector, count, buffer);
    uint32_t ret = boot_read_sector(sector, count, buffer);
    //printf(" => %X\n", ret);

    if (ret)
    {
        char text[64];
        snprintf(text, sizeof(text), "   SD read error: %d, sector: 0x%08X, count: %d\n", ret, sector, count);
        print_line(COLOR_RED, 1, text);
        return FF_ERR_DRIVER_FATAL_ERROR;
    }

    return 0;
}

static unsigned long FF_blk_write(unsigned char *buffer, unsigned long sector, unsigned short count, void *priv)
{
    /* probably overkill, but... */
    sync_caches_portable();

    //printf(" - WR %08X %4X %08X", sector, count, buffer);
    uint32_t ret = boot_write_sector(sector, count, buffer);
    //printf(" => %X\n", ret);

    if (ret)
    {
        char text[64];
        snprintf(text, sizeof(text), "   SD write error: %d, sector: 0x%08X, count: %d\n", ret, sector, count);
        print_line(COLOR_RED, 1, text);
        return FF_ERR_DRIVER_FATAL_ERROR;
    }

    return 0;
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
        print_err(err, "Failed to init driver");
        halt_blink_code(10, 2);
    }

    FF_RegisterBlkDevice(ioman, 0x200, (FF_WRITE_BLOCKS) &FF_blk_write, (FF_READ_BLOCKS) &FF_blk_read, &err);
    if(err)
    {
        print_err(err, "Failed to register driver");
        halt_blink_code(10, 3);
    }

    err = FF_MountPartition(ioman, 0);
    if(err)
    {
        print_err(err, "Failed to mount partition");
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
        print_err(err, "Failed to unmount partition");
        halt_blink_code(10, 8);
    }

    err = FF_DestroyIOMAN(ioman);
    if(err)
    {
        print_err(err, "Failed to deinit driver");
        halt_blink_code(10, 9);
    }
}

/* previous line should be printed without \n, for the progress bar */
/* this routine will add a newline on its own */
static void save_file(const char * filename, void * addr, int size)
{
    FF_IOMAN *ioman = fat_init();
    FF_ERROR err = FF_ERR_NONE;

    char filename_ex[32];
    snprintf(filename_ex, sizeof(filename_ex), "\\%s", filename);

    FF_FILE *file = FF_Open(ioman, filename_ex, FF_GetModeBits("w"), &err);
    if(err)
    {
        print_err(err, "Failed to create dump file");
        fat_deinit(ioman);
        halt_blink_code(10, 5);
    }

    uint32_t block_size = 0x10000;
    while (size % block_size) block_size--;
    uint32_t block_count = size / block_size;

    if (block_count > 1)
    {
        /* are we going to print a progress indicator? */
        printf("    ");
    }

    for(uint32_t block = 0; block < block_count; block++)
    {
        err = FF_Write(file, block_size, 1, (FF_T_UINT8 *) (addr + block * block_size));
        if(err <= 0)
        {
            print_err(err, "Failed to write dump file");
            FF_Close(file);
            fat_deinit(ioman);
            halt_blink_code(10, 6);
        }
        if (block_count > 1)
        {
            /* 100% will be printed just once */
            uint32_t progress = (block + 1) * 100 / block_count;
            printf("\b\b\b%2d%%", progress);
        }
    }

    err = FF_Close(file);
    if(err)
    {
        print_err(err, "Failed to close dump file");
        fat_deinit(ioman);
        halt_blink_code(10, 7);
    }

    //print_line(COLOR_CYAN, 1, "   Finished, cleaning up");
    fat_deinit(ioman);
    printf("\n");
}

/* this function should be repeated a couple of times */
/* to see the cache coherency issues, run this test with caches enabled, 
 * but comment out the contents of sync_caches_portable() */
static void cache_file_read_test_do(FF_IOMAN *ioman)
{
    FF_ERROR err = FF_ERR_NONE;

    /* cacheable buffer that will fit the entire cache */
    /* placed at different offsets, to increase the likelihood of cache coherency issues */
    /* this will make the data in cache very different from what we are going to read from card */
    static uint8_t buf[8192 * 2];               /* will slide the actual buffer through this one */
    const size_t size = sizeof(buf) / 2;        /* only use half of this buffer for actual reads */
    static int off = 0; off += 4;               /* unaligned buffers may work, too */
    if (off + size >= sizeof(buf)) off = 0;     /* wrap around, if many tests are requested */

    /* read a small part from our own code and check its MD5 */
    FF_FILE *file = FF_Open(ioman, "\\AUTOEXEC.BIN", FF_GetModeBits("r"), &err);
    if(err)
    {
        print_err(err, "Failed to open file");
        fat_deinit(ioman);
        halt_blink_code(10, 5);
    }

    err = FF_Read(file, size, 1, (FF_T_UINT8 *) buf + off);
    if(err <= 0)
    {
        print_err(err, "Failed to read file");
        FF_Close(file);
        fat_deinit(ioman);
        halt_blink_code(10, 6);
    }

    err = FF_Close(file);

    /* just check visually whether the sums are the same */
    uint8_t md5_bin[16];
    char md5_ascii[50];
    printf("%8x:", buf + off);
    md5(buf + off, size, md5_bin);
    snprintf(md5_ascii, sizeof(md5_ascii),
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
        md5_bin[0],  md5_bin[1],  md5_bin[2],  md5_bin[3],
        md5_bin[4],  md5_bin[5],  md5_bin[6],  md5_bin[7],
        md5_bin[8],  md5_bin[9],  md5_bin[10], md5_bin[11],
        md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15]
    );
    printf("%s", md5_ascii);
}

static void cache_file_read_test()
{
    FF_IOMAN *ioman = fat_init();
    for (int i = 0; i < 50; i++)
    {
        cache_file_read_test_do(ioman);
    }
    fat_deinit(ioman);
}

static void init_file_io()
{
    /* run only once */
    static int first_time = 1;
    if (!first_time) return;
    first_time = 0;

    /* find Canon stubs */
    init_sector_io_stubs();
    check_sector_io_stubs();

    /* FullFAT will require malloc */
    extern void malloc_init(void *ptr, uint32_t size);
    malloc_init((void *)0x02000000, 0x02000000);

    /* call Canon's card initialization routine */
    init_card();

    /* FAT library will be initialized on the fly,
     * for each file, to keep things simple */

#if 0
    /* check whether we are syncing the caches properly */
    /* see also stub_test_cache_fio in selftest.c */
    cache_file_read_test();
#endif
}

#else

/* some models have file I/O routines in the bootloader */
/* they kinda work, but generally they are very buggy:
 * - they are writing from cacheable memory, without syncing the caches first
 * - they will *destroy* the filesystem on large cards
 */

/* file I/O stub */
enum boot_drive { DRIVE_CF, DRIVE_SD };
static int (*boot_open_write)(int drive, const char * filename, void * addr, uint32_t size) = 0;

/* optional, for intercepting Canon messages */
static int (*boot_putchar)(int ch) = 0;

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

static void init_boot_file_io_stubs()
{
    /* autodetect this one */
    uint32_t (*find_func_from_str)(const char *, uint32_t, uint32_t) = is_digic78() ? find_func_from_string_thumb : find_func_from_string;
    boot_open_write = (void*) find_func_from_str("Open file for write : %s\n", 0, 0x50);

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

static void check_boot_file_io_stubs()
{
    /* are we calling the right stubs? */

    if (!boot_open_write)
    {
        print_line(COLOR_RED, 2, " - Boot file write stub not set.\n");
        fail();
    }

    uint32_t boot_open_write_addr = ((uint32_t) boot_open_write) & ~1;
    uint32_t is_thumb = ((uint32_t) boot_open_write) & 1;
    uint32_t expected_insn = (is_thumb) ? 0x47f0e92d : 0xe92d47f0;
    printf(" - Open for write %X %X\n", boot_open_write, MEM(boot_open_write_addr));

    if (MEM(boot_open_write_addr) != expected_insn)
    {
        print_line(COLOR_RED, 2, " - Boot file write stub incorrect.\n");
        printf(" - Address: %X   Value: %X\n", boot_open_write, MEM(boot_open_write));
        fail();
    }
}

static void init_file_io()
{
    /* run only once */
    static int first_time = 1;
    if (!first_time) return;
    first_time = 0;

    init_boot_file_io_stubs();
    check_boot_file_io_stubs();
    init_card();
}

/* previous line should be printed without \n, for compatibility with the FULLFAT version */
/* this routine will add a newline on its own */
static void save_file(const char * filename, void * addr, int size)
{
    int drive = DRIVE_SD;

    if (strcmp(get_model_string(), "7D2") == 0)
    {
        /* https://www.magiclantern.fm/forum/index.php?topic=13746.msg205531#msg205531 */
        /* 1 = CF (not working); 2 = SD; 0 also works, why? */
        drive = 2;
    }

    /* check whether our stubs were initialized */
    if (!boot_open_write)
    {
        printf("\n");
        print_line(COLOR_RED, 2, " - Boot file write stub not set.\n");
        fail();
    }

    if (boot_open_write(drive, filename, (void*) addr, size) == -1)
    {
        printf("\n");
        print_line(COLOR_RED, 2, " - Boot file write error.\n");
        fail();
    }

    printf("\n");
}

#endif  /* Canon bootloader I/O */

static void dump_md5(const char * filename, void * addr, int size)
{
    uint8_t md5_bin[16];
    char md5_ascii[50];
    printf(" - MD5: ");
    md5(addr, size, md5_bin);
    snprintf(md5_ascii, sizeof(md5_ascii),
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x  %s\n",
        md5_bin[0],  md5_bin[1],  md5_bin[2],  md5_bin[3],
        md5_bin[4],  md5_bin[5],  md5_bin[6],  md5_bin[7],
        md5_bin[8],  md5_bin[9],  md5_bin[10], md5_bin[11],
        md5_bin[12], md5_bin[13], md5_bin[14], md5_bin[15],
        filename
    );
    char file_base[16];
    snprintf(file_base, sizeof(file_base), "%s", filename);
    file_base[strlen(file_base)-4] = 0;
    char md5file[16];
    snprintf(md5file, sizeof(md5file), "%s.MD5", file_base);
    md5_ascii[32] = 0;
    printf("%s", md5_ascii);   /* newline printed by save_file */
    md5_ascii[32] = ' ';

    save_file(md5file, (void*) md5_ascii, strlen(md5_ascii));
}

/* previous line should be without newline */
static void boot_dump(char* filename, uint32_t addr, int size)
{
    /* turn on the LED */
    led_on();

    /* save the file and the MD5 checksum */
    save_file(filename, (void *) addr, size);
    dump_md5(filename, (void *) addr, size);

    /* turn off the LED */
    led_off();
}

extern void _vec_data_abort();
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

static int check_read(const void * a)
{
    uint32_t x1 = MEM(a);
    uint32_t y1 = MEM(a + 4);
    uint32_t x2 = MEM(a);
    uint32_t y2 = MEM(a + 4);
    if (x1 != x2 || y1 != y2)
    {
        clear_line();
        printf(" - %08X: inconsistent!\n", a);
        return 0;
    }
    return 1;
}

static int memcmp64(const void * a, const void * b, int n)
{
    if (n % 8)
    {
        return memcmp(a, b, n);
    }

    for (int i = 0; i < n / 8; i++)
    {
        if (((uint64_t *)a)[i] != ((uint64_t *)b)[i])
        {
            return 1;
        }
    }

    return 0;
}

/* memcmp with progress indicator */
int memcmp_v(const void * a, const void * b, int n, int n0)
{
    const int block_size = 0x100000;

    if (n > block_size)
    {
        static char progress[] = "|/-\\";
        static int status = 0;
        int p = (uint64_t) (n0 - n) * 100ULL / n0;
        printf("\b\b\b\b\b\b%3d%% %c", p, progress[(status++) & 3]);
        if (memcmp64(a, b, block_size) == 0)
        {
            /* this gets optimized as tail call :) */
            return memcmp_v(a + block_size, b + block_size, n - block_size, n0);
        }
    }

    return memcmp64(a, b, n);
}

static int repeated_byte(uint32_t * buf, int size, int full_size)
{
    if (size % 4)
    {
        /* should not happen */
        while(1);
    }

    uint32_t dword = buf[0];
    uint32_t rbyte = dword & 0xFF;
    rbyte |= (rbyte << 8);
    rbyte |= (rbyte << 16);
    if (dword != rbyte)
    {
        return 0;
    }

    clear_line();
    printf(" - %08X-%08X: %08X REP? ...    ", buf, buf + full_size - 1, size);

    for (int i = 0; i < size / 4; i++)
    {
        if (buf[i] != rbyte)
        {
            return 0;
        }
    }

    return 1;
}

static int check_rom_mirroring(void * buf, int size, int full_size)
{
    clear_line();
    printf(" - %08X-%08X: %08X...      ", buf, buf + full_size - 1, size);

    if (size / 2 == 0)
    {
        return 0;
    }

    if (!check_read(buf))
    {
        if (size / 2 >= 0x1000)
        {
            /* try the upper half, hopefully that's valid */
            return check_rom_mirroring(buf + size / 2, size / 2, size / 2);
        }
        else
        {
            /* nevermind */
            return 0;
        }
    }

    if (!check_read(buf + size / 2))
    {
        /* unlikely */
        return 0;
    }

    if (memcmp_v(buf, buf + size / 2, size / 2, size / 2) == 0)
    {
        /* identical halves? */

        /* maybe same byte repeated over and over?
         * check this particular case now, as otherwise it would use a lot of stack space,
         * possibly overflowing the stack on certain models */
        if (size % 4 == 0 && repeated_byte(buf, size, full_size))
        {
            int byte = MEM(buf) & 0xFF;
            clear_line();
            printf(" - %08X-%08X: byte 0x%X x 0x%X\n", buf, buf + full_size - 1, byte, size);
        }
        else
        {
            /* check recursively to find the lowest size with unique data */
            if (!check_rom_mirroring(buf, size / 2, full_size))
            {
                clear_line();
                printf(" - %08X-%08X: uniq 0x%X x 0x%X\n", buf, buf + full_size - 1, size / 2, full_size / (size / 2));
            }
        }
        return 1;
    }
    else
    {
        /* different halves, let's check them */
        if (size / 2 >= 0x100000)
        {
            check_rom_mirroring(buf, size / 2, size / 2);
            check_rom_mirroring(buf + size / 2, size / 2, size / 2);
        }
        return 0;
    }
}

static void print_rom_layout()
{
    if (0)
    {
        /* useful as initial diagnostic, or if it locks up */
        uint32_t start_addrs[] = { 0xFF800000, 0xFF000000, 0xFE000000, 0xFC000000, 0xF8000000, 0xF7000000, 0xF0000000, 0xE0000000, 0xE8000000 };
        for (int i = 0; i < COUNT(start_addrs); i++)
        {
            uint32_t a = start_addrs[i];

            /* print just the address, in case it locks up */
            printf(" - %08X-%08X: ", a, a + 4);

            if (1)
            {
                uint32_t x1 = MEM(a);
                uint32_t y1 = MEM(a + 4);
                uint32_t x2 = MEM(a);
                uint32_t y2 = MEM(a + 4);
                if (x1 != x2 || y1 != y2)
                {
                    printf("inconsistent!\n");
                }
                else
                {
                    printf("%08X %08X\n", x1, y1);
                }
            }

            if (0)
            {
                /* split this, just in case it locks up */
                printf("%08X %08X %08X\n", MEM(a), MEM(a + 4), MEM(a));

                /* let's try a larger size */
                for (int size = 2; size < 0x10000000; size *= 2)
                {
                    printf(" - %08X-%08X: ", a, a + size - 1);
                    printf("%d (%x,%x)\n", memcmp((void *) a, (void *) a + size/2, size/2), MEM(a), MEM(a + size/2));
                }
            }
        }
    }

    /* is this generic enough? will it work on all models without locking up? */
    /* DIGIC 4: reading 0xE0000000 usually (but not always!) returns the same byte repeated over and over */
    /* DIGIC 5: reading 0xE0000000 is very slow and gives different values every time */
    /*          it appears to read back some of the address bits */
    /* DIGIC 6: locks up when reading 0xE0000000 or 0xEE000000, though bootloader configures the latter */
    /* DIGIC 7: the main ROM is at 0xE0000000 and there is another one at 0xF0000000, so it should be OK */

    if (is_digic78())
    {
        check_rom_mirroring((void *) 0xE0000000, 0x20000000, 0x20000000);
    }
    else
    {
        check_rom_mirroring((void *) 0xF0000000, 0x10000000, 0x10000000);
    }
}

static uint32_t find_firmware_start()
{
    printf(" - ROMBASEADDR...           ");

    if (is_digic78())
    {
        for (uint32_t start = 0xE0000000; start < 0xF0000000; start += 0x10000)
        {
            printf("\b\b\b\b\b\b\b\b\b\b0x%08X", start);

            /* DIGIC 7: MCR p15, 0, R0,c12,c0, 0 (set VBAR - Vector Base Address Register) */
            if ((MEM(start + 0) >> 16)    == 0xEE0C &&
                (MEM(start + 4) & 0xFFFF) == 0x0F10)
            {
                return start;
            }
        }
    }

    for (uint32_t start = 0xFFFF0000; start >= 0xE0000000; start -= 0x10000)
    {
        printf("\b\b\b\b\b\b\b\b\b\b0x%08X", start);

        /* DIGIC 2/3: Wind River Systems */
        if (MEM(start + 0x3C) == 0x646E6957 && MEM(start + 0x40) == 0x76695220)
        {
            return start;
        }

        /* DIGIC 4/5: gaonisoy */
        if (MEM(start + 4) == 0x6e6f6167 && MEM(start + 8) == 0x796f7369)
        {
            return start;
        }

        /* DIGIC 6: add r0, pc, #4; orr r0, r0, #1; bx r0 */
        if (MEM(start + 0) == 0xE28F0004 &&
            MEM(start + 4) == 0xE3800001 &&
            MEM(start + 8) == 0xE12FFF10)
        {
            return start;
        }
    }

    return 0;
}

static void print_firmware_start()
{
    uint32_t start = find_firmware_start();

    clear_line();

    if (start)
    {
        printf(" - ROMBASEADDR: 0x%08X\n", start);
    }
    else
    {
        printf(" - ROMBASEADDR: not found!\n");
    }
}

/** Shadow copy of the NVRAM boot flags stored at 0xF8000000
 * (0xFC040000 on DIGIC 6, 0xE1FF8000 on DIGIC 7 ) */
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
        is_digic6()  ? 0xFC040000 :
        is_digic78() ? 0xE1FF8000 :
                       0xF8000000
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
#ifdef CONFIG_BOOT_SROM_DUMPER
static void (*sf_init)(void) = NULL;
static void (*sf_command_sio)(uint32_t command[], void * out_buffer, int out_buffer_size, int toggle_cs) = NULL;

/* Canon's serial flash routine outputs bytes in a 32-byte array */
/* use this wrapper to write them to a "simple" buffer in memory */
static void sf_read(uint32_t addr, uint8_t * buf, int size)
{
    if (!sf_command_sio)
    {
        return;
    }

    /* apparently we can't perform large reads without DMA */
    /* "You can use several loads with a wheelbarrow, many loads with a bucket, or lots and lots of loads with a spoon" (Absolute FreeBSD, 2nd Ed.) */
    static uint32_t teaspoon[0x100];

    for (int i = 0; i < size; i++)
    {
        if (i % COUNT(teaspoon) == 0)
        {
            uint32_t a = addr + i;
            sf_command_sio((uint32_t[]) {3, (a >> 16) & 0xFF, (a >> 8) & 0xFF, a & 0xFF, -1}, teaspoon, COUNT(teaspoon), 1);
        }

        if (i % 0x10000 == 0)
        {
            /* progress indicator */
            printf("\b\b\b%2d%%", i * 100 / size);
        }

        buf[i] = teaspoon[i % COUNT(teaspoon)];
    }

    printf("\b\b\b100%%\n");
}

static void sf_dump()
{
    /* turn on the LED */
    led_on();

    /* dumping more than actual size will crash the emulator (possibly the actual camera, too) */
    /* most models have 0x800000, a few ones have 0x1000000 */
    uint32_t sf_size = 0;

    switch (get_model_id())
    {
        case 0x349: /* 5D4 */
        case 0x346: /* 100D */
        case 0x355: /* EOSM2 */
            sf_size = 0x1000000;
            break;

        case 0x350: /* 80D */
        case 0x393: /* 750D */
        case 0x347: /* 760D */
      //case 0x289: /* 7D2: serial flash is on the other CPU (slave) */
        case 0x325: /* 70D */
        case 0x301: /* 650D */
        case 0x326: /* 700D */
        case 0x331: /* EOSM */
        case 0x302: /* 6D */
            sf_size = 0x800000;
            break;
    }

    if (!sf_size)
    {
        /* assuming no serial flash on this model */
        printf(" - No serial flash.\n");
        return;
    }

    if (get_model_id() == 0x349)
    {
        /* 5D4 */
        sf_init = (void *) find_func_called_near_string_ref("\n**** SROM(", 0xD2090000, -0x10);
    }
    else
    {
        /* DIGIC 5 & 6 */
        uint32_t srom_tag = is_digic6() ? 0xC0820200 : 0xC0400000;
        sf_init = (void *) find_func_called_near_string_ref("\n**** SROM(SIO%d) Menu ****\n", srom_tag, -0x10);

        if (!sf_init)
        {
            /* 700D, 650D, EOSM */
            srom_tag = 0xC022C000;
            sf_init = (void *) find_func_called_near_string_ref("\n**** SROM Menu ****\n", srom_tag, -0x10);
        }
    }
    printf(" - sf_init %X\n", sf_init);

    uint32_t sf_cmd_tag = is_digic6() ? 0xD20B0000 : 0xC0820000;
    sf_command_sio = (void *) find_func_called_near_string_ref("Read Address[0x%06x-0x%06x]:0x", sf_cmd_tag, 0x100);
    printf(" - sf_command_sio %X\n", sf_command_sio);

    if (!sf_init || !sf_command_sio)
    {
        print_line(COLOR_RED, 2, "- Serial flash stubs not found.\n");
        led_off();
        return;
    }

    /* hardware initialization */
    uint32_t clock_reg = is_digic6() ? 0xD2090008 : 0xC0400008;
    MEM(clock_reg) |= 0x200000;
    sf_init();

    /* allocate max size statically and hope for the best */
    static uint8_t __buffer_alloc[0x1000000];
    uint8_t * buffer = (uint8_t *)((uint32_t) __buffer_alloc | 0x40000000);

    /* save the file  */
    printf(" - Reading serial flash...    ");
    qprintf("Serial flash buffer %X - %X\n", buffer, buffer + sf_size);
    sf_read(0, buffer, sf_size);

    printf(" - Writing SFDATA.BIN...");
    save_file("SFDATA.BIN", buffer, sf_size);

    /* also compute and save a MD5 checksum */
    dump_md5("SFDATA.BIN", buffer, sf_size);
}
#endif

static void dump_rom_to_file()
{
    init_file_io();

    /* note: boot_dump is going to print the newlines */

    if (is_digic6())
    {
        printf(" - Dumping ROM1...");
        boot_dump("ROM1.BIN", 0xFC000000, 0x02000000);
    }
    else if (is_digic78())
    {
        printf(" - Dumping ROM0...");
        boot_dump("ROM0.BIN", 0xE0000000, 0x02000000);
        printf(" - Dumping ROM1...");
        boot_dump("ROM1.BIN", 0xF0000000, 0x01000000);
    }
    else
    {
        printf(" - Dumping ROM0...");
        boot_dump("ROM0.BIN", 0xF0000000, 0x01000000);
        printf(" - Dumping ROM1...");
        boot_dump("ROM1.BIN", 0xF8000000, 0x01000000);
    }

    #ifdef CONFIG_BOOT_SROM_DUMPER
    sf_dump();
    #endif
}

static int (*set_bootflag)(int flag, int value) = 0;

static void enable_bootflag()
{
    const char * SetFlag_str = "Set flag?(Y=ON(0x%08x)/N=OFF(0x%08x))? :";
    uint32_t set_flag_i = 0;

    if (is_digic78())
    {
        set_flag_i = find_func_from_string_thumb(SetFlag_str, 0, 0x100);
        set_bootflag = (void*) find_func_called_after_string_ref_thumb(SetFlag_str, 2);
    }
    else if (strcmp(get_model_string(), "5D4") == 0)
    {
        /* this one is harder to autodetect */
        set_bootflag = (void*) 0x102704;
        set_flag_i = find_func_from_string(SetFlag_str, 0, 0x100);
        if (set_flag_i != 0x101510) fail();
    }
    else
    {
        set_flag_i = find_func_from_string(SetFlag_str, 0, 0x100);
        set_bootflag = (void*) find_func_called_near_string_ref(SetFlag_str, 0xFC040000, 0x100);
    }

    printf(" - set_bootflag %X (interactive %X)\n", set_bootflag, set_flag_i);

    if (set_bootflag && set_flag_i)
    {
        /* enable the boot flag */
        printf(" - Enabling boot flag...\n");
        set_bootflag(1, -1);
    }
}

static void cpuinfo_print(void)
{
    extern void cpuinfo_print_v5(void);     /* ARMv5TE (DIGIC 2..5) */
    extern void cpuinfo_print_v7p(void);    /* ARMv7 PMSA (DIGIC 6) */
    extern void cpuinfo_print_v7v(void);    /* ARMv7 VMSA (DIGIC 7) */

    if (is_digic6()) {
        cpuinfo_print_v7p();
    } else if (is_digic78()) {
        cpuinfo_print_v7v();
    } else {
        cpuinfo_print_v5();
    }
}

void
__attribute__((noreturn))
cstart( int loaded_as_thumb )
{
    zero_bss();
 
    ml_loaded_as_thumb = loaded_as_thumb;

    if (!ml_loaded_as_thumb)
    {
        /* install custom data abort handler */
        MEM(0x00000024) = (uint32_t)&_vec_data_abort;
        MEM(0x00000028) = (uint32_t)&_vec_data_abort;
        MEM(0x0000002C) = (uint32_t)&_vec_data_abort;
        MEM(0x00000030) = (uint32_t)&_vec_data_abort;
        MEM(0x00000038) = (uint32_t)&_vec_data_abort;
        MEM(0x0000003C) = (uint32_t)&_vec_data_abort;
    }

    disp_init();
    /* disable caching (seems to interfere with ROM dumping) */
    //~ disable_dcache();
    //~ disable_icache();
    //~ disable_write_buffer();

#ifdef CONFIG_BOOT_DUMPER
    /* Canon bug: their file I/O function ("Open file for write") copies data to CACHEABLE memory before saving!
     * https://www.magiclantern.fm/forum/index.php?topic=16534.msg170417#msg170417
     * present at least on DIGIC 4, 5 and 6, finally fixed in DIGIC 7! */
#endif

#ifndef CONFIG_BOOT_FULLFAT
    if (is_digic6())
    {
        sync_caches_d6();
        disable_caches_region1_ram_d6();
    }
    else
    {
        sync_caches();
        disable_all_caches();
    }
#endif

    print_line(COLOR_CYAN, 3, "  Magic Lantern Rescue\n");
    print_line(COLOR_CYAN, 3, " ----------------------------\n");

    print_model();
    prop_diag();
    print_bootflags();
    print_firmware_start();

    #if defined(CONFIG_BOOT_ROM_LAYOUT)
    print_rom_layout();
    #endif

    #if defined(CONFIG_BOOT_DUMPER)
        /* pick one method for dumping the ROM */
        #if defined(CONFIG_BOOT_DISP_DUMP)
            disp_dump(0xFC000000, 0x02000000);
        #else
            dump_rom_to_file();
        #endif
    #endif
    #if defined(CONFIG_BOOT_BOOTFLAG)
        enable_bootflag();
        print_bootflags();
    #endif
    #if defined(CONFIG_BOOT_CPUINFO)
        printf("\n");
        printf("CHDK CPU info for 0x%X %s\n", get_model_id(), get_model_string());
        printf("------------------------------\n");
        int old_size = printf_font_size; printf_font_size = 1;
        int old_color = printf_font_color; printf_font_color = COLOR_WHITE;
        cpuinfo_print();
        printf("\n");
        printf_font_size = old_size;
        printf_font_color = old_color;
    #endif

    printf(" - DONE!\n");

    if (1)
    {
        int saved_length = log_length;  /* save until here */
        init_file_io();
        printf(" - Saving RESCUE.LOG ...");
        save_file("RESCUE.LOG", log_buffer, saved_length);
    }

    printf("\n");
    print_line(COLOR_WHITE, 2, " You may now remove the battery.\n");

    while(1);
}
