/**
 * Access to ISO-related registers. For research only.
 * 
 * Hardcoded for 5D3.
 * 
 * (C) 2014 a1ex. GPL.
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <raw.h>
#include <lens.h>
#include <math.h>

#include <gdb.h>

static CONFIG_INT("enabled", hooks_enabled, 0);
static CONFIG_INT("gain.cmos", cmos_gain, 0);
static CONFIG_INT("gain.adtg", adtg_gain, 0);
static CONFIG_INT("gain.digital", digital_gain, 0);
static CONFIG_INT("saturate.offset", saturate_offset, 0);
static CONFIG_INT("black.offset", black_white_offset, 0);
static CONFIG_INT("black.ref", black_reference, 0);
static CONFIG_INT("adtg.preamp", adtg_preamp, -1);
static CONFIG_INT("adtg.fe", adtg_fe, -1);
static int default_cmos_gain = 0;
static int default_adtg_gain = 0;
static int default_digital_gain = 0;
static int default_saturate_offset = 0;
static int default_black_white_offset = 0;
static int default_black_reference = 0;
static int default_adtg_preamp = 0;
static int default_adtg_fe = 0;

/* default settings used for full-stop ISOs */
static int default_white_level_fullstop = 15283;        /* my 5D3 only */
static int default_black_level_fullstop = 2048;         /* all 5D3's */
static int default_saturate_offset_fullstop = 0xC4C;    /* all 5D3's */

/* CMOS gains for base ISO */
static int cmos_gains[8] = {
    0x003,  /* ISO 100 */
    0x113,  /* ISO 200 */
    0x223,  /* ISO 400 */
    0x333,  /* ISO 800 */
    0x443,  /* ISO 1600 */
    0x553,  /* ISO 3200 */
    0xDD3,  /* ISO 6400 */
    0xFF3,  /* ISO 12800 */
};

static int extended_cmos_gains[16] = {
    0x003,  /* ISO 100 */
    0x113,  /* ISO 200 */
    0x223,  /* ISO 400 */
    0x333,  /* ISO 800 */
    0x443,  /* ISO 1600 */
    0x553,  /* ISO 3200 */
    0x663,  /* ISO 600 */
    0x773,  /* ISO 6000 */
    0x883,  /* ISO 200 */
    0x993,  /* ISO 400 */
    0xAA3,  /* ISO 800 */
    0xBB3,  /* ISO 1600 */
    0xCC3,  /* ISO 3000 */
    0xDD3,  /* ISO 6400 */
    0xEE3,  /* ISO 1100 */
    0xFF3,  /* ISO 12800 */
};

/* todo: double-check these with raw_diag */
static int extended_cmos_isos[16] = {
    100,
    200,
    400,
    800,
    1600,
    3200,
    600,
    6000,
    200,
    400,
    800,
    1600,
    3000,
    6400,
    1100,
    12800,
};

/* only used to do the math on equivalent ISOs and so on */
/* these vary from camera to camera slightly (say 2-3 units) */
static int fullstop_adtg_gains[8] = {
    0x41C,  /* ISO 100 */
    0x434,  /* ISO 200 */
    0x448,  /* ISO 400 */
    0x443,  /* ISO 800 */
    0x455,  /* ISO 1600 */
    0x47E,  /* ISO 3200 */
    0x4B2,  /* ISO 6400 */
    0x4CA,  /* ISO 12800 */
};

/* full-stop ISO index from CMOS gain */
static int get_cmos_iso_index(int cmos_gain)
{
    for (int i = 0; i < COUNT(cmos_gains); i++)
    {
        if (cmos_gains[i] == cmos_gain)
            return i;
    }
    return 0;
}

static int get_extended_cmos_iso_index(int cmos_gain)
{
    for (int i = 0; i < COUNT(extended_cmos_gains); i++)
    {
        if (extended_cmos_gains[i] == cmos_gain)
            return i;
    }
    return 0;
}

/* full-stop ISO index from Canon settings */
static int get_raw_iso_index()
{
    /* from 100 to 12800 */
    return COERCE(((lens_info.raw_iso+3)/8*8 - ISO_100) / 8, 0, 7);
}

