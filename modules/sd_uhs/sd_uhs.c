/**
 * Experimental SD UHS overclocking.
 */

#include <module.h>
#include <dryos.h>
#include <patch.h>
#include <console.h>

/* all log_printf's are logged to file */
static char * log_buf = NULL;
static int log_buf_size = 0;
static int log_len = 0;

extern void console_puts(const char * str);

static int log_printf(const char* fmt, ...)
{
    int len = 0;

    if (log_buf && log_buf_size)
    {
        char * msg = log_buf + log_len;
        va_list ap;
        va_start(ap, fmt);
        len = vsnprintf(msg, log_buf_size - log_len - 1, fmt, ap);
        va_end(ap);
        console_puts(msg);
    }

    log_len += len;
    return len;
}

/* camera-specific parameters */
static uint32_t sd_enable_18V = 0;
static uint32_t sd_setup_mode = 0;
static uint32_t sd_setup_mode_in = 0;
static uint32_t sd_setup_mode_reg = 0xFFFFFFFF;
static uint32_t sd_set_function = 0;
static int (*SD_ReConfiguration)() = 0;

static uint32_t uhs_regs[]     = { 0xC0400600, 0xC0400604,/*C0400608, C040060C*/0xC0400610, 0xC0400614, 0xC0400618, 0xC0400624, 0xC0400628, 0xC040061C, 0xC0400620 /*, drv strength */ };   /* register addresses */
static uint32_t sdr50_700D[]   = {        0x3,        0x3,                             0x4, 0x1D000301,        0x0,      0x201,      0x201,      0x100,        0x4,               0    };   /* SDR50 values from 700D (96MHz) */
static uint32_t sdr_80MHz[]    = {        0x3,        0x3,                             0x5, 0x1D000401,        0x0,      0x201,      0x201,      0x100,        0x5,               0    };   /* underclocked values: 80MHz = 96*(4+1)/(5+1) */
static uint32_t sdr_120MHz[]   = {        0x3,        0x3,                             0x3, 0x1D000201,        0x0,      0x201,      0x201,      0x100,        0x3,               0    };   /* overclocked values: 120MHz = 96*(4+1)/(3+1) */
static uint32_t sdr_132MHz[]   = {        0x2,        0x2,                             0x2, 0x1D000201,        0x0,      0x100,      0x100,      0x100,        0x2,               0    };   /* overclocked values: 132MHz?! (found by brute-forcing) */
static uint32_t sdr_160MHz[]   = {        0x2,        0x3,                             0x1, 0x1D000001,        0x0,      0x100,      0x100,      0x100,        0x1,               0    };   /* overclocked values: 160MHz = 96*(4+1)/(2?+1) (found by brute-forcing) */
static uint32_t sd_tweakable[] = {        0x7,        0x7,                             0x7, 0x00000700,        0x0,      0x300,      0x300,        0x0,        0x7,               0    };   /* what can be tweaked during brute-forcing? (bit mask) */
// 5D3 mode 0                                                                          17F  0x1D004101           0        403F        403F          7F          7F 
// 5D3 mode 1 16MHz                         3           3          1          1         1D  0x1D001001           0       0xF0E       0xF0E          1D          1D
// 5D3 mode 2 24MHz                         3           3          1          1         13  0x1D000B01           0       0xA09       0xA09          13          13
// 5D3 mode 3 48MHz                         3           3          1          1          9  0x1D000601           0       0x504       0x504       0x100           9
// 700D mode 4 96MHz SDR50                  3           3          1          1          4  0x1D000301           0       0x201       0x201       0x100           4
// 5D3 mode 5 serial flash?                 3                                            7  0x1D000501           0       0x403       0x403       0x403           7
// 5D3 mode 6 (bench 5.5MB/s)               7                                           13  0x1D000B01           0       0xA09       0xA09          13          13

/* after registers, we may have some other parameters */
#define DRV_STRENGTH_IDX COUNT(uhs_regs)

static uint32_t uhs_vals[COUNT(uhs_regs) + 1];          /* current values */
static uint32_t uhs_best_vals[COUNT(uhs_regs) + 1];     /* best values */
static uint32_t uhs_backup_vals[COUNT(uhs_regs) + 1];   /* backup values (for error recovery) */
static int uhs_best_time = INT_MAX;

static int sd_setup_mode_enable = 0;

