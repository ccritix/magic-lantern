/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.4 firmware for the 5D Mark IV.
 *
 */
/*
 * Copyright (C) 2017 Magic Lantern Team
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

// some functions are copied to RAM at around ????????; they have to be called from RAM...
#define RAM_OFFSET 0x????????  // = 0x???????? - 0x????


/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0x????????,  cstart)
NSTUB(0x????????,  bzero32)
NSTUB(0x????????,  create_init_task)
NSTUB(0xFE0DD70C,  init_task)

NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0x????????,  cstart)
NSTUB(0x???????? - RAM_OFFSET,  bzero32)
NSTUB(0x???????? - RAM_OFFSET,  create_init_task)
NSTUB(0x????????,  init_task)
NSTUB(0x????????,  additional_version)                      // GuiDataStorage

/** File I/O **/
NSTUB(0xFE48AD9A,  FIO_CloseFile)
NSTUB(0xFE48BF80,  FIO_FindClose)                           // proper name: FindClose
NSTUB(0xFE48BECA,  FIO_FindNextEx)
NSTUB(0xFE48AB12,  FIO_ReadFile)
NSTUB(0xFE48ABAC,  FIO_SeekSkipFile)
NSTUB(0xFE48AC8E,  FIO_WriteFile)
NSTUB(0xFE48B734, _FIO_CreateDirectory)
NSTUB(0xFE48A9D2, _FIO_CreateFile)
NSTUB(0xFE48BDF4, _FIO_FindFirstEx)
NSTUB(0xFE48AF20, _FIO_GetFileSize)
NSTUB(0xFE48A928, _FIO_OpenFile)
NSTUB(0xFE48AA82, _FIO_RemoveFile)
// NSTUB(????????, _FIO_RenameFile)

/** GUI **/
NSTUB(0x????????,  gui_main_struct)
NSTUB(0x????????,  gui_task_list)
NSTUB(0xFE1710B2,  GUI_Control)
NSTUB(0xFE49FAFE,  SetGUIRequestMode)
NSTUB(0xFE48C60C,  gui_init_end)
NSTUB(0xFE170F0C,  gui_main_task)
NSTUB(0xFE28DB54,  gui_massive_event_loop)

/** ASIF **/
NSTUB(0x????????,  PowerAudioOutput)
NSTUB(0x????????,  SetAudioVolumeOut)
NSTUB(0x????????,  SetNextASIFADCBuffer)
NSTUB(0x????????,  SetNextASIFDACBuffer)
NSTUB(0x????????,  SetSamplingRate)
NSTUB(0x????????,  StartASIFDMAADC)
NSTUB(0x????????,  StartASIFDMADAC)
NSTUB(0x????????,  StopASIFDMAADC)
NSTUB(0x????????,  StopASIFDMADAC)

/** Audio **/
NSTUB(0x????????, _audio_ic_read)
NSTUB(0x????????, _audio_ic_write)                          // str:Reg_0x_02X_Data_04x

/** Bitmap **/
NSTUB(0x????????,  LCD_Palette)                             // TerminateBitmapDisplayDevice
NSTUB(0x????????,  bmp_vram_info)                           // SetBitmapVramAddress - as in 60D

/** Camera interface? **/
NSTUB(0x????????,  camera_engine)                           // HDMIDetectISR

/** DMA **/
// NSTUB(0x???????? - RAM_OFFSET, dma_memcpy)

/** Debug messages **/
NSTUB(0x????????,  dm_names)
NSTUB(0xFE426B8C - RAM_OFFSET,  DryosDebugMsg)
NSTUB(0xFE9F1AFC - RAM_OFFSET,  dm_set_store_level)

/** Dialog API **/
NSTUB(0xFE4BA9A0,  dialog_redraw)
NSTUB(0xFE4BA5D8,  dialog_set_property_str)