/* intercept engio_write calls to override digital gain/offset registers */
static void engio_write_log(breakpoint_t *bkpt)
{
    uint32_t* data_buf = (uint32_t*) bkpt->ctx[0];
    
    while(*data_buf != 0xFFFFFFFF)
    {
        uint32_t reg = (*data_buf) & 0xFFFFFFFE;
        data_buf++;
        uint32_t val = (*data_buf);
        
        if (reg == 0xc0f37ae4 || reg == 0xc0f37af0 || reg == 0xc0f37afc || reg == 0xc0f37b08)
        {
            default_digital_gain = val;
            if (digital_gain) *data_buf = digital_gain;
        }
        
        if (reg == 0xc0f37ae0 || reg == 0xc0f37aec || reg == 0xc0f37af8 || reg == 0xc0f37b04)
        {
            default_black_white_offset = val - 0x7000;
            if (black_white_offset) *data_buf = black_white_offset + 0x7000;
        }
        
        if (reg == 0xc0f0819c)
        {
            default_saturate_offset = val;
            if (saturate_offset) *data_buf = saturate_offset;
        }

        data_buf++;
    }
}

/* intercept adtg_write calls to override ADTG gain */
static void adtg_log(breakpoint_t *bkpt)
{
    uint32_t cs = bkpt->ctx[0];
    uint32_t *data_buf = (uint32_t *) bkpt->ctx[1];
    int dst = cs & 0xF;

    while(*data_buf != 0xFFFFFFFF)
    {
        uint32_t data = *data_buf;
        uint32_t reg = data >> 16;
        uint32_t val = data & 0xFFFF;
        if ((reg == 0x8882 || reg == 0x8884 || reg == 0x8886 || reg == 0x8888) && (dst == 2 || dst == 4))
        {
            if (reg == 0x8882 && dst == 2) default_adtg_gain = val;
            if (adtg_gain) val = adtg_gain;
            *data_buf = (reg << 16) | (val & 0xFFFF);
        }
        if ((reg == 0x8 || reg == 0x9 || reg == 0xA || reg == 0xB) && (dst == 2 || dst == 4))
        {
            if (reg == 0x8 && dst == 2) default_adtg_preamp = val;
            if (adtg_preamp >= 0) val = adtg_preamp;
            *data_buf = (reg << 16) | (val & 0xFFFF);
        }
        if ((reg == 0xFE) && (dst == 2 || dst == 4))
        {
            if (dst == 2) default_adtg_fe = val;
            if (adtg_fe >= 0) val = adtg_fe;
            *data_buf = (reg << 16) | (val & 0xFFFF);
        }
        if ((reg == 0x8880) && (dst == 6))
        {
            if (!default_black_reference) default_black_reference = val;    // this only works on first test picture
            if (black_reference) val = black_reference;
            *data_buf = (reg << 16) | (val & 0xFFFF);
        }
        data_buf++;
    }
}

static int cmos_gain_changed = 0;

/* intercept cmos_write calls to override CMOS gain */
static void cmos_log(breakpoint_t *bkpt)
{
    uint16_t *data_buf = (unsigned short *) bkpt->ctx[0];
    
    while(*data_buf != 0xFFFF)
    {
        uint16_t data = *data_buf;
        uint16_t reg = data >> 12;
        uint16_t val = data & 0xFFF;
        
        if (reg == 0)
        {
            default_cmos_gain = val;
            if (cmos_gain)
            {
                cmos_gain_changed = 1;
                val = cmos_gain;
                *data_buf = (reg << 12) | (val & 0xFFF);
            }
        }
        
        data_buf++;
    }
}

