/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.0 firmware for the EOS R.
 *
 */
/*
 * Copyright (C) 2018 Magic Lantern Team
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

/* All functions are Thumb. The least significant bit needs to be set manually. */

/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xE00400FD,  cstart)                 /* calls bzero32 and create_init_task(..., init_task, ...) */
NSTUB(0xE05A33FD,  bzero32)                /* zeros out a data structure */
NSTUB(0xE01218E1,  create_init_task)       /* low-level DryOS initialization */
NSTUB(0xE0040225,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */
NSTUB(0xE0320761,  dcache_clean)           /* loop with MCR p15 c7,c10,1; DSB */
NSTUB(0xE0320835,  icache_invalidate)      /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */

/** Tasks **/
NSTUB(0xE055760F,  task_create)            /* used to start TaskMain, GuiMainTask etc */
NSTUB(0xE0589683,  msleep)                 /* argument is always multiple of 10 */
NSTUB(    0x1028,  current_task)           /* from task_create; pointer to the current task structure */
    NSTUB(    0x1010,  current_interrupt)      /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */

/** Dumper **/
NSTUB(0xE00C0EAB,  dump_file)              /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Memory allocation **/
NSTUB(0xE05924D9, _AllocateMemory)         /* NG AllocateMemory */
NSTUB(0xE0592691, _FreeMemory)             /* uses the same heap */
NSTUB(0xE0592363,  GetMemoryInformation)   /* called from AllocateMemory */
NSTUB(0xE0592337,  GetSizeOfMaxRegion)     /* same */
NSTUB(0xE0591F39, _alloc_dma_memory)       /* allocates 64 bytes more, aligns at 32 and sets the uncacheable bit */
NSTUB(0xE0591F6D, _free_dma_memory)        /* paired with the above; clears the uncacheable bit and frees the raw pointer */
NSTUB(0xE02A54DF,  malloc)                 /* see "can't malloc"
NSTUB(0xE02A5517,  free)                   /* next function; paired with the above */

/** Memory info **/
NSTUB(0xE00D1669,  malloc_info)            /* Malloc Information */
NSTUB(0xE00D1701,  sysmem_info)            /* System Memory Information */
NSTUB(0xE0090355,  memmap_info)            /* Exception vector, DRYOS system memory etc */
NSTUB(0xE015DC73,  smemShowFix)            /* Common Lower, Common Upper etc */

/** Debug messages **/
NSTUB(0xE05A35D5,  DryosDebugMsg)          /* lots of debug messages; format string is third argument */

/** Eventprocs (call by name) **/
NSTUB(0xE05932F7,  call)                   /* used in ElectricCurrentFirm */

/** File I/O **/
NSTUB(0xE057B225, _FIO_OpenFile)
NSTUB(0xE057B2D7, _FIO_CreateFile)
NSTUB(0xE057B6AF, _FIO_ReadFile)
NSTUB(0xE057B84B, _FIO_WriteFile)
NSTUB(0xE057B78B,  FIO_SeekSkipFile)       // guess: Search?!
NSTUB(0xE057BA5F,  FIO_CloseFile)
NSTUB(0xE057CAB9, _FIO_CreateDirectory)
NSTUB(0xE057CF1B, _FIO_FindFirstEx)        // FirstEnt
NSTUB(0xE057D013,  FIO_FindNextEx)         // NextEnt
NSTUB(0xE057D0D5,  FIO_FindClose)          // CloseEnt
NSTUB(0xE057BBFB, _FIO_GetFileSize)        // AcqSize
NSTUB(0xE057B615, _FIO_RemoveFile)
NSTUB(0xE057C631, _FIO_RenameFile)
NSTUB(0xE057CCA3,  FIO_Flush)              // to be called after FIO_CloseFile?

/** GUI timers **/
NSTUB(0xE057ADE7,  CancelTimer)            /* CancelRefreshTimer, CancelPlayTimer */
NSTUB(0xE03B72B7,  SetHPTimerAfterNow)     /* from error message */
NSTUB(0xE03B7317,  SetHPTimerNextTick)     /* next function; same "worker" as SetHPTimerAfterNow */
NSTUB(0xE057AD31,  SetTimerAfter)          /* from error message */

/** Misc **/
NSTUB(0xE0067E7D,  vsnprintf)              /* called by LogFile_printf(..., "*Data Lost*\n", ...); the one called by DebugMsg only knows %s */
