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
void _vec_data_abort();
extern uint32_t _dat_data_abort;

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
    "ADR     R4, addresses\n"
    
    /* flush all caches so we can work on uncached memory */
    "MOV     R0, #0\n"
    "MCR     p15, 0, R0, c7, c5, 0\n" // entire I cache
    "MCR     p15, 0, R0, c7, c6, 0\n" // entire D cache
    "MCR     p15, 0, R0, c7, c10, 4\n" // drain write buffer
    
    /* now copy the whole binary to 0x40008000 */
    "LDR     R0, [R4, #0]\n"
    "LDR     R1, [R4, #4]\n"
    "LDR     R2, [R4, #8]\n"
    "MOV     R3, #0x40000000\n"
    "ORR     R0, R0, R3\n"
    "ORR     R1, R1, R3\n"
    
    "_copy_loop:\n"
    "CMP     R0, R1\n"
    "BGE     _exec_main\n"
    "LDMIA   R2!, {R5, R6, R7, R8, R9, R10, R11, R12}\n"
    "STMIA   R0!, {R5, R6, R7, R8, R9, R10, R11, R12}\n"
    "B       _copy_loop\n"
    
    /* and jump to the cached memory */
    "_exec_main:\n"
    "MOV     R0, #0\n"
    "MCR     p15, 0, R0, c7, c5, 0\n" // flush I cache
    "LDR     R0, [R4, #0x0C]\n"
    "BX      R0\n"
    
    "addresses:\n"
    ".word   _start\n"
    ".word   _end\n"
    ".word   0x40800000\n"
    ".word   cstart\n"
    
    /* return on ABORT */
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

void print_char()
{
    char print_tmp[2];
    uint32_t ints = cli();
    
    print_tmp[0] = (char)MEM(0x00008008);
    print_tmp[1] = 0;
    
    font_draw(&print_x, &print_y, COLOR_WHITE, 1, print_tmp, 1);
    
    if(print_y >= 480)
    {
        print_y = 480 - disp_direct_scroll_up(1);
    }
    
    sei(ints);
}

