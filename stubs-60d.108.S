/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.0.8 firmware for the 60d.
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

.text

#define NSTUB(addr,name) \
	.global name; \
	name = addr

NSTUB( ROMBASEADDR, firmware_entry )

/** These must be found first for any progress to be made */
NSTUB( 0xff011028, cstart )
NSTUB( 0xff538214, bzero32 ) // called by cstart()
NSTUB( 0xff0193ac, init_task ) // passed as arg to create_init_task, look for dmSetup
NSTUB( 0xff017b54, create_init_task ) 

#if 0
/* to be changed in 5d-hack.c */

//This should be a "BL cstart" instruction.
INSTR( 0xFF01019C ) = RET_INSTR;

// Reserve memory after the BSS for our application
INSTR( 0xFF0110D0 ) = (uintptr_t) _bss_end;

// Fix the calls to bzero32() and create_init_task()
FIXUP_BRANCH( 0xFF011038, bzero32 );
FIXUP_BRANCH( 0xFF0110C0, create_init_task );

// Set our init task to run instead of the firmware one
INSTR( 0xFF0110DC ) = (uint32_t) my_init_task;

// Determine the task address
struct task * const task = *(struct task**) 0x1a2c;
#endif
// toward a generic *-hack.c ?  not used yet
NSTUB( 0xFF01019C, HIJACK_INSTR_BL_CSTART ) 
NSTUB( 0xFF0110D0, HIJACK_INSTR_BSS_END ) 
NSTUB( 0xFF011038, HIJACK_FIXBR_BZERO32 ) 
NSTUB( 0xFF0110C0, HIJACK_FIXBR_CREATE_ITASK ) 
NSTUB( 0xFF0110DC, HIJACK_INSTR_MY_ITASK ) 
NSTUB( 0x1a2c, HIJACK_TASK_ADDR ) 

/** Look for the normal printf strings */
NSTUB( 0xff06bf78, DebugMsg )

/** Task dispatch hook hasn't moved in several versions, was 0x1938 in 550-109 */
NSTUB( 0x193c, task_dispatch_hook )

/** Find the additional version string in GUI_GetFirmVersion */
NSTUB( 0x250DC, additional_version )
NSTUB( 0xFF214744, GUI_GetFirmVersion )

// Everything below here is not needed for the first boot

/** Camera interface? */
NSTUB( 0xC0220000, camera_engine )

NSTUB( 0xff06ce40, call )

/** Find the shell register commands */
NSTUB( 0xff1dc2fc, register_func )

/** Debugging commands are all registered by name.  Search for the strings */
NSTUB( 0xff06bef8, dmstart )
NSTUB( 0xff06bf38, dmstop )
NSTUB( 0xFF06C918, dumpf )

/** Look in DebugMsg, near the bottom.  It calls vsnprintf()
 * with a buffer size of 0x4C.
 */
NSTUB( 0xff1da8cc, vsnprintf )

NSTUB( 0xff06f0a4, msleep ) // called from EV_GETDONE_CACHE param 0x64
NSTUB( 0xff06f158, task_create )
NSTUB( 0xff1ca2b4, FIO_Open)
NSTUB( 0xff1ca370, FIO_CreateFile )
NSTUB( 0xFF1CA41C, FIO_RemoveFile ) // after FIO_CreateFile, before FIO_ReadFile
NSTUB( 0xff1ca4c4, FIO_ReadFile )
NSTUB( 0xff1ca664, FIO_WriteFile )
NSTUB( 0xff1ca714, FIO_CloseFile )
NSTUB( 0xFF1CA890, FIO_GetFileSize )
	
NSTUB( 0xff06ecb8, give_semaphore )
NSTUB( 0xff06ebd0, take_semaphore )
NSTUB( 0xff06c2d8, dm_set_store_level ) // called by "dmstore"
NSTUB( 0xff05b6f4, prop_register_slave ) // called by"GUI_RegisterPropertySlave"
NSTUB( 0xff05b08c, prop_request_change )
NSTUB( 0xff05b66c, prop_deliver ) 
NSTUB( 0xff05b8ac, prop_get_value )
NSTUB( 0xff05b814, prop_cleanup )
	
NSTUB( 0xFF1CCFFC, dispcheck )
NSTUB( 0xFF05DAF8, SetBitmapVramAddress ) // where to find bmp_vram_info
NSTUB( 0x3D7B0, bmp_vram_info )
NSTUB( 0x55500, vram_info )
NSTUB( 0xFF2CDD58, vram_get_number )   // DebugMsg has been removed.   after "[ImgPlyer]   Rate"
NSTUB( 0xff1cbc58, vram_get_pos_and_size )
 
