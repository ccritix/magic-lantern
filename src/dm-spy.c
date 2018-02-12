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
#include "console.h"

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

/* for consistency with interrupt_name from dm-spy-extra.c */
static const char * task_interrupt_name(int i)
{
    static char name[] = "**INT-00h**";
    int i0 = (i & 0xF);
    int i1 = (i >> 4) & 0xF;
    int i2 = (i >> 8) & 0xF;
    name[5] = i2 ? '0' + i2 : '-';
    name[6] = i1 < 10 ? '0' + i1 : 'A' + i1 - 10;
    name[7] = i0 < 10 ? '0' + i0 : 'A' + i0 - 10;
    return name;
}


int debug_format_msg(struct debug_msg * dm, char * msg, int size)
{
    if (!dm->task_name && !dm->pc)
    {
        /* assume we only have a timestamped message with no context info */
        return snprintf(msg, size,
            "%05X> %s\n",
            dm->us_timer,
            dm->msg
        );
    }

    /* message coming from a regular DryOS task, or from an interrupt? */
    const char * task_name = dm->task_name;
    if (dm->interrupt & 1)
    {
        task_name = task_interrupt_name(dm->interrupt >> 2);
    }

    /* Canon's vsnprintf doesn't know %20s */
    char task_name_padded[11] = "           ";
    int spaces = 10 - strlen(task_name);
    if (spaces < 0) spaces = 0;
    snprintf(task_name_padded + spaces, 11 - spaces, "%s", task_name);

    char class_str[7] = "00:00:";

    if (dm->class_name)
    {
        int class_len = strlen(dm->class_name);
        snprintf(class_str, sizeof(class_str), "%s", dm->class_name);
        class_str[6] = '\0';
        class_str[5] = ':';
        for (int i = sizeof(class_str)-3; i >= class_len; i--)
        {
            class_str[i] = ' ';
        }
    }

    /* format the message */
    return snprintf(msg, size,
        "%05X> %s:%08x:%s %s\n",
        dm->us_timer, task_name_padded,
        dm->pc, class_str, dm->msg
    );
}

static void * buf = 0;      /* will hold struct debug_msg's, but variable-sized (with inline strings) */
static int buf_size = 0;    /* variable, model-specific */
static int len = 0;         /* 32-bit aligned */

static struct debug_msg * last_block = 0;

extern void dm_spy_extra_install();
extern void dm_spy_extra_uninstall();

struct debug_msg * debug_get_last_block()
{
    return last_block;
}

void debug_log_line(const char * str)
{
    if (!buf) return;
    if (!len) return;

    int old = cli();
    int str_len = strlen(str);
    int block_size = sizeof(struct debug_msg) + ALIGN32SUP(str_len + 1);
    if (len + block_size > buf_size) return;

    *(last_block = buf + len) = (struct debug_msg) {
        .block_size     = block_size,
        .msg            = buf + len + sizeof(struct debug_msg),
        #ifdef CONFIG_MMIO_TRACE
        .mmio_index     = io_trace_log_get_index() + 1,
        .us_timer       = io_trace_get_timer(),
        #else
        .us_timer       = MEM(0xC0242014),      /* to be unwrapped later */
        #endif
    };

    memcpy(buf + len + sizeof(struct debug_msg), str, str_len + 1);
    len += block_size;
    sei(old);
}

static char line_buf[256] = {0};
static int  line_len = 0;

/* log simple strings without formatting
 * useful when we don't have snprintf / enough stack / whatever)
 * to be called with interrupts cleared (not checked) */
void debug_logstr(const char * str)
{
#if 0
    int old = cli();
    if ((old & 0xC0) != 0xC0)
    {
        qprintf("fixme: cli at %s\n", str);
        while(1);
    }
    sei(old);
#endif

    int copied_chars = MIN(strlen(str), sizeof(line_buf) - line_len - 1);
    copied_chars = (copied_chars > 0) ? copied_chars : 0;
    memcpy(line_buf + line_len, str, copied_chars);
    line_len += copied_chars;
    ASSERT(line_len < (int)sizeof(line_buf));
    line_buf[line_len] = '\0';

    if (line_len > 0 && line_buf[line_len-1] == '\n')
    {
        qprint(line_buf);
        line_buf[line_len-1] = 0;
        debug_log_line(line_buf);
        line_len = 0;
    }
}

