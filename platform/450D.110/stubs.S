/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.9 firmware for the 450d.
 *
 * \todo Sort this file?  Generate it from the IDA map?
 */
/*
 * Copyright (C) 2010 Magic Lantern Team
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

NSTUB( ROMBASEADDR, firmware_entry )

NSTUB(0xFFCFEBD4, AcquireRecursiveLock)
NSTUB(0xFFD16D74, AllocateMemory)
NSTUB(0xFFD16D74, _AllocateMemory)
NSTUB(0xFF95D554, CreateDialogBox)
NSTUB(0xFFD065B8, CreateRecursiveLock)
    //NSTUB(0xff2ff7c4, DeleteDialogBox)
NSTUB(0xFFD07654, DryosDebugMsg)
NSTUB(0xFFCCD90C, FIO_FindClose) // AJ__switch_0x1A50_n_calls_fstOpenDir	 FIO_FindClose
NSTUB(0xFFCCD4B4, FIO_CloseFile)
NSTUB(0xFFCCD6DC, _FIO_CreateDirectory)
NSTUB(0xFFCCD104, _FIO_CreateFile)
NSTUB(0xFFCCCCD0, _FIO_FindFirstEx)
NSTUB(0xFFCCCE34, FIO_FindNextEx)
NSTUB(0xFFCCD568, _FIO_GetFileSize)
NSTUB(0xFFCCD038, _FIO_OpenFile)
NSTUB(0xFFCCD28C, FIO_ReadFile)
NSTUB(0xFFCCD1C8, _FIO_RemoveFile)
NSTUB(0xFFCCD3FC, FIO_WriteFile)
NSTUB(0xFFD16DB4, FreeMemory)
NSTUB(0xFFD16DB4, _FreeMemory)
    //NSTUB(0xff022004, GUI_ChangeMode)
NSTUB(0xFF861F8C, GUI_Control)
NSTUB(0xFF892FBC, GUI_GetFirmVersion)
NSTUB(0xFF91E3BC, GetCFnData)
NSTUB(0xFF8DA6A0, GuiEdLedBlink)
NSTUB(0xFF8DA670, GuiEdLedOff)
NSTUB(0xFF8DA640, GuiEdLedOn)
NSTUB(0xFFCF3C34, LoadCalendarFromRTC)
NSTUB(0xFFC6DD48, MuteOff_0)
NSTUB(0xFFC6DDC4, MuteOn_0)
NSTUB(0xFF97D5C4, RedrawDisplay)
NSTUB(0xFFCFECFC, ReleaseRecursiveLock) // AJ_KernelDry_KerRLock.c_p2
    //NSTUB(0xff15fd74, RemoteRelease)
    //NSTUB(0xff06f2fc, SetBitmapVramAddress)
NSTUB(0xFF91E43C, SetCFnData)
NSTUB(0xFF883E88, SetGUIRequestMode)
    //NSTUB(0xff072f7c, _audio_ic_read)
    //NSTUB(0xff0730c8, _audio_ic_write)
NSTUB(   0x4154, additional_version) // or 4154	 not sure
NSTUB(0xFFD12088, alloc_dma_memory)
NSTUB(0xFFD12088, _alloc_dma_memory)
    NSTUB(   0x30A90, bmp_vram_info ) // need checking
//NSTUB(0xffd7d718, bzero32) // memset at 0xffd7d718
NSTUB(0xFFCF9788, call)
    NSTUB(0xC0220000, camera_engine)
//NSTUB(0xffd1989c, cfReadBlk)
NSTUB(   0x314F8, cf_device) // not sure
NSTUB(0xFFD2073C, cli_save)
    //NSTUB(0xff01559c, create_init_task)
NSTUB(0xFFCFEE00, create_named_semaphore)
    //NSTUB(0xff010fb0, cstart)
    //NSTUB(0xff2fe9f4, ctrlman_dispatch_event)
NSTUB(0xFF95EF4C, dialog_redraw)
NSTUB(0xFF8F0758, dialog_set_property_str)
    //NSTUB(    0x2b18, dm_names)
    //NSTUB(0xffd4cb6c, dm_set_store_level)
NSTUB(0xFFD16DB4, free) // not good	 points to FreeMemory
NSTUB(0xFFD16DB4, _free) // not good	 points to FreeMemory
NSTUB(0xFFD120B4, free_dma_memory)
NSTUB(0xFFD120B4, _free_dma_memory)
    //NSTUB(0xff3d7798, fsuDecodePartitionTable) // AJ_fsuGetPart_related
NSTUB(0xFFCFF390, give_semaphore)
NSTUB(0xFF8646A4, gui_change_lcd_state_post)
NSTUB(0xFF863B18, gui_change_mode)
NSTUB(0xFF8645E4, gui_change_shoot_type_post)
NSTUB(0xFF861538, gui_init_end)
NSTUB(0xFF8B3268, gui_init_event)
NSTUB(0xFF863EE4, gui_local_post)
NSTUB(    0x17530, gui_main_struct)
NSTUB(0xFF861A28, gui_main_task)
NSTUB(0xFF862734, gui_massive_event_loop)
NSTUB(0xFF864380, gui_other_post)
NSTUB(0xFF864514, gui_post_10000062)
NSTUB(0xFF95BC58, gui_task_create)
NSTUB(0xFF95BDA8, gui_task_destroy)
NSTUB(   0x4AF8, gui_task_list)
NSTUB(0xFFCFD6F4, gui_timer_something)
    //NSTUB(    0x3624, gui_timer_struct)
    NSTUB(0xdeadbeef, init_task)
NSTUB(0xFFD16D74, malloc) // not good	 points to AllocateMemory
NSTUB(0xFFD16D74, _malloc)
NSTUB(0xFFCFE720, msg_queue_post)
NSTUB(0xFFCFE03C, msg_queue_receive)
NSTUB(0xFFCFF768, msleep)
    //NSTUB(0xff1e0e04, mvrFixQScale)
    //NSTUB(0xff1e0870, mvrSetDeblockingFilter)
    //NSTUB(0xff1e08e0, mvrSetDefQScale)
    //NSTUB(0xff1e0e24, mvrSetPrintMovieLog)
    //NSTUB(    0xa39c, mvr_config)
    //NSTUB(0xff078e6c, oneshot_timer)
NSTUB(0xFFC35324, _prop_cleanup)
NSTUB(0xFFC35144, prop_deliver)
NSTUB(0xFFC35458, prop_get_value)
NSTUB(0xFFC35200, prop_register_slave)
NSTUB(0xFFC353AC, _prop_request_change)

NSTUB(0xFFCF98C4, register_func)
	//NSTUB(0xFFB97BF4, sdReadBlk)                      // might be good (dumps=1	 score=8.2)
	//NSTUB(   0x208D8, sd_device)
NSTUB(0xFFD0ADA4, sei_restore)
    //NSTUB(    0x1f54, sounddev)
    //NSTUB(0xff063d64, sounddev_task)
    //NSTUB(0xFF0640EC, sounddev_active_in)
NSTUB(0xFFCFF1F4, take_semaphore)
NSTUB(0xFFCFFAB4, task_create)
    //NSTUB(    0x1934, task_dispatch_hook )
    //NSTUB(0xff084ca4, task_trampoline)
    //NSTUB(0xff2cb1e0, vram_get_number)
NSTUB(0x00030528, vram_info)
NSTUB(0xFFD08758, vsnprintf)

NSTUB(0xFF8C1EA0, LiveViewApp_handler)
NSTUB(0xFF8A6A04, PlayMain_handler)
    //NSTUB(0xFF42B700, PlayMovieGuideApp_handler)
NSTUB(0xFF8D560C, OlcAFFrameApp_handler)
NSTUB(0xdeadbeef, ErrCardForLVApp_handler)
    //NSTUB(0xFF3674A4, LiveViewWbApp_handler)
NSTUB(0xFF8D64DC, ErrForCamera_handler) // ERR70	 ERR80 etc (DlgErrForCamera.c	 AJ_DIALOG.HANDLER_DlgErrForCamera.c)

    //NSTUB(0xff1f6b20, _engio_write)
    //NSTUB(0xff1f664c, shamem_read) // AJ_0x8FB0_engio_struct_n_R0_manipulation_to_get_ptr
    //NSTUB(0xff1f675c, _EngDrvOut) // AJ_EngDrvOut_1xVar_to_ShadowStruct

NSTUB(0xFF8BBA54, ShootOlcApp_handler) // AJ_DIALOG.HANDLER_DlgShootOlcInfo.c

    //NSTUB(0x29A9C, LCD_Palette) // in InitializeBitmapDisplayDevice	 right after 0xc0f14800

NSTUB(0xFFD16E84, GetMemoryInformation)

NSTUB(0xFFD06204, msg_queue_create)

    //NSTUB(0xff0372b4, PD_RemoteRelease)
    //NSTUB( 0xff16004c, PtpDps_remote_release_SW1_SW2_worker ) // called from: ptpRemoteRelease Occupy

	// for task information
    //NSTUB(0x2B24, task_max)
    //NSTUB(0xFF087940, is_taskid_valid) // AJ_task_trampoline_related_p10
    //NSTUB(0xff08779c, get_obj_attr) // AJ_checks_if_Process_id_created_by_Dryos
    //NSTUB(0xff014c10, get_current_task)

NSTUB(0xFFD17E18, AllocateMemoryResource) // m_pfAllocMemoryCBR
NSTUB(0xFFD17E6C, AllocateContinuousMemoryResource) // m_pfContAllocMemoryCBR
NSTUB(0xFFD17EC0, FreeMemoryResource) // m_pfFreeMemoryCBR
NSTUB(0xFFD03548, GetFirstChunkFromSuite) // AJ_PackMemory_PackMem_p3
NSTUB(0xFFD031B4, GetMemoryAddressOfMemoryChunk)

    //NSTUB(0xff07365c, PowerAudioOutput)
    //NSTUB(0xff061c44, StartASIFDMADAC)
    //NSTUB(0xFF061A88, StartASIFDMAADC)
    //NSTUB(0xff061d20, StopASIFDMADAC)
    //NSTUB(0xFF0621C4, SetNextASIFADCBuffer) // called by SetNextUINT8ASIFADCBuffer and SetNextINT16ASIFADCBuffer
    //NSTUB(0xFF06227C, SetNextASIFDACBuffer)
    //NSTUB(0xff0736f4, SetSamplingRate )
    //NSTUB(0xFF073944, SetAudioVolumeOut)

    //NSTUB(0xFF06EDD0, AsyncEnableImagePhysicalScreenParameter)
    //NSTUB(0xff06e8b0, EnableImagePhysicalScreenParameter)

NSTUB(0xFF8A8C78, StartPlayProtectGuideApp)
NSTUB(0xFF8A9144, StopPlayProtectGuideApp)

NSTUB(0xFFCFFCC0, DeleteTask)
NSTUB(0xFFD0068C, QueryTaskByName)

NSTUB(0x30A98, LCD_Palette)
NSTUB(0x30AD8, RGB_Palette)
NSTUB(0x309C8, PB_Palette)

NSTUB(0xFFC64BE8, SetRGBPaletteToDisplayDevice)
NSTUB(0xFF8F08C4, ChangeColorPalette)
NSTUB(0xFFC63A88, SetParameterToBitmapDisplayDevice)

NSTUB(0xFFC6D1EC, EnableBitmapVBufferForPlayBackAndWaiting)

NSTUB(0xFFC6B6B0, BmpDDev_give_semaphore)
NSTUB(0xFFC6B660, BmpDDev_take_semaphore)

NSTUB(0xFF81594C, bindGUISwitchCBR)

NSTUB(0xFFCFDC18, register_interrupt)
NSTUB(0xFFC3B624, SIO3_ISR)
NSTUB(0xFFC3B55C, MREQ_ISR)

NSTUB(0xFFD0A7A0, TryPostEvent)
NSTUB(0xFFD0A800, TryPostEvent_end)

NSTUB(0xFFD1180C, TryPostStageEvent)
NSTUB(0xFFD1197C, TryPostStageEvent_end) // PendStageEvent

NSTUB(0xFFD28528, get_current_task) // 0x355C0
NSTUB(0xFFD282EC, get_task_info)
NSTUB(0xFFD29C10, get_active_task_list)
NSTUB(   0x355C0, current_task)

NSTUB(0xFFD0A0D4, create_task_cmd_shell)

NSTUB(0xFFB4AB18, ptp_register_handler)
NSTUB(0xFFB42198, ptp_register_handlers_0x9800)

NSTUB(0xFFD07654, DM_TryPostEvent)

NSTUB(0xFF84C384, FA_Release)

/** EDMAC routines **/
NSTUB(0xFFCADCA0, SetEDmac)
NSTUB(0xFFCADD00, StartEDmac)
NSTUB(0xFFCADDC0, PopEDmac)
//NSTUB([idk], AbortEDmac)
NSTUB(0xFFCADCC0, ConnectWriteEDmac)
NSTUB(0xFFCADCDC, ConnectReadEDmac)

/** keep the old name until all are refcatored **/
//NSTUB(0xFFCADDA0, EDMAC_RegisterCompleteCBR)

/** register/unregister CBR names **/
NSTUB(0xFFCADDA0, RegisterEDmacCompleteCBR)
NSTUB(0xFFCADDD8, RegisterEDmacAbortCBR)
NSTUB(0xFFCADE58, RegisterEDmacPopCBR)
NSTUB(0xFFCADDC0, UnregisterEDmacCompleteCBR)
NSTUB(0xFFCADE20, UnregisterEDmacAbortCBR)
NSTUB(0xFFCADE78, UnregisterEDmacPopCBR)

