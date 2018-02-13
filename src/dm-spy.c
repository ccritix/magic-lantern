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
#include "io_trace.h"
#include "dryos.h"
#include "bmp.h"
#include "beep.h"
#include "patch.h"
#include "blink.h"
#include "asm.h"
#include "qemu-util.h"
#include "propvalues.h"

#define BUF_SIZE_STATIC (16*1024)

/* the buffer is reallocated early in the boot process
 * we can't tell in advance how much free space will be eventually
 */
#define BUF_SIZE_MALLOC (512*1024)

/* very little memory available */
#if defined(CONFIG_1100D)
#define BUF_SIZE_MALLOC (128*1024)
#endif

/* plenty of memory available */
#if defined(CONFIG_5D2) || defined(CONFIG_5D3) || defined(CONFIG_50D) || defined(CONFIG_500D) || defined(CONFIG_100D) || defined(CONFIG_70D)
#define BUF_SIZE_MALLOC (2048*1024)
#endif

static char * volatile buf = 0;
static volatile int buf_size = 0;
static volatile int len = 0;

extern void dm_spy_extra_install();
extern void dm_spy_extra_uninstall();

/* log simple strings without formatting
 * useful when we don't have snprintf / enough stack / whatever) */
void debug_logstr(const char * str)
{
    if (!buf) return;
    if (!str) return;

    uint32_t old = cli();

    int copied_chars = MIN(strlen(str), buf_size - len - 1);
    copied_chars = (copied_chars > 0) ? copied_chars : 0;
    memcpy(buf + len, str, copied_chars);
    len += copied_chars;
    buf[len] = '\0';

    qprint(str);
    sei(old);
}

/* log 32-bit integers (print as hex) */
void debug_loghex2(uint32_t x, int digits)
{
    uint32_t old = cli();
    char * str = &buf[len];

    for (int i = (digits-1) * 4; i >= 0 && len < buf_size-1; i -= 4)
    {
        uint32_t c = (x >> i) & 0xF;
        buf[len++] = c + ((c <= 9) ? '0' : 'A' - 10);
    }
    buf[len] = '\0';

    qprint(str);
    sei(old);
}

void debug_loghex(uint32_t x)
{
    return debug_loghex2(x, 8);
}

static void my_DebugMsg(int class, int level, char* fmt, ...)
{
    #ifdef PRINT_STACK
    uintptr_t sp = read_sp();
    int len0 = len;
    #endif
    
    uintptr_t lr = read_lr();

    if (!buf) return;
        
    if (class == 21) // engio, lots of messages
        return;
    
    #ifdef CONFIG_7D
    if (class == 0x3E) // IPC, causes ERR70
        return;
    #endif

    /* only log messages from LiveView tasks until the buffer is half-full
     * (these are very verbose) */
    if (lv && len > buf_size / 2)
    {
        if (streq(current_task->name, "Evf") ||
            streq(current_task->name, "Epp") ||
            streq(current_task->name, "AeWb") ||
            streq(current_task->name, "LVFACE") ||
            streq(current_task->name, "LVC_FACE") ||
            streq(current_task->name, "CLR_CALC") ||
            streq(current_task->name, "DisplayMgr") ||
            streq(current_task->name, "LiveViewMgr") ||
            0)
        {
            return;
        }
    }

    va_list            ap;

    uint32_t old = cli();

    #ifdef CONFIG_MMIO_TRACE
    io_trace_log_flush();
    #endif

    char* msg = buf+len;

    //~ char* classname = dm_names[class]; /* not working, some names are gibberish; todo: check for printable characters? */
    
    /* can be replaced with get_us_clock_value, with a slightly higher overhead */
    uint32_t us_timer = MEM(0xC0242014);

    char* task_name = get_current_task_name();
    
    /* Canon's vsnprintf doesn't know %20s */
    char task_name_padded[11] = "           ";
    int spaces = 10 - strlen(task_name);
    if (spaces < 0) spaces = 0;
    snprintf(task_name_padded + spaces, 11 - spaces, "%s", task_name);

    len += snprintf( buf+len, buf_size-len, "%05X> %s:%08x:%02x:%02x: ", us_timer, task_name_padded, lr-4, class, level );

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

int debug_intercept_running()
{
    return (buf != 0);
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
        #ifdef CONFIG_MMIO_TRACE
        io_trace_install();
        #endif

        patch_instruction(
            DebugMsg_addr,                              /* hook on the first instruction in DebugMsg */
            MEM(DebugMsg_addr),                         /* do not do any checks; on 5D2 it would be e92d000f, not sure if portable */
            B_INSTR(DebugMsg_addr, my_DebugMsg),        /* replace all calls to DebugMsg with our own function (no need to call the original) */
            "dm-spy: log all DebugMsg calls"
        );
    }
    else // subsequent call, uninstall the hook and save log to file
    {
        #ifdef CONFIG_MMIO_TRACE
        io_trace_uninstall();
        #endif
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
        #ifdef CONFIG_MMIO_TRACE
        io_trace_install();
        #endif

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
        #ifdef CONFIG_MMIO_TRACE
        io_trace_uninstall();
        #endif
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
