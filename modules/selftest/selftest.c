#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <menu.h>
#include <beep.h>
#include <lens.h>
#include <shoot.h>
#include <zebra.h>
#include <propvalues.h>
#include <timer.h>
#include <console.h>
#include <ml_rpc.h>
#include <edmac-memcpy.h>
#include <screenshot.h>
#include <powersave.h>

/* optional routines */
extern WEAK_FUNC(ret_0) uint32_t ml_rpc_send(uint32_t command, uint32_t parm1, uint32_t parm2, uint32_t parm3, uint32_t wait);
extern WEAK_FUNC(ret_0) void ml_rpc_verbose(uint32_t state);
extern WEAK_FUNC(ret_0) void expfuse_preview_update_task(int direction);
extern WEAK_FUNC(ret_0) void playback_compare_images_task(int direction);

/* macros are used to get the names from consts.h in the stub test log */
#define MALLOC_FREE_MEMORY GetFreeMemForMalloc()
#define DISPLAY_IS_ON display_is_on()
#define PLAY_MODE is_play_mode()
#define MENU_MODE is_menu_mode()
#define DLG_SIGNATURE get_dlg_signature()
#define HALFSHUTTER_PRESSED get_halfshutter_pressed()
#define CURRENT_DIALOG_MAYBE get_gui_mode()

/* button codes */
static int BGMT_PLAY;
static int BGMT_MENU;
static int BGMT_INFO;
static int BGMT_LV;
static int BGMT_PRESS_SET;
static int BGMT_WHEEL_LEFT;
static int BGMT_WHEEL_RIGHT;
static int BGMT_WHEEL_UP;
static int BGMT_WHEEL_DOWN;

/* some private functions that should not be called from user code */
extern void* __priv_malloc(size_t size);
extern void  __priv_free(void* ptr);
extern void* __priv_AllocateMemory(size_t size);
extern void  __priv_FreeMemory(void* ptr);
extern void* __priv_alloc_dma_memory(size_t size);
extern void  __priv_free_dma_memory(void* ptr);
extern void* __priv_shoot_malloc(size_t size);
extern void  __priv_shoot_free(void* ptr);
#define _malloc __priv_malloc
#define _free __priv_free
#define _AllocateMemory __priv_AllocateMemory
#define _FreeMemory __priv_FreeMemory
#define _alloc_dma_memory __priv_alloc_dma_memory
#define _free_dma_memory __priv_free_dma_memory
#define _shoot_malloc __priv_shoot_malloc
#define _shoot_free __priv_shoot_free


static void stress_test_picture(int n, int delay)
{
    if (shutter_count > 20000)
    {
        NotifyBox(2000, "Skipping picture taking test");
        msleep(2000);
        beep();
        return;
    }
    
    msleep(delay);
    for (int i = 0; i < n; i++)
    {
        NotifyBox(10000, "Picture taking: %d/%d", i+1, n);
        msleep(200);
        lens_take_picture(64, 0);
    }
    lens_wait_readytotakepic(64);
    msleep(delay);
}

static volatile int timer_func = 0;
static volatile int timer_arg = 0;
static volatile int64_t timer_time = 0;

static void timer_cbr(int arg1, void* arg2)
{
    timer_func = 1;
    timer_arg = arg1;
    timer_time = get_us_clock_value();
}

static void overrun_cbr(int arg1, void* arg2)
{
    timer_func = 2;
    timer_arg = arg1;
    timer_time = get_us_clock_value();
}

static void next_tick_cbr(int arg1, void* arg2)
{
    timer_func = 3;
    timer_arg = arg1;
    timer_time = get_us_clock_value();
    SetHPTimerNextTick(arg1, 100000, timer_cbr, overrun_cbr, 0);
}

#define TEST_MSG(fmt, ...) { if (!silence || !ok) log_len += snprintf(log_buf + log_len, maxlen - log_len, fmt, ## __VA_ARGS__); printf(fmt, ## __VA_ARGS__); }
#define TEST_VOID(x) { x; ok = 1; TEST_MSG("       %s\n", #x); }
#define TEST_FUNC(x) { int ans = (int)(x); ok = 1; TEST_MSG("       %s => 0x%x\n", #x, ans); }
#define TEST_FUNC_CHECK(x, condition) { int ans = (int)(x); ok = ans condition; TEST_MSG("[%s] %s => 0x%x\n", ok ? "Pass" : "FAIL", #x, ans); if (ok) passed_tests++; else failed_tests++; }
#define TEST_FUNC_CHECK_STR(x, expected_string) { char* ans = (char*)(x); ok = streq(ans, expected_string); TEST_MSG("[%s] %s => '%s'\n", ok ? "Pass" : "FAIL", #x, ans); if (ok) passed_tests++; else { failed_tests++; msleep(500); } }

static int test_task_created = 0;
static void test_task() { test_task_created = 1; }

