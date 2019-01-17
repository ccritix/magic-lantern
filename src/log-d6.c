/* DIGIC 6 logging experiments
 * based on dm-spy-experiments  code */

#include "dryos.h"

/* fixme */
extern __attribute__((long_call)) void DryosDebugMsg(int,int,const char *,...);
extern void dump_file(char* name, uint32_t addr, uint32_t size);
//extern void * _AllocateMemory(size_t);
//extern void _FreeMemory(void *);
extern int GetMemoryInformation(int *, int *);

/* custom logging buffer */
static char buf[192 * 1024];        /* adjust this until it runs out of memory (512 might work, maybe more) */
static int buf_size = sizeof(buf);
static int len = 0;

char* get_current_task_name()
{
    /* DryOS: right after current_task we have a flag
     * set to 1 when handling an interrupt */
    uint32_t interrupt_active = MEM((uintptr_t)&current_task + 4);
    
    if (!interrupt_active)
    {
        return current_task->name;
    }
    else
    {
        static char isr[] = "**INT-00h**";
        int i = current_interrupt;
        int i0 = (i & 0xF);
        int i1 = (i >> 4) & 0xF;
        int i2 = (i >> 8) & 0xF;
        isr[5] = i2 ? '0' + i2 : '-';
        isr[6] = i1 < 10 ? '0' + i1 : 'A' + i1 - 10;
        isr[7] = i0 < 10 ? '0' + i0 : 'A' + i0 - 10;
        return isr;
    }
}

/* override Canon's DebugMsg to save all messages */
static void my_DebugMsg(int class, int level, char* fmt, ...)
{
    uintptr_t lr = read_lr();

    if (!buf) return;
    if (buf_size - len < 100) return;

    if ((class != 0 || level != 15) && level < 3)
    {
        /* skip "unimportant" messages */
        return;
    }

    uint32_t old = cli();
   
    uint32_t us_timer = MEM(0xD400000C);

    char* task_name = get_current_task_name();
    
    /* Canon's vsnprintf doesn't know %20s */
    char task_name_padded[11] = "           ";
    int spaces = 10 - strlen(task_name);
    if (spaces < 0) spaces = 0;
    snprintf(task_name_padded + spaces, 11 - spaces, "%s", task_name);

    len += snprintf( buf+len, buf_size-len, "%08X> %s:%08x:%02x:%02x: ", us_timer, task_name_padded, lr-4, class, level );

    va_list ap;
    va_start( ap, fmt );
    len += vsnprintf( buf+len, buf_size-len-1, fmt, ap );
    va_end( ap );

    len += snprintf( buf+len, buf_size-len, "\n" );

    sei(old);
}


/* comment out one of these to see anything in QEMU */
/* on real hardware, more interrupts are expected */
static char* isr_names[0x200] = {
    [0x0D] = "Omar",
    [0x0E] = "UTimerDriver",
    [0x19] = "OCH_SPx",
    [0x1B] = "dryos_timer",
    [0x1C] = "Omar",
    [0x1E] = "UTimerDriver",
    [0x28] = "HPTimer",
    [0x29] = "OCHxEPx",
    [0x2A] = "MREQ",
    [0x2D] = "Omar",
    [0x2E] = "UTimerDriver",
    [0x35] = "SlowMossy",
    [0x39] = "OCH_SPx",
    [0x3C] = "Omar",
    [0x3E] = "UTimerDriver",
    [0x41] = "WRDMAC1",
    [0x49] = "OCHxEPx",
    [0x4A] = "MREQ2_ICU",
    [0x4D] = "Omar",
    [0x4E] = "UTimerDriver",
    [0x59] = "OCH_SPx",
    [0x5A] = "LENSIF_SEL",
    [0x5C] = "Omar",
    [0x5E] = "UTimerDriver",
    [0x69] = "OCHxEPx",
    [0x6D] = "Omar",
    [0x6E] = "UTimerDriver",
    [0x79] = "OCH_SPx",
    [0x7A] = "XINT_7",
    [0x7C] = "Omar",
    [0x7E] = "UTimerDriver",
    [0x89] = "OCHxEPx",
    [0x8A] = "INT_LM",
    [0x98] = "CAMIF_0",
    [0x99] = "OCH_SPx",
    [0x9C] = "Omar",
    [0xA8] = "CAMIF_1",
    [0xA9] = "OCHxEPx",
    [0xB9] = "OCH_SPx",
    [0xBC] = "Omar",
    [0xBE] = "sd_dma",
    [0xC9] = "OCHxEPx",
    [0xCA] = "INT_LM",
    [0xCD] = "Omar",
    [0xCE] = "SerialFlash",
    [0xD9] = "ICAPCHx",
    [0xDC] = "Omar",
    [0xDE] = "SerialFlash",
    [0xE9] = "ICAPCHx",
    [0xEE] = "sd_driver",
    [0xF9] = "ICAPCHx",
    [0xFC] = "Omar",
    [0xFE] = "SerialFlash",
    [0x102] = "RDDMAC13",
    [0x109] = "ICAPCHx",
    [0x10C] = "BltDmac",
    [0x10E] = "SerialFlash",
    [0x119] = "ICAPCHx",
    [0x129] = "ICAPCHx",
    [0x12A] = "mreq",
    [0x139] = "ICAPCHx",
    [0x13A] = "CAPREADY",
    [0x13E] = "xdmac",
    [0x145] = "MZRM",
    [0x147] = "SIO3",
    [0x149] = "ICAPCHx",
    [0x14E] = "xdmac",
    [0x159] = "ICAPCHx",
    [0x15D] = "uart_rx",
    [0x15E] = "xdmac",
    [0x169] = "ICAPCHx",
    [0x16D] = "uart_tx",
    [0x16E] = "xdmac",
    [0x179] = "ICAPCHx",
    [0x189] = "ICAPCHx",
    [0x18B] = "WdtInt",
};

