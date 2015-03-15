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

enum { DRIVE_CF, DRIVE_SD } boot_drive;
enum { MODE_READ=1, MODE_WRITE=2 } boot_access_mode;


static void boot_dump(char* filename, uint32_t addr, int size)
{
    /* check whether our stubs were initialized */
    if (!boot_fileopen) fail();
    if (!boot_filewrite) fail();
    if (!boot_fileclose) fail();
    
    int block_size = 512;
    int pos = 0;
    int drive = DRIVE_SD;
    
    /* turn on the LED */
    #ifdef CARD_LED_ADDRESS
    MEM(CARD_LED_ADDRESS) = (LEDON);
    #endif

    /* save the file */
    int f = boot_fileopen(drive, filename, MODE_WRITE);
    if (f != -1)
    {
        while ( pos < size )
        {
            #ifdef CARD_LED_ADDRESS
            MEM(CARD_LED_ADDRESS) = ((pos >> 16) & 1)?(LEDON):(LEDOFF);
            #endif
            
            if(size - pos < block_size)
            {
                block_size = size - pos;
            }
            if(boot_filewrite(drive, f, (void*)(addr + pos), block_size) == -1)
            {
                break;
            }
            pos += block_size;
        }
        boot_fileclose(1, f);
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
uint32_t print_line_pos = 2;


void print_line(uint32_t color, uint32_t scale, char *txt)
{
    font_draw(20, print_line_pos * 10, color, scale, txt);
    
    print_line_pos += scale;
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

void fat_deinit(FF_IOMAN *ioman)
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

void dump_rom(FF_IOMAN *ioman)
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
    
    print_line(COLOR_CYAN, 3, " Magic Lantern Rescue");
    print_line(COLOR_CYAN, 3, "----------------------------");
    
    prop_diag();
    
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
    
    printf(" - DONE!");
    
    //led_dump(0xF8000000, 0x01000000);

    while(1)
    {
        blink(10);
        busy_wait(20);
    }

#if 0   
    /* 550D stubs, all called from "Open file for write: %s\n" */
    boot_fileopen  = (void*) 0xFFFF9D30;
    boot_filewrite = (void*) 0xFFFFA140;
    boot_fileclose = (void*) 0xFFFF9E80;
    
    /* 60D check */
    if (MEM(boot_fileopen)  != 0xe92d41f0) fail();
    if (MEM(boot_filewrite) != 0xe92d4fff) fail();
    if (MEM(boot_fileclose) != 0xe92d41f0) fail();
#endif


    /* 600D v1.0.2 stubs, all called from "Open file for write: %s\n" */
    boot_fileopen  = (void*) 0xFFFF9D80;
    boot_filewrite = (void*) 0xFFFFA190;
    boot_fileclose = (void*) 0xFFFF9ED0;
    
    /* 60D check */
    if (MEM(boot_fileopen)  != 0xe92d41f0) fail();
    if (MEM(boot_filewrite) != 0xe92d4fff) fail();
    if (MEM(boot_fileclose) != 0xe92d41f0) fail();

    busy_wait(1000);
    
    /* ROM dump */
    boot_dump("DUMMY.BIN", 0xF8000000, 0x01000000);
    boot_dump("ROM1.BIN", 0xF8000000, 0x01000000);
    
    /* signal it's finished */
    blink(100);
    
    while(1)
    {
    }
}
