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
NSTUB(0xFE0D316D,  cstart)              // Thumb
NSTUB(0xFE3FC21C,  bzero32)
NSTUB(0xFE3FBC78,  create_init_task)
NSTUB(0xFE0D3385,  init_task)           // Thumb

/** Tasks **/
NSTUB(0xFE3FC0EC,  task_create)
NSTUB(0xFE3FC1D4,  msleep)

/** Dumper **/
NSTUB(0xFE2E4AEB, dump_file)            // Thumb

/** Memory info **/
NSTUB(0xFE2DB8AD, malloc_info)          // Thumb
NSTUB(0xFE2DB945, sysmem_info)          // Thumb
NSTUB(0xFE15F747, smemShowFix)          // Thumb

/** Eventprocs (call by name) **/
NSTUB(0xFE48422F,  call)                // Thumb
