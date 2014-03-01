/**
 * ADTG logging toolbox
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>

#include <gdb.h>
#include <cache_hacks.h>

static unsigned int log_abort = 0;
static unsigned int log_running = 0;

static unsigned int hook_calls = 0;
static unsigned int *adtg_buf = NULL;
static unsigned int adtg_buf_pos = 0;
static unsigned int adtg_buf_pos_max = 0;
static unsigned int adtg_current_vsync_pos = 0;
static unsigned int adtg_last_vsync_pos = 0;

static uint32_t ADTG_WRITE_FUNC = 0;
static uint32_t CMOS_WRITE_FUNC = 0;
static uint32_t CMOS2_WRITE_FUNC = 0;
static uint32_t CMOS16_WRITE_FUNC = 0;

static uint32_t nrzi_decode( uint32_t in_val )
{
    uint32_t val = 0;
    if (in_val & 0x8000)
        val |= 0x8000;
    for (int num = 0; num < 31; num++)
    {
        uint32_t old_bit = (val & 1<<(30-num+1)) >> 1;
        val |= old_bit ^ (in_val & 1<<(30-num));
    }
    return val;
}

unsigned short cmos_regs[8];
static int cmos_delta[8];

static void cmos_reg_update(unsigned short cmos_data)
{
    unsigned short reg = (cmos_data >> 12) & 0x07;
    unsigned short data = cmos_data & 0x0FFF;

    cmos_regs[reg] = data;
}

static void cmos_reg_manipulate(unsigned short *cmos_data)
{
    unsigned short reg = (*cmos_data >> 12) & 0x07;
    unsigned short data = *cmos_data & 0x0FFF;

    data += cmos_delta[reg];
    *cmos_data = (data & 0x0FFF) | (reg << 12);
}

static unsigned int adtg_log_vsync_cbr(unsigned int unused)
{
    if(!adtg_buf)
    {
        return;
    }
    
    if(adtg_buf_pos + 2 >= adtg_buf_pos_max)
    {
        return;
    }
    
    adtg_last_vsync_pos = adtg_current_vsync_pos;
    adtg_current_vsync_pos = adtg_buf_pos;
    
    adtg_buf[adtg_buf_pos] = 0xFFFFFFFF;
    adtg_buf_pos++;    
    adtg_buf[adtg_buf_pos] = 0xFFFFFFFF;
    adtg_buf_pos++;
}

static void adtg_log(breakpoint_t *bkpt)
{
    if(!adtg_buf)
    {
        return;
    }
    
    unsigned int cs = bkpt->ctx[0];
    unsigned int *data_buf = bkpt->ctx[1];
    
    void* buf0 = data_buf;
    
    /* log all ADTG writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        if(adtg_buf_pos + 2 >= adtg_buf_pos_max)
        {
            return;
        }

/* shutter override
        uint32_t dat = *data_buf;
        int reg = dat >> 16;
        int val = dat & 0xFFFF;
        if (reg == 0x8060)
        {
            *data_buf = 0x80600479;
            NotifyBox(1000, "%x ", buf0);
        }
*/
        
        adtg_buf[adtg_buf_pos] = cs;
        adtg_buf_pos++;
        adtg_buf[adtg_buf_pos] = *data_buf;
        adtg_buf_pos++;

        data_buf++;
    }
}

static void cmos_log(breakpoint_t *bkpt)
{
    static int loops = 0;
    
    loops++;
    loops &= 0xFFF;
    
    if(!adtg_buf)
    {
        return;
    }
    
    unsigned short *data_buf = bkpt->ctx[0];
    
    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        if(adtg_buf_pos + 2 >= adtg_buf_pos_max)
        {
            return;
        }
        adtg_buf[adtg_buf_pos] = 0x00FF0000;
        adtg_buf_pos++;
        adtg_buf[adtg_buf_pos] = *data_buf;
        adtg_buf_pos++;
        
        cmos_reg_update(*data_buf);
        cmos_reg_manipulate(data_buf);
        
        data_buf++;
    }
}