static void mpu_decode(const char * in, char * out, int max_len);

static void pre_isr_log(uint32_t isr)
{
//#ifdef CONFIG_DIGIC_VI
    extern const uint32_t isr_table_handler[];
    extern const uint32_t isr_table_param[];
    uint32_t handler = isr_table_handler[2 * isr];
    uint32_t arg     = isr_table_param  [2 * isr];
//#endif

    const char * name = isr_names[isr & 0x1FF];
    //DryosDebugMsg(0, 15, "INT-%03Xh %s %X(%X)", isr, name ? name : "", handler, arg);

    if (isr == 0x2A || isr == 0x12A || isr == 0x147 || isr == 0x1B)
    {
        /* SIO3/MREQ; also check on timer interrupt */
        extern const char * const mpu_send_ring_buffer[50];
        extern const int mpu_send_ring_buffer_tail;
        static int last_tail = 0;
        while (last_tail != mpu_send_ring_buffer_tail)
        {
            char * last_message = &mpu_send_ring_buffer[last_tail][4];
            static char msg[256];
            mpu_decode(last_message, msg, sizeof(msg));
            //qprintf("[%d] mpu_send(%s)%s\n", last_tail, msg, last_message[-2] == 1 ? "" : " ?!?");
            DryosDebugMsg(0, 15, "[%d] *** mpu_send(%s)%s", last_tail, msg, last_message[-2] == 1 ? "" : " ?!?");
            INC_MOD(last_tail, COUNT(mpu_send_ring_buffer));
        }
    }
}

static void post_isr_log(uint32_t isr)
{
    if (isr == 0x147)
    {
        /* expecting at most one message fully received at the end of this interrupt */
        extern const char * const mpu_recv_ring_buffer[80];
        extern const int mpu_recv_ring_buffer_tail;
        static int last_tail = 0;

        if (last_tail != mpu_recv_ring_buffer_tail)
        {
            const char * last_message = &mpu_recv_ring_buffer[last_tail][4];
            static char msg[256];
            mpu_decode(last_message, msg, sizeof(msg));
            //qprintf("[%d] mpu_recv(%s)\n", last_tail, msg);
            DryosDebugMsg(0, 15, "[%d] *** mpu_recv(%s)", last_tail, msg);
            last_tail = mpu_recv_ring_buffer_tail;
        }
    }
}

extern void (*pre_isr_hook)();
extern void (*post_isr_hook)();

static void mpu_decode(const char * in, char * out, int max_len)
{
    int len = 0;
    int size = in[0];

    /* print each byte as hex */
    for (const char * c = in; c < in + size; c++)
    {
        len += snprintf(out+len, max_len-len, "%02x ", *c);
    }
    
    /* trim the last space */
    if (len) out[len-1] = 0;
}

#if 0
extern int (*mpu_recv_cbr)(char * buf, int size);
extern int __attribute__((long_call)) mpu_recv(char * buf);

static int mpu_recv_log(char * buf, int size_unused)
{
    int size = buf[-1];
    char msg[256];
    mpu_decode(buf, msg, sizeof(msg));
    DryosDebugMsg(0, 15, "*** mpu_recv(%02x %s)", size, msg);

    /* call the original */
    return mpu_recv(buf);
}
#endif

int GetFreeMemForAllocateMemory()
{
    int a,b;
    GetMemoryInformation(&a,&b);
    return b;
}

void log_start()
{
    /* allocate memory for our logging buffer */
    //buf_size = 1024 * 1024;
    //qprintf("Free memory: %X\n", GetFreeMemForAllocateMemory());
    //buf = _AllocateMemory(buf_size);
    qprintf("Logging buffer: %X - %X\n", buf, buf + buf_size - 1);
    //qprintf("Free memory: %X\n", GetFreeMemForAllocateMemory());
    while (!buf);

    /* override Canon's DebugMsg (requires RAM address) */
    uint32_t old_int = cli();
    uint32_t DebugMsg_addr = (uint32_t) &DryosDebugMsg & ~1;
    qprintf("Replacing %X DebugMsg with %X...\n", DebugMsg_addr, &my_DebugMsg);
    MEM(DebugMsg_addr)     = 0xC004F8DF;    /* ldr.w  r12, [pc, #4] */
    MEM(DebugMsg_addr + 4) = 0x00004760;    /* bx r12 */
    MEM(DebugMsg_addr + 8) = (uint32_t) &my_DebugMsg;
    sync_caches();
    sei(old_int);

    /* install hooks before and after each hardware interrupt */
    pre_isr_hook = &pre_isr_log;
    post_isr_hook = &post_isr_log;

#if 0
    /* wait for InitializeIntercom to complete
     * then install our own hook quickly
     * this assumes Canon's init_task is already running */
    while (!mpu_recv_cbr)
    {
        msleep(10);
    }
    mpu_recv_cbr = &mpu_recv_log;
#endif

    //dm_set_store_level(255, 1);
    DryosDebugMsg(0, 15, "Logging started.");
    //DryosDebugMsg(0, 15, "Free memory: %d bytes.", GetFreeMemForAllocateMemory());

    sync_caches();
}

void log_finish()
{
    //dm_set_store_level(255, 15);
    DryosDebugMsg(0, 15, "Logging finished.");
    DryosDebugMsg(0, 15, "Free memory: %d bytes.", GetFreeMemForAllocateMemory());

    qprintf("Saving log %X size %X...\n", buf, len);
    dump_file("DEBUGMSG.LOG", (uint32_t) buf, len);

    pre_isr_hook = 0;
    post_isr_hook = 0;
    sync_caches();
}
