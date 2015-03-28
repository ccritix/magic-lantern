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


#include "arm-mcr.h"
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

#define MEM(x)      (*(volatile uint32_t *)(x))
#define CACHED(x)   (((uint32_t)x) & ~0x40000000)
#define UNCACHED(x) (((uint32_t)x) | 0x40000000)
#define ALIGN(x,y)  (((x) + (y) - 1) & ~((y)-1))

extern uint8_t kernel_start;
extern uint8_t kernel_end;
extern uint8_t initrd_start;
extern uint8_t initrd_end;
extern uint8_t _end;
extern uint8_t *disp_yuvbuf;

uint32_t print_x = 0;
uint32_t print_y = 0;
char print_tmp[2];

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
    "ADR     R0, cstart_addr\n"
    "LDR     R0, [R0]\n"
    "MOV     R1, #0x40000000\n"
    "BIC     R0, R0, R1\n"
    "BX      R0\n"
    "B       cstart\n"
    
    "cstart_addr:\n"
    ".word   cstart\n"
);

void print_char()
{
    print_tmp[0] = (char)MEM(0x40000008);
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
    print_line(COLOR_CYAN, 1, buf);
    return 0;
}

void __attribute__ ((noinline)) dma_sync_caches()
{
    sync_caches();
}

uint32_t dma_int_status = 0;

void dma_pre_setup()
{
    dma_int_status = cli();
    dma_sync_caches();
}

void dma_post_setup()
{
    dma_sync_caches();
    sei(dma_int_status);
}

/* use dma engine for memcopy */
void dma_memcpy(void *dst, void *src, uint32_t bytes)
{
    if(bytes < 0x20)
    {
        memcpy(dst, src, bytes);
        return;
    }
    
    /* write back cache content and mark everything invalid */
    dma_pre_setup();
    
    /* initialize DMA engine */
    MEM(0xC0A10000) = 0x00000001;
    MEM(0xC0A10018) = src;
    MEM(0xC0A1001C) = dst;
    MEM(0xC0A10020) = bytes;
    
    /* start copying */
    MEM(0xC0A10008) = 0x00000201;
    
    while(MEM(0xC0A10008) & 1)
    {
    }
    
    /* write back cache content and mark everything invalid */
    dma_post_setup();
}

/* use dma engine for memset */
void dma_memset(void *dst, uint8_t value, uint32_t bytes)
{
    if(bytes < 0x20)
    {
        memset(dst, value, bytes);
        return;
    }
    
    /* write back cache content and mark everything invalid */
    dma_pre_setup();
    
    /* build a 32 bit word with the memset value */
    uint32_t memset_buf = value;
    memset_buf |= memset_buf << 8;
    memset_buf |= memset_buf << 16;
    
    /* fill 0x10 bytes already with that new value */
    uint32_t *memset_ptr = dst;
    
    memset_ptr[0] = memset_buf;
    memset_ptr[1] = memset_buf;
    memset_ptr[2] = memset_buf;
    memset_ptr[3] = memset_buf;
    
    /* initialize DMA engine */
    MEM(0xC0A10000) = 0x00000001;
    MEM(0xC0A10018) = dst;
    MEM(0xC0A1001C) = (uint32_t)(memset_ptr) + 0x10;
    MEM(0xC0A10020) = bytes - 0x10;
    
    /* start copying without altering source address */
    MEM(0xC0A10008) = 0x00000201 | 0x20;
    
    while(MEM(0xC0A10008) & 1)
    {
    }
    
    /* write back cache content and mark everything invalid */
    dma_post_setup();
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
    printf("   - Ramdisk: %d KiB\n", size);
    
    (*params)->hdr.tag = ATAG_RAMDISK; /* Ramdisk tag */
    (*params)->hdr.size = tag_size(atag_ramdisk); /* size tag */
    (*params)->u.ramdisk.flags = 0; /* Load the ramdisk */
    (*params)->u.ramdisk.size = size; /* Decompressed ramdisk size */
    (*params)->u.ramdisk.start = 0; /* Unused */
    *params = tag_next(*params); /* move pointer to next tag */
}

static void setup_initrd2_tag(struct atag **params, uint32_t start, uint32_t size)
{
    printf("   - initrd:  0x%08X Size: 0x%08X\n", start, size);

	(*params)->hdr.tag = ATAG_INITRD2;         /* Initrd2 tag */
	(*params)->hdr.size = tag_size(atag_initrd2);  /* size tag */

	(*params)->u.initrd2.start = start;        /* physical start */
	(*params)->u.initrd2.size = size;          /* compressed ramdisk size */

	*params = tag_next(*params);              /* move pointer to next tag */
}

