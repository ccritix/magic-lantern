/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.2 firmware for the EOS 80D.
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
NSTUB(0xFE0D316D,  cstart)                                  // Thumb
NSTUB(0xFE3FC21C,  bzero32)
NSTUB(0xFE3FBC78,  create_init_task)
NSTUB(0xFE0D3385,  init_task)                               // Thumb

/** Tasks **/
NSTUB(0xFE3FC0EC,  task_create)
NSTUB(0xFE3FC1D4,  msleep)

/** Dumper **/
NSTUB(0xFE2E4AEB,  dump_file)                               // Thumb

/** Memory info **/
NSTUB(0xFE2DB8AD,  malloc_info)                             // Thumb
NSTUB(0xFE2DB945,  sysmem_info)                             // Thumb
NSTUB(0xFE15F747,  smemShowFix)                             // Thumb

/** Debug messages **/
NSTUB(0xFE3FC214,  DryosDebugMsg)
NSTUB(0xFE3FBEEC,  dm_set_store_level)

/** Eventprocs (call by name) **/
NSTUB(0xFE48422F,  call)                                    // Thumb

/** Interrupts **/
NSTUB(    0x44B0,  pre_isr_hook)
NSTUB(    0x44B4,  post_isr_hook)
NSTUB(   0x19F14,  isr_table_handler)
NSTUB(   0x19F18,  isr_table_param)

/** MPU communication **/
NSTUB(0xFE253C0F,  mpu_send)                                // Thumb
NSTUB(0xFE5475CF,  mpu_recv)                                // Thumb
NSTUB(    0x890C,  mpu_recv_cbr)
NSTUB(0x80007BB8,  called_by_mpu_send)

/** Misc **/
NSTUB(0x800054AD,  vsnprintf)                               // Thumb
