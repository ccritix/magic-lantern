/** \file
 * This is similar to boot-hack, but simplified. It only does the first-time install (bootflag setup).
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
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

#include "dryos.h"
#include "config.h"
#include "version.h"
#include "bmp.h"
#include "menu.h"
#include "version.h"
#include "property.h"
#include "consts.h"


void NotifyBox(int timeout, char* fmt, ...) 
{
    beep();
    static char notify_box_msg[100];
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( notify_box_msg, sizeof(notify_box_msg), fmt, ap );
    va_end( ap );
    bmp_printf(FONT_LARGE, 0, 0, "%s", notify_box_msg);
}

/** These are called when new tasks are created */
int my_init_task(int a, int b, int c, int d);
void my_bzero( uint8_t * base, uint32_t size );

/** This just goes into the bss */
#define RELOCSIZE 0x10000
static uint8_t _reloc[ RELOCSIZE ];
#define RELOCADDR ((uintptr_t) _reloc)

/** Translate a firmware address into a relocated address */
#define INSTR( addr ) ( *(uint32_t*)( (addr) - ROMBASEADDR + RELOCADDR ) )

/** Fix a branch instruction in the relocated firmware image */
#define FIXUP_BRANCH( rom_addr, dest_addr ) \
    INSTR( rom_addr ) = BL_INSTR( &INSTR( rom_addr ), (dest_addr) )

/** Was this an autoboot or firmware file load? */
int autoboot_loaded;

/** Specified by the linker */
extern uint32_t _bss_start[], _bss_end[];

static inline void
zero_bss( void )
{
    uint32_t *bss = _bss_start;
    while( bss < _bss_end )
        *(bss++) = 0;
}

void card_led_on() { cli_save(); *(int*)CARD_LED_ADDRESS |= 2; sei_restore(); }
void card_led_off() { cli_save(); *(int*)CARD_LED_ADDRESS &= ~2; sei_restore(); }
void card_led_blink(int times, int delay_on, int delay_off)
{
    int i;
    for (i = 0; i < times; i++)
    {
        card_led_on();
        Msleep(delay_on);
        card_led_off();
        Msleep(delay_off);
    }
}

void
__attribute__((noreturn,noinline,naked))
copy_and_restart( int offset )
{
    zero_bss();

    // Set the flag if this was an autoboot load
    autoboot_loaded = (offset == 0);

    // Copy the firmware to somewhere safe in memory
    const uint8_t * const firmware_start = (void*) ROMBASEADDR;
    const uint32_t firmware_len = RELOCSIZE;
    uint32_t * const new_image = (void*) RELOCADDR;

    blob_memcpy( new_image, firmware_start, firmware_start + firmware_len );

    /*
     * in entry2() (0xff010134) make this change to
     * return to our code before calling cstart().
     * This should be a "BL cstart" instruction.
     */
    INSTR( HIJACK_INSTR_BL_CSTART ) = RET_INSTR;

    /*
     * in cstart() (0xff010ff4) make these changes:
     * calls bzero(), then loads bs_end and calls
     * create_init_task
     */
    // Reserve memory after the BSS for our application
    INSTR( HIJACK_INSTR_BSS_END ) = (uintptr_t) _bss_end;

    // Fix the calls to bzero32() and create_init_task()
    FIXUP_BRANCH( HIJACK_FIXBR_BZERO32, bzero32 );
    FIXUP_BRANCH( HIJACK_FIXBR_CREATE_ITASK, create_init_task );

    // Set our init task to run instead of the firmware one
    INSTR( HIJACK_INSTR_MY_ITASK ) = (uint32_t) my_init_task;
    
    // Make sure that our self-modifying code clears the cache
    clean_d_cache();
    flush_caches();

    // We enter after the signature, avoiding the
    // relocation jump that is at the head of the data
    thunk reloc_entry = (thunk)( RELOCADDR + 0xC );
    reloc_entry();

    /*
    * We're back!
    * The RAM copy of the firmware startup has:
    * 1. Poked the DMA engine with what ever it does
    * 2. Copied the rw_data segment to 0x1900 through 0x20740
    * 3. Zeroed the BSS from 0x20740 through 0x47550
    * 4. Copied the interrupt handlers to 0x0
    * 5. Copied irq 4 to 0x480.
    * 6. Installed the stack pointers for CPSR mode D2 and D3
    * (we are still in D3, with a %sp of 0x1000)
    * 7. Returned to us.
    *
    * Now is our chance to fix any data segment things, or
    * install our own handlers.
    */

    // This will jump into the RAM version of the firmware,
    // but the last branch instruction at the end of this
    // has been modified to jump into the ROM version
    // instead.
    void (*ram_cstart)(void) = (void*) &INSTR( cstart );
    ram_cstart();

    // Unreachable
    while(1)
        ;
}

