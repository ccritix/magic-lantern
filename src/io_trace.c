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
 *      0000001E: region size
 *      00000001: enabled
 * ARM946E-S TRM, 2.3.9 Register 6, Protection Region Base and Size Registers
 */
static ASM_VAR uint32_t protected_region = 0xC022001F;

static ASM_VAR uint32_t irq_end_part_calls;
static ASM_VAR uint32_t irq_end_full_calls;
static ASM_VAR uint32_t irq_entry_calls;
static ASM_VAR uint32_t irq_depth;
static ASM_VAR uint32_t irq_orig;

static uint32_t trap_orig = 0;
static uint32_t irq_end_full_addr = 0;
static uint32_t irq_end_part_addr = 0;
static uint32_t hook_full = 0;
static uint32_t hook_part = 0;

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

static void __attribute__ ((naked)) irq_entry()
{
    /* irq code is being called. disable protection. */
    asm(
        /* first save scratch register to buffer */
        "str    r0, irq_tmp\n"

        /* enable full access to memory as the IRQ handler relies on it */
        "mov    r0, #0x00\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"

        /* update some counters */
        "ldr    r0, irq_entry_calls\n"
        "add    r0, #0x01\n"
        "str    r0, irq_entry_calls\n"
        "ldr    r0, irq_depth\n"
        "add    r0, #0x01\n"
        "str    r0, irq_depth\n"

        /* now restore scratch and jump to IRQ handler */
        "ldr    r0, irq_tmp\n"
        "ldr    pc, irq_orig\n"

        "irq_tmp:\n"
        ".word 0x00000000\n"
    );
}

static void __attribute__ ((naked)) irq_end_full()
{
    asm(
        /* save int flags and disable all interrupts */
        "mrs    r2, CPSR\n"
        "orr    r1, r2, #0xC0\n"
        "msr    CPSR_c, r1\n"
        "and    r2, r2, #0xC0\n"

        /* update some counters */
        "ldr    r0, irq_end_full_calls\n"
        "add    r0, #0x01\n"
        "str    r0, irq_end_full_calls\n"
        "ldr    r0, irq_depth\n"
        "sub    r0, #0x01\n"
        "str    r0, irq_depth\n"

        "cmp    r0, #0x00\n"
        "bne    irq_end_full_nested\n"

        /* enable memory protection again */
        "ldr    r0, protected_region\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"

        "irq_end_full_nested:\n"

        /* restore int flags */
        "mrs    r1, CPSR\n"
        "bic    r1, r1, #0xC0\n"
        "and    r2, r2, #0xC0\n"
        "orr    r1, r1, r2\n"
        "msr    CPSR_c, r1\n"

        "LDMFD  SP!, {R0-R12,LR,PC}^\n"
    );
}

static void __attribute__ ((naked)) irq_end_part()
{
    asm(
        /* save int flags and disable all interrupts */
        "mrs    r2, CPSR\n"
        "orr    r1, r2, #0xC0\n"
        "msr    CPSR_c, r1\n"
        "and    r2, r2, #0xC0\n"

        /* update some counters */
        "ldr    r0, irq_end_part_calls\n"
        "add    r0, #0x01\n"
        "str    r0, irq_end_part_calls\n"
        "ldr    r0, irq_depth\n"
        "sub    r0, #0x01\n"
        "str    r0, irq_depth\n"

        "cmp    r0, #0x00\n"
        "bne    irq_end_part_nested\n"

        /* enable memory protection again */
        "ldr    r0, protected_region\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"

        "irq_end_part_nested:\n"

        /* restore int flags */
        "mrs    r1, CPSR\n"
        "bic    r1, r1, #0xC0\n"
        "and    r2, r2, #0xC0\n"
        "orr    r1, r1, r2\n"
        "msr    CPSR_c, r1\n"

        "LDMFD   SP!, {R0-R4,R12,PC}^\n"
    );
}


