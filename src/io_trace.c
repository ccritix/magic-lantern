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

/* select the MMIO range to be logged: */

#ifdef CONFIG_DIGIC_VI
    /* DIGIC 6 (Cortex R4):
     * DRBAR, mask 0xFFFFFFE0: base address
     *  DRSR, mask 0x0000003E: region size (min. 4096, power of 2)
     *  DRSR, mask 0x00000001: enabled (must be 1)
     *  DRSR, mask 0x0000FF00: sub-region disable bits (unused)
     * Cortex R4 TRM, 4.3.20 c6, MPU memory region programming registers
     */
    #define REGION_BASE(base) (base & 0xFFFFFFE0)
    #define REGION_SIZE(size) ((LOG2(size/2) << 1) | 1)
#else
    /* DIGIC V and earlier (ARM946E-S):
     * PRBSn, mask FFFFF000: base address
     *             0000001E: region size (min. 4096, power of 2)
     *             00000001: enabled (must be 1)
     * ARM946E-S TRM, 2.3.9 Register 6, Protection Region Base and Size Registers
     */
    #define REGION(base, size) ((base & 0xFFFFF000) | (LOG2(size/2) << 1) | 1)
#endif

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
 * REGION(0xC0000000, 0x1000000): nearly everything except EEKO? (DIGIC <= 5)
 * REGION(0xC0000000, 0x20000000): everything including EEKO? (DIGIC 5)
 * REGION(0xE0000000, 0x1000): DFE (5D2, 50D); untested
 */

#ifdef CONFIG_DIGIC_VI

static ASM_VAR uint32_t protected_region_base = REGION_BASE(0xD0000000);
static ASM_VAR uint32_t protected_region_size = REGION_SIZE(0x10000000);

#else /* DIGIC V and earlier */

static ASM_VAR uint32_t protected_region = REGION(0xC0000000,
                                                  0x20000000);

#endif

static uint32_t trap_orig = 0;

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
 *
 * 80D:
 * 00000000-00002BFF ATCM, used by Canon code
 * 00002C00-00003FFF ATCM, unused
 * 80000000-8000A4BF BTCM, used by Canon code
 * 8000A4C0-8000FFFF BTCM, unused
 *
 * 5D4:
 * 00000000-0000338F ATCM, used by Canon code
 * 00003390-00003FFF ATCM, unused
 * 80000000-80009C7F BTCM, used by Canon code
 * 80009C80-8000FFFF BTCM, unused
 *
 * 750D/760D:
 * 00000000-00003D4F ATCM, used by Canon code
 * 00003D50-00003FFF ATCM, unused
 * 80000000-8000923F BTCM, used by Canon code
 * 80009240-8000FFFF BTCM, unused
 *
 */

static void __attribute__ ((naked)) trap()
{
    /* data abort exception occurred. switch stacks, log the access,
     * enable permissions and re-execute trapping instruction */
    asm(
        /* set up a stack in some unused area in the TCM (we need 56 bytes) */
#ifdef CONFIG_DIGIC_VI
        "MOV    SP,     #0x0000FF00\n"
        "ORR    SP, SP, #0x80000000\n"
#else /* DIGIC V and earlier */
        "MOV    SP, #0x740\n"
#endif

        /* save context */
        "STMFD  SP!, {R0-R12, LR}\n"

        /* retrieve PC where the exception happened */
        "SUB    R5, LR, #8\n"

        /* disable our memory protection region for re-execution */
        /* caveat: it will be permanently disabled for further MMIO events */
        "MOV    R0, #0x00\n"
#ifdef CONFIG_DIGIC_VI
        "MOV    R7, #0x07\n"
        "MCR    p15, 0, R7, c6, c2, 0\n"    /* write RGNR (adjust memory region #7) */
        "MCR    p15, 0, R0, c6, c1, 2\n"    /* enable full access to memory (disable region #7) */
#else /* DIGIC V and earlier */
        "MCR    p15, 0, R0, c6, c7, 0\n"    /* enable full access to memory */
#endif

#ifdef CONFIG_QEMU
        "LDR    R3, =0xCF123010\n"          /* disassemble the trapped instruction */
        "ORR    R5, R5, #1\n"               /* ... as Thumb code (comment out for ARM code) */
        "STR    R5, [R3]\n"
#endif

        /* restore context */
        "LDMFD  SP!, {R0-R12, LR}\n"

        /* retry the trapped instruction */
        "SUBS   PC, LR, #8\n"
    );
}

#define TRAP_INSTALLED (MEM(0x0000002C) == (uint32_t) &trap)

void io_trace_uninstall()
{
#if 0
    ASSERT(buffer);
#endif
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
#ifdef CONFIG_DIGIC_VI
        "MOV    R0, #0x07\n"
        "MCR    p15, 0, R0, c6, c2, 0\n"    /* write RGNR (adjust memory region #7) */
        "MOV    R0, #0x00\n"
        "MCR    p15, 0, R0, c6, c1, 2\n"    /* enable full access to memory (disable region #7) */
#else /* DIGIC V and earlier */
        "MOV     R0, #0x00\n"
        "MCR     p15, 0, r0, c6, c7, 0\n"
#endif
        ::: "r0"
    );

    sync_caches();

    sei(int_status);
}

/* to be called after uninstallation, to free the buffer */
void io_trace_cleanup()
{
#if 0
    ASSERT(buffer);
    ASSERT(!TRAP_INSTALLED);

    if (TRAP_INSTALLED)
    {
        /* called at the wrong moment; don't do more damage */
        return;
    }

    free(buffer);
    buffer = 0;
#endif
}

