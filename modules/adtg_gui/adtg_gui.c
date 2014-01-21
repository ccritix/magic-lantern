/**
 * ADTG register editing GUI
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>

#include <gdb.h>
#include <cache_hacks.h>

#include "avl.h"
#include "avl.c"    /* unusual include in order to avoid exporting the AVL symbols (keep the namespace clean) */

#define DST_CMOS16  0xF000
#define DST_CMOS    0x0F00
#define DST_ADTG    0x000F      /* any ADTG */
#define DST_ANY     0xFFFF

struct known_reg
{
    uint16_t dst;
    uint16_t reg;
    uint16_t is_nrzi;   /* this will override the default guess */
    char* description;
};

static struct known_reg known_regs[] = {
    {DST_CMOS,      0, 0, "Analog ISO (most cameras)"},
    {DST_CMOS,      1, 0, "Vertical offset"},
    {DST_CMOS,      2, 0, "Horizontal offset / column skipping"},
    {DST_CMOS,      3, 0, "Analog ISO on 6D"},
    {DST_CMOS,      5, 0, "Fine vertical offset, black area maybe"},
    {DST_CMOS,      6, 0, "ISO 50 or timing related: FFF => darker image"},
    {DST_CMOS,      7, 0, "Looks like the cmos is dieing (g3gg0)"},
    {DST_ADTG, 0x8000, 0, "Causes interlacing (g3gg0)"},
    {DST_ADTG, 0x800C, 0, "Line skipping factor (2 = 1080p, 4 = 720p, 0 = zoom)"},
    {DST_ADTG, 0x805E, 1, "Shutter blanking for x5/x10 zoom"},
    {DST_ADTG, 0x8060, 1, "Shutter blanking for LiveView 1x"},
    {DST_ADTG, 0x8172, 1, "Line count to sample. same as video resolution (g3gg0)"},
    {DST_ADTG, 0x8178, 1, "dwSrFstAdtg1[4], Line count + 1"},
    {DST_ADTG, 0x8179, 1, "dwSrFstAdtg1[5]"},
    {DST_ADTG, 0x8196, 1, "dwSrFstAdtg1[2], Line count + 1"},
    {DST_ADTG, 0x8197, 1, "dwSrFstAdtg1[3]"},
    {DST_ADTG, 0x82F3, 1, "Line count that gets darker (top optical black related)"},
    {DST_ADTG, 0x82F8, 1, "Line count"},
    {DST_ADTG, 0x8830, 0, "Only slightly changes the color of the image (g3gg0)"},
    {DST_ADTG, 0x8880, 0, "Black level (reference value for the feedback loop?)"},
    {DST_ADTG, 0x8882, 0, "Digital gain (per column maybe)"}, /* I believe it's digital, not 100% sure */
    {DST_ADTG, 0x8884, 0, "Digital gain (per column maybe)"},
    {DST_ADTG, 0x8886, 0, "Digital gain (per column maybe)"},
    {DST_ADTG, 0x8888, 0, "Digital gain (per column maybe)"},

    {DST_ADTG, 0x105F, 1, "Shutter blanking for x5/x10 zoom"},
    {DST_ADTG, 0x106E, 1, "Shutter blanking for LiveView 1x"},

    {DST_ADTG,   0x14, 1, "ISO related"},
    {DST_ADTG,   0x15, 1, "ISO related"},
    
    {0xC0F0,   0x8030, 0, "Digital gain for ISO (SHAD_GAIN)"},
    {0xC0F0,   0x8034, 0, "Black level used for developing the image (SHAD_PRESETUP)"},
    {0xC0F0,   0x819c, 0, "Saturate Offset (photo mode)"},
    
    {0xC0F0,   0x6000, 0, "FPS register for confirming changes"},
    {0xC0F0,   0x6004, 0, "FPS related, SetHeadForReadout"},
    {0xC0F0,   0x6008, 0, "FPS register A"},
    {0xC0F0,   0x600C, 0, "FPS related"},
    {0xC0F0,   0x6010, 0, "FPS related"},
    {0xC0F0,   0x6014, 0, "FPS register B"},
    {0xC0F0,   0x6018, 0, "FPS related"},
    {0xC0F0,   0x601C, 0, "FPS related"},
    {0xC0F0,   0x6020, 0, "FPS related"},

    {0xC0F0,   0x6088, 0, "Video Y-Res related? 600D: FHD 1182|1070, 3x 1048|1102, HD 720|1070"},

    {0xC0F0,   0x8D1C, 0, "Vignetting correction data (DIGIC V)"},
    {0xC0F0,   0x8D24, 0, "Vignetting correction data (DIGIC V)"},
    {0xC0F0,   0x8578, 0, "Vignetting correction data (DIGIC IV)"},
    {0xC0F0,   0x857C, 0, "Vignetting correction data (DIGIC IV)"},
    
    {0xC0F1,   0x40c4, 0, "Display saturation"},
    {0xC0F1,   0x41B8, 0, "Display brightness and contrast"},
    {0xC0F1,   0x4140, 0, "Display filter (EnableFilter, DIGIC peaking)"},
    {0xC0F1,   0x4164, 0, "Display position (vertical shift)"},
    {0xC0F1,   0x40cc, 0, "Display zebras (used for fast zebras in ML)"},
    
    {0xC0F3,   0x7ae4, 0, "ISO digital gain (5D3 photo mode)"},
    {0xC0F3,   0x7af0, 0, "ISO digital gain (5D3 photo mode)"},
    {0xC0F3,   0x7afc, 0, "ISO digital gain (5D3 photo mode)"},
    {0xC0F3,   0x7b08, 0, "ISO digital gain (5D3 photo mode)"},

    {0xC0F3,   0x7ae0, 0, "ISO black/white offset (5D3 photo mode)"},
    {0xC0F3,   0x7aec, 0, "ISO black/white offset (5D3 photo mode)"},
    {0xC0F3,   0x7af8, 0, "ISO black/white offset (5D3 photo mode)"},
    {0xC0F3,   0x7b04, 0, "ISO black/white offset (5D3 photo mode)"},
};

static int adtg_enabled = 0;
static int edit_multiplier = 0;
static int show_what = 0;

#define SHOW_ALL 0
#define SHOW_KNOWN_ONLY 1
#define SHOW_MODIFIED 2
#define SHOW_OVERRIDEN 3

static int digic_intercept = 0;

static uint32_t ADTG_WRITE_FUNC = 0;
static uint32_t CMOS_WRITE_FUNC = 0;
static uint32_t CMOS2_WRITE_FUNC = 0;
static uint32_t CMOS16_WRITE_FUNC = 0;
static uint32_t ENGIO_WRITE_FUNC = 0;
static uint32_t ENG_DRV_OUT_FUNC = 0;
static uint32_t ENG_DRV_OUTS_FUNC = 0;

struct reg_entry
{
    struct avl avl;
    union
    {
        struct
        {
            uint16_t dst;       /* register "class" */
            uint16_t reg;       /* register offset */
        };
        uint32_t key;           /* key in the AVL tree */
    };
    uint16_t val;
    uint16_t prev_val;
    int override;
    unsigned is_nrzi:1;
    void* addr;
    uint32_t caller_task;
    uint32_t caller_pc;
};

static int cmp_reg(void* a,void* b){
    return ((struct reg_entry*)a)->key - ((struct reg_entry*)b)->key;
}

static struct reg_entry regs[2048];
static int reg_num = 0;

/* we need very fast insertion */
/* tiny delays in ADTG or ENGIO may result in broken images */
static struct avl_tree regs_tree;

static int known_match(int i, int reg)
{
    return
        (
            (known_regs[i].dst == regs[reg].dst) || 
            (known_regs[i].dst == DST_ADTG && regs[reg].dst == (regs[reg].dst & 0xF)) || 
            (known_regs[i].dst == DST_ANY)
        )
        &&
        (
            (known_regs[i].reg == regs[reg].reg)
        )
    ;
}

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

static uint32_t nrzi_encode( uint32_t in_val )
{
    uint32_t out_val = 0;
    uint32_t old_bit = 0;
    for (int num = 0; num < 32; num++)
    {
        uint32_t bit = in_val & 1<<(30-num) ? 1 : 0;
        if (bit != old_bit)
            out_val |= (1 << (30-num));
        old_bit = bit;
    }
    return out_val;
}

static int reg_iter(struct avl * a)
{
    /* assume 32-bit pointer */
    return (int) a;
}

static struct reg_entry * reg_find(uint16_t dst, uint16_t reg)
{
    struct reg_entry ref = {{0}};
    ref.dst = dst;
    ref.reg = reg;
    return (struct reg_entry *) avl_search(&regs_tree, (struct avl *) &ref, reg_iter);
}

static void reg_update_unique(uint16_t dst, void* addr, uint32_t data, uint32_t reg_shift, uint32_t is_nrzi, uint32_t caller_task, uint32_t caller_pc)
{
    if (reg_num + 1 >= COUNT(regs))
    {
        return;
    }
    
    uint32_t reg = data >> reg_shift;
    uint32_t val = data & ((1 << reg_shift) - 1);
    
    struct reg_entry * re = reg_find(dst, reg);

    if (!re)
    {
        /* new entry */
        re = &regs[reg_num];
        re->dst = dst;
        re->reg = reg;
        re->override = INT_MIN;
        re->is_nrzi = is_nrzi; /* initial guess; may be overriden */
        re->prev_val = val;
        reg_num++;
        avl_insert(&regs_tree, (struct avl *) re);
    }

    /* fill the data */
    if (re->override != INT_MIN)
    {
        int ovr = re->is_nrzi ? (int)nrzi_encode(re->override) : re->override;
        uint16_t* val_ptr = addr;
        ovr &= ((1 << reg_shift) - 1);
        *val_ptr &= ~((1 << reg_shift) - 1);
        *val_ptr |= ovr;
    }

    re->addr = addr;
    re->val = val;
    re->caller_task = caller_task;
    re->caller_pc = caller_pc;
}

static void reg_update_unique_32(uint16_t dst, uint16_t reg, uint32_t* pval, uint32_t caller_task, uint32_t caller_pc)
{
    if (reg_num + 1 >= COUNT(regs))
    {
        return;
    }

    uint32_t val = *pval;

    struct reg_entry * re = reg_find(dst, reg);

    if (!re)
    {
        /* new entry */
        re = &regs[reg_num];
        re->dst = dst;
        re->reg = reg;
        re->override = INT_MIN;
        re->is_nrzi = 0;
        re->prev_val = val;
        reg_num++;
        avl_insert(&regs_tree, (struct avl *) re);
    }

    if (re->override != INT_MIN)
    {
        uint32_t ovr = re->override;
        *pval = ovr;
    }

    re->addr = pval;
    re->val = val;
    re->caller_task = caller_task;
    re->caller_pc = caller_pc;
}

static void adtg_log(breakpoint_t *bkpt)
{
    unsigned int cs = bkpt->ctx[0];
    unsigned int *data_buf = (unsigned int *) bkpt->ctx[1];
    int dst = cs & 0xF;

    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];
    
    /* log all ADTG writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        /* ADTG4 registers seem to use NRZI */
        reg_update_unique(dst, data_buf, *data_buf, 16, dst == 4, caller_task, caller_pc);
        data_buf++;
    }
}

static void cmos_log(breakpoint_t *bkpt)
{
    unsigned short *data_buf = (unsigned short *) bkpt->ctx[0];
    
    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];
    
    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        reg_update_unique(DST_CMOS, data_buf, *data_buf, 12, 0, caller_task, caller_pc);
        data_buf++;
    }
}