static void stub_test_task(void* arg)
{
    int maxlen = 1024*1024;
    int log_len = 0;
    char* log_buf = fio_malloc(maxlen);
    if (!log_buf) return;
    
    console_show();

    // this test can be repeated many times, as burn-in test
    int n = (int)arg > 0 ? 1 : 100;
    msleep(1000);
    info_led_on();
    int passed_tests = 0;
    int failed_tests = 0;

    int silence = 0;    // if 1, only failures are logged to file
    int ok = 1;

    for (int i=0; i < n; i++)
    {
        /* File I/O */

        FILE* f;
        TEST_FUNC_CHECK(f = FIO_CreateFile("test.dat"), != 0);
        TEST_FUNC_CHECK(FIO_WriteFile(f, (void*)0xFF000000, 0x10000), == 0x10000);
        TEST_FUNC_CHECK(FIO_WriteFile(f, (void*)0xFF000000, 0x10000), == 0x10000);
        TEST_VOID(FIO_CloseFile(f));
        uint32_t size;
        TEST_FUNC_CHECK(FIO_GetFileSize("test.dat", &size), == 0);
        TEST_FUNC_CHECK(size, == 0x20000);
        void* p;
        TEST_FUNC_CHECK(p = (void*)_alloc_dma_memory(0x20000), != 0);
        TEST_FUNC_CHECK(f = FIO_OpenFile("test.dat", O_RDONLY | O_SYNC), != 0);
        TEST_FUNC_CHECK(FIO_ReadFile(f, p, 0x20000), == 0x20000);
        TEST_VOID(FIO_CloseFile(f));
        TEST_VOID(_free_dma_memory(p));

        {
            int count = 0;
            FILE* f = FIO_CreateFile("test.dat");
            if (f)
            {
                for (int i = 0; i < 1000; i++)
                    count += FIO_WriteFile(f, "Will it blend?\n", 15);
                FIO_CloseFile(f);
            }
            TEST_FUNC_CHECK(count, == 1000*15);
        }
        
        /* FIO_SeekSkipFile test */
        {
            void* buf = 0;
            TEST_FUNC_CHECK(buf = fio_malloc(0x1000000), != 0);
            memset(buf, 0x13, 0x1000000);
            if (buf)
            {
                /* create a file a little higher than 2 GiB for testing */
                /* to make sure the stub handles 64-bit position arguments */
                FILE* f = FIO_CreateFile("test.dat");
                if (f)
                {
                    printf("Creating a 2GB file...       ");
                    for (int i = 0; i < 130; i++)
                    {
                        printf("\b\b\b\b\b\b\b%3d/130", i);
                        FIO_WriteFile(f, buf, 0x1000000);
                    }
                    printf("\n");
                    FIO_CloseFile(f);
                    TEST_FUNC_CHECK(FIO_GetFileSize_direct("test.dat"), == (int)0x82000000);
                    
                    /* now reopen it to append something */
                    TEST_FUNC_CHECK(f = FIO_OpenFile("test.dat", O_RDWR | O_SYNC), != 0);
                    TEST_FUNC_CHECK(FIO_SeekSkipFile(f, 0, SEEK_END), == (int)0x82000000);
                    TEST_FUNC_CHECK(FIO_WriteFile(f, buf, 0x10), == 0x10);

                    /* some more seeking around */
                    TEST_FUNC_CHECK(FIO_SeekSkipFile(f, -0x20, SEEK_END), == (int)0x81fffff0);
                    TEST_FUNC_CHECK(FIO_WriteFile(f, buf, 0x30), == 0x30);
                    TEST_FUNC_CHECK(FIO_SeekSkipFile(f, 0x20, SEEK_SET), == 0x20);
                    TEST_FUNC_CHECK(FIO_SeekSkipFile(f, 0x30, SEEK_CUR), == 0x50);
                    TEST_FUNC_CHECK(FIO_SeekSkipFile(f, -0x20, SEEK_CUR), == 0x30);
                    
                    /* note: seeking past the end of a file does not work on all cameras, so we'll not test that */

                    FIO_CloseFile(f);
                    TEST_FUNC_CHECK(FIO_GetFileSize_direct("test.dat"), == (int)0x82000020);
                }
            }
            fio_free(buf);
        }

        TEST_FUNC_CHECK(FIO_RemoveFile("test.dat"), == 0);


        /* GUI timers */
        
        /* SetTimerAfter, CancelTimer */
        {
            int t0 = get_us_clock_value()/1000;
            int ta0 = 0;

            /* this one should overrun */
            timer_func = 0;
            TEST_FUNC_CHECK(SetTimerAfter(0, timer_cbr, overrun_cbr, 0), == 0x15);
            TEST_FUNC_CHECK(timer_func, == 2);
            ta0 = timer_arg;

            /* this one should not overrun */
            timer_func = 0;
            TEST_FUNC_CHECK(SetTimerAfter(1000, timer_cbr, overrun_cbr, 0), % 2 == 0);
            TEST_VOID(msleep(900));
            TEST_FUNC_CHECK(timer_func, == 0);  /* ta0 +  900 => CBR should not be called yet */
            TEST_VOID(msleep(200));
            TEST_FUNC_CHECK(timer_func, == 1);  /* ta0 + 1100 => CBR should be called by now */
            TEST_FUNC_CHECK(ABS((timer_time/1000 - t0) - 1000), <= 20);
            TEST_FUNC_CHECK(ABS((timer_arg - ta0) - 1000), <= 20);
            // current time: ta0+1100

            /* this one should not call the CBR, because we'll cancel it */
            timer_func = 0;
            int timer;
            TEST_FUNC_CHECK(timer = SetTimerAfter(1000, timer_cbr, overrun_cbr, 0), % 2 == 0);
            TEST_VOID(msleep(400));
            TEST_VOID(CancelTimer(timer));
            TEST_FUNC_CHECK(timer_func, == 0);  /* ta0 + 1500 => CBR should be not be called, and we'll cancel it early */
            TEST_VOID(msleep(1500));
            TEST_FUNC_CHECK(timer_func, == 0);  /* ta0 + 3000 => CBR should be not be called, because it was canceled */
        }
        
        /* microsecond timer wraps around at 1048576 */
        int DeltaT(int a, int b)
        {
            return MOD(a - b, 1048576);
        }

        /* SetHPTimerNextTick, SetHPTimerAfterTimeout, SetHPTimerAfterNow */
        {
            /* run these tests in PLAY mode, because the CPU usage is higher in other modes, and may influence the results */
            SetGUIRequestMode(1);
            msleep(1000);

            int64_t t0 = get_us_clock_value();
            int ta0 = 0;

            /* this one should overrun */
            timer_func = 0;
            TEST_FUNC_CHECK(SetHPTimerAfterNow(0, timer_cbr, overrun_cbr, 0), == 0x15);
            TEST_FUNC_CHECK(timer_func, == 2);
            ta0 = timer_arg;

            /* this one should not overrun */
            timer_func = 0;
            TEST_FUNC_CHECK(SetHPTimerAfterNow(100000, timer_cbr, overrun_cbr, 0), % 2 == 0);
            TEST_VOID(msleep(90));
            TEST_FUNC_CHECK(timer_func, == 0);  /* ta0 +  90000 => CBR should not be called yet */
            TEST_VOID(msleep(20));
            TEST_FUNC_CHECK(timer_func, == 1);  /* ta0 + 110000 => CBR should be called by now */
            
            TEST_FUNC_CHECK(ABS(DeltaT(timer_time, t0) - 100000), <= 1000);
            TEST_FUNC_CHECK(ABS(DeltaT(timer_arg, ta0) - 100000), <= 1000);
            TEST_FUNC_CHECK(ABS((get_us_clock_value() - t0) - 110000), <= 1000);

            /* this one should call SetHPTimerNextTick in the CBR */
            timer_func = 0;
            TEST_FUNC_CHECK(SetHPTimerAfterNow(90000, next_tick_cbr, overrun_cbr, 0), % 2 == 0);
            TEST_VOID(msleep(80));
            TEST_FUNC_CHECK(timer_func, == 0);  /* ta0 + 190000 => CBR should not be called yet */
            TEST_VOID(msleep(20));
            TEST_FUNC_CHECK(timer_func, == 3);  /* ta0 + 210000 => next_tick_cbr should be called by now */
                                                    /* and it will setup timer_cbr via SetHPTimerNextTick */
            TEST_VOID(msleep(80));
            TEST_FUNC_CHECK(timer_func, == 3);  /* ta0 + 290000 => timer_cbr should not be called yet */
            TEST_VOID(msleep(20));
            TEST_FUNC_CHECK(timer_func, == 1);  /* ta0 + 310000 => timer_cbr should be called by now */
            TEST_FUNC_CHECK(ABS(DeltaT(timer_time, t0) - 300000), <= 1000);
            TEST_FUNC_CHECK(ABS(DeltaT(timer_arg, ta0) - 300000), <= 1000);
            TEST_FUNC_CHECK(ABS((get_us_clock_value() - t0) - 310000), <= 1000);
        }

        /* uncomment to test only the timers */
        //~ continue;

        // strlen
        TEST_FUNC_CHECK(strlen("abc"), == 3);
        TEST_FUNC_CHECK(strlen("qwertyuiop"), == 10);
        TEST_FUNC_CHECK(strlen(""), == 0);

        // strcpy
        char msg[10];
        TEST_FUNC_CHECK(strcpy(msg, "hi there"), == (int)msg);
        TEST_FUNC_CHECK_STR(msg, "hi there");

        // strcmp, snprintf
        // gcc will optimize strcmp calls with constant arguments, so use snprintf to force gcc to call strcmp
        char a[50]; char b[50];

        TEST_FUNC_CHECK(snprintf(a, sizeof(a), "foo"), == 3);
        TEST_FUNC_CHECK(snprintf(b, sizeof(b), "foo"), == 3);
        TEST_FUNC_CHECK(strcmp(a, b), == 0);

        TEST_FUNC_CHECK(snprintf(a, sizeof(a), "bar"), == 3);
        TEST_FUNC_CHECK(snprintf(b, sizeof(b), "baz"), == 3);
        TEST_FUNC_CHECK(strcmp(a, b), < 0);

        TEST_FUNC_CHECK(snprintf(a, sizeof(a), "Display"), == 7);
        TEST_FUNC_CHECK(snprintf(b, sizeof(b), "Defishing"), == 9);
        TEST_FUNC_CHECK(strcmp(a, b), > 0);

        // vsnprintf (called by snprintf)
        char buf[4];
        TEST_FUNC_CHECK(snprintf(buf, 3, "%d", 1234), == 2);
        TEST_FUNC_CHECK_STR(buf, "12");

        // memcpy, memset, bzero32
        char foo[] __attribute__((aligned(32))) = "qwertyuiop";
        char bar[] __attribute__((aligned(32))) = "asdfghjkl;";
        TEST_FUNC_CHECK(memcpy(foo, bar, 6), == (int)foo);
        TEST_FUNC_CHECK_STR(foo, "asdfghuiop");
        TEST_FUNC_CHECK(memset(bar, '*', 5), == (int)bar);
        TEST_FUNC_CHECK_STR(bar, "*****hjkl;");
        TEST_VOID(bzero32(bar + 5, 5));
        TEST_FUNC_CHECK_STR(bar, "****");

        // digic clock, msleep
        int t0, t1;
        TEST_FUNC(t0 = *(uint32_t*)0xC0242014);
        TEST_VOID(msleep(250));
        TEST_FUNC(t1 = *(uint32_t*)0xC0242014);
        TEST_FUNC_CHECK(ABS(MOD(t1-t0, 1048576)/1000 - 250), < 30);

        // calendar
        struct tm now;
        int s0, s1;
        TEST_VOID(LoadCalendarFromRTC( &now ));
        TEST_FUNC(s0 = now.tm_sec);

        TEST_MSG(
            "       Date/time: %04d/%02d/%02d %02d:%02d:%02d\n",
            now.tm_year + 1900,
            now.tm_mon + 1,
            now.tm_mday,
            now.tm_hour,
            now.tm_min,
            now.tm_sec
        );

        TEST_VOID(msleep(1500));
        TEST_VOID(LoadCalendarFromRTC( &now ));
        TEST_FUNC(s1 = now.tm_sec);
        TEST_FUNC_CHECK(MOD(s1-s0, 60), >= 1);
        TEST_FUNC_CHECK(MOD(s1-s0, 60), <= 2);

        // mallocs
        // bypass the memory backend and use low-level calls only for these tests
        // run this test 200 times to check for memory leaks
        for (int i = 0; i < 200; i++)
        {
            int silence = (i > 0);
            int m0, m1, m2;
            void* p;
            TEST_FUNC(m0 = MALLOC_FREE_MEMORY);
            TEST_FUNC_CHECK(p = (void*)_malloc(50*1024), != 0);
            TEST_FUNC_CHECK(CACHEABLE(p), == (int)p);
            TEST_FUNC(m1 = MALLOC_FREE_MEMORY);
            TEST_VOID(_free(p));
            TEST_FUNC(m2 = MALLOC_FREE_MEMORY);
            TEST_FUNC_CHECK(ABS((m0-m1) - 50*1024), < 2048);
            TEST_FUNC_CHECK(ABS(m0-m2), < 2048);

            TEST_FUNC(m0 = GetFreeMemForAllocateMemory());
            TEST_FUNC_CHECK(p = (void*)_AllocateMemory(256*1024), != 0);
            TEST_FUNC_CHECK(CACHEABLE(p), == (int)p);
            TEST_FUNC(m1 = GetFreeMemForAllocateMemory());
            TEST_VOID(_FreeMemory(p));
            TEST_FUNC(m2 = GetFreeMemForAllocateMemory());
            TEST_FUNC_CHECK(ABS((m0-m1) - 256*1024), < 2048);
            TEST_FUNC_CHECK(ABS(m0-m2), < 2048);

            // these buffers may be from different memory pools, just check for leaks in main pools
            int m01, m02, m11, m12;
            TEST_FUNC(m01 = MALLOC_FREE_MEMORY);
            TEST_FUNC(m02 = GetFreeMemForAllocateMemory());
            TEST_FUNC_CHECK(p = (void*)_alloc_dma_memory(256*1024), != 0);
            TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
            TEST_FUNC_CHECK(CACHEABLE(p), != (int)p);
            TEST_FUNC_CHECK(UNCACHEABLE(CACHEABLE(p)), == (int)p);
            TEST_VOID(_free_dma_memory(p));
            TEST_FUNC_CHECK(p = (void*)_shoot_malloc(24*1024*1024), != 0);
            TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
            TEST_VOID(_shoot_free(p));
            TEST_FUNC(m11 = MALLOC_FREE_MEMORY);
            TEST_FUNC(m12 = GetFreeMemForAllocateMemory());
            TEST_FUNC_CHECK(ABS(m01-m11), < 2048);
            TEST_FUNC_CHECK(ABS(m02-m12), < 2048);
        }

        // exmem
        // run this test 20 times to check for memory leaks
        for (int i = 0; i < 20; i++)
        {
            int silence = (i > 0);

            struct memSuite * suite = 0;
            struct memChunk * chunk = 0;
            void* p = 0;
            int total = 0;

            // contiguous allocation
            TEST_FUNC_CHECK(suite = shoot_malloc_suite_contig(24*1024*1024), != 0);
            TEST_FUNC_CHECK_STR(suite->signature, "MemSuite");
            TEST_FUNC_CHECK(suite->num_chunks, == 1);
            TEST_FUNC_CHECK(suite->size, == 24*1024*1024);
            TEST_FUNC_CHECK(chunk = GetFirstChunkFromSuite(suite), != 0);
            TEST_FUNC_CHECK_STR(chunk->signature, "MemChunk");
            TEST_FUNC_CHECK(chunk->size, == 24*1024*1024);
            TEST_FUNC_CHECK(p = GetMemoryAddressOfMemoryChunk(chunk), != 0);
            TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
            TEST_VOID(shoot_free_suite(suite); suite = 0; chunk = 0;);

            // contiguous allocation, largest block
            TEST_FUNC_CHECK(suite = shoot_malloc_suite_contig(0), != 0);
            TEST_FUNC_CHECK_STR(suite->signature, "MemSuite");
            TEST_FUNC_CHECK(suite->num_chunks, == 1);
            TEST_FUNC_CHECK(suite->size, > 24*1024*1024);
            TEST_FUNC_CHECK(chunk = GetFirstChunkFromSuite(suite), != 0);
            TEST_FUNC_CHECK_STR(chunk->signature, "MemChunk");
            TEST_FUNC_CHECK(chunk->size, == suite->size);
            TEST_FUNC_CHECK(p = GetMemoryAddressOfMemoryChunk(chunk), != 0);
            TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
            TEST_VOID(shoot_free_suite(suite); suite = 0; chunk = 0;);

            // fragmented allocation
            TEST_FUNC_CHECK(suite = shoot_malloc_suite(64*1024*1024), != 0);
            TEST_FUNC_CHECK_STR(suite->signature, "MemSuite");
            TEST_FUNC_CHECK(suite->num_chunks, > 1);
            TEST_FUNC_CHECK(suite->size, == 64*1024*1024);

            // iterating through chunks
            total = 0;
            TEST_FUNC_CHECK(chunk = GetFirstChunkFromSuite(suite), != 0);
            while(chunk)
            {
                TEST_FUNC_CHECK_STR(chunk->signature, "MemChunk");
                TEST_FUNC_CHECK(total += chunk->size, <= 64*1024*1024);
                TEST_FUNC_CHECK(p = GetMemoryAddressOfMemoryChunk(chunk), != 0);
                TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
                TEST_FUNC(chunk = GetNextMemoryChunk(suite, chunk));
            }
            TEST_FUNC_CHECK(total, == 64*1024*1024);
            TEST_VOID(shoot_free_suite(suite); suite = 0; chunk = 0; );

            // fragmented allocation, max size
            TEST_FUNC_CHECK(suite = shoot_malloc_suite(0), != 0);
            TEST_FUNC_CHECK_STR(suite->signature, "MemSuite");
            TEST_FUNC_CHECK(suite->num_chunks, > 1);
            TEST_FUNC_CHECK(suite->size, > 64*1024*1024);

            // iterating through chunks
            total = 0;
            TEST_FUNC_CHECK(chunk = GetFirstChunkFromSuite(suite), != 0);
            while(chunk)
            {
                TEST_FUNC_CHECK_STR(chunk->signature, "MemChunk");
                TEST_FUNC_CHECK(total += chunk->size, <= suite->size);
                TEST_FUNC_CHECK(p = GetMemoryAddressOfMemoryChunk(chunk), != 0);
                TEST_FUNC_CHECK(UNCACHEABLE(p), == (int)p);
                TEST_FUNC(chunk = GetNextMemoryChunk(suite, chunk));
            }
            TEST_FUNC_CHECK(total, == suite->size);
            TEST_VOID(shoot_free_suite(suite); suite = 0; chunk = 0; );
        }

        // engio
        TEST_VOID(EngDrvOut(LCD_Palette[0], 0x1234));
        TEST_FUNC_CHECK(shamem_read(LCD_Palette[0]), == 0x1234);

        // call, DISPLAY_IS_ON
        TEST_VOID(call("TurnOnDisplay"));
        TEST_FUNC_CHECK(DISPLAY_IS_ON, != 0);
        TEST_VOID(call("TurnOffDisplay"));
        TEST_FUNC_CHECK(DISPLAY_IS_ON, == 0);
        TEST_VOID(call("TurnOnDisplay"));
        TEST_FUNC_CHECK(DISPLAY_IS_ON, != 0);

        // SetGUIRequestMode, CURRENT_DIALOG_MAYBE
        TEST_VOID(SetGUIRequestMode(1); msleep(1000););
        TEST_FUNC_CHECK(CURRENT_DIALOG_MAYBE, == 1);
        TEST_VOID(SetGUIRequestMode(2); msleep(1000););
        TEST_FUNC_CHECK(CURRENT_DIALOG_MAYBE, == 2);
        TEST_VOID(SetGUIRequestMode(0); msleep(1000););
        TEST_FUNC_CHECK(CURRENT_DIALOG_MAYBE, == 0);
        TEST_FUNC_CHECK(display_idle(), != 0);

        // GUI_Control
        msleep(1000);
        TEST_VOID(GUI_Control(BGMT_PLAY, 0, 0, 0); msleep(1000););
        TEST_FUNC_CHECK(PLAY_MODE, != 0);
        TEST_FUNC_CHECK(MENU_MODE, == 0);
        TEST_VOID(GUI_Control(BGMT_MENU, 0, 0, 0); msleep(1000););
        TEST_FUNC_CHECK(MENU_MODE, != 0);
        TEST_FUNC_CHECK(PLAY_MODE, == 0);

        // also check DLG_SIGNATURE here, because display is on for sure
        struct gui_task * current = gui_task_list.current;
        struct dialog * dialog = current->priv;
        TEST_FUNC_CHECK(MEM(dialog->type), == DLG_SIGNATURE);

        TEST_VOID(GUI_Control(BGMT_MENU, 0, 0, 0); msleep(500););
        TEST_FUNC_CHECK(MENU_MODE, == 0);
        TEST_FUNC_CHECK(PLAY_MODE, == 0);

        // task_create
        TEST_FUNC(task_create("test", 0x1c, 0x1000, test_task, 0));
        msleep(100);
        TEST_FUNC_CHECK(test_task_created, == 1);
        TEST_FUNC_CHECK_STR(get_task_name_from_id(get_current_task()), "run_test");
        
        extern int task_max;
        TEST_FUNC_CHECK(task_max, >= 104);    /* so far, task_max is 104 on most cameras */
        TEST_FUNC_CHECK(task_max, <= 512);    /* I guess it's not higher than that */

        // mq
        static struct msg_queue * mq = 0;
        int m = 0;
        TEST_FUNC_CHECK(mq = mq ? mq : (void*)msg_queue_create("test", 5), != 0);
        TEST_FUNC_CHECK(msg_queue_post(mq, 0x1234567), == 0);
        TEST_FUNC_CHECK(msg_queue_receive(mq, (struct event **) &m, 500), == 0);
        TEST_FUNC_CHECK(m, == 0x1234567);
        TEST_FUNC_CHECK(msg_queue_receive(mq, (struct event **) &m, 500), != 0);

        // sem
        static struct semaphore * sem = 0;
        TEST_FUNC_CHECK(sem = sem ? sem : create_named_semaphore("test", 1), != 0);
        TEST_FUNC_CHECK(take_semaphore(sem, 500), == 0);
        TEST_FUNC_CHECK(take_semaphore(sem, 500), != 0);
        TEST_FUNC_CHECK(give_semaphore(sem), == 0);
        TEST_FUNC_CHECK(take_semaphore(sem, 500), == 0);
        TEST_FUNC_CHECK(give_semaphore(sem), == 0);

        // recursive lock
        static void * rlock = 0;
        TEST_FUNC_CHECK(rlock = rlock ? rlock : CreateRecursiveLock(0), != 0);
        TEST_FUNC_CHECK(AcquireRecursiveLock(rlock, 500), == 0);
        TEST_FUNC_CHECK(AcquireRecursiveLock(rlock, 500), == 0);
        TEST_FUNC_CHECK(ReleaseRecursiveLock(rlock), == 0);
        TEST_FUNC_CHECK(ReleaseRecursiveLock(rlock), == 0);
        TEST_FUNC_CHECK(ReleaseRecursiveLock(rlock), != 0);

        // sw1
        TEST_VOID(SW1(1,100));
        TEST_FUNC_CHECK(HALFSHUTTER_PRESSED, == 1);
        TEST_VOID(SW1(0,100));
        TEST_FUNC_CHECK(HALFSHUTTER_PRESSED, == 0);

        beep();
    }

    FILE* log = FIO_CreateFile( "stubtest.log" );
    if (log)
    {
        FIO_WriteFile(log, log_buf, log_len);
        FIO_CloseFile(log);
    }
    fio_free(log_buf);

    printf(
        "=========================================================\n"
        "Test complete, %d passed, %d failed.\n.",
        passed_tests, failed_tests
    );
}