int autoexec_ok; // true if autoexec.bin was found
int fonts_ok; // true if fonts.dat was found
int old_shooting_mode; // to detect when mode dial changes
int bootflag_written = 0;
// print a message and redraw it continuously (so it won't be erased by camera firmware)
#define PERSISTENT_PRINTF(times, font, x, y, msg, ...) { int X = times; while(X--) { bmp_printf(font, x, y, msg, ## __VA_ARGS__); Msleep(100); } }

#define UILOCK_EVERYTHING_EXCEPT_POWEROFF_AND_MODEDIAL 0x4100014f
#define UILOCK_EVERYTHING 0x4100017f

void ui_lock(int x)
{
    int unlocked = 0x41000000;
    prop_request_change(PROP_ICU_UILOCK, &unlocked, 4);
    Msleep(200);
    prop_request_change(PROP_ICU_UILOCK, &x, 4);
    Msleep(200);
}

void beep()
{
    call("StartPlayWaveData");
    Msleep(1000);
    call("StopPlayWaveData");
}
void fake_simple_button(int bgmt_code)
{
    GUI_Control(bgmt_code, 0, 0, 0);
}

PROP_INT(PROP_TFT_STATUS, tft_status);

void install_task()
{
    
    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "Install task starting...");
    
    while (sensor_cleaning) Msleep(200);

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "Sensor cleaning done... ");
    
    Msleep(200);
    
    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "TFT status: %d          ", tft_status);
    
    if (tft_status || gui_state != GUISTATE_IDLE) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status || gui_state != GUISTATE_IDLE) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status || gui_state != GUISTATE_IDLE) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status || gui_state != GUISTATE_IDLE) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status || gui_state != GUISTATE_IDLE) { SW1(1,100); SW1(0,100); Msleep(500); }
#if defined(CONFIG_50D) || defined(CONFIG_5D2)
    if (tft_status) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status) { fake_simple_button(BGMT_INFO); Msleep(500); }
    if (tft_status)
#else
    if (tft_status || gui_state != GUISTATE_IDLE)
#endif
    {
        PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "TFT status error...     ");
        card_led_blink(10, 10, 90);
        card_led_blink(10, 90, 10);
        card_led_blink(10, 10, 90);
        card_led_blink(10, 90, 10);
        card_led_blink(10, 10, 90);
        card_led_blink(10, 90, 10);
        beep();
        return; // display off, can't install
    }
    Msleep(500);

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "TFT status OK!          ");
    canon_gui_disable_front_buffer();
    ui_lock(UILOCK_EVERYTHING);

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "UI locked!              ");

    autoexec_ok = check_autoexec();
    fonts_ok = check_fonts();

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "Autoexec & fonts checked");

    initial_install();

    old_shooting_mode = shooting_mode;

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "Initial install DONE!!! ");
    
    ui_lock(UILOCK_EVERYTHING_EXCEPT_POWEROFF_AND_MODEDIAL);

    //~ PERSISTENT_PRINTF(30, FONT_LARGE, 50, 50, "Going interactive.......");
    
    int k = 0;
    for (;;k++)
    {
        while (sensor_cleaning) msleep(100);
        
        check_install();
    
        if (shooting_mode != old_shooting_mode)
        {
            bootflag_toggle();
        }
        old_shooting_mode = shooting_mode;
        
        Msleep(100);
    }
}

void redraw() { clrscr(); }

