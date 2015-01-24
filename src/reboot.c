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

#include "compiler.h"
#include "consts.h"

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
);

static void busy_wait(int n)
{
    int i,j;
    static volatile int k = 0;
    for (i = 0; i < n; i++)
        for (j = 0; j < 100000; j++)
            k++;
}

static void blink(int n)
{
    while (1)
    {
        #if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
        *(volatile int*) (CARD_LED_ADDRESS) = (LEDON);
        busy_wait(n);
        *(volatile int*)(CARD_LED_ADDRESS) = (LEDOFF);
        busy_wait(n);
        #endif
    }
}

static void fail()
{
    blink(50);
}

/* file I/O stubs */
static int (*boot_fileopen)(int drive, char *filename, int mode) = 0; // read=1, write=2
static int (*boot_filewrite)(int drive, int handle, void* address, unsigned long size) = 0;
static int (*boot_fileclose)(int drive, int handle) = 0;

enum { DRIVE_CF, DRIVE_SD } boot_drive;
enum { MODE_READ=1, MODE_WRITE=2 } boot_access_mode;

#define MEM(x) *(volatile uint32_t*)(x)

static void boot_dump(char* filename, uint32_t addr, int size)
{
    /* check whether our stubs were initialized */
    if (!boot_fileopen) fail();
    if (!boot_filewrite) fail();
    if (!boot_fileclose) fail();
    
    int blockSize = 512;
    int pos = 0;
    int drive = DRIVE_SD;
    
    /* turn on the LED */
    MEM(CARD_LED_ADDRESS) = (LEDON);

    /* save the file */
    int f = boot_fileopen(drive, filename, MODE_WRITE);
    if (f != -1)
    {
        while ( pos < size )
        {
            MEM(CARD_LED_ADDRESS) = ((pos >> 16) & 1)?(LEDON):(LEDOFF);
            
            if(size - pos < blockSize)
            {
                blockSize = size - pos;
            }
            if(boot_filewrite(drive, f, (void*)(addr + pos), blockSize) == -1)
            {
                break;
            }
            pos += blockSize;
        }
        boot_fileclose(1, f);
    }
    else
    {
        fail();
    }

    /* turn off the LED */
    MEM(CARD_LED_ADDRESS) = (LEDOFF);
}

void
__attribute__((noreturn))
cstart( void )
{
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

    /* ROM dump */
    boot_dump("DUMMY.BIN", 0xF8000000, 0x01000000);
    boot_dump("ROM1.BIN", 0xF8000000, 0x01000000);
    
    /* signal it's finished */
    blink(100);
    
    // Unreachable
    while(1)
        ;
}
