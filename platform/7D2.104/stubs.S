/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.4 firmware for the 7D Mark II.
 *
 */
/*
 * Copyright (C) 2016 Magic Lantern Team
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
NSTUB(0xFE0A0B19,  cstart)                                  // Thumb
NSTUB(0xFE683258,  bzero32)
NSTUB(0xFE683260,  create_init_task)
NSTUB(0xFE0A1981,  init_task)                               // Thumb

/** Tasks **/
NSTUB(   0x28548,  current_interrupt)
NSTUB(   0x28568,  current_task)
NSTUB(0xFEC28D28,  msleep)
NSTUB(0xFE6833A4,  task_create)

/** Interrupts **/
NSTUB(   0x3BFB4,  isr_table_handler)
NSTUB(   0x3BFB8,  isr_table_param)
NSTUB(   0x28528,  post_isr_hook)
NSTUB(   0x28524,  pre_isr_hook)

/** Debug messages **/
NSTUB(     0x237,  DryosDebugMsg)                           // Thumb

/** Dumper **/
NSTUB(0xFE2C22C1,  dump_file)                               // Thumb

/** Eventprocs (call by name) **/
NSTUB(0xFE1098AF,  call)                                    // Thumb

/** GUI timers **/
NSTUB(0xFE10E4D7,  SetHPTimerNextTick)                      // Thumb; same "worker" function as SetHPTimerAfterNow
NSTUB(0xFE10E487,  SetHPTimerAfterNow)                      // Thumb; from error message
NSTUB(0xFE117A71,  SetTimerAfter)                           // Thumb; from error message
NSTUB(0xFE117AE3,  CancelTimer)                             // Thumb; from error message

/** MPU communication **/
NSTUB(0xFE0FA87F,  mpu_recv)                                // Thumb; passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
NSTUB(   0x2CAF8,  mpu_recv_cbr)
NSTUB(   0x519A4,  mpu_recv_ring_buffer)                    // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time
NSTUB(   0x2CAE0,  mpu_recv_ring_buffer_tail)               // ring buffer index incremented in the above subroutine
NSTUB(0xFE250B2F,  mpu_send)                                // Thumb; "dwSize < TXBD_DATA_SIZE"
NSTUB(   0x51AE4,  mpu_send_ring_buffer)                    // ring buffer used in mpu_send
NSTUB(   0x2CAE8,  mpu_send_ring_buffer_tail)               // ring buffer index incremented in mpu_send

/** Memory allocation **/
NSTUB(0x8000AFC4, _AllocateMemory)
NSTUB(0x8000AFDC, _FreeMemory)
NSTUB(0xFE6838BC,  GetMemoryInformation)
NSTUB(0xFE68355C,  GetSizeOfMaxRegion)

/** Memory info **/
NSTUB(0xFE0A1AD1,  malloc_info)                             // Thumb
NSTUB(0xFE1A3083,  smemShowFix)                             // Thumb
NSTUB(0xFE0A1B69,  sysmem_info)                             // Thumb

/** Misc **/
NSTUB(0x3280,  vsnprintf)
