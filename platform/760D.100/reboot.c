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
    /* https://chdk.setepontos.com/index.php?topic=11316.msg111290#msg111290
     * https://chdk.setepontos.com/index.php?topic=1493.msg13469#msg13469
     * 7D2 LED is at 0xd20b0c34 (i=269).
     */
    while (1)
    {
        for (int i = 0; i < 512; i++)
        {
            *(volatile int*)(0xd20b0800 + i * 4) = 0x4c0003;
        }

        busy_wait(n);

        for (int i = 0; i < 512; i++)
        {
            *(volatile int*)(0xd20b0800 + i * 4) = 0x4d0002;
        }

        busy_wait(n);
    }
}

/* read/write a uint32_t from memory */
#define MEM(x) *(volatile uint32_t*)(x)

void
__attribute__((noreturn))
__attribute__ ((section(".dump_asm")))
cstart( void )
{
    /* uncomment this to try blinking the LED */
    //~ blink(10);
    
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
