/** \file
 * Minimal ML - for debugging
 */

#include "dryos.h"
#include "vram.h"
#include "bmp.h"
#include "font_direct.h"


/** This just goes into the bss */
#define RELOCSIZE 0x0000300 // look in HIJACK macros for the highest address, and subtract ROMBASEADDR
static uint8_t _reloc[RELOCSIZE];
#define RELOCADDR ((uintptr_t) _reloc)

#define ROMBASEADDR  0xFE0A0000
#define ROMBASEADDR2 0xFE0EE2FC

/** Translate a firmware address into a relocated address */
#define INSTR( addr )    ( *(uint32_t*)( (addr) - ROMBASEADDR + RELOCADDR ) )
#define INSTR16( addr )  ( *(uint16_t*)( (addr) - ROMBASEADDR + RELOCADDR ) )
#define INSTR2( addr )   ( *(uint32_t*)( (addr) - ROMBASEADDR2 + RELOCADDR + 0x130 ) )
#define INSTR216( addr ) ( *(uint16_t*)( (addr) - ROMBASEADDR2 + RELOCADDR + 0x130 ) )

/** Fix a branch instruction in the relocated firmware image */
#define THUMB_B_W_INSTR(pc,dest)      thumb_branch_instr(pc,dest,0x9000f000)
#define THUMB_BL_W_INSTR(pc,dest)     thumb_branch_instr(pc,dest,0xd000f000)
#define THUMB_BLX_W_INSTR(pc,dest)    thumb_branch_instr(pc,dest,0xc000f000)

#define FIXUP_BRANCH_BLX( rom_addr, dest_addr )  INSTR( rom_addr ) = THUMB_BLX_W_INSTR( &INSTR( rom_addr ), (dest_addr) )
#define FIXUP_BRANCH_BL( rom_addr, dest_addr )  INSTR( rom_addr ) = THUMB_BL_W_INSTR( &INSTR( rom_addr ), (dest_addr) )

#define FIXUP_BRANCH2_BLX( rom_addr, dest_addr )  INSTR2( rom_addr ) = THUMB_BLX_W_INSTR( &INSTR2( rom_addr ), (dest_addr) )
#define FIXUP_BRANCH2_BL( rom_addr, dest_addr )  INSTR2( rom_addr ) = THUMB_BL_W_INSTR( &INSTR2( rom_addr ), (dest_addr) )

/** Specified by the linker */
extern uint32_t _bss_start[], _bss_end[];

static inline void
zero_bss( void )
{
    uint32_t *bss = _bss_start;
    while( bss < _bss_end )
        *(bss++) = 0;
}

static inline uint32_t thumb_branch_instr(uint32_t pc, uint32_t dest, uint32_t opcode)
{
    /* thanks atonal */
    uint32_t offset = dest - ((pc + 4) & ~3);
    uint32_t s = (offset >> 24) & 1;
    uint32_t i1 = (offset >> 23) & 1;
    uint32_t i2 = (offset >> 22) & 1;
    uint32_t imm10 = (offset >> 12) & 0x3ff;
    uint32_t imm11 = (offset >> 1) & 0x7ff;
    uint32_t j1 = (!(i1 ^ s)) & 0x1;
    uint32_t j2 = (!(i2 ^ s)) & 0x1;

    return opcode | (s << 10) | imm10 | (j1 << 29) | (j2 << 27) | (imm11 << 16);
}


static void my_create_init_task(int a1, int a2, int a3);
static void reloc_func_1();
static void reloc_func_2(int a1, int a2);
static void my_init_task();

void
__attribute__((noreturn,noinline,naked))
copy_and_restart( int offset )
{
    zero_bss();
    
    /* first copy _startup */
    blob_memcpy( RELOCADDR, ROMBASEADDR, ROMBASEADDR + 0x130 );
    /* then copy cstart */
    blob_memcpy( RELOCADDR + 0x130, (void*)ROMBASEADDR2, (void*)ROMBASEADDR2 + 0x100 );

    /* jump from _startup to cstart */
    INSTR16( 0xFE0A00FE ) = 0xE017;
    
    /* relocate some io init func here */
    FIXUP_BRANCH_BLX( 0xFE0A00A4, &reloc_func_1);
    
    /* relocate bzero and create_init_task here */
    FIXUP_BRANCH2_BLX( 0xFE0EE31A, &reloc_func_2);
    FIXUP_BRANCH2_BLX( 0xFE0EE36E, &my_create_init_task );
    
    /* reserver memory */
    uint32_t ml_reserved_mem = (uintptr_t) _bss_end - INSTR2( HIJACK_INSTR_BSS_END );
    INSTR2( 0xFE0EE388 ) += ml_reserved_mem;
    INSTR2( 0xFE0EE38C ) -= ml_reserved_mem;

    sync_caches();

    /* now boot camera */
    thunk reloc_entry = (thunk)(RELOCADDR);
    
    reloc_entry();
    
    // Unreachable
    while(1)
        ;
}

static void my_create_init_task(int a1, int a2, int a3)
{
    void (*reloc_func)(int, int, int) = (void *)(0xFE3F3958);
    
    a2 = &my_init_task;
    
    reloc_func(a1, a2, a3);
}

static void reloc_func_1()
{
    void (*reloc_func)() = (void *)(0xFE0EE2DC + 1);
    
    reloc_func();
}