void io_trace_prepare()
{
#if 0
    extern int ml_started;
    if (!ml_started)
    {
        qprintf("[io_trace] FIXME: large allocators not available\n");
        return;
    }

    qprintf("[io_trace] allocating memory...\n");

    /* allocate RAM */
    buffer_index = 0;
    buffer_count = 4*1024*1024;
    int alloc_size = (buffer_count + RECORD_SIZE) * sizeof(buffer[0]);
    ASSERT(!buffer);
    buffer = malloc(alloc_size);
    if (!buffer) return;
    memset(buffer, 0, alloc_size);
#endif
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
#ifdef CONFIG_DIGIC_VI

        /* enable memory protection */
        "MOV    R7, #0x07\n"
        "MCR    p15, 0, R7, c6, c2, 0\n"        /* write RGNR (adjust memory region #7) */
        "LDR    R0, protected_region_base\n"
        "MCR    p15, 0, R0, c6, c1, 0\n"        /* write DRBAR (base address) */
        "LDR    R0, protected_region_size\n"
        "MCR    p15, 0, R0, c6, c1, 2\n"        /* write DRSR (size and enable register) */
        "MOV    R0, #0x005\n"                   /* like 0xC0000000, but with AP bits disabled */
        "MCR    p15, 0, R0, c6, c1, 4\n"        /* write DRACR (access control register) */
        : : : "r7"

#else /* DIGIC V and earlier */

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
#endif
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

uint32_t io_trace_log_get_index()
{
#if 0
    return buffer_index / RECORD_SIZE;
#endif
}

uint32_t io_trace_log_get_nmax()
{
#if 0
    return buffer_count / RECORD_SIZE;
#endif
}

static uint32_t ror(uint32_t word, uint32_t count)
{
    return word >> count | word << (32 - count);
}

int io_trace_log_message(uint32_t msg_index, char * msg_buffer, int msg_size)
{
#if 0
    uint32_t i = msg_index * RECORD_SIZE;
    if (i >= buffer_index) return 0;

    uint32_t pc = buffer[i];
    uint32_t insn = MEM(pc);
    uint32_t Rn = buffer[i+3];
    uint32_t Rm = buffer[i+4];
    uint32_t Rd = buffer[i+5];
    char *   task_name = (char *) buffer[i+1];
    uint32_t interrupt = buffer[i+2];
    uint32_t us_timer = buffer[i+6];

    uint32_t is_ldr = insn & (1 << 20);
    uint32_t offset = 0;
    char raw[48] = "";

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
        snprintf(raw, sizeof(raw), " (%08X, Rn=%08X, Rm=%08X)", insn, Rn, Rm);
        qprintf("%x: %x ???\n", pc, insn);
    }

    /* all of the above may use the sign bit */
    if (!(insn & (1 << 23)))
    {
        offset = -offset;
    }

    char msg[128];
    snprintf(msg, sizeof(msg),
        "[0x%08X] %s 0x%08X%s",
        Rn + offset,            /* Rn - MMIO register (to be interpreted after disassembling the instruction) */
        is_ldr ? "->" : "<-",   /* assume LDR or STR; can you find a counterexample? */
        Rd,                     /* Rd - destination register (MMIO register value) */
        raw                     /* optional raw values */
    );

    /* we don't store this in the "blockchain", so we don't really need block_size */
    struct debug_msg dm = {
        .msg            = msg,
        .class_name     = "MMIO",
        .us_timer       = us_timer,
        .pc             = pc,
        .task_name      = task_name,
        .interrupt      = interrupt,
    };

    int len = debug_format_msg(&dm, msg_buffer, msg_size);

    if (buffer[buffer_count - RECORD_SIZE])
    {
        /* index wrapped around? */
        printf("[MMIO] warning: buffer full\n");
        len += snprintf(msg_buffer + len, msg_size - len,
            "[MMIO] warning: buffer full\n");
        buffer[buffer_count - RECORD_SIZE] = 0;
    }

    return len;
#endif
}

static inline void local_io_trace_pause()
{
#if 0
    asm volatile (
        /* enable full access to memory */
        "MOV     r4, #0x00\n"
        "MCR     p15, 0, r4, c6, c7, 0\n"
        ::: "r4"
    );
#endif
}

static inline void local_io_trace_resume()
{
#if 0
    asm volatile (
        /* re-enable memory protection */
        "LDR    R4, protected_region\n"
        "MCR    p15, 0, r4, c6, c7, 0\n"
        ::: "r4"
    );
#endif
}

/* public wrappers */
void io_trace_pause()
{
    uint32_t old = cli();
    if (TRAP_INSTALLED)
    {
        local_io_trace_pause();
    }
    sei(old);
}

void io_trace_resume()
{
    uint32_t old = cli();
    if (TRAP_INSTALLED)
    {
        local_io_trace_resume();
    }
    sei(old);
}

/* get timer value without logging it as MMIO access */
uint32_t io_trace_get_timer()
{
    uint32_t old = cli();

    if (!TRAP_INSTALLED)
    {
        uint32_t timer = MEM(0xC0242014);
        sei(old);
        return timer;
    }

    local_io_trace_pause();
    uint32_t timer = MEM(0xC0242014);
    local_io_trace_resume();

    sei(old);
    return timer;
}