static void cmos16_log(breakpoint_t *bkpt)
{
    if(!adtg_buf)
    {
        return;
    }
    
    hook_calls++;
    
    unsigned short *data_buf = bkpt->ctx[0];
    
    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        if(adtg_buf_pos + 2 >= adtg_buf_pos_max)
        {
            return;
        }
        adtg_buf[adtg_buf_pos] = 0xFF000000;
        adtg_buf_pos++;
        adtg_buf[adtg_buf_pos] = *data_buf;
        adtg_buf_pos++;
        
        cmos_reg_update(*data_buf);
        cmos_reg_manipulate(data_buf);
        data_buf++;
    }
}

void adtg_log_task()
{
    adtg_buf_pos_max = 128 * 1024;
    adtg_buf = shoot_malloc(adtg_buf_pos_max * 4 + 0x100);
    
    if(!adtg_buf)
    {   
        return;
    }
    
    log_running++;
    
    /* set watchpoints at ADTG and CMOS writes */
    gdb_setup();
    breakpoint_t *bkpt1 = gdb_add_watchpoint(ADTG_WRITE_FUNC, 0, &adtg_log);
    breakpoint_t *bkpt2 = gdb_add_watchpoint(CMOS_WRITE_FUNC, 0, &cmos_log);
    breakpoint_t *bkpt3 = gdb_add_watchpoint(CMOS16_WRITE_FUNC, 0, &cmos16_log);
    
    /* wait for buffer being filled */
    while(adtg_buf_pos + 2 < adtg_buf_pos_max)
    {
        bmp_printf(FONT_MED, 10, 20, "cmos:");
        for(int pos = 0; pos < 8; pos++)
        {
            bmp_printf(FONT_MED, 10, 40 + pos * font_med.height, "[%d] 0x%03X ", pos, cmos_regs[pos]);
        }

        int x = 200;
        int y = 30;
        bmp_printf(FONT_MED, x, y-10, "adtg:");
        for (int pos = adtg_last_vsync_pos + 2; pos < adtg_current_vsync_pos; pos += 2)
        {
            uint32_t dst = adtg_buf[pos];
            uint32_t dat = adtg_buf[pos+1];
            if (dst == 0xFF000000) /* CMOS16 */
            {
            }
            else if (dst == 0x00FF0000) /* CMOS */
            {
            }
            else
            {
                int reg = dat >> 16;
                int val = dat & 0xFFFF;
                bmp_printf(FONT_SMALL, x, y += font_small.height,
                    "[ADTG%d] %04X -> %04X (%X)", dst & 0x0F, reg, val, nrzi_decode(val)
                );
                if (y > 400)
                {
                    y = 30;
                    x += 250;
                }
            }
        }
        msleep(100);
        
        if(log_abort)
        {
            break;
        }
    }
    
    beep();
    
    /* uninstall watchpoints */
    gdb_delete_bkpt(bkpt1);
    gdb_delete_bkpt(bkpt2);
    gdb_delete_bkpt(bkpt3);
    
    /* dump all stuff */
    char filename[100];
    snprintf(filename, sizeof(filename), "%s/adtg.bin", get_dcim_dir());
    FILE* f = FIO_CreateFileEx(filename);
    FIO_WriteFile(f, adtg_buf, adtg_buf_pos * 4); 
    FIO_CloseFile(f);
    
    /* free buffer */
    void *buf = adtg_buf;
    adtg_buf = NULL;
    shoot_free(buf);
    
    log_running--;
}

static MENU_SELECT_FUNC(adtg_log_toggle)
{
    if(!adtg_buf)
    {
        task_create("adtg_log_task", 0x1e, 0x1000, adtg_log_task, (void*)0);
    }
    else
    {
        adtg_buf_pos_max = adtg_buf_pos;
    }
}


