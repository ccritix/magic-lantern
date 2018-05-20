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
 * REGION(0xC0F00000, 0x010000): EDMAC #0-15 and many others
 * REGION(0xC0F00000, 0x100000): EDMAC (all), display (C0F14) and many others (more likely to crash)
 * REGION(0xC0C00000, 0x100000): SDIO/SFIO (also paired with SDDMA/SFDMA, unsure whether they can be logged at the same time)
 * REGION(0xC0500000, 0x100000): SDDMA/SFDMA/CFDMA
 * REGION(0xC0E20000, 0x010000): JPCORE (JP57, lossless)
 * REGION(0xC0000000, 0x1000000): everything except EEKO? (DIGIC <= 5)
 */
static ASM_VAR uint32_t protected_region = REGION(0xC0220000, 0x010000);

/* number of 32-bit integers recorded for one MMIO event (power of 2) */
#define RECORD_SIZE 8

static uint32_t trap_orig = 0;

/* buffer size must be power of 2 and multiple of RECORD_SIZE, to simplify bounds checking */
static uint32_t buffer[4096] __attribute__((used)) = {0};
static ASM_VAR uint32_t buffer_index = 0;

/*
 * TCM usage in main firmware
 * ==========================
 *
 * checked 60D, 500D, 5D2, 50D, 1300D, 5D3, 70D, 700D, 100D, EOSM in QEMU
 * procedure:
 *   - fill both TCMs with 0xBADCAFFE when PC reaches F8010000 or F80C0000
 *     (doable from either QEMU dbi/logging.c or GDB breakpoint)
 *   - confirm that it still boots the GUI and you can navigate Canon menus
 *   - examine TCM contents with 'x/1024 0' and 'x/1024 0x40000000' in GDB
 *
 * DIGIC 4:
 * 400006F8-40000AF7 ISR handler table (256 entries?)
 * 40000AF8-40000EF7 ISR argument table
 * 40000000-400006F7 possibly unused (no code reads/writes it in QEMU)
 * 40000EF8-40000FFF possibly unused
 *
 * DIGIC 5 and 1300D:
 * 40000000-400007FF ISR handler table (512 entries, some unused?)
 * 40000800-40000FFF ISR argument table
 * 40000E00-40000FFF possibly unused ISR entries? probably best not to rely on it
 *
 * DIGIC 4 and 5:
 * 00000000-00000020 exception vectors (0x18 = IRQ, 0x10 = Data Abort etc)
 * 00000020-00000040 exception routines (used with LDR PC, [PC, #20])
 * 00000120-000001BF some executable code (60D: copied from FF055CA8; 1300D,100D: 00000100-000001BF; 70D: not present)
 * 000004B0-000006C0 interrupt handler (code jumps there from 0x18; end address may vary slightly)
 * 000006C4-00001000 interrupt stack (start address may vary slightly; used until about 0xf00)
 *
 * other exception stacks are at UND:400007fc FIQ:4000077c ABT:400007bc SYS/USR:4000057c (IRQ was 4000067c)
 * these were set up from bootloader, but they are no longer valid in main firmware
 * pick SP at the bottom of the interrupt stack, roughly 0x700-0x740
 */