NSTUB( 0xFF018908, malloc )     //finsig found 0xff018904
NSTUB( 0xFF018960, free )
NSTUB( 0xFF06FFD0, AllocateMemory ) 
 	
NSTUB( 0x2B34, dm_names )                // in DebugMsg, before the 1st loop target
NSTUB( 0xFF1F0888, strcpy )              // finsig was wrong
NSTUB( 0xff068208, LoadCalendarFromRTC )
NSTUB( 0xff07bf48, task_trampoline )

NSTUB( 0xff068860, _audio_ic_write )    // DebugMsg removed
NSTUB( 0xff068714, _audio_ic_read )     // DebugMsg removed
NSTUB( 0xff056e90, sounddev_task )
NSTUB( 0xff0571f8, sounddev_active_in )
NSTUB( 0xff5aee64, audio_thresholds )        // after ptr to "ALVFactorySemSignature"
NSTUB( 0x1F50, sounddev )                    // in sounddev_task

NSTUB( 0xff06ea70, create_named_semaphore )    // called CreateBinarySemaphore in 550d
NSTUB( 0xff30649c, gui_task_create )
NSTUB( 0xff3065bc, gui_task_destroy )
NSTUB( 0xff30634c, ctrlman_dispatch_event )

NSTUB( 0xff538120, memcpy )
NSTUB( 0xFF214D70, prop_request_icu_auto_poweroff )
NSTUB( 0xff1de6f0, alloc_dma_memory )
NSTUB( 0xff1de724, free_dma_memory )

NSTUB( 0xFF4BB41C, cfReadBlk )
NSTUB( 0x35104, cf_device )                // in cfReadBlk
NSTUB( 0xFF4C10EC, sdReadBlk )
NSTUB( 0x35128, sd_device )                // in sdReadBlk

NSTUB( 0xFF01B18C, hotplug_task )
NSTUB( 0x1A7C, hotplug_struct )            // found only by Alex tool!
NSTUB( 0x1AC8, hotplug_usb_buf )          // after "USB Disconnect" 

NSTUB( 0xFF06BBA0, bootdisk_enable )          
NSTUB( 0xFF06BBAC, bootdisk_disable )

NSTUB( 0xFF010500, cli_save )
NSTUB( 0xFF010514, sei_restore )
NSTUB( 0xff2938bc, ptp_register_handler )
NSTUB( 0xFF13F6C4, gui_lock )             // PtpPropSetUILock. found only by Alex tool!
NSTUB( 0xff06d8c4, oneshot_timer )
NSTUB( 0xFF2B4D30, gui_task_destroy )
NSTUB( 0xFF30649C, gui_task_create )
NSTUB( 0xFF024188, gui_main_task )       //  uses a 8 functions table (FF53DA40) like 550d. found only by Alex tool!
NSTUB( 0xff024ba0, gui_init_end )     // EndGuiInit
NSTUB( 0x442C, gui_timer_struct )     // in GUI_Control_Post. found only by Alex tool!
NSTUB( 0x1CF8, gui_main_struct )      //  in gui_main_task. LDR     R0, [R4,#0x30]  ; queue_handle
NSTUB( 0xFF1D6E04, msg_queue_receive )
NSTUB( 0xFF0A8D60, gui_local_post )     // found only by Alex tool!
NSTUB( 0xFF0A8620, gui_change_mode )   // GUI_ChangeMode_Post
NSTUB( 0xFF0A9324, gui_other_post )     // just after gui_local_post. found only by Alex tool

NSTUB( FF53DA40, gui_main_task_functbl )

NSTUB( 0xFF04EFE4, mvrSetBitRate )
NSTUB( 0xFF1AE164, mvrSetQScale )
NSTUB( 0xFF1AE758, mvrFixQScale )
NSTUB( 0xFF1AE234, mvrSetDefQScale )
NSTUB( 0xFF1AE250, mvr_set_time_const ) //mvrSetTimeConst
NSTUB( 0xFF1AE690, mvr_setd_fullhd ) //mvrSetD_HD

#if 0   // not checked yet in 60d 108
 NSTUB( 0xFF0959A4, gui_post_10000085 ) // should be called post 100000a5 no ?
NSTUB( 0xff0a9dd8, gui_init_event )
NSTUB( 0xff0a962c, gui_change_shoot_type_post )
 NSTUB( 0xFF095AC8, gui_change_lcd_state_post )
 NSTUB( 0xFF093108, gui_massive_event_loop )   // GUI_Control_Post
NSTUB( 0xff1d69f4, gui_timer_something )

#endif
