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
NSTUB(0xFE0A0B19,  cstart)              // Thumb
NSTUB(0xFE683258,  bzero32)
NSTUB(0xFE683260,  create_init_task)
NSTUB(0xFE0A1981,  init_task)           // Thumb

/** Tasks **/
NSTUB(0xFE6833A4,  task_create)
NSTUB(0xFEC28D28,  msleep)

/** Dumper **/
NSTUB(0xFE2C22C1, dump_file)            // Thumb