static void __attribute__ ((naked)) trap()
{
    /* data abort exception occurred. switch stacks, log the access,
     * enable permissions and re-execute trapping instruction */
    asm(
        /* set up a stack in some unused area in the TCM (we need 56 bytes) */
        "MOV    SP, #0x740\n"

        /* save context */
        "STMFD  SP!, {R0-R12, LR}\n"

        /* prepare to save information about trapping code */
        "LDR    R4, =buffer\n"          /* load buffer address */
        "LDR    R2, buffer_index\n"     /* load buffer index from memory */
        "BIC    R2, #0x000FF000\n"      /* avoid writing outside array bounds (mask should match buffer size) */

        /* store the program counter */
        "SUB    R5, LR, #8\n"           /* retrieve PC where the exception happened */
        "STR    R5, [R4, R2, LSL#2]\n"  /* store PC at index [0] */
        "ADD    R2, #1\n"               /* increment index */

#ifdef CONFIG_QEMU
        /* disassemble the instruction */
        "LDR    R3, =0xCF123010\n"
        "STR    R0, [R3]\n"
#endif

        /* get and store DryOS task name and interrupt ID */
        "LDR    R0, =current_task\n"
        "LDR    R1, [R0, #4]\n"         /* 1 if running in interrupt, 0 otherwise; other values? */
        "LDR    R0, [R0]\n"
        "LDR    R0, [R0, #0x24]\n"

        "STR    R0, [R4, R2, LSL#2]\n"  /* store task name at index [1] */
        "ADD    R2, #1\n"               /* increment index */

        "LDR    R0, =current_interrupt\n"
        "LDR    R0, [R0]\n"             /* interrupt ID is shifted by 2 */
        "ORR    R0, R1\n"               /* store whether the interrupt ID is valid, in the LSB */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store interrupt ID at index [2] */
        "ADD    R2, #1\n"               /* increment index */

        /* prepare to re-execute the old instruction */
        /* copy it into this routine (cacheable memory) */
        "LDR    R6, [R5]\n"
        "ADR    R1, trapped_instruction\n"
        "STR    R6, [R1]\n"

        /* clean the cache for this address (without touching the cache hacks),
         * then disable our memory protection region temporarily for re-execution */
        "MOV    R0, #0x00\n"
        "MCR    p15, 0, R1, c7, c10, 1\n"   /* first clean that address in dcache */
        "MCR    p15, 0, R0, c7, c10, 4\n"   /* then drain write buffer */
        "MCR    p15, 0, R1, c7, c5, 1\n"    /* flush icache line for that address */
        "MCR    p15, 0, R0, c6, c7, 0\n"    /* enable full access to memory */

        /* find the source and destination registers */
        "MOV    R1, R6, LSR#12\n"       /* extract destination register */
        "AND    R1, #0xF\n"
        "STR    R1, destination\n"      /* store it to memory; will use it later */

        "MOV    R1, R6, LSR#16\n"       /* extract source register */
        "AND    R1, #0xF\n"
        "LDR    R0, [SP, R1, LSL#2]\n"  /* load source register contents from stack */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store source register at index [3] */
        "ADD    R2, #1\n"               /* increment index */

        "STR    R2, buffer_index\n"     /* store buffer index to memory */

        /* restore context */
        /* FIXME: some instructions may change LR; this will give incorrect result, but at least it appears not to crash */
        /* 5D3 113: 0x1C370 LDMIA R1!, {R3,R4,R12,LR} on REGION(0xC0F19000, 0x001000) when entering LiveView */
        "LDMFD  SP!, {R0-R12, LR}\n"
        "STMFD  SP!, {LR}\n"

        /* placeholder for executing the old instruction */
        "trapped_instruction:\n"
        ".word 0x00000000\n"

        /* save context once again */
        "LDMFD  SP!, {LR}\n"
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

        /* store the result (value read from / written to MMIO) */
        "LDR    R4, =buffer\n"          /* load buffer address */
        "LDR    R2, buffer_index\n"     /* load buffer index from memory */
        "LDR    R0, destination\n"      /* load destination register index from memory */
        "LDR    R0, [SP, R0, LSL#2]\n"  /* load destination register contents from stack */
        "STR    R0, [R4, R2, LSL#2]\n"  /* store destination register at index [4] */
        "ADD    R2, #1\n"               /* increment index */

        /* timestamp the event (requires MMIO access; do this before re-enabling memory protection) */
        "LDR    R0, =0xC0242014\n"      /* 20-bit microsecond timer */
        "LDR    R0, [R0]\n"
        "STR    R0, [R4, R2, LSL#2]\n"  /* store timestamp at index[5]; will be unwrapped in post */
        "ADD    R2, #3\n"               /* increment index (total 8 = RECORD_SIZE, two positions reserved for future use) */
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

static const char * interrupt_name(int i)
{
    static char name[] = "INT-00h";
    int i0 = (i & 0xF);
    int i1 = (i >> 4) & 0xF;
    int i2 = (i >> 8) & 0xF;
    name[3] = i2 ? '0' + i2 : '-';
    name[4] = i1 < 10 ? '0' + i1 : 'A' + i1 - 10;
    name[5] = i0 < 10 ? '0' + i0 : 'A' + i0 - 10;
    return name;
}

void io_trace_log_flush()
{
    uint32_t old = cli();

    for (uint32_t i = 0; i < buffer_index; i += RECORD_SIZE)
    {
        uint32_t pc = buffer[i];
        uint32_t insn = MEM(pc);
        uint32_t Rn = buffer[i+3];
        uint32_t Rd = buffer[i+4];
        char *   task_name = (char *) buffer[i+1];
        uint32_t interrupt = buffer[i+2];
        uint32_t timestamp = buffer[i+5];

        uint32_t is_ldr = insn & (1 << 20);
        uint32_t offset = 0;
        if ((insn & 0x0F200000) == 0x05000000)
        {
            /* ARM ARM: A5.2.2 Load and Store Word or Unsigned Byte - Immediate offset */
            offset = (insn & 0xFFF);
            if (!(insn & (1 << 23)))
            {
                offset = -offset;
            }
        }
        
        debug_loghex2(timestamp, 5);
        debug_logstr("> ");
        if (buffer[i+2] & 1)
        {
            /* interrupt */
            int interrupt_id = interrupt >> 2;
            debug_logstr("**");
            debug_logstr(interrupt_name(interrupt_id));
            debug_logstr("*");
        }
        else
        {
            /* task name, padded */
            char task_nam[] = "          ";
            int pad = strlen(task_nam) - strlen(task_name);
            pad = (pad > 0) ? pad : 0;
            memcpy(task_nam + pad, task_name, sizeof(task_nam) - pad);
            task_nam[sizeof(task_nam)-1] = 0;
            debug_logstr(task_nam);
        }
        debug_logstr(":");
        debug_loghex(pc);           /* PC */
        debug_logstr(":MMIO:  [0x");
        debug_loghex(Rn + offset);  /* Rn - MMIO register (to be interpreted after disassembling the instruction) */
        debug_logstr(is_ldr ? "] -> 0x" : "] <- 0x");   /* assume LDR or STR; can you find a counterexample? */
        debug_loghex(Rd);           /* Rd - destination register (MMIO register value) */
        if ((insn & 0xFFF) && !offset)
        {
            /* likely unhandled case; print raw values */
            debug_logstr(" (");
            debug_loghex(insn);         /* instruction */
            debug_logstr(",");
            debug_loghex(Rn);           /* raw Rn */
            debug_logstr(")");
        }
        debug_logstr("\n");
    }

    if (buffer_index > COUNT(buffer) / 2 || buffer[COUNT(buffer) - RECORD_SIZE])
    {
        /* warn if our buffer is too small */
        debug_logstr("[MMIO] Used: ");
        debug_loghex(buffer_index);
        debug_logstr("/");
        debug_loghex(COUNT(buffer));
        for (int j = COUNT(buffer) - RECORD_SIZE; j >= 0; j -= RECORD_SIZE)
        {
            if (buffer[j])
            {
                debug_logstr("; peak: ");
                debug_loghex(j + RECORD_SIZE);
                break;
            }
        }
        debug_logstr("\n");
    }

    if (buffer[COUNT(buffer) - RECORD_SIZE])
    {
        debug_logstr("[MMIO] WARNING: lost data (try increasing buffer size)\n");
        buffer[COUNT(buffer) - RECORD_SIZE] = 0;
    }

    buffer_index = 0;

    sei(old);
}