static void rpc_test_task(void* unused)
{
    uint32_t loops = 0;

    ml_rpc_verbose(1);
    while(1)
    {
        msleep(50);

        ml_rpc_send(ML_RPC_PING, *(volatile uint32_t *)0xC0242014, 0, 0, 1);
        loops++;
    }
    ml_rpc_verbose(0);
}

static void stress_test_task(void* unused)
{
    NotifyBox(10000, "Stability Test..."); msleep(2000);

    msleep(2000);

    /* 50D: taking pics while REC crashes with Canon firmware too */
    if (!is_camera("50D", "*"))
    {
        ensure_movie_mode();
        msleep(1000);
        for (int i = 0; i <= 5; i++)
        {
            NotifyBox(1000, "Pics while recording: %d", i);
            movie_start();
            msleep(1000);
            lens_take_picture(64, 0);
            msleep(1000);
            lens_take_picture(64, 0);
            msleep(1000);
            lens_take_picture(64, 0);
            while (lens_info.job_state) msleep(100);
            while (!lv) msleep(100);
            msleep(1000);
            movie_end();
            msleep(2000);
        }
    }

    msleep(2000);

    extern struct semaphore * gui_sem;

    msleep(2000);

    for (int i = 0; i <= 1000; i++)
    {
        NotifyBox(1000, "ML menu toggle: %d", i);

        if (i == 250)
        {
            msleep(2000);
            gui_stop_menu();
            msleep(500);
            if (!lv) force_liveview();
        }

        if (i == 500)
        {
            msleep(2000);
            gui_stop_menu();
            msleep(500);
            ensure_movie_mode();
            movie_start();
        }

        if (i == 750)
        {
            msleep(2000);
            gui_stop_menu();
            msleep(500);
            movie_end();
            msleep(2000);
            fake_simple_button(BGMT_PLAY);
            msleep(1000);
        }

        give_semaphore(gui_sem);
        msleep(rand()%100);
        info_led_blink(1,50,50);

    }
    msleep(2000);
    gui_stop_menu();
    msleep(1000);
    if (!lv) force_liveview();
    msleep(2000);

    NotifyBox(1000, "Cropmarks preview...");
    select_menu_by_name("Overlay", "Cropmarks");
    give_semaphore( gui_sem );
    msleep(500);
    menu_open_submenu();
    msleep(100);
    for (int i = 0; i <= 100; i++)
    {
        fake_simple_button(BGMT_WHEEL_RIGHT);
        msleep(rand()%500);
    }
    gui_stop_menu();
    msleep(2000);

    NotifyBox(1000, "ML menu scroll...");
    give_semaphore(gui_sem);
    msleep(1000);
    for (int i = 0; i <= 5000; i++)
    {
        static int dir = 0;
        switch(dir)
        {
            case 0: fake_simple_button(BGMT_WHEEL_LEFT); break;
            case 1: fake_simple_button(BGMT_WHEEL_RIGHT); break;
            case 2: fake_simple_button(BGMT_WHEEL_UP); break;
            case 3: fake_simple_button(BGMT_WHEEL_DOWN); break;
            case 4: fake_simple_button(BGMT_INFO); break;
            case 5: fake_simple_button(BGMT_MENU); break;
            //~ case 6: fake_simple_button(BGMT_PRESS_ZOOMIN_MAYBE); break;
        }
        dir = MOD(dir + rand()%3 - 1, 7);
        msleep(20);
    }
    gui_stop_menu();

    msleep(2000);

    beep();
    fake_simple_button(BGMT_PLAY); msleep(1000);
    for (int i = 0; i < 100; i++)
    {
        NotifyBox(1000, "PLAY: image compare: %d", i);
        playback_compare_images_task(1);
    }
    get_out_of_play_mode(500);
    msleep(2000);

    fake_simple_button(BGMT_PLAY); msleep(1000);
    for (int i = 0; i < 10; i++)
    {
        NotifyBox(1000, "PLAY: exposure fusion: %d", i);
        expfuse_preview_update_task(1);
    }
    get_out_of_play_mode(500);
    msleep(2000);

    fake_simple_button(BGMT_PLAY); msleep(1000);
    for (int i = 0; i < 50; i++)
    {
        NotifyBox(1000, "PLAY scrolling: %d", i);
        next_image_in_play_mode(1);
    }
    extern int timelapse_playback;
    timelapse_playback = 1;
    for (int i = 0; i < 50; i++)
    {
        NotifyBox(1000, "PLAY scrolling: %d", i+50);
        msleep(200);
    }
    timelapse_playback = 0;
    get_out_of_play_mode(500);

    msleep(2000);

    if (!lv) force_liveview();

    for (int i = 0; i <= 100; i++)
    {
        int r = rand()%3;
        set_lv_zoom(r == 0 ? 1 : r == 1 ? 5 : 10);
        NotifyBox(1000, "LV zoom test: %d", i);
        msleep(rand()%200);
    }
    set_lv_zoom(1);
    msleep(2000);

    for (int i = 0; i <= 100; i++)
    {
        set_expsim(i%3);
        NotifyBox(1000, "ExpSim toggle: %d", i/10);
        msleep(rand()%100);
    }

    msleep(2000);

    for (int i = 0; i <= 100; i++)
    {
        bv_toggle(0, 1);
        NotifyBox(1000, "Exp.Override toggle: %d", i/10);
        msleep(rand()%100);
    }
    msleep(2000);

/*    for (int i = 0; i < 100; i++)
    {
        NotifyBox(1000, "Disabling Canon GUI (%d)...", i);
        canon_gui_disable();
        msleep(rand()%300);
        canon_gui_enable();
        msleep(rand()%300);
    } */

    msleep(2000);

    NotifyBox(10000, "LCD backlight...");
    int old_backlight_level = backlight_level;
    for (int i = 0; i < 5; i++)
    {
        for (int k = 1; k <= 7; k++)
        {
            set_backlight_level(k);
            msleep(50);
        }
        for (int k = 7; k >= 1; k--)
        {
            set_backlight_level(k);
            msleep(50);
        }
    }
    set_backlight_level(old_backlight_level);

    if (!lv) force_liveview();
    for (int k = 0; k < 10; k++)
    {
        NotifyBox(1000, "LiveView / Playback (%d)...", k*10);
        fake_simple_button(BGMT_PLAY);
        msleep(rand() % 1000);
        SW1(1, rand()%100);
        SW1(0, rand()%100);
        msleep(rand() % 1000);
    }
    if (!lv) force_liveview();
    msleep(2000);
    lens_set_rawiso(0);
    for (int k = 0; k < 5; k++)
    {
        NotifyBox(1000, "LiveView gain test: %d", k*20);
        for (int i = 0; i <= 16; i++)
        {
            set_display_gain_equiv(1<<i);
            msleep(100);
        }
        for (int i = 16; i >= 0; i--)
        {
            set_display_gain_equiv(1<<i);
            msleep(100);
        }
    }
    set_display_gain_equiv(0);

    msleep(1000);

    for (int i = 0; i <= 10; i++)
    {
        NotifyBox(1000, "LED blinking: %d", i*10);
        info_led_blink(10, i*3, (10-i)*3);
    }

    msleep(2000);

    for (int i = 0; i <= 100; i++)
    {
        NotifyBox(1000, "Redraw test: %d", i);
        msleep(50);
        redraw();
        msleep(50);
    }

    msleep(2000);

    NotifyBox(10000, "Menu scrolling");
    fake_simple_button(BGMT_MENU);
    msleep(1000);
    for (int i = 0; i < 5000; i++)
        fake_simple_button(BGMT_WHEEL_LEFT);
    for (int i = 0; i < 5000; i++)
        fake_simple_button(BGMT_WHEEL_RIGHT);
    SW1(1,0);
    SW1(0,0);

    stress_test_picture(2, 2000); // make sure we have at least 2 pictures for scrolling :)

    msleep(2000);

#if 0 // unsafe
    for (int i = 0; i <= 10; i++)
    {
        NotifyBox(1000, "Mode switching: %d", i*10);
        set_shooting_mode(SHOOTMODE_AUTO);    msleep(100);
        set_shooting_mode(SHOOTMODE_MOVIE);    msleep(2000);
        set_shooting_mode(SHOOTMODE_SPORTS);    msleep(100);
        set_shooting_mode(SHOOTMODE_NIGHT);    msleep(100);
        set_shooting_mode(SHOOTMODE_CA);    msleep(100);
        set_shooting_mode(SHOOTMODE_M);    msleep(100);
        ensure_bulb_mode(); msleep(100);
        set_shooting_mode(SHOOTMODE_TV);    msleep(100);
        set_shooting_mode(SHOOTMODE_AV);    msleep(100);
        set_shooting_mode(SHOOTMODE_P);    msleep(100);
    }

    stress_test_picture(2, 2000);
#endif

    if (!lv) force_liveview();
    NotifyBox(10000, "Focus tests...");
    msleep(2000);
    for (int i = 1; i <= 3; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            lens_focus( 1, i, 1, 0);
            lens_focus(-1, i, 1, 0);
        }
    }

    msleep(2000);

    NotifyBox(10000, "Expo tests...");

    if (!lv) force_liveview();
    msleep(1000);
    for (int i = KELVIN_MIN; i <= KELVIN_MAX; i += KELVIN_STEP)
    {
        NotifyBox(1000, "Kelvin: %d", i);
        lens_set_kelvin(i); msleep(200);
    }
    lens_set_kelvin(6500);

    stress_test_picture(2, 2000);

    set_shooting_mode(SHOOTMODE_M);
    msleep(1000);
    if (!lv) force_liveview();
    msleep(1000);

    for (int i = 72; i <= 136; i++)
    {
        NotifyBox(1000, "ISO: raw %d  ", i);
        lens_set_rawiso(i); msleep(200);
    }
    lens_set_rawiso(ISO_400);

    stress_test_picture(2, 2000);

    msleep(5000);
    if (!lv) force_liveview();
    msleep(1000);

    for (int i = 0; i <= 100; i++)
    {
        NotifyBox(1000, "Pause LiveView: %d", i);
        PauseLiveView(); msleep(rand()%200);
        ResumeLiveView(); msleep(rand()%200);
    }

    stress_test_picture(2, 2000);

    msleep(2000);
    if (!lv) force_liveview();
    msleep(1000);

    for (int i = 0; i <= 100; i++)
    {
        NotifyBox(1000, "BMP overlay: %d", i);
        bmp_off(); msleep(rand()%200);
        bmp_on(); msleep(rand()%200);
    }

    stress_test_picture(2, 2000);

    msleep(2000);
    if (!lv) force_liveview();
    msleep(1000);

    for (int i = 0; i <= 100; i++)
    {
        NotifyBox(1000, "Display on/off: %d", i);
        display_off(); msleep(rand()%200);
        display_on(); msleep(rand()%200);
    }

    msleep(3000); // 60D: display on/off is slow and will continue a while after this

    ensure_photo_mode();

    stress_test_picture(2, 2000);

    NotifyBox(10000, "LiveView switch...");
    set_shooting_mode(SHOOTMODE_M);
    for (int i = 0; i < 21; i++)
    {
        fake_simple_button(BGMT_LV); msleep(rand()%200);
    }

    stress_test_picture(2, 2000);

    set_shooting_mode(SHOOTMODE_BULB);

    msleep(1000);
    NotifyBox(10000, "Bulb picture taking");
    bulb_take_pic(2000);
    bulb_take_pic(100);
    bulb_take_pic(1500);
    bulb_take_pic(10);
    bulb_take_pic(1000);
    bulb_take_pic(1);

    NotifyBox(10000, "Movie recording");
    ensure_movie_mode();
    msleep(1000);
    for (int i = 0; i <= 5; i++)
    {
        NotifyBox(10000, "Movie recording: %d", i);
        movie_start();
        msleep(5000);
        movie_end();
        msleep(5000);
    }

    stress_test_picture(2, 2000);

    NotifyBox(2000, "Test complete."); msleep(2000);
    NotifyBox(2000, "Is the camera still working?"); msleep(2000);
    NotifyBox(10000, ":)");
    //~ NotifyBox(10000, "Burn-in test (will take hours!)");
    //~ set_shooting_mode(SHOOTMODE_M);
    //~ xx_test2(0);

}