static struct menu_entry adtg_log_menu[] =
{
    {
        .name = "ADTG Logging",
        .select = &adtg_log_toggle,
        .priv = &adtg_buf,
        .max = 1,
        .help = "Log ADTG writes",
        .children =  (struct menu_entry[]) {
            {
                .name = "cmos[0]",
                .priv = cmos_delta,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[1]",
                .priv = cmos_delta+1,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[2]",
                .priv = cmos_delta+2,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[3]",
                .priv = cmos_delta+3,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[4]",
                .priv = cmos_delta+4,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[5]",
                .priv = cmos_delta+5,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[6]",
                .priv = cmos_delta+6,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            {
                .name = "cmos[7]",
                .priv = cmos_delta+7,
                .edit_mode = EM_MANY_VALUES_LV,
                .min = -1000,
                .max = 1000,
            },
            MENU_EOL,
        },
    }
};

static unsigned int adtg_log_init()
{
    if (is_camera("5D3", "1.1.3"))
    {
        ADTG_WRITE_FUNC = 0x11644;
        CMOS_WRITE_FUNC = 0x119CC;
        CMOS2_WRITE_FUNC = 0x11784;
        CMOS16_WRITE_FUNC = 0x11AB8;
    }
    else if (is_camera("5D3", "1.2.3"))
    {
        ADTG_WRITE_FUNC = 0x11644;    // 0xFFA01E04 - RAM_OFFSET
        CMOS_WRITE_FUNC = 0x119CC;    // 0xFFA0218C - RAM_OFFSET
        CMOS2_WRITE_FUNC = 0x11784;   // 0xFFA01F44 - RAM_OFFSET
        CMOS16_WRITE_FUNC = 0x11AB8;  // 0xFFA02278 - RAM_OFFSET
    }
    else if (is_camera("5D2", "2.1.2"))
    {
        ADTG_WRITE_FUNC = 0xffa35cbc;
        CMOS_WRITE_FUNC = 0xffa35e70;
    }
    else if (is_camera("500D", "1.1.1")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg70325#msg70325
    {
        ADTG_WRITE_FUNC = 0xFF22F8F4; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0xFF22F9DC; //"[REG] ############ Start CMOS"
    }
    else if (is_camera("550D", "1.0.9")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg61551#msg61551
    {
        ADTG_WRITE_FUNC = 0xff27ee34;
        CMOS_WRITE_FUNC = 0xff27f028;
    }
    else if (is_camera("60D", "1.1.1")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg69719#msg69719
    {
        ADTG_WRITE_FUNC = 0xFF2C9788; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0xFF2C997C; //"[REG] ############ Start CMOS"
    }
    else if (is_camera("50D", "1.0.9")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg63322#msg63322
    {
        ADTG_WRITE_FUNC = 0xFFA11FDC;
        CMOS_WRITE_FUNC = 0xFFA12190;
    }
    else if (is_camera("6D", "1.1.3")) // from 1%
    {
        CMOS_WRITE_FUNC = 0x2445C; //"[REG] ############ Start CMOS OC_KICK"
        CMOS2_WRITE_FUNC = 0x2420C; //"[REG] ############ Start CMOS"
        ADTG_WRITE_FUNC = 0x24108; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS16_WRITE_FUNC = 0x24548; //"[REG] ############ Start CMOS16 OC_KICK"
    }
    else if (is_camera("EOSM", "2.0.2")) // from 1%
    {
        ADTG_WRITE_FUNC = 0x2986C;
        CMOS_WRITE_FUNC = 0x2998C;
    }
    else if (is_camera("600D", "1.0.2")) // from 1% TL 2.0
    {
        ADTG_WRITE_FUNC = 0xFF2DCEF4; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0xFF2DD0E8; //"[REG] ############ Start CMOS"
    }
    else if (is_camera("650D", "1.0.4"))
    {
        ADTG_WRITE_FUNC = 0x178FC; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0x17A1C; //"[REG] ############ Start CMOS"
    }
    else if (is_camera("700D", "1.1.1"))
    {
        ADTG_WRITE_FUNC = 0x178FC; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0x17A1C; //"[REG] ############ Start CMOS"
    }    
    else return CBR_RET_ERROR;

    
    menu_add("Movie", adtg_log_menu, COUNT(adtg_log_menu));
    return 0;
}

static unsigned int adtg_log_deinit()
{
    menu_remove("Movie", adtg_log_menu, COUNT(adtg_log_menu));
    
    while(log_running)
    {
        log_abort = 1;
        msleep(100);
    }
    
    return 0;
}



MODULE_INFO_START()
    MODULE_INIT(adtg_log_init)
    MODULE_DEINIT(adtg_log_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_VSYNC, adtg_log_vsync_cbr, 0)
MODULE_CBRS_END()