static volatile int init_funcs_done;

static void
call_init_funcs( void * priv )
{
    // Call all of the init functions
    extern struct task_create _init_funcs_start[];
    extern struct task_create _init_funcs_end[];
    struct task_create * init_func = _init_funcs_start;

    for( ; init_func < _init_funcs_end ; init_func++ )
    {
        DebugMsg( DM_MAGIC, 3,
            "Calling init_func %s (%x)",
            init_func->name,
            (unsigned) init_func->entry
        );
        thunk entry = (thunk) init_func->entry;
        entry();
    }

}

// this Msleep works with display off too
void Msleep(int ms)
{
    int i;
    for (i = 0; i < ms/100; i++)
    {
        msleep(100);
        call("DisablePowerSave"); // trick from AJ_MREQ_ISR
        call("EnablePowerSave"); // to prevent camera for entering "deep sleep"
    }
}

/** Initial task setup.
 *
 * This is called instead of the task at 0xFF811DBC.
 * It does all of the stuff to bring up the debug manager,
 * the terminal drivers, stdio, stdlib and armlib.
 */
int my_init_task(int a, int b, int c, int d)
{
    // Call their init task
    int ans = init_task(a,b,c,d);

    // Overwrite the PTPCOM message
    dm_names[ DM_MAGIC ] = "[MAGIC] ";
    //~ dmstart(); // already called by firmware?

    DebugMsg( DM_MAGIC, 3, "Magic Lantern %s (%s)",
        build_version,
        build_id
    );

    DebugMsg( DM_MAGIC, 3, "Built on %s by %s",
        build_date,
        build_user
    );

    // Re-write the version string.
    // Don't use strcpy() so that this can be done
    // before strcpy() or memcpy() are located.
    extern char additional_version[];
    additional_version[0] = '-';
    additional_version[1] = 'm';
    additional_version[2] = 'l';
    additional_version[3] = '\0';

    Msleep( 2000 );
    
    call_init_funcs(0);
    Msleep(1000);

    task_create("install_task", 0x1b, 0, install_task, 0);
    return ans;
}

/** Shadow copy of the NVRAM boot flags stored at 0xF8000000 */
#define NVRAM_BOOTFLAGS     ((void*) 0xF8000000)
struct boot_flags
{
    uint32_t        firmware;   // 0x00
    uint32_t        bootdisk;   // 0x04
    uint32_t        ram_exe;    // 0x08
    uint32_t        update;     // 0x0c
    uint32_t        flag_0x10;
    uint32_t        flag_0x14;
    uint32_t        flag_0x18;
    uint32_t        flag_0x1c;
};

static struct boot_flags * const    boot_flags = NVRAM_BOOTFLAGS;;

void SW1(int v, int wait)
{
    prop_request_change(PROP_REMOTE_SW1, &v, 2);
    Msleep(wait);
}

void SW2(int v, int wait)
{
    prop_request_change(PROP_REMOTE_SW2, &v, 2);
    Msleep(wait);
}

/** Perform an initial install and configuration */
void
initial_install(void)
{
    bmp_fill(0, 0, 0, 720, 480);

    int y = 0;
    bmp_printf(FONT_LARGE, 0, y+=30, "Magic Lantern install");

    FILE * f = FIO_CreateFile(CARD_DRIVE "ROM0.BIN");
    if (f != (void*) -1)
    {
        bmp_printf(FONT_LARGE, 0, 60, "Writing ROM");
        FIO_WriteFile(f, (void*) 0xFF010000, 0x900000);
        FIO_CloseFile(f);
    }

    if (!autoexec_ok)
    {
        bmp_printf(FONT_LARGE, 0, y+=30, "AUTOEXEC.BIN not found!");
        msleep(5000);
        return;
    }

    if (!fonts_ok)
    {
        bmp_printf(FONT_LARGE, 0, y+=30, "FONTS.DAT not found!");
        msleep(5000);
        return;
    }

    if (!boot_flags->bootdisk )
    {
        if (!lv && !sensor_cleaning && shooting_mode != SHOOTMODE_MOVIE)
        {
            bmp_printf(FONT_LARGE, 0, y+=30, "Setting boot flag");
            call( "EnableBootDisk" );
        }
        else
        {
            bmp_printf(FONT_LARGE, 0, y+=30, "Did not enable boot flag.");
            msleep(5000);
            return;
        }
    }

    bmp_printf(FONT_LARGE, 0, y+=30, "Making card bootable...");
    bootflag_written = bootflag_write_bootblock();
    if (!bootflag_written)
    {
        bmp_printf(FONT_LARGE, 0, y, "You need to run EOSCard/MacBoot.");
        msleep(5000);
        
    }
    
    //~ bmp_printf(FONT_LARGE, 0, y+=30, "Writing boot log");
    //~ dumpf();

    bmp_printf(FONT_LARGE, 0, y+=30, "Done!");
    Msleep(1000);
}

