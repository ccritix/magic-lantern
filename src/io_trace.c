/**
 * This installs a data abort handler in the MMIO region
 * (a subset of C0000000 - CFFFFFFF) and logs all reads/writes from this range.
 * 
 * These logs are very useful for emulation.
 * 
 * Based on mem_prot.
 */

#include <dryos.h>

/* select the MMIO range to be logged:
 * mask FFFFF000: base address
 *      0000001E: region size
 *      00000001: enabled
 * ARM946E-S TRM, 2.3.9 Register 6, Protection Region Base and Size Registers
 */
extern unsigned int io_trace_protected_region;

#define IO_TRACE_STACK_SIZE 128

extern unsigned int io_trace_irq_end_part_calls;
extern unsigned int io_trace_irq_end_full_calls;
extern unsigned int io_trace_irq_entry_calls;
extern unsigned int io_trace_irq_depth;

extern unsigned int io_trace_trap_count;
extern unsigned int io_trace_trap_addr;
extern unsigned int io_trace_trap_task;

extern unsigned int io_trace_irq_orig;
extern unsigned int io_trace_trap_stackptr;
unsigned int io_trace_trap_stack[IO_TRACE_STACK_SIZE];

unsigned int io_trace_trap_orig = 0;
unsigned int io_trace_irq_end_full_addr = 0;
unsigned int io_trace_irq_end_part_addr = 0;
unsigned int io_trace_hook_full = 0;
unsigned int io_trace_hook_part = 0;

void __attribute__ ((naked)) io_trace_trap()
{
    /* data abort exception occurred. switch stacks, log the access,
     * enable permissions and re-execute trapping instruction */
    asm(
        "STR     SP, io_trace_trap_stackptr_bak\n"
        "LDR     SP, io_trace_trap_stackptr\n"
        
        "STMFD   SP!, {R0-R12, LR}\n"
        
        /* save information about trapping code */
        "LDR     R0, =current_task\n"
        "LDR     R0, [R0]\n"
        "LDR     R0, [R0, #64]\n"               /* task ID */
        "STR     R0, io_trace_trap_task\n"
        "LDR     R0, io_trace_trap_count\n"
        "ADD     R0, #0x01\n"
        "STR     R0, io_trace_trap_count\n"
        "SUB     R0, R14, #8\n"
        "STR     R0, io_trace_trap_addr\n"

#ifdef CONFIG_QEMU
        /* disassemble the instruction */
        "LDR     R1, =0xCF123010\n"
        "STR     R0, [R1]\n"
#endif

        /* enable full access to memory */
        "MOV     R0, #0x00\n"
        "MCR     p15, 0, r0, c6, c7, 0\n"
        
        "LDMFD   SP!, {R0-R12, LR}\n"
        "LDR     SP, io_trace_trap_stackptr_bak\n"
        
        /* execute instruction again */
        "SUBS    PC, R14, #8\n"
        
        /* ------------------------------------------ */
        
        "io_trace_protected_region:\n"
        ".word 0xC0820017\n"
        "io_trace_trap_stackptr:\n"
        ".word 0x00000000\n"
        "io_trace_trap_stackptr_bak:\n"
        ".word 0x00000000\n"
        "io_trace_trap_count:\n"
        ".word 0x00000000\n"
        "io_trace_trap_addr:\n"
        ".word 0x00000000\n"
        "io_trace_trap_task:\n"
        ".word 0x00000000\n"
    );
}

void __attribute__ ((naked)) io_trace_irq_entry()
{
    /* irq code is being called. disable protection. */
    asm(
        /* first save scratch register to buffer */
        "str r0, io_trace_irq_tmp\n"
        
        /* enable full access to memory as the IRQ handler relies on it */
        "mov r0, #0x00\n"
        "mcr p15, 0, r0, c6, c7, 0\n"
        
        /* update some counters */
        "ldr r0, io_trace_irq_entry_calls\n"
        "add r0, #0x01\n"
        "str r0, io_trace_irq_entry_calls\n"
        "ldr r0, io_trace_irq_depth\n"
        "add r0, #0x01\n"
        "str r0, io_trace_irq_depth\n"

        /* now restore scratch and jump to IRQ handler */
        "ldr r0, io_trace_irq_tmp\n"
        "ldr pc, io_trace_irq_orig\n"

        "io_trace_irq_tmp:\n"
        ".word 0x00000000\n"
        "io_trace_irq_orig:\n"
        ".word 0x00000000\n"
        "io_trace_irq_entry_calls:\n"
        ".word 0x00000000\n"
    );
}