/*
static void stress_test_toggle_menu_item(char* menu_name, char* item_name)
{
    extern struct semaphore * gui_sem;
    select_menu_by_name(menu_name, item_name);
    if (!gui_menu_shown()) give_semaphore( gui_sem );
    msleep(400);
    fake_simple_button(BGMT_PRESS_SET);
    msleep(200);
    give_semaphore( gui_sem );
    msleep(200);
    return;
} */

static void stress_test_toggle_random_menu_item()
{
    extern struct semaphore * gui_sem;
    if (!gui_menu_shown()) give_semaphore( gui_sem );
    msleep(400);
    int dx = rand() % 20 - 10;
    int dy = rand() % 20 - 10;
    for (int i = 0; i < ABS(dx); i++)
        fake_simple_button(dx > 0 ? BGMT_WHEEL_RIGHT : BGMT_WHEEL_LEFT);
    msleep(200);
    for (int i = 0; i < ABS(dy); i++)
        fake_simple_button(dy > 0 ? BGMT_WHEEL_UP : BGMT_WHEEL_DOWN);
    msleep(200);
    fake_simple_button(BGMT_PRESS_SET);
    msleep(200);
    give_semaphore( gui_sem );
    msleep(200);
    return;
}

static void stress_test_random_action()
{
    switch (rand() % 50)
    {
        case 0:
            lens_take_picture(64, rand() % 2);
            return;
        case 1:
            fake_simple_button(BGMT_LV);
            return;
        case 2:
            fake_simple_button(BGMT_PLAY);
            return;
        case 3:
            fake_simple_button(BGMT_MENU);
            return;
        default:
            stress_test_toggle_random_menu_item();
    }
}