void bootflag_toggle( void * priv )
{
    if( boot_flags->firmware )
    {
        firmware_fix();
        return;
    }

    clrscr();
    ui_lock(UILOCK_EVERYTHING);
    clrscr();
    int i;
    if( boot_flags->bootdisk )
    {
        for (i = 0; i < 10; i++)
        {
            bmp_printf(FONT_LARGE, 50, 100, "DisableBootDisk");
            card_led_blink(1, 50, 50);
        }
        if (!lv && !sensor_cleaning && shooting_mode != SHOOTMODE_MOVIE)
            call( "DisableBootDisk" ); // in movie mode, it causes ERR80 and then asks for a firmware update
    }
    else
    {
        for (i = 0; i < 10; i++)
        {
            bmp_printf(FONT_LARGE, 50, 100, "EnableBootDisk");
            card_led_blink(1, 50, 50);
        }
        if (!lv && !sensor_cleaning && shooting_mode != SHOOTMODE_MOVIE)
            call( "EnableBootDisk" );
    }
    card_led_blink(5, 100, 100);
    ui_lock(UILOCK_EVERYTHING_EXCEPT_POWEROFF_AND_MODEDIAL);
}

void firmware_fix( void * priv )
{
    clrscr();
    ui_lock(UILOCK_EVERYTHING);
    clrscr();
    int i;
    if( boot_flags->firmware )
    {
        for (i = 0; i < 10; i++)
        {
            bmp_printf(FONT_LARGE, 50, 100, "EnableMainFirm");
            card_led_blink(1, 50, 50);
        }
        if (!lv && !sensor_cleaning && shooting_mode != SHOOTMODE_MOVIE)
            call( "EnableMainFirm" ); // in movie mode, it causes ERR80 and then asks for a firmware update
    }
    card_led_blink(5, 100, 100);
    ui_lock(UILOCK_EVERYTHING_EXCEPT_POWEROFF_AND_MODEDIAL);
}

// check if autoexec.bin is present on the card
int check_autoexec()
{
    FILE * f = FIO_Open(CARD_DRIVE "AUTOEXEC.BIN", 0);
    if (f != (void*) -1)
    {
        FIO_CloseFile(f);
        return 1;
    }
    return 0;
}

// check if fonts.dat is present on the card
int check_fonts()
{
    FILE * f = FIO_Open(CARD_DRIVE "FONTS.DAT", 0);
    if (f != (void*) -1)
    {
        FIO_CloseFile(f);
        return 1;
    }
    return 0;
}

