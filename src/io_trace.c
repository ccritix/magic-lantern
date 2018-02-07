/**
 * This installs a data abort handler in the MMIO region
 * (a subset of C0000000 - CFFFFFFF) and logs all reads/writes from this range.
 *
 * These logs are very useful for emulation.
 *
 * Based on mem_prot.
 */

#include <dryos.h>
#include <dm-spy.h>

#define ASM_VAR  __attribute__((section(".text"))) __attribute__((used))

/* select the MMIO range to be logged:
 * mask FFFFF000: base address
 *      0000001E: region size (min. 4096, power of 2)
 *      00000001: enabled (must be 1)
 * ARM946E-S TRM, 2.3.9 Register 6, Protection Region Base and Size Registers
 */
#define REGION(base, size) ((base & 0xFFFFF000) | (LOG2(size/2) << 1) | 1)

/* address ranges can be found at magiclantern.wikia.com/wiki/Register_Map
 * or in QEMU source: contrib/qemu/eos/eos.c, eos_handlers
 * examples:
 * REGION(0xC0220000, 0x010000): GPIO (LEDs, chip select signals etc)
 * REGION(0xC0820000, 0x001000): SIO communication (SPI)
 * REGION(0xC0210000, 0x001000): timers (activity visible only with early startup logging, as in da607f7 or !1dd2792)
 * REGION(0xC0243000, 0x001000): HPTimer (high resolution timers)
 * REGION(0xC0200000, 0x002000): interrupt controller (it works! interrupts are interrupted!)
 * REGION(0xC0F00000, 0x010000): EDMAC #0-15 and many others (may lock up during startup, works fine afterwards)
 * REGION(0xC0F00000, 0x100000): EDMAC (all) and many others (locks up in LiveView, works in play mode)
 * REGION(0xC0C00000, 0x100000): SDIO/SFIO (also paired with SDDMA/SFDMA, unsure whether they can be logged at the same time)
 * REGION(0xC0500000, 0x100000): SDDMA/SFDMA/CFDMA
 */
static ASM_VAR uint32_t protected_region = REGION(0xC0220000, 0x010000);

static ASM_VAR uint32_t irq_orig;

static uint32_t trap_orig = 0;

static uint32_t buffer[1024+8] __attribute__((used));
static ASM_VAR uint32_t buffer_index = 0;

static void __attribute__ ((naked)) trap()
{
    /* data abort exception occurred. switch stacks, log the access,
     * enable permissions and re-execute trapping instruction */
    asm(
        /* save context */
        "STMFD  SP!, {R0-R12, LR}\n"

        /* prepare to save information about trapping code */
        "LDR    R4, =buffer\n"          /* load buffer address */
        "LDR    R2, buffer_index\n"     /* load buffer index from memory */

        /* store the program counter */
        "SUB    R0, LR, #8\n"           /* retrieve PC where the exception happened */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store PC */
        "ADD    R2, #1\n"               /* increment index */

#ifdef CONFIG_QEMU
        /* disassemble the instruction */
        "LDR    R3, =0xCF123010\n"
        "STR    R0, [R3]\n"
#endif

        /* get and store DryOS task name */
        "LDR    R0, =current_task\n"
        "LDR    R0, [R0]\n"
        "LDR    R0, [R0, #0x24]\n"

        "STR    R0, [R4, R2, LSL#2]\n"  /* store task name */
        "ADD    R2, #1\n"               /* increment index */

        /* prepare to re-execute the old instruction */
        "SUB    R0, LR, #8\n"
        "LDR    R0, [R0]\n"
        /* copy it to uncacheable memory - will this work? */
        "LDR    R1, =trapped_instruction\n"
        "ORR    R1, #0x40000000\n"
        "STR    R0, [R1]\n"

        /* find the source and destination registers */
        "MOV    R1, R0, LSR#12\n"       /* extract destination register */
        "AND    R1, #0xF\n"
        "STR    R1, destination\n"      /* store it to memory; will use it later */

        "MOV    R1, R0, LSR#16\n"       /* extract source register */
        "AND    R1, #0xF\n"
        "LDR    R0, [SP, R1, LSL#2]\n"  /* load source register contents from stack */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store source register */
        "ADD    R2, #1\n"               /* increment index */

        "STR    R2, buffer_index\n"     /* store buffer index to memory */

        /* sync caches - unsure */
        "mov    r0, #0\n"
        "mcr    p15, 0, r0, c7, c10, 4\n" // drain write buffer
        "mcr    p15, 0, r0, c7, c5, 0\n" // flush I cache

        /* enable full access to memory */
        "MOV    R0, #0x00\n"
        "MCR    p15, 0, r0, c6, c7, 0\n"

        /* restore context */
        "LDMFD  SP!, {R0-R12, LR}\n"

        /* placeholder for executing the old instruction */
        "trapped_instruction:\n"
        ".word 0x00000000\n"

        /* save context once again */
        "STMFD  SP!, {R0-R12, LR}\n"

#ifdef CONFIG_QEMU
        /* print the register and value loaded from MMIO */
        "LDR    R3, =0xCF123000\n"
        "LDR    R0, destination\n"
        "LDR    R0, [SP, R0, LSL#2]\n"
        "STR    R0, [R3,#0xC]\n"
        "MOV    R0, #10\n"
        "STR    R0, [R3]\n"
#endif

        "LDR    R4, =buffer\n"          /* load buffer address */
        "LDR    R2, buffer_index\n"     /* load buffer index from memory */
        "LDR    R0, destination\n"      /* load destination register index from memory */
        "LDR    R0, [SP, R0, LSL#2]\n"  /* load destination register contents from stack */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store destination register */
        "ADD    R2, #1\n"               /* increment index */
        "BIC    R2, #0x0003FC00\n"      /* avoid writing outside array bounds */
        "STR    R2, buffer_index\n"     /* store buffer index to memory */

        /* re-enable memory protection */
        "LDR    R0, protected_region\n"
        "MCR    p15, 0, R0, c6, c7, 0\n"

        /* restore context */
        "LDMFD  SP!, {R0-R12, LR}\n"

        /* continue the execution after the trapped instruction */
        "SUBS   PC, LR, #4\n"

        /* ------------------------------------------ */

        "destination:\n"
        ".word 0x00000000\n"
    );
}