/* start of the function */
static void sd_setup_mode_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode(dev=%x)\n", regs[0]);

    /* this function is also used for other interfaces, such as serial flash */
    /* only enable overriding when called with dev=1 */
    sd_setup_mode_enable = (regs[0] == 1);
}

/* called right before the case switch in sd_setup_mode (not at the start of the function!) */
static void sd_setup_mode_in_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_setup_mode switch(mode=%x) en=%d\n", regs[sd_setup_mode_reg], sd_setup_mode_enable);

    if (sd_setup_mode_enable && regs[sd_setup_mode_reg] == 4)   /* SDR50? */
    {
        /* set our register overrides */
        for (int i = 0; i < COUNT(uhs_regs); i++)
        {
            MEM(uhs_regs[i]) = uhs_vals[i];
        }

        /* set some invalid mode to bypass the case switch
         * and keep our register values only */
        regs[sd_setup_mode_reg] = 0x13;
    }
}

static void sd_set_function_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    qprintf("sd_set_function(0x%x)\n", regs[0]);

    /* UHS-I SDR50? */
    if (regs[0] == 0xff0002)
    {
        /* force UHS-I SDR104 */
        /* also set the driver strength */
        /* caveat: I don't know what I'm doing */
        uint32_t drv_strength = uhs_vals[DRV_STRENGTH_IDX] & 0x3;
        uint32_t access_mode = 3;   /* SDR104 */
        uint32_t power_limit = 0;   /* default; sounds risky */
        log_printf("D%x ", drv_strength);
        regs[0] = 0xff0000 |
                  (access_mode  << 0) |
                  (drv_strength << 8) |
                  (power_limit << 12) ;
    }
}

const int buffer_size = 16 * 1024 * 1024;

static int test_fio()
{
    void * buf = fio_malloc(buffer_size);
    memset(buf, 0x5A, buffer_size);

    log_printf(" W:");
    int64_t t0 = get_us_clock();

    FILE * f = FIO_CreateFile("B:/test.tmp");
    if (!f)
    {
        log_printf("ERC");
        return -1;
    }

    int64_t t1 = get_us_clock();
    int r = FIO_WriteFile(f, buf, buffer_size);
    if (r != buffer_size)
    {
        log_printf("ERW");
        return -1;
    }
    int64_t t2 = get_us_clock();
    FIO_CloseFile(f);

    memset(buf, 0x77, buffer_size);

    f = FIO_OpenFile("B:/test.tmp", O_RDONLY | O_SYNC);
    if (!f)
    {
        log_printf("ERO");
        return -1;
    }

    log_printf("%s/s ", format_memory_size((uint64_t) buffer_size * 1000000ull / (t2 - t1)));
    log_printf("R:");

    int64_t t3 = get_us_clock();
    r = FIO_ReadFile(f, buf, buffer_size);
    if (r != buffer_size)
    {
        log_printf("ERR");
        return -1;
    }
    int64_t t4 = get_us_clock();
    FIO_CloseFile(f);
    int64_t t5 = get_us_clock();

    log_printf("%s/s ", format_memory_size((uint64_t) buffer_size * 1000000ull / (t4 - t3)));

    for (int i = 0; i < buffer_size/4; i++)
    {
        if (((uint32_t *)buf)[i] != 0x5A5A5A5A)
        {
            log_printf("Err!!!");
            break;
        }
    }

    free(buf);

    return (int)(t5 - t0);
}

struct cf_device
{
    /* type b always reads from raw sectors */
    int (*read_block)(
        struct cf_device * dev,
        void * buf,
        uintptr_t block,
        size_t num_blocks
    );

    int (*write_block)(
        struct cf_device * dev,
        const void * buf,
        uintptr_t block,
        size_t num_blocks
    );
};

static void sd_reset(struct cf_device * const dev)
{
    log_printf(" [SAFE] ");

    /* back to some safe values */
    memcpy(uhs_backup_vals, uhs_vals, sizeof(uhs_backup_vals));
    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));

    /* clear error flag to allow activity after something went wrong */
    MEM((uintptr_t)dev + 80) = 0;

    /* re-initialize card */
    SD_ReConfiguration();
}

