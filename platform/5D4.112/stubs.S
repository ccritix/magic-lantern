/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.1.2 firmware for the 5D Mark IV.
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

/* Some functions are Thumb, others are ARM. The Thumb bit needs to be enabled manually. */

/** Startup **/
NSTUB( ROMBASEADDR, firmware_entry )
NSTUB(0xFE0DD4F5,  cstart)                 /* calls bzero32 and create_init_task(..., init_task, ...) */
NSTUB(0xFE428CD0,  bzero32)                /* zeros out a data structure */
NSTUB(0xFE428778,  create_init_task)       /* low-level DryOS initialization */
NSTUB(0xFE0DD70D,  init_task)              /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */

/** File I/O **/
NSTUB(0xFE48D4B3,  FIO_CloseFile)
NSTUB(0xFE48E699,  FIO_FindClose)
NSTUB(0xFE48E5E3,  FIO_FindNextEx)
NSTUB(0xFE48D22B,  FIO_ReadFile)
NSTUB(0xFE48D2C5,  FIO_SeekSkipFile)
NSTUB(0xFE48D3A7,  FIO_WriteFile)
NSTUB(0xFE48DE4D, _FIO_CreateDirectory)
NSTUB(0xFE48D0EB, _FIO_CreateFile)
NSTUB(0xFE48E50D, _FIO_FindFirstEx)
NSTUB(0xFE48D639, _FIO_GetFileSize)
NSTUB(0xFE48D041, _FIO_OpenFile)
NSTUB(0xFE48D19B, _FIO_RemoveFile)
NSTUB(0xFE48DBE1, _FIO_RenameFile)

/** GUI **/
//NSTUB(0x????????,  gui_main_struct)
//NSTUB(0x????????,  gui_task_list)
NSTUB(0xFE171897,  GUI_Control)
NSTUB(0xFE4A2287,  SetGUIRequestMode)
NSTUB(0xFE48ED25,  gui_init_end)           /* EndGuiInit */
NSTUB(0xFE1716F1,  gui_main_task)

/** ASIF **/
//NSTUB(0x????????,  PowerAudioOutput)
NSTUB(0xFE42DD19,  SetAudioVolumeOut)                       // JC Best guess
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
// ROM: 0xFE428CD8
NSTUB(0xC15,       DryosDebugMsg)            /* prefer the RAM version, to override it easily */

/** Dialog API **/
NSTUB(0xFE4BD128,  dialog_redraw)            /* not checked */
NSTUB(0xFE4BCD60,  dialog_set_property_str)  /* not checked */

/** Dialog handlers **/
//NSTUB(0x????????,  ErrCardForLVApp_handler)
NSTUB(0xFE5941E7,  ErrForCamera_handler)                    // ERR70, ERR80 etc (DlgErrForCamera.c, AJ_DIALOG.HANDLER_DlgErrForCamera.c)
NSTUB(0xFE3FB2F1,  LiveViewApp_handler)                     // Thumb function, but LSB not set in Canon code, why?
NSTUB(0xFE6E7895,  LiveViewLevelApp_handler)                // fixme: some other app may be at top level in LiveView
NSTUB(0xFE4750E5,  PlayMain_handler)                        // this should be active when reviewing a still image
NSTUB(0xFE29A7A1,  PlayMovieGuideApp_handler)               // this should be active when playing a video
NSTUB(0xFE6E0AFD,  ShootOlcApp_handler)                     // this should be active in photo mode, outside LiveView

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
NSTUB(0xFE4AA6E1,  GUI_SetRollingPitchingLevelStatus)       // JC-C RequestRollingPitching - no side effects :)

/** Engio **/
//NSTUB(0x????????, _EngDrvOut)                               // AJ_EngDrvOut_1xVar_to_ShadowStruct
NSTUB(0xFE4DFDF8, _engio_write)         /* no mirror?! */
//NSTUB(0x????????,  shamem_read)                             // AJ_0x_engio_struct_n_R0_manipulation_to_get_ptr

/** Eventprocs (call by name) **/
NSTUB(0xFE48CEEF,  call)

/** ExMem **/
//NSTUB(0x????????,  AddMemoryChunk)
NSTUB(0xFE53DFED,  AllocateContinuousMemoryResource)        // m_pfContAllocMemoryCBR
NSTUB(0xFE53DFBF,  AllocateMemoryResource)                  // m_pfAllocMemoryCBR
NSTUB(0xFE49CFBF,  CreateMemoryChunk)
NSTUB(0xFE49DA87,  CreateMemorySuite)
NSTUB(0xFE49D7A1,  DeleteMemorySuite)          // or FEA8E248?
NSTUB(0xFE53E093,  FreeMemoryResource)                      // m_pfFreeMemoryCBR
NSTUB(0xFEA944A9,  GetFirstChunkFromSuite)     // or FE49AE88?
NSTUB(0xFE49D411,  GetMemoryAddressOfMemoryChunk)
NSTUB(0xFE49DE8D,  GetNextMemoryChunk)
NSTUB(0xFE47F6BB,  SRM_AllocateMemoryResourceFor1stJob)
NSTUB(0xFE481FAB,  SRM_FreeMemoryResourceFor1stJob)

