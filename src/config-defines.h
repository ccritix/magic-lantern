/**
 *  Conditional defines for camera internals and ML features.
 */
#ifndef _config_defines_h_
#define _config_defines_h_

/** 
 * Enable these for early ports
 */

    /** If CONFIG_EARLY_PORT is defined, only a few things will be enabled (e.g. changing version string) */
    //~ #define CONFIG_EARLY_PORT

    /** Load fonts and print Hello World (disable CONFIG_EARLY_PORT); will not start any other ML tasks, handlers etc. */
    //~ #define CONFIG_HELLO_WORLD
    
    /** Create a developer FIR for enabling the bootflag and dumping the ROM. */
    //~ #define CONFIG_DUMPER_BOOTFLAG

/**
 * Some common stuff - you can override them in platform files
 */

    /** If something goes wrong (ERR70), we can save a crash log **/
    #define CONFIG_CRASH_LOG

    /** It's a good idea to back up ROM contents on the card - just in case **/
    #define CONFIG_AUTOBACKUP_ROM

    /** You may want to disable this for troubleshooting **/
    #define CONFIG_CONFIG_FILE
    
    /** Show detailed info about tasks and CPU usage */
    #define CONFIG_TSKMON

/**
 * Some debug stuff - you should enable it Makefile.user to avoid pushing unwanted changes to the repo
 */
    /** This may help discovering some cool new stuff - http://magiclantern.wikia.com/wiki/Register_Map/Brute_Force **/
    /** For developers only; can be dangerous **/
    //~ #define CONFIG_DIGIC_POKE

    /** A bunch of debug tools **/
    //~ #define CONFIG_DEBUGMSG 1

    /** Hack to see what memory regions were touched by Canon code and what seems to be unused */
    /** warning: it will slow down boot by a few seconds */
    //~ #define CONFIG_MARK_UNUSED_MEMORY_AT_STARTUP

/** What internals do we have on each camera? **/
#include "internals.h" // from platform directory

/** What features are enabled on each camera? **/
#include "features.h" // from platform directory

#endif // _config_defines_h_