/** Dialog handlers **/
NSTUB(0x????????,  ErrCardForLVApp_handler)
NSTUB(0xFE590348,  ErrForCamera_handler)                    // ERR70, ERR80 etc (DlgErrForCamera.c, AJ_DIALOG.HANDLER_DlgErrForCamera.c)
NSTUB(0x????????,  HideUnaviFeedBack_maybe)                 // look for StartUnaviFeedBackTimer
NSTUB(0x????????,  LiveViewApp_handler)
NSTUB(0x????????,  LiveViewLevelApp_handler)
NSTUB(0x????????,  LiveViewWbApp_handler)
NSTUB(0x????????,  PlayMain_handler)
NSTUB(0x????????,  PlayMovieGuideApp_handler)
NSTUB(0x????????,  ShootOlcApp_handler)                     // AJ_DIALOG.HANDLER_DlgShootOlcInfo.c

/** EDMAC **/
NSTUB(0x???????? - RAM_OFFSET,  ConnectReadEDmac)
NSTUB(0x???????? - RAM_OFFSET,  ConnectWriteEDmac)
NSTUB(0x???????? - RAM_OFFSET,  RegisterEDmacAbortCBR)
NSTUB(0x???????? - RAM_OFFSET,  RegisterEDmacCompleteCBR)
NSTUB(0x???????? - RAM_OFFSET,  RegisterEDmacPopCBR)
NSTUB(0x???????? - RAM_OFFSET,  SetEDmac)
NSTUB(0x???????? - RAM_OFFSET,  StartEDmac)
NSTUB(0x???????? - RAM_OFFSET,  UnregisterEDmacAbortCBR)
NSTUB(0x???????? - RAM_OFFSET,  UnregisterEDmacCompleteCBR)
NSTUB(0x???????? - RAM_OFFSET,  UnregisterEDmacPopCBR)

/** Electronic Level **/
NSTUB(0x????????,  GUI_SetRollingPitchingLevelStatus)       // RequestRollingPitching - no side effects :)

/** Engio **/
NSTUB(0x????????, _EngDrvOut)                               // AJ_EngDrvOut_1xVar_to_ShadowStruct
NSTUB(0x????????, _engio_write)
NSTUB(0x????????,  shamem_read)                             // AJ_0x_engio_struct_n_R0_manipulation_to_get_ptr

/** Eventprocs (call by name) **/
NSTUB(0xFE48A7D6,  call)

/** ExMem **/
NSTUB(0x???????? - RAM_OFFSET,  AddMemoryChunk)
NSTUB(0xFE53A460,  AllocateContinuousMemoryResource)        // m_pfContAllocMemoryCBR
NSTUB(0xFE53A432,  AllocateMemoryResource)                  // m_pfAllocMemoryCBR
NSTUB(0xFE49A836 - RAM_OFFSET,  CreateMemoryChunk)
NSTUB(0xFE49B2FE - RAM_OFFSET,  CreateMemorySuite)
NSTUB(0xFE49B018 - RAM_OFFSET,  DeleteMemorySuite)          // or FEA8E248?
NSTUB(0xFE53A506,  FreeMemoryResource)                      // m_pfFreeMemoryCBR
NSTUB(0xFEA8E0B8 - RAM_OFFSET,  GetFirstChunkFromSuite)     // or FE49AE88?
NSTUB(0x???????? - RAM_OFFSET,  GetMemoryAddressOfMemoryChunk)
NSTUB(0x???????? - RAM_OFFSET,  GetNextMemoryChunk)
NSTUB(0x????????,  SRM_AllocateMemoryResourceFor1stJob)
NSTUB(0x????????,  SRM_FreeMemoryResourceFor1stJob)

/** GUI timers **/
NSTUB(0xFE55F218,  CancelDateTimer)                         // Look for "CancelDateTimer" or "StopDateTimer" (7D)
NSTUB(0x???????? - RAM_OFFSET,  CancelTimer)
NSTUB(0x???????? - RAM_OFFSET,  SetHPTimerAfterNow)
NSTUB(0x???????? - RAM_OFFSET,  SetHPTimerNextTick)
NSTUB(0x???????? - RAM_OFFSET,  SetTimerAfter)

