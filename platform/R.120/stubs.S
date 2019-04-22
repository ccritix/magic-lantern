/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.2.0 firmware for the EOS R.
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

/* All functions are Thumb. */


/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
THUMB_FN(0xE00400FC,  cstart)                               /* calls bzero32 and create_init_task(..., init_task, ...) */
THUMB_FN(0xE05A4428,  bzero32)                              /* zeros out a data structure */
THUMB_FN(0xE011E098,  create_init_task)                     /* low-level DryOS initialization */
THUMB_FN(0xE031BEE4,  dcache_clean)                         /* loop with MCR p15 c7,c10,1; DSB */
THUMB_FN(0xE031BFB8,  icache_invalidate)                    /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */
THUMB_FN(0xE0040224,  init_task)                            /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */

/** Tasks **/
DATA_PTR(    0x1010,  current_interrupt)                    /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */
DATA_PTR(    0x1028,  current_task)                         /* from task_create; pointer to the current task structure */
THUMB_FN(0xE058AD16,  msleep)                               /* argument is always multiple of 10 */
THUMB_FN(0xE055A662,  task_create)                          /* used to start TaskMain, GuiMainTask etc */

/** File I/O **/cd ..
THUMB_FN(0xE057C3D2, _FIO_CreateFile)
THUMB_FN(0xE057C320, _FIO_OpenFile)
THUMB_FN(0xE057CB5A,  FIO_CloseFile)
THUMB_FN(0xE057C7AA, _FIO_ReadFile)
THUMB_FN(0xE057C946, _FIO_WriteFile)
THUMB_FN(0xE057C886,  FIO_SeekSkipFile)                     // guess: Search?!
THUMB_FN(0xE057CCF6, _FIO_GetFileSize)                      // AcqSize
THUMB_FN(0xE057C710, _FIO_RemoveFile)
THUMB_FN(0xE057D72C, _FIO_RenameFile)
THUMB_FN(0xE057DBB4, _FIO_CreateDirectory)
THUMB_FN(0xE057E016, _FIO_FindFirstEx)                      // FirstEnt
THUMB_FN(0xE057E10E,  FIO_FindNextEx)                       // NextEnt
THUMB_FN(0xE057E1D0,  FIO_FindClose)                        // CloseEnt
THUMB_FN(0xE057DD9E,  FIO_Flush)                            // to be called after FIO_CloseFile?

/** Bitmap **/
DATA_PTR(    0xFB1C,  bmp_vram_info)                        /* first VRAM buffer, used right after DISP_SetUpdateOSDVram */

/** Debug messages **/
THUMB_FN(0xE05A4600,  DryosDebugMsg)                        /* lots of debug messages; format string is third argument */

/** Dumper **/
THUMB_FN(0xE00BD652,  dump_file)                            /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Eventprocs (call by name) **/
THUMB_FN(0xE0594382,  call)                                 /* used in ElectricCurrentFirm */

/** GUI timers **/
THUMB_FN(0xE03B65F2,  SetHPTimerNextTick)                   /* next function; same "worker" as SetHPTimerAfterNow */
THUMB_FN(0xE03B6592,  SetHPTimerAfterNow)                   /* System::HPTimer.c */
THUMB_FN(0xE057BE2C,  SetTimerAfter)                        /* from error message */
THUMB_FN(0xE057BEE2,  CancelTimer)                          /* CancelRefreshTimer, CancelPlayTimer */

/** Memory allocation **/
THUMB_FN(0xE0593B24, _AllocateMemory)                       /* NG AllocateMemory */
THUMB_FN(0xE0593CDC, _FreeMemory)                           /* uses the same heap */
THUMB_FN(0xE05939AE,  GetMemoryInformation)                 /* called from AllocateMemory */
THUMB_FN(0xE0593982,  GetSizeOfMaxRegion)                   /* same */
THUMB_FN(0xE0593584, _alloc_dma_memory)                     /* allocates 64 bytes more, aligns at 32 and sets the uncacheable bit */
THUMB_FN(0xE05935B8, _free_dma_memory)                      /* paired with the above; clears the uncacheable bit and frees the raw pointer */
THUMB_FN(0xE02A40F8, _malloc)                               /* see "can't malloc"
THUMB_FN(0xE02A4130, _free)                                 /* next function; paired with the above */

/** Memory info **/
THUMB_FN(0xE00CDE10,  malloc_info)                          /* Malloc Information */
THUMB_FN(0xE008CAC8,  memmap_info)                          /* Exception vector, DRYOS system memory etc */
THUMB_FN(0xE0159F8A,  smemShowFix)                          /* Common Lower, Common Upper etc */
THUMB_FN(0xE00CDEA8,  sysmem_info)                          /* System Memory Information */

/** Misc **/
THUMB_FN(0xE058E62C,  uart_printf)
THUMB_FN(0xE0064548,  vsnprintf)                            /* called by LogFile_printf(..., "*Data Lost*\n", ...); the one called by DebugMsg only knows %s */