/* enable/disable the hack */
static MENU_SELECT_FUNC(iso_regs_toggle)
{
    hooks_enabled = !hooks_enabled;

    static breakpoint_t * bkpt1 = 0;
    static breakpoint_t * bkpt2 = 0;
    static breakpoint_t * bkpt3 = 0;
    static breakpoint_t * bkpt4 = 0;

    if (hooks_enabled)
    {
        uint32_t ADTG_WRITE_FUNC  = 0x11644;
        uint32_t CMOS_WRITE_FUNC  = 0x119CC;
        uint32_t CMOS2_WRITE_FUNC = 0x11784;
        uint32_t ENGIO_WRITE_FUNC = 0xff28cc3c;

        if (streq(firmware_version, "1.2.3"))
        {
            ADTG_WRITE_FUNC = 0x11640;
            ENGIO_WRITE_FUNC = 0xFF290F98+8;    /* was ist das? */
        }
        else if (streq(firmware_version, "1.1.3"))
        {
        }
        else return;

        gdb_setup();
        bkpt1 = gdb_add_watchpoint(ADTG_WRITE_FUNC,  0, &adtg_log);
        bkpt2 = gdb_add_watchpoint(CMOS_WRITE_FUNC,  0, &cmos_log);
        bkpt3 = gdb_add_watchpoint(CMOS2_WRITE_FUNC, 0, &cmos_log);
        bkpt4 = gdb_add_watchpoint(ENGIO_WRITE_FUNC, 0, &engio_write_log);
    }
    else
    {
        gdb_delete_bkpt(bkpt1);
        gdb_delete_bkpt(bkpt2);
        gdb_delete_bkpt(bkpt3);
        gdb_delete_bkpt(bkpt4);
    }
}

/* compute resulting ISO as a result of changing CMOS gain, ADTG gain and Saturate Offset */
/* the digital gain registers will not affect the clipping point, except for overflow/underflow */
/* (that is, when black/white levels go out of range) */
static int get_resulting_iso()
{
    /* Effect of ADTG gain is a simple linear scaling. */
    
    /* Effect of SaturateOffset is also a linear scaling.
     * Say the default range is (canon_white_level - canon_black_level).
     * After changing SaturateOffset, the black level changes with the same delta,
     * and highlight detail gets pulled. New range will be default_range + delta.
     * Scaling factor will be default_range / new_range.
     */
    int highlights_pulled = saturate_offset ? default_saturate_offset_fullstop - saturate_offset : 0;
    highlights_pulled += (black_reference ? default_black_reference - black_reference : 0);
    int old_range = default_white_level_fullstop - default_black_level_fullstop;
    int new_range = old_range + highlights_pulled;
    float saturate_offset_scaling = (float) old_range / new_range;
    
    /* consider Canon full-stop ISOs accurate (ISO 100, 200, 400...) */
    /* that means, default register configuration at these ISOs is used as reference */
    int current_cmos_gain = cmos_gain ? cmos_gain : default_cmos_gain;
    int base_iso = extended_cmos_isos[(current_cmos_gain >> 4) & 0xF];
    int iso_index = (int)roundf(log2f(base_iso / 100.0f));
    
    /* note that we may get some differences because ADTG default gains are not constant (continuously calibrated?) */
    int current_adtg_gain = adtg_gain ? adtg_gain : default_adtg_gain;
    float adtg_gain_scaling = (float) current_adtg_gain / fullstop_adtg_gains[iso_index];
    
    /* ADTG preamp is a gain configured in EV, where 1 unit = roughly 0.0059 EV */
    int adtg_preamp_delta = adtg_preamp >= 0 ? adtg_preamp - default_adtg_preamp : 0;
    float adtg_preamp_scaling = powf(2, adtg_preamp_delta * 0.0059f);
    
    /* register 0xFE is irregular (seems to control 3 amplifiers triggered by bits, but the measured gains don't fully match this hypothesis) */
    float gains_fe[8] = {-0.54, -0.46, -0.43, -0.37, 0, 0.11, 0.18, 0.32};
    int current_fe = adtg_fe >= 0 ? adtg_fe : default_adtg_fe;
    float adtg_fe_scaling = powf(2, gains_fe[current_fe & 7]);

    /* combine all these gains */
    /* assumming the white level did not drop, and the response curve is still linear, the result should be fine */
    int equivalent_iso = (int)roundf(base_iso * saturate_offset_scaling * adtg_gain_scaling * adtg_preamp_scaling * adtg_fe_scaling);
    
    return equivalent_iso;
}

static int reboot_required = 0;

static MENU_UPDATE_FUNC(check_warnings)
{
    if (reboot_required)
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Restart your camera.");
    }

    if (!default_cmos_gain)
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Take a test picture first.");
    }
}