static void sd_restore(struct cf_device * const dev)
{
    log_printf(" [BACK] ");

    /* restore old values */
    memcpy(uhs_vals, uhs_backup_vals, sizeof(uhs_vals));

    /* clear error flag to allow activity after something went wrong */
    MEM((uintptr_t)dev + 80) = 0;

    /* re-initialize card */
    SD_ReConfiguration();
}

static int test_lo()
{
    int start = 1024 * 1024 * 1024 / 512;
    void * buf = fio_malloc(buffer_size);
    if (!buf)
    {
        log_printf("malloc error\n");
        return -1;
    }

    extern struct cf_device * const sd_device[];
    struct cf_device * const dev = (struct cf_device *) sd_device[1];
    if (!dev)
    {
        log_printf("SD dev error\n");
        return -1;
    }

    log_printf("r:");

    int64_t t0 = get_us_clock();

    if (dev->read_block(dev, buf, start, buffer_size / 512) != buffer_size / 512)
    {
        log_printf("err");

        sd_reset(dev);

        if (dev->read_block(dev, buf, start, buffer_size / 512) != buffer_size / 512)
        {
            log_printf("!!!");
        }

        sd_restore(dev);

        free(buf);
        return -1;
    }

    int64_t t1 = get_us_clock();

    log_printf("%s/s ", format_memory_size((uint64_t) buffer_size * 1000000ull / (t1 - t0)));
    log_printf("w:");

    int64_t t2 = get_us_clock();

    if (dev->write_block(dev, buf, start, buffer_size / 512) != buffer_size / 512)
    {
        log_printf("ERR");

        sd_reset(dev);

        if (dev->write_block(dev, buf, start, buffer_size / 512) != buffer_size / 512)
        {
            log_printf("!!!");
        }

        sd_restore(dev);

        free(buf);
        return -1;
    }

    int64_t t3 = get_us_clock();

    log_printf("%s/s ", format_memory_size((uint64_t) buffer_size * 1000000ull / (t3 - t2)));

    free(buf);
    return (int)(t3 - t2);
}

static void test()
{
    /* power-cycle and reconfigure the SD card */
    /* for some reason, results seem a bit more consistent if resetting twice (why?) */
    /* FIXME: not thread-safe! */
    SD_ReConfiguration();
    SD_ReConfiguration();

    /* low-level read-write test */
    int t = test_lo();
    if (t <= 0)
    {
        log_printf("\n");
        return;
    }

    /* high-level FIO read-write test */
    int t2 = test_fio();
    if (t2 <= 0)
    {
        log_printf("\n");
        return;
    }

    /* seems OK */
    if (t < uhs_best_time)
    {
        log_printf((t < uhs_best_time * 9 / 10) ? " 8) " : " :) ");
        uhs_best_time = t;
        memcpy(uhs_best_vals, uhs_vals, sizeof(uhs_best_vals));
    }
    else
    {
        log_printf((t < uhs_best_time * 11 / 10) ? " ::)" : (t < uhs_best_time * 20 / 10) ? " meh" : " ???");
    }

    /* test_lo returns write time in microseconds */
    log_printf(" [best %s/s]\n", format_memory_size((uint64_t) buffer_size * 1000000ull / (uhs_best_time)));
}

static int alter_byte(uint32_t pos, int delta, int force)
{
    uint8_t * vals8 = (uint8_t *) uhs_vals;
    uint8_t * twak8 = (uint8_t *) sd_tweakable;

    if (pos < COUNT(uhs_vals) * 4 && (force || twak8[pos]))   /* is this parameter tweakable? */
    {
        uint8_t old = vals8[pos];
        vals8[pos] += delta;
        log_printf("[%d:%x-%x] ", pos, old, vals8[pos]);
        return 1;
    }
    return 0;
}

static int alter_2_bytes(uint32_t pos1, uint32_t pos2, int delta)
{
    uint8_t * vals8 = (uint8_t *) uhs_vals;
    uint8_t * twak8 = (uint8_t *) sd_tweakable;

    if (pos1 < COUNT(uhs_vals) * 4 && twak8[pos1] &&
        pos2 < COUNT(uhs_vals) * 4 && twak8[pos2]) /* are these parameters tweakable? */
    {
        uint8_t old1 = vals8[pos1];
        uint8_t old2 = vals8[pos2];
        vals8[pos1] += delta;
        vals8[pos2] += delta;
        log_printf("[%d,%d] %x,%x -> %x,%x: ", pos1, pos2, old1, old2, vals8[pos1], vals8[pos2]);
        return 1;
    }
    return 0;
}