void __attribute__ ((naked)) io_trace_irq_end_full()
{
    asm(
        /* save int flags and disable all interrupts */
        "mrs    r2, CPSR\n"
        "orr    r1, r2, #0xC0\n"
        "msr    CPSR_c, r1\n"
        "and    r2, r2, #0xC0\n"
        
        /* update some counters */
        "ldr    r0, io_trace_irq_end_full_calls\n"
        "add    r0, #0x01\n"
        "str    r0, io_trace_irq_end_full_calls\n"
        "ldr    r0, io_trace_irq_depth\n"
        "sub    r0, #0x01\n"
        "str    r0, io_trace_irq_depth\n"
        
        "cmp    r0, #0x00\n"
        "bne    io_trace_irq_end_full_nested\n"
        
        /* enable memory protection again */
        "ldr    r0, io_trace_protected_region\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"
        
        "io_trace_irq_end_full_nested:\n"
        
        /* restore int flags */
        "mrs    r1, CPSR\n"
        "bic    r1, r1, #0xC0\n"
        "and    r2, r2, #0xC0\n"
        "orr    r1, r1, r2\n"
        "msr    CPSR_c, r1\n" 
        
        "LDMFD  SP!, {R0-R12,LR,PC}^\n"
        
        "io_trace_irq_end_full_calls:\n"
        ".word 0x00000000\n"
    );
}

void __attribute__ ((naked)) io_trace_irq_end_part()
{
    asm(
        /* save int flags and disable all interrupts */
        "mrs    r2, CPSR\n"
        "orr    r1, r2, #0xC0\n"
        "msr    CPSR_c, r1\n"
        "and    r2, r2, #0xC0\n"
        
        /* update some counters */
        "ldr    r0, io_trace_irq_end_part_calls\n"
        "add    r0, #0x01\n"
        "str    r0, io_trace_irq_end_part_calls\n"
        "ldr    r0, io_trace_irq_depth\n"
        "sub    r0, #0x01\n"
        "str    r0, io_trace_irq_depth\n"
        
        "cmp    r0, #0x00\n"
        "bne    io_trace_irq_end_part_nested\n"
        
        /* enable memory protection again */
        "ldr    r0, io_trace_protected_region\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"
        
        "io_trace_irq_end_part_nested:\n"
        
        /* restore int flags */
        "mrs    r1, CPSR\n"
        "bic    r1, r1, #0xC0\n"
        "and    r2, r2, #0xC0\n"
        "orr    r1, r1, r2\n"
        "msr    CPSR_c, r1\n" 
        
        "LDMFD   SP!, {R0-R4,R12,PC}^\n"
        
        "io_trace_irq_end_part_calls:\n"
        ".word 0x00000000\n"
        "io_trace_irq_depth:\n"
        ".word 0x00000000\n"
    );
}


unsigned int io_trace_find_hooks()
{
    unsigned int addr = MEM(0x00000030);
    
    while(MEM(addr) != 0xEEEEEEEE && addr < 0x1000)
    {
        if(MEM(addr) == 0xE8FDDFFF)
        {
            if(io_trace_hook_full)
            {
                return 1;
            }
            io_trace_hook_full = addr;
        }
        if(MEM(addr) == 0xE8FD901F)
        {
            if(io_trace_hook_part)
            {
                return 2;
            }
            io_trace_hook_part = addr;
        }
        addr += 4;
    }
    
    /* failed to find irq stack head */
    if(addr >= 0x1000)
    {
        /* use free space in IV for our pointers. vectors RESET and BKPT are not used. */
        io_trace_irq_end_full_addr = 0x00;
        io_trace_irq_end_part_addr = 0x14;
    }
    else
    {
        /* leave some space to prevent false stack overflow alarms (if someone ever checked...) */
        io_trace_irq_end_full_addr = addr + 0x100;
        io_trace_irq_end_part_addr = addr + 0x104;
    }
    
    if(io_trace_hook_full && io_trace_hook_part)
    {
        return 0;
    }
    
    return 4;
}

void io_trace_ins_ldr(unsigned int pos, unsigned int dest)
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
    unsigned int int_status = cli();
    
    /* restore original irq handler */
    MEM(0x00000030) = io_trace_irq_orig;
    
    /* unhook os irq handler */
    MEM(io_trace_hook_full) = 0xE8FDDFFF;
    MEM(io_trace_hook_part) = 0xE8FD901F;
    
    /* remove our trap handler */
    MEM(0x0000002C) = (unsigned int)io_trace_trap_orig;
    
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

    unsigned int err = io_trace_find_hooks();

    if(err)
    {
        qprintf("[io_trace] Failed to find hook points (err %d).\n", err);
        return;
    }

    qprintf("[io_trace] irq_end: %x, %x.\n", io_trace_irq_end_full_addr, io_trace_irq_end_part_addr);

    unsigned int int_status = cli();
    
    /* place jumps to our interrupt end code */
    MEM(io_trace_irq_end_full_addr) = &io_trace_irq_end_full;
    MEM(io_trace_irq_end_part_addr) = &io_trace_irq_end_part;
    
    /* install pre-irq hook */
    io_trace_irq_orig = MEM(0x00000030);
    MEM(0x00000030) = (unsigned int)&io_trace_irq_entry;
    
    /* place a LDR PC, [PC, rel_offset] at irq end to jump to our code */
    io_trace_ins_ldr(io_trace_hook_full, io_trace_irq_end_full_addr);
    io_trace_ins_ldr(io_trace_hook_part, io_trace_irq_end_part_addr);
    
    /* install data abort handler */
    io_trace_trap_orig = MEM(0x0000002C);
    MEM(0x0000002C) = (unsigned int)&io_trace_trap;
    
    /* set up its own stack */
    io_trace_trap_stackptr = (unsigned int)&io_trace_trap_stack[IO_TRACE_STACK_SIZE];
    
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
