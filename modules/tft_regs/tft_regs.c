/**
 * Display SIO registers
 * 
 * Reverse engineering tool.
 */

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <shoot.h>

/* stubs (model-specific) */
static void (*tft_sio_init)() = (void *) 0;
static void (*tft_sio_write)(uint32_t * data, int size) = (void *) 0;
static void (*tft_sio_finish)(void * sio_obj) = (void *) 0;
static void ** p_tft_sio_obj = (void **) 0;

/* menu vars */
/* changes are not persistent */
static int current_reg = 0;
static int reg_values[256] = {0};
static int reg_before[256] = {0};

/* send a SIO command sequence to TFT controller */
static void tft_command(uint32_t * buffer, int size)
{
    /* there is a call to take_semaphore in tft_sio_init and give_semaphore in tft_sio_finish */
    /* so it's probably thread-safe (todo: test) */
    tft_sio_init();
    tft_sio_write(buffer, size);
    tft_sio_finish(*p_tft_sio_obj);
}

/* change one TFT register */
static void tft_set_reg(int reg, int val)
{
    tft_command((uint32_t[]) { (reg << 8) | val }, 1);
}

/* try all values for the register currently selected in menu */
static void brute_force_reg()
{
    msleep(2000);

    for (int val = 0; val < 0x100; val++)
    {
        bmp_printf(FONT_LARGE, 50, 50, "%02x: %02x", current_reg, val);
        tft_set_reg(current_reg, val);
        msleep(200);
    }
}

/* try all possible registers and values */
/* is it dangerous? don't know, camera still alive at the time of writing :) */
static void brute_force_all_regs()
{
    msleep(2000);

    for (int reg = 0; reg < 0x100; reg++)
    {
        for (int val = 0; val < 0x100; val++)
        {
            bmp_printf(FONT_LARGE, 50, 50, "%02x: %02x", reg, val);
            /* 5D3: Canon code sends 0x34 before changing stuff (same as TftDeepStanby)
             * and 0x35 afterwards to wake up the display; important? */
            tft_set_reg(reg, val);
            msleep(50);
        }

        /* restore the display back to working condition */
        enter_play_mode();
        exit_play_qr_mode();
    }
}

/* forward reference */
static struct menu_entry tft_regs_menu[];

static MENU_UPDATE_FUNC(current_reg_update)
{
    /* link the Value field to the new register */
    tft_regs_menu->children[1].priv = &reg_values[current_reg];
}

static MENU_UPDATE_FUNC(reg_value_update)
{
    /* change value in selected register */
    /* fixme: our menu backend is not very flexible when it comes to custom toggles */
    if (reg_values[current_reg] != reg_before[current_reg])
    {
        tft_set_reg(current_reg, reg_values[current_reg]);
        reg_before[current_reg] = reg_values[current_reg];
    }
}

static struct menu_entry tft_regs_menu[] =
{
    {
        .name   = "TFT SIO registers",
        .select = menu_open_submenu,
        .help   = "Adjust TFT LCD controller registers.",
        .children =  (struct menu_entry[]) {
            {
                .name       = "Register",
                .priv       = &current_reg,
                .update     = current_reg_update,
                .max        = 255,
                .unit       = UNIT_HEX,
                .icon_type  = IT_PERCENT,
                .help       = "TFT register address (0-255)",
            },
            {
                .name       = "Value",
                .priv       = &reg_values[0],
                .update     = reg_value_update,
                .max        = 255,
                .unit       = UNIT_HEX,
                .icon_type  = IT_PERCENT,
                .help       = "TFT register value (0-255)",
            },
            {
                .name       = "Brute-force this register",
                .priv       = &brute_force_reg,
                .select     = run_in_separate_task,
                .help       = "Try all possible values for this register (0-255)",
            },
            {
                .name       = "Brute-force all registers",
                .priv       = &brute_force_all_regs,
                .select     = run_in_separate_task,
                .help       = "Try all possible values for all registers (0-255 x 0-255)",
            },
            MENU_EOL,
        },
    },
};

