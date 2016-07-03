/** \file
 * ROM dumper using LED blinks
 */

#include "arm-mcr.h"
#include "consts.h"

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
    while (1)
    {
#if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
        *(volatile int*)(CARD_LED_ADDRESS) = (LEDON);
        busy_wait(n);
        *(volatile int*)(CARD_LED_ADDRESS) = (LEDOFF);
        busy_wait(n);
#else
    #warning LED address unknown
#endif
    }
}

static void blink_all(int n)
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

/* Attempt to identify the LED address, knowing it is somewhere
 * between 0xd20b0800 and 0xd20b1000 (512 possible MMIO addresses).
 * 
 * On each address, we will send its binary representation (9 bits),
 * all in parallel; I expect only one of those 512 addresses (the LED)
 * will actually blink, so you would just write down the blink sequence
 * (long/short) and identify the LED address.
 *
 * n ... time for short blinks, total cycle time = 6 n,
 * long blinks are 3 n long and pause between blinks is 3 n
 * between two cycles of address blinks we have an additional pause of 4 n
 * 
 * credits: zloe
 */
static void guess_led(int n)
{
    const int N = 512;
    
    while (1)
    {
        for (int bit = 8; bit >= 0; --bit)
        {
            // turn all led on all addresses on
            for (int i = 0; i < N; ++i)
            {
                *(volatile int*)(0xd20b0800 + i * 4) = 0x4c0003;
            }
            
            // sleep short
            busy_wait(n);
            
            // turn off all addresses which have 0 on the currently selected bit
            for (int i = 0; i < N; ++i)
            {
                int shortBlink = (i & (1<<bit)) == 0;
                if (shortBlink)
                {
                    *(volatile int*)(0xd20b0800 + i * 4) = 0x4d0002;
                }
            }

            // additional sleep for the long time period
            // 2 n to more easily distinguish short and long blinks
            busy_wait(2*n);

            // turn led off on all addresses
            for (int i = 0; i < N; ++i)
            {
                *(volatile int*)(0xd20b0800 + i * 4) = 0x4d0002;
            }
            
            // pause between blinking
            busy_wait(3*n);
        }
        
        // pause before we repeat the code
        busy_wait(4*n);
    }
}

/* read/write a uint32_t from memory */
#define MEM(x) *(volatile uint32_t*)(x)


static void try_jumping_to_firmware()
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
}

void
__attribute__((noreturn))
__attribute__ ((section(".dump_asm")))
cstart( void )
{
    /* Try guessing the LED address */
    guess_led(10);
    
    /* Try blinking the LED at the address defined in platform consts.h */
    //~ blink(10);
    
    /* Try blinking a range of addresses, hoping the LED might be there */
    //~ blink_all(10);
    
    /* Try jumping to main firmware from FIR */
    //~ try_jumping_to_firmware();
    
    // should be unreachable
    while(1);
}