/* log 32-bit integers (print as hex)
 * to be called with interrupts cleared (not checked) */
void debug_loghex2(uint32_t x, int digits)
{
    int max_len = sizeof(line_buf) - 1;
    for (int i = (digits-1) * 4; i >= 0 && line_len < max_len; i -= 4)
    {
        uint32_t c = (x >> i) & 0xF;
        line_buf[line_len++] = c + ((c <= 9) ? '0' : 'A' - 10);
    }
    ASSERT(line_len < (int)sizeof(line_buf));
    line_buf[line_len] = '\0';
}

/* log 32-bit integers (print as hex with 8 digits)
 * to be called with interrupts cleared (not checked) */
void debug_loghex(uint32_t x)
{
    return debug_loghex2(x, 8);
}

static void my_DebugMsg(int class, int level, char* fmt, ...)
{
    uintptr_t lr = read_lr();

    if (!buf) return;
    if (len + (int) sizeof(struct debug_msg) >= buf_size - 1) return;

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

    uint32_t old = cli();

    char * msg = fmt;
    uint32_t block_size = sizeof(struct debug_msg);

    if (strchr(fmt, '%'))
    {
        /* formatted message? */
        /* todo: format later if CPU usage becomes an issue? */
        char * dm = buf + len + block_size;
        int dm_size = buf_size - len - block_size - 1;
        ASSERT(dm_size > 0);
        va_list ap;
        va_start( ap, fmt );
        block_size += vsnprintf( dm, dm_size, fmt, ap ) + 1;
        va_end( ap );
        msg = dm;
    }
    else if (((uint32_t) fmt & 0xF0000000) != 0xF0000000)
    {
        /* unformatted string in RAM? copy it */
        char * dm = buf + len + block_size;
        int dm_size = buf_size - len - block_size - 1;
        int copied = MIN(dm_size, strlen(fmt));
        memcpy(dm, fmt, copied);
        dm[copied] = '\0';
        block_size += copied + 1;
        msg = dm;
    }
    else
    {
        /* unformatted string in ROM? just link to it */
    }

    block_size = ALIGN32SUP(block_size);

    *(last_block = buf + len) = (struct debug_msg) {
        .block_size     = block_size,
        .msg            = msg,
        .class          = class,
        .level          = level,
        .pc             = lr - 4,               /* where we got called from */
        .task_name      = current_task->name,
        .interrupt      = current_interrupt | MEM((uintptr_t)&current_task + 4),
        #ifdef CONFIG_MMIO_TRACE
        .mmio_index     = io_trace_log_get_index() + 1,
        .us_timer       = io_trace_get_timer(),
        #else
        .us_timer       = MEM(0xC0242014),      /* to be unwrapped later */
        #endif
      //.edmac_index    = ... (TODO, edmac.mo) */
    };

    len += block_size;

    sei(old);
}

int debug_intercept_running()
{
    return (buf != 0);
}

#ifdef CONFIG_DEBUG_INTERCEPT_STARTUP
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
#endif /* CONFIG_DEBUG_INTERCEPT_STARTUP */

