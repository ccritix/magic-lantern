/**
 * Adapted from io_trace.
 * https://bitbucket.org/hudson/magic-lantern/src/io_trace_full/src/io_trace.c
 * 
 * This installs a data abort handler in the MMIO region
 * (a subset of C0000000 - CFFFFFFF) and logs all reads/writes from this range.
 * 
 * It forwards each MMIO access to a C callback function.
 */

#define ASM_VAR  __attribute__((section(".text"))) __attribute__((used))

/* log all ENGIO registers (not the entire MMIO range) */
#define REGION(base, size) ((base & 0xFFFFF000) | (LOG2(size/2) << 1) | 1)
#define PROTECTED_REGION REGION(0xC0F00000, 0x100000)   /* FIXME: hardcoded in ASM */

static uint32_t trap_orig = 0;

/* handler called for each register */
static void mmio_handler(uint32_t pc, uint32_t * regs) __attribute__((used));

/* data abort trap */
static void __attribute__ ((naked)) trap()
{
    /* data abort exception occurred. switch stacks, log the access,
     * enable permissions and re-execute trapping instruction */
    asm(
        /* set up a stack in some unused area in the TCM */
        "MOV    SP, #0xA00\n"

        /* save context, including flags */
        "STMFD  SP!, {R0-R12, LR}\n"
        "MRS    R0, CPSR\n"
        "STMFD  SP!, {R0}\n"

        /* prepare to re-execute the old instruction */
        /* copy it into this routine (cacheable memory) */
        "SUB    R5, LR, #8\n"           /* retrieve PC where the exception happened */
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

        /* call the handler function */
        "MOV    R0, R5\n"                   /* PC */
        "ADD    R1, SP, #4\n"               /* R0-R12,LR */
        "BL     mmio_handler\n"

        /* restore context */
        /* FIXME: some instructions may change LR; this will give incorrect result, but at least it appears not to crash */
        /* 5D3 113: 0x1C370 LDMIA R1!, {R3,R4,R12,LR} on REGION(0xC0F19000, 0x001000) when entering LiveView */
        "LDMFD  SP!, {R0}\n"
        "MSR    CPSR_f, R0\n"
        "LDMFD  SP!, {R0-R12, LR}\n"
        "STMFD  SP!, {LR}\n"

        /* placeholder for executing the old instruction */
        "trapped_instruction:\n"
        ".word 0x00000000\n"

        /* save context once again */
        "LDMFD  SP!, {LR}\n"
        "STMFD  SP!, {R0}\n"

        /* re-enable memory protection */
      //"LDR    R0, =0xC0F00027\n"          /* fixme: how to pass PROTECTED_REGION ? */
        "MOV    R0, #0xC0000000\n"
        "ORR    R0, #0x00F00000\n"
        "ORR    R0, #0x00000027\n"
        "MCR    p15, 0, R0, c6, c7, 0\n"

        /* restore context */
        "LDMFD  SP!, {R0}\n"

        /* continue the execution after the trapped instruction */
        "SUBS   PC, LR, #4\n"
    );
}

#define TRAP_INSTALLED (MEM(0x0000002C) == (uint32_t) &trap)

static void io_trace_uninstall()
{
    ASSERT(trap_orig);
    ASSERT(TRAP_INSTALLED);

    if (!TRAP_INSTALLED)
    {
        /* not installed, nothing to do */
        return;
    }

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

static void io_trace_install()
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
      //"LDR    R0, =0xC0F00027\n"          /* fixme: how to pass PROTECTED_REGION ? */
        "MOV    R0, #0xC0000000\n"
        "ORR    R0, #0x00F00000\n"
        "ORR    R0, #0x00000027\n"
        "mcr    p15, 0, r0, c6, c7, 0\n"

        : : : "r4"
    );

    sync_caches();

    sei(int_status);

    qprintf("[io_trace] installed.\n");
}

static uint32_t ror(uint32_t word, uint32_t count)
{
    return word >> count | word << (32 - count);
}

static int decode_mmio_str(uint32_t pc, uint32_t * regs, uint32_t * mmio_reg, uint32_t * mmio_val, uint32_t * out_Rd)
{
    uint32_t insn = MEM(pc);

    uint32_t Rn = regs[(insn >> 16) & 0xF];
    uint32_t Rm = regs[(insn      ) & 0xF];
    uint32_t Rd = regs[(insn >> 12) & 0xF];
    *out_Rd = (insn >> 12) & 0xF;

    uint32_t is_ldr = insn & (1 << 20);
    uint32_t offset = 0;

    if (is_ldr)
    {
        return 0;
    }

    if ((insn & 0x0F000000) == 0x05000000)
    {
        /* ARM ARM:
         * A5.2.2 Load and Store Word or Unsigned Byte - Immediate offset
         * A5.2.5 Load and Store Word or Unsigned Byte - Immediate pre-indexed
         */

        offset = (insn & 0xFFF);
    }
    else if ((insn & 0x0D200000) == 0x04000000)
    {
        /* A5.2.8  Load and Store Word or Unsigned Byte - Immediate post-indexed
         * A5.2.9  Load and Store Word or Unsigned Byte - Register post-indexed
         * A5.2.10 Load and Store Word or Unsigned Byte - Scaled register post-indexed
         */
        offset = 0;
    }
    else if ((insn & 0x0F000000) == 0x07000000)
    {
        /* A5.2.3 Load and Store Word or Unsigned Byte - Register offset
         * A5.2.4 Load and Store Word or Unsigned Byte - Scaled register offset
         * A5.2.6 Load and Store Word or Unsigned Byte - Register pre-indexed
         * A5.2.7 Load and Store Word or Unsigned Byte - Scaled register pre-indexed
         */
        uint32_t shift = (insn >> 5) & 0x3;
        uint32_t shift_imm = (insn >> 7) & 0x1F;

        /* index == offset */
        switch (shift)
        {
            case 0b00:  /* LSL */
                offset = Rm << shift_imm;
                qprintf("%x: %x [Rn, Rm, LSL#%d] => %x\n", pc, insn, shift_imm, offset);
                break;
            case 0b01:  /* LSR */
                offset = Rm >> (shift_imm ? shift_imm : 32);
                qprintf("%x: %x [Rn, Rm, LSR#%d] => %x\n", pc, insn, shift_imm, offset);
                break;
            case 0b10:  /* ASR */
                offset = (int32_t) Rm >> (shift_imm ? shift_imm : 32);
                qprintf("%x: %x [Rn, Rm, ASR#%d] => %x\n", pc, insn, shift_imm, offset);
                break;
            case 0b11:  /* ROR or RRX */
                offset = (shift_imm) ? ror(Rm, shift_imm) : 0 /* FIXME: C not saved; important? */;
                qprintf("%x: %x [Rn, Rm, ROR#%d] => %x\n", pc, insn, shift_imm, offset);
                ASSERT(shift_imm);
                break;
        }
    }
    else if ((insn & 0x0F600F90) == 0x01000090)
    {
        /* A5.3.3 Miscellaneous Loads and Stores - Register offset */
        offset = Rm;
    }
    else
    {
        /* unhandled case; print raw values to assist troubleshooting */
        //snprintf(raw, sizeof(raw), " (%08X, Rn=%08X, Rm=%08X)", insn, Rn, Rm);
        qprintf("%x: %x ???\n", pc, insn);
        return 0;
    }

    /* all of the above may use the sign bit */
    if (!(insn & (1 << 23)))
    {
        offset = -offset;
    }

    *mmio_reg = Rn + offset;
    *mmio_val = Rd;
    return 1;
}