static void stress_test_random_task(void* unused)
{
    extern int config_autosave;
    config_autosave = 0; // this will make many changes in menu, don't save them
    TASK_LOOP
    {
        stress_test_random_action();
        //~ stress_test_toggle_menu_item("Play", "Zoom in PLAY mode");
        msleep(rand() % 1000);
    }
}

/*static void stress_test_random_action_simple()
{
    {
        switch (rand() % 4)
        {
            case 0:
            {
                stress_test_toggle_menu_item("Overlay", "Global Draw");
                return;
            }
            case 1:
                fake_simple_button(BGMT_PLAY);
                return;
            case 2:
                fake_simple_button(BGMT_MENU);
                return;
            case 3:
                fake_simple_button(BGMT_INFO);
                return;
        }
    }
}
*/

static void stress_test_menu_dlg_api_task(void* unused)
{
    msleep(2000);
    info_led_blink(5,50,50);
    extern struct semaphore * gui_sem;
    TASK_LOOP
    {
        give_semaphore(gui_sem);
        msleep(20);
    }
}

static void excessive_redraws_task()
{
    info_led_blink(5,50,1000);
    while(1)
    {
        if (gui_menu_shown()) menu_redraw();
        else redraw();
        msleep(10);
    }
}

static void bmp_fill_test_task()
{
    msleep(2000);
    while(1)
    {
        int x1 = rand() % 720;
        int x2 = rand() % 720;
        int y1 = rand() % 480;
        int y2 = rand() % 480;
        int xm = MIN(x1,x2); int xM = MAX(x1,x2);
        int ym = MIN(y1,y2); int yM = MAX(y1,y2);
        int w = xM-xm;
        int h = yM-ym;
        int c = rand() % 255;
        bmp_fill(c, xm, ym, w, h);
        msleep(20);
    }
}