void print_line_ext(uint32_t color, uint32_t scale, char *txt, uint32_t count)
{
    uint32_t ints = cli();
    
    font_draw(&print_x, &print_y, color, scale, txt, count);
    
    if(print_y >= 480)
    {
        print_y = 480 - disp_direct_scroll_up(1);
    }
    sei(ints);
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
    MEM(0xC0A10000) = 1;
    MEM(0xC0A10004) = 0;
    MEM(0xC0A10010) = 0;
    MEM(0xC0A10014) = 0;
    MEM(0xC0A10018) = (uint32_t)src;
    MEM(0xC0A1001C) = (uint32_t)dst;
    MEM(0xC0A10020) = bytes;
    
    /* start copying */
    MEM(0xC0A10008) = 0x00030200;
    MEM(0xC0A10008) |= 1;
    
    while((MEM(0xC0A10010) | 1) != 1)
    {
    }
    while(MEM(0xC0A10008) & 1)
    {
    }
    
    /* reset some flags */
    MEM(0xC0A10010) = 0;
    
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
    MEM(0xC0A10000) = 1;
    MEM(0xC0A10004) = 0;
    MEM(0xC0A10010) = 0;
    MEM(0xC0A10014) = 0;
    MEM(0xC0A10018) = (uint32_t)dst;
    MEM(0xC0A1001C) = ((uint32_t)(memset_ptr) + 0x10);
    MEM(0xC0A10020) = bytes - 0x10;
    
    /* start copying without altering source address */
    MEM(0xC0A10008) = 0x00030200 | 0x20;
    MEM(0xC0A10008) |= 1;
    
    while((MEM(0xC0A10010) | 1) != 1)
    {
    }
    while(MEM(0xC0A10008) & 1)
    {
    }
    
    /* reset some flags */
    MEM(0xC0A10010) = 0;
    
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
    uint32_t *interface_ptr = (void*)0x00008000;
    struct atag *atags_ptr = (void*)0x00000100;    
    
    uint32_t ram_start = 0x01000000;
    uint32_t ram_end   = 0x20000000;
    
    /* check for RAM size */
    printf("  Checking RAM size...");
    MEM(0x50008000) = 0xDAAABEEF;
    
    if(MEM(0x50008000) != 0xDAAABEEF)
    {
        ram_end = 0x10000000;
    }
    uint32_t ram_size  = ram_end - ram_start;
    
    printf("  %d MiB\n", ram_size / 1024 / 1024);
    
    /* force 256MiB because 7D crashes during boot. other digic writing there? */
    ram_end = 0x10000000;
    
    /* place kernel */
    uint32_t kernel_size = &kernel_end - &kernel_start;
    uint32_t kernel_addr = 0x00800000;
    
    /* place initrd at RAM end */
    uint32_t initrd_size = &initrd_end - &initrd_start;
    uint32_t initrd_addr = ram_end - initrd_size;
    
    /* setup callback functions, this loader acts as "BIOS" */
    interface_ptr[0] = (uint32_t)&print_line_ext;
    interface_ptr[1] = (uint32_t)&print_char;
    interface_ptr[2] = 0;
    
    /* setup atags */  
    printf("  Setup ATAGs...\n");
    setup_core_tag(&atags_ptr, 8192, 0x100);
    setup_mem_tag(&atags_ptr, ram_start, ram_size);
    setup_ramdisk_tag(&atags_ptr, (initrd_size + 1023) / 1024);
    setup_initrd2_tag(&atags_ptr, initrd_addr, initrd_size);
    setup_cmdline_tag(&atags_ptr, "init=/bin/init root=/dev/ram0 earlyprintk=1");
    setup_end_tag(&atags_ptr);
    
    /* move kernel into free space */
    printf("  Copying kernel to 0x%08X (0x%08X bytes)...\n", kernel_addr, kernel_size);
    dma_memcpy((void *)kernel_addr, &kernel_start, kernel_size);
    printf("  Copying initrd to 0x%08X (0x%08X bytes)...\n", initrd_addr, initrd_size);
    dma_memcpy((void *)initrd_addr, &initrd_start, initrd_size);
    
    printf("  Starting Kernel...\n");
    
    //enable_dcache();
    //enable_icache();
    
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
        ".word   0x00800000\n" /* linux kernel address */
        ".word   0x00700000\n" /* stack for kernel boot */
    );
}




void int_enable(uint32_t id)
{
    MEM(0xC0201010) = id;
    volatile uint32_t status = MEM(0xC0201200);
}

uint32_t gpio_get_base(uint32_t port)
{
    return 0xC0220000 | (port << 2);
}

void gpio_high(uint32_t port)
{
    uint32_t base = gpio_get_base(port);
    
    MEM(base) |= 0x02;
}

void gpio_low(uint32_t port)
{
    uint32_t base = gpio_get_base(port);
    
    MEM(base) &= ~0x02;
}

uint32_t gpio_get(uint32_t port)
{
    uint32_t base = gpio_get_base(port);
    
    return ((MEM(base) & 0x02) != 0);
}

void gpio_set(uint32_t port, uint32_t state)
{
    if(state)
    {
        gpio_high(port);
    }
    else
    {
        gpio_low(port);
    }
}

uint32_t sio_get_base(uint32_t module)
{
    return 0xC0820000 | (module << 8);
}

void sio_xmit_word(uint32_t module, uint16_t *data)
{
    uint32_t base = sio_get_base(module);
    
    MEM(base + 0x18) = *data;
    
    MEM(base + 0x04) |= 1;
    
    while(MEM(base + 0x04) & 1)
    {
    }
    
    *data = MEM(base + 0x1C);
}

void sio_send_async(uint32_t module, uint16_t data)
{
    uint32_t base = sio_get_base(module);
    
    MEM(base + 0x18) = data;
    MEM(base + 0x04) |= 1;
}

void sio_recv_async(uint32_t module, uint16_t *data)
{
    uint32_t base = sio_get_base(module);
    
    MEM(base + 0x10) = 0;
    *data = MEM(base + 0x1C);
}

uint32_t sio_ready(uint32_t module)
{
    uint32_t base = sio_get_base(module);
    
    return ((MEM(base + 0x04) & 1) == 0);
}

