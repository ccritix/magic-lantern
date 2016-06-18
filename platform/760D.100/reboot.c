/** \file
 * 760D dumper
 */

#include "arm-mcr.h"

asm(
    ".text\n"
    ".globl _start\n"
    "_start:\n"

    "MRS     R0, CPSR\n"
    "BIC     R0, R0, #0x3F\n"   // Clear I,F,T
    "ORR     R0, R0, #0xD3\n"   // Set I,T, M=10011 == supervisor
    "MSR     CPSR, R0\n"
    "B       cstart\n"
);

/* read/write a uint32_t from memory */
#define MEM(x) *(volatile uint32_t*)(x)

void
__attribute__((noreturn))
__attribute__ ((section(".dump_asm")))
cstart( void )
{
    // look for main firmware start
    // 7D2: add r0, pc, #4; orr r0, r0, #1; bx r0
    for (uint32_t start = 0xF8000000; start < 0xFFFF0000; start += 4)
    {
        if (MEM(start+0) == 0xE28F0004 &&
            MEM(start+4) == 0xE3800001 &&
            MEM(start+8) == 0xE12FFF10)
        {
            void(*firmware_start)(void) = (void*)start;
            firmware_start();
            
            // unreachable
            while(1);
        }
    }
    
    // should be unreachable
    while(1);
}
