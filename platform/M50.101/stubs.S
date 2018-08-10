/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.1 firmware for the EOS M50.
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
NSTUB(0xE0578C59,  bzero32)                /* zeros out a data structure */
NSTUB(0xE0143CBD,  create_init_task)       /* low-level DryOS initialization */
NSTUB(0xE0040225,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */
NSTUB(0xE05776C5,  dcache_clean)           /* loop with MCR p15 c7,c10,1; DSB */
NSTUB(0xE0577799,  icache_invalidate)      /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */

/** Tasks **/
NSTUB(0xE0545DD7,  task_create)            /* used to start TaskMain, GuiMainTask etc */
NSTUB(0xE05597EF,  msleep)                 /* argument is always multiple of 10 */
NSTUB(    0x1028,  current_task)           /* from task_create; pointer to the current task structure */
NSTUB(    0x1010,  current_interrupt)      /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */

/** Dumper **/
NSTUB(0xE0085F61,  dump_file)              /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Memory info **/
NSTUB(0xE00F1BD9,  malloc_info)            /* Malloc Information */
NSTUB(0xE00F1C71,  sysmem_info)            /* System Memory Information */
NSTUB(0xE009DE99,  memmap_info)            /* Exception vector, DRYOS system memory etc */
NSTUB(0xE0146F97,  smemShowFix)            /* Common Lower, Common Upper etc */

/** Debug messages **/
NSTUB(0xE0577EC5,  DryosDebugMsg)
NSTUB(0xE05782A5,  dm_set_store_level)     /* called by dmstore */

/** Eventprocs (call by name) **/
NSTUB(0xE05721BB,  call)                   /* many functions called by name (lv_start, lv_stop etc) */

/** Misc **/
NSTUB(0xE0096041,  vsnprintf)              /* called by LogFile_printf(..., "*Data Lost*\n", ...); the one called by DebugMsg only knows %s */