static void setup_mem_tag(struct atag **params, uint32_t start, uint32_t len)
{
    printf("   - Memory:  0x%08X Size: 0x%08X\n", start, len);
    
	(*params)->hdr.tag = ATAG_MEM;             /* Memory tag */
	(*params)->hdr.size = tag_size(atag_mem);  /* size tag */

	(*params)->u.mem.start = start;            /* Start of memory area (physical address) */
	(*params)->u.mem.size = len;               /* Length of area */

	*params = tag_next(*params);              /* move pointer to next tag */
}

static void setup_cmdline_tag(struct atag **params, const char * line)
{
    printf("   - cmdline: '%s'\n", line);
    
	int linelen = strlen(line);

	if(!linelen)
    {
		return;                             /* do not insert a tag for an empty commandline */
    }

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
    uint32_t *interface_ptr = (void*)0x40000000;
    struct atag *atags_ptr = (void*)0x00000100;    
    
    disable_dcache();
    disable_icache();
    
    uint32_t ram_start = 0x00E00000;
    uint32_t ram_end   = 0x0FF20000;
    uint32_t ram_size  = ram_end - ram_start;
    
    /* place kernel at the start o */
    uint32_t kernel_size = CACHED(&kernel_end) - CACHED(&kernel_start);
    uint32_t kernel_addr = 0x8000;
    
    /* place initrd at RAM end */
    uint32_t initrd_size = CACHED(&initrd_end) - CACHED(&initrd_start);
    uint32_t initrd_addr = ram_start + ram_size - initrd_size;
    
    /* setup callback functions */
    interface_ptr[0] = &print_line_ext;
    interface_ptr[1] = &print_char;
    interface_ptr[2] = 0;
    
    /* setup atags */  
    printf("  Setup ATAGs...\n");
    setup_core_tag(&atags_ptr, 4096, 0x100);
    setup_mem_tag(&atags_ptr, CACHED(ram_start), ram_size);
    setup_ramdisk_tag(&atags_ptr, (initrd_size + 1023) / 1024);
    setup_initrd2_tag(&atags_ptr, CACHED(initrd_addr), initrd_size);
    setup_cmdline_tag(&atags_ptr, "init=/init root=/dev/ram0 earlyprintk=1");
    setup_end_tag(&atags_ptr);
    
    /* move kernel into free spa ce */
    printf("  Copying kernel to 0x%08X (0x%08X bytes)...\n", kernel_addr, kernel_size);
    dma_memcpy(kernel_addr, &kernel_start, kernel_size);
    printf("  Copying initrd to 0x%08X (0x%08X bytes)...\n", initrd_addr, initrd_size);
    dma_memcpy(initrd_addr, &initrd_start, initrd_size);
    
    printf("  Starting Kernel...\n");
    
    enable_dcache();
    enable_icache();
    
    
    asm(
        /* enter SVC mode */
        "MRS   R0, CPSR         \n"
        "BIC   R0, R0, #0x1F    \n"
        "ORR   R0, R0, #0xD3    \n"
        "MSR   CPSR, R0         \n"
        
        /* setup registers for kernel boot */
        "ADR   R4,  parameters  \n"
        "MOV   R0,  #0x00       \n"
        "LDR   R1,  [R4, #0x00] \n"
        "LDR   R2,  [R4, #0x04] \n"
        "LDR   R3,  [R4, #0x08] \n"
        "LDR   SP,  [R4, #0x0C] \n"
        
        /* boot kernel */
        "BX    R3               \n"
        
        /* memory addresses */
        "parameters:\n"
        ".word   0xFFFFFFFF\n" /* machine ID */
        ".word   0x00000100\n" /* ATAGs address */
        ".word   0x00008000\n" /* linux kernel address */
        ".word   0x00008000\n" /* stack for kernel boot */
    );
}

void
__attribute__((noreturn))
cstart( void )
{
    print_x = 0;
    print_y = 0;
    
    MEM(0xC0400004) = 0x403;
    MEM(0xC0400004) |= 0x03000000;
    MEM(0xC0400044) = 0;
    MEM(0xC0400134) = 0;
    MEM(0xC0400008) |= 0x43000F;
    
    disp_init();
    
    printf(" Magic Lantern Linux Loader\n");
    printf("----------------------------\n");
    
    boot_linux();
    while(1);
}