/** GUI timers **/
NSTUB(0xFE562FD5,  CancelDateTimer)                         // Look for "CancelDateTimer" or "StopDateTimer" (7D)
NSTUB(0xFE4A1AAF,  CancelTimer)
NSTUB(0xFE328A63,  SetHPTimerAfterNow)
NSTUB(0xFE328AB3,  SetHPTimerNextTick)
NSTUB(0xFE4A1A39,  SetTimerAfter)

/** Interrupts **/
NSTUB(    0x4560,  pre_isr_hook)
NSTUB(    0x4564,  post_isr_hook)
NSTUB(   0x1F968,  isr_table_handler)
NSTUB(   0x1F96C,  isr_table_param)

/** Making the card bootable **/
//NSTUB(0x????????,  sd_device)
//NSTUB(0x????????,  fsuDecodePartitionTable)                 // AJ_fsuGetPart_related   - called from loc_FE256364?

/** Memory allocation **/
NSTUB(0x80004AAB, _AllocateMemory)
NSTUB(0x80004C45, _FreeMemory)
NSTUB(0x80004955,  GetMemoryInformation)
NSTUB(0x80004929,  GetSizeOfMaxRegion)
NSTUB(0xFE428C01, _alloc_dma_memory)
NSTUB(0xFE428C39, _free_dma_memory)
NSTUB(0xFE343D61, _malloc)
NSTUB(0xFE343D97, _free)

/** Message queues **/
NSTUB(    0x1E2B,  msg_queue_count)        /* used in "[TASK ERROR] GetNumOfEvent" */
NSTUB(    0x1BE1,  msg_queue_create)       /* CreateMessageQueue */
NSTUB(    0x1DF5,  msg_queue_post)         /* TryPostMessageQueue */
NSTUB(    0x1CA1,  msg_queue_receive)      /* ReceiveMessageQueue */

/** PTP **/
//NSTUB(0x????????,  ptp_register_handler)

/** Properties **/
NSTUB(0xFE49F023, _prop_cleanup)           /* called at the end of property handlers */
NSTUB(0xFE42CCB7, _prop_request_change)    /* pRequestChange */
NSTUB(0xFE49EF93,  prop_deliver)           /* pDeliverProperty */
NSTUB(0xFE49EA3B,  prop_register_slave)    /* pRegisterSlave */

/** Recursive locks **/
//NSTUB(0x????????,  AcquireRecursiveLock)       // AJ_KernelDry_KerRLock.c
//NSTUB(0x????????,  CreateRecursiveLock)
//NSTUB(0x????????,  ReleaseRecursiveLock)       // AJ_KernelDry_KerRLock.c_p2

/** ResLock **/
NSTUB(0xFE9F6865,  CreateResLockEntry)
NSTUB(0xFE9F6A47,  LockEngineResources)
NSTUB(0xFE2EE86D,  UnLockEngineResources)

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
NSTUB(0xFE4C47E5,  FSUunMountDevice)
NSTUB(0xFE35F217,  LoadCalendarFromRTC)
NSTUB(0xFE95F425,  vsnprintf)

/** Logging experiments */
NSTUB(0xFE129BEF, dump_file)

/** MPU communication **/
NSTUB(0xFE291317,  mpu_send)                  // "dwSize < TXBD_DATA_SIZE"
NSTUB(0xFE32693F,  mpu_recv)                  // passed as last argument by InitializeIntercom and eventually stored into mpu_recv_cbr
NSTUB(    0x7A64,  mpu_recv_cbr)              // mpu_recv is called indirectly through this function pointer
NSTUB(   0x32EEC,  mpu_send_ring_buffer)      // ring buffer used in mpu_send
NSTUB(    0x7A48,  mpu_send_ring_buffer_tail) // ring buffer index incremented in mpu_send
NSTUB(   0x32DAC,  mpu_recv_ring_buffer)      // ring buffer used in SIO3_ISR, subroutine that processes two chars at a time
NSTUB(    0x7A40,  mpu_recv_ring_buffer_tail) // ring buffer index incremented in the above subroutine

/** Memory info **/
NSTUB(0xFE29FF7D,  malloc_info)                             // Thumb
NSTUB(0xFE2A0015,  sysmem_info)                             // Thumb
NSTUB(0xFE2E02D1,  smemShowFix)                             // Thumb
