/** \file
 * 80D dumper
 */

#include "dryos.h"
#include "log.h"

/** These are called when new tasks are created */
static void my_init_task(int a, int b, int c, int d);

/** This just goes into the bss */
#define RELOCSIZE 0x40000 // look in HIJACK macros for the highest address, and subtract ROMBASEADDR
static uint8_t _reloc[ RELOCSIZE ];
#define RELOCADDR ((uintptr_t) _reloc)

/** Encoding for the Thumb instructions used for patching the boot code */
#define THUMB_RET_INSTR 0x00004770        /* BX LR */

static inline uint32_t thumb_branch_instr(uint32_t pc, uint32_t dest, uint32_t opcode)
{
    /* thanks atonal */
    uint32_t offset = dest - ((pc + 4) & ~3); /* according to datasheets, this should be the correct calculation -> ALIGN(PC, 4) */
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
#define THUMB_BLX_INSTR(pc,dest)    thumb_branch_instr((uint32_t)(pc), (uint32_t)(dest), 0xc000f000)

#define INSTR( addr ) ( *(uint32_t*)( (addr) - ROMBASEADDR + RELOCADDR ) )

/** Fix a branch instruction in the relocated firmware image */
#define FIXUP_BRANCH( rom_addr, dest_addr ) \
    INSTR( rom_addr ) = THUMB_BLX_INSTR( &INSTR( rom_addr ), (dest_addr) )

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
    // the malloc buffer is specified as start + size (not start + end)
    // so we adjust both values in order to keep things close to the traditional ML boot process
    // (alternative: we could adjust only the size, and place ML at the end of malloc buffer)
    uint32_t ml_reserved_mem = (uintptr_t) _bss_end - INSTR( HIJACK_INSTR_BSS_END );
    qprint("[BOOT] reserving memory:"); qprintn(ml_reserved_mem); qprint("\n");
    qprint("before: user_mem_start = "); qprintn(INSTR( HIJACK_INSTR_BSS_END));
    qprint("size = "); qprintn(INSTR( HIJACK_INSTR_BSS_END + 4 )); qprint("\n");
    INSTR( HIJACK_INSTR_BSS_END     ) += ml_reserved_mem;
    INSTR( HIJACK_INSTR_BSS_END + 4 ) -= ml_reserved_mem;
    qprint(" after: user_mem_start = "); qprintn(INSTR( HIJACK_INSTR_BSS_END));
    qprint("size = "); qprintn(INSTR( HIJACK_INSTR_BSS_END + 4 )); qprint("\n");

    // Fix the calls to bzero32() and create_init_task()
    FIXUP_BRANCH( HIJACK_FIXBR_BZERO32, my_bzero32 );
    FIXUP_BRANCH( HIJACK_FIXBR_CREATE_ITASK, my_create_init_task );

    // Set our init task to run instead of the firmware one
    qprint("[BOOT] changing init_task from "); qprintn(INSTR( HIJACK_INSTR_MY_ITASK ));
    qprint("to "); qprintn((uint32_t) my_init_task); qprint("\n");
    INSTR( HIJACK_INSTR_MY_ITASK ) = (uint32_t) my_init_task;
    
    // Make sure that our self-modifying code clears the cache
    sync_caches();

    // We enter after the signature, avoiding the
    // relocation jump that is at the head of the data
    // this is Thumb code
    MEM(0xD20C0084) = 0;
    thunk __attribute__((long_call)) reloc_entry = (thunk)( RELOCADDR + 0xC + 1 );
    reloc_entry();

    // Unreachable
    while(1)
        ;
}

extern void dump_file(char* name, uint32_t addr, uint32_t size);
extern void malloc_info(void);
extern void sysmem_info(void);
extern void smemShowFix(void);

static void DUMP_ASM dump_task()
{
#if 0
    /* print memory info on QEMU console */
    malloc_info();
    sysmem_info();
    smemShowFix();

    /* dump ROM1 */
    dump_file("ROM1.BIN", 0xFC000000, 0x02000000);

    /* dump RAM */
    dump_file("ATCM.BIN", 0x00000000, 0x00004000);
    dump_file("BTCM.BIN", 0x80000000, 0x00010000);
  //dump_file("RAM4.BIN", 0x40000000, 0x40000000);    - runs out of space in QEMU
    dump_file("BFE0.BIN", 0xBFE00000, 0x00200000);
    dump_file("DFE0.BIN", 0xDFE00000, 0x00200000);
  //dump_file("EE00.BIN", 0xEE000000, 0x02000000);    - unknown, may crash 
#endif

    /* save a diagnostic log */
    log_finish();
    call("dumpf");
}

static void
my_init_task(int a, int b, int c, int d)
{
    qprintf("[BOOT] autoexec.bin loaded at %X - %X.\n", &_text_start, &_bss_end);

    init_task(a,b,c,d);

    log_start();

    msleep(1000);

    task_create("dump", 0x1e, 0x1000, dump_task, 0 );
}

/* used by font_draw */
/* we don't have a valid display buffer yet */
void disp_set_pixel(int x, int y, int c)
{
}

/* dummy */
int FIO_WriteFile( FILE* stream, const void* ptr, size_t count ) { };