static unsigned int tft_regs_init()
{
    if (is_camera("5D3", "1.1.3"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF12D284;
        tft_sio_write   = (void *) 0xFF12D1E0;
        tft_sio_finish  = (void *) 0xFF13BDC8;
        p_tft_sio_obj   = (void **)   0x246F0;
    }

    if (is_camera("5D3", "1.2.3"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF12C950;
        tft_sio_write   = (void *) 0xFF12C8AC;
        tft_sio_finish  = (void *) 0xFF13BE8C;
        p_tft_sio_obj   = (void **)   0x246D8;
    }

    if (is_camera("5D3", "1.3.4"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF12C958;
        tft_sio_write   = (void *) 0xFF12C8B4;
        tft_sio_finish  = (void *) 0xFF13BE94;
        p_tft_sio_obj   = (void **)   0x246D8;
    }

    if (is_camera("5D3", "1.3.5"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF12C950;
        tft_sio_write   = (void *) 0xFF12C8AC;
        tft_sio_finish  = (void *) 0xFF13BE8C;
        p_tft_sio_obj   = (void **)   0x27BE0;
    }

    if (is_camera("5D2", "2.1.2"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF065B50;
        tft_sio_write   = (void *) 0xFF065A54;
        tft_sio_finish  = (void *) 0xFF071FD4;
        p_tft_sio_obj   = (void **)    0x2828;
    }

    if (is_camera("6D", "1.1.6"))
    {
        /* TftDeepStanby, CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF1397F4;
        tft_sio_write   = (void *) 0xFF139870;
        tft_sio_finish  = (void *) 0xFF14A0B0;
        p_tft_sio_obj   = (void **)   0x75540;
    }

    if (is_camera("7D", "2.0.3"))
    {
        /* TftDeepStanby, PD_IsDispLvOnTFT */
        tft_sio_init    = (void *) 0xFF070114;
        tft_sio_write   = (void *) 0xFF070068;
        tft_sio_finish  = (void *) 0xFF07F8F4;
        p_tft_sio_obj   = (void **)    0x21D0;
    }

    if (is_camera("7D", "2.0.6"))
    {
        /* TftDeepStanby, PD_IsDispLvOnTFT */
        tft_sio_init    = (void *) 0xFF0700E8;
        tft_sio_write   = (void *) 0xFF07003C;
        tft_sio_finish  = (void *) 0xFF07F8C8;
        p_tft_sio_obj   = (void **)    0x21D0;
    }

    if (is_camera("60D", "1.1.1"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF06125C;
        tft_sio_write   = (void *) 0xFF06115C;
        tft_sio_finish  = (void *) 0xFF0724FC;
        p_tft_sio_obj   = (void **)    0x2488;
    }

    if (is_camera("50D", "1.0.9"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF86017C;
        tft_sio_write   = (void *) 0xFF860044;
        tft_sio_finish  = (void *) 0xFF86AA00;
        p_tft_sio_obj   = (void **)    0x2880;
    }

    if (is_camera("700D", "1.1.5"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF128A28;
        tft_sio_write   = (void *) 0xFF128928;
        tft_sio_finish  = (void *) 0xFF13C420;
        p_tft_sio_obj   = (void **)   0x23C58;
    }

    if (is_camera("650D", "1.0.4"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF127E88;
        tft_sio_write   = (void *) 0xFF127D88;
        tft_sio_finish  = (void *) 0xFF13B868;
        p_tft_sio_obj   = (void **)   0x23C48;
    }

    if (is_camera("600D", "1.0.2"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF0625B4;
        tft_sio_write   = (void *) 0xFF062430;
        tft_sio_finish  = (void *) 0xFF074FE8;
        p_tft_sio_obj   = (void **)    0x2410;
    }

    if (is_camera("550D", "1.0.9"))//
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF05D638;
        tft_sio_write   = (void *) 0xFF05D594;
        tft_sio_finish  = (void *) 0xFF06DE40;
        p_tft_sio_obj   = (void **)    0x240C;
    }

    if (is_camera("500D", "1.1.1"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF060E18;
        tft_sio_write   = (void *) 0xFF060CF4;
        tft_sio_finish  = (void *) 0xFF06D9A8;
        p_tft_sio_obj   = (void **)    0x2620;
    }

    if (is_camera("1100D", "1.0.5"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF062374;
        tft_sio_write   = (void *) 0xFF0622D0;
        tft_sio_finish  = (void *) 0xFF073564;
        p_tft_sio_obj   = (void **)    0x23BC;
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF12909C;
        tft_sio_write   = (void *) 0xFF128F9C;
        tft_sio_finish  = (void *) 0xFF13B454;
        p_tft_sio_obj   = (void **)   0x3E690;
    }

    if (is_camera("EOSM", "2.0.3"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF12914C;
        tft_sio_write   = (void *) 0xFF12904C;
        tft_sio_finish  = (void *) 0xFF13B504;
        p_tft_sio_obj   = (void **)   0X3E690;
    }

    if (is_camera("EOSM2", "1.0.3"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF137360;
        tft_sio_write   = (void *) 0xFF1371F0;
        tft_sio_finish  = (void *) 0xFF14A6E8;
        p_tft_sio_obj   = (void **)   0X904D8;
    }

    if (is_camera("100D", "1.0.1"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF131C1C;
        tft_sio_write   = (void *) 0xFF131AE0;
        tft_sio_finish  = (void *) 0xFF146C20;
        p_tft_sio_obj   = (void **)   0x65B28;
    }

    if (is_camera("70D", "1.1.2"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xff134c64;
        tft_sio_write   = (void *) 0xff134bb0;
        tft_sio_finish  = (void *) 0xff147bd0;
        p_tft_sio_obj   = (void **)   0x7B41C;
    }

    if (tft_sio_init && tft_sio_write && tft_sio_finish && p_tft_sio_obj)
    {
        menu_add("Debug", tft_regs_menu, COUNT(tft_regs_menu));
    }
    else
    {
        NotifyBox(5000, "tft_regs: unsupported model");
    }

    return 0;
}

static unsigned int tft_regs_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(tft_regs_init)
    MODULE_DEINIT(tft_regs_deinit)
MODULE_INFO_END()