static void cmos16_log(breakpoint_t *bkpt)
{
    unsigned short *data_buf = (unsigned short *) bkpt->ctx[0];
    
    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];

    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        reg_update_unique(DST_CMOS16, data_buf, *data_buf, 12, 0, caller_task, caller_pc);
        data_buf++;
    }
}

static void engio_write_log(breakpoint_t *bkpt)
{
    if (!digic_intercept) return;
    
    uint32_t* data_buf = (uint32_t*) bkpt->ctx[0];

    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];
    
    /* log all ENGIO register writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        uint16_t dst = ((*data_buf) & 0xFFFF0000) >> 16;
        uint16_t reg = (*data_buf) & 0x0000FFFF;
        data_buf++;
        reg_update_unique_32(dst, reg, data_buf, caller_task, caller_pc);
        data_buf++;
    }
}

static void EngDrvOut_log(breakpoint_t *bkpt)
{
    if (!digic_intercept) return;

    uint32_t data = (uint32_t) bkpt->ctx[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t val = (uint32_t) bkpt->ctx[1];

    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];
    
    reg_update_unique_32(dst, reg, &val, caller_task, caller_pc);
    bkpt->ctx[1] = val;
}

static void EngDrvOuts_log(breakpoint_t *bkpt)
{
    if (!digic_intercept) return;

    uint32_t data = (uint32_t) bkpt->ctx[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t * values = (uint32_t *) bkpt->ctx[1];
    uint32_t num = (uint32_t) bkpt->ctx[2];

    uint32_t caller_task = get_current_task();
    uint32_t caller_pc = bkpt->ctx[14];
    
    for (uint32_t i = 0; i < num; i++)
    {
        reg_update_unique_32(dst, reg + 4*i, &values[i], caller_task, caller_pc);
    }
}

static MENU_SELECT_FUNC(adtg_toggle)
{
    adtg_enabled = !adtg_enabled;
    
    static breakpoint_t * bkpt1 = 0;
    static breakpoint_t * bkpt2 = 0;
    static breakpoint_t * bkpt3 = 0;
    static breakpoint_t * bkpt4 = 0;
    static breakpoint_t * bkpt5 = 0;
    static breakpoint_t * bkpt6 = 0;
    static breakpoint_t * bkpt7 = 0;
    
    if (adtg_enabled)
    {
        /* set watchpoints at ADTG and CMOS writes */
        gdb_setup();
        if (ADTG_WRITE_FUNC)   bkpt1 = gdb_add_watchpoint(ADTG_WRITE_FUNC, 0, &adtg_log);
        if (CMOS_WRITE_FUNC)   bkpt2 = gdb_add_watchpoint(CMOS_WRITE_FUNC, 0, &cmos_log);
        if (CMOS2_WRITE_FUNC)  bkpt3 = gdb_add_watchpoint(CMOS2_WRITE_FUNC, 0, &cmos_log);
        if (CMOS16_WRITE_FUNC) bkpt4 = gdb_add_watchpoint(CMOS16_WRITE_FUNC, 0, &cmos16_log);
        if (ENGIO_WRITE_FUNC)  bkpt5 = gdb_add_watchpoint(ENGIO_WRITE_FUNC, 0, &engio_write_log);
        if (ENG_DRV_OUT_FUNC)  bkpt6 = gdb_add_watchpoint(ENG_DRV_OUT_FUNC, 0, &EngDrvOut_log);
        if (ENG_DRV_OUTS_FUNC) bkpt7 = gdb_add_watchpoint(ENG_DRV_OUTS_FUNC, 0, &EngDrvOuts_log);
    }
    else
    {
        /* uninstall watchpoints */
        if (bkpt1) gdb_delete_bkpt(bkpt1);
        if (bkpt2) gdb_delete_bkpt(bkpt2);
        if (bkpt3) gdb_delete_bkpt(bkpt3);
        if (bkpt4) gdb_delete_bkpt(bkpt4);
        if (bkpt5) gdb_delete_bkpt(bkpt5);
        if (bkpt6) gdb_delete_bkpt(bkpt6);
        if (bkpt7) gdb_delete_bkpt(bkpt7);
    }
}

static MENU_UPDATE_FUNC(reg_update)
{
    int reg = (int) entry->priv;
    if (reg < 0 || reg >= COUNT(regs))
        return;
    
    char dst_name[10];
    if (regs[reg].dst == DST_CMOS)
        snprintf(dst_name, sizeof(dst_name), "CMOS");
    else if (regs[reg].dst == DST_CMOS16)
        snprintf(dst_name, sizeof(dst_name), "CMOS16");
    else if (regs[reg].dst & 0xFFF0)
        snprintf(dst_name, sizeof(dst_name), "%X", regs[reg].dst);
    else
        snprintf(dst_name, sizeof(dst_name), "ADTG%d", regs[reg].dst);

    for (int i = 0; i < COUNT(known_regs); i++)
    {
        if (known_match(i, reg))
        {
            regs[reg].is_nrzi = known_regs[i].is_nrzi;
        }
    }

    MENU_SET_NAME("%s[%x]%s", dst_name, regs[reg].reg, regs[reg].is_nrzi ? " N" : "");
    
    if (show_what == SHOW_MODIFIED && regs[reg].override == INT_MIN)
    {
        MENU_SET_VALUE(
            "0x%x (was 0x%x)",
            regs[reg].is_nrzi ? nrzi_decode(regs[reg].val) : regs[reg].val,
            regs[reg].is_nrzi ? nrzi_decode(regs[reg].prev_val) : regs[reg].prev_val
        );
    }
    else
    {
        MENU_SET_VALUE(
            "0x%x",
            regs[reg].is_nrzi ? nrzi_decode(regs[reg].val) : regs[reg].val
        );
    }
    
    MENU_SET_HELP("%s:%x:%x v=%d(0x%x) nrzi=%d(0x%x).", get_task_name_from_id(regs[reg].caller_task), regs[reg].caller_pc, regs[reg].addr, regs[reg].val, regs[reg].val, nrzi_decode(regs[reg].val), nrzi_decode(regs[reg].val));
    
    if (reg_num >= COUNT(regs)-1)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Too many registers.");

    MENU_SET_ICON(MNI_BOOL(regs[reg].override != INT_MIN), 0);

    if (regs[reg].override != INT_MIN)
    {
        MENU_SET_RINFO("-> 0x%x", regs[reg].override);
        if (menu_active_and_not_hidden())
            MENU_SET_WARNING(MENU_WARN_INFO, "Press Q to stop overriding this register.");
    }
    else
    {
        for (int i = 0; i < COUNT(known_regs); i++)
        {
            if (known_match(i, reg))
            {
                MENU_SET_WARNING(MENU_WARN_INFO, "%s.", known_regs[i].description);
                
                if (show_what != SHOW_MODIFIED) /* do we have enough space to show a shortened description? */
                {
                    char msg[12];
                    snprintf(msg, sizeof(msg), "%s", known_regs[i].description);
                    if (!streq(msg, known_regs[i].description))
                    {
                        int len = COUNT(msg);
                        msg[len-1] = 0;
                        msg[len-2] = '.';
                        msg[len-3] = '.';
                        msg[len-4] = '.';
                    }
                    MENU_SET_RINFO("%s", msg);
                }
            }
        }
    }
}

static MENU_SELECT_FUNC(reg_toggle)
{
    int reg = (int) priv;
    if (reg < 0 || reg >= COUNT(regs))
        return;
    
    if (regs[reg].override == INT_MIN)
        regs[reg].override = regs[reg].is_nrzi ? nrzi_decode(regs[reg].val) : regs[reg].val;
 
    static int multipliers[] = {1, 16, 256, 10, 100, 1000};
    regs[reg].override += delta * multipliers[edit_multiplier];
}

static MENU_SELECT_FUNC(reg_clear_override)
{
    int reg = (int) priv;
    if (reg < 0 || reg >= COUNT(regs))
        return;
    
    if (regs[reg].override != INT_MIN)
        regs[reg].override = INT_MIN;
    else
        menu_close_submenu(); 
}

#define REG_ENTRY(i) \
        { \
            .priv = (void*)i, \
            .select = reg_toggle, \
            .select_Q = reg_clear_override, \
            .update = reg_update, \
            .edit_mode = EM_MANY_VALUES_LV, \
            .shidden = 1, \
        }

static MENU_UPDATE_FUNC(show_update);

static MENU_UPDATE_FUNC(adtg_update)
{
    MENU_SET_ICON(MNI_BOOL(adtg_enabled), 0);
    if (adtg_enabled)
        MENU_SET_WARNING(MENU_WARN_INFO, "ADTG hooks enabled, press PLAY to disable.");
}

static MENU_SELECT_FUNC(adtg_main)
{
    if (delta > 0)
    {
        if (!adtg_enabled)
            adtg_toggle(priv, delta);
        menu_open_submenu();
    }
    else
    {
        if (adtg_enabled)
            adtg_toggle(priv, delta);
    }
}