static MENU_UPDATE_FUNC(resulting_iso_update)
{
    int new_iso = get_resulting_iso();
    int old_iso = lens_info.iso;
    if (new_iso)
    {
        MENU_SET_VALUE("%d", new_iso);
        int ev = (int)roundf(log2f((float)new_iso / old_iso) * 100.0);
        MENU_SET_RINFO("%s%d.%02d EV", FMT_FIXEDPOINT2(ev));
    }
    else
    {
        MENU_SET_VALUE("N/A");
    }
}

static MENU_UPDATE_FUNC(cmos_gain_update)
{
    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }
    else
    {
        MENU_SET_VALUE("0x%03X", cmos_gain);
    }
    
    if (default_cmos_gain)
    {
        MENU_SET_RINFO("Default %03X", default_cmos_gain);
    }
    
    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(adtg_gain_update)
{
    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }
    
    if (default_adtg_gain)
    {
        MENU_SET_RINFO("Default %d", default_adtg_gain);
    }

    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(digital_gain_update)
{
    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }
    
    if (default_digital_gain)
    {
        MENU_SET_RINFO("Default %d", default_digital_gain);
    }

    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(saturate_offset_update)
{
    if (default_saturate_offset)
    {
        MENU_SET_RINFO("Default %d", default_saturate_offset);
    }

    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }

    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(black_white_offset_update)
{
    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }
    
    if (default_black_white_offset)
    {
        MENU_SET_RINFO("Default %d", default_black_white_offset);
    }

    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(black_reference_update)
{
    if (default_black_reference)
    {
        MENU_SET_RINFO("Default %d", default_black_reference);
    }
    
    if (CURRENT_VALUE == 0)
    {
        MENU_SET_VALUE("OFF");
    }
}

static MENU_UPDATE_FUNC(adtg_preamp_update)
{
    if (CURRENT_VALUE < 0)
    {
        MENU_SET_VALUE("OFF");
        MENU_SET_ENABLED(0);
    }
    else
    {
        MENU_SET_ENABLED(1);
    }
    
    if (default_adtg_preamp)
    {
        MENU_SET_RINFO("Default %d", default_adtg_preamp);
    }

    check_warnings(entry, info);
}

static MENU_UPDATE_FUNC(adtg_fe_update)
{
    if (CURRENT_VALUE < 0)
    {
        MENU_SET_VALUE("OFF");
        MENU_SET_ENABLED(0);
    }
    else
    {
        MENU_SET_ENABLED(1);
    }
    
    if (default_adtg_fe)
    {
        MENU_SET_RINFO("Default %d", default_adtg_fe);
    }

    check_warnings(entry, info);
}

/* copy Canon settings from last picture */
static MENU_SELECT_FUNC(copy_canon_settings)
{
    cmos_gain = default_cmos_gain;
    adtg_gain = default_adtg_gain;
    digital_gain = default_digital_gain;
    saturate_offset = default_saturate_offset;
    black_white_offset = default_black_white_offset;
    black_reference = default_black_reference;
    adtg_preamp = default_adtg_preamp;
    adtg_fe = default_adtg_fe;
}

static MENU_SELECT_FUNC(disable_overrides)
{
    if (cmos_gain_changed)
    {
        reboot_required = 1;
    }
    cmos_gain = 0;
    adtg_gain = 0;
    digital_gain = 0;
    saturate_offset = 0;
    black_white_offset = 0;
    black_reference = 0;
    adtg_preamp = -1;
    adtg_fe = -1;
}

