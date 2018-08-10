/** \file
 * Minimal startup code and ROM dumper for DIGIC 7
 */

#include "dryos.h"

/** These are called when new tasks are created */
static void my_init_task(int a, int b, int c, int d);

/** This just goes into the bss */
#define RELOCSIZE 0x1000 // look in HIJACK macros for the highest address, and subtract ROMBASEADDR
static uint8_t _reloc[ RELOCSIZE ];
#define RELOCADDR ((uintptr_t) _reloc)

/** Encoding for the Thumb instructions used for patching the boot code */
#define THUMB_RET_INSTR 0x00004770        /* BX LR */

static inline uint32_t thumb_branch_instr(uint32_t pc, uint32_t dest, uint32_t opcode)
{
    /* thanks atonal */
    //uint32_t offset = dest - ((pc + 4) & ~3); /* according to datasheets, this should be the correct calculation -> ALIGN(PC, 4) */
    uint32_t offset = dest - (pc + 4);  /* this one works for BX */
    uint32_t s = (offset >> 24) & 1;
    uint32_t i1 = (offset >> 23) & 1;
    uint32_t i2 = (offset >> 22) & 1;
    uint32_t imm10 = (offset >> 12) & 0x3ff;
    uint32_t imm11 = (offset >> 1) & 0x7ff;
    uint32_t j1 = (!(i1 ^ s)) & 0x1;
    uint32_t j2 = (!(i2 ^ s)) & 0x1;

    return opcode | (s << 10) | imm10 | (j1 << 29) | (j2 << 27) | (imm11 << 16);
}

#define THUMB_B_W_INSTR(pc,dest)    thumb_branch_instr((uint32_t)(pc), (uint32_t)(dest), 0x9000f000)
#define THUMB_BL_INSTR(pc,dest)     thumb_branch_instr((uint32_t)(pc), (uint32_t)(dest), 0xd000f000)
#define THUMB_BLX_INSTR(pc,dest)    thumb_branch_instr((uint32_t)(pc), (uint32_t)(dest), 0xc000f000)

#define INSTR( addr ) ( *(uint32_t*)( (addr) - ROMBASEADDR + RELOCADDR ) )

/** Fix a branch instruction in the relocated firmware image */
#define FIXUP_BRANCH( rom_addr, dest_addr ) \
    qprint("[BOOT] fixing up branch at "); qprintn((uint32_t) &INSTR( rom_addr )); \
    qprint(" (ROM: "); qprintn(rom_addr); qprint(") to "); qprintn((uint32_t)(dest_addr)); qprint("\n"); \
    INSTR( rom_addr ) = THUMB_BL_INSTR( &INSTR( rom_addr ), (dest_addr) )

/** Specified by the linker */
extern uint32_t _bss_start[], _bss_end[], _text_start;

static inline void
zero_bss( void )
{
    uint32_t *bss = _bss_start;
    while( bss < _bss_end )
        *(bss++) = 0;
}

static void my_bzero32(void* buf, size_t len)
{
    bzero32(buf, len);
}

static void my_create_init_task(uint32_t a, uint32_t b, uint32_t c)
{
    create_init_task(a, b, c);
}

static void my_dcache_clean(uint32_t addr, uint32_t size)
{
    extern void dcache_clean(uint32_t, uint32_t);
    dcache_clean(addr, size);
}

static void my_icache_invalidate(uint32_t addr, uint32_t size)
{
    extern void icache_invalidate(uint32_t, uint32_t);
    icache_invalidate(addr, size);
}