static struct menu_entry adtg_gui_menu[] =
{
    {
        .name = "ADTG registers",
        .update = adtg_update,
        .select = &adtg_main,
        .icon_type = IT_SUBMENU,
        .help = "Edit ADTG/CMOS register values.",
        .submenu_width = 710,
        .children =  (struct menu_entry[]) {
            {
                .name = "Editing step", 
                .priv = &edit_multiplier,
                .max = 5,
                .choices = CHOICES("1", "16 (x << 4)", "256 (x << 8)", "10", "100", "1000"),
                .help = "Step used when editing register values."
            },
            {
                .name = "Show",
                .priv = &show_what,
                .update = show_update,
                .max = 3,
                .choices = CHOICES("Everything", "Known regs only", "Modified regs only", "Overriden regs only"),
                .help2 =    "Everything: show all registers as soon as they are written.\n"
                            "Known: show only the registers with a known description.\n"
                            "Modified: show only regs that have changed their values.\n"
                            "Overriden: show only regs where you have changed the value.\n"
            },
            {
                .name = "DIGIC registers",
                .priv = &digic_intercept,
                .max = 1,
                .help = "Also intercept DIGIC registers (EngDrvOut and engio_write).\n"
            },
            // for i in range(512): print "            REG_ENTRY(%d)," % i
            REG_ENTRY(0),
            REG_ENTRY(1),
            REG_ENTRY(2),
            REG_ENTRY(3),
            REG_ENTRY(4),
            REG_ENTRY(5),
            REG_ENTRY(6),
            REG_ENTRY(7),
            REG_ENTRY(8),
            REG_ENTRY(9),
            REG_ENTRY(10),
            REG_ENTRY(11),
            REG_ENTRY(12),
            REG_ENTRY(13),
            REG_ENTRY(14),
            REG_ENTRY(15),
            REG_ENTRY(16),
            REG_ENTRY(17),
            REG_ENTRY(18),
            REG_ENTRY(19),
            REG_ENTRY(20),
            REG_ENTRY(21),
            REG_ENTRY(22),
            REG_ENTRY(23),
            REG_ENTRY(24),
            REG_ENTRY(25),
            REG_ENTRY(26),
            REG_ENTRY(27),
            REG_ENTRY(28),
            REG_ENTRY(29),
            REG_ENTRY(30),
            REG_ENTRY(31),
            REG_ENTRY(32),
            REG_ENTRY(33),
            REG_ENTRY(34),
            REG_ENTRY(35),
            REG_ENTRY(36),
            REG_ENTRY(37),
            REG_ENTRY(38),
            REG_ENTRY(39),
            REG_ENTRY(40),
            REG_ENTRY(41),
            REG_ENTRY(42),
            REG_ENTRY(43),
            REG_ENTRY(44),
            REG_ENTRY(45),
            REG_ENTRY(46),
            REG_ENTRY(47),
            REG_ENTRY(48),
            REG_ENTRY(49),
            REG_ENTRY(50),
            REG_ENTRY(51),
            REG_ENTRY(52),
            REG_ENTRY(53),
            REG_ENTRY(54),
            REG_ENTRY(55),
            REG_ENTRY(56),
            REG_ENTRY(57),
            REG_ENTRY(58),
            REG_ENTRY(59),
            REG_ENTRY(60),
            REG_ENTRY(61),
            REG_ENTRY(62),
            REG_ENTRY(63),
            REG_ENTRY(64),
            REG_ENTRY(65),
            REG_ENTRY(66),
            REG_ENTRY(67),
            REG_ENTRY(68),
            REG_ENTRY(69),
            REG_ENTRY(70),
            REG_ENTRY(71),
            REG_ENTRY(72),
            REG_ENTRY(73),
            REG_ENTRY(74),
            REG_ENTRY(75),
            REG_ENTRY(76),
            REG_ENTRY(77),
            REG_ENTRY(78),
            REG_ENTRY(79),
            REG_ENTRY(80),
            REG_ENTRY(81),
            REG_ENTRY(82),
            REG_ENTRY(83),
            REG_ENTRY(84),
            REG_ENTRY(85),
            REG_ENTRY(86),
            REG_ENTRY(87),
            REG_ENTRY(88),
            REG_ENTRY(89),
            REG_ENTRY(90),
            REG_ENTRY(91),
            REG_ENTRY(92),
            REG_ENTRY(93),
            REG_ENTRY(94),
            REG_ENTRY(95),
            REG_ENTRY(96),
            REG_ENTRY(97),
            REG_ENTRY(98),
            REG_ENTRY(99),
            REG_ENTRY(100),
            REG_ENTRY(101),
            REG_ENTRY(102),
            REG_ENTRY(103),
            REG_ENTRY(104),
            REG_ENTRY(105),
            REG_ENTRY(106),
            REG_ENTRY(107),
            REG_ENTRY(108),
            REG_ENTRY(109),
            REG_ENTRY(110),
            REG_ENTRY(111),
            REG_ENTRY(112),
            REG_ENTRY(113),
            REG_ENTRY(114),
            REG_ENTRY(115),
            REG_ENTRY(116),
            REG_ENTRY(117),
            REG_ENTRY(118),
            REG_ENTRY(119),
            REG_ENTRY(120),
            REG_ENTRY(121),
            REG_ENTRY(122),
            REG_ENTRY(123),
            REG_ENTRY(124),
            REG_ENTRY(125),
            REG_ENTRY(126),
            REG_ENTRY(127),
            REG_ENTRY(128),
            REG_ENTRY(129),
            REG_ENTRY(130),
            REG_ENTRY(131),
            REG_ENTRY(132),
            REG_ENTRY(133),
            REG_ENTRY(134),
            REG_ENTRY(135),
            REG_ENTRY(136),
            REG_ENTRY(137),
            REG_ENTRY(138),
            REG_ENTRY(139),
            REG_ENTRY(140),
            REG_ENTRY(141),
            REG_ENTRY(142),
            REG_ENTRY(143),
            REG_ENTRY(144),
            REG_ENTRY(145),
            REG_ENTRY(146),
            REG_ENTRY(147),
            REG_ENTRY(148),
            REG_ENTRY(149),
            REG_ENTRY(150),
            REG_ENTRY(151),
            REG_ENTRY(152),
            REG_ENTRY(153),
            REG_ENTRY(154),
            REG_ENTRY(155),
            REG_ENTRY(156),
            REG_ENTRY(157),
            REG_ENTRY(158),
            REG_ENTRY(159),
            REG_ENTRY(160),
            REG_ENTRY(161),
            REG_ENTRY(162),
            REG_ENTRY(163),
            REG_ENTRY(164),
            REG_ENTRY(165),
            REG_ENTRY(166),
            REG_ENTRY(167),
            REG_ENTRY(168),
            REG_ENTRY(169),
            REG_ENTRY(170),
            REG_ENTRY(171),
            REG_ENTRY(172),
            REG_ENTRY(173),
            REG_ENTRY(174),
            REG_ENTRY(175),
            REG_ENTRY(176),
            REG_ENTRY(177),
            REG_ENTRY(178),
            REG_ENTRY(179),
            REG_ENTRY(180),
            REG_ENTRY(181),
            REG_ENTRY(182),
            REG_ENTRY(183),
            REG_ENTRY(184),
            REG_ENTRY(185),
            REG_ENTRY(186),
            REG_ENTRY(187),
            REG_ENTRY(188),
            REG_ENTRY(189),
            REG_ENTRY(190),
            REG_ENTRY(191),
            REG_ENTRY(192),
            REG_ENTRY(193),
            REG_ENTRY(194),
            REG_ENTRY(195),
            REG_ENTRY(196),
            REG_ENTRY(197),
            REG_ENTRY(198),
            REG_ENTRY(199),
            REG_ENTRY(200),
            REG_ENTRY(201),
            REG_ENTRY(202),
            REG_ENTRY(203),
            REG_ENTRY(204),
            REG_ENTRY(205),
            REG_ENTRY(206),
            REG_ENTRY(207),
            REG_ENTRY(208),
            REG_ENTRY(209),
            REG_ENTRY(210),
            REG_ENTRY(211),
            REG_ENTRY(212),
            REG_ENTRY(213),
            REG_ENTRY(214),
            REG_ENTRY(215),
            REG_ENTRY(216),
            REG_ENTRY(217),
            REG_ENTRY(218),
            REG_ENTRY(219),
            REG_ENTRY(220),
            REG_ENTRY(221),
            REG_ENTRY(222),
            REG_ENTRY(223),
            REG_ENTRY(224),
            REG_ENTRY(225),
            REG_ENTRY(226),
            REG_ENTRY(227),
            REG_ENTRY(228),
            REG_ENTRY(229),
            REG_ENTRY(230),
            REG_ENTRY(231),
            REG_ENTRY(232),
            REG_ENTRY(233),
            REG_ENTRY(234),
            REG_ENTRY(235),
            REG_ENTRY(236),
            REG_ENTRY(237),
            REG_ENTRY(238),
            REG_ENTRY(239),
            REG_ENTRY(240),
            REG_ENTRY(241),
            REG_ENTRY(242),
            REG_ENTRY(243),
            REG_ENTRY(244),
            REG_ENTRY(245),
            REG_ENTRY(246),
            REG_ENTRY(247),
            REG_ENTRY(248),
            REG_ENTRY(249),
            REG_ENTRY(250),
            REG_ENTRY(251),
            REG_ENTRY(252),
            REG_ENTRY(253),
            REG_ENTRY(254),
            REG_ENTRY(255),
            REG_ENTRY(256),
            REG_ENTRY(257),
            REG_ENTRY(258),
            REG_ENTRY(259),
            REG_ENTRY(260),
            REG_ENTRY(261),
            REG_ENTRY(262),
            REG_ENTRY(263),
            REG_ENTRY(264),
            REG_ENTRY(265),
            REG_ENTRY(266),
            REG_ENTRY(267),
            REG_ENTRY(268),
            REG_ENTRY(269),
            REG_ENTRY(270),
            REG_ENTRY(271),
            REG_ENTRY(272),
            REG_ENTRY(273),
            REG_ENTRY(274),
            REG_ENTRY(275),
            REG_ENTRY(276),
            REG_ENTRY(277),
            REG_ENTRY(278),
            REG_ENTRY(279),
            REG_ENTRY(280),
            REG_ENTRY(281),
            REG_ENTRY(282),
            REG_ENTRY(283),
            REG_ENTRY(284),
            REG_ENTRY(285),
            REG_ENTRY(286),
            REG_ENTRY(287),
            REG_ENTRY(288),
            REG_ENTRY(289),
            REG_ENTRY(290),
            REG_ENTRY(291),
            REG_ENTRY(292),
            REG_ENTRY(293),
            REG_ENTRY(294),
            REG_ENTRY(295),
            REG_ENTRY(296),
            REG_ENTRY(297),
            REG_ENTRY(298),
            REG_ENTRY(299),
            REG_ENTRY(300),
            REG_ENTRY(301),
            REG_ENTRY(302),
            REG_ENTRY(303),
            REG_ENTRY(304),
            REG_ENTRY(305),
            REG_ENTRY(306),
            REG_ENTRY(307),
            REG_ENTRY(308),
            REG_ENTRY(309),
            REG_ENTRY(310),
            REG_ENTRY(311),
            REG_ENTRY(312),
            REG_ENTRY(313),
            REG_ENTRY(314),
            REG_ENTRY(315),
            REG_ENTRY(316),
            REG_ENTRY(317),
            REG_ENTRY(318),
            REG_ENTRY(319),
            REG_ENTRY(320),
            REG_ENTRY(321),
            REG_ENTRY(322),
            REG_ENTRY(323),
            REG_ENTRY(324),
            REG_ENTRY(325),
            REG_ENTRY(326),
            REG_ENTRY(327),
            REG_ENTRY(328),
            REG_ENTRY(329),
            REG_ENTRY(330),
            REG_ENTRY(331),
            REG_ENTRY(332),
            REG_ENTRY(333),
            REG_ENTRY(334),
            REG_ENTRY(335),
            REG_ENTRY(336),
            REG_ENTRY(337),
            REG_ENTRY(338),
            REG_ENTRY(339),
            REG_ENTRY(340),
            REG_ENTRY(341),
            REG_ENTRY(342),
            REG_ENTRY(343),
            REG_ENTRY(344),
            REG_ENTRY(345),
            REG_ENTRY(346),
            REG_ENTRY(347),
            REG_ENTRY(348),
            REG_ENTRY(349),
            REG_ENTRY(350),
            REG_ENTRY(351),
            REG_ENTRY(352),
            REG_ENTRY(353),
            REG_ENTRY(354),
            REG_ENTRY(355),
            REG_ENTRY(356),
            REG_ENTRY(357),
            REG_ENTRY(358),
            REG_ENTRY(359),
            REG_ENTRY(360),
            REG_ENTRY(361),
            REG_ENTRY(362),
            REG_ENTRY(363),
            REG_ENTRY(364),
            REG_ENTRY(365),
            REG_ENTRY(366),
            REG_ENTRY(367),
            REG_ENTRY(368),
            REG_ENTRY(369),
            REG_ENTRY(370),
            REG_ENTRY(371),
            REG_ENTRY(372),
            REG_ENTRY(373),
            REG_ENTRY(374),
            REG_ENTRY(375),
            REG_ENTRY(376),
            REG_ENTRY(377),
            REG_ENTRY(378),
            REG_ENTRY(379),
            REG_ENTRY(380),
            REG_ENTRY(381),
            REG_ENTRY(382),
            REG_ENTRY(383),
            REG_ENTRY(384),
            REG_ENTRY(385),
            REG_ENTRY(386),
            REG_ENTRY(387),
            REG_ENTRY(388),
            REG_ENTRY(389),
            REG_ENTRY(390),
            REG_ENTRY(391),
            REG_ENTRY(392),
            REG_ENTRY(393),
            REG_ENTRY(394),
            REG_ENTRY(395),
            REG_ENTRY(396),
            REG_ENTRY(397),
            REG_ENTRY(398),
            REG_ENTRY(399),
            REG_ENTRY(400),
            REG_ENTRY(401),
            REG_ENTRY(402),
            REG_ENTRY(403),
            REG_ENTRY(404),
            REG_ENTRY(405),
            REG_ENTRY(406),
            REG_ENTRY(407),
            REG_ENTRY(408),
            REG_ENTRY(409),
            REG_ENTRY(410),
            REG_ENTRY(411),
            REG_ENTRY(412),
            REG_ENTRY(413),
            REG_ENTRY(414),
            REG_ENTRY(415),
            REG_ENTRY(416),
            REG_ENTRY(417),
            REG_ENTRY(418),
            REG_ENTRY(419),
            REG_ENTRY(420),
            REG_ENTRY(421),
            REG_ENTRY(422),
            REG_ENTRY(423),
            REG_ENTRY(424),
            REG_ENTRY(425),
            REG_ENTRY(426),
            REG_ENTRY(427),
            REG_ENTRY(428),
            REG_ENTRY(429),
            REG_ENTRY(430),
            REG_ENTRY(431),
            REG_ENTRY(432),
            REG_ENTRY(433),
            REG_ENTRY(434),
            REG_ENTRY(435),
            REG_ENTRY(436),
            REG_ENTRY(437),
            REG_ENTRY(438),
            REG_ENTRY(439),
            REG_ENTRY(440),
            REG_ENTRY(441),
            REG_ENTRY(442),
            REG_ENTRY(443),
            REG_ENTRY(444),
            REG_ENTRY(445),
            REG_ENTRY(446),
            REG_ENTRY(447),
            REG_ENTRY(448),
            REG_ENTRY(449),
            REG_ENTRY(450),
            REG_ENTRY(451),
            REG_ENTRY(452),
            REG_ENTRY(453),
            REG_ENTRY(454),
            REG_ENTRY(455),
            REG_ENTRY(456),
            REG_ENTRY(457),
            REG_ENTRY(458),
            REG_ENTRY(459),
            REG_ENTRY(460),
            REG_ENTRY(461),
            REG_ENTRY(462),
            REG_ENTRY(463),
            REG_ENTRY(464),
            REG_ENTRY(465),
            REG_ENTRY(466),
            REG_ENTRY(467),
            REG_ENTRY(468),
            REG_ENTRY(469),
            REG_ENTRY(470),
            REG_ENTRY(471),
            REG_ENTRY(472),
            REG_ENTRY(473),
            REG_ENTRY(474),
            REG_ENTRY(475),
            REG_ENTRY(476),
            REG_ENTRY(477),
            REG_ENTRY(478),
            REG_ENTRY(479),
            REG_ENTRY(480),
            REG_ENTRY(481),
            REG_ENTRY(482),
            REG_ENTRY(483),
            REG_ENTRY(484),
            REG_ENTRY(485),
            REG_ENTRY(486),
            REG_ENTRY(487),
            REG_ENTRY(488),
            REG_ENTRY(489),
            REG_ENTRY(490),
            REG_ENTRY(491),
            REG_ENTRY(492),
            REG_ENTRY(493),
            REG_ENTRY(494),
            REG_ENTRY(495),
            REG_ENTRY(496),
            REG_ENTRY(497),
            REG_ENTRY(498),
            REG_ENTRY(499),
            REG_ENTRY(500),
            REG_ENTRY(501),
            REG_ENTRY(502),
            REG_ENTRY(503),
            REG_ENTRY(504),
            REG_ENTRY(505),
            REG_ENTRY(506),
            REG_ENTRY(507),
            REG_ENTRY(508),
            REG_ENTRY(509),
            REG_ENTRY(510),
            REG_ENTRY(511),
            REG_ENTRY(512),
            REG_ENTRY(513),
            REG_ENTRY(514),
            REG_ENTRY(515),
            REG_ENTRY(516),
            REG_ENTRY(517),
            REG_ENTRY(518),
            REG_ENTRY(519),
            REG_ENTRY(520),
            REG_ENTRY(521),
            REG_ENTRY(522),
            REG_ENTRY(523),
            REG_ENTRY(524),
            REG_ENTRY(525),
            REG_ENTRY(526),
            REG_ENTRY(527),
            REG_ENTRY(528),
            REG_ENTRY(529),
            REG_ENTRY(530),
            REG_ENTRY(531),
            REG_ENTRY(532),
            REG_ENTRY(533),
            REG_ENTRY(534),
            REG_ENTRY(535),
            REG_ENTRY(536),
            REG_ENTRY(537),
            REG_ENTRY(538),
            REG_ENTRY(539),
            REG_ENTRY(540),
            REG_ENTRY(541),
            REG_ENTRY(542),
            REG_ENTRY(543),
            REG_ENTRY(544),
            REG_ENTRY(545),
            REG_ENTRY(546),
            REG_ENTRY(547),
            REG_ENTRY(548),
            REG_ENTRY(549),
            REG_ENTRY(550),
            REG_ENTRY(551),
            REG_ENTRY(552),
            REG_ENTRY(553),
            REG_ENTRY(554),
            REG_ENTRY(555),
            REG_ENTRY(556),
            REG_ENTRY(557),
            REG_ENTRY(558),
            REG_ENTRY(559),
            REG_ENTRY(560),
            REG_ENTRY(561),
            REG_ENTRY(562),
            REG_ENTRY(563),
            REG_ENTRY(564),
            REG_ENTRY(565),
            REG_ENTRY(566),
            REG_ENTRY(567),
            REG_ENTRY(568),
            REG_ENTRY(569),
            REG_ENTRY(570),
            REG_ENTRY(571),
            REG_ENTRY(572),
            REG_ENTRY(573),
            REG_ENTRY(574),
            REG_ENTRY(575),
            REG_ENTRY(576),
            REG_ENTRY(577),
            REG_ENTRY(578),
            REG_ENTRY(579),
            REG_ENTRY(580),
            REG_ENTRY(581),
            REG_ENTRY(582),
            REG_ENTRY(583),
            REG_ENTRY(584),
            REG_ENTRY(585),
            REG_ENTRY(586),
            REG_ENTRY(587),
            REG_ENTRY(588),
            REG_ENTRY(589),
            REG_ENTRY(590),
            REG_ENTRY(591),
            REG_ENTRY(592),
            REG_ENTRY(593),
            REG_ENTRY(594),
            REG_ENTRY(595),
            REG_ENTRY(596),
            REG_ENTRY(597),
            REG_ENTRY(598),
            REG_ENTRY(599),
            REG_ENTRY(600),
            REG_ENTRY(601),
            REG_ENTRY(602),
            REG_ENTRY(603),
            REG_ENTRY(604),
            REG_ENTRY(605),
            REG_ENTRY(606),
            REG_ENTRY(607),
            REG_ENTRY(608),
            REG_ENTRY(609),
            REG_ENTRY(610),
            REG_ENTRY(611),
            REG_ENTRY(612),
            REG_ENTRY(613),
            REG_ENTRY(614),
            REG_ENTRY(615),
            REG_ENTRY(616),
            REG_ENTRY(617),
            REG_ENTRY(618),
            REG_ENTRY(619),
            REG_ENTRY(620),
            REG_ENTRY(621),
            REG_ENTRY(622),
            REG_ENTRY(623),
            REG_ENTRY(624),
            REG_ENTRY(625),
            REG_ENTRY(626),
            REG_ENTRY(627),
            REG_ENTRY(628),
            REG_ENTRY(629),
            REG_ENTRY(630),
            REG_ENTRY(631),
            REG_ENTRY(632),
            REG_ENTRY(633),
            REG_ENTRY(634),
            REG_ENTRY(635),
            REG_ENTRY(636),
            REG_ENTRY(637),
            REG_ENTRY(638),
            REG_ENTRY(639),
            REG_ENTRY(640),
            REG_ENTRY(641),
            REG_ENTRY(642),
            REG_ENTRY(643),
            REG_ENTRY(644),
            REG_ENTRY(645),
            REG_ENTRY(646),
            REG_ENTRY(647),
            REG_ENTRY(648),
            REG_ENTRY(649),
            REG_ENTRY(650),
            REG_ENTRY(651),
            REG_ENTRY(652),
            REG_ENTRY(653),
            REG_ENTRY(654),
            REG_ENTRY(655),
            REG_ENTRY(656),
            REG_ENTRY(657),
            REG_ENTRY(658),
            REG_ENTRY(659),
            REG_ENTRY(660),
            REG_ENTRY(661),
            REG_ENTRY(662),
            REG_ENTRY(663),
            REG_ENTRY(664),
            REG_ENTRY(665),
            REG_ENTRY(666),
            REG_ENTRY(667),
            REG_ENTRY(668),
            REG_ENTRY(669),
            REG_ENTRY(670),
            REG_ENTRY(671),
            REG_ENTRY(672),
            REG_ENTRY(673),
            REG_ENTRY(674),
            REG_ENTRY(675),
            REG_ENTRY(676),
            REG_ENTRY(677),
            REG_ENTRY(678),
            REG_ENTRY(679),
            REG_ENTRY(680),
            REG_ENTRY(681),
            REG_ENTRY(682),
            REG_ENTRY(683),
            REG_ENTRY(684),
            REG_ENTRY(685),
            REG_ENTRY(686),
            REG_ENTRY(687),
            REG_ENTRY(688),
            REG_ENTRY(689),
            REG_ENTRY(690),
            REG_ENTRY(691),
            REG_ENTRY(692),
            REG_ENTRY(693),
            REG_ENTRY(694),
            REG_ENTRY(695),
            REG_ENTRY(696),
            REG_ENTRY(697),
            REG_ENTRY(698),
            REG_ENTRY(699),
            REG_ENTRY(700),
            REG_ENTRY(701),
            REG_ENTRY(702),
            REG_ENTRY(703),
            REG_ENTRY(704),
            REG_ENTRY(705),
            REG_ENTRY(706),
            REG_ENTRY(707),
            REG_ENTRY(708),
            REG_ENTRY(709),
            REG_ENTRY(710),
            REG_ENTRY(711),
            REG_ENTRY(712),
            REG_ENTRY(713),
            REG_ENTRY(714),
            REG_ENTRY(715),
            REG_ENTRY(716),
            REG_ENTRY(717),
            REG_ENTRY(718),
            REG_ENTRY(719),
            REG_ENTRY(720),
            REG_ENTRY(721),
            REG_ENTRY(722),
            REG_ENTRY(723),
            REG_ENTRY(724),
            REG_ENTRY(725),
            REG_ENTRY(726),
            REG_ENTRY(727),
            REG_ENTRY(728),
            REG_ENTRY(729),
            REG_ENTRY(730),
            REG_ENTRY(731),
            REG_ENTRY(732),
            REG_ENTRY(733),
            REG_ENTRY(734),
            REG_ENTRY(735),
            REG_ENTRY(736),
            REG_ENTRY(737),
            REG_ENTRY(738),
            REG_ENTRY(739),
            REG_ENTRY(740),
            REG_ENTRY(741),
            REG_ENTRY(742),
            REG_ENTRY(743),
            REG_ENTRY(744),
            REG_ENTRY(745),
            REG_ENTRY(746),
            REG_ENTRY(747),
            REG_ENTRY(748),
            REG_ENTRY(749),
            REG_ENTRY(750),
            REG_ENTRY(751),
            REG_ENTRY(752),
            REG_ENTRY(753),
            REG_ENTRY(754),
            REG_ENTRY(755),
            REG_ENTRY(756),
            REG_ENTRY(757),
            REG_ENTRY(758),
            REG_ENTRY(759),
            REG_ENTRY(760),
            REG_ENTRY(761),
            REG_ENTRY(762),
            REG_ENTRY(763),
            REG_ENTRY(764),
            REG_ENTRY(765),
            REG_ENTRY(766),
            REG_ENTRY(767),
            REG_ENTRY(768),
            REG_ENTRY(769),
            REG_ENTRY(770),
            REG_ENTRY(771),
            REG_ENTRY(772),
            REG_ENTRY(773),
            REG_ENTRY(774),
            REG_ENTRY(775),
            REG_ENTRY(776),
            REG_ENTRY(777),
            REG_ENTRY(778),
            REG_ENTRY(779),
            REG_ENTRY(780),
            REG_ENTRY(781),
            REG_ENTRY(782),
            REG_ENTRY(783),
            REG_ENTRY(784),
            REG_ENTRY(785),
            REG_ENTRY(786),
            REG_ENTRY(787),
            REG_ENTRY(788),
            REG_ENTRY(789),
            REG_ENTRY(790),
            REG_ENTRY(791),
            REG_ENTRY(792),
            REG_ENTRY(793),
            REG_ENTRY(794),
            REG_ENTRY(795),
            REG_ENTRY(796),
            REG_ENTRY(797),
            REG_ENTRY(798),
            REG_ENTRY(799),
            REG_ENTRY(800),
            REG_ENTRY(801),
            REG_ENTRY(802),
            REG_ENTRY(803),
            REG_ENTRY(804),
            REG_ENTRY(805),
            REG_ENTRY(806),
            REG_ENTRY(807),
            REG_ENTRY(808),
            REG_ENTRY(809),
            REG_ENTRY(810),
            REG_ENTRY(811),
            REG_ENTRY(812),
            REG_ENTRY(813),
            REG_ENTRY(814),
            REG_ENTRY(815),
            REG_ENTRY(816),
            REG_ENTRY(817),
            REG_ENTRY(818),
            REG_ENTRY(819),
            REG_ENTRY(820),
            REG_ENTRY(821),
            REG_ENTRY(822),
            REG_ENTRY(823),
            REG_ENTRY(824),
            REG_ENTRY(825),
            REG_ENTRY(826),
            REG_ENTRY(827),
            REG_ENTRY(828),
            REG_ENTRY(829),
            REG_ENTRY(830),
            REG_ENTRY(831),
            REG_ENTRY(832),
            REG_ENTRY(833),
            REG_ENTRY(834),
            REG_ENTRY(835),
            REG_ENTRY(836),
            REG_ENTRY(837),
            REG_ENTRY(838),
            REG_ENTRY(839),
            REG_ENTRY(840),
            REG_ENTRY(841),
            REG_ENTRY(842),
            REG_ENTRY(843),
            REG_ENTRY(844),
            REG_ENTRY(845),
            REG_ENTRY(846),
            REG_ENTRY(847),
            REG_ENTRY(848),
            REG_ENTRY(849),
            REG_ENTRY(850),
            REG_ENTRY(851),
            REG_ENTRY(852),
            REG_ENTRY(853),
            REG_ENTRY(854),
            REG_ENTRY(855),
            REG_ENTRY(856),
            REG_ENTRY(857),
            REG_ENTRY(858),
            REG_ENTRY(859),
            REG_ENTRY(860),
            REG_ENTRY(861),
            REG_ENTRY(862),
            REG_ENTRY(863),
            REG_ENTRY(864),
            REG_ENTRY(865),
            REG_ENTRY(866),
            REG_ENTRY(867),
            REG_ENTRY(868),
            REG_ENTRY(869),
            REG_ENTRY(870),
            REG_ENTRY(871),
            REG_ENTRY(872),
            REG_ENTRY(873),
            REG_ENTRY(874),
            REG_ENTRY(875),
            REG_ENTRY(876),
            REG_ENTRY(877),
            REG_ENTRY(878),
            REG_ENTRY(879),
            REG_ENTRY(880),
            REG_ENTRY(881),
            REG_ENTRY(882),
            REG_ENTRY(883),
            REG_ENTRY(884),
            REG_ENTRY(885),
            REG_ENTRY(886),
            REG_ENTRY(887),
            REG_ENTRY(888),
            REG_ENTRY(889),
            REG_ENTRY(890),
            REG_ENTRY(891),
            REG_ENTRY(892),
            REG_ENTRY(893),
            REG_ENTRY(894),
            REG_ENTRY(895),
            REG_ENTRY(896),
            REG_ENTRY(897),
            REG_ENTRY(898),
            REG_ENTRY(899),
            REG_ENTRY(900),
            REG_ENTRY(901),
            REG_ENTRY(902),
            REG_ENTRY(903),
            REG_ENTRY(904),
            REG_ENTRY(905),
            REG_ENTRY(906),
            REG_ENTRY(907),
            REG_ENTRY(908),
            REG_ENTRY(909),
            REG_ENTRY(910),
            REG_ENTRY(911),
            REG_ENTRY(912),
            REG_ENTRY(913),
            REG_ENTRY(914),
            REG_ENTRY(915),
            REG_ENTRY(916),
            REG_ENTRY(917),
            REG_ENTRY(918),
            REG_ENTRY(919),
            REG_ENTRY(920),
            REG_ENTRY(921),
            REG_ENTRY(922),
            REG_ENTRY(923),
            REG_ENTRY(924),
            REG_ENTRY(925),
            REG_ENTRY(926),
            REG_ENTRY(927),
            REG_ENTRY(928),
            REG_ENTRY(929),
            REG_ENTRY(930),
            REG_ENTRY(931),
            REG_ENTRY(932),
            REG_ENTRY(933),
            REG_ENTRY(934),
            REG_ENTRY(935),
            REG_ENTRY(936),
            REG_ENTRY(937),
            REG_ENTRY(938),
            REG_ENTRY(939),
            REG_ENTRY(940),
            REG_ENTRY(941),
            REG_ENTRY(942),
            REG_ENTRY(943),
            REG_ENTRY(944),
            REG_ENTRY(945),
            REG_ENTRY(946),
            REG_ENTRY(947),
            REG_ENTRY(948),
            REG_ENTRY(949),
            REG_ENTRY(950),
            REG_ENTRY(951),
            REG_ENTRY(952),
            REG_ENTRY(953),
            REG_ENTRY(954),
            REG_ENTRY(955),
            REG_ENTRY(956),
            REG_ENTRY(957),
            REG_ENTRY(958),
            REG_ENTRY(959),
            REG_ENTRY(960),
            REG_ENTRY(961),
            REG_ENTRY(962),
            REG_ENTRY(963),
            REG_ENTRY(964),
            REG_ENTRY(965),
            REG_ENTRY(966),
            REG_ENTRY(967),
            REG_ENTRY(968),
            REG_ENTRY(969),
            REG_ENTRY(970),
            REG_ENTRY(971),
            REG_ENTRY(972),
            REG_ENTRY(973),
            REG_ENTRY(974),
            REG_ENTRY(975),
            REG_ENTRY(976),
            REG_ENTRY(977),
            REG_ENTRY(978),
            REG_ENTRY(979),
            REG_ENTRY(980),
            REG_ENTRY(981),
            REG_ENTRY(982),
            REG_ENTRY(983),
            REG_ENTRY(984),
            REG_ENTRY(985),
            REG_ENTRY(986),
            REG_ENTRY(987),
            REG_ENTRY(988),
            REG_ENTRY(989),
            REG_ENTRY(990),
            REG_ENTRY(991),
            REG_ENTRY(992),
            REG_ENTRY(993),
            REG_ENTRY(994),
            REG_ENTRY(995),
            REG_ENTRY(996),
            REG_ENTRY(997),
            REG_ENTRY(998),
            REG_ENTRY(999),
            REG_ENTRY(1000),
            REG_ENTRY(1001),
            REG_ENTRY(1002),
            REG_ENTRY(1003),
            REG_ENTRY(1004),
            REG_ENTRY(1005),
            REG_ENTRY(1006),
            REG_ENTRY(1007),
            REG_ENTRY(1008),
            REG_ENTRY(1009),
            REG_ENTRY(1010),
            REG_ENTRY(1011),
            REG_ENTRY(1012),
            REG_ENTRY(1013),
            REG_ENTRY(1014),
            REG_ENTRY(1015),
            REG_ENTRY(1016),
            REG_ENTRY(1017),
            REG_ENTRY(1018),
            REG_ENTRY(1019),
            REG_ENTRY(1020),
            REG_ENTRY(1021),
            REG_ENTRY(1022),
            REG_ENTRY(1023),
            REG_ENTRY(1024),
            REG_ENTRY(1025),
            REG_ENTRY(1026),
            REG_ENTRY(1027),
            REG_ENTRY(1028),
            REG_ENTRY(1029),
            REG_ENTRY(1030),
            REG_ENTRY(1031),
            REG_ENTRY(1032),
            REG_ENTRY(1033),
            REG_ENTRY(1034),
            REG_ENTRY(1035),
            REG_ENTRY(1036),
            REG_ENTRY(1037),
            REG_ENTRY(1038),
            REG_ENTRY(1039),
            REG_ENTRY(1040),
            REG_ENTRY(1041),
            REG_ENTRY(1042),
            REG_ENTRY(1043),
            REG_ENTRY(1044),
            REG_ENTRY(1045),
            REG_ENTRY(1046),
            REG_ENTRY(1047),
            REG_ENTRY(1048),
            REG_ENTRY(1049),
            REG_ENTRY(1050),
            REG_ENTRY(1051),
            REG_ENTRY(1052),
            REG_ENTRY(1053),
            REG_ENTRY(1054),
            REG_ENTRY(1055),
            REG_ENTRY(1056),
            REG_ENTRY(1057),
            REG_ENTRY(1058),
            REG_ENTRY(1059),
            REG_ENTRY(1060),
            REG_ENTRY(1061),
            REG_ENTRY(1062),
            REG_ENTRY(1063),
            REG_ENTRY(1064),
            REG_ENTRY(1065),
            REG_ENTRY(1066),
            REG_ENTRY(1067),
            REG_ENTRY(1068),
            REG_ENTRY(1069),
            REG_ENTRY(1070),
            REG_ENTRY(1071),
            REG_ENTRY(1072),
            REG_ENTRY(1073),
            REG_ENTRY(1074),
            REG_ENTRY(1075),
            REG_ENTRY(1076),
            REG_ENTRY(1077),
            REG_ENTRY(1078),
            REG_ENTRY(1079),
            REG_ENTRY(1080),
            REG_ENTRY(1081),
            REG_ENTRY(1082),
            REG_ENTRY(1083),
            REG_ENTRY(1084),
            REG_ENTRY(1085),
            REG_ENTRY(1086),
            REG_ENTRY(1087),
            REG_ENTRY(1088),
            REG_ENTRY(1089),
            REG_ENTRY(1090),
            REG_ENTRY(1091),
            REG_ENTRY(1092),
            REG_ENTRY(1093),
            REG_ENTRY(1094),
            REG_ENTRY(1095),
            REG_ENTRY(1096),
            REG_ENTRY(1097),
            REG_ENTRY(1098),
            REG_ENTRY(1099),
            REG_ENTRY(1100),
            REG_ENTRY(1101),
            REG_ENTRY(1102),
            REG_ENTRY(1103),
            REG_ENTRY(1104),
            REG_ENTRY(1105),
            REG_ENTRY(1106),
            REG_ENTRY(1107),
            REG_ENTRY(1108),
            REG_ENTRY(1109),
            REG_ENTRY(1110),
            REG_ENTRY(1111),
            REG_ENTRY(1112),
            REG_ENTRY(1113),
            REG_ENTRY(1114),
            REG_ENTRY(1115),
            REG_ENTRY(1116),
            REG_ENTRY(1117),
            REG_ENTRY(1118),
            REG_ENTRY(1119),
            REG_ENTRY(1120),
            REG_ENTRY(1121),
            REG_ENTRY(1122),
            REG_ENTRY(1123),
            REG_ENTRY(1124),
            REG_ENTRY(1125),
            REG_ENTRY(1126),
            REG_ENTRY(1127),
            REG_ENTRY(1128),
            REG_ENTRY(1129),
            REG_ENTRY(1130),
            REG_ENTRY(1131),
            REG_ENTRY(1132),
            REG_ENTRY(1133),
            REG_ENTRY(1134),
            REG_ENTRY(1135),
            REG_ENTRY(1136),
            REG_ENTRY(1137),
            REG_ENTRY(1138),
            REG_ENTRY(1139),
            REG_ENTRY(1140),
            REG_ENTRY(1141),
            REG_ENTRY(1142),
            REG_ENTRY(1143),
            REG_ENTRY(1144),
            REG_ENTRY(1145),
            REG_ENTRY(1146),
            REG_ENTRY(1147),
            REG_ENTRY(1148),
            REG_ENTRY(1149),
            REG_ENTRY(1150),
            REG_ENTRY(1151),
            REG_ENTRY(1152),
            REG_ENTRY(1153),
            REG_ENTRY(1154),
            REG_ENTRY(1155),
            REG_ENTRY(1156),
            REG_ENTRY(1157),
            REG_ENTRY(1158),
            REG_ENTRY(1159),
            REG_ENTRY(1160),
            REG_ENTRY(1161),
            REG_ENTRY(1162),
            REG_ENTRY(1163),
            REG_ENTRY(1164),
            REG_ENTRY(1165),
            REG_ENTRY(1166),
            REG_ENTRY(1167),
            REG_ENTRY(1168),
            REG_ENTRY(1169),
            REG_ENTRY(1170),
            REG_ENTRY(1171),
            REG_ENTRY(1172),
            REG_ENTRY(1173),
            REG_ENTRY(1174),
            REG_ENTRY(1175),
            REG_ENTRY(1176),
            REG_ENTRY(1177),
            REG_ENTRY(1178),
            REG_ENTRY(1179),
            REG_ENTRY(1180),
            REG_ENTRY(1181),
            REG_ENTRY(1182),
            REG_ENTRY(1183),
            REG_ENTRY(1184),
            REG_ENTRY(1185),
            REG_ENTRY(1186),
            REG_ENTRY(1187),
            REG_ENTRY(1188),
            REG_ENTRY(1189),
            REG_ENTRY(1190),
            REG_ENTRY(1191),
            REG_ENTRY(1192),
            REG_ENTRY(1193),
            REG_ENTRY(1194),
            REG_ENTRY(1195),
            REG_ENTRY(1196),
            REG_ENTRY(1197),
            REG_ENTRY(1198),
            REG_ENTRY(1199),
            REG_ENTRY(1200),
            REG_ENTRY(1201),
            REG_ENTRY(1202),
            REG_ENTRY(1203),
            REG_ENTRY(1204),
            REG_ENTRY(1205),
            REG_ENTRY(1206),
            REG_ENTRY(1207),
            REG_ENTRY(1208),
            REG_ENTRY(1209),
            REG_ENTRY(1210),
            REG_ENTRY(1211),
            REG_ENTRY(1212),
            REG_ENTRY(1213),
            REG_ENTRY(1214),
            REG_ENTRY(1215),
            REG_ENTRY(1216),
            REG_ENTRY(1217),
            REG_ENTRY(1218),
            REG_ENTRY(1219),
            REG_ENTRY(1220),
            REG_ENTRY(1221),
            REG_ENTRY(1222),
            REG_ENTRY(1223),
            REG_ENTRY(1224),
            REG_ENTRY(1225),
            REG_ENTRY(1226),
            REG_ENTRY(1227),
            REG_ENTRY(1228),
            REG_ENTRY(1229),
            REG_ENTRY(1230),
            REG_ENTRY(1231),
            REG_ENTRY(1232),
            REG_ENTRY(1233),
            REG_ENTRY(1234),
            REG_ENTRY(1235),
            REG_ENTRY(1236),
            REG_ENTRY(1237),
            REG_ENTRY(1238),
            REG_ENTRY(1239),
            REG_ENTRY(1240),
            REG_ENTRY(1241),
            REG_ENTRY(1242),
            REG_ENTRY(1243),
            REG_ENTRY(1244),
            REG_ENTRY(1245),
            REG_ENTRY(1246),
            REG_ENTRY(1247),
            REG_ENTRY(1248),
            REG_ENTRY(1249),
            REG_ENTRY(1250),
            REG_ENTRY(1251),
            REG_ENTRY(1252),
            REG_ENTRY(1253),
            REG_ENTRY(1254),
            REG_ENTRY(1255),
            REG_ENTRY(1256),
            REG_ENTRY(1257),
            REG_ENTRY(1258),
            REG_ENTRY(1259),
            REG_ENTRY(1260),
            REG_ENTRY(1261),
            REG_ENTRY(1262),
            REG_ENTRY(1263),
            REG_ENTRY(1264),
            REG_ENTRY(1265),
            REG_ENTRY(1266),
            REG_ENTRY(1267),
            REG_ENTRY(1268),
            REG_ENTRY(1269),
            REG_ENTRY(1270),
            REG_ENTRY(1271),
            REG_ENTRY(1272),
            REG_ENTRY(1273),
            REG_ENTRY(1274),
            REG_ENTRY(1275),
            REG_ENTRY(1276),
            REG_ENTRY(1277),
            REG_ENTRY(1278),
            REG_ENTRY(1279),
            REG_ENTRY(1280),
            REG_ENTRY(1281),
            REG_ENTRY(1282),
            REG_ENTRY(1283),
            REG_ENTRY(1284),
            REG_ENTRY(1285),
            REG_ENTRY(1286),
            REG_ENTRY(1287),
            REG_ENTRY(1288),
            REG_ENTRY(1289),
            REG_ENTRY(1290),
            REG_ENTRY(1291),
            REG_ENTRY(1292),
            REG_ENTRY(1293),
            REG_ENTRY(1294),
            REG_ENTRY(1295),
            REG_ENTRY(1296),
            REG_ENTRY(1297),
            REG_ENTRY(1298),
            REG_ENTRY(1299),
            REG_ENTRY(1300),
            REG_ENTRY(1301),
            REG_ENTRY(1302),
            REG_ENTRY(1303),
            REG_ENTRY(1304),
            REG_ENTRY(1305),
            REG_ENTRY(1306),
            REG_ENTRY(1307),
            REG_ENTRY(1308),
            REG_ENTRY(1309),
            REG_ENTRY(1310),
            REG_ENTRY(1311),
            REG_ENTRY(1312),
            REG_ENTRY(1313),
            REG_ENTRY(1314),
            REG_ENTRY(1315),
            REG_ENTRY(1316),
            REG_ENTRY(1317),
            REG_ENTRY(1318),
            REG_ENTRY(1319),
            REG_ENTRY(1320),
            REG_ENTRY(1321),
            REG_ENTRY(1322),
            REG_ENTRY(1323),
            REG_ENTRY(1324),
            REG_ENTRY(1325),
            REG_ENTRY(1326),
            REG_ENTRY(1327),
            REG_ENTRY(1328),
            REG_ENTRY(1329),
            REG_ENTRY(1330),
            REG_ENTRY(1331),
            REG_ENTRY(1332),
            REG_ENTRY(1333),
            REG_ENTRY(1334),
            REG_ENTRY(1335),
            REG_ENTRY(1336),
            REG_ENTRY(1337),
            REG_ENTRY(1338),
            REG_ENTRY(1339),
            REG_ENTRY(1340),
            REG_ENTRY(1341),
            REG_ENTRY(1342),
            REG_ENTRY(1343),
            REG_ENTRY(1344),
            REG_ENTRY(1345),
            REG_ENTRY(1346),
            REG_ENTRY(1347),
            REG_ENTRY(1348),
            REG_ENTRY(1349),
            REG_ENTRY(1350),
            REG_ENTRY(1351),
            REG_ENTRY(1352),
            REG_ENTRY(1353),
            REG_ENTRY(1354),
            REG_ENTRY(1355),
            REG_ENTRY(1356),
            REG_ENTRY(1357),
            REG_ENTRY(1358),
            REG_ENTRY(1359),
            REG_ENTRY(1360),
            REG_ENTRY(1361),
            REG_ENTRY(1362),
            REG_ENTRY(1363),
            REG_ENTRY(1364),
            REG_ENTRY(1365),
            REG_ENTRY(1366),
            REG_ENTRY(1367),
            REG_ENTRY(1368),
            REG_ENTRY(1369),
            REG_ENTRY(1370),
            REG_ENTRY(1371),
            REG_ENTRY(1372),
            REG_ENTRY(1373),
            REG_ENTRY(1374),
            REG_ENTRY(1375),
            REG_ENTRY(1376),
            REG_ENTRY(1377),
            REG_ENTRY(1378),
            REG_ENTRY(1379),
            REG_ENTRY(1380),
            REG_ENTRY(1381),
            REG_ENTRY(1382),
            REG_ENTRY(1383),
            REG_ENTRY(1384),
            REG_ENTRY(1385),
            REG_ENTRY(1386),
            REG_ENTRY(1387),
            REG_ENTRY(1388),
            REG_ENTRY(1389),
            REG_ENTRY(1390),
            REG_ENTRY(1391),
            REG_ENTRY(1392),
            REG_ENTRY(1393),
            REG_ENTRY(1394),
            REG_ENTRY(1395),
            REG_ENTRY(1396),
            REG_ENTRY(1397),
            REG_ENTRY(1398),
            REG_ENTRY(1399),
            REG_ENTRY(1400),
            REG_ENTRY(1401),
            REG_ENTRY(1402),
            REG_ENTRY(1403),
            REG_ENTRY(1404),
            REG_ENTRY(1405),
            REG_ENTRY(1406),
            REG_ENTRY(1407),
            REG_ENTRY(1408),
            REG_ENTRY(1409),
            REG_ENTRY(1410),
            REG_ENTRY(1411),
            REG_ENTRY(1412),
            REG_ENTRY(1413),
            REG_ENTRY(1414),
            REG_ENTRY(1415),
            REG_ENTRY(1416),
            REG_ENTRY(1417),
            REG_ENTRY(1418),
            REG_ENTRY(1419),
            REG_ENTRY(1420),
            REG_ENTRY(1421),
            REG_ENTRY(1422),
            REG_ENTRY(1423),
            REG_ENTRY(1424),
            REG_ENTRY(1425),
            REG_ENTRY(1426),
            REG_ENTRY(1427),
            REG_ENTRY(1428),
            REG_ENTRY(1429),
            REG_ENTRY(1430),
            REG_ENTRY(1431),
            REG_ENTRY(1432),
            REG_ENTRY(1433),
            REG_ENTRY(1434),
            REG_ENTRY(1435),
            REG_ENTRY(1436),
            REG_ENTRY(1437),
            REG_ENTRY(1438),
            REG_ENTRY(1439),
            REG_ENTRY(1440),
            REG_ENTRY(1441),
            REG_ENTRY(1442),
            REG_ENTRY(1443),
            REG_ENTRY(1444),
            REG_ENTRY(1445),
            REG_ENTRY(1446),
            REG_ENTRY(1447),
            REG_ENTRY(1448),
            REG_ENTRY(1449),
            REG_ENTRY(1450),
            REG_ENTRY(1451),
            REG_ENTRY(1452),
            REG_ENTRY(1453),
            REG_ENTRY(1454),
            REG_ENTRY(1455),
            REG_ENTRY(1456),
            REG_ENTRY(1457),
            REG_ENTRY(1458),
            REG_ENTRY(1459),
            REG_ENTRY(1460),
            REG_ENTRY(1461),
            REG_ENTRY(1462),
            REG_ENTRY(1463),
            REG_ENTRY(1464),
            REG_ENTRY(1465),
            REG_ENTRY(1466),
            REG_ENTRY(1467),
            REG_ENTRY(1468),
            REG_ENTRY(1469),
            REG_ENTRY(1470),
            REG_ENTRY(1471),
            REG_ENTRY(1472),
            REG_ENTRY(1473),
            REG_ENTRY(1474),
            REG_ENTRY(1475),
            REG_ENTRY(1476),
            REG_ENTRY(1477),
            REG_ENTRY(1478),
            REG_ENTRY(1479),
            REG_ENTRY(1480),
            REG_ENTRY(1481),
            REG_ENTRY(1482),
            REG_ENTRY(1483),
            REG_ENTRY(1484),
            REG_ENTRY(1485),
            REG_ENTRY(1486),
            REG_ENTRY(1487),
            REG_ENTRY(1488),
            REG_ENTRY(1489),
            REG_ENTRY(1490),
            REG_ENTRY(1491),
            REG_ENTRY(1492),
            REG_ENTRY(1493),
            REG_ENTRY(1494),
            REG_ENTRY(1495),
            REG_ENTRY(1496),
            REG_ENTRY(1497),
            REG_ENTRY(1498),
            REG_ENTRY(1499),
            REG_ENTRY(1500),
            REG_ENTRY(1501),
            REG_ENTRY(1502),
            REG_ENTRY(1503),
            REG_ENTRY(1504),
            REG_ENTRY(1505),
            REG_ENTRY(1506),
            REG_ENTRY(1507),
            REG_ENTRY(1508),
            REG_ENTRY(1509),
            REG_ENTRY(1510),
            REG_ENTRY(1511),
            REG_ENTRY(1512),
            REG_ENTRY(1513),
            REG_ENTRY(1514),
            REG_ENTRY(1515),
            REG_ENTRY(1516),
            REG_ENTRY(1517),
            REG_ENTRY(1518),
            REG_ENTRY(1519),
            REG_ENTRY(1520),
            REG_ENTRY(1521),
            REG_ENTRY(1522),
            REG_ENTRY(1523),
            REG_ENTRY(1524),
            REG_ENTRY(1525),
            REG_ENTRY(1526),
            REG_ENTRY(1527),
            REG_ENTRY(1528),
            REG_ENTRY(1529),
            REG_ENTRY(1530),
            REG_ENTRY(1531),
            REG_ENTRY(1532),
            REG_ENTRY(1533),
            REG_ENTRY(1534),
            REG_ENTRY(1535),
            REG_ENTRY(1536),
            REG_ENTRY(1537),
            REG_ENTRY(1538),
            REG_ENTRY(1539),
            REG_ENTRY(1540),
            REG_ENTRY(1541),
            REG_ENTRY(1542),
            REG_ENTRY(1543),
            REG_ENTRY(1544),
            REG_ENTRY(1545),
            REG_ENTRY(1546),
            REG_ENTRY(1547),
            REG_ENTRY(1548),
            REG_ENTRY(1549),
            REG_ENTRY(1550),
            REG_ENTRY(1551),
            REG_ENTRY(1552),
            REG_ENTRY(1553),
            REG_ENTRY(1554),
            REG_ENTRY(1555),
            REG_ENTRY(1556),
            REG_ENTRY(1557),
            REG_ENTRY(1558),
            REG_ENTRY(1559),
            REG_ENTRY(1560),
            REG_ENTRY(1561),
            REG_ENTRY(1562),
            REG_ENTRY(1563),
            REG_ENTRY(1564),
            REG_ENTRY(1565),
            REG_ENTRY(1566),
            REG_ENTRY(1567),
            REG_ENTRY(1568),
            REG_ENTRY(1569),
            REG_ENTRY(1570),
            REG_ENTRY(1571),
            REG_ENTRY(1572),
            REG_ENTRY(1573),
            REG_ENTRY(1574),
            REG_ENTRY(1575),
            REG_ENTRY(1576),
            REG_ENTRY(1577),
            REG_ENTRY(1578),
            REG_ENTRY(1579),
            REG_ENTRY(1580),
            REG_ENTRY(1581),
            REG_ENTRY(1582),
            REG_ENTRY(1583),
            REG_ENTRY(1584),
            REG_ENTRY(1585),
            REG_ENTRY(1586),
            REG_ENTRY(1587),
            REG_ENTRY(1588),
            REG_ENTRY(1589),
            REG_ENTRY(1590),
            REG_ENTRY(1591),
            REG_ENTRY(1592),
            REG_ENTRY(1593),
            REG_ENTRY(1594),
            REG_ENTRY(1595),
            REG_ENTRY(1596),
            REG_ENTRY(1597),
            REG_ENTRY(1598),
            REG_ENTRY(1599),
            REG_ENTRY(1600),
            REG_ENTRY(1601),
            REG_ENTRY(1602),
            REG_ENTRY(1603),
            REG_ENTRY(1604),
            REG_ENTRY(1605),
            REG_ENTRY(1606),
            REG_ENTRY(1607),
            REG_ENTRY(1608),
            REG_ENTRY(1609),
            REG_ENTRY(1610),
            REG_ENTRY(1611),
            REG_ENTRY(1612),
            REG_ENTRY(1613),
            REG_ENTRY(1614),
            REG_ENTRY(1615),
            REG_ENTRY(1616),
            REG_ENTRY(1617),
            REG_ENTRY(1618),
            REG_ENTRY(1619),
            REG_ENTRY(1620),
            REG_ENTRY(1621),
            REG_ENTRY(1622),
            REG_ENTRY(1623),
            REG_ENTRY(1624),
            REG_ENTRY(1625),
            REG_ENTRY(1626),
            REG_ENTRY(1627),
            REG_ENTRY(1628),
            REG_ENTRY(1629),
            REG_ENTRY(1630),
            REG_ENTRY(1631),
            REG_ENTRY(1632),
            REG_ENTRY(1633),
            REG_ENTRY(1634),
            REG_ENTRY(1635),
            REG_ENTRY(1636),
            REG_ENTRY(1637),
            REG_ENTRY(1638),
            REG_ENTRY(1639),
            REG_ENTRY(1640),
            REG_ENTRY(1641),
            REG_ENTRY(1642),
            REG_ENTRY(1643),
            REG_ENTRY(1644),
            REG_ENTRY(1645),
            REG_ENTRY(1646),
            REG_ENTRY(1647),
            REG_ENTRY(1648),
            REG_ENTRY(1649),
            REG_ENTRY(1650),
            REG_ENTRY(1651),
            REG_ENTRY(1652),
            REG_ENTRY(1653),
            REG_ENTRY(1654),
            REG_ENTRY(1655),
            REG_ENTRY(1656),
            REG_ENTRY(1657),
            REG_ENTRY(1658),
            REG_ENTRY(1659),
            REG_ENTRY(1660),
            REG_ENTRY(1661),
            REG_ENTRY(1662),
            REG_ENTRY(1663),
            REG_ENTRY(1664),
            REG_ENTRY(1665),
            REG_ENTRY(1666),
            REG_ENTRY(1667),
            REG_ENTRY(1668),
            REG_ENTRY(1669),
            REG_ENTRY(1670),
            REG_ENTRY(1671),
            REG_ENTRY(1672),
            REG_ENTRY(1673),
            REG_ENTRY(1674),
            REG_ENTRY(1675),
            REG_ENTRY(1676),
            REG_ENTRY(1677),
            REG_ENTRY(1678),
            REG_ENTRY(1679),
            REG_ENTRY(1680),
            REG_ENTRY(1681),
            REG_ENTRY(1682),
            REG_ENTRY(1683),
            REG_ENTRY(1684),
            REG_ENTRY(1685),
            REG_ENTRY(1686),
            REG_ENTRY(1687),
            REG_ENTRY(1688),
            REG_ENTRY(1689),
            REG_ENTRY(1690),
            REG_ENTRY(1691),
            REG_ENTRY(1692),
            REG_ENTRY(1693),
            REG_ENTRY(1694),
            REG_ENTRY(1695),
            REG_ENTRY(1696),
            REG_ENTRY(1697),
            REG_ENTRY(1698),
            REG_ENTRY(1699),
            REG_ENTRY(1700),
            REG_ENTRY(1701),
            REG_ENTRY(1702),
            REG_ENTRY(1703),
            REG_ENTRY(1704),
            REG_ENTRY(1705),
            REG_ENTRY(1706),
            REG_ENTRY(1707),
            REG_ENTRY(1708),
            REG_ENTRY(1709),
            REG_ENTRY(1710),
            REG_ENTRY(1711),
            REG_ENTRY(1712),
            REG_ENTRY(1713),
            REG_ENTRY(1714),
            REG_ENTRY(1715),
            REG_ENTRY(1716),
            REG_ENTRY(1717),
            REG_ENTRY(1718),
            REG_ENTRY(1719),
            REG_ENTRY(1720),
            REG_ENTRY(1721),
            REG_ENTRY(1722),
            REG_ENTRY(1723),
            REG_ENTRY(1724),
            REG_ENTRY(1725),
            REG_ENTRY(1726),
            REG_ENTRY(1727),
            REG_ENTRY(1728),
            REG_ENTRY(1729),
            REG_ENTRY(1730),
            REG_ENTRY(1731),
            REG_ENTRY(1732),
            REG_ENTRY(1733),
            REG_ENTRY(1734),
            REG_ENTRY(1735),
            REG_ENTRY(1736),
            REG_ENTRY(1737),
            REG_ENTRY(1738),
            REG_ENTRY(1739),
            REG_ENTRY(1740),
            REG_ENTRY(1741),
            REG_ENTRY(1742),
            REG_ENTRY(1743),
            REG_ENTRY(1744),
            REG_ENTRY(1745),
            REG_ENTRY(1746),
            REG_ENTRY(1747),
            REG_ENTRY(1748),
            REG_ENTRY(1749),
            REG_ENTRY(1750),
            REG_ENTRY(1751),
            REG_ENTRY(1752),
            REG_ENTRY(1753),
            REG_ENTRY(1754),
            REG_ENTRY(1755),
            REG_ENTRY(1756),
            REG_ENTRY(1757),
            REG_ENTRY(1758),
            REG_ENTRY(1759),
            REG_ENTRY(1760),
            REG_ENTRY(1761),
            REG_ENTRY(1762),
            REG_ENTRY(1763),
            REG_ENTRY(1764),
            REG_ENTRY(1765),
            REG_ENTRY(1766),
            REG_ENTRY(1767),
            REG_ENTRY(1768),
            REG_ENTRY(1769),
            REG_ENTRY(1770),
            REG_ENTRY(1771),
            REG_ENTRY(1772),
            REG_ENTRY(1773),
            REG_ENTRY(1774),
            REG_ENTRY(1775),
            REG_ENTRY(1776),
            REG_ENTRY(1777),
            REG_ENTRY(1778),
            REG_ENTRY(1779),
            REG_ENTRY(1780),
            REG_ENTRY(1781),
            REG_ENTRY(1782),
            REG_ENTRY(1783),
            REG_ENTRY(1784),
            REG_ENTRY(1785),
            REG_ENTRY(1786),
            REG_ENTRY(1787),
            REG_ENTRY(1788),
            REG_ENTRY(1789),
            REG_ENTRY(1790),
            REG_ENTRY(1791),
            REG_ENTRY(1792),
            REG_ENTRY(1793),
            REG_ENTRY(1794),
            REG_ENTRY(1795),
            REG_ENTRY(1796),
            REG_ENTRY(1797),
            REG_ENTRY(1798),
            REG_ENTRY(1799),
            REG_ENTRY(1800),
            REG_ENTRY(1801),
            REG_ENTRY(1802),
            REG_ENTRY(1803),
            REG_ENTRY(1804),
            REG_ENTRY(1805),
            REG_ENTRY(1806),
            REG_ENTRY(1807),
            REG_ENTRY(1808),
            REG_ENTRY(1809),
            REG_ENTRY(1810),
            REG_ENTRY(1811),
            REG_ENTRY(1812),
            REG_ENTRY(1813),
            REG_ENTRY(1814),
            REG_ENTRY(1815),
            REG_ENTRY(1816),
            REG_ENTRY(1817),
            REG_ENTRY(1818),
            REG_ENTRY(1819),
            REG_ENTRY(1820),
            REG_ENTRY(1821),
            REG_ENTRY(1822),
            REG_ENTRY(1823),
            REG_ENTRY(1824),
            REG_ENTRY(1825),
            REG_ENTRY(1826),
            REG_ENTRY(1827),
            REG_ENTRY(1828),
            REG_ENTRY(1829),
            REG_ENTRY(1830),
            REG_ENTRY(1831),
            REG_ENTRY(1832),
            REG_ENTRY(1833),
            REG_ENTRY(1834),
            REG_ENTRY(1835),
            REG_ENTRY(1836),
            REG_ENTRY(1837),
            REG_ENTRY(1838),
            REG_ENTRY(1839),
            REG_ENTRY(1840),
            REG_ENTRY(1841),
            REG_ENTRY(1842),
            REG_ENTRY(1843),
            REG_ENTRY(1844),
            REG_ENTRY(1845),
            REG_ENTRY(1846),
            REG_ENTRY(1847),
            REG_ENTRY(1848),
            REG_ENTRY(1849),
            REG_ENTRY(1850),
            REG_ENTRY(1851),
            REG_ENTRY(1852),
            REG_ENTRY(1853),
            REG_ENTRY(1854),
            REG_ENTRY(1855),
            REG_ENTRY(1856),
            REG_ENTRY(1857),
            REG_ENTRY(1858),
            REG_ENTRY(1859),
            REG_ENTRY(1860),
            REG_ENTRY(1861),
            REG_ENTRY(1862),
            REG_ENTRY(1863),
            REG_ENTRY(1864),
            REG_ENTRY(1865),
            REG_ENTRY(1866),
            REG_ENTRY(1867),
            REG_ENTRY(1868),
            REG_ENTRY(1869),
            REG_ENTRY(1870),
            REG_ENTRY(1871),
            REG_ENTRY(1872),
            REG_ENTRY(1873),
            REG_ENTRY(1874),
            REG_ENTRY(1875),
            REG_ENTRY(1876),
            REG_ENTRY(1877),
            REG_ENTRY(1878),
            REG_ENTRY(1879),
            REG_ENTRY(1880),
            REG_ENTRY(1881),
            REG_ENTRY(1882),
            REG_ENTRY(1883),
            REG_ENTRY(1884),
            REG_ENTRY(1885),
            REG_ENTRY(1886),
            REG_ENTRY(1887),
            REG_ENTRY(1888),
            REG_ENTRY(1889),
            REG_ENTRY(1890),
            REG_ENTRY(1891),
            REG_ENTRY(1892),
            REG_ENTRY(1893),
            REG_ENTRY(1894),
            REG_ENTRY(1895),
            REG_ENTRY(1896),
            REG_ENTRY(1897),
            REG_ENTRY(1898),
            REG_ENTRY(1899),
            REG_ENTRY(1900),
            REG_ENTRY(1901),
            REG_ENTRY(1902),
            REG_ENTRY(1903),
            REG_ENTRY(1904),
            REG_ENTRY(1905),
            REG_ENTRY(1906),
            REG_ENTRY(1907),
            REG_ENTRY(1908),
            REG_ENTRY(1909),
            REG_ENTRY(1910),
            REG_ENTRY(1911),
            REG_ENTRY(1912),
            REG_ENTRY(1913),
            REG_ENTRY(1914),
            REG_ENTRY(1915),
            REG_ENTRY(1916),
            REG_ENTRY(1917),
            REG_ENTRY(1918),
            REG_ENTRY(1919),
            REG_ENTRY(1920),
            REG_ENTRY(1921),
            REG_ENTRY(1922),
            REG_ENTRY(1923),
            REG_ENTRY(1924),
            REG_ENTRY(1925),
            REG_ENTRY(1926),
            REG_ENTRY(1927),
            REG_ENTRY(1928),
            REG_ENTRY(1929),
            REG_ENTRY(1930),
            REG_ENTRY(1931),
            REG_ENTRY(1932),
            REG_ENTRY(1933),
            REG_ENTRY(1934),
            REG_ENTRY(1935),
            REG_ENTRY(1936),
            REG_ENTRY(1937),
            REG_ENTRY(1938),
            REG_ENTRY(1939),
            REG_ENTRY(1940),
            REG_ENTRY(1941),
            REG_ENTRY(1942),
            REG_ENTRY(1943),
            REG_ENTRY(1944),
            REG_ENTRY(1945),
            REG_ENTRY(1946),
            REG_ENTRY(1947),
            REG_ENTRY(1948),
            REG_ENTRY(1949),
            REG_ENTRY(1950),
            REG_ENTRY(1951),
            REG_ENTRY(1952),
            REG_ENTRY(1953),
            REG_ENTRY(1954),
            REG_ENTRY(1955),
            REG_ENTRY(1956),
            REG_ENTRY(1957),
            REG_ENTRY(1958),
            REG_ENTRY(1959),
            REG_ENTRY(1960),
            REG_ENTRY(1961),
            REG_ENTRY(1962),
            REG_ENTRY(1963),
            REG_ENTRY(1964),
            REG_ENTRY(1965),
            REG_ENTRY(1966),
            REG_ENTRY(1967),
            REG_ENTRY(1968),
            REG_ENTRY(1969),
            REG_ENTRY(1970),
            REG_ENTRY(1971),
            REG_ENTRY(1972),
            REG_ENTRY(1973),
            REG_ENTRY(1974),
            REG_ENTRY(1975),
            REG_ENTRY(1976),
            REG_ENTRY(1977),
            REG_ENTRY(1978),
            REG_ENTRY(1979),
            REG_ENTRY(1980),
            REG_ENTRY(1981),
            REG_ENTRY(1982),
            REG_ENTRY(1983),
            REG_ENTRY(1984),
            REG_ENTRY(1985),
            REG_ENTRY(1986),
            REG_ENTRY(1987),
            REG_ENTRY(1988),
            REG_ENTRY(1989),
            REG_ENTRY(1990),
            REG_ENTRY(1991),
            REG_ENTRY(1992),
            REG_ENTRY(1993),
            REG_ENTRY(1994),
            REG_ENTRY(1995),
            REG_ENTRY(1996),
            REG_ENTRY(1997),
            REG_ENTRY(1998),
            REG_ENTRY(1999),
            REG_ENTRY(2000),
            REG_ENTRY(2001),
            REG_ENTRY(2002),
            REG_ENTRY(2003),
            REG_ENTRY(2004),
            REG_ENTRY(2005),
            REG_ENTRY(2006),
            REG_ENTRY(2007),
            REG_ENTRY(2008),
            REG_ENTRY(2009),
            REG_ENTRY(2010),
            REG_ENTRY(2011),
            REG_ENTRY(2012),
            REG_ENTRY(2013),
            REG_ENTRY(2014),
            REG_ENTRY(2015),
            REG_ENTRY(2016),
            REG_ENTRY(2017),
            REG_ENTRY(2018),
            REG_ENTRY(2019),
            REG_ENTRY(2020),
            REG_ENTRY(2021),
            REG_ENTRY(2022),
            REG_ENTRY(2023),
            REG_ENTRY(2024),
            REG_ENTRY(2025),
            REG_ENTRY(2026),
            REG_ENTRY(2027),
            REG_ENTRY(2028),
            REG_ENTRY(2029),
            REG_ENTRY(2030),
            REG_ENTRY(2031),
            REG_ENTRY(2032),
            REG_ENTRY(2033),
            REG_ENTRY(2034),
            REG_ENTRY(2035),
            REG_ENTRY(2036),
            REG_ENTRY(2037),
            REG_ENTRY(2038),
            REG_ENTRY(2039),
            REG_ENTRY(2040),
            REG_ENTRY(2041),
            REG_ENTRY(2042),
            REG_ENTRY(2043),
            REG_ENTRY(2044),
            REG_ENTRY(2045),
            REG_ENTRY(2046),
            REG_ENTRY(2047),
            /* sorry for this; but since it's a hacker module, it shouldn't be that bad */
            /* 256 is not enough... */
            MENU_EOL,
        }
    }
};

