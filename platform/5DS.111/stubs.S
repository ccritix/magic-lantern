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
NSTUB(ROMBASEADDR, firmware_entry )
NSTUB(0xFE0A0B19,  cstart)              // Thumb
NSTUB(0xFE683258,  bzero32)
NSTUB(0xFE3F3958,  create_init_task)
NSTUB(0xFE0EE525,  init_task)           // Thumb

/** Tasks **/
NSTUB(0xFE3F3E08,  task_create)
NSTUB(0xFE3F3E90,  msleep)

NSTUB(0xFE3F3D80,  AllocateUncacheableMemory)
NSTUB(0xFE3F3E58,  FreeUncachableMemory)

/** File I/O **/
NSTUB(0xFE46D6A5,  FIO_CloseFile)
NSTUB(0xFEE9A839,  FIO_FindClose)
NSTUB(0xFEE9A841,  FIO_FindNextEx)
NSTUB(0xFE46D525,  FIO_ReadFile)
NSTUB(0xFE46D595,  FIO_SeekSkipFile)
NSTUB(0xFE46D637,  FIO_WriteFile)
NSTUB(0xFE46D707,  FIO_CreateDirectory)
NSTUB(0xFE46D44F,  FIO_CreateFile)
NSTUB(0xFEE9A849,  FIO_FindFirstEx)
NSTUB(0xFE46D791,  FIO_GetFileSize)
NSTUB(0xFE46D3D9,  FIO_OpenFile)                            // or OpenFile?
NSTUB(0xFE46D4B9,  FIO_RemoveFile)
// NSTUB(    ???, _FIO_RenameFile)                          /* present on 60D.111, 6D.113, 600D.102, 650D.104, 550D.109, 1100D.105, EOSM.202, 700D.113 */


/** Slave Comm **/
NSTUB(0xFE3F3980,  BulkOutIPCTransfer)
NSTUB(0xFE3F3B04,  BulkInIPCTransfer)