static void reloc_func_2(int a1, int a2)
{
    void (*reloc_func)(int, int) = (void *)(0xFE3F3EA8);
    
    reloc_func(a1, a2);
}

#if 0
/* ROM dumper */
extern FILE* _FIO_CreateFile(const char* filename );

/* this cannot run from init_task */
static void run_test()
{
    /* change to A:/ for CF cards */
    FILE * f = _FIO_CreateFile("A:/FF000000.BIN");
    
    if (f != (void*) -1)
    {
        FIO_WriteFile(f, (void*) 0xFF000000, 0x1000000);
        FIO_CloseFile(f);
    }
}
#endif

static int addr_pos = 5;
/* used by font_draw */
void disp_set_pixel(int x, int y, int c)
{
    uint8_t* bmp = 0x7ED2E000 ;
    bmp[x + y * 960] = c;
}

uint32_t BulkOutIPCTransfer(int type, uint8_t *buffer, int length, uint32_t master_addr, void (*cb)(uint32_t, uint32_t, uint32_t), uint32_t cb_parm);
uint32_t BulkInIPCTransfer(int type, uint8_t *buffer, int length, uint32_t master_addr, void (*cb)(uint32_t, uint32_t, uint32_t), uint32_t cb_parm);
void *AllocateUncacheableMemory(uint32_t length);
void FreeUncachableMemory(void *buffer);

void bulk_cbr(uint32_t parm, uint32_t address, uint32_t length)
{
    *(uint32_t*)parm = 0;
}

static void dump_slave(uint32_t address, uint32_t length, char *filename)
{
    uint32_t blocksize = 0x100000;
    void *buffer = AllocateUncacheableMemory(blocksize);
    FILE *f = FIO_CreateFile(filename);
    
    while(length)
    {
        volatile uint32_t wait = 1;
        uint32_t len = (length > blocksize) ? blocksize : length;
        
        BulkInIPCTransfer(0, buffer, len, address, &bulk_cbr, (uint32_t)&wait);
        while(wait)
        {
            msleep(10);
        }
        
        FIO_WriteFile(f, buffer, len);
        length -= len;
        address += len;
    }
    
    FIO_CloseFile(f);
    FreeUncachableMemory(buffer);
}

static void dump_master(uint32_t address, uint32_t length, char *filename)
{
    uint32_t blocksize = 0x100000;
    void *buffer = AllocateUncacheableMemory(blocksize);
    FILE *f = FIO_CreateFile(filename);
    
    while(length)
    {
        uint32_t len = (length > blocksize) ? blocksize : length;
        
        memcpy(buffer, address, len);
        FIO_WriteFile(f, buffer, len);
        
        length -= len;
        address += len;
    }
    
    FIO_CloseFile(f);
    FreeUncachableMemory(buffer);
}

static void my_ml_task()
{
    msleep(4000);
    
    dump_slave(0x00000000, 0x10000000, "A:/00000000.SLV");
    dump_slave(0x10000000, 0x10000000, "A:/10000000.SLV");
    dump_slave(0x20000000, 0x10000000, "A:/20000000.SLV");
    dump_slave(0x30000000, 0x10000000, "A:/30000000.SLV");
    dump_slave(0x40000000, 0x10000000, "A:/40000000.SLV");
    dump_slave(0x50000000, 0x10000000, "A:/50000000.SLV");
    dump_slave(0x60000000, 0x10000000, "A:/60000000.SLV");
    dump_slave(0x70000000, 0x10000000, "A:/70000000.SLV");
    dump_slave(0x80000000, 0x10000000, "A:/80000000.SLV");
    dump_slave(0x90000000, 0x10000000, "A:/90000000.SLV");
    dump_slave(0xA0000000, 0x10000000, "A:/A0000000.SLV");
    dump_slave(0xB0000000, 0x10000000, "A:/B0000000.SLV");
    dump_slave(0xE0000000, 0x10000000, "A:/E0000000.SLV");
    dump_slave(0xF0000000, 0x08000000, "A:/F0000000.SLV");
    dump_slave(0xF8000000, 0x08000000, "A:/F8000000.SLV");
    dump_slave(0xFE000000, 0x02000000, "A:/FE000000.SLV");
    
    dump_master(0x00000000, 0x00004000, "A:/00000000.MST");
    dump_master(0x00004000, 0x0FFFC000, "A:/00004000.MST");
    dump_master(0x40000000, 0x10000000, "A:/40000000.MST");
    dump_master(0x50000000, 0x10000000, "A:/50000000.MST");
    dump_master(0x60000000, 0x10000000, "A:/60000000.MST");
    dump_master(0x70000000, 0x10000000, "A:/70000000.MST");
    dump_master(0xBFE00000, 0x00200000, "A:/BFE00000.MST");
    dump_master(0x80000000, 0x00010000, "A:/80000000.MST");
    dump_master(0xFC000000, 0x02000000, "A:/FC000000.MST");
    dump_master(0xFE000000, 0x02000000, "A:/FE000000.MST");
    
    return;
}

/** Initial task setup.
 *
 * This is called instead of the task at 0xFF811DBC.
 * It does all of the stuff to bring up the debug manager,
 * the terminal drivers, stdio, stdlib and armlib.
 */
static void my_init_task()
{
    void (*reloc_func)() = (void *)(0xFE0EE524 + 1);
    
    task_create("ML", 0x17, 0x1000, &my_ml_task, 0);
    reloc_func();
    
    return;
}
