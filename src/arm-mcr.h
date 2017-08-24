/** \file
 * ARM control registers
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

#ifndef _arm_mcr_h_
#define _arm_mcr_h_

#define inline inline __attribute__((always_inline))

asm(
    ".text\n"
    ".align 4\n"
);

#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include "compiler.h"

typedef void (*thunk)(void);


#if 0
typedef signed long    int32_t;
typedef unsigned long    uint32_t;
typedef signed short    int16_t;
typedef unsigned short    uint16_t;
typedef signed char    int8_t;
typedef unsigned char    uint8_t;

typedef uint32_t    size_t;
typedef int32_t        ssize_t;
#endif

static inline uint32_t
read_lr( void )
{
    uint32_t lr;
    asm __volatile__ ( "mov %0, %%lr" : "=&r"(lr) );
    return lr;
}

static inline uint32_t
read_sp( void )
{
    uint32_t sp;
    asm __volatile__ ( "mov %0, %%sp" : "=&r"(sp) );
    return sp;
}

static inline void
select_normal_vectors( void )
{
    uint32_t reg;
    asm(
        "mrc p15, 0, %0, c1, c0\n"
        "bic %0, %0, #0x2000\n"
        "mcr p15, 0, %0, c1, c0\n"
        : "=r"(reg)
    );
}

/*
     naming conventions
    --------------------
    
    FLUSH instruction cache:
        just mark all entries in the I cache as invalid. no other effect than slowing down code execution until cache is populated again.
    
    DRAIN write buffer:
        halt CPU execution until every RAM write operation has finished.
    
    FLUSH data cache / cache entry:
        invalidating the cache content *without* writing back the dirty data into memory. (dangerous!)
        this will simply mark any data in cache (or in the line) as invalid, no matter if it was written back into RAM.
        e.g. 
            mcr p15, 0, Rd, c7, c6, 0  # whole D cache
            mcr p15, 0, Rd, c7, c6, 1  # single line in D cache
    
    CLEAN data cache entry:
        cleaning is the process of checking a single cahe entry and writing it back into RAM if it was not written yet.
        this will ensure that the contents of the data cache are also in RAM. only possible for a single line, so we have to loop though all lines.
        e.g.
            mcr p15, 0, Rd, c7, c10, 1 # Clean data cache entry (by address)
            mcr p15, 0, Rd, c7, c14, 1 # Clean data cache entry (by index/segment)
            
    CLEAN and FLUSH data cache entry:
        the logical consequence - write it back into RAM and mark cache entry as invalid. As if it never was in cache.
        e.g.
            mcr p15, 0, Rd, c7, c10, 2 # Clean and flush data cache entry (by address)
            mcr p15, 0, Rd, c7, c14, 2 # Clean and flush data cache entry (by index/segment)

*/

/* do you really want to call that? */
static inline void _flush_caches()
{
    uint32_t reg = 0;
    asm(
        "mov %0, #0\n"
        "mcr p15, 0, %0, c7, c5, 0\n" // entire I cache
        "mov %0, #0\n"
        "mcr p15, 0, %0, c7, c6, 0\n" // entire D cache
        "mcr p15, 0, %0, c7, c10, 4\n" // drain write buffer
        : : "r"(reg)
    );
}

/* write back all data into RAM and mark as invalid in data cache */
static inline void clean_d_cache()
{
    /* assume 8KB data cache */
    /* http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0092b/ch04s03s04.html */
    uint32_t segment = 0;
    do {
        uint32_t line = 0;
        for( ; line != 0x800 ; line += 0x20 )
        {
            asm(
                "mcr p15, 0, %0, c7, c14, 2"
                : : "r"( line | segment )
            );
        }
    } while( segment += 0x40000000 );
    
    /* ensure everything is written into RAM, halts CPU execution until pending operations are done */
    uint32_t reg = 0;
    asm(
        "mcr p15, 0, %0, c7, c10, 4\n" // drain write buffer
        : : "r"(reg)
    );
}

/* mark all entries in I cache as invalid */
static inline void flush_i_cache()
{
    asm(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c5, 0\n" // flush I cache
        : : : "r0"
    );
}

