/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.1 firmware for the EOS 200D.
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

/* Using Thumb stubs everywhere to keep things simple. The Thumb bit needs to be enabled manually. */

/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xE00400FD,  cstart)                 /* calls bzero32 and create_init_task(..., init_task, ...) */
NSTUB(0xDF00D299,  bzero32)                /* zeros out a data structure */
NSTUB(0xDF006515,  create_init_task)       /* low-level DryOS initialization */
NSTUB(0xE0040225,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */
NSTUB(0xE06BE169,  dcache_clean)           /* loop with MCR p15 c7,c10,1; DSB */
NSTUB(0xE06BE23D,  icache_invalidate)      /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */

/** Tasks **/
NSTUB(0xDF008CE7,  task_create)            /* used to start TaskMain, GuiMainTask etc */
NSTUB(0xDF00880F,  msleep)                 /* argument is always multiple of 10 */
NSTUB(    0x1028,  current_task)           /* from task_create; pointer to the current task structure */
NSTUB(    0x1010,  current_interrupt)      /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */

/** Dumper **/
NSTUB(0xE007EE5B,  dump_file)              /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

/** Memory info **/
NSTUB(0xE023D3C9,  malloc_info)            /* Malloc Information */
NSTUB(0xE023D461,  sysmem_info)            /* System Memory Information */
NSTUB(0xE01CA9C9,  memmap_info)            /* Exception vector, DRYOS system memory etc */
NSTUB(0xE0148E3B,  smemShowFix)            /* Common Lower, Common Upper etc */

/** Memory allocation **/
NSTUB(0xDF0079E3,  GetMemoryInformation)   /* called from AllocateMemory */

/** Debug messages **/
NSTUB(0xDF006E6D,  DryosDebugMsg)          /* lots of debug messages; format string is third argument */

/** Eventprocs (call by name) **/
NSTUB(0xE04BDB8F,  call)                   /* many functions called by name (lv_start, lv_stop etc) */

/** GUI timers **/
NSTUB(0xE04B9A3B,  CancelTimer)            /* from error message */
NSTUB(0xE05301DF,  SetHPTimerAfterNow)     /* from error message */
NSTUB(0xE0530233,  SetHPTimerNextTick)     /* same "worker" function as SetHPTimerAfterNow */
NSTUB(0xE04B9985,  SetTimerAfter)          /* from error message */

/** Interrupts **/
NSTUB(    0x4030,  pre_isr_hook)
NSTUB(    0x4034,  post_isr_hook)
NSTUB(   0x6CC10,  isr_table_handler)
NSTUB(   0x6CC14,  isr_table_param)

/** MPU communication **/
NSTUB(0xE01C7657,  mpu_send)                  // "dwSize < TXBD_DATA_SIZE"
NSTUB(0xE056314F,  mpu_recv)                  // passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
NSTUB(    0x79C0,  mpu_recv_cbr)              // mpu_recv is called indirectly through this function pointer
NSTUB(   0x87AD8,  mpu_send_ring_buffer)      // ring buffer used in mpu_send
NSTUB(    0x79A4,  mpu_send_ring_buffer_tail) // ring buffer index incremented in mpu_send
NSTUB(   0x87998,  mpu_recv_ring_buffer)      // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time
NSTUB(    0x799C,  mpu_recv_ring_buffer_tail) // ring buffer index incremented in the above subroutine

/** Misc **/
NSTUB(0xE00764F1,  vsnprintf)              /* references "01234567", "0123456789", "0123456789abcdef" and "0123456789ABCDEF"; second arg is size; the one called by DebugMsg only knows %s */
