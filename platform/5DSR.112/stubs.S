/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.1.2 firmware for the 5DS R.
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


/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xFE0EE2FD,  cstart)                                  // Thumb
NSTUB(0xFE3F4470,  bzero32)
NSTUB(0xFE3F3F20,  create_init_task)
NSTUB(0xFE0EE525,  init_task)                               // Thumb

/** Tasks **/
NSTUB(    0x44D4,  current_interrupt)
NSTUB(    0x44F4,  current_task)
NSTUB(0xFE3F4458,  msleep)
NSTUB(0xFE3F43D0,  task_create)

/** Interrupts **/
NSTUB(   0x1A770,  isr_table_handler)
NSTUB(   0x1A774,  isr_table_param)
NSTUB(    0x44B4,  post_isr_hook)
NSTUB(    0x44B0,  pre_isr_hook)

/** Debug messages **/
NSTUB(     0x269,  DryosDebugMsg)                           // Thumb

/** Dumper **/
NSTUB(0xFE1B1691,  dump_file)                               // Thumb

/** Eventprocs (call by name) **/
NSTUB(0xFE465807,  call)                                    // Thumb

/** GUI timers **/
NSTUB(0xFE52B6AB,  SetHPTimerNextTick)                      // Thumb; same "worker" function as SetHPTimerAfterNow
NSTUB(0xFE52B65B,  SetHPTimerAfterNow)                      // Thumb; from error message
NSTUB(0xFE46D865,  SetTimerAfter)                           // Thumb; from error message
NSTUB(0xFE46D8D7,  CancelTimer)                             // Thumb; unsure

/** MPU communication **/
NSTUB(0xFE54B4AB,  mpu_recv)                                // Thumb; passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
NSTUB(    0xA2B8,  mpu_recv_cbr)
NSTUB(   0x30644,  mpu_recv_ring_buffer)                    // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time
NSTUB(    0xA294,  mpu_recv_ring_buffer_tail)               // ring buffer index incremented in the above subroutine
NSTUB(0xFE323B9F,  mpu_send)                                // Thumb; "dwSize < TXBD_DATA_SIZE"
NSTUB(   0x30784,  mpu_send_ring_buffer)                    // ring buffer used in mpu_send
NSTUB(    0xA29C,  mpu_send_ring_buffer_tail)               // ring buffer index incremented in mpu_send

/** Memory allocation **/
NSTUB(0xFE3F4468, _AllocateMemory)
NSTUB(0xFE3F4478, _FreeMemory)
NSTUB(0xFE3F4130,  GetMemoryInformation)
NSTUB(0xFE3F4138,  GetSizeOfMaxRegion)

/** Memory info **/
NSTUB(0xFE333941,  malloc_info)                             // Thumb
NSTUB(0xFE19E391,  smemShowFix)                             // Thumb
NSTUB(0xFE3339D9,  sysmem_info)                             // Thumb

/** Misc **/
NSTUB(    0x1D70,  vsnprintf)