static uint32_t find_hooks()
{
    uint32_t addr = MEM(0x00000030);

    while(MEM(addr) != 0xEEEEEEEE && addr < 0x1000)
    {
        if(MEM(addr) == 0xE8FDDFFF)
        {
            if(hook_full)
            {
                return 1;
            }
            hook_full = addr;
        }
        if(MEM(addr) == 0xE8FD901F)
        {
            if(hook_part)
            {
                return 2;
            }
            hook_part = addr;
        }
        addr += 4;
    }

    /* failed to find irq stack head */
    if(addr >= 0x1000)
    {
        /* use free space in IV for our pointers. vectors RESET and BKPT are not used. */
        irq_end_full_addr = 0x00;
        irq_end_part_addr = 0x14;
    }
    else
    {
        /* leave some space to prevent false stack overflow alarms (if someone ever checked...) */
        irq_end_full_addr = addr + 0x100;
        irq_end_part_addr = addr + 0x104;
    }

    if(hook_full && hook_part)
    {
        return 0;
    }

    return 4;
}

static void ins_ldr(uint32_t pos, uint32_t dest)
{
    int offset = dest - (pos + 8);

    if(offset >= 0)
    {
        MEM(pos) = 0xE59FF000 | (offset & 0xFFF);
    }
    else
    {
        MEM(pos) = 0xE51FF000 | ((-offset) & 0xFFF);
    }
}


void io_trace_uninstall()
{
    uint32_t int_status = cli();

    /* restore original irq handler */
    MEM(0x00000030) = irq_orig;

    /* unhook os irq handler */
    MEM(hook_full) = 0xE8FDDFFF;
    MEM(hook_part) = 0xE8FD901F;

    /* remove our trap handler */
    MEM(0x0000002C) = (uint32_t)trap_orig;

    /* set range 0x00000000 - 0x00001000 buffer/cache bits */
    asm(
        /* set area cachable */
        "mrc    p15, 0, R4, c2, c0, 0\n"
        "and    r4, #0x7F\n"
        "mcr    p15, 0, R4, c2, c0, 0\n"
        "mrc    p15, 0, R4, c2, c0, 1\n"
        "and    r4, #0x7F\n"
        "mcr    p15, 0, R4, c2, c0, 1\n"

        /* set area bufferable */
        "mrc    p15, 0, R4, c3, c0, 0\n"
        "and    r4, #0x7F\n"
        "mcr    p15, 0, R4, c3, c0, 0\n"

        /* set access permissions */
        "mrc    p15, 0, R4, c5, c0, 2\n"
        "lsl    R4, #0x04\n"
        "lsr    R4, #0x04\n"
        "orr    R4, #0x30000000\n"
        "mcr    p15, 0, R4, c5, c0, 2\n"

        "mrc    p15, 0, R4, c5, c0, 3\n"
        "lsl    R4, #0x04\n"
        "lsr    R4, #0x04\n"
        "orr    R4, #0x30000000\n"
        "mcr    p15, 0, R4, c5, c0, 3\n"

        : : : "r4"
    );

    sync_caches();

    sei(int_status);
}

void io_trace_install()
{
    qprintf("[io_trace] installing...\n");

    uint32_t err = find_hooks();

    if(err)
    {
        qprintf("[io_trace] Failed to find hook points (err %d).\n", err);
        return;
    }

    qprintf("[io_trace] irq_end: %x, %x.\n", irq_end_full_addr, irq_end_part_addr);

    uint32_t int_status = cli();

    /* place jumps to our interrupt end code */
    MEM(irq_end_full_addr) = &irq_end_full;
    MEM(irq_end_part_addr) = &irq_end_part;

    /* install pre-irq hook */
    irq_orig = MEM(0x00000030);
    MEM(0x00000030) = (uint32_t) &irq_entry;

    /* place a LDR PC, [PC, rel_offset] at irq end to jump to our code */
    ins_ldr(hook_full, irq_end_full_addr);
    ins_ldr(hook_part, irq_end_part_addr);

    /* install data abort handler */
    trap_orig = MEM(0x0000002C);
    MEM(0x0000002C) = (uint32_t) &trap;

    /* set buffer/cache bits for the logged region
     * protection will get enabled after next interrupt */
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

        /* set access permissions (disable access in the protected area) */
        "mrc    p15, 0, R4, c5, c0, 2\n"
        "bic    R4, #0xF0000000\n"
        "mcr    p15, 0, R4, c5, c0, 2\n"

        "mrc    p15, 0, R4, c5, c0, 3\n"
        "bic    R4, #0xF0000000\n"
        "mcr    p15, 0, R4, c5, c0, 3\n"

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
