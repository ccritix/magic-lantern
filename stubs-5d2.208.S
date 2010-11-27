/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 2.0.4 firmware.
 *
 * \todo Sort this file?  Generate it from the IDA map?
 */
/*
 * Copyright (C) 2010 Trammell Hudson <hudson+ml@osresearch.net>
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
NSTUB( 0xFF810894, cstart )
NSTUB( 0xFFC45858, bzero32 ) // called by cstart()
NSTUB( 0xFF811DBC, init_task ) // passed as arg to create_init_task, look for dmSetup
NSTUB( 0xFF817470, create_init_task ) 

/** Look for the normal printf strings */
NSTUB( 0xFF86AF64, DebugMsg )

/** Task dispatch hook hasn't moved in several versions */
NSTUB( 0x1934, task_dispatch_hook )

/** Find the additioanl version string in GUI_GetFirmVersion */
NSTUB( 0x13424, additional_version )

// Everything below here is not needed for the first boot

/** Camera interface? */
NSTUB( 0xC0220000, camera_engine )


///** Find the shell register commands */
//NSTUB( 0xFF81C4DC, register_func )

/** Debugging commands are all registered by name.  Search for the strings */
NSTUB( 0xFF86AEE4, dmstart )
NSTUB( 0xFF86B7F8, dumpf )
NSTUB( 0xFF86AF24, dmstop )

/** Look in DebugMsg, near the bottom.  It calls vsnprintf()
 * with a buffer size of 0x4C.
 */
NSTUB( 0xFF9B72B0, vsnprintf )

NSTUB( 0xFF86E22C, msleep )
NSTUB( 0xFF86E2E4, task_create )
NSTUB( 0xFF9AAB48, FIO_Open)
NSTUB( 0xFF9AABFC, FIO_CreateFile )
NSTUB( 0xFF9AACA4, FIO_CloseFile )
NSTUB( 0xFF9AB298, FIO_GetFileSize )
NSTUB( 0xFF9AB0E4, FIO_WriteFile )
NSTUB( 0xFF9AAF34, FIO_ReadFile )
NSTUB( 0xFF86DE70, give_semaphore )
NSTUB( 0xFF86DD88, take_semaphore )
NSTUB( 0xFF86BCE0, call )
NSTUB( 0xFF86B2B0, dm_set_store_level )
NSTUB( 0xFF85F0F0, prop_register_slave )
NSTUB( 0xFF85F228, prop_request_change )
NSTUB( 0xFF85F068, prop_deliver )
NSTUB( 0xFF85F1C4, prop_cleanup )
NSTUB( 0xFF86DC28, create_named_semaphore )
NSTUB( 0xFFA6BEAC, gui_task_create )
NSTUB( 0xFF8696D8, LoadCalendarFromRTC )
NSTUB( 0xFF86F16C, malloc )
NSTUB( 0xFF86F4FC, free )
NSTUB( 0xFF9BA11C, alloc_dma_memory )
NSTUB( 0xFF9BA150, free_dma_memory )
NSTUB( 0xFF859638, _audio_ic_write )
NSTUB( 0xFF859468, _audio_ic_read )
NSTUB( 0xFF85B0E4, sounddev_task )
NSTUB( 0x2360, sounddev )
NSTUB( 0xFF87CBEC, strcpy )
NSTUB( 0xFFA0FE64, ptp_register_handler )
NSTUB( 0xFF92EAEC, gui_lock )
NSTUB( 0xFF893CF4, prop_request_icu_auto_poweroff )
NSTUB( 0xFFC45764, memcpy )
NSTUB( 0xFF86C720, oneshot_timer )
NSTUB( 0xFF81DE08, hotplug_task )
NSTUB( 0x1AA4, hotplug_struct )
NSTUB( 0x1AD8, hotplug_usb_buf )
NSTUB( 0xFF864694, dispcheck )
NSTUB( 0x27630, bmp_vram_info )
NSTUB( 0x38920, vram_info )
NSTUB( 0xFFA41590, vram_get_number )
NSTUB( 0xFF8773C0, task_trampoline )
NSTUB( 0x208A8, cf_device )
NSTUB( 0x2BA0, dm_names )
NSTUB( 0xFFA6BFCC, gui_task_destroy )
NSTUB( 0xFF82399C, gui_main_task )
NSTUB( 0xFF82434C, gui_init_end )
NSTUB( 0x38F0, gui_timer_struct )
NSTUB( 0x1C4C, gui_main_struct )
NSTUB( 0xFF86D988, msg_queue_receive )
NSTUB( 0xFF891BBC, gui_local_post )
NSTUB( 0xFF891718, gui_change_mode )
NSTUB( 0xFF891FB4, gui_other_post )
NSTUB( 0xFF89220C, gui_post_10000085 )
NSTUB( 0xFF89284C, gui_init_event )
NSTUB( 0xFF8922C0, gui_change_shoot_type_post )
NSTUB( 0xFF892358, gui_change_lcd_state_post )
NSTUB( 0xFFA6BD5C, ctrlman_dispatch_event )
NSTUB( 0xFF8901A4, gui_massive_event_loop )
NSTUB( 0xFF9B3A5C, gui_timer_something )
NSTUB( 0xFFCB4A64, audio_thresholds )
NSTUB( 0xFF85B454, sounddev_active_in )