void
__attribute__((noreturn,noinline,naked))
copy_and_restart( int offset )
{
    zero_bss();

    // Copy the firmware to somewhere safe in memory
    const uint8_t * const firmware_start = (void*) ROMBASEADDR;
    const uint32_t firmware_len = RELOCSIZE;
    uint32_t * const new_image = (void*) RELOCADDR;

    blob_memcpy( new_image, firmware_start, firmware_start + firmware_len );

    /*
     * in cstart() make these changes:
     * calls bzero(), then loads bs_end and calls
     * create_init_task
     */
    // Reserve memory at the end of malloc pool for our application
    // Note: unlike most (all?) DIGIC 4/5 cameras,
    // the malloc heap is specified as start + size (not start + end)
    // easiest way is to reduce its size and load ML right after it
    uint32_t ml_reserved_mem = 0x40000;
    qprint("[BOOT] reserving memory: "); qprintn(ml_reserved_mem); qprint("\n");
    qprint("before: user_mem_size = "); qprintn(INSTR(HIJACK_INSTR_HEAP_SIZE)); qprint("\n");
    INSTR( HIJACK_INSTR_HEAP_SIZE ) -= ml_reserved_mem;
    qprint(" after: user_mem_size = "); qprintn(INSTR(HIJACK_INSTR_HEAP_SIZE)); qprint("\n");

    // Fix cache maintenance calls before cstart
    FIXUP_BRANCH( HIJACK_FIXBR_DCACHE_CLN_1, my_dcache_clean );
    FIXUP_BRANCH( HIJACK_FIXBR_DCACHE_CLN_2, my_dcache_clean );
    FIXUP_BRANCH( HIJACK_FIXBR_ICACHE_INV_1, my_icache_invalidate );
    FIXUP_BRANCH( HIJACK_FIXBR_ICACHE_INV_2, my_icache_invalidate );

    // Fix the absolute jump to cstart
    FIXUP_BRANCH( HIJACK_INSTR_BL_CSTART, &INSTR( cstart ) );

    // Fix the calls to bzero32() and create_init_task() in cstart
    FIXUP_BRANCH( HIJACK_FIXBR_BZERO32, my_bzero32 );
    FIXUP_BRANCH( HIJACK_FIXBR_CREATE_ITASK, my_create_init_task );

    /* there are two more functions in cstart that don't require patching */
    /* the first one is within the relocated code; it initializes the per-CPU data structure at VA 0x1000 */
    /* the second one is called only when running on CPU1; assuming our code only runs on CPU0 */

    // Set our init task to run instead of the firmware one
    qprint("[BOOT] changing init_task from "); qprintn(INSTR( HIJACK_INSTR_MY_ITASK ));
    qprint("to "); qprintn((uint32_t) my_init_task); qprint("\n");
    INSTR( HIJACK_INSTR_MY_ITASK ) = (uint32_t) my_init_task;

    // Make sure that our self-modifying code clears the cache
    sync_caches();

    // jump to Canon firmware (Thumb code)
    thunk __attribute__((long_call)) reloc_entry = (thunk)( RELOCADDR + 1 );
    qprint("[BOOT] jumping to relocated startup code at "); qprintn((uint32_t) reloc_entry); qprint("\n");
    reloc_entry();

    // Unreachable
    while(1)
        ;
}

extern void dump_file(char* name, uint32_t addr, uint32_t size);
extern void memmap_info(void);
extern void malloc_info(void);
extern void sysmem_info(void);
extern void smemShowFix(void);

static void led_blink(int times, int delay_on, int delay_off)
{
    for (int i = 0; i < times; i++)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(delay_on);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
        msleep(delay_off);
    }
}

static void DUMP_ASM dump_task()
{
    /* print memory info on QEMU console */
    memmap_info();
    malloc_info();
    sysmem_info();
    smemShowFix();

    /* LED blinking test */
    led_blink(5, 500, 500);

    /* dump ROM */
    dump_file("ROM0.BIN", 0xE0000000, 0x02000000);
  //dump_file("ROM1.BIN", 0xF0000000, 0x02000000); // - might be slow

    /* dump RAM */
  //dump_file("RAM4.BIN", 0x40000000, 0x40000000); // - large file; decrease size if it locks up
  //dump_file("DF00.BIN", 0xDF000000, 0x00010000); // - size unknown; try increasing
 
    /* attempt to take a picture */
    call("Release");
    msleep(2000);

    /* save a diagnostic log */
    call("dumpf");
}

static void
my_init_task(int a, int b, int c, int d)
{
    qprintf("[BOOT] autoexec.bin loaded at %X - %X.\n", &_text_start, &_bss_end);

    init_task(a,b,c,d);

    msleep(1000);

    task_create("dump", 0x1e, 0x1000, dump_task, 0 );
}

/* used by font_draw */
/* we don't have a valid display buffer yet */
void disp_set_pixel(int x, int y, int c)
{
}