/** Interrupts **/
NSTUB(0x???????? - RAM_OFFSET,  post_isr_hook)
NSTUB(0x???????? - RAM_OFFSET,  pre_isr_hook)

/** Making the card bootable **/
NSTUB(0x????????,  cf_device_ptr)                           // cf_device is at MEM(MEM(0x) + 0x)
NSTUB(0x????????,  sd_device)
NSTUB(0x????????,  fsuDecodePartitionTable)                 // AJ_fsuGetPart_related   - called from loc_FE256364?

/** Memory allocation **/
NSTUB(0xFE9F5530 - RAM_OFFSET,  GetMemoryInformation)       // or FEF1CC90 ?
NSTUB(0x???????? - RAM_OFFSET,  GetSizeOfMaxRegion)         // AJ_FC_sys_free_p2
NSTUB(0x???????? - RAM_OFFSET, _AllocateMemory)
NSTUB(0x???????? - RAM_OFFSET, _FreeMemory)
NSTUB(0x???????? - RAM_OFFSET, _alloc_dma_memory)
NSTUB(0x????????, _free)                                    // see "can't malloc"
NSTUB(0x???????? - RAM_OFFSET, _free_dma_memory)
NSTUB(0x????????, _malloc)

/** Message queues **/
NSTUB(0xFE87C852 - RAM_OFFSET,  msg_queue_count)            // or FEF175D6?
NSTUB(0x???????? - RAM_OFFSET,  msg_queue_create)
NSTUB(0x???????? - RAM_OFFSET,  msg_queue_post)
NSTUB(0x???????? - RAM_OFFSET,  msg_queue_receive)

/** PTP **/
NSTUB(0x????????,  ptp_register_handler)

/** Properties **/
NSTUB(0x????????, _prop_cleanup)
NSTUB(0x????????, _prop_request_change)
NSTUB(0x????????,  prop_deliver)
NSTUB(0x????????,  prop_register_slave)

/** Recursive locks **/
NSTUB(0x???????? - RAM_OFFSET,  AcquireRecursiveLock)       // AJ_KernelDry_KerRLock.c
NSTUB(0x???????? - RAM_OFFSET,  CreateRecursiveLock)
NSTUB(0x???????? - RAM_OFFSET,  ReleaseRecursiveLock)       // AJ_KernelDry_KerRLock.c_p2

/** ResLock **/
NSTUB(0x????????,  CreateResLockEntry)
NSTUB(0x????????,  LockEngineResources)
NSTUB(0x????????,  UnLockEngineResources)

/** Semaphores **/
NSTUB(0x???????? - RAM_OFFSET,  create_named_semaphore)
NSTUB(0x???????? - RAM_OFFSET,  give_semaphore)
NSTUB(0x???????? - RAM_OFFSET,  take_semaphore)

/** Task info **/
NSTUB(0x????????,  task_max)
NSTUB(0x???????? - RAM_OFFSET,  is_taskid_valid)            // AJ_task_trampoline_related_p10
NSTUB(0x????????,  current_task)
NSTUB(0x????????,  current_interrupt)                       // in interrupt handler (0x), where MEM(C0201004) is stored

/** Task sleep **/
NSTUB(0x???????? - RAM_OFFSET,  msleep)

/** Tasks **/
NSTUB(0x????????,  task_dispatch_hook)
NSTUB(0x???????? - RAM_OFFSET,  task_create)
NSTUB(0x???????? - RAM_OFFSET,  task_trampoline)

/** Misc **/
NSTUB(0xFE4C205C,  FSUunMountDevice)
NSTUB(0xFE35DB16,  LoadCalendarFromRTC)
NSTUB(0xFE959E10 - RAM_OFFSET,  vsnprintf)