#if 0
static void menu_duplicate_test()
{
    struct menu * menu_get_root();
    struct menu * menu = menu_get_root();
    
    for( ; menu ; menu = menu->next )
    {
        if (menu == my_menu) continue;
        if (menu == mod_menu) continue;
        
        struct menu_entry * entry = menu->children;
        for( ; entry ; entry = entry->next )
        {
            if (!entry->name) continue;
            if (entry->shidden) continue;
            
            struct menu_entry * e = entry_find_by_name(0, entry->name);
            
            if (e != entry)
            {
                printf("Duplicate: %s->%s\n", menu->name, entry->name);
            }
        }
    }
}

// for menu entries with custom toggle: check if it wraps around in both directions
static int entry_check_wrap(const char* name, const char* entry_name, int dir)
{
    struct menu_entry * entry = entry_find_by_name(name, entry_name);
    ASSERT(entry);
    ASSERT(entry->select);

    // we will need exclusive access to menu_display_info
    take_semaphore(menu_sem, 0);
    
    // if it doesn't seem to cycle, cancel earlier
    char first[MENU_MAX_VALUE_LEN];
    char last[MENU_MAX_VALUE_LEN];
    snprintf(first, sizeof(first), "%s", menu_get_str_value_from_script(name, entry_name));
    snprintf(last, sizeof(last), "%s", menu_get_str_value_from_script(name, entry_name));
    
    if (entry->icon_type == IT_ACTION)
        goto ok; // don't check actions
    
    if (strlen(first)==0)
        goto ok; // no value field, skip it
    
    for (int i = 0; i < 500; i++) // cycle until it returns to initial value
    {
        bmp_printf(FONT_MED, 0, 0, "%s->%s: %s (%s)                  ", name, entry_name, last, dir > 0 ? "+" : "-");

        // next value
        entry->select( entry->priv, dir);
        msleep(20); // we may need to wait for property handlers to update

        char* current = menu_get_str_value_from_script(name, entry_name);
        
        if (streq(current, last)) // value not changing? not good
        {
            printf("Value not changing: %s, %s -> %s (%s).\n", current, name, entry_name, dir > 0 ? "+" : "-");
            goto err;
        }
        
        if (streq(current, first)) // back to first value? success!
            goto ok;

        snprintf(last, sizeof(last), "%s", current);
    }
    printf("'Infinite' range: %s -> %s (%s)\n", name, entry_name, dir > 0 ? "+" : "-");

err:
    give_semaphore(menu_sem);
    return 0; // boo :(

ok:
    give_semaphore(menu_sem);
    return 1; // :)
}

