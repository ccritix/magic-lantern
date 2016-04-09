/** 
 * Attempt to intercept all Canon debug messages by overriding DebugMsg call with cache hacks
 * 
 * Usage: in Makefile.user, define one of those:
 * - CONFIG_DEBUG_INTERCEPT=y                     : from ML menu: Debug -> DebugMsg Log
 * - CONFIG_DEBUG_INTERCEPT_STARTUP=y             : intercept startup messages and save a log file after boot is complete
 * - CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK=y       : intercept startup messages and blink the log file via card LED after boot is complete
 * 
 * (note: CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK will also enable CONFIG_DEBUG_INTERCEPT_STARTUP)
 * 
 * 
 **/

/** Some local options */
//~ #define PRINT_EACH_MESSAGE  /* also print each message as soon as it's received (on the screen or via card LED) */
//~ #define PRINT_STACK /* also print the stack contents for each message */

#include "dm-spy.h"
#include "dryos.h"
#include "bmp.h"
#include "beep.h"
#include "patch.h"
#include "blink.h"
#include "asm.h"
#include "qemu-util.h"

#ifdef CONFIG_DEBUG_INTERCEPT_STARTUP /* for researching the startup process */
    /* we don't have malloc from the beginning, so we'll use a static buffer, which should be small */
    #define BUF_SIZE (64*1024)
#else
    /* we can use a larger buffer, because we have the memory backend up and running */
    #define BUF_SIZE (1024*1024)
#endif


extern void dm_spy_extra_install();
extern void dm_spy_extra_uninstall();

static char* buf = 0;
static volatile int len = 0;

static void my_DebugMsg(int class, int level, char* fmt, ...)
{
    if (!buf) return;
        
    if (class == 21) // engio, lots of messages
        return;
    
    va_list            ap;

    uint32_t old = cli();
    
    char* msg = buf+len;
    
    #ifdef PRINT_STACK
    int len0 = len;
    uintptr_t sp = read_sp();
    #endif
    
    uintptr_t lr = read_lr();
    
    //~ char* classname = dm_names[class]; /* not working, some names are gibberish; todo: check for printable characters? */
    
    char* task_name = get_current_task_name();
    
    /* Canon's vsnprintf doesn't know %20s */
    char task_name_padded[11] = "           ";
    int spaces = 10 - strlen(task_name);
    if (spaces < 0) spaces = 0;
    snprintf(task_name_padded + spaces, 11 - spaces, "%s", task_name);
    
    len += snprintf( buf+len, MIN(50, BUF_SIZE-len), "%s:%08x:%02x:%02x: ", task_name_padded, lr-4, class, level );

    va_start( ap, fmt );
    len += vsnprintf( buf+len, BUF_SIZE-len-1, fmt, ap );
    va_end( ap );
    
    #ifdef PRINT_STACK
    {
        int spaces = COERCE(120 - (len - len0), 0, BUF_SIZE-len-1);
        memset(buf+len, ' ', spaces);
        len += spaces;
        
        len += snprintf( buf+len, BUF_SIZE-len, "stack: " );
        for (int i = 0; i < 200; i++)
        {
            uint32_t addr = MEM(sp+i*4) - 4;

            /* does it look like a pointer to some ARM instruction? */
            if (is_sane_ptr(addr) && (MEM(addr) & 0xF0000000) == 0xE0000000)
            {
                len += snprintf( buf+len, BUF_SIZE-len, "%x ", addr);
            }
        }
    }
    #endif

    len += snprintf( buf+len, BUF_SIZE-len, "\n" );
    
    #ifdef PRINT_EACH_MESSAGE
        #ifdef CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK
            blink_str(msg);
        #else
            extern int ml_started;
            if (ml_started)
            {
                static int y = 0;
                bmp_printf(FONT_SMALL, 0, y, "%s\n                                                               ", msg);
                y += font_small.height;
                if (y > 450) y = 0;
            }
        #endif
    #endif

    #ifdef CONFIG_QEMU
    /* in QEMU, just print things at the console, without logging,
     * so we aren't limited by buffer size */
    qprint(msg);
    len = 0;
    #endif

    sei(old);
}

#ifdef CONFIG_DEBUG_INTERCEPT_STARTUP /* for researching the startup process */

static char staticbuf[BUF_SIZE];

// call this from boot-hack.c
void debug_intercept()
{
    uint32_t DebugMsg_addr = (uint32_t)&DryosDebugMsg;
    
    if (!buf) // first call, intercept debug messages
    {
        #if defined(PRINT_EACH_MESSAGE) && defined(CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK)
        blink_init();
        #endif
        
        buf = staticbuf;
        dm_spy_extra_install();
        patch_instruction(
            DebugMsg_addr,                              /* hook on the first instruction in DebugMsg */
            MEM(DebugMsg_addr),                         /* do not do any checks; on 5D2 it would be e92d000f, not sure if portable */
            B_INSTR(DebugMsg_addr, my_DebugMsg),        /* replace all calls to DebugMsg with our own function (no need to call the original) */
            "dm-spy: log all DebugMsg calls"
        );
    }
    else // subsequent call, uninstall the hook and save log to file
    {
        dm_spy_extra_uninstall();
        buf = 0;
        unpatch_memory(DebugMsg_addr);
        staticbuf[len] = 0;
        
        #ifdef CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK
            blink_init();
            blink_str(staticbuf);
        #else
            dump_seg(staticbuf, len, "dm.log");
            NotifyBox(2000, "DebugMsg log: saved %d bytes.", len);
        #endif
        len = 0;
    }
}

#else /* for regular use */

// call this from "don't click me"
void debug_intercept()
{
    uint32_t DebugMsg_addr = (uint32_t)&DryosDebugMsg;
    
    if (!buf) // first call, intercept debug messages
    {
        buf = malloc(BUF_SIZE);                         /* allocate memory for our logs (it's huge) */
        
        dm_spy_extra_install();
        
        int err = patch_instruction(
            DebugMsg_addr,                              /* hook on the first instruction in DebugMsg */
            MEM(DebugMsg_addr),                         /* do not do any checks; on 5D2 it would be e92d000f, not sure if portable */
            B_INSTR(DebugMsg_addr, my_DebugMsg),        /* replace all calls to DebugMsg with our own function (no need to call the original) */
            "dm-spy: log all DebugMsg calls"
        );
        
        if (err)
        {
            NotifyBox(2000, "Could not hack DebugMsg (%x)", err);
        }
        else
        {
            NotifyBox(2000, "Now logging... ALL DebugMsg's :)");
        }
    }
    else // subsequent call, uninstall the hook and save log to file
    {
        dm_spy_extra_uninstall();
        unpatch_memory(DebugMsg_addr);
        buf[len] = 0;
        clean_d_cache();
        dump_seg(buf, len, "dm.log");
        NotifyBox(2000, "DebugMsg log: saved %d bytes.", len);
        len = 0;
        fio_free(buf);
        buf = 0;
    }
    
    beep();
}

#endif
