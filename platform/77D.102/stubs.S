/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.2 firmware for the EOS 77D.
 *
 */
/*
 * Copyright (C) 2019 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stub.h>

/* Using Thumb stubs everywhere to keep things simple. The Thumb bit needs to be enabled manually. */


/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xE00400ED,  cstart)                                  /* calls bzero32 and create_init_task(..., init_task, ...) */
NSTUB(0xDF00D285,  bzero32)                                 /* zeros out a data structure. From sub_E0428334 LDR PC, =(loc_DF00D284+1) */
NSTUB(0xDF006515,  create_init_task)                        /* low-level DryOS initialization. From sub_E0427890 LDR PC, =(sub_DF006514+1) */
NSTUB(0xE065E279,  dcache_clean)                            /* loop with MCR p15 c7,c10,1; DSB */
NSTUB(0xE065E34D,  icache_invalidate)                       /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */
NSTUB(0xE0040215,  init_task)                               /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */

/** Tasks **/
NSTUB(    0x1008,  current_interrupt)                       /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */
NSTUB(    0x1020,  current_task)                            /* from task_create; pointer to the current task structure */
NSTUB(0xDF0087FF,  msleep)                                  /* argument is always multiple of 10 */
NSTUB(0xDF008CD3,  task_create)                             /* used to start TaskMain, GuiMainTask etc */

/** Interrupts **/
NSTUB(0xE0152EB1,  cli_spin_lock)                           /* used in AllocateMemory/FreeMemory and others */
NSTUB(   0x6D0C0,  isr_table_handler)                       /* from interrupt handler; address of the ISR handler */
NSTUB(   0x6D0C4,  isr_table_param)                         /* from interrupt handler; argument of the ISR handler */
NSTUB(    0x4034,  post_isr_hook)                           /* from interrupt handler; called after the ISR handler */
NSTUB(    0x4030,  pre_isr_hook)                            /* from interrupt handler; called before the ISR handler */

/** File I/O **/
NSTUB(0xE04D71B7, _FIO_CreateFile)
NSTUB(0xE04D714D, _FIO_OpenFile)
NSTUB(0xE04D7389,  FIO_CloseFile)
NSTUB(0xE04D7271, _FIO_ReadFile)
NSTUB(0xE04D7317, _FIO_WriteFile)
NSTUB(0xE04D8895,  FIO_SeekSkipFile)
NSTUB(0xE04D74A7, _FIO_GetFileSize)
NSTUB(0xE04D7225, _FIO_RemoveFile)
NSTUB(0xE04D7B2B, _FIO_RenameFile)
NSTUB(0xE04D7CB1, _FIO_CreateDirectory)
NSTUB(0xE04D80DB, _FIO_FindFirstEx)                         /* 0xe04d7fc1 is FIO_FindFirst */
NSTUB(0xE04D8173,  FIO_FindNextEx)                          /* 0xe04d804f is FIO_FindNext */
NSTUB(0xE04D80BB,  FIO_FindClose)                           /* 0xe04d81de is FIO_FincCloseEx(!) */
NSTUB(0xE04D7DD5,  FIO_Flush)                               // to be called after FIO_CloseFile?

/** Debug messages **/
NSTUB(0xDF006E6D,  DryosDebugMsg)                           /* lots of debug messages; format string is third argument */

/** Dumper **/
NSTUB(0xE007FC47,  dump_file)                               /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Eventprocs (call by name) **/
NSTUB(0xE04D8AEF,  call)                                    /* many functions called by name (lv_start, lv_stop etc) */

/** GUI timers **/
NSTUB(0xE05AADCB,  SetHPTimerNextTick)                      /* same "worker" function as SetHPTimerAfterNow */
NSTUB(0xE05AAD77,  SetHPTimerAfterNow)                      /* from error message */
NSTUB(0xE04D48E5,  SetTimerAfter)                           /* from error message */
NSTUB(0xE04D499B,  CancelTimer)                             /* from error message */

/** MPU communication **/
NSTUB(0xE058866B,  mpu_recv)                                // passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
NSTUB(    0x7CF4,  mpu_recv_cbr)                            // mpu_recv is called indirectly through this function pointer
NSTUB(   0x88694,  mpu_recv_ring_buffer)                    // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time
NSTUB(    0x7CD0,  mpu_recv_ring_buffer_tail)               // ring buffer index incremented in the above subroutine
NSTUB(0xE01E781F,  mpu_send)                                // "dwSize < TXBD_DATA_SIZE"
NSTUB(   0x887D4,  mpu_send_ring_buffer)                    // ring buffer used in mpu_send
NSTUB(    0x7CD8,  mpu_send_ring_buffer_tail)               // ring buffer index incremented in mpu_send

/** Memory allocation **/
NSTUB(0xDF007B49, _AllocateMemory)                          /* "NG AllocateMemory" */
NSTUB(0xDF007D01, _FreeMemory)                              /* from context; worker function operates on the same memory pool */
NSTUB(0xDF0079D3,  GetMemoryInformation)                    /* called from AllocateMemory */
NSTUB(0xDF0079A7,  GetSizeOfMaxRegion)                      /* same */
NSTUB(0xDF00AC41, _alloc_dma_memory)                        /* calls AllocateMemory(size + 64), aligns the pointer, makes it uncacheable, syncs caches */
NSTUB(0xDF00AC6D, _free_dma_memory)                         /* opposite of the above */
NSTUB(0xE06CE68F, _malloc)                                  /* "can't malloc" */
NSTUB(0xE06CE6C7, _free)                                    /* from context */

/** Memory info **/
NSTUB(0xE02640B5,  malloc_info)                             /* Malloc Information */
NSTUB(0xE01EAF81,  memmap_info)                             /* Exception vector, DRYOS system memory etc */
NSTUB(0xE0164CA7,  smemShowFix)                             /* Common Lower, Common Upper etc */
NSTUB(0xE026414D,  sysmem_info)                             /* System Memory Information */

/** Misc **/
NSTUB(0xE04D8BBD,  uart_printf)
NSTUB(0xE11F93D5,  vsnprintf)                               /* called near dmstart; references "01234567", "0123456789", "0123456789abcdef" and "0123456789ABCDEF"; second arg is size; the one called by DebugMsg only knows %s */