static int alter_bit(uint32_t pos, int force)
{
    uint8_t * vals8 = (uint8_t *) uhs_vals;
    uint8_t * twak8 = (uint8_t *) sd_tweakable;
    uint8_t bit = (1 << (pos % 8));
    uint32_t i = pos / 8;

    if (i < COUNT(uhs_vals) * 4 && (force || (twak8[i] & bit)))   /* is this parameter tweakable? */
    {
        uint8_t old = vals8[i];
        vals8[i] ^= bit;
        log_printf("[%d:%x-%x] ", i, old, vals8[i]);
        return 1;
    }
    return 0;
}

static void sd_overclock_task()
{
    msleep(2000);
    console_clear();
    console_show();
    msleep(1000);

    /* init logging */
    ASSERT(log_buf == NULL);
    log_buf = fio_malloc(1024 * 1024);
    if (!log_buf)
    {
        printf("malloc error\n");
        return;
    }
    log_buf_size = 1024 * 1024;
    log_len = 0;

    struct tm now;
    LoadCalendarFromRTC( &now );

    log_printf("\n");
    log_printf("===================\n");
    log_printf("%4d/%02d/%02d %02d:%02d:%02d\n",
        now.tm_year + 1900,
        now.tm_mon + 1,
        now.tm_mday,
        now.tm_hour,
        now.tm_min,
        now.tm_sec
    );
    log_printf("===================\n");

    /* benchmark without the hack */
    log_printf("Before the hack: "); test();

    /* install the hack */
    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));
    if (sd_enable_18V)
    {
        patch_instruction(sd_enable_18V, 0xe3a00000, 0xe3a00001, "SD 1.8V");
    }
    patch_hook_function(sd_setup_mode, MEM(sd_setup_mode), sd_setup_mode_log, "SD UHS");
    patch_hook_function(sd_setup_mode_in, MEM(sd_setup_mode_in), sd_setup_mode_in_log, "SD UHS");

    /* test some presets */
    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));
    log_printf("SDR50 @ 96MHz  : "); test();
    log_printf("SDR50 @ 96MHz  : "); test();

    memcpy(uhs_vals, sdr_80MHz, sizeof(uhs_vals));
    log_printf("SDR50 @ 80MHz  : "); test();
    log_printf("SDR50 @ 80MHz  : "); test();

    memcpy(uhs_vals, sdr_120MHz, sizeof(uhs_vals));
    log_printf("SDR50 @ 120MHz : "); test();
    log_printf("SDR50 @ 120MHz : "); test();

    /* enable SDR104 */
    patch_hook_function(sd_set_function, MEM(sd_set_function), sd_set_function_log, "SDR104");

    memcpy(uhs_vals, sdr50_700D, sizeof(uhs_vals));
    uhs_vals[DRV_STRENGTH_IDX] = 0; log_printf("SDR104 @ 96MHz : "); test();
    uhs_vals[DRV_STRENGTH_IDX] = 1; log_printf("SDR104 @ 96MHz : "); test();

    memcpy(uhs_vals, sdr_80MHz, sizeof(uhs_vals));
    uhs_vals[DRV_STRENGTH_IDX] = 0; log_printf("SDR104 @ 80MHz : "); test();
    uhs_vals[DRV_STRENGTH_IDX] = 1; log_printf("SDR104 @ 80MHz : "); test();

    memcpy(uhs_vals, sdr_120MHz, sizeof(uhs_vals));
    uhs_vals[DRV_STRENGTH_IDX] = 0; log_printf("SDR104 @ 120MHz: "); test();
    uhs_vals[DRV_STRENGTH_IDX] = 1; log_printf("SDR104 @ 120MHz: "); test();

    memcpy(uhs_vals, sdr_132MHz, sizeof(uhs_vals));
    uhs_vals[DRV_STRENGTH_IDX] = 0; log_printf("SDR104 @ 132MHz: "); test();
    uhs_vals[DRV_STRENGTH_IDX] = 1; log_printf("SDR104 @ 132MHz: "); test();

    memcpy(uhs_vals, sdr_160MHz, sizeof(uhs_vals));
    for (int d = 0; d < 16; d++)
    {
        uhs_vals[DRV_STRENGTH_IDX] = d & 3;
        log_printf("SDR104 @ 160MHz: "); test();
    }

    if (!get_halfshutter_pressed())
    {
        /* hold half-shutter to start brute-forcing */
        goto end;
    }

    /* start optimizing from this configuration */
    /* using "uhs_best_vals" will always tweak the best config found so far,
     * otherwise it will tweak some fixed preset */
    uint8_t * uhs_ref_vals = (uint8_t *) uhs_best_vals;

    log_printf("Trying all bytes +1...\n");
    memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
    for (int pos = 0; pos < COUNT(uhs_vals) * 4; pos++)
    {
        if (alter_byte(pos, 1, 0)) test();
    }

    log_printf("Trying all bytes -1...\n");
    memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
    for (int pos = 0; pos < COUNT(uhs_vals) * 4; pos++)
    {
        if (alter_byte(pos, -1, 0)) test();
    }

    log_printf("Trying one byte...\n");
    for (int pos = 0; pos < COUNT(uhs_vals) * 4; pos++)
    {
        memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
        if (alter_byte(pos, 1, 1)) test();
        memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
        if (alter_byte(pos, -1, 1)) test();
    }

    log_printf("Trying two bytes...\n");
    for (int pos1 = 0; pos1 < COUNT(uhs_vals) * 4; pos1++)
    {
        for (int pos2 = pos1 + 1; pos2 < COUNT(uhs_vals) * 4; pos2++)
        {
            memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
            if (alter_2_bytes(pos1, pos2, 1)) test();
            memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
            if (alter_2_bytes(pos1, pos2, -1)) test();
        }
    }

    log_printf("Trying random... (half-shutter to stop)\n");

    /* stop random poking on half-shutter press */
    while (!get_halfshutter_pressed())
    {
        for (int k = 0; k < 10; k++)
        {
            /* flip random bits (tweakable only) */
            memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
            /* how many bits to flip? (not too many) */
            int n = (rand() % COUNT(uhs_vals)) + 1;
            for (int i = 0; i < n; i++)
            {
                while (!alter_bit(rand() % (COUNT(uhs_vals) * 32), 0));
            }
            test();
            if (get_halfshutter_pressed()) break;
        }

        if (1)
        {
            /* flip random bits (try all) */
            memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
            int n = (rand() % COUNT(uhs_vals)) + 1;
            for (int i = 0; i < n; i++)
            {
                while (!alter_bit(rand() % (COUNT(uhs_vals) * 32), 1));
            }
            test();
        }

        if (1)
        {
            /* random bytes +/- 1 */
            memcpy(uhs_vals, uhs_ref_vals, sizeof(uhs_vals));
            int n = (rand() % COUNT(uhs_vals) * 4) + 1;
            for (int i = 0; i < n; i++)
            {
                while (!alter_byte(rand() % (COUNT(uhs_vals) * 4), rand() % 2 ? 1 : -1, 1));
            }
            test();
        }
    }

    log_printf("Best parameters: \n");
    for (int i = 0; i < COUNT(uhs_regs); i++)
    {
        log_printf("[%X]: %X\n", uhs_regs[i], uhs_best_vals[i]);
    }
    log_printf("Driver strength: %X\n", uhs_best_vals[DRV_STRENGTH_IDX]);