static struct menu_entry iso_regs_menu[] =
{
    {
        .name = "ISO registers", 
        .priv = &hooks_enabled, 
        .select = iso_regs_toggle,
        .max = 1,
        .help  = "Access to ISO-related registers. For research only.",
        .submenu_width = 710,
        .children =  (struct menu_entry[]) {
            {
                .name = "CMOS Gain",
                .priv = &cmos_gain,
                .update = cmos_gain_update,
                .min = 0,
                .max = 0xFFF,
                .unit = UNIT_HEX,
                .help  = "Analog gain in coarse steps (used for full-stop ISOs).",
            },
            {
                .name = "ADTG Gain",
                .priv = &adtg_gain,
                .update = adtg_gain_update,
                .min = 0,
                .max = 5000,
                .unit = UNIT_DEC,
                .help  = "Analog gain in fine steps. Decrease to get more highlight details.",
                .help2 = "Watch out RAW zebras; should be solid and clean in highlights.",
            },
            {
                .name = "Saturate Offset",
                .priv = &saturate_offset,
                .update = saturate_offset_update,
                .min = 0,
                .max = 2000,
                .unit = UNIT_DEC,
                .help  = "Alters black level and stretches the range, keeping white fixed.",
                .help2 = "Decrease to get more highlight details, but watch out RAW zebras.",
            },
            {
                .name = "Digital Gain",
                .priv = &digital_gain,
                .update = digital_gain_update,
                .min = 0,
                .max = 5000,
                .unit = UNIT_DEC,
                .help  = "Digital scaling factor. It gets burned into the RAW data (sadly).",
                .help2 = "Adjust so OB histogram has no gaps/spikes, or fill the 14bit range.",
            },
            {
                .name = "Black/White Offset ",
                .priv = &black_white_offset,
                .update = black_white_offset_update,
                .min = 0,
                .max = 5000,
                .unit = UNIT_DEC,
                .help  = "Digital offset for both black and white levels.",
                .help2 = "(may interfere with Canon's nonlinear adjustments, visible at high ISO)",
            },
            {
                .name = "Black Reference",
                .priv = &black_reference,
                .update = black_reference_update,
                .min = 0,
                .max = 8192,
                .unit = UNIT_DEC,
                .help  = "Reference value for the black feedback loop?",
                .help2 = "(I believe this is analog correction of the black level)",
            },
            {
                .name = "ADTG preamp",
                .priv = &adtg_preamp,
                .update = adtg_preamp_update,
                .min = -1,
                .max = 0x100,
                .unit = UNIT_DEC,
                .help  = "Gain applied before the ADTG gain and before black clamping.",
                .help2 = "Units: EV (1 unit = 0.006 EV)"
            },
            {
                .name = "ADTG 0xFE",
                .priv = &adtg_fe,
                .update = adtg_fe_update,
                .min = -1,
                .max = 7,
                .unit = UNIT_DEC,
                .help  = "Yet another ADTG gain.",
            },
            {
                .name = "Resulting ISO",
                .update = resulting_iso_update,
                .help = "ISO (by average brightness) obtained with current parameters,",
                .help2 = "assuming ISO 100, 200 ... 12800 are real, and no over/underflows."
            },
            {
                .name = "Copy Canon settings",
                .select = copy_canon_settings,
                .update = check_warnings,
                .help = "Copy Canon default settings from last picture taken,",
                .help2 = "so you can start tweaking from there."
            },
            {
                .name = "Disable all overrides",
                .select = disable_overrides,
                .update = check_warnings,
                .help = "Set everything back to default.",
                .help2 = "If you change CMOS gain, you will have to restart the camera."
            },
            MENU_EOL,
        },
    },
};

static unsigned int iso_regs_init()
{
    if (!is_camera("5D3", "1.1.3") && !is_camera("5D3", "1.2.3"))
    {
        return CBR_RET_ERROR;
    }

    menu_add("Debug", iso_regs_menu, COUNT(iso_regs_menu));

    //~ stateobj_start_spy(SCS_STATE);

    /* feature enabled at startup? */
    if (hooks_enabled)
    {
        hooks_enabled = 0;
        iso_regs_toggle(0,0);
    }
    return 0;
}

static unsigned int iso_regs_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(iso_regs_init)
    MODULE_DEINIT(iso_regs_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(hooks_enabled)
    MODULE_CONFIG(cmos_gain)
    MODULE_CONFIG(adtg_gain)
    MODULE_CONFIG(digital_gain)
    MODULE_CONFIG(saturate_offset)
    MODULE_CONFIG(black_white_offset)
    MODULE_CONFIG(black_reference)
    MODULE_CONFIG(adtg_preamp)
    MODULE_CONFIG(adtg_fe)
MODULE_CONFIGS_END()
