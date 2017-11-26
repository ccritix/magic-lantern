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

    if (is_camera("60D", "1.1.1"))
    {
        /* TftDeepStanby, EnableTftCtrl CurrentBrightness=%d */
        tft_sio_init    = (void *) 0xFF06125C;
        tft_sio_write   = (void *) 0xFF06115C;
        tft_sio_finish  = (void *) 0xFF0724FC;
        p_tft_sio_obj   = (void **)    0x2488;
    }

    if (is_camera("650D", "1.0.4"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF127E88;
        tft_sio_write   = (void *) 0xFF127D88;
        tft_sio_finish  = (void *) 0xFF13B868;
        p_tft_sio_obj   = (void **)   0x23C48;
    }

    if (is_camera("700D", "1.1.5"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF128A28;
        tft_sio_write   = (void *) 0xFF128928;
        tft_sio_finish  = (void *) 0xFF13C420;
        p_tft_sio_obj   = (void **)   0x23C58;
    }

    if (is_camera("EOSM", "2.0.2"))
    {
        /* TFT Command start ... TFT Command end */
        tft_sio_init    = (void *) 0xFF12906C;
        tft_sio_write   = (void *) 0xFF128F6C;
        tft_sio_finish  = (void *) 0xFF13B30C;
        p_tft_sio_obj   = (void **)   0x3EAF0;
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