void menu_check_wrap()
{
    int ok = 0;
    int bad = 0;
    struct menu * menu = menus;
    for( ; menu ; menu = menu->next )
    {
        struct menu_entry * entry = menu->children;
        for( ; entry ; entry = entry->next )
        {
            if (entry->shidden) continue;
            if (!entry->select) continue;
            
            int r = entry_check_wrap(menu->name, entry->name, 1);
            if (r) ok++; else bad++;

            r = entry_check_wrap(menu->name, entry->name, -1);
            if (r) ok++; else bad++;
            
            msleep(100);
        }
    }
    printf("Wrap test: %d OK, %d bad\n", ok, bad);
}

void menu_self_test()
{
    msleep(2000);
    console_show();
    menu_duplicate_test();
    printf("\n");
    menu_check_wrap();
}
#endif

static void srm_test_task()
{
    printf("SRM memory test...\n");
    msleep(2000);
    console_show();
    msleep(1000);
    
    /* let's see how much RAM we can get */
    struct memSuite * suite = srm_malloc_suite(0);
    struct memChunk * chunk = GetFirstChunkFromSuite(suite);
    printf("hSuite %x (%dx%s)\n", suite, suite->num_chunks, format_memory_size(chunk->size));
    
    printf("You should not be able to take pictures,\n");
    printf("but autofocus should work.\n");

    info_led_on();
    for (int i = 10; i >= 0; i--)
    {
        msleep(1000);
        printf("%d...", i);
    }
    printf("\b\b\n");
    info_led_off();
    
    srm_free_suite(suite);
    msleep(1000);

    printf("Now try taking some pictures during the test.\n");
    printf("It should work, and it should not crash.\n");
    msleep(5000);
    
    /* we must be able to allocate at least two 25MB buffers on top of what you can get from shoot_malloc */
    /* 50D/500D have 27M, 5D3 has 40 */
    for (int i = 0; i < 1000; i++)
    {
        void* buf1 = srm_malloc(25*1024*1024);
        printf("srm_malloc(25M) => %x\n", buf1);
        
        void* buf2 = srm_malloc(25*1024*1024);
        printf("srm_malloc(25M) => %x\n", buf2);

        /* we must be able to free them in any order, even if the backend doesn't allow that */
        if (rand()%2)
        {
            free(buf1);
            free(buf2);
        }
        else
        {
            free(buf2);
            free(buf1);
        }

        if (i == 0)
        {
            /* delay the first iteration, so you can see what's going on */
            /* also save a screenshot */
            msleep(5000);
            take_screenshot(0, SCREENSHOT_BMP);
        }
        
        if (!buf1 || !buf2)
        {
            /* allocation failed? wait before retrying */
            msleep(1000);
        }
    }
    
    printf("SRM memory test completed.\n");
    printf("Are you able to take pictures now? (you should)\n");
    msleep(5000);
    console_hide();
}

static void malloc_test_task()
{
    printf("Small-block malloc test...\n");
    msleep(2000);
    console_show();
    msleep(1000);

    /* allocate up to 50000 small blocks of RAM, 32K each */
    int N = 50000;
    int blocksize = 32*1024;
    void** ptr = malloc(N * sizeof(ptr[0]));
    if (ptr)
    {
        for (int i = 0; i < N; i++)
        {
            ptr[i] = 0;
        }

        for (int i = 0; i < N; i++)
        {
            ptr[i] = malloc(blocksize);
            printf("alloc %d %8x (total %s)\n", i, ptr[i], format_memory_size(i * blocksize));
            if (ptr[i]) memset(ptr[i], rand(), blocksize);
            else break;
        }
        
        msleep(2000);
        
        for (int i = 0; i < N; i++)
        {
            if (ptr[i])
            {
                printf("free %x\n", ptr[i]);
                free(ptr[i]);
                ptr[i] = 0;
            }
        }
    }
    free(ptr);

    printf("Small-block malloc test completed.\n\n");
    printf("You will see an error in the Debug menu,\n");
    printf("on the 'Free Memory' menu item. That's OK.\n");

    msleep(5000);
    console_hide();
}

static void memory_leak_test_task()
{
    printf("Memory leak test...\n");
    msleep(2000);
    console_show();
    msleep(1000);

    /* check for memory leaks */
    for (int i = 0; i < 1000; i++)
    {
        printf("%d/1000\n", i);
        
        /* with this large size, the backend will use fio_malloc, which returns uncacheable pointers */
        void* p = malloc(16*1024*1024 + 64);
        
        if (!p)
        {
            printf("malloc err\n");
            continue;
        }
        
        /* however, user code should not care about this; we have requested a plain old cacheable pointer; did we get one? */
        ASSERT(p == CACHEABLE(p));
        
        /* do something with our memory */
        memset(p, 1234, 1234);
        msleep(20);
        
        /* done, now free it */
        /* the backend should put back the uncacheable flag (if handled incorrectly, there may be memory leaks) */
        free(p);
        msleep(20);
    }
    
    /* if we managed to arrive here, the test ran successfully */
    printf("Memory leak test completed.\n");
    msleep(5000);
    console_hide();
}