static MENU_UPDATE_FUNC(show_update)
{
    static struct tm tm;
    if (show_what == SHOW_MODIFIED)
        MENU_SET_VALUE("Modified since %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    else
        LoadCalendarFromRTC(&tm);

    int changed = 0;
    
    for (int reg = 0; reg < reg_num; reg++)
    {
        /* XXX: change this if you ever add or remove menu entries */
        struct menu_entry * entry = &(adtg_gui_menu[0].children[reg + 3]);
        
        if ((int)entry->priv != reg)
            break;

        switch (show_what)
        {
            case SHOW_KNOWN_ONLY:
            {
                int found = 0;
                for (int i = 0; i < COUNT(known_regs); i++)
                {
                    if (known_match(i, reg))
                    {
                        found = 1;
                    }
                }
             
                if (entry->shidden != !found)
                {
                    entry->shidden = !found;
                    changed = 1;
                }
                break;
            }
            case SHOW_MODIFIED:
            {
                int modified = regs[reg].val != regs[reg].prev_val;
                if (entry->shidden != !modified)
                {
                    entry->shidden = !modified;
                    changed = 1;
                }
                break;
            }
            case SHOW_OVERRIDEN:
            {
                int overriden = regs[reg].override != INT_MIN;
                if (entry->shidden != !overriden)
                {
                    entry->shidden = !overriden;
                    changed = 1;
                }
                break;
            }
            case SHOW_ALL:
            {
                if (entry->shidden)
                {
                    entry->shidden = 0;
                    changed = 1;
                }
                break;
            }
        }
        
        if (show_what != SHOW_MODIFIED)
        {
            regs[reg].prev_val = regs[reg].val;
        }
    }
    
    if (changed && info->can_custom_draw)
    {
        /* just a little trick to avoid transient redrawing artifacts */
        /* todo: better backend support for dynamic menus? */
        info->custom_drawing = CUSTOM_DRAW_THIS_MENU;
        bmp_printf(FONT_LARGE, info->x, info->y, "Updating...");
    }
}

static unsigned int adtg_gui_init()
{
    if (is_camera("5D3", "1.1.3"))
    {
        ADTG_WRITE_FUNC = 0x11644;
        CMOS_WRITE_FUNC = 0x119CC;
        CMOS2_WRITE_FUNC = 0x11784;
        CMOS16_WRITE_FUNC = 0x11AB8;
        ENGIO_WRITE_FUNC = 0xff28cc3c;
        ENG_DRV_OUT_FUNC = 0xff28c92c;
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
        ENGIO_WRITE_FUNC = 0xFF9A5618;  // from stubs
        //~ ENG_DRV_OUT_FUNC = 0xff9a54a8;  /* this causes ADTG hook to stop working (why?) */
        ENG_DRV_OUTS_FUNC = 0xff9a5554;
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
        ENGIO_WRITE_FUNC = 0xFF1C5A68;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF1C56A4;
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

    regs_tree.compar = cmp_reg;
    regs_tree.root = 0;

    menu_add("Debug", adtg_gui_menu, COUNT(adtg_gui_menu));
    return 0;
}

static unsigned int adtg_gui_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(adtg_gui_init)
    MODULE_DEINIT(adtg_gui_deinit)
MODULE_INFO_END()