void sio_init(uint32_t module)
{
    uint32_t base = sio_get_base(module);
    
    /* enable clocks */
    MEM(0xC0400008) |=  0x430005;
    
    /* setup GPIOs? */
    MEM(0xC0221300) =  0x25;
    
    MEM(base + 0x08) =  0x1;
    MEM(base + 0x0C) =  0x13020010;
}

#define CS_ACTIVE   0
#define CS_INACTIVE 1

void mpu_set_cs(uint32_t state)
{
    gpio_set(39, state);
}

uint32_t mpu_get_cs()
{
    return gpio_get(39) ? CS_INACTIVE : CS_ACTIVE;
}



enum mpu_xmit_state
{
    MPU_XMIT_IDLE = 0,
    
    /* via mpu_xmit(), then sio3_isr() */
    MPU_XMIT_SEND_START,
    MPU_XMIT_SEND,
    MPU_XMIT_SEND_WAIT_ISR,
    
    /* via mreq_isr() */
    MPU_XMIT_RECV_HDR,
    MPU_XMIT_RECV_HDR_WAIT_ISR,
    /* via sio3_isr() */
    MPU_XMIT_RECV_DATA,
    MPU_XMIT_RECV_DATA_WAIT_ISR
};


volatile enum mpu_xmit_state mpu_state = MPU_XMIT_IDLE;
uint32_t mpu_mreq_pending = 0;
uint8_t recv_buf[128];
uint32_t recv_buf_count = 0;
uint32_t recv_buf_pos = 0;
uint8_t send_buf[128];
uint32_t send_buf_count = 0;
uint32_t send_buf_pos = 0;

#define BYTESWAP(x) (((x)>>8) | ((x)<<8))
static void mpu_handle(void);
static void mpu_handle(void);
static void mpu_received(uint8_t *data, uint32_t length);
static void mpu_send(uint8_t *buf, uint32_t length, uint32_t blocking);


void mpu_init()
{
    mpu_state = MPU_XMIT_IDLE;
    mpu_mreq_pending = 0;
    recv_buf_count = 0;
    recv_buf_pos = 0;
    send_buf_count = 0;
    send_buf_pos = 0;
    
    uint8_t init_msg[] = { 0x06, 0x04, 0x02, 0x00, 0x00, 0x00};
    mpu_send(init_msg, 6, 1);
}

static void mpu_send(uint8_t *buf, uint32_t length, uint32_t blocking)
{
restart:
    /* wait till ready */
    while(mpu_state != MPU_XMIT_IDLE)
    {
    }
    
    uint32_t int_status = cli();
    
    if(mpu_state != MPU_XMIT_IDLE)
    {
        sei(int_status);
        goto restart;
    }
    
    /* we got the lock */
    memcpy(send_buf, buf, length);
    send_buf_count = length;
    mpu_state = MPU_XMIT_SEND_START;
    
    sei(int_status);
    
    /* start transfer */
    mpu_handle();
    
    /* wait till done */
    while(blocking && mpu_state != MPU_XMIT_IDLE)
    {
    }
}

static void mpu_received(uint8_t *data, uint32_t length)
{
    printf("Received %d bytes: ", length);
    
    for(uint32_t pos = 0; pos < length; pos++)
    {
        printf(" %02X", data[pos]);
    }
    printf("\n");
    
    /* do not call mpu_send() blocking from here, as we are in interrupt */
}