end:
    memcpy(uhs_vals, uhs_best_vals, sizeof(uhs_vals));
    log_printf("Best: "); test();
    log_printf("Best: "); test();
    log_printf("\n");
    log_printf("Done.\n");
    log_printf("Please run THOROUGH tests before using!!!");
    log_printf("\n");

    /* save log file */
    ASSERT(log_buf && log_buf_size && log_len);
    FILE * log = FIO_CreateFileOrAppend("ML/LOGS/SD_UHS.LOG");
    FIO_WriteFile(log, log_buf, log_len);
    FIO_CloseFile(log);

    /* cleanup */
    free(log_buf);
    log_buf = NULL;
    log_buf_size = 0;
    log_len = 0;

    printf("Press half-shutter to close.\n");
    while (!get_halfshutter_pressed())
    {
        msleep(10);
    }
    console_hide();
}

static MENU_UPDATE_FUNC(sd_overclock_warn)
{
    MENU_SET_VALUE("MAY CAUSE DATA LOSS");
    MENU_SET_WARNING(MENU_WARN_ADVICE, entry->help2);
}

static struct menu_entry sd_uhs_menu[] =
{
    {
        .name   = "SD overclock", 
        .priv   = &sd_overclock_task, 
        .select = run_in_separate_task,
        .update = sd_overclock_warn,
        .help   = "Run this ONLY if you don't mind playing Russian Roulette with your data.",
        .help2  = "No other file I/O should run during the test. You have been warned.",
    }
};

