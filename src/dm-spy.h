#ifndef _dm_spy_h_
#define _dm_spy_h_

#include "dryos.h"

/* formatting the messages as they are received may be too CPU-intensive in some scenarios
 * (such as photo capture or LiveView)
 * let's store the logs in a binary (fast) data structure and format things later
 */
struct debug_msg
{
    uint32_t block_size;    /* total size of this block (multiple of 4) */
    char * msg;             /* pointer to message (if it's in RAM, it will be copied) */
    uint32_t us_timer;      /* 20-bit, to be unwrapped later */
    uint32_t pc;            /* where the message happened (LR-4, DFAR etc) */
    uint32_t interrupt;     /* interrupt ID is shifted by 2; LSB is 1 if valid */
    char * task_name;       /* assume the task name string will still be around when saving the logs */
    char * class_name;      /* optional class name, e.g. MMIO */
    uint32_t class;         /* first arg of DebugMsg */
    uint32_t level;         /* second arg of DebugMsg */
    uint32_t mmio_index;    /* to sync with MMIO messages; 0 (omitted) if unused */
    uint32_t edmac_index;   /* to sync with EDMAC polling messages (TODO, edmac.mo) */
    /* ... */               /* todo: sync with other loggers that may have their own data structure */
    /* optional payload (variable size) */
} __attribute__((packed,aligned(4)));

/****** before the logging session *******/

/* start/stop logging */
void debug_intercept();

/****** during the logging session *******/

/* check whether any logging is in progress */
/* used e.g. at shutdown to auto-save the file */
int debug_intercept_running();

/* log (enqueue) a simple line of text without formatting
 * a newline will be appended, a timestamp will be prepended
 * can be used for a multiline string as well, if you don't mind the extra newline
 * and the timestamp prepended only to the first line */
void debug_log_line(const char * str);

/* for very simple messages formatted without snprintf
 * to be used if that's too heavy, not yet available or whatever
 * line-buffered; timestamped and enqueued when \n is received
 * to be called with interrupts cleared (not checked) */
void debug_logstr(const char * str);
void debug_loghex(uint32_t x);
void debug_loghex2(uint32_t x, int digits);

/* for further customization */
struct debug_msg * debug_get_last_block();

/****** after the logging session *******/

/* to be called by other logging code that may want to print messages formatted in the same way */
/* behavior: scnprintf-like */
int debug_format_msg(struct debug_msg * dm, char * msg, int size);

#endif // _dm_spy_h_