/* ensure data is written into RAM and the instruction cache is empty so everything will get fetched again */
static inline void sync_caches()
{
    clean_d_cache();
    flush_i_cache();
}

// This must be a macro
#define setup_memory_region( region, value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c6, c" #region "\n" : : "r"(value) )

#define set_d_cache_regions( value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c2, c0\n" : : "r"(value) )

#define set_i_cache_regions( value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c2, c0, 1\n" : : "r"(value) )

#define set_d_buffer_regions( value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c3, c0\n" : : "r"(value) )

#define set_d_rw_regions( value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c5, c0, 0\n" : : "r"(value) )

#define set_i_rw_regions( value ) \
    asm __volatile__ ( "mcr p15, 0, %0, c5, c0, 1\n" : : "r"(value) )

static inline void
set_control_reg( uint32_t value )
{
    asm __volatile__ ( "mcr p15, 0, %0, c3, c0\n" : : "r"(value) );
}

static inline uint32_t
read_control_reg( void )
{
    uint32_t value;
    asm __volatile__ ( "mrc p15, 0, %0, c3, c0\n" : "=r"(value) );
    return value;
}


static inline void
set_d_tcm( uint32_t value )
{
    asm( "mcr p15, 0, %0, c9, c1, 0\n" : : "r"(value) );
}

static inline void
set_i_tcm( uint32_t value )
{
    asm( "mcr p15, 0, %0, c9, c1, 1\n" : : "r"(value) );
}


/** Routines to enable / disable interrupts */
static inline uint32_t
cli(void)
{
    uint32_t old_irq;
    
    asm __volatile__ (
        "mrs %0, CPSR\n"
        "orr r1, %0, #0xC0\n" // set I flag to disable IRQ
        "msr CPSR_c, r1\n"
        "and %0, %0, #0xC0\n"
        : "=r"(old_irq) : : "r1"
    );
    return old_irq; // return the flag itself
}

static inline void
sei( uint32_t old_irq )
{
    asm __volatile__ (
        "mrs r1, CPSR\n"
        "bic r1, r1, #0xC0\n"
        "and %0, %0, #0xC0\n"
        "orr r1, r1, %0\n"
        "msr CPSR_c, r1" : : "r"(old_irq) : "r1" );
}

/**
 * Some common instructions.
 * Thanks to ARMada by g3gg0 for some of the black magic :)
 */
#define RET_INSTR    0xe12fff1e    // bx lr
#define FAR_CALL_INSTR    0xe51ff004    // ldr pc, [pc,#-4]
#define LOOP_INSTR    0xeafffffe    // 1: b 1b
#define NOP_INSTR    0xe1a00000    // mov r0, r0
#define MOV_R0_0_INSTR 0xe3a00000
#define MOV_R1_0xC80000_INSTR 0xe3a01732 // mov r1, 0xc80000 
#define MOV_R1_0xC60000_INSTR 0xE3A018C6 // mov r1, 0xc60000 

#define MOV_RD_IMM_INSTR(rd,imm)\
    ( 0xE3A00000 \
    | (rd << 15) \
    )

#define BL_INSTR(pc,dest) \
    ( 0xEB000000 \
    | ((( ((uint32_t)dest) - ((uint32_t)pc) - 8 ) >> 2) & 0x00FFFFFF) \
    )

#define B_INSTR(pc,dest) \
    ( 0xEA000000 \
    | ((( ((uint32_t)dest) - ((uint32_t)pc) - 8 ) >> 2) & 0x00FFFFFF) \
    )

#define ROR(val,count)   (ROR32(val,(count)%32))
#define ROR32(val,count) (((val) >> (count))|((val) << (32-(count))))

/** Simple boot loader memcpy.
 *
 * \note This is not general purpose; len must be > 0 and must be % 4
 */
static inline void
blob_memcpy(
    void *        dest_v,
    const void *    src_v,
    const void *    end
)
{
    uint32_t *    dest = dest_v;
    const uint32_t * src = src_v;
    const uint32_t len = ((const uint32_t*) end) - src;
    uint32_t i;

    for( i=0 ; i<len ; i++ )
        dest[i] = src[i];
}

#endif
