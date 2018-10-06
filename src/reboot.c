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

#include "compiler.h"
#include "consts.h"
#include "fullfat.h"
#include "md5.h"
#include "asm.h"

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
    "LDR PC, =cstart\n"         // long jump (into the uncacheable version, if linked that way)

    ".code 16\n"
    "loaded_as_thumb:\n"
    "MOVS R0, #1\n"             // cstart(1) = loaded as Thumb; MOV fails
    "LDR R1, _cstart_addr\n"    /* long jump into ARM code (uncacheable, if linked that way) */
    "BX R1\n"
    "NOP\n"
    "_cstart_addr:\n"
    ".word cstart\n"

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

    return 0;
}

uint32_t is_digic6()
{
    return get_model_id() == *(uint32_t *)0xFC060014;
}

uint32_t is_digic7()
{
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

static char log_buffer_alloc[16384];
static char * log_buffer = 0;
static int log_length = 0;

static int printf_font_size = 2;
static int printf_font_color = COLOR_CYAN;

static uint32_t print_x = 0;
static uint32_t print_y = 20;

void print_line(uint32_t color, uint32_t scale, char *txt)
{
    qprint(txt);

    if (!log_buffer)
    {
        uint32_t caching_bit = (is_vxworks()) ? 0x10000000 : 0x40000000;
        log_buffer = (uintptr_t) log_buffer_alloc | caching_bit;
    }
    log_length += snprintf(log_buffer + log_length, sizeof(log_buffer_alloc) - log_length, "%s", txt);

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

/* only for lines where '\n' was not yet printed */
static void clear_line()
{
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    printf("                                            ");
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
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

/* file I/O stubs */
enum boot_drive { DRIVE_CF, DRIVE_SD };
static int (*boot_open_write)(int drive, char* filename, void* buf, uint32_t size);
static int (*boot_card_init)();

/* for intercepting Canon messages */
static int (*boot_putchar)(int ch) = 0;

static void save_file(int drive, char* filename, void* addr, int size)
{
    /* check whether our stubs were initialized */
    if (!boot_open_write)
    {
        print_line(COLOR_RED, 2, " - Boot file write stub not set.\n");
        fail();
    }

    if (boot_open_write(drive, filename, (void*) addr, size) == -1)
    {
        print_line(COLOR_RED, 2, " - Boot file write error.\n");
        fail();
    }
}

static void dump_md5(int drive, char* filename, void * addr, int size)
{
    uint8_t md5_bin[16];
    char md5_ascii[50];
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
    printf(" - MD5: %s\n", md5_ascii);
    md5_ascii[32] = ' ';

    save_file(drive, md5file, (void*) md5_ascii, strlen(md5_ascii));
}

static void boot_dump(int drive, char* filename, uint32_t addr, int size)
{
    /* turn on the LED */
    led_on();

    /* save the file and the MD5 checksum */
    save_file(drive, filename, (void *) addr, size);
    dump_md5(drive, filename, (void *) addr, size);

    /* turn off the LED */
    led_off();
}

#ifdef CONFIG_BOOT_FULLFAT
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

static struct sd_ctx sd_ctx;

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

extern void malloc_init(void *ptr, uint32_t size);

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

    if (is_digic7())
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

    if (is_digic7())
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
        is_digic6() ? 0xFC040000 :
        is_digic7() ? 0xE1FF8000 :
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

    uint32_t (*find_func_from_str)(const char *, uint32_t, uint32_t) = is_digic7() ? find_func_from_string_thumb : find_func_from_string;

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

    return found;
}

static void init_boot_file_io_stubs()
{
    /* autodetect this one */
    uint32_t (*find_func_from_str)(const char *, uint32_t, uint32_t) = is_digic7() ? find_func_from_string_thumb : find_func_from_string;
    boot_open_write = (void*) find_func_from_str("Open file for write : %s\n", 0, 0x50);
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

static void sf_dump(int drive)
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

        /* 80D, 750D, 760D, 7D2 use 0x800000 */
        /* 70D, 650D, 700D, EOSM and 6D, too */
        default:
            sf_size = 0x800000;
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
    printf(" - Reading serial flash to memory:    ");
    qprintf("Serial flash buffer %X - %X\n", buffer, buffer + sf_size);
    sf_read(0, buffer, sf_size);

    printf(" - Writing serial flash to SFDATA.BIN\n");
    save_file(drive, "SFDATA.BIN", buffer, sf_size);

    /* also compute and save a MD5 checksum */
    dump_md5(drive, "SFDATA.BIN", buffer, sf_size);
}
#endif

static void dump_rom_with_canon_routines()
{
    init_boot_file_io_stubs();

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
        printf(" - Dumping ROM1...\n");
        boot_dump(DRIVE_SD, "ROM1.BIN", 0xFC000000, 0x02000000);
    }
    else if (is_digic7())
    {
        printf(" - Dumping ROM0...\n");
        boot_dump(DRIVE_SD, "ROM0.BIN", 0xE0000000, 0x02000000);
        printf(" - Dumping ROM1...\n");
        boot_dump(DRIVE_SD, "ROM1.BIN", 0xF0000000, 0x01000000);
    }
    else
    {
        printf(" - Dumping ROM0...\n");
        boot_dump(DRIVE_SD, "ROM0.BIN", 0xF0000000, 0x01000000);
        printf(" - Dumping ROM1...\n");
        boot_dump(DRIVE_SD, "ROM1.BIN", 0xF8000000, 0x01000000);
    }

    #ifdef CONFIG_BOOT_SROM_DUMPER
    sf_dump(DRIVE_SD);
    #endif
}

#ifdef CONFIG_BOOT_FULLFAT
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
#else
    print_line(COLOR_RED, 2, " - FullFAT unsupported.\n");
#endif
}
#endif

static int (*set_bootflag)(int flag, int value) = 0;

static void enable_bootflag()
{
    const char * SetFlag_str = "Set flag?(Y=ON(0x%08x)/N=OFF(0x%08x))? :";
    uint32_t set_flag_i = 0;

    if (is_digic7())
    {
        set_flag_i = find_func_from_string_thumb(SetFlag_str, 0, 0x100);
        set_bootflag = (void*) find_func_called_after_string_ref_thumb(SetFlag_str, 2);
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
    } else if (is_digic7()) {
        cpuinfo_print_v7v();
    } else {
        cpuinfo_print_v5();
    }
}

extern void disable_caches_region1_ram_d6(void);

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
    if (is_digic6())
    {
        disable_caches_region1_ram_d6();
    }
    else if (!is_digic7())
    {
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
        #if defined(CONFIG_BOOT_FULLFAT)
            dump_rom_with_fullfat();
        #elif defined(CONFIG_BOOT_DISP_DUMP)
            disp_dump(0xFC000000, 0x02000000);
        #else
            dump_rom_with_canon_routines();
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

    if (boot_open_write && log_buffer)
    {
        int saved_length = log_length;  /* save until here */
        printf(" - Saving RESCUE.LOG ...\n");
        save_file(DRIVE_SD, "RESCUE.LOG", log_buffer, saved_length);
    }

    printf("\n");
    print_line(COLOR_WHITE, 2, " You may now remove the battery.\n");

    while(1);
}
