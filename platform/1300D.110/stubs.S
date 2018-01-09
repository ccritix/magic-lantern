/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.5 firmware for the 1100d / Rebel T3.
 *
 */
/*
 * Copyright (C) 2011 Magic Lantern Team
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

.text


// Initial Stubs taken from DeinGott post #105 from http://www.magiclantern.fm/forum/index.php?topic=17969

/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )                        // 0xF8010000
NSTUB(0xFE0C3A24,  cstart)
NSTUB(0x00029898,  bzero32)
NSTUB(0xFE0C3AF8,  create_init_task)
NSTUB(0xFE1296C8,  init_task)
NSTUB(   0x61123,  additional_version)
NSTUB(0xFE11F394,  DryosDebugMsg)
NSTUB(    0x38FC,  task_create)

/** File I/O **/
NSTUB(0xFE2A43FC,  FIO_CloseFile)
NSTUB(0xFE2A53D0,  FIO_FindClose)
NSTUB(0xFE2A52F0,  FIO_FindNextEx)
NSTUB(0xFE2A41AC, _FIO_ReadFile)
NSTUB(0xFE2A425C,  FIO_SeekSkipFile)
NSTUB(0xFE2A434C, _FIO_WriteFile)
NSTUB(0xFE2A4C3C, _FIO_CreateDirectory)
NSTUB(0xFE2A4058, _FIO_CreateFile)
NSTUB(0xFE2A51FC, _FIO_FindFirstEx)
NSTUB(0xFE2A4578, _FIO_GetFileSize)
NSTUB(0xFE2A3F9C, _FIO_OpenFile)
NSTUB(0xFE2A4104, _FIO_RemoveFile)
NSTUB(0xFE2A4A74, _FIO_RenameFile)