static unsigned int sd_uhs_init()
{
    if (is_camera("5D3", "1.1.3"))
    {
        /* sd_setup_mode:
         * sdSendCommand: CMD%d  Retry=... -> 
         * sd_configure_device(1) (called after a function without args) ->
         * sd_setup_mode(dev) if dev is 1 or 2 ->
         * logging hooks are placed both at start of sd_setup_mode and before the case switch
         */
        sd_setup_mode       = 0xFF47B4C0;   /* start of the function; not strictly needed on 5D3 */
        sd_setup_mode_in    = 0xFF47B4EC;   /* after loading sd_mode in R0, before the switch */
        sd_setup_mode_reg   = 0;            /* switch variable is in R0 */
        sd_set_function     = 0xFF6ADE34;   /* sdSetFunction */
        sd_enable_18V       = 0xFF47B4B8;   /* 5D3 only (Set 1.8V Signaling) */
        SD_ReConfiguration  = (void *) 0xFF6AFF1C;
    }

    if (is_camera("5D3", "1.2.3"))
    {
        sd_setup_mode       = 0xFF484474;
        sd_setup_mode_in    = 0xFF4844A0;
        sd_setup_mode_reg   = 0;
        sd_set_function     = 0xFF6B8FD0;
        sd_enable_18V       = 0xFF48446C;   /* 5D3 only (Set 1.8V Signaling) */
        SD_ReConfiguration  = (void *) 0xFF6BB0B8;
    }

    if (is_camera("700D", "1.1.5"))
    {
        sd_setup_mode       = 0xFF3376E8;   /* start of the function */
        sd_setup_mode_in    = 0xFF337770;   /* right before the switch */
        sd_setup_mode_reg   = 1;            /* switch variable is in R1 (likely all D5 other than 5D3) */
        sd_set_function     = 0xFF748F18;
        SD_ReConfiguration  = (void *) 0xFF74B35C;
    }

    if (is_camera("6D", "1.1.6"))
    {
        sd_setup_mode       = 0xFF325A20;
        sd_setup_mode_in    = 0xFF325AA8;
        sd_setup_mode_reg   = 1;            /* switch variable is in R1 (likely all D5 other than 5D3) */
        sd_set_function     = 0xFF78F308;
        SD_ReConfiguration  = (void *) 0xFF791408;
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        sd_setup_mode       = 0xFF338D40;
        sd_setup_mode_in    = 0xFF338DC8;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF63EF60;
        SD_ReConfiguration  = (void *) 0xFF641314;
    }

    if (is_camera("100D", "1.0.1"))
    {
        sd_setup_mode       = 0xFF3355B0;
        sd_setup_mode_in    = 0xFF335648;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF6530A4;
        SD_ReConfiguration  = (void *) 0xFF655458;
    }

    if (is_camera("650D", "1.0.4"))
    {
        sd_setup_mode       = 0xFF334C4C;
        sd_setup_mode_in    = 0xFF334CD4;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF73FD20;
        SD_ReConfiguration  = (void *) 0xFF7420D4;
    }

    if (is_camera("70D", "1.1.2"))
    {
        sd_setup_mode       = 0xFF33E078;
        sd_setup_mode_in    = 0xFF33E100;
        sd_setup_mode_reg   = 1;
        sd_set_function     = 0xFF7CE4B8;
        SD_ReConfiguration  = (void *) 0xFF7D086C;
    }

    if (sd_setup_mode && sd_setup_mode_in &&
        sd_setup_mode_reg != 0xFFFFFFFF &&
        sd_set_function && SD_ReConfiguration)
    {
        /* stubs present? create the menu */
        menu_add("Debug", sd_uhs_menu, COUNT(sd_uhs_menu));
    }
    else
    {
        NotifyBox(2000, "FIXME: unsupported model.");
    }

    return 0;
}

static unsigned int sd_uhs_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(sd_uhs_init)
    MODULE_DEINIT(sd_uhs_deinit)
MODULE_INFO_END()