void io_trace_uninstall()
{
    uint32_t int_status = cli();

    /* remove our trap handler */
    MEM(0x0000002C) = (uint32_t)trap_orig;

    sync_caches();

    asm(
        /* enable full access to memory */
        "MOV     R0, #0x00\n"
        "MCR     p15, 0, r0, c6, c7, 0\n"
    );

    sync_caches();

    sei(int_status);
}

void io_trace_install()
{
    qprintf("[io_trace] installing...\n");

    uint32_t int_status = cli();

    /* install data abort handler */
    trap_orig = MEM(0x0000002C);
    MEM(0x0000002C) = (uint32_t) &trap;

    /* also needed before? */
    sync_caches();

    /* set buffer/cache bits for the logged region */
    asm(
        /* set area uncacheable (already set up that way, but...) */
        "mrc    p15, 0, R4, c2, c0, 0\n"
        "bic    r4, #0x80\n"
        "mcr    p15, 0, R4, c2, c0, 0\n"
        "mrc    p15, 0, R4, c2, c0, 1\n"
        "bic    r4, #0x80\n"
        "mcr    p15, 0, R4, c2, c0, 1\n"

        /* set area non-bufferable (already set up that way, but...) */
        "mrc    p15, 0, R4, c3, c0, 0\n"
        "bic    r4, #0x80\n"
        "mcr    p15, 0, R4, c3, c0, 0\n"

        /* set access permissions (disable data access in the protected area) */
        "mrc    p15, 0, R4, c5, c0, 2\n"
        "bic    R4, #0xF0000000\n"
        "mcr    p15, 0, R4, c5, c0, 2\n"

        /* enable memory protection */
        "ldr    r0, protected_region\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"

        : : : "r4"
    );

    sync_caches();

    sei(int_status);

    qprintf("[io_trace] installed.\n");
}

void io_trace_log_flush()
{
    uint32_t old = cli();

    for (uint32_t i = 0; i < buffer_index; i += 4)
    {
        debug_logstr("MMIO: ");
        debug_logstr((char*)buffer[i+1]);   /* task name */
        debug_logstr(":");
        debug_loghex(buffer[i]);            /* PC */
        debug_logstr(" ");
        debug_loghex(MEM(buffer[i]));       /* instruction */
        debug_logstr(" ");
        debug_loghex(buffer[i+2]);          /* Rn - MMIO register (to be interpreted after disassembling the instruction) */
        debug_logstr(" ");
        debug_loghex(buffer[i+3]);          /* Rd - destination register (MMIO register value) */
        debug_logstr("\n");
    }
    if (buffer_index > COUNT(buffer) / 2)
    {
        /* warn if our buffer is too small */
        debug_logstr("Used: ");
        debug_loghex(buffer_index);
        debug_logstr("/");
        debug_loghex(COUNT(buffer));
        debug_logstr("\n");
    }
    buffer_index = 0;

    sei(old);
}
