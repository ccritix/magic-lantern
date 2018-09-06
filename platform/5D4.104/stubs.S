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

/* Some functions are Thumb, others are ARM. The Thumb bit needs to be enabled manually. */

/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xFE0DD4F5,  cstart)                 /* calls bzero32 and create_init_task(..., init_task, ...) */
NSTUB(0xFE426B84,  bzero32)                /* zeros out a data structure */
NSTUB(0xFE42662C,  create_init_task)       /* low-level DryOS initialization */
NSTUB(0xFE0DD70D,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */

/** File I/O **/
NSTUB(0xFE48AD9B,  FIO_CloseFile)
NSTUB(0xFE48BF81,  FIO_FindClose)
NSTUB(0xFE48BECB,  FIO_FindNextEx)
NSTUB(0xFE48AB13,  FIO_ReadFile)
NSTUB(0xFE48ABAD,  FIO_SeekSkipFile)
NSTUB(0xFE48AC8F,  FIO_WriteFile)
NSTUB(0xFE48B735, _FIO_CreateDirectory)
NSTUB(0xFE48A9D3, _FIO_CreateFile)
NSTUB(0xFE48BDF5, _FIO_FindFirstEx)
NSTUB(0xFE48AF21, _FIO_GetFileSize)
NSTUB(0xFE48A929, _FIO_OpenFile)
NSTUB(0xFE48AA83, _FIO_RemoveFile)
NSTUB(0xFE48B4C9, _FIO_RenameFile)

/** GUI **/
//NSTUB(0x????????,  gui_main_struct)
//NSTUB(0x????????,  gui_task_list)
NSTUB(0xFE1710B3,  GUI_Control)
NSTUB(0xFE49FAFF,  SetGUIRequestMode)
NSTUB(0xFE48C60D,  gui_init_end)           /* EndGuiInit */
NSTUB(0xFE170F0D,  gui_main_task)

/** ASIF **/
//NSTUB(0x????????,  PowerAudioOutput)
NSTUB(0xFE42BB01,  SetAudioVolumeOut)                       // JC Best guess
//NSTUB(0x????????,  SetNextASIFADCBuffer)
//NSTUB(0x????????,  SetNextASIFDACBuffer)
//NSTUB(0x????????,  SetSamplingRate)
//NSTUB(0x????????,  StartASIFDMAADC)
//NSTUB(0x????????,  StartASIFDMADAC)
//NSTUB(0x????????,  StopASIFDMAADC)
//NSTUB(0x????????,  StopASIFDMADAC)

/** Audio **/
//NSTUB(0x????????, _audio_ic_read)
//NSTUB(0x????????, _audio_ic_write)                          // str:Reg_0x_02X_Data_04x

/** DMA **/
// NSTUB(0x????????, dma_memcpy)

/** Debug messages **/
NSTUB(0xC15,       DryosDebugMsg)            /* prefer the RAM version, to override it easily */

/** Dialog API **/
NSTUB(0xFE4BA9A0,  dialog_redraw)            /* not checked */
NSTUB(0xFE4BA5D8,  dialog_set_property_str)  /* not checked */

/** Dialog handlers **/
//NSTUB(0x????????,  ErrCardForLVApp_handler)
NSTUB(0xFE5902D3,  ErrForCamera_handler)                    // ERR70, ERR80 etc (DlgErrForCamera.c, AJ_DIALOG.HANDLER_DlgErrForCamera.c)
NSTUB(0xFE3F92B3,  LiveViewApp_handler)                     // Thumb function, but LSB not set in Canon code, why?
NSTUB(0xFE6E2FA1,  LiveViewLevelApp_handler)                // fixme: some other app may be at top level in LiveView
NSTUB(0xFE4729CD,  PlayMain_handler)                        // this should be active when reviewing a still image
NSTUB(0xFE2995D1,  PlayMovieGuideApp_handler)               // this should be active when playing a video
NSTUB(0xFE6DC209,  ShootOlcApp_handler)                     // this should be active in photo mode, outside LiveView

/** EDMAC **/
//NSTUB(0x????????,  ConnectReadEDmac)
//NSTUB(0x????????,  ConnectWriteEDmac)
//NSTUB(0x????????,  RegisterEDmacAbortCBR)
//NSTUB(0x????????,  RegisterEDmacCompleteCBR)
//NSTUB(0x????????,  RegisterEDmacPopCBR)
//NSTUB(0x????????,  SetEDmac)
//NSTUB(0x????????,  StartEDmac)
//NSTUB(0x????????,  UnregisterEDmacAbortCBR)
//NSTUB(0x????????,  UnregisterEDmacCompleteCBR)
//NSTUB(0x????????,  UnregisterEDmacPopCBR)

/** Electronic Level **/
NSTUB(0xFE4A7F59,  GUI_SetRollingPitchingLevelStatus)       // JC-C RequestRollingPitching - no side effects :)

/** Engio **/
//NSTUB(0x????????, _EngDrvOut)                               // AJ_EngDrvOut_1xVar_to_ShadowStruct
NSTUB(0xFE4DD624, _engio_write)         /* no mirror?! */
//NSTUB(0x????????,  shamem_read)                             // AJ_0x_engio_struct_n_R0_manipulation_to_get_ptr

/** Eventprocs (call by name) **/
NSTUB(0xFE48A7D7,  call)

/** ExMem **/
//NSTUB(0x????????,  AddMemoryChunk)
NSTUB(0xFE53A461,  AllocateContinuousMemoryResource)        // m_pfContAllocMemoryCBR
NSTUB(0xFE53A433,  AllocateMemoryResource)                  // m_pfAllocMemoryCBR
NSTUB(0xFE49A837,  CreateMemoryChunk)
NSTUB(0xFE49B2FF,  CreateMemorySuite)
NSTUB(0xFE49B019,  DeleteMemorySuite)          // or FEA8E248?
NSTUB(0xFE53A507,  FreeMemoryResource)                      // m_pfFreeMemoryCBR
NSTUB(0xFEA8E0B9,  GetFirstChunkFromSuite)     // or FE49AE88?
NSTUB(0xFE49AC89,  GetMemoryAddressOfMemoryChunk)
NSTUB(0xFE49B705,  GetNextMemoryChunk)
NSTUB(0xFE47CFA3,  SRM_AllocateMemoryResourceFor1stJob)
NSTUB(0xFE47F893,  SRM_FreeMemoryResourceFor1stJob)

/** GUI timers **/
NSTUB(0xFE55F219,  CancelDateTimer)                         // Look for "CancelDateTimer" or "StopDateTimer" (7D)
NSTUB(0xFE49F327,  CancelTimer)
NSTUB(0xFE3275EF,  SetHPTimerAfterNow)
NSTUB(0xFE32763F,  SetHPTimerNextTick)
NSTUB(0xFE49F2B1,  SetTimerAfter)

/** Interrupts **/
NSTUB(    0x4560,  pre_isr_hook)
NSTUB(    0x4564,  post_isr_hook)
NSTUB(   0x1F790,  isr_table_handler)
NSTUB(   0x1F794,  isr_table_param)

/** Making the card bootable **/
//NSTUB(0x????????,  sd_device)
//NSTUB(0x????????,  fsuDecodePartitionTable)                 // AJ_fsuGetPart_related   - called from loc_FE256364?

/** Memory allocation **/
NSTUB(0x80004AAB, _AllocateMemory)
NSTUB(0x80004C45, _FreeMemory)
NSTUB(0x80004955,  GetMemoryInformation)
NSTUB(0x80004929,  GetSizeOfMaxRegion)
NSTUB(0xFE426AB5, _alloc_dma_memory)
NSTUB(0xFE426AED, _free_dma_memory)
NSTUB(0xFE342681, _malloc)
NSTUB(0xFE3426B7, _free)

/** Message queues **/
NSTUB(    0x1E2B,  msg_queue_count)        /* used in "[TASK ERROR] GetNumOfEvent" */
NSTUB(    0x1BE1,  msg_queue_create)       /* CreateMessageQueue */
NSTUB(    0x1DF5,  msg_queue_post)         /* TryPostMessageQueue */
NSTUB(    0x1CA1,  msg_queue_receive)      /* ReceiveMessageQueue */

/** PTP **/
//NSTUB(0x????????,  ptp_register_handler)

/** Properties **/
NSTUB(0xFE49C89B, _prop_cleanup)           /* called at the end of property handlers */
NSTUB(0xFE42AA9F, _prop_request_change)    /* pRequestChange */
NSTUB(0xFE49C80B,  prop_deliver)           /* pDeliverProperty */
NSTUB(0xFE49C2B3,  prop_register_slave)    /* pRegisterSlave */

/** Recursive locks **/
//NSTUB(0x????????,  AcquireRecursiveLock)       // AJ_KernelDry_KerRLock.c
//NSTUB(0x????????,  CreateRecursiveLock)
//NSTUB(0x????????,  ReleaseRecursiveLock)       // AJ_KernelDry_KerRLock.c_p2

/** ResLock **/
NSTUB(0xFE9F11F9,  CreateResLockEntry)
NSTUB(0xFE9F13DB,  LockEngineResources)
NSTUB(0xFE2ED5A9,  UnLockEngineResources)

/** Semaphores **/
NSTUB(     0x1D1,  create_named_semaphore)     /* CreateBinarySemaphore */
NSTUB(     0x33F,  give_semaphore)
NSTUB(     0x2C7,  take_semaphore)

/** Task info **/
//NSTUB(0x????????,  task_max)
//NSTUB(0x????????,  is_taskid_valid)            // AJ_task_trampoline_related_p10
NSTUB(    0x45A4,  current_task)
NSTUB(    0x4584,  current_interrupt)

/** Task sleep **/
NSTUB(     0x5B9,  msleep)

/** Tasks **/
//NSTUB(0x????????,  task_dispatch_hook)
NSTUB(     0x6D9,  task_create)
//NSTUB(0x????????,  task_trampoline)

/** Misc **/
NSTUB(0xFE4C205D,  FSUunMountDevice)
NSTUB(0xFE35DB17,  LoadCalendarFromRTC)
NSTUB(0xFE959E11,  vsnprintf)