static void mpu_handle()
{
    uint32_t int_status = cli();
    uint32_t restart = 0;
    uint16_t data = 0;
    
    do
    {
        restart = 0;
        
        switch(mpu_state)
        {
            case MPU_XMIT_IDLE:
                if(!mpu_mreq_pending)
                {
                    break;
                }
                
                /* oh, the MREQ line was toggled meanwhile. handle it */
                mpu_mreq_pending = 0;
                recv_buf_pos = 0;
                recv_buf_count = 0;
                mpu_state = MPU_XMIT_RECV_HDR;
                restart = 1;
                break;
                
            case MPU_XMIT_SEND_START:  
                recv_buf_pos = 0;
                recv_buf_count = 0;
                send_buf_pos = 0;          
                mpu_set_cs(CS_ACTIVE);
                break;
                
            case MPU_XMIT_SEND:
                if(send_buf_pos >= send_buf_count)
                {
                    mpu_set_cs(CS_INACTIVE);
                    mpu_state = MPU_XMIT_IDLE;
                    break;
                }
                
                /* there is data to send */
                data = send_buf[send_buf_pos++] << 8;
                data |= send_buf[send_buf_pos++];
                
                sio_send_async(3, data);
                mpu_state = MPU_XMIT_SEND_WAIT_ISR;
                break;
                
            case MPU_XMIT_RECV_HDR:
                mpu_set_cs(CS_ACTIVE);
                sio_send_async(3, 0);
                mpu_state = MPU_XMIT_RECV_HDR_WAIT_ISR;
                break;
                
            case MPU_XMIT_RECV_DATA:
                /* finished transfer? */
                if(recv_buf_pos >= recv_buf_count)
                {
                    mpu_set_cs(CS_INACTIVE);
                    mpu_state = MPU_XMIT_IDLE;
                    mpu_received(recv_buf, recv_buf_pos);
                    break;
                }
                
                /* we need some more data */
                sio_send_async(3, 0);
                mpu_state = MPU_XMIT_RECV_DATA_WAIT_ISR;
                break;
                
            default:
                break;
        }
    } while(restart);

    sei(int_status);
}

static void mreq_isr()
{
    MEM(0xC020302C) = 0x1C;

    /* check for MREQ? */
    if (!(MEM(0xC020302C) & 1))
    {
        switch(mpu_state)
        {
            case MPU_XMIT_IDLE:
                mpu_mreq_pending = 1;
                break;
                
            case MPU_XMIT_SEND_START:
                mpu_state = MPU_XMIT_SEND;
                break;
                
            default:
                break;
        }
        
        mpu_handle();
    }
}

static void sio3_isr()
{
    /* we received a SIO ISR, so there is new data available */
    uint16_t data = 0;
    sio_recv_async(3, &data);
    
    recv_buf[recv_buf_pos++] = data >> 8;
    recv_buf[recv_buf_pos++] = data & 0xFF;
    
    switch(mpu_state)
    {
        case MPU_XMIT_SEND_WAIT_ISR:
            mpu_state = MPU_XMIT_SEND;
            break;
            
        case MPU_XMIT_RECV_HDR_WAIT_ISR:
            recv_buf_count = recv_buf[0];
            mpu_state = MPU_XMIT_RECV_DATA;
            break;
            
        case MPU_XMIT_RECV_DATA_WAIT_ISR:
            mpu_state = MPU_XMIT_RECV_DATA;
            break;
                
        default:
            break;
    }
    
    mpu_handle();
}


void __attribute__((interrupt)) irq_handler()
{
    /* get interrupt id */
    uint32_t irq_id = MEM(0xC0201004) >> 2;
    
    /* check if timer counted up */
    if(irq_id == 0x50)
    {
        mreq_isr();
    }

    if(irq_id == 0x36)
    {
        sio3_isr();
    }

    /* reset/re-arm interrupt. works without, but firmware does so */
    int_enable(irq_id);
}

void
__attribute__((noreturn))
cstart( void )
{
    disable_dcache();
    enable_icache();
    
    /* install custom data abort handler */
    MEM(0x00000024) = (uint32_t)&_vec_data_abort;
    MEM(0x00000028) = (uint32_t)&_vec_data_abort;
    MEM(0x0000002C) = (uint32_t)&_vec_data_abort;
    MEM(0x00000030) = (uint32_t)&_vec_data_abort;
    MEM(0x00000038) = (uint32_t)&irq_handler;
    MEM(0x0000003C) = (uint32_t)&_vec_data_abort;
    
    /* allow interrupts */
    sei(0);
    
    print_x = 0;
    print_y = 0;
    
    disp_init(&_end, (void*)0x00800000);
    
    printf(" Magic Lantern Linux Loader\n");
    printf("----------------------------\n");


    printf("  Setup MPU...\n");
    
    /* setup SPI lines */
    mpu_set_cs(CS_INACTIVE);
    sio_init(3);
    
    /* setup MREQ? */
    MEM(0xC020302C) = 0xC;
    
    /* enable interrupts for MREQ and SPI */
    int_enable(0x50);
    int_enable(0x36);
    
    mpu_init();
    
    printf("  Done\n");
    
    //boot_linux();
    while(1);
}