// check ML installation and print a message
void check_install()
{
    //~ Msleep(1);
    
    if (boot_flags->firmware)
    {
        bmp_printf(FONT(FONT_LARGE, COLOR_RED, 0), 0, 0, 
            " MAIN_FIRMWARE flag is DISABLED!    \n"
            "                                    \n"
            " This was probably caused by a      \n"
            " failed install process.            \n"
            "                                    \n"
            " Camera may work with Magic Lantern,\n"
            " but it will ask for a firmware     \n"
            " update as soon as you insert a     \n"
            " formatted card.                    \n"
            "                                    \n"
            " To fix this (at your own risk!):   \n"
            " a) Turn the mode dial (P/Tv/Av/M)  \n"
            "    (ML will try to fix this)       \n"
            " b) Reinstall Canon firmware.       \n"
            "                                    \n"
        );
    }
    else if( boot_flags->bootdisk )
    {
        if (autoexec_ok)
        {
            if (fonts_ok)
            {
                bmp_printf(FONT(FONT_LARGE, COLOR_GREEN1, 0), 0, 0, 
                    " ********************************** \n"
                    " *            SUCCESS!            * \n"
                    " ********************************** \n"
                    "                                    \n"
                    " BOOTDISK flag is ENABLED.          \n"
                    " AUTOEXEC.BIN found.                \n"
                    "                                    \n"
                    " Magic Lantern is installed.        \n"
                    " You may now restart your camera.   \n"
                    "%s"
                    "                                    \n"
                    " To disable the BOOTDISK flag,      \n"
                    " change the shooting mode from the  \n"
                    " mode dial (P/Tv/Av/M).             \n"
                    "                                    \n",
                    bootflag_written ?
                    "                                    \n" : 
                    " DON'T FORGET to make card bootable!\n"
                );
            }
            else
            {
                bmp_printf(FONT(FONT_LARGE, COLOR_YELLOW, 0), 0, 0, 
                    "                                    \n"
                    " BOOTDISK flag is ENABLED.          \n"
                    " AUTOEXEC.BIN found.                \n"
                    "                                    \n"
                    " !!! FONTS.DAT NOT FOUND !!!        \n"
                    "                                    \n"
                    " Please copy ALL ML files on your   \n"
                    " SD card. They only take a few MB.  \n"
                    "                                    \n"
                    " You may now turn off your camera.  \n"
                    "                                    \n"
                    " To disable the BOOTDISK flag,      \n"
                    " change the shooting mode from the  \n"
                    " mode dial (P/Tv/Av/M).             \n"
                    "                                    \n"
                );
            }
        }
        else
        {
            bmp_printf(FONT(FONT_LARGE, COLOR_RED, 0), 0, 0, 
                "                                    \n"
                " BOOTDISK flag is ENABLED.          \n"
                "                                    \n"
                " !!! AUTOEXEC.BIN NOT FOUND !!!     \n"
                "                                    \n"
                " Remove battery and card            \n"
                " and copy AUTOEXEC.BIN,             \n"
                " otherwise camera will not boot!    \n"
                "                                    \n"
                "                                    \n"
                "                                    \n"
                " To disable the BOOTDISK flag,      \n"
                " change the shooting mode from the  \n"
                " mode dial (P/Tv/Av/M).             \n"
                "                                    \n"
            );
        }
    }
    else
    {
            bmp_printf(FONT(FONT_LARGE, COLOR_WHITE, 0), 0, 0, 
                "                                    \n"
                " BOOTDISK flag is DISABLED.         \n"
                "                                    \n"
                " Magic Lantern is NOT installed.    \n"
                " You may now restart your camera.   \n"
                "                                    \n"
                " After reboot, clear all the        \n"
                " settings and custom functions      \n"
                " to complete the uninstallation.    \n"
                "                                    \n"
                "                                    \n"
                " To enable the BOOTDISK flag,       \n"
                " change the shooting mode from the  \n"
                " mode dial (P/Tv/Av/M).             \n"
                "                                    \n"
            );
    }
    bmp_printf( FONT_SMALL,
        620,
        430,
        "Firmware  %d\n"
        "Bootdisk  %d\n"
        "RAM_EXE   %d\n"
        "Update    %d\n",
        boot_flags->firmware,
        boot_flags->bootdisk,
        boot_flags->ram_exe,
        boot_flags->update
    );
}

// dummy definitions for bmp.c
int ext_monitor_hdmi = 0;
int ext_monitor_rca = 0;
int hdmi_code = 0;
int lv = 0;
void bvram_mirror_init(){};
void bmp_mute_flag_reset(){};
struct font font_small_shadow;
struct font font_med_shadow;
struct font font_large_shadow;

void ml_assert_handler(char* msg, char* file, int line, const char* func) {}
void CheckBmpAcquireRecursiveLock(){};
void CheckBmpReleaseRecursiveLock(){};