void debug_intercept()
{
    uint32_t DebugMsg_addr = (uint32_t)&DryosDebugMsg;
    
    if (!buf) // first call, intercept debug messages
    {
        #if defined(PRINT_EACH_MESSAGE) && defined(CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK)
        blink_init();
        #endif
        
        #ifdef CONFIG_DEBUG_INTERCEPT_STARTUP
        buf = staticbuf;                                /* malloc not working yet, use a static buffer instead */
        buf_size = BUF_SIZE_STATIC;
        #else
        buf = malloc(BUF_SIZE_MALLOC);                  /* allocate memory for our logs (it's huge) */
        buf_size = BUF_SIZE_MALLOC;
        #endif

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

        #ifndef CONFIG_DEBUG_INTERCEPT_STARTUP
        if (err) {
            NotifyBox(2000, "Could not hack DebugMsg (%x)", err);
        } else {
            NotifyBox(2000, "Now logging... ALL DebugMsg's :)");
        }
        #endif
    }
    else // subsequent call, uninstall the hook and save log to file
    {
        ASSERT(len <= buf_size);

        printf("[dm-spy] stop logging...\n");

        /* stop the loggers */
        #ifdef CONFIG_MMIO_TRACE
        io_trace_uninstall();
        int mmio_index = io_trace_log_get_index();
        printf("[MMIO] captured %d events (%s)\n", mmio_index, format_memory_size(mmio_index * 8 * 4));
        #endif
        dm_spy_extra_uninstall();
        unpatch_memory(DebugMsg_addr);

        printf("[dm-spy] captured %s of messages\n", format_memory_size(len));
        NotifyBox(10000, "Pretty-printing... (%s)", format_memory_size(len));
        msleep(200);

        /* assume we've got large malloc by now */
        /* we don't know in advance how big the formatted buffer will be
         * allocate the largest contiguous block from shoot_malloc - should be large enough */
        struct memSuite * msg_suite = shoot_malloc_suite_contig(0);
        if (!msg_suite)
        {
            NotifyBox(2000, "malloc error");
            return;
        }

        char * msg_buf = GetMemoryAddressOfMemoryChunk(GetFirstChunkFromSuite(msg_suite));
        int msg_max_size = GetSizeOfMemoryChunk(GetFirstChunkFromSuite(msg_suite));
        printf("[dm-spy] output buffer: %s\n", format_memory_size(msg_max_size));
        int msg_len = 0;

        #ifdef CONFIG_MMIO_TRACE
        uint32_t mmio_idx = 0;
        #endif

        static int prev_s = 0; int s = 0;

        /* format all our messages */
        for (struct debug_msg * dm = buf;
            (void *) dm < buf + len;
            dm = (void *) dm + dm->block_size)
        {
            if ((s = get_seconds_clock()) != prev_s)
            {
                /* progress indicator - this may take a few seconds */
                int remain = len - ((void *) dm - buf);
                printf("[dm-spy] pretty-printing: %s left\n", format_memory_size(remain));
                NotifyBox(10000, "Pretty-printing... (%s left)", format_memory_size(remain));
                msleep(50);
                prev_s = s;
            }

            #ifdef CONFIG_MMIO_TRACE
            /* any pending MMIO messages? */
            if (dm->mmio_index)
            {
                for ( ; mmio_idx < dm->mmio_index - 1; mmio_idx++)
                {
                    msg_len += io_trace_log_message(mmio_idx, msg_buf + msg_len, msg_max_size - msg_len);
                }
            }
            #endif

            /* any pending EDMAC messages? */
            if (dm->edmac_index)
            {
                /* TODO */
            }

            /* format our message */
            msg_len += debug_format_msg(dm, msg_buf + msg_len, msg_max_size - msg_len);

            if (msg_len >= msg_max_size - 1000)
            {
                printf("[dm-spy] output full after %s\n", format_memory_size((void*) dm - buf));
                console_show();
                break;
            }
        }

        printf("[dm-spy] pretty-printing complete (%s)\n", format_memory_size(msg_len));
        #ifdef CONFIG_MMIO_TRACE
        printf("[MMIO] saved %d events (%s) out of %d.\n", mmio_idx, format_memory_size(mmio_idx * 8 * 4), mmio_index);
        #endif

        /* can we free the binary buffer? */
        #ifndef CONFIG_DEBUG_INTERCEPT_STARTUP
        fio_free(buf);
        #endif
        buf = 0;
        len = 0;

        #ifdef CONFIG_MMIO_TRACE
        io_trace_cleanup();
        #endif

        /* output by blinking? */
        #ifdef CONFIG_DEBUG_INTERCEPT_STARTUP_BLINK
        blink_init();
        blink_str(msg_buf);
        #endif

        /* output to file? */
        char log_filename[100];
        get_numbered_file_name("dm-%04d.log", 9999, log_filename, sizeof(log_filename));
        NotifyBox(2000, "%s: saving...", log_filename);
        dump_seg(msg_buf, msg_len, log_filename);
        NotifyBox(2000, "%s: saved %d bytes.", log_filename, msg_len);
        printf("%s: saved %d bytes.\n", log_filename, msg_len);

        /* cleanup */
        shoot_free_suite(msg_suite);
        msg_buf = 0;
        msg_len = 0;
    }

    /* finished! */
    beep();
}
