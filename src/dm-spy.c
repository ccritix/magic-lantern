/** 
 * Attempt to intercept all Canon debug messages by overriding DebugMsg call with cache hacks
 * 
 * Usage:
 * - add dm-spy.o to platform/camera/Makefile.setup.default
 * - call "debug_intercept" from "don't click me", or define CONFIG_DEBUG_INTERCEPT in features.h
 * 
 **/

#include "dm-spy.h"
#include "dryos.h"
#include "bmp.h"
#include "beep.h"
#include "patch.h"

extern WEAK_FUNC(ret_0) void dm_spy_extra_install();
extern WEAK_FUNC(ret_0) void dm_spy_extra_uninstall();

unsigned int BUF_SIZE = (1024*1024);
static char* buf = 0;
static volatile int len = 0;

void my_DebugMsg(int class, int level, char* fmt, ...)
{
    if (!buf) return;
        
    if (class == 21) // engio
        return;
    
    va_list            ap;

    uint32_t old = cli();
    
    //~ char* classname = dm_names[class]; /* not working, some names are gibberish; todo: check for printable characters? */
    
    char* task_name = get_task_name_from_id(get_current_task());
    
    /* Canon's vsnprintf doesn't know %20s */
    char task_name_padded[] = "                ";
    snprintf(task_name_padded + sizeof(task_name_padded) - strlen(task_name), sizeof(task_name_padded), "%s", task_name);
    
    len += snprintf( buf+len, MIN(50, BUF_SIZE-len), "%s:%02x:%02x: ", task_name_padded, class, level );

    va_start( ap, fmt );
    len += vsnprintf( buf+len, BUF_SIZE-len-1, fmt, ap );
    va_end( ap );

    len += snprintf( buf+len, BUF_SIZE-len, "\n" );
    
    sei(old);
    
    //~ static int y = 0;
    //~ bmp_printf(FONT_SMALL, 0, y, "%s\n                                                               ", msg);
    //~ y += font_small.height;
    //~ if (y > 450) y = 0;
}

// call this from "don't click me"
void debug_intercept()
{
    uint32_t DebugMsg_addr = (uint32_t)&DryosDebugMsg;
    
    if (!buf) // first call, intercept debug messages
    {
        buf = fio_malloc(BUF_SIZE);                     /* allocate memory for our logs (it's huge) */
        
        dm_spy_extra_install();
        
        int err = patch_memory(
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
        dump_seg(buf, len, "dm.log");
        NotifyBox(2000, "DebugMsg log: saved %d bytes.", len);
        len = 0;
        fio_free(buf);
        buf = 0;
    }
    
    beep();
}