static void edmac_test_task()
{
    msleep(2000);
    
    /* this test requires display on */
    
    if (!display_is_on())
    {
        fake_simple_button(BGMT_PLAY);
        msleep(1000);
    }
    
    if (!display_is_on())
    {
        beep();
        return;
    }

    uint8_t* real = bmp_vram_real();
    uint8_t* idle = bmp_vram_idle();
    int xPos = 0;
    int xOff = 2;
    int yPos = 0;

    edmac_memcpy_res_lock();
    edmac_copy_rectangle_adv(BMP_VRAM_START(idle), BMP_VRAM_START(real), 960, 120, 50, 960, 120, 50, 720, 440);
    while(true)
    {
        edmac_copy_rectangle_adv(BMP_VRAM_START(real), BMP_VRAM_START(idle), 960, 120, 50, 960, 120+xPos, 50+yPos, 720-xPos, 440-yPos);
        xPos += xOff;

        if(xPos >= 100 || xPos <= -100)
        {
            xOff *= -1;
        }
    }
    edmac_memcpy_res_unlock();
}

static struct menu_entry selftest_menu[] =
{
    {
        .name       = "Self tests",
        .select     = menu_open_submenu,
        .help       = "Tests to make sure Magic Lantern is stable and won't crash.",
        .submenu_width = 650,
        .children   = (struct menu_entry[]) {
            {
                .name       = "Stubs API test",
                .select     = run_in_separate_task,
                .priv       = stub_test_task,
                .help       = "Tests Canon functions called by ML. SET=once, PLAY=100x."
            },
            {
                .name       = "RPC reliability test (infinite)",
                .select     = run_in_separate_task,
                .priv       = rpc_test_task,
                .help       = "Flood master with RPC requests and print delay. ",
                .shidden    = 1,    /* 7D only */
            },
            {
                .name       = "Quick test (around 15 min)",
                .select     = run_in_separate_task,
                .priv       = stress_test_task,
                .help       = "A quick test which covers basic functionality. "
            },
            {
                .name       = "Random tests (infinite loop)",
                .select     = run_in_separate_task,
                .priv       = stress_test_random_task,
                .help       = "A thorough test which randomly enables functions from menu. "
            },
            {
                .name       = "Menu backend test (infinite)",
                .select     = run_in_separate_task,
                .priv       = stress_test_menu_dlg_api_task,
                .help       = "Tests proper usage of Canon API calls in ML menu backend."
            },
            {
                .name       = "Redraw test (infinite)",
                .select     = run_in_separate_task,
                .priv       = excessive_redraws_task,
                .help       = "Causes excessive redraws for testing the graphics backend",
            },
            {
                .name       = "Rectangle test (infinite)",
                .select     = run_in_separate_task,
                .priv       = bmp_fill_test_task,
                .help       = "Stresses graphics bandwith. Run this while recording.",
            },
            {
                .name       = "SRM memory test (5 minutes)",
                .select     = run_in_separate_task,
                .priv       = srm_test_task,
                .help       = "Tests SRM memory allocation routines.",
            },
            {
                .name       = "Small-block malloc test (quick)",
                .select     = run_in_separate_task,
                .priv       = malloc_test_task,
                .help       = "Allocate up to 50000 small blocks, 32K each, until memory gets full."
            },
            {
                .name       = "Memory leak test (1 minute)",
                .select     = run_in_separate_task,
                .priv       = memory_leak_test_task,
                .help       = "Allocate and free a large block of RAM (16 MB); repeat 1000 times."
            },
            {
                .name       = "EDMAC screen test (infinite)",
                .select     = run_in_separate_task,
                .priv       = edmac_test_task,
                .help       = "Shift the entire display left and right with EDMAC routines.",
                .help2      = "Fixme: this will lock up if you change the video mode during the test.",
            },
            MENU_EOL,
        }
    },
    #if 0
    {
        .name       = "Fault emulation...",
        .select     = menu_open_submenu,
        .help       = "Causes intentionally wrong behavior to see DryOS reaction.",
        .children   = (struct menu_entry[]) {
            {
                .name       = "Create a stuck task",
                .select     = run_in_separate_task,
                .priv       = frozen_task,
                .help       = "Creates a task which will become stuck in an infinite loop."
            },
            {
                .name       = "Freeze GUI task",
                .select     = freeze_gui_task,
                .help       = "Freezes main GUI task. Camera will stop reacting to buttons."
            },
            {
                .name       = "Division by zero",
                .select     = run_in_separate_task,
                .priv       = divzero_task,
                .help       = "Performs some math operations which will divide by zero."
            },
            {
                .name       = "Allocate 1MB of RAM",
                .select     = run_in_separate_task,
                .priv       = alloc_1M_task,
                .help       = "Allocates 1MB RAM from system memory, without freeing it."
            },
            MENU_EOL,
        }
    },
    #endif
};

static struct menu_entry * selftest_menu_entry(const char* entry_name)
{
    /* menu entries are not yet linked, so iterate as in array, not as in linked list */
    for(struct menu_entry * entry = selftest_menu[0].children ; !MENU_IS_EOL(entry) ; entry++ )
    {
        if (streq(entry->name, entry_name))
        {
            return entry;
        }
    }
    return 0;
}

/* fixme: move to core */
static void selftest_menu_show(const char* entry_name)
{
    struct menu_entry * entry = selftest_menu_entry(entry_name);
    if (entry)
    {
        entry->shidden = 0;
    }
    else
    {
        console_show();
        printf("Could not find '%s'\n", entry_name);
    }
}

static unsigned int selftest_init()
{
    BGMT_PLAY        = module_translate_key(MODULE_KEY_PLAY,        MODULE_KEY_CANON);
    BGMT_MENU        = module_translate_key(MODULE_KEY_MENU,        MODULE_KEY_CANON);
    BGMT_INFO        = module_translate_key(MODULE_KEY_INFO,        MODULE_KEY_CANON);
    BGMT_LV          = module_translate_key(MODULE_KEY_LV,          MODULE_KEY_CANON);
    BGMT_PRESS_SET   = module_translate_key(MODULE_KEY_PRESS_SET,   MODULE_KEY_CANON);
    BGMT_WHEEL_LEFT  = module_translate_key(MODULE_KEY_WHEEL_LEFT,  MODULE_KEY_CANON);
    BGMT_WHEEL_RIGHT = module_translate_key(MODULE_KEY_WHEEL_RIGHT, MODULE_KEY_CANON);
    BGMT_WHEEL_UP    = module_translate_key(MODULE_KEY_WHEEL_UP,    MODULE_KEY_CANON);
    BGMT_WHEEL_DOWN  = module_translate_key(MODULE_KEY_WHEEL_DOWN,  MODULE_KEY_CANON);
    
    menu_add("Debug", selftest_menu, COUNT(selftest_menu));
    
    if (is_camera("7D", "*"))
    {
        selftest_menu_show("RPC reliability test (infinite)");
    }
    
    return 0;
}

static unsigned int selftest_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(selftest_init)
    MODULE_DEINIT(selftest_deinit)
MODULE_INFO_END()
