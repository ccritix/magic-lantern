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

#define BUF_SIZE_STATIC (16*1024)
#define BUF_SIZE_MALLOC (2048*1024)

static char * volatile buf = 0;
static volatile int buf_size = 0;
static volatile int len = 0;

extern void dm_spy_extra_install();
extern void dm_spy_extra_uninstall();

static void my_DebugMsg(int class, int level, char* fmt, ...)
{
    if (!buf) return;
        
    if (class == 21) // engio, lots of messages
        return;
    
    #ifdef CONFIG_7D
    if (class == 0x3E) // IPC, causes ERR70
        return;
    #endif
    
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
    
    len += snprintf( buf+len, MIN(50, buf_size-len), "%s:%08x:%02x:%02x: ", task_name_padded, lr-4, class, level );

    va_start( ap, fmt );
    len += vsnprintf( buf+len, buf_size-len-1, fmt, ap );
    va_end( ap );
    
    #ifdef PRINT_STACK
    {
        int spaces = COERCE(120 - (len - len0), 0, buf_size-len-1);
        memset(buf+len, ' ', spaces);
        len += spaces;
        
        len += snprintf( buf+len, buf_size-len, "stack: " );
        for (int i = 0; i < 200; i++)
        {
            uint32_t addr = MEM(sp+i*4) - 4;

            /* does it look like a pointer to some ARM instruction? */
            if (is_sane_ptr(addr) && (MEM(addr) & 0xF0000000) == 0xE0000000)
            {
                len += snprintf( buf+len, buf_size-len, "%x ", addr);
            }
        }
    }
    #endif

    len += snprintf( buf+len, buf_size-len, "\n" );
    
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

/* use a small buffer until the memory backend gets initialized */
static char staticbuf[BUF_SIZE_STATIC] = {0};

void debug_realloc()
{
    DryosDebugMsg(0, 0, "(*) Reallocating logging buffer...");

    extern void * _AllocateMemory(size_t size);
    extern void * _malloc(size_t size);

    int new_buf_size = BUF_SIZE_MALLOC;
    void * new_buf = _AllocateMemory(new_buf_size);

    DryosDebugMsg(0, 0, "(*) New buffer: %x, size=%d.", new_buf, new_buf_size);

    if (new_buf)
    {
        uint32_t old = cli();

        /* switch to the new buffer */
        void * old_buf = buf;
        int old_buf_size = buf_size;
        buf = new_buf;
        buf_size = new_buf_size;
        
        /* copy old contents */
        memcpy(buf, old_buf, len);

        sei(old);

        DryosDebugMsg(0, 0, "(*) Buffer reallocated (%d -> %d).", old_buf_size, buf_size);

        /* check for overflow */
        if (len >= old_buf_size - 50)
        {
            DryosDebugMsg(0, 0, "(*) Old buffer full, some lines may be lost.");
        }

        /* old buffer remains allocated; any real reason to free it? */
    }
    else
    {
        DryosDebugMsg(0, 0, "(*) malloc error!");
        /* if it fails, you can either try _malloc, reduce buffer size,
         * or look for unused memory areas in RscMgr */
    }
}

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
        buf_size = BUF_SIZE_STATIC;
        
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
        unpatch_memory(DebugMsg_addr);
        
        #ifdef CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK
            blink_init();
            blink_str(staticbuf);
        #else
            char log_filename[100];
            get_numbered_file_name("dm-%04d.log", 9999, log_filename, sizeof(log_filename));
            dump_seg(buf, len, log_filename);
            NotifyBox(2000, "%s: saved %d bytes.", log_filename, len);
        #endif
        buf = 0;
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
        buf = malloc(BUF_SIZE_MALLOC);                  /* allocate memory for our logs (it's huge) */
        buf_size = BUF_SIZE_MALLOC;
        
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
        char log_filename[100];
        get_numbered_file_name("dm-%04d.log", 9999, log_filename, sizeof(log_filename));
        dump_seg(buf, len, log_filename);
        NotifyBox(2000, "%s: saved %d bytes.", log_filename, len);
        len = 0;
        fio_free(buf);
        buf = 0;
    }
    
    beep();
}

#endif
