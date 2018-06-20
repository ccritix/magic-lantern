/**
 * ADTG register editing GUI
 */

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <menu.h>
#include <config.h>
#include <patch.h>

#include "avl.h"
#include "avl.c"    /* unusual include in order to avoid exporting the AVL symbols (keep the namespace clean) */

#define DST_DFE     0xF000
#define DST_CMOS16  0x0F00
#define DST_CMOS    0x00F0
#define DST_ADTG    0x000F      /* any ADTG */
#define DST_ANY     0xFFFF

struct known_reg
{
    uint16_t dst;
    uint16_t reg;
    uint16_t is_nrzi;   /* this will override the default guess */
    char* description;
};

/* todo: check for duplicates */
static struct known_reg known_regs[] = {
    {DST_CMOS,      0, 0, "Analog ISO (most cameras)"},
    {DST_CMOS,      1, 0, "Vertical offset"},
    {DST_CMOS,      2, 0, "Horizontal offset / column skipping"},
    {DST_CMOS,      3, 0, "Analog ISO on 6D"},
    {DST_CMOS,      4, 0, "ISO-related?"},
    {DST_CMOS,      5, 0, "Fine vertical offset, black area maybe"},
    {DST_CMOS,      6, 0, "ISO 50 or timing related: FFF => darker image"},
    {DST_CMOS,      7, 0, "Looks like the cmos is dieing (g3gg0)"},
    {DST_ADTG, 0x8000, 0, "Causes interlacing (g3gg0)"},
    {DST_ADTG, 0x8806, 0, "Causes interlacing artifacts"},
    {DST_ADTG, 0x800C, 0, "Line skipping factor (2 = 1080p, 4 = 720p, 0 = zoom)"},
    {DST_ADTG, 0x805E, 1, "Shutter blanking for x5/x10 zoom"},
    {DST_ADTG, 0x8060, 1, "Shutter blanking for LiveView 1x"},
    {DST_ADTG, 0x8172, 1, "Line count to sample. same as video resolution (g3gg0)"},
    {DST_ADTG, 0x8178, 1, "dwSrFstAdtg1[4], Line count + 1"},
    {DST_ADTG, 0x8179, 1, "dwSrFstAdtg1[5]"},
    {DST_ADTG, 0x8196, 1, "dwSrFstAdtg1[2], Line count + 1"},
    {DST_ADTG, 0x8197, 1, "dwSrFstAdtg1[3]"},
    {DST_ADTG, 0x82F9, 1, "dwSrFstAdtg1 and FPS related"},
    {DST_ADTG, 0x82F3, 1, "Line count that gets darker (top optical black related)"},
    {DST_ADTG, 0x82F8, 1, "Line count"},
    {DST_ADTG, 0x8830, 0, "Only slightly changes the color of the image (g3gg0)"},
    {DST_ADTG, 0x8880, 0, "Black level (reference value for the feedback loop?)"},

    {DST_ADTG, 0x8882, 0, "ISO ADTG gain (per column, mod 4 or mod 8)"},
    {DST_ADTG, 0x8884, 0, "ISO ADTG gain (per column, mod 4 or mod 8)"},
    {DST_ADTG, 0x8886, 0, "ISO ADTG gain (per column, mod 4 or mod 8)"},
    {DST_ADTG, 0x8888, 0, "ISO ADTG gain (per column, mod 4 or mod 8)"},
    {0xC0F0,   0x6000, 0, "FPS register for confirming changes"},
    {0xC0F0,   0x6004, 0, "FPS related, SetHeadForReadout"},
    {0xC0F0,   0x6008, 0, "FPS register A"},
    {0xC0F0,   0x600C, 0, "FPS related"},
    {0xC0F0,   0x6010, 0, "FPS related"},
    {0xC0F0,   0x6014, 0, "FPS register B"},
    {0xC0F0,   0x6018, 0, "FPS related"},
    {0xC0F0,   0x601C, 0, "FPS related"},
    {0xC0F0,   0x6020, 0, "FPS related"},
    {0xC0F0,   0x6084, 0, "RAW first line|column. Column is / 2. 600D: 0x0001007E."},
    {0xC0F0,   0x6088, 0, "RAW last line|column. 600D: FHD 1182|1070, 3x 1048|1102, HD 720|1070"},

    {0xC0F0,   0x6800, 0, "RAW first line|column. Column is / 8 on 5D3 (parallel readout?)"},
    {0xC0F0,   0x6804, 0, "RAW last line|column. 5D3: f6e|2fe, first 1|18 => 5936x3950"},
    {0xC0F0,   0x6824, 0, " "},
    {0xC0F0,   0x6828, 0, " "},
    {0xC0F0,   0x682C, 0, " "},
    {0xC0F0,   0x6830, 0, " "},
   
    {0xc0f0,   0x7134, 0, "HEAD3 timer (start?)"},
    {0xc0f0,   0x7138, 0, "HEAD3 timer"},
    {0xc0f0,   0x713C, 0, "HEAD3 timer (ticks?)"},

    {0xc0f0,   0x7148, 0, "HEAD4 timer (start?)"},
    {0xc0f0,   0x714c, 0, "HEAD4 timer"},
    {0xc0f0,   0x7150, 0, "HEAD4 timer (ticks?)"},
};

static int adtg_enabled = 0;
static int show_what = 0;

#define SHOW_ALL 0
#define SHOW_KNOWN_ONLY 1
#define SHOW_MODIFIED_AT_LEAST_TWICE 2
#define SHOW_MODIFIED_SINCE_TIMESTAMP 3
#define SHOW_OVERRIDEN 4
#define SHOW_FPS_TIMERS 5
#define SHOW_DISPLAY_REGS 6
#define SHOW_IMAGE_SIZE_REGS 7

static int digic_intercept = 0;
static int photo_only = 0;
static int force_low_fps = 0;

static int unique_key = 0;
#define UNIQUE_REG 0
#define UNIQUE_REG_AND_CALLER_TASK 1
#define UNIQUE_REG_AND_CALLER_PC 2

static int random_pokes = 0;

static uint32_t ADTG_WRITE_FUNC = 0;
static uint32_t CMOS_WRITE_FUNC = 0;
static uint32_t CMOS2_WRITE_FUNC = 0;
static uint32_t CMOS16_WRITE_FUNC = 0;
static uint32_t ENGIO_WRITE_FUNC = 0;
static uint32_t ENG_DRV_OUT_FUNC = 0;
static uint32_t ENG_DRV_OUTS_FUNC = 0;
static uint32_t SEND_DATA_TO_DFE_FUNC = 0;
static uint32_t SCS_DUMMY_READOUT_DONE_FUNC = 0;

struct reg_entry
{
    struct avl avl;
    union
    {
        struct
        {
            uint32_t context;   /* optional, place where this register was changed from (PC, task, whatever) */
            uint16_t reg;       /* register offset */
            uint16_t dst;       /* register "class" */
        };
        int64_t key;           /* key in the AVL tree */
    };
    int32_t val;
    int32_t prev_val;
    int override;
    void* addr;
    uint32_t caller_task;
    uint32_t caller_pc;
    uint32_t num_changes;
    unsigned is_nrzi:1;
    unsigned override_enabled:1;
};

static int cmp_reg(void* a,void* b){
    /* simply returning the difference will overflow */
    if (((struct reg_entry*)a)->key > ((struct reg_entry*)b)->key) return 1;
    if (((struct reg_entry*)a)->key < ((struct reg_entry*)b)->key) return -1;
    return 0;
}

static struct reg_entry regs[4096];
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

static int32_t nrzi_decode( uint32_t in_val )
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
    for (int num = 0; num < 31; num++)
    {
        uint32_t bit = in_val & 1<<(30-num) ? 1 : 0;
        if (bit != old_bit)
            out_val |= (1 << (30-num));
        old_bit = bit;
    }
    return out_val;
}

static int get_override_value(struct reg_entry * re)
{
    if (random_pokes == 1)
    {
        return rand();
    }

    if (random_pokes == 2)
    {
        int time = get_seconds_clock();
        int reg = re->reg;
        int dst = re->dst;
        int context = re->context;
        
        /* http://www.cs.hmc.edu/~geoff/classes/hmc.cs070.200101/homework10/hashfuncs.html */
        uint32_t k = time*123 + reg*456 + dst*789 + context * 357;
        uint32_t s = 2654435769;
        uint32_t x = k*s;
        return x >> 16;
    }

    return re->override;
}

static volatile struct reg_entry * found_reg = 0;

static int reg_iter(avl * a)
{
    found_reg = (struct reg_entry *) a;
    return 0;
}

static int reg_total = 0;
static struct reg_entry * reg_find(uint16_t dst, uint16_t reg, uint32_t context)
{
    reg_total++;
    struct reg_entry ref = {{0}};
    ref.dst = dst;
    ref.reg = reg;
    ref.context = context;
    found_reg = 0;
    avl_search(&regs_tree, (struct avl *) &ref, reg_iter);
    
    /* unoptimized code, for reference/troubleshooting */
    /*
    struct reg_entry * found = 0;
    int i = 0;
    for (i = 0; i < reg_num; i++)
    {
        if (regs[i].dst == dst && regs[i].reg == reg)
        {
            found = &regs[i];
            break;
        }
    }
    
    if (found != found_reg)
    {
        printf("%x %x (%d/%d) %x %x (%d) %d\n", found, found_reg, i, reg_num, found->key, found_reg->key, regs_tree.compar(found, &ref), checktree(regs_tree.root, regs_tree.compar));
    }
    return (struct reg_entry *) found;
    */
    
    return (struct reg_entry *) found_reg;
}

static void reg_update_unique(uint16_t dst, void* addr, uint32_t data, uint16_t* val_ptr, uint32_t reg_shift, uint32_t is_nrzi, uint32_t caller_task, uint32_t caller_pc)
{
    if (reg_num + 1 >= COUNT(regs))
    {
        return;
    }
    
    uint32_t reg = data >> reg_shift;
    int32_t val = data & ((1 << reg_shift) - 1);
    uint32_t context = 
        unique_key == UNIQUE_REG_AND_CALLER_PC ? caller_pc :
        unique_key == UNIQUE_REG_AND_CALLER_TASK ? caller_task : 0;
    
    uint32_t old = cli();
    struct reg_entry * re = reg_find(dst, reg, context);

    if (!re)
    {
        /* new entry */
        re = &regs[reg_num];
        re->dst = dst;
        re->reg = reg;
        re->override = INT_MIN;
        re->override_enabled = 0;
        re->is_nrzi = is_nrzi; /* initial guess; may be overriden */
        re->val = INT_MIN;
        re->prev_val = val;
        re->num_changes = 0;
        re->context = context;
        avl_insert(&regs_tree, (struct avl *) re);
        reg_num++;
    }
    sei(old);

    /* fill the data */
    if (re->override_enabled)
    {
        int ovr = get_override_value(re);
        ovr &= ((1 << reg_shift) - 1);
        *val_ptr &= ~((1 << reg_shift) - 1);
        *val_ptr |= ovr;
    }

    if (re->val != val)
    {
        int old = cli();
        re->num_changes++;
        re->val = val;
        if (!re->override_enabled)
        {
            re->override = val;
        }
        sei(old);
    }

    re->addr = addr;
    re->caller_task = caller_task;
    re->caller_pc = caller_pc;
}

static void reg_update_unique_32(uint16_t dst, uint16_t reg, uint32_t* addr, uint32_t* val_ptr, uint32_t caller_task, uint32_t caller_pc)
{
    if (reg_num + 1 >= COUNT(regs))
    {
        return;
    }

    int32_t val = *(int32_t*)val_ptr;
    uint32_t context = 
        unique_key == UNIQUE_REG_AND_CALLER_PC ? caller_pc :
        unique_key == UNIQUE_REG_AND_CALLER_TASK ? caller_task : 0;

    uint32_t old = cli();
    struct reg_entry * re = reg_find(dst, reg, context);

    if (!re)
    {
        /* new entry */
        re = &regs[reg_num];
        re->dst = dst;
        re->reg = reg;
        re->override = INT_MIN;
        re->override_enabled = 0;
        re->is_nrzi = 0;
        re->val = INT_MIN;
        re->prev_val = val;
        re->num_changes = 0;
        re->context = context;
        avl_insert(&regs_tree, (struct avl *) re);
        reg_num++;
    }
    sei(old);
    
    if (force_low_fps)
    {
        /* override these registers before they get a chance to appear in the menu */
        if (re->dst == 0xC0F0 && re->reg == 0x6014)
        {
            *val_ptr = 8191;
        }
    }

    if (re->override_enabled)
    {
        int ovr = get_override_value(re);
        *val_ptr = ovr;
    }

    if (re->val != val)
    {
        int old = cli();
        re->num_changes++;
        re->val = val;
        if (!re->override_enabled)
        {
            re->override = val;
        }
        sei(old);
    }
    
    re->addr = addr;
    re->caller_task = caller_task;
    re->caller_pc = caller_pc;
}

/* used in cmos/adtg/engio log functions */
#define INCREMENT(data_buf, copy_ptr, copy_end) {  \
        data_buf++;                                         \
        copy_ptr++;                                         \
        if (copy_ptr > copy_end) {                          \
            cli();                                          \
            while(1); /* on error, lock up the camera */    \
        }                                                   \
    }

static void adtg_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;
    
    uint32_t cs = regs[0];
    uint32_t *data_buf = (uint32_t *) regs[1];
    int dst = cs & 0xF;

    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();

    /* copy data into a buffer, to make the override temporary */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;

    /* log all ADTG writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        if (dst & 1) reg_update_unique(1, data_buf, *data_buf, (uint16_t*)copy_ptr, 16, 0, caller_task, caller_pc);
        if (dst & 2) reg_update_unique(2, data_buf, *data_buf, (uint16_t*)copy_ptr, 16, 0, caller_task, caller_pc);
        if (dst & 4) reg_update_unique(4, data_buf, *data_buf, (uint16_t*)copy_ptr, 16, 0, caller_task, caller_pc);
        INCREMENT(data_buf, copy_ptr, copy_end);
    }
    *copy_ptr = 0xFFFFFFFF;
    
    /* pass our modified register list to adtg_write */
    regs[1] = (uint32_t) copy;
}

static void cmos_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;

    uint16_t *data_buf = (uint16_t *) regs[0];
    
    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();

    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks / overriding things,
     * values are back to normal */
    static uint16_t copy[512];
    uint16_t* copy_end = &copy[COUNT(copy)];
    uint16_t* copy_ptr = copy;

    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        *copy_ptr = *data_buf;
        reg_update_unique(DST_CMOS, data_buf, *data_buf, copy_ptr, 12, 0, caller_task, caller_pc);
        INCREMENT(data_buf, copy_ptr, copy_end);
    }
    *copy_ptr = 0xFFFF;

    /* pass our modified register list to cmos_write */
    regs[0] = (uint32_t) copy;
}

static void cmos16_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;

    uint16_t *data_buf = (uint16_t *) regs[0];
    
    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();

    /* copy data into a buffer, to make the override temporary */
    /* that means: as soon as we stop executing the hooks / overriding things,
     * values are back to normal */
    static uint16_t copy[512];
    uint16_t* copy_end = &copy[COUNT(copy)];
    uint16_t* copy_ptr = copy;

    /* log all CMOS writes */
    while(*data_buf != 0xFFFF)
    {
        *copy_ptr = *data_buf;
        reg_update_unique(DST_CMOS16, data_buf, *data_buf, copy_ptr, 12, 0, caller_task, caller_pc);
        INCREMENT(data_buf, copy_ptr, copy_end);
    }
    *copy_ptr = 0xFFFF;

    /* pass our modified register list to cmos16_write */
    regs[0] = (uint32_t) copy;
}

static void engio_write_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    /* fixme: use nondestructive overriding */
    if (photo_only && lv) return;
    if (!digic_intercept) return;
    
    uint32_t* data_buf = (uint32_t*) regs[0];

    /* copy data into a buffer, to make the override temporary */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;

    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();

    /* log all ENGIO register writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        uint16_t dst = ((*data_buf) & 0xFFFF0000) >> 16;
        uint16_t reg = (*data_buf) & 0x0000FFFF;
        INCREMENT(data_buf, copy_ptr, copy_end);
        *copy_ptr = *data_buf;
        reg_update_unique_32(dst, reg, data_buf, copy_ptr, caller_task, caller_pc);
        INCREMENT(data_buf, copy_ptr, copy_end);
    }
    *copy_ptr = 0xFFFFFFFF;

    /* pass our modified register list to engio_write */
    regs[0] = (uint32_t) copy;
}

static void EngDrvOut_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;
    if (!digic_intercept) return;

    uint32_t data = (uint32_t) regs[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t val = (uint32_t) regs[1];

    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();
    
    reg_update_unique_32(dst, reg, &val, &val, caller_task, caller_pc);
    regs[1] = val;
}

static void EngDrvOuts_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;
    if (!digic_intercept) return;

    uint32_t data = (uint32_t) regs[0];
    uint16_t dst = (data & 0xFFFF0000) >> 16;
    uint16_t reg = data & 0x0000FFFF;
    uint32_t * values = (uint32_t *) regs[1];
    uint32_t num = (uint32_t) regs[2];

    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();
    
    /* fixme: copy the data to make sure our overrides are temporary */
    for (uint32_t i = 0; i < num; i++)
    {
        reg_update_unique_32(dst, reg + 4*i, &values[i], &values[i], caller_task, caller_pc);
    }
}

static void SendDataToDfe_log(uint32_t* regs, uint32_t* stack, uint32_t pc)
{
    if (photo_only && lv) return;
    
    uint32_t *data_buf = (uint32_t *) regs[0];

    uint32_t caller_task = current_task->taskId;
    uint32_t caller_pc = PATCH_HOOK_CALLER();

    /* copy data into a buffer, to make the override temporary */
    static uint32_t copy[512];
    uint32_t* copy_end = &copy[COUNT(copy)];
    uint32_t* copy_ptr = copy;

    /* log all DFE writes */
    while(*data_buf != 0xFFFFFFFF)
    {
        *copy_ptr = *data_buf;
        reg_update_unique(DST_DFE, data_buf, *data_buf, (uint16_t*)copy_ptr, 16, 0, caller_task, caller_pc);
        INCREMENT(data_buf, copy_ptr, copy_end);
    }
    *copy_ptr = 0xFFFFFFFF;
    
    /* pass our modified register list to SendDataToDfe */
    regs[0] = (uint32_t) copy;
}

static void dummy_readout_log(uint32_t* _regs, uint32_t* stack, uint32_t pc)
{
    /* fixme: naming conflict */
    for (int reg = 0; reg < reg_num; reg++)
    {
        regs[reg].prev_val = regs[reg].val;
    }
}

static MENU_SELECT_FUNC(adtg_toggle)
{
    adtg_enabled = !adtg_enabled;
    
    if (adtg_enabled)
    {
        /* set hooks at ADTG and CMOS writes */
        if (ADTG_WRITE_FUNC)   patch_hook_function(ADTG_WRITE_FUNC, MEM(ADTG_WRITE_FUNC), &adtg_log, "adtg_log");
        if (CMOS_WRITE_FUNC)   patch_hook_function(CMOS_WRITE_FUNC, MEM(CMOS_WRITE_FUNC), &cmos_log, "cmos_log");
        if (CMOS2_WRITE_FUNC)  patch_hook_function(CMOS2_WRITE_FUNC, MEM(CMOS2_WRITE_FUNC), &cmos_log, "cmos_log");
        if (CMOS16_WRITE_FUNC) patch_hook_function(CMOS16_WRITE_FUNC, MEM(CMOS16_WRITE_FUNC), &cmos16_log, "cmos16_log");
        if (ENGIO_WRITE_FUNC)  patch_hook_function(ENGIO_WRITE_FUNC, MEM(ENGIO_WRITE_FUNC), &engio_write_log, "engio_write_log");
       // if (ENG_DRV_OUT_FUNC)  patch_hook_function(ENG_DRV_OUT_FUNC, MEM(ENG_DRV_OUT_FUNC), &EngDrvOut_log, "EngDrvOut_log"); //
       // if (ENG_DRV_OUTS_FUNC) patch_hook_function(ENG_DRV_OUTS_FUNC, MEM(ENG_DRV_OUTS_FUNC), &EngDrvOuts_log, "EngDrvOuts_log"); //
        if (SEND_DATA_TO_DFE_FUNC) patch_hook_function(SEND_DATA_TO_DFE_FUNC, MEM(SEND_DATA_TO_DFE_FUNC), &SendDataToDfe_log, "SendDataToDfe_log");
        if (SCS_DUMMY_READOUT_DONE_FUNC) patch_hook_function(SCS_DUMMY_READOUT_DONE_FUNC, MEM(SCS_DUMMY_READOUT_DONE_FUNC), &dummy_readout_log, "dummy_readout_log");
    }
    else
    {
        /* uninstall watchpoints */
        if (ADTG_WRITE_FUNC)   unpatch_memory(ADTG_WRITE_FUNC);
        if (CMOS_WRITE_FUNC)   unpatch_memory(CMOS_WRITE_FUNC);
        if (CMOS2_WRITE_FUNC)  unpatch_memory(CMOS2_WRITE_FUNC);
        if (CMOS16_WRITE_FUNC) unpatch_memory(CMOS16_WRITE_FUNC);
        if (ENGIO_WRITE_FUNC)  unpatch_memory(ENGIO_WRITE_FUNC);
       // if (ENG_DRV_OUT_FUNC)  unpatch_memory(ENG_DRV_OUT_FUNC); //
       // if (ENG_DRV_OUTS_FUNC) unpatch_memory(ENG_DRV_OUTS_FUNC); //
        if (SEND_DATA_TO_DFE_FUNC) unpatch_memory(SEND_DATA_TO_DFE_FUNC);
        if (SCS_DUMMY_READOUT_DONE_FUNC) unpatch_memory(SCS_DUMMY_READOUT_DONE_FUNC);
    }
}

static int get_reg_from_priv(void* priv)
{
    /* in order to use menu caret editing, entry->priv points to regs[reg].override */
    return ((intptr_t) priv - (intptr_t) &regs[0].override) / sizeof(regs[0]);
}

static MENU_UPDATE_FUNC(reg_update)
{
    int reg = get_reg_from_priv(entry->priv);
    if (reg < 0 || reg >= COUNT(regs))
        return;
    
    if (entry->selected && regs[reg].override != regs[reg].val)
    {
        /* this assummes override and val are always kept in sync by the log functions (atomic updates) */
        /* so, in order to make sure they are identical, we need to check their equality in an atomic operation too */
        int old = cli();
        if (regs[reg].override != regs[reg].val)
        {
            regs[reg].override_enabled = 1;
        }
        sei(old);
    }
    
    char dst_name[10];
    if (regs[reg].dst == DST_DFE)
    {
        snprintf(dst_name, sizeof(dst_name), "DFE");
        entry->max = 0xFFFF;
    }
    else if (regs[reg].dst == DST_CMOS)
    {
        snprintf(dst_name, sizeof(dst_name), "CMOS");
        entry->max = 0xFFF;
    }
    else if (regs[reg].dst == DST_CMOS16)
    {
        snprintf(dst_name, sizeof(dst_name), "CMOS16");
        entry->max = 0xFFF;
    }
    else if (regs[reg].dst & 0xFFF0)
    {
        snprintf(dst_name, sizeof(dst_name), "%X", regs[reg].dst);
        entry->max = 0x40000000; /* fixme: menu backend freezes at higher values */
    }
    else
    {
        snprintf(dst_name, sizeof(dst_name), "ADTG%d", regs[reg].dst);
        entry->max = 0xFFFF;
    }

    for (int i = 0; i < COUNT(known_regs); i++)
    {
        if (known_match(i, reg))
        {
            regs[reg].is_nrzi = known_regs[i].is_nrzi;
        }
    }
    
    MENU_SET_NAME("%s[%x]%s", dst_name, regs[reg].reg, regs[reg].is_nrzi ? " N" : "");
    
    if (show_what == SHOW_MODIFIED_SINCE_TIMESTAMP && !regs[reg].override_enabled)
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
    
    MENU_SET_HELP("%s:%x:%x v=%d(0x%x) nrzi_dec=%d(0x%x).", get_task_name_from_id(regs[reg].caller_task), regs[reg].caller_pc, regs[reg].addr, regs[reg].val, regs[reg].val, nrzi_decode(regs[reg].val), nrzi_decode(regs[reg].val));
    
    if (reg_num >= COUNT(regs)-1)
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Too many registers.");

    MENU_SET_ICON(MNI_BOOL(regs[reg].override_enabled), 0);
    MENU_SET_ENABLED(1);

    if (regs[reg].override_enabled)
    {
        int ovr = get_override_value(&regs[reg]);
        if (get_menu_edit_mode() || menu_active_but_hidden())
        {
            int val = regs[reg].val;
            MENU_SET_RINFO("<- 0x%x", regs[reg].is_nrzi ? nrzi_decode(val) : val);
        }
        else
        {
            MENU_SET_RINFO("-> 0x%x", regs[reg].is_nrzi ? nrzi_decode(ovr) : ovr);
        }
        
        if (menu_active_and_not_hidden())
        {
            MENU_SET_WARNING(MENU_WARN_INFO, "Press Q to stop overriding this register.");
        }
    }
    else
    {
        for (int i = 0; i < COUNT(known_regs); i++)
        {
            if (known_match(i, reg))
            {
                MENU_SET_WARNING(MENU_WARN_INFO, "%s.", known_regs[i].description);
                
                if (show_what != SHOW_MODIFIED_SINCE_TIMESTAMP) /* do we have enough space to show a shortened description? */
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
        if (regs[reg].num_changes > 1)
        {
            MENU_APPEND_RINFO(" "SYM_TIMES"%d", regs[reg].num_changes);
        }
    }
}

static MENU_SELECT_FUNC(reg_toggle_override)
{
    int reg = get_reg_from_priv(priv);
    if (reg < 0 || reg >= COUNT(regs))
        return;
    
    if (regs[reg].override_enabled)
    {
        regs[reg].override = regs[reg].val;
        regs[reg].override_enabled = 0;
    }
    else
    {
        menu_close_submenu(); 
    }
}

#define REG_ENTRY(i) \
        { \
            .name = "(empty)", \
            .priv = &regs[i].override, \
            .select_Q = reg_toggle_override, \
            .update = reg_update, \
            .unit = UNIT_HEX, \
            .max = 0xFFFF, \
            .shidden = 1, \
            .edit_mode = EM_SHOW_LIVEVIEW, \
        }

static MENU_UPDATE_FUNC(show_update);

static MENU_UPDATE_FUNC(adtg_update)
{
    MENU_SET_ICON(MNI_BOOL(adtg_enabled), 0);
    if (adtg_enabled)
    {
        MENU_SET_WARNING(MENU_WARN_INFO, "ADTG hooks enabled. Press Q to start researching.");
        MENU_SET_RINFO("%d uniq / %d", reg_num, reg_total);
    }
}

static int unique_change_attempt = 0;
static MENU_SELECT_FUNC(unique_key_toggle)
{
    if (reg_num == 0)
    {
        menu_numeric_toggle(priv, delta, 0, 2);
    }
    else
    {
        unique_change_attempt = 5;
    }
}

static MENU_UPDATE_FUNC(unique_key_update)
{
    if (reg_num > 0 && unique_change_attempt)
    {
        MENU_SET_WARNING(MENU_WARN_ADVICE, "You can no longer change this, sorry. Restart the camera.");
        unique_change_attempt--;
    }
}

static struct menu_entry adtg_gui_menu[];

static MENU_SELECT_FUNC(lock_displayed_registers)
{
    for (int reg = 0; reg < reg_num; reg++)
    {
        /* XXX: change this if you ever add or remove menu entries */
        /* fixme: duplicate code */
        struct menu_entry * entry = &(adtg_gui_menu[0].children[reg + 2]);
        
        if (entry->shidden)
            continue;
        
        if (!regs[reg].override_enabled)
        {
            regs[reg].override = regs[reg].val;
            regs[reg].override_enabled = 1;
        }
    }
    
    menu_toggle_submenu();
}

/* pack two 6-bit values into a 12-bit one */
#define PACK12(lo,hi) ((((lo) & 0x3F) | ((hi) << 6)) & 0xFFF)

static int crop_mode_reg(int reg)
{
       return 0;
}

static int res3k_reg(int reg)
{

   if (is_camera("100D", "1.0.1"))
    {

       if (regs[reg].dst == DST_CMOS)
       {
           switch (regs[reg].reg)
           {
		case 5:
	  	    return 0x200;       /* CMOS[5]: ISO related? */
                 case 7:
		    return 0xf20;       /* CMOS[7]: ISO related? */
           }
       }

       if (regs[reg].dst == 0xC0F0)
       {

           switch (regs[reg].reg)
           {
                case 0x6804:               
                    return 0xa1b0421;       /* 4056x2552 9fps 14-bit lossless */
	    	case 0x6824:
		    return 0x4ca;
	    	case 0x6828:
		    return 0x4ca;
	    	case 0x682c:
		    return 0x4ca;
	    	case 0x6830:
		    return 0x4ca;
	    	case 0x6008:
		    return 0x45b045b;
	    	case 0x600c:
		    return 0x45b045b;
	    	case 0x6010: 	
		    return 0x45b;
	    	case 0x6014:
		    return 0xbd4;
	    	case 0x713c:
		    return 0xa55;
            }

        }
        else if (regs[reg].dst == 2)        /* ADTG 2 */
        {
            switch (regs[reg].reg)
            {
            case 0x82b6:
	       return 0xbf4;      /* it's 5 in zoom mode and 6 in 1080p; this also overrides ADTG4 */
            case 0x8172:
	       return 0x8fd;
      	    case 0x8178:
	       return 0x8fd;

            }

        }

    }
    else if (is_camera("EOSM", "2.0.2"))
    {

       if (regs[reg].dst == DST_CMOS)
       {
           switch (regs[reg].reg)
           {
		case 5:
	  	    return 0x200;       /* CMOS[5]: ISO related? */
                 case 7:
		    return 0xf20;       /* CMOS[7]: ISO related? */
           }
       }

       if (regs[reg].dst == 0xC0F0)
       {

           switch (regs[reg].reg)
           {
                case 0x6804:               
                    return 0xa1b0422;       /* 4096x2552 9fps 14-bit lossless */
	    	case 0x6824:
		    return 0x4ca;
	    	case 0x6828:
		    return 0x4ca;
	    	case 0x682c:
		    return 0x4ca;
	    	case 0x6830:
		    return 0x4ca;
	    	case 0x6008:
		    return 0x45b045b;
	    	case 0x600c:
		    return 0x45b045b;
	    	case 0x6010: 	
		    return 0x45b;
	    	case 0x6014:
		    return 0xbd4;
	    	case 0x713c:
		    return 0xa55;
            }

        }
        else if (regs[reg].dst == 2)        /* ADTG 2 */
        {
            switch (regs[reg].reg)
            {
            case 0x82b6:
	       return 0xbf4;      /* it's 5 in zoom mode and 6 in 1080p; this also overrides ADTG4 */
            case 0x8172:
	       return 0x8fd;
      	    case 0x8178:
	       return 0x8fd;

            }

        }

    }
    else if (is_camera("700D", "1.1.5"))
    {

       if (regs[reg].dst == 0xC0F0)
       {

           switch (regs[reg].reg)
           {
                case 0x6804:                /* C0F06804 - raw resolution */
                    return 0x5840298;       /* Valid liveview 2520x1248 24fps 14-bit lossless */
                case 0x6014:
                    return 0x747; 
                case 0x713c:
                    return 0x516;
      
            }

        }
        else if (regs[reg].dst == 2)        /* ADTG 2 */
        { 
            switch (regs[reg].reg)
            {
                 case 0x8172:
                    return 0x87c;
                 case 0x8178:
                    return 0x87c;
                 case 0x82b6:
                    return 0x08f4;

            }

        }

    }
    else if (is_camera("6D", "1.1.6"))
    {

     if (regs[reg].dst == DST_CMOS)
         {
           switch (regs[reg].reg)
           {
                 case 7:
		    return 0x268;       /* CMOS[7]: White bar at the bottom removement */
           }
         }

       if (regs[reg].dst == 0xC0F0)
       {

           switch (regs[reg].reg)
           {
                case 0x6804:               
                    return 0x4e502a0;       
                case 0x6014:
                   return 0x5ba;
                case 0x713c:               
                    return 0x516;       
                case 0x7150:
                   return 0x4e5;
                case 0x6010:               
                    return 0x2b9;       
                case 0x6008:
                   return 0x2b902b9;
                case 0x600c:
                   return 0x2b902b9;            
               
            }

        }
        else if (regs[reg].dst == 2)        /* ADTG 2 */
        {
            switch (regs[reg].reg)
            {
                 case 0x8172:
                    return 0x695;
                 case 0x8178:
                    return 0x695;
                 case 0x82f8:
                    return 0x697;
                 case 0x8179:
                    return 0x51c;
                 case 0x82f9:
                    return 0x560;
                 case 0x800c:
                    return 0x0;
                 case 0x8000:
                    return 0x5;

            }

        }

    }

    return 0;
}
static void apply_preset(int(*get_preset_reg)(int))
{
    for (int reg = 0; reg < reg_num; reg++)
    {
        int ovr = get_preset_reg(reg);
        if (ovr)
        {
            regs[reg].override = ovr;
            regs[reg].override_enabled = 1;
        }
    }
}

static MENU_SELECT_FUNC(crop_mode_overrides)
{
    apply_preset(crop_mode_reg);
    menu_toggle_submenu();
}

static MENU_SELECT_FUNC(crop_mode_overrides_3k)
{
    apply_preset(crop_mode_reg);
    apply_preset(res3k_reg);
    menu_toggle_submenu();
}

#include <lens.h>
static int log_all_regs = 0;
static volatile int log_iso_regs_running = 0;

static void log_iso_regs()
{
    log_iso_regs_running = 1;
    
    msleep(1000);
    int size = 1024*1024;
    char* msg = fio_malloc(size);
    if (!msg) return;
    msg[0] = 0;
    int len = 0;
    int saved_regs = 0;

    len += snprintf(msg+len, size-len, "%s %s\n", camera_model, firmware_version);
    for (int i = 0; i < reg_num; i++)
    {
        /* XXX: change this if you ever add or remove menu entries */
        /* fixme: duplicate code */
        struct menu_entry * entry = &(adtg_gui_menu[0].children[i + 2]);
        
        if (entry->shidden)
            continue;

        len += snprintf(msg+len, size-len, "%04x%04x:%8x ", regs[i].dst, regs[i].reg, regs[i].val);
        if (regs[i].val != regs[i].prev_val)
        {
            snprintf(msg+len, size-len, "(was %x)         ", regs[i].prev_val);
            len += 5 + 8 + 2;
        }
        len += snprintf(msg+len, size-len, "ISO=%d Tv=%d Av=%d ", raw2iso(lens_info.raw_iso), lens_info.shutter, lens_info.aperture);
        len += snprintf(msg+len, size-len, "lv=%d zoom=%d mv=%d res=%d crop=%d ", lv, lv_dispsize, is_movie_mode(), is_movie_mode() ? video_mode_resolution : -1, is_movie_mode() ? video_mode_crop : -1);
        len += snprintf(msg+len, size-len, "task=%s pc=%x addr=%x ", get_task_name_from_id(regs[i].caller_task), regs[i].caller_pc, regs[i].addr);

        for (int j = 0; j < COUNT(known_regs); j++)
            if (known_match(j, i))
                len += snprintf(msg+len, size-len, "%s", known_regs[j].description);

        len += snprintf(msg+len, size-len, "\n");
        
        saved_regs++;
    }

    len += snprintf(msg+len, size-len, "==================================================================\n");

    FILE * f = FIO_CreateFileOrAppend("ML/LOGS/adtg.log");
    FIO_WriteFile(f, msg, len);
    FIO_CloseFile(f);
    fio_free(msg);
    NotifyBox(2000, "Saved %d regs, %d bytes", saved_regs, len);
    log_iso_regs_running = 0;
}

PROP_HANDLER(PROP_GUI_STATE)
{
    if (buf[0] == GUISTATE_QR)
    {
        if (log_all_regs && adtg_enabled && !log_iso_regs_running)
        {
            log_iso_regs_running = 1;
            task_create("log_iso_regs", 0x1c, 0x1000, log_iso_regs, 0);
        }
    }
}

static struct menu_entry adtg_gui_menu[] =
{
    {
        .name           = "ADTG Registers",
        .priv           = &adtg_enabled,
        .select         = &adtg_toggle,
        .max            = 1,
        .update         = adtg_update,
        .help           = "Edit ADTG/CMOS register values.",
        .submenu_width  = 710,
        .children       = (struct menu_entry[])
        {
            {
                .name           = "Show",
                .priv           = &show_what,
                .update         = show_update,
                .max            = 7,
                .choices        = CHOICES(
                                    "Everything",
                                    "Known regs only",
                                    "Modified at least twice",
                                    "Modified from now on",
                                    "Overriden regs only",
                                    "FPS timers only",
                                    "Display registers only",
                                    "Image size regs only",
                                  ),
                .help2          =  "Everything: show all registers as soon as they are written.\n"
                                   "Known: show only the registers with a known description.\n"
                                   "Modified at least twice: only regs that were changed more than once.\n"
                                   "Modified from now on: only regs where final value was modified.\n"
                                   "Overriden: show only regs where you have changed the value.\n"
                                   "FPS timers only: show only FPS timer A and B.\n"
                                   "Display registers only: C0F14000 ... C0F14FFF.\n"
                                   "Image size regs only: registers related to raw image size (resolution).\n"
            },
            {
                .name           = "Advanced",
                .select         = menu_open_submenu,
                .children       = (struct menu_entry[])
                {
                    {
                        .name           = "DIGIC Registers",
                        .priv           = &digic_intercept,
                        .max            = 1,
                        .help           = "Also intercept DIGIC registers (EngDrvOut and engio_write).",
                    },
                    {
                        .name           = "Unique Key",
                        .priv           = &unique_key,
                        .select         = unique_key_toggle,
                        .update         = unique_key_update,
                        .max            = 2,
                        .icon_type      = IT_DICE_OFF,
                        .choices        = CHOICES("Register", "Register + caller task", "Register + caller PC"),
                        .help           = "When two register operations are identical? (for grouping).",
                        .help2          = "When register number and type (family, class) are equal.\n"
                                           "When reg num/type are equal AND changed from the same task.\n"
                                           "When reg num/type equal AND changed from same prog counter.\n"
                    },
                    {
                        .name           = "Force low FPS",
                        .priv           = &force_low_fps,
                        .max            = 1,
                        .help           = "Try enabling this if it locks up in LiveView.",
                        .help2          = "Also enable 'DIGIC Registers'.",
                    },
                    {
                        .name           = "Disable Logging",
                        .priv           = &photo_only,
                        .max            = 1,
                        .icon_type      = IT_DICE_OFF,
                        .choices        = CHOICES("OFF", "in LiveView"),
                        .help           = "You may disable logging in LiveView.",
                    },
                    {
                        .name           = "Auto Log Registers",
                        .priv           = &log_all_regs,
                        .max            = 1,
                        .choices        = CHOICES("OFF", "After taking a pic"),
                        .help           = "Save visible registers to a log file (adtg.log) after taking a picture.",
                    },
                    {
                        .name           = "Log Registers Now",
                        .priv           = &log_iso_regs,
                        .select         = (void (*)(void*,int))run_in_separate_task,
                        .help           = "Save visible registers to a log file (adtg.log) right now.",
                    }, 
                    {
                        .name           = "Random Pokes",
                        .priv           = &random_pokes,
                        .choices        = CHOICES("OFF", "Every update", "Every second"),
                        .max            = 2,
                        .help           = "Use a random value when overriding the registers.",
                    },
                    {
                        .name           = "Lock Displayed Registers",
                        .select         = lock_displayed_registers,
                        .help           = "Override all displayed registers to their current value.",
                        .help2          = "Registers already overriden will not be changed.",
                    },
                    {
                        .name           = "1:1 crop mode (5D3)",
                        .select         = crop_mode_overrides,
                        .help           = "Turns regular 1080p into 1:1 crop mode. For other cameras:",
                        .help2          = "magiclantern.fm/forum/index.php?topic=10111.msg145036#msg145036",
                    },
                    {
                        .name           = "1:1 3K crop mode (5D3)",
                        .select         = crop_mode_overrides_3k,
                        .help           = "Experimental 3K video mode with 1:1 crop. Preview broken.",
                    },
                    MENU_EOL,
                },
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
            REG_ENTRY(2048),
            REG_ENTRY(2049),
            REG_ENTRY(2050),
            REG_ENTRY(2051),
            REG_ENTRY(2052),
            REG_ENTRY(2053),
            REG_ENTRY(2054),
            REG_ENTRY(2055),
            REG_ENTRY(2056),
            REG_ENTRY(2057),
            REG_ENTRY(2058),
            REG_ENTRY(2059),
            REG_ENTRY(2060),
            REG_ENTRY(2061),
            REG_ENTRY(2062),
            REG_ENTRY(2063),
            REG_ENTRY(2064),
            REG_ENTRY(2065),
            REG_ENTRY(2066),
            REG_ENTRY(2067),
            REG_ENTRY(2068),
            REG_ENTRY(2069),
            REG_ENTRY(2070),
            REG_ENTRY(2071),
            REG_ENTRY(2072),
            REG_ENTRY(2073),
            REG_ENTRY(2074),
            REG_ENTRY(2075),
            REG_ENTRY(2076),
            REG_ENTRY(2077),
            REG_ENTRY(2078),
            REG_ENTRY(2079),
            REG_ENTRY(2080),
            REG_ENTRY(2081),
            REG_ENTRY(2082),
            REG_ENTRY(2083),
            REG_ENTRY(2084),
            REG_ENTRY(2085),
            REG_ENTRY(2086),
            REG_ENTRY(2087),
            REG_ENTRY(2088),
            REG_ENTRY(2089),
            REG_ENTRY(2090),
            REG_ENTRY(2091),
            REG_ENTRY(2092),
            REG_ENTRY(2093),
            REG_ENTRY(2094),
            REG_ENTRY(2095),
            REG_ENTRY(2096),
            REG_ENTRY(2097),
            REG_ENTRY(2098),
            REG_ENTRY(2099),
            REG_ENTRY(2100),
            REG_ENTRY(2101),
            REG_ENTRY(2102),
            REG_ENTRY(2103),
            REG_ENTRY(2104),
            REG_ENTRY(2105),
            REG_ENTRY(2106),
            REG_ENTRY(2107),
            REG_ENTRY(2108),
            REG_ENTRY(2109),
            REG_ENTRY(2110),
            REG_ENTRY(2111),
            REG_ENTRY(2112),
            REG_ENTRY(2113),
            REG_ENTRY(2114),
            REG_ENTRY(2115),
            REG_ENTRY(2116),
            REG_ENTRY(2117),
            REG_ENTRY(2118),
            REG_ENTRY(2119),
            REG_ENTRY(2120),
            REG_ENTRY(2121),
            REG_ENTRY(2122),
            REG_ENTRY(2123),
            REG_ENTRY(2124),
            REG_ENTRY(2125),
            REG_ENTRY(2126),
            REG_ENTRY(2127),
            REG_ENTRY(2128),
            REG_ENTRY(2129),
            REG_ENTRY(2130),
            REG_ENTRY(2131),
            REG_ENTRY(2132),
            REG_ENTRY(2133),
            REG_ENTRY(2134),
            REG_ENTRY(2135),
            REG_ENTRY(2136),
            REG_ENTRY(2137),
            REG_ENTRY(2138),
            REG_ENTRY(2139),
            REG_ENTRY(2140),
            REG_ENTRY(2141),
            REG_ENTRY(2142),
            REG_ENTRY(2143),
            REG_ENTRY(2144),
            REG_ENTRY(2145),
            REG_ENTRY(2146),
            REG_ENTRY(2147),
            REG_ENTRY(2148),
            REG_ENTRY(2149),
            REG_ENTRY(2150),
            REG_ENTRY(2151),
            REG_ENTRY(2152),
            REG_ENTRY(2153),
            REG_ENTRY(2154),
            REG_ENTRY(2155),
            REG_ENTRY(2156),
            REG_ENTRY(2157),
            REG_ENTRY(2158),
            REG_ENTRY(2159),
            REG_ENTRY(2160),
            REG_ENTRY(2161),
            REG_ENTRY(2162),
            REG_ENTRY(2163),
            REG_ENTRY(2164),
            REG_ENTRY(2165),
            REG_ENTRY(2166),
            REG_ENTRY(2167),
            REG_ENTRY(2168),
            REG_ENTRY(2169),
            REG_ENTRY(2170),
            REG_ENTRY(2171),
            REG_ENTRY(2172),
            REG_ENTRY(2173),
            REG_ENTRY(2174),
            REG_ENTRY(2175),
            REG_ENTRY(2176),
            REG_ENTRY(2177),
            REG_ENTRY(2178),
            REG_ENTRY(2179),
            REG_ENTRY(2180),
            REG_ENTRY(2181),
            REG_ENTRY(2182),
            REG_ENTRY(2183),
            REG_ENTRY(2184),
            REG_ENTRY(2185),
            REG_ENTRY(2186),
            REG_ENTRY(2187),
            REG_ENTRY(2188),
            REG_ENTRY(2189),
            REG_ENTRY(2190),
            REG_ENTRY(2191),
            REG_ENTRY(2192),
            REG_ENTRY(2193),
            REG_ENTRY(2194),
            REG_ENTRY(2195),
            REG_ENTRY(2196),
            REG_ENTRY(2197),
            REG_ENTRY(2198),
            REG_ENTRY(2199),
            REG_ENTRY(2200),
            REG_ENTRY(2201),
            REG_ENTRY(2202),
            REG_ENTRY(2203),
            REG_ENTRY(2204),
            REG_ENTRY(2205),
            REG_ENTRY(2206),
            REG_ENTRY(2207),
            REG_ENTRY(2208),
            REG_ENTRY(2209),
            REG_ENTRY(2210),
            REG_ENTRY(2211),
            REG_ENTRY(2212),
            REG_ENTRY(2213),
            REG_ENTRY(2214),
            REG_ENTRY(2215),
            REG_ENTRY(2216),
            REG_ENTRY(2217),
            REG_ENTRY(2218),
            REG_ENTRY(2219),
            REG_ENTRY(2220),
            REG_ENTRY(2221),
            REG_ENTRY(2222),
            REG_ENTRY(2223),
            REG_ENTRY(2224),
            REG_ENTRY(2225),
            REG_ENTRY(2226),
            REG_ENTRY(2227),
            REG_ENTRY(2228),
            REG_ENTRY(2229),
            REG_ENTRY(2230),
            REG_ENTRY(2231),
            REG_ENTRY(2232),
            REG_ENTRY(2233),
            REG_ENTRY(2234),
            REG_ENTRY(2235),
            REG_ENTRY(2236),
            REG_ENTRY(2237),
            REG_ENTRY(2238),
            REG_ENTRY(2239),
            REG_ENTRY(2240),
            REG_ENTRY(2241),
            REG_ENTRY(2242),
            REG_ENTRY(2243),
            REG_ENTRY(2244),
            REG_ENTRY(2245),
            REG_ENTRY(2246),
            REG_ENTRY(2247),
            REG_ENTRY(2248),
            REG_ENTRY(2249),
            REG_ENTRY(2250),
            REG_ENTRY(2251),
            REG_ENTRY(2252),
            REG_ENTRY(2253),
            REG_ENTRY(2254),
            REG_ENTRY(2255),
            REG_ENTRY(2256),
            REG_ENTRY(2257),
            REG_ENTRY(2258),
            REG_ENTRY(2259),
            REG_ENTRY(2260),
            REG_ENTRY(2261),
            REG_ENTRY(2262),
            REG_ENTRY(2263),
            REG_ENTRY(2264),
            REG_ENTRY(2265),
            REG_ENTRY(2266),
            REG_ENTRY(2267),
            REG_ENTRY(2268),
            REG_ENTRY(2269),
            REG_ENTRY(2270),
            REG_ENTRY(2271),
            REG_ENTRY(2272),
            REG_ENTRY(2273),
            REG_ENTRY(2274),
            REG_ENTRY(2275),
            REG_ENTRY(2276),
            REG_ENTRY(2277),
            REG_ENTRY(2278),
            REG_ENTRY(2279),
            REG_ENTRY(2280),
            REG_ENTRY(2281),
            REG_ENTRY(2282),
            REG_ENTRY(2283),
            REG_ENTRY(2284),
            REG_ENTRY(2285),
            REG_ENTRY(2286),
            REG_ENTRY(2287),
            REG_ENTRY(2288),
            REG_ENTRY(2289),
            REG_ENTRY(2290),
            REG_ENTRY(2291),
            REG_ENTRY(2292),
            REG_ENTRY(2293),
            REG_ENTRY(2294),
            REG_ENTRY(2295),
            REG_ENTRY(2296),
            REG_ENTRY(2297),
            REG_ENTRY(2298),
            REG_ENTRY(2299),
            REG_ENTRY(2300),
            REG_ENTRY(2301),
            REG_ENTRY(2302),
            REG_ENTRY(2303),
            REG_ENTRY(2304),
            REG_ENTRY(2305),
            REG_ENTRY(2306),
            REG_ENTRY(2307),
            REG_ENTRY(2308),
            REG_ENTRY(2309),
            REG_ENTRY(2310),
            REG_ENTRY(2311),
            REG_ENTRY(2312),
            REG_ENTRY(2313),
            REG_ENTRY(2314),
            REG_ENTRY(2315),
            REG_ENTRY(2316),
            REG_ENTRY(2317),
            REG_ENTRY(2318),
            REG_ENTRY(2319),
            REG_ENTRY(2320),
            REG_ENTRY(2321),
            REG_ENTRY(2322),
            REG_ENTRY(2323),
            REG_ENTRY(2324),
            REG_ENTRY(2325),
            REG_ENTRY(2326),
            REG_ENTRY(2327),
            REG_ENTRY(2328),
            REG_ENTRY(2329),
            REG_ENTRY(2330),
            REG_ENTRY(2331),
            REG_ENTRY(2332),
            REG_ENTRY(2333),
            REG_ENTRY(2334),
            REG_ENTRY(2335),
            REG_ENTRY(2336),
            REG_ENTRY(2337),
            REG_ENTRY(2338),
            REG_ENTRY(2339),
            REG_ENTRY(2340),
            REG_ENTRY(2341),
            REG_ENTRY(2342),
            REG_ENTRY(2343),
            REG_ENTRY(2344),
            REG_ENTRY(2345),
            REG_ENTRY(2346),
            REG_ENTRY(2347),
            REG_ENTRY(2348),
            REG_ENTRY(2349),
            REG_ENTRY(2350),
            REG_ENTRY(2351),
            REG_ENTRY(2352),
            REG_ENTRY(2353),
            REG_ENTRY(2354),
            REG_ENTRY(2355),
            REG_ENTRY(2356),
            REG_ENTRY(2357),
            REG_ENTRY(2358),
            REG_ENTRY(2359),
            REG_ENTRY(2360),
            REG_ENTRY(2361),
            REG_ENTRY(2362),
            REG_ENTRY(2363),
            REG_ENTRY(2364),
            REG_ENTRY(2365),
            REG_ENTRY(2366),
            REG_ENTRY(2367),
            REG_ENTRY(2368),
            REG_ENTRY(2369),
            REG_ENTRY(2370),
            REG_ENTRY(2371),
            REG_ENTRY(2372),
            REG_ENTRY(2373),
            REG_ENTRY(2374),
            REG_ENTRY(2375),
            REG_ENTRY(2376),
            REG_ENTRY(2377),
            REG_ENTRY(2378),
            REG_ENTRY(2379),
            REG_ENTRY(2380),
            REG_ENTRY(2381),
            REG_ENTRY(2382),
            REG_ENTRY(2383),
            REG_ENTRY(2384),
            REG_ENTRY(2385),
            REG_ENTRY(2386),
            REG_ENTRY(2387),
            REG_ENTRY(2388),
            REG_ENTRY(2389),
            REG_ENTRY(2390),
            REG_ENTRY(2391),
            REG_ENTRY(2392),
            REG_ENTRY(2393),
            REG_ENTRY(2394),
            REG_ENTRY(2395),
            REG_ENTRY(2396),
            REG_ENTRY(2397),
            REG_ENTRY(2398),
            REG_ENTRY(2399),
            REG_ENTRY(2400),
            REG_ENTRY(2401),
            REG_ENTRY(2402),
            REG_ENTRY(2403),
            REG_ENTRY(2404),
            REG_ENTRY(2405),
            REG_ENTRY(2406),
            REG_ENTRY(2407),
            REG_ENTRY(2408),
            REG_ENTRY(2409),
            REG_ENTRY(2410),
            REG_ENTRY(2411),
            REG_ENTRY(2412),
            REG_ENTRY(2413),
            REG_ENTRY(2414),
            REG_ENTRY(2415),
            REG_ENTRY(2416),
            REG_ENTRY(2417),
            REG_ENTRY(2418),
            REG_ENTRY(2419),
            REG_ENTRY(2420),
            REG_ENTRY(2421),
            REG_ENTRY(2422),
            REG_ENTRY(2423),
            REG_ENTRY(2424),
            REG_ENTRY(2425),
            REG_ENTRY(2426),
            REG_ENTRY(2427),
            REG_ENTRY(2428),
            REG_ENTRY(2429),
            REG_ENTRY(2430),
            REG_ENTRY(2431),
            REG_ENTRY(2432),
            REG_ENTRY(2433),
            REG_ENTRY(2434),
            REG_ENTRY(2435),
            REG_ENTRY(2436),
            REG_ENTRY(2437),
            REG_ENTRY(2438),
            REG_ENTRY(2439),
            REG_ENTRY(2440),
            REG_ENTRY(2441),
            REG_ENTRY(2442),
            REG_ENTRY(2443),
            REG_ENTRY(2444),
            REG_ENTRY(2445),
            REG_ENTRY(2446),
            REG_ENTRY(2447),
            REG_ENTRY(2448),
            REG_ENTRY(2449),
            REG_ENTRY(2450),
            REG_ENTRY(2451),
            REG_ENTRY(2452),
            REG_ENTRY(2453),
            REG_ENTRY(2454),
            REG_ENTRY(2455),
            REG_ENTRY(2456),
            REG_ENTRY(2457),
            REG_ENTRY(2458),
            REG_ENTRY(2459),
            REG_ENTRY(2460),
            REG_ENTRY(2461),
            REG_ENTRY(2462),
            REG_ENTRY(2463),
            REG_ENTRY(2464),
            REG_ENTRY(2465),
            REG_ENTRY(2466),
            REG_ENTRY(2467),
            REG_ENTRY(2468),
            REG_ENTRY(2469),
            REG_ENTRY(2470),
            REG_ENTRY(2471),
            REG_ENTRY(2472),
            REG_ENTRY(2473),
            REG_ENTRY(2474),
            REG_ENTRY(2475),
            REG_ENTRY(2476),
            REG_ENTRY(2477),
            REG_ENTRY(2478),
            REG_ENTRY(2479),
            REG_ENTRY(2480),
            REG_ENTRY(2481),
            REG_ENTRY(2482),
            REG_ENTRY(2483),
            REG_ENTRY(2484),
            REG_ENTRY(2485),
            REG_ENTRY(2486),
            REG_ENTRY(2487),
            REG_ENTRY(2488),
            REG_ENTRY(2489),
            REG_ENTRY(2490),
            REG_ENTRY(2491),
            REG_ENTRY(2492),
            REG_ENTRY(2493),
            REG_ENTRY(2494),
            REG_ENTRY(2495),
            REG_ENTRY(2496),
            REG_ENTRY(2497),
            REG_ENTRY(2498),
            REG_ENTRY(2499),
            REG_ENTRY(2500),
            REG_ENTRY(2501),
            REG_ENTRY(2502),
            REG_ENTRY(2503),
            REG_ENTRY(2504),
            REG_ENTRY(2505),
            REG_ENTRY(2506),
            REG_ENTRY(2507),
            REG_ENTRY(2508),
            REG_ENTRY(2509),
            REG_ENTRY(2510),
            REG_ENTRY(2511),
            REG_ENTRY(2512),
            REG_ENTRY(2513),
            REG_ENTRY(2514),
            REG_ENTRY(2515),
            REG_ENTRY(2516),
            REG_ENTRY(2517),
            REG_ENTRY(2518),
            REG_ENTRY(2519),
            REG_ENTRY(2520),
            REG_ENTRY(2521),
            REG_ENTRY(2522),
            REG_ENTRY(2523),
            REG_ENTRY(2524),
            REG_ENTRY(2525),
            REG_ENTRY(2526),
            REG_ENTRY(2527),
            REG_ENTRY(2528),
            REG_ENTRY(2529),
            REG_ENTRY(2530),
            REG_ENTRY(2531),
            REG_ENTRY(2532),
            REG_ENTRY(2533),
            REG_ENTRY(2534),
            REG_ENTRY(2535),
            REG_ENTRY(2536),
            REG_ENTRY(2537),
            REG_ENTRY(2538),
            REG_ENTRY(2539),
            REG_ENTRY(2540),
            REG_ENTRY(2541),
            REG_ENTRY(2542),
            REG_ENTRY(2543),
            REG_ENTRY(2544),
            REG_ENTRY(2545),
            REG_ENTRY(2546),
            REG_ENTRY(2547),
            REG_ENTRY(2548),
            REG_ENTRY(2549),
            REG_ENTRY(2550),
            REG_ENTRY(2551),
            REG_ENTRY(2552),
            REG_ENTRY(2553),
            REG_ENTRY(2554),
            REG_ENTRY(2555),
            REG_ENTRY(2556),
            REG_ENTRY(2557),
            REG_ENTRY(2558),
            REG_ENTRY(2559),
            REG_ENTRY(2560),
            REG_ENTRY(2561),
            REG_ENTRY(2562),
            REG_ENTRY(2563),
            REG_ENTRY(2564),
            REG_ENTRY(2565),
            REG_ENTRY(2566),
            REG_ENTRY(2567),
            REG_ENTRY(2568),
            REG_ENTRY(2569),
            REG_ENTRY(2570),
            REG_ENTRY(2571),
            REG_ENTRY(2572),
            REG_ENTRY(2573),
            REG_ENTRY(2574),
            REG_ENTRY(2575),
            REG_ENTRY(2576),
            REG_ENTRY(2577),
            REG_ENTRY(2578),
            REG_ENTRY(2579),
            REG_ENTRY(2580),
            REG_ENTRY(2581),
            REG_ENTRY(2582),
            REG_ENTRY(2583),
            REG_ENTRY(2584),
            REG_ENTRY(2585),
            REG_ENTRY(2586),
            REG_ENTRY(2587),
            REG_ENTRY(2588),
            REG_ENTRY(2589),
            REG_ENTRY(2590),
            REG_ENTRY(2591),
            REG_ENTRY(2592),
            REG_ENTRY(2593),
            REG_ENTRY(2594),
            REG_ENTRY(2595),
            REG_ENTRY(2596),
            REG_ENTRY(2597),
            REG_ENTRY(2598),
            REG_ENTRY(2599),
            REG_ENTRY(2600),
            REG_ENTRY(2601),
            REG_ENTRY(2602),
            REG_ENTRY(2603),
            REG_ENTRY(2604),
            REG_ENTRY(2605),
            REG_ENTRY(2606),
            REG_ENTRY(2607),
            REG_ENTRY(2608),
            REG_ENTRY(2609),
            REG_ENTRY(2610),
            REG_ENTRY(2611),
            REG_ENTRY(2612),
            REG_ENTRY(2613),
            REG_ENTRY(2614),
            REG_ENTRY(2615),
            REG_ENTRY(2616),
            REG_ENTRY(2617),
            REG_ENTRY(2618),
            REG_ENTRY(2619),
            REG_ENTRY(2620),
            REG_ENTRY(2621),
            REG_ENTRY(2622),
            REG_ENTRY(2623),
            REG_ENTRY(2624),
            REG_ENTRY(2625),
            REG_ENTRY(2626),
            REG_ENTRY(2627),
            REG_ENTRY(2628),
            REG_ENTRY(2629),
            REG_ENTRY(2630),
            REG_ENTRY(2631),
            REG_ENTRY(2632),
            REG_ENTRY(2633),
            REG_ENTRY(2634),
            REG_ENTRY(2635),
            REG_ENTRY(2636),
            REG_ENTRY(2637),
            REG_ENTRY(2638),
            REG_ENTRY(2639),
            REG_ENTRY(2640),
            REG_ENTRY(2641),
            REG_ENTRY(2642),
            REG_ENTRY(2643),
            REG_ENTRY(2644),
            REG_ENTRY(2645),
            REG_ENTRY(2646),
            REG_ENTRY(2647),
            REG_ENTRY(2648),
            REG_ENTRY(2649),
            REG_ENTRY(2650),
            REG_ENTRY(2651),
            REG_ENTRY(2652),
            REG_ENTRY(2653),
            REG_ENTRY(2654),
            REG_ENTRY(2655),
            REG_ENTRY(2656),
            REG_ENTRY(2657),
            REG_ENTRY(2658),
            REG_ENTRY(2659),
            REG_ENTRY(2660),
            REG_ENTRY(2661),
            REG_ENTRY(2662),
            REG_ENTRY(2663),
            REG_ENTRY(2664),
            REG_ENTRY(2665),
            REG_ENTRY(2666),
            REG_ENTRY(2667),
            REG_ENTRY(2668),
            REG_ENTRY(2669),
            REG_ENTRY(2670),
            REG_ENTRY(2671),
            REG_ENTRY(2672),
            REG_ENTRY(2673),
            REG_ENTRY(2674),
            REG_ENTRY(2675),
            REG_ENTRY(2676),
            REG_ENTRY(2677),
            REG_ENTRY(2678),
            REG_ENTRY(2679),
            REG_ENTRY(2680),
            REG_ENTRY(2681),
            REG_ENTRY(2682),
            REG_ENTRY(2683),
            REG_ENTRY(2684),
            REG_ENTRY(2685),
            REG_ENTRY(2686),
            REG_ENTRY(2687),
            REG_ENTRY(2688),
            REG_ENTRY(2689),
            REG_ENTRY(2690),
            REG_ENTRY(2691),
            REG_ENTRY(2692),
            REG_ENTRY(2693),
            REG_ENTRY(2694),
            REG_ENTRY(2695),
            REG_ENTRY(2696),
            REG_ENTRY(2697),
            REG_ENTRY(2698),
            REG_ENTRY(2699),
            REG_ENTRY(2700),
            REG_ENTRY(2701),
            REG_ENTRY(2702),
            REG_ENTRY(2703),
            REG_ENTRY(2704),
            REG_ENTRY(2705),
            REG_ENTRY(2706),
            REG_ENTRY(2707),
            REG_ENTRY(2708),
            REG_ENTRY(2709),
            REG_ENTRY(2710),
            REG_ENTRY(2711),
            REG_ENTRY(2712),
            REG_ENTRY(2713),
            REG_ENTRY(2714),
            REG_ENTRY(2715),
            REG_ENTRY(2716),
            REG_ENTRY(2717),
            REG_ENTRY(2718),
            REG_ENTRY(2719),
            REG_ENTRY(2720),
            REG_ENTRY(2721),
            REG_ENTRY(2722),
            REG_ENTRY(2723),
            REG_ENTRY(2724),
            REG_ENTRY(2725),
            REG_ENTRY(2726),
            REG_ENTRY(2727),
            REG_ENTRY(2728),
            REG_ENTRY(2729),
            REG_ENTRY(2730),
            REG_ENTRY(2731),
            REG_ENTRY(2732),
            REG_ENTRY(2733),
            REG_ENTRY(2734),
            REG_ENTRY(2735),
            REG_ENTRY(2736),
            REG_ENTRY(2737),
            REG_ENTRY(2738),
            REG_ENTRY(2739),
            REG_ENTRY(2740),
            REG_ENTRY(2741),
            REG_ENTRY(2742),
            REG_ENTRY(2743),
            REG_ENTRY(2744),
            REG_ENTRY(2745),
            REG_ENTRY(2746),
            REG_ENTRY(2747),
            REG_ENTRY(2748),
            REG_ENTRY(2749),
            REG_ENTRY(2750),
            REG_ENTRY(2751),
            REG_ENTRY(2752),
            REG_ENTRY(2753),
            REG_ENTRY(2754),
            REG_ENTRY(2755),
            REG_ENTRY(2756),
            REG_ENTRY(2757),
            REG_ENTRY(2758),
            REG_ENTRY(2759),
            REG_ENTRY(2760),
            REG_ENTRY(2761),
            REG_ENTRY(2762),
            REG_ENTRY(2763),
            REG_ENTRY(2764),
            REG_ENTRY(2765),
            REG_ENTRY(2766),
            REG_ENTRY(2767),
            REG_ENTRY(2768),
            REG_ENTRY(2769),
            REG_ENTRY(2770),
            REG_ENTRY(2771),
            REG_ENTRY(2772),
            REG_ENTRY(2773),
            REG_ENTRY(2774),
            REG_ENTRY(2775),
            REG_ENTRY(2776),
            REG_ENTRY(2777),
            REG_ENTRY(2778),
            REG_ENTRY(2779),
            REG_ENTRY(2780),
            REG_ENTRY(2781),
            REG_ENTRY(2782),
            REG_ENTRY(2783),
            REG_ENTRY(2784),
            REG_ENTRY(2785),
            REG_ENTRY(2786),
            REG_ENTRY(2787),
            REG_ENTRY(2788),
            REG_ENTRY(2789),
            REG_ENTRY(2790),
            REG_ENTRY(2791),
            REG_ENTRY(2792),
            REG_ENTRY(2793),
            REG_ENTRY(2794),
            REG_ENTRY(2795),
            REG_ENTRY(2796),
            REG_ENTRY(2797),
            REG_ENTRY(2798),
            REG_ENTRY(2799),
            REG_ENTRY(2800),
            REG_ENTRY(2801),
            REG_ENTRY(2802),
            REG_ENTRY(2803),
            REG_ENTRY(2804),
            REG_ENTRY(2805),
            REG_ENTRY(2806),
            REG_ENTRY(2807),
            REG_ENTRY(2808),
            REG_ENTRY(2809),
            REG_ENTRY(2810),
            REG_ENTRY(2811),
            REG_ENTRY(2812),
            REG_ENTRY(2813),
            REG_ENTRY(2814),
            REG_ENTRY(2815),
            REG_ENTRY(2816),
            REG_ENTRY(2817),
            REG_ENTRY(2818),
            REG_ENTRY(2819),
            REG_ENTRY(2820),
            REG_ENTRY(2821),
            REG_ENTRY(2822),
            REG_ENTRY(2823),
            REG_ENTRY(2824),
            REG_ENTRY(2825),
            REG_ENTRY(2826),
            REG_ENTRY(2827),
            REG_ENTRY(2828),
            REG_ENTRY(2829),
            REG_ENTRY(2830),
            REG_ENTRY(2831),
            REG_ENTRY(2832),
            REG_ENTRY(2833),
            REG_ENTRY(2834),
            REG_ENTRY(2835),
            REG_ENTRY(2836),
            REG_ENTRY(2837),
            REG_ENTRY(2838),
            REG_ENTRY(2839),
            REG_ENTRY(2840),
            REG_ENTRY(2841),
            REG_ENTRY(2842),
            REG_ENTRY(2843),
            REG_ENTRY(2844),
            REG_ENTRY(2845),
            REG_ENTRY(2846),
            REG_ENTRY(2847),
            REG_ENTRY(2848),
            REG_ENTRY(2849),
            REG_ENTRY(2850),
            REG_ENTRY(2851),
            REG_ENTRY(2852),
            REG_ENTRY(2853),
            REG_ENTRY(2854),
            REG_ENTRY(2855),
            REG_ENTRY(2856),
            REG_ENTRY(2857),
            REG_ENTRY(2858),
            REG_ENTRY(2859),
            REG_ENTRY(2860),
            REG_ENTRY(2861),
            REG_ENTRY(2862),
            REG_ENTRY(2863),
            REG_ENTRY(2864),
            REG_ENTRY(2865),
            REG_ENTRY(2866),
            REG_ENTRY(2867),
            REG_ENTRY(2868),
            REG_ENTRY(2869),
            REG_ENTRY(2870),
            REG_ENTRY(2871),
            REG_ENTRY(2872),
            REG_ENTRY(2873),
            REG_ENTRY(2874),
            REG_ENTRY(2875),
            REG_ENTRY(2876),
            REG_ENTRY(2877),
            REG_ENTRY(2878),
            REG_ENTRY(2879),
            REG_ENTRY(2880),
            REG_ENTRY(2881),
            REG_ENTRY(2882),
            REG_ENTRY(2883),
            REG_ENTRY(2884),
            REG_ENTRY(2885),
            REG_ENTRY(2886),
            REG_ENTRY(2887),
            REG_ENTRY(2888),
            REG_ENTRY(2889),
            REG_ENTRY(2890),
            REG_ENTRY(2891),
            REG_ENTRY(2892),
            REG_ENTRY(2893),
            REG_ENTRY(2894),
            REG_ENTRY(2895),
            REG_ENTRY(2896),
            REG_ENTRY(2897),
            REG_ENTRY(2898),
            REG_ENTRY(2899),
            REG_ENTRY(2900),
            REG_ENTRY(2901),
            REG_ENTRY(2902),
            REG_ENTRY(2903),
            REG_ENTRY(2904),
            REG_ENTRY(2905),
            REG_ENTRY(2906),
            REG_ENTRY(2907),
            REG_ENTRY(2908),
            REG_ENTRY(2909),
            REG_ENTRY(2910),
            REG_ENTRY(2911),
            REG_ENTRY(2912),
            REG_ENTRY(2913),
            REG_ENTRY(2914),
            REG_ENTRY(2915),
            REG_ENTRY(2916),
            REG_ENTRY(2917),
            REG_ENTRY(2918),
            REG_ENTRY(2919),
            REG_ENTRY(2920),
            REG_ENTRY(2921),
            REG_ENTRY(2922),
            REG_ENTRY(2923),
            REG_ENTRY(2924),
            REG_ENTRY(2925),
            REG_ENTRY(2926),
            REG_ENTRY(2927),
            REG_ENTRY(2928),
            REG_ENTRY(2929),
            REG_ENTRY(2930),
            REG_ENTRY(2931),
            REG_ENTRY(2932),
            REG_ENTRY(2933),
            REG_ENTRY(2934),
            REG_ENTRY(2935),
            REG_ENTRY(2936),
            REG_ENTRY(2937),
            REG_ENTRY(2938),
            REG_ENTRY(2939),
            REG_ENTRY(2940),
            REG_ENTRY(2941),
            REG_ENTRY(2942),
            REG_ENTRY(2943),
            REG_ENTRY(2944),
            REG_ENTRY(2945),
            REG_ENTRY(2946),
            REG_ENTRY(2947),
            REG_ENTRY(2948),
            REG_ENTRY(2949),
            REG_ENTRY(2950),
            REG_ENTRY(2951),
            REG_ENTRY(2952),
            REG_ENTRY(2953),
            REG_ENTRY(2954),
            REG_ENTRY(2955),
            REG_ENTRY(2956),
            REG_ENTRY(2957),
            REG_ENTRY(2958),
            REG_ENTRY(2959),
            REG_ENTRY(2960),
            REG_ENTRY(2961),
            REG_ENTRY(2962),
            REG_ENTRY(2963),
            REG_ENTRY(2964),
            REG_ENTRY(2965),
            REG_ENTRY(2966),
            REG_ENTRY(2967),
            REG_ENTRY(2968),
            REG_ENTRY(2969),
            REG_ENTRY(2970),
            REG_ENTRY(2971),
            REG_ENTRY(2972),
            REG_ENTRY(2973),
            REG_ENTRY(2974),
            REG_ENTRY(2975),
            REG_ENTRY(2976),
            REG_ENTRY(2977),
            REG_ENTRY(2978),
            REG_ENTRY(2979),
            REG_ENTRY(2980),
            REG_ENTRY(2981),
            REG_ENTRY(2982),
            REG_ENTRY(2983),
            REG_ENTRY(2984),
            REG_ENTRY(2985),
            REG_ENTRY(2986),
            REG_ENTRY(2987),
            REG_ENTRY(2988),
            REG_ENTRY(2989),
            REG_ENTRY(2990),
            REG_ENTRY(2991),
            REG_ENTRY(2992),
            REG_ENTRY(2993),
            REG_ENTRY(2994),
            REG_ENTRY(2995),
            REG_ENTRY(2996),
            REG_ENTRY(2997),
            REG_ENTRY(2998),
            REG_ENTRY(2999),
            REG_ENTRY(3000),
            REG_ENTRY(3001),
            REG_ENTRY(3002),
            REG_ENTRY(3003),
            REG_ENTRY(3004),
            REG_ENTRY(3005),
            REG_ENTRY(3006),
            REG_ENTRY(3007),
            REG_ENTRY(3008),
            REG_ENTRY(3009),
            REG_ENTRY(3010),
            REG_ENTRY(3011),
            REG_ENTRY(3012),
            REG_ENTRY(3013),
            REG_ENTRY(3014),
            REG_ENTRY(3015),
            REG_ENTRY(3016),
            REG_ENTRY(3017),
            REG_ENTRY(3018),
            REG_ENTRY(3019),
            REG_ENTRY(3020),
            REG_ENTRY(3021),
            REG_ENTRY(3022),
            REG_ENTRY(3023),
            REG_ENTRY(3024),
            REG_ENTRY(3025),
            REG_ENTRY(3026),
            REG_ENTRY(3027),
            REG_ENTRY(3028),
            REG_ENTRY(3029),
            REG_ENTRY(3030),
            REG_ENTRY(3031),
            REG_ENTRY(3032),
            REG_ENTRY(3033),
            REG_ENTRY(3034),
            REG_ENTRY(3035),
            REG_ENTRY(3036),
            REG_ENTRY(3037),
            REG_ENTRY(3038),
            REG_ENTRY(3039),
            REG_ENTRY(3040),
            REG_ENTRY(3041),
            REG_ENTRY(3042),
            REG_ENTRY(3043),
            REG_ENTRY(3044),
            REG_ENTRY(3045),
            REG_ENTRY(3046),
            REG_ENTRY(3047),
            REG_ENTRY(3048),
            REG_ENTRY(3049),
            REG_ENTRY(3050),
            REG_ENTRY(3051),
            REG_ENTRY(3052),
            REG_ENTRY(3053),
            REG_ENTRY(3054),
            REG_ENTRY(3055),
            REG_ENTRY(3056),
            REG_ENTRY(3057),
            REG_ENTRY(3058),
            REG_ENTRY(3059),
            REG_ENTRY(3060),
            REG_ENTRY(3061),
            REG_ENTRY(3062),
            REG_ENTRY(3063),
            REG_ENTRY(3064),
            REG_ENTRY(3065),
            REG_ENTRY(3066),
            REG_ENTRY(3067),
            REG_ENTRY(3068),
            REG_ENTRY(3069),
            REG_ENTRY(3070),
            REG_ENTRY(3071),
            REG_ENTRY(3072),
            REG_ENTRY(3073),
            REG_ENTRY(3074),
            REG_ENTRY(3075),
            REG_ENTRY(3076),
            REG_ENTRY(3077),
            REG_ENTRY(3078),
            REG_ENTRY(3079),
            REG_ENTRY(3080),
            REG_ENTRY(3081),
            REG_ENTRY(3082),
            REG_ENTRY(3083),
            REG_ENTRY(3084),
            REG_ENTRY(3085),
            REG_ENTRY(3086),
            REG_ENTRY(3087),
            REG_ENTRY(3088),
            REG_ENTRY(3089),
            REG_ENTRY(3090),
            REG_ENTRY(3091),
            REG_ENTRY(3092),
            REG_ENTRY(3093),
            REG_ENTRY(3094),
            REG_ENTRY(3095),
            REG_ENTRY(3096),
            REG_ENTRY(3097),
            REG_ENTRY(3098),
            REG_ENTRY(3099),
            REG_ENTRY(3100),
            REG_ENTRY(3101),
            REG_ENTRY(3102),
            REG_ENTRY(3103),
            REG_ENTRY(3104),
            REG_ENTRY(3105),
            REG_ENTRY(3106),
            REG_ENTRY(3107),
            REG_ENTRY(3108),
            REG_ENTRY(3109),
            REG_ENTRY(3110),
            REG_ENTRY(3111),
            REG_ENTRY(3112),
            REG_ENTRY(3113),
            REG_ENTRY(3114),
            REG_ENTRY(3115),
            REG_ENTRY(3116),
            REG_ENTRY(3117),
            REG_ENTRY(3118),
            REG_ENTRY(3119),
            REG_ENTRY(3120),
            REG_ENTRY(3121),
            REG_ENTRY(3122),
            REG_ENTRY(3123),
            REG_ENTRY(3124),
            REG_ENTRY(3125),
            REG_ENTRY(3126),
            REG_ENTRY(3127),
            REG_ENTRY(3128),
            REG_ENTRY(3129),
            REG_ENTRY(3130),
            REG_ENTRY(3131),
            REG_ENTRY(3132),
            REG_ENTRY(3133),
            REG_ENTRY(3134),
            REG_ENTRY(3135),
            REG_ENTRY(3136),
            REG_ENTRY(3137),
            REG_ENTRY(3138),
            REG_ENTRY(3139),
            REG_ENTRY(3140),
            REG_ENTRY(3141),
            REG_ENTRY(3142),
            REG_ENTRY(3143),
            REG_ENTRY(3144),
            REG_ENTRY(3145),
            REG_ENTRY(3146),
            REG_ENTRY(3147),
            REG_ENTRY(3148),
            REG_ENTRY(3149),
            REG_ENTRY(3150),
            REG_ENTRY(3151),
            REG_ENTRY(3152),
            REG_ENTRY(3153),
            REG_ENTRY(3154),
            REG_ENTRY(3155),
            REG_ENTRY(3156),
            REG_ENTRY(3157),
            REG_ENTRY(3158),
            REG_ENTRY(3159),
            REG_ENTRY(3160),
            REG_ENTRY(3161),
            REG_ENTRY(3162),
            REG_ENTRY(3163),
            REG_ENTRY(3164),
            REG_ENTRY(3165),
            REG_ENTRY(3166),
            REG_ENTRY(3167),
            REG_ENTRY(3168),
            REG_ENTRY(3169),
            REG_ENTRY(3170),
            REG_ENTRY(3171),
            REG_ENTRY(3172),
            REG_ENTRY(3173),
            REG_ENTRY(3174),
            REG_ENTRY(3175),
            REG_ENTRY(3176),
            REG_ENTRY(3177),
            REG_ENTRY(3178),
            REG_ENTRY(3179),
            REG_ENTRY(3180),
            REG_ENTRY(3181),
            REG_ENTRY(3182),
            REG_ENTRY(3183),
            REG_ENTRY(3184),
            REG_ENTRY(3185),
            REG_ENTRY(3186),
            REG_ENTRY(3187),
            REG_ENTRY(3188),
            REG_ENTRY(3189),
            REG_ENTRY(3190),
            REG_ENTRY(3191),
            REG_ENTRY(3192),
            REG_ENTRY(3193),
            REG_ENTRY(3194),
            REG_ENTRY(3195),
            REG_ENTRY(3196),
            REG_ENTRY(3197),
            REG_ENTRY(3198),
            REG_ENTRY(3199),
            REG_ENTRY(3200),
            REG_ENTRY(3201),
            REG_ENTRY(3202),
            REG_ENTRY(3203),
            REG_ENTRY(3204),
            REG_ENTRY(3205),
            REG_ENTRY(3206),
            REG_ENTRY(3207),
            REG_ENTRY(3208),
            REG_ENTRY(3209),
            REG_ENTRY(3210),
            REG_ENTRY(3211),
            REG_ENTRY(3212),
            REG_ENTRY(3213),
            REG_ENTRY(3214),
            REG_ENTRY(3215),
            REG_ENTRY(3216),
            REG_ENTRY(3217),
            REG_ENTRY(3218),
            REG_ENTRY(3219),
            REG_ENTRY(3220),
            REG_ENTRY(3221),
            REG_ENTRY(3222),
            REG_ENTRY(3223),
            REG_ENTRY(3224),
            REG_ENTRY(3225),
            REG_ENTRY(3226),
            REG_ENTRY(3227),
            REG_ENTRY(3228),
            REG_ENTRY(3229),
            REG_ENTRY(3230),
            REG_ENTRY(3231),
            REG_ENTRY(3232),
            REG_ENTRY(3233),
            REG_ENTRY(3234),
            REG_ENTRY(3235),
            REG_ENTRY(3236),
            REG_ENTRY(3237),
            REG_ENTRY(3238),
            REG_ENTRY(3239),
            REG_ENTRY(3240),
            REG_ENTRY(3241),
            REG_ENTRY(3242),
            REG_ENTRY(3243),
            REG_ENTRY(3244),
            REG_ENTRY(3245),
            REG_ENTRY(3246),
            REG_ENTRY(3247),
            REG_ENTRY(3248),
            REG_ENTRY(3249),
            REG_ENTRY(3250),
            REG_ENTRY(3251),
            REG_ENTRY(3252),
            REG_ENTRY(3253),
            REG_ENTRY(3254),
            REG_ENTRY(3255),
            REG_ENTRY(3256),
            REG_ENTRY(3257),
            REG_ENTRY(3258),
            REG_ENTRY(3259),
            REG_ENTRY(3260),
            REG_ENTRY(3261),
            REG_ENTRY(3262),
            REG_ENTRY(3263),
            REG_ENTRY(3264),
            REG_ENTRY(3265),
            REG_ENTRY(3266),
            REG_ENTRY(3267),
            REG_ENTRY(3268),
            REG_ENTRY(3269),
            REG_ENTRY(3270),
            REG_ENTRY(3271),
            REG_ENTRY(3272),
            REG_ENTRY(3273),
            REG_ENTRY(3274),
            REG_ENTRY(3275),
            REG_ENTRY(3276),
            REG_ENTRY(3277),
            REG_ENTRY(3278),
            REG_ENTRY(3279),
            REG_ENTRY(3280),
            REG_ENTRY(3281),
            REG_ENTRY(3282),
            REG_ENTRY(3283),
            REG_ENTRY(3284),
            REG_ENTRY(3285),
            REG_ENTRY(3286),
            REG_ENTRY(3287),
            REG_ENTRY(3288),
            REG_ENTRY(3289),
            REG_ENTRY(3290),
            REG_ENTRY(3291),
            REG_ENTRY(3292),
            REG_ENTRY(3293),
            REG_ENTRY(3294),
            REG_ENTRY(3295),
            REG_ENTRY(3296),
            REG_ENTRY(3297),
            REG_ENTRY(3298),
            REG_ENTRY(3299),
            REG_ENTRY(3300),
            REG_ENTRY(3301),
            REG_ENTRY(3302),
            REG_ENTRY(3303),
            REG_ENTRY(3304),
            REG_ENTRY(3305),
            REG_ENTRY(3306),
            REG_ENTRY(3307),
            REG_ENTRY(3308),
            REG_ENTRY(3309),
            REG_ENTRY(3310),
            REG_ENTRY(3311),
            REG_ENTRY(3312),
            REG_ENTRY(3313),
            REG_ENTRY(3314),
            REG_ENTRY(3315),
            REG_ENTRY(3316),
            REG_ENTRY(3317),
            REG_ENTRY(3318),
            REG_ENTRY(3319),
            REG_ENTRY(3320),
            REG_ENTRY(3321),
            REG_ENTRY(3322),
            REG_ENTRY(3323),
            REG_ENTRY(3324),
            REG_ENTRY(3325),
            REG_ENTRY(3326),
            REG_ENTRY(3327),
            REG_ENTRY(3328),
            REG_ENTRY(3329),
            REG_ENTRY(3330),
            REG_ENTRY(3331),
            REG_ENTRY(3332),
            REG_ENTRY(3333),
            REG_ENTRY(3334),
            REG_ENTRY(3335),
            REG_ENTRY(3336),
            REG_ENTRY(3337),
            REG_ENTRY(3338),
            REG_ENTRY(3339),
            REG_ENTRY(3340),
            REG_ENTRY(3341),
            REG_ENTRY(3342),
            REG_ENTRY(3343),
            REG_ENTRY(3344),
            REG_ENTRY(3345),
            REG_ENTRY(3346),
            REG_ENTRY(3347),
            REG_ENTRY(3348),
            REG_ENTRY(3349),
            REG_ENTRY(3350),
            REG_ENTRY(3351),
            REG_ENTRY(3352),
            REG_ENTRY(3353),
            REG_ENTRY(3354),
            REG_ENTRY(3355),
            REG_ENTRY(3356),
            REG_ENTRY(3357),
            REG_ENTRY(3358),
            REG_ENTRY(3359),
            REG_ENTRY(3360),
            REG_ENTRY(3361),
            REG_ENTRY(3362),
            REG_ENTRY(3363),
            REG_ENTRY(3364),
            REG_ENTRY(3365),
            REG_ENTRY(3366),
            REG_ENTRY(3367),
            REG_ENTRY(3368),
            REG_ENTRY(3369),
            REG_ENTRY(3370),
            REG_ENTRY(3371),
            REG_ENTRY(3372),
            REG_ENTRY(3373),
            REG_ENTRY(3374),
            REG_ENTRY(3375),
            REG_ENTRY(3376),
            REG_ENTRY(3377),
            REG_ENTRY(3378),
            REG_ENTRY(3379),
            REG_ENTRY(3380),
            REG_ENTRY(3381),
            REG_ENTRY(3382),
            REG_ENTRY(3383),
            REG_ENTRY(3384),
            REG_ENTRY(3385),
            REG_ENTRY(3386),
            REG_ENTRY(3387),
            REG_ENTRY(3388),
            REG_ENTRY(3389),
            REG_ENTRY(3390),
            REG_ENTRY(3391),
            REG_ENTRY(3392),
            REG_ENTRY(3393),
            REG_ENTRY(3394),
            REG_ENTRY(3395),
            REG_ENTRY(3396),
            REG_ENTRY(3397),
            REG_ENTRY(3398),
            REG_ENTRY(3399),
            REG_ENTRY(3400),
            REG_ENTRY(3401),
            REG_ENTRY(3402),
            REG_ENTRY(3403),
            REG_ENTRY(3404),
            REG_ENTRY(3405),
            REG_ENTRY(3406),
            REG_ENTRY(3407),
            REG_ENTRY(3408),
            REG_ENTRY(3409),
            REG_ENTRY(3410),
            REG_ENTRY(3411),
            REG_ENTRY(3412),
            REG_ENTRY(3413),
            REG_ENTRY(3414),
            REG_ENTRY(3415),
            REG_ENTRY(3416),
            REG_ENTRY(3417),
            REG_ENTRY(3418),
            REG_ENTRY(3419),
            REG_ENTRY(3420),
            REG_ENTRY(3421),
            REG_ENTRY(3422),
            REG_ENTRY(3423),
            REG_ENTRY(3424),
            REG_ENTRY(3425),
            REG_ENTRY(3426),
            REG_ENTRY(3427),
            REG_ENTRY(3428),
            REG_ENTRY(3429),
            REG_ENTRY(3430),
            REG_ENTRY(3431),
            REG_ENTRY(3432),
            REG_ENTRY(3433),
            REG_ENTRY(3434),
            REG_ENTRY(3435),
            REG_ENTRY(3436),
            REG_ENTRY(3437),
            REG_ENTRY(3438),
            REG_ENTRY(3439),
            REG_ENTRY(3440),
            REG_ENTRY(3441),
            REG_ENTRY(3442),
            REG_ENTRY(3443),
            REG_ENTRY(3444),
            REG_ENTRY(3445),
            REG_ENTRY(3446),
            REG_ENTRY(3447),
            REG_ENTRY(3448),
            REG_ENTRY(3449),
            REG_ENTRY(3450),
            REG_ENTRY(3451),
            REG_ENTRY(3452),
            REG_ENTRY(3453),
            REG_ENTRY(3454),
            REG_ENTRY(3455),
            REG_ENTRY(3456),
            REG_ENTRY(3457),
            REG_ENTRY(3458),
            REG_ENTRY(3459),
            REG_ENTRY(3460),
            REG_ENTRY(3461),
            REG_ENTRY(3462),
            REG_ENTRY(3463),
            REG_ENTRY(3464),
            REG_ENTRY(3465),
            REG_ENTRY(3466),
            REG_ENTRY(3467),
            REG_ENTRY(3468),
            REG_ENTRY(3469),
            REG_ENTRY(3470),
            REG_ENTRY(3471),
            REG_ENTRY(3472),
            REG_ENTRY(3473),
            REG_ENTRY(3474),
            REG_ENTRY(3475),
            REG_ENTRY(3476),
            REG_ENTRY(3477),
            REG_ENTRY(3478),
            REG_ENTRY(3479),
            REG_ENTRY(3480),
            REG_ENTRY(3481),
            REG_ENTRY(3482),
            REG_ENTRY(3483),
            REG_ENTRY(3484),
            REG_ENTRY(3485),
            REG_ENTRY(3486),
            REG_ENTRY(3487),
            REG_ENTRY(3488),
            REG_ENTRY(3489),
            REG_ENTRY(3490),
            REG_ENTRY(3491),
            REG_ENTRY(3492),
            REG_ENTRY(3493),
            REG_ENTRY(3494),
            REG_ENTRY(3495),
            REG_ENTRY(3496),
            REG_ENTRY(3497),
            REG_ENTRY(3498),
            REG_ENTRY(3499),
            REG_ENTRY(3500),
            REG_ENTRY(3501),
            REG_ENTRY(3502),
            REG_ENTRY(3503),
            REG_ENTRY(3504),
            REG_ENTRY(3505),
            REG_ENTRY(3506),
            REG_ENTRY(3507),
            REG_ENTRY(3508),
            REG_ENTRY(3509),
            REG_ENTRY(3510),
            REG_ENTRY(3511),
            REG_ENTRY(3512),
            REG_ENTRY(3513),
            REG_ENTRY(3514),
            REG_ENTRY(3515),
            REG_ENTRY(3516),
            REG_ENTRY(3517),
            REG_ENTRY(3518),
            REG_ENTRY(3519),
            REG_ENTRY(3520),
            REG_ENTRY(3521),
            REG_ENTRY(3522),
            REG_ENTRY(3523),
            REG_ENTRY(3524),
            REG_ENTRY(3525),
            REG_ENTRY(3526),
            REG_ENTRY(3527),
            REG_ENTRY(3528),
            REG_ENTRY(3529),
            REG_ENTRY(3530),
            REG_ENTRY(3531),
            REG_ENTRY(3532),
            REG_ENTRY(3533),
            REG_ENTRY(3534),
            REG_ENTRY(3535),
            REG_ENTRY(3536),
            REG_ENTRY(3537),
            REG_ENTRY(3538),
            REG_ENTRY(3539),
            REG_ENTRY(3540),
            REG_ENTRY(3541),
            REG_ENTRY(3542),
            REG_ENTRY(3543),
            REG_ENTRY(3544),
            REG_ENTRY(3545),
            REG_ENTRY(3546),
            REG_ENTRY(3547),
            REG_ENTRY(3548),
            REG_ENTRY(3549),
            REG_ENTRY(3550),
            REG_ENTRY(3551),
            REG_ENTRY(3552),
            REG_ENTRY(3553),
            REG_ENTRY(3554),
            REG_ENTRY(3555),
            REG_ENTRY(3556),
            REG_ENTRY(3557),
            REG_ENTRY(3558),
            REG_ENTRY(3559),
            REG_ENTRY(3560),
            REG_ENTRY(3561),
            REG_ENTRY(3562),
            REG_ENTRY(3563),
            REG_ENTRY(3564),
            REG_ENTRY(3565),
            REG_ENTRY(3566),
            REG_ENTRY(3567),
            REG_ENTRY(3568),
            REG_ENTRY(3569),
            REG_ENTRY(3570),
            REG_ENTRY(3571),
            REG_ENTRY(3572),
            REG_ENTRY(3573),
            REG_ENTRY(3574),
            REG_ENTRY(3575),
            REG_ENTRY(3576),
            REG_ENTRY(3577),
            REG_ENTRY(3578),
            REG_ENTRY(3579),
            REG_ENTRY(3580),
            REG_ENTRY(3581),
            REG_ENTRY(3582),
            REG_ENTRY(3583),
            REG_ENTRY(3584),
            REG_ENTRY(3585),
            REG_ENTRY(3586),
            REG_ENTRY(3587),
            REG_ENTRY(3588),
            REG_ENTRY(3589),
            REG_ENTRY(3590),
            REG_ENTRY(3591),
            REG_ENTRY(3592),
            REG_ENTRY(3593),
            REG_ENTRY(3594),
            REG_ENTRY(3595),
            REG_ENTRY(3596),
            REG_ENTRY(3597),
            REG_ENTRY(3598),
            REG_ENTRY(3599),
            REG_ENTRY(3600),
            REG_ENTRY(3601),
            REG_ENTRY(3602),
            REG_ENTRY(3603),
            REG_ENTRY(3604),
            REG_ENTRY(3605),
            REG_ENTRY(3606),
            REG_ENTRY(3607),
            REG_ENTRY(3608),
            REG_ENTRY(3609),
            REG_ENTRY(3610),
            REG_ENTRY(3611),
            REG_ENTRY(3612),
            REG_ENTRY(3613),
            REG_ENTRY(3614),
            REG_ENTRY(3615),
            REG_ENTRY(3616),
            REG_ENTRY(3617),
            REG_ENTRY(3618),
            REG_ENTRY(3619),
            REG_ENTRY(3620),
            REG_ENTRY(3621),
            REG_ENTRY(3622),
            REG_ENTRY(3623),
            REG_ENTRY(3624),
            REG_ENTRY(3625),
            REG_ENTRY(3626),
            REG_ENTRY(3627),
            REG_ENTRY(3628),
            REG_ENTRY(3629),
            REG_ENTRY(3630),
            REG_ENTRY(3631),
            REG_ENTRY(3632),
            REG_ENTRY(3633),
            REG_ENTRY(3634),
            REG_ENTRY(3635),
            REG_ENTRY(3636),
            REG_ENTRY(3637),
            REG_ENTRY(3638),
            REG_ENTRY(3639),
            REG_ENTRY(3640),
            REG_ENTRY(3641),
            REG_ENTRY(3642),
            REG_ENTRY(3643),
            REG_ENTRY(3644),
            REG_ENTRY(3645),
            REG_ENTRY(3646),
            REG_ENTRY(3647),
            REG_ENTRY(3648),
            REG_ENTRY(3649),
            REG_ENTRY(3650),
            REG_ENTRY(3651),
            REG_ENTRY(3652),
            REG_ENTRY(3653),
            REG_ENTRY(3654),
            REG_ENTRY(3655),
            REG_ENTRY(3656),
            REG_ENTRY(3657),
            REG_ENTRY(3658),
            REG_ENTRY(3659),
            REG_ENTRY(3660),
            REG_ENTRY(3661),
            REG_ENTRY(3662),
            REG_ENTRY(3663),
            REG_ENTRY(3664),
            REG_ENTRY(3665),
            REG_ENTRY(3666),
            REG_ENTRY(3667),
            REG_ENTRY(3668),
            REG_ENTRY(3669),
            REG_ENTRY(3670),
            REG_ENTRY(3671),
            REG_ENTRY(3672),
            REG_ENTRY(3673),
            REG_ENTRY(3674),
            REG_ENTRY(3675),
            REG_ENTRY(3676),
            REG_ENTRY(3677),
            REG_ENTRY(3678),
            REG_ENTRY(3679),
            REG_ENTRY(3680),
            REG_ENTRY(3681),
            REG_ENTRY(3682),
            REG_ENTRY(3683),
            REG_ENTRY(3684),
            REG_ENTRY(3685),
            REG_ENTRY(3686),
            REG_ENTRY(3687),
            REG_ENTRY(3688),
            REG_ENTRY(3689),
            REG_ENTRY(3690),
            REG_ENTRY(3691),
            REG_ENTRY(3692),
            REG_ENTRY(3693),
            REG_ENTRY(3694),
            REG_ENTRY(3695),
            REG_ENTRY(3696),
            REG_ENTRY(3697),
            REG_ENTRY(3698),
            REG_ENTRY(3699),
            REG_ENTRY(3700),
            REG_ENTRY(3701),
            REG_ENTRY(3702),
            REG_ENTRY(3703),
            REG_ENTRY(3704),
            REG_ENTRY(3705),
            REG_ENTRY(3706),
            REG_ENTRY(3707),
            REG_ENTRY(3708),
            REG_ENTRY(3709),
            REG_ENTRY(3710),
            REG_ENTRY(3711),
            REG_ENTRY(3712),
            REG_ENTRY(3713),
            REG_ENTRY(3714),
            REG_ENTRY(3715),
            REG_ENTRY(3716),
            REG_ENTRY(3717),
            REG_ENTRY(3718),
            REG_ENTRY(3719),
            REG_ENTRY(3720),
            REG_ENTRY(3721),
            REG_ENTRY(3722),
            REG_ENTRY(3723),
            REG_ENTRY(3724),
            REG_ENTRY(3725),
            REG_ENTRY(3726),
            REG_ENTRY(3727),
            REG_ENTRY(3728),
            REG_ENTRY(3729),
            REG_ENTRY(3730),
            REG_ENTRY(3731),
            REG_ENTRY(3732),
            REG_ENTRY(3733),
            REG_ENTRY(3734),
            REG_ENTRY(3735),
            REG_ENTRY(3736),
            REG_ENTRY(3737),
            REG_ENTRY(3738),
            REG_ENTRY(3739),
            REG_ENTRY(3740),
            REG_ENTRY(3741),
            REG_ENTRY(3742),
            REG_ENTRY(3743),
            REG_ENTRY(3744),
            REG_ENTRY(3745),
            REG_ENTRY(3746),
            REG_ENTRY(3747),
            REG_ENTRY(3748),
            REG_ENTRY(3749),
            REG_ENTRY(3750),
            REG_ENTRY(3751),
            REG_ENTRY(3752),
            REG_ENTRY(3753),
            REG_ENTRY(3754),
            REG_ENTRY(3755),
            REG_ENTRY(3756),
            REG_ENTRY(3757),
            REG_ENTRY(3758),
            REG_ENTRY(3759),
            REG_ENTRY(3760),
            REG_ENTRY(3761),
            REG_ENTRY(3762),
            REG_ENTRY(3763),
            REG_ENTRY(3764),
            REG_ENTRY(3765),
            REG_ENTRY(3766),
            REG_ENTRY(3767),
            REG_ENTRY(3768),
            REG_ENTRY(3769),
            REG_ENTRY(3770),
            REG_ENTRY(3771),
            REG_ENTRY(3772),
            REG_ENTRY(3773),
            REG_ENTRY(3774),
            REG_ENTRY(3775),
            REG_ENTRY(3776),
            REG_ENTRY(3777),
            REG_ENTRY(3778),
            REG_ENTRY(3779),
            REG_ENTRY(3780),
            REG_ENTRY(3781),
            REG_ENTRY(3782),
            REG_ENTRY(3783),
            REG_ENTRY(3784),
            REG_ENTRY(3785),
            REG_ENTRY(3786),
            REG_ENTRY(3787),
            REG_ENTRY(3788),
            REG_ENTRY(3789),
            REG_ENTRY(3790),
            REG_ENTRY(3791),
            REG_ENTRY(3792),
            REG_ENTRY(3793),
            REG_ENTRY(3794),
            REG_ENTRY(3795),
            REG_ENTRY(3796),
            REG_ENTRY(3797),
            REG_ENTRY(3798),
            REG_ENTRY(3799),
            REG_ENTRY(3800),
            REG_ENTRY(3801),
            REG_ENTRY(3802),
            REG_ENTRY(3803),
            REG_ENTRY(3804),
            REG_ENTRY(3805),
            REG_ENTRY(3806),
            REG_ENTRY(3807),
            REG_ENTRY(3808),
            REG_ENTRY(3809),
            REG_ENTRY(3810),
            REG_ENTRY(3811),
            REG_ENTRY(3812),
            REG_ENTRY(3813),
            REG_ENTRY(3814),
            REG_ENTRY(3815),
            REG_ENTRY(3816),
            REG_ENTRY(3817),
            REG_ENTRY(3818),
            REG_ENTRY(3819),
            REG_ENTRY(3820),
            REG_ENTRY(3821),
            REG_ENTRY(3822),
            REG_ENTRY(3823),
            REG_ENTRY(3824),
            REG_ENTRY(3825),
            REG_ENTRY(3826),
            REG_ENTRY(3827),
            REG_ENTRY(3828),
            REG_ENTRY(3829),
            REG_ENTRY(3830),
            REG_ENTRY(3831),
            REG_ENTRY(3832),
            REG_ENTRY(3833),
            REG_ENTRY(3834),
            REG_ENTRY(3835),
            REG_ENTRY(3836),
            REG_ENTRY(3837),
            REG_ENTRY(3838),
            REG_ENTRY(3839),
            REG_ENTRY(3840),
            REG_ENTRY(3841),
            REG_ENTRY(3842),
            REG_ENTRY(3843),
            REG_ENTRY(3844),
            REG_ENTRY(3845),
            REG_ENTRY(3846),
            REG_ENTRY(3847),
            REG_ENTRY(3848),
            REG_ENTRY(3849),
            REG_ENTRY(3850),
            REG_ENTRY(3851),
            REG_ENTRY(3852),
            REG_ENTRY(3853),
            REG_ENTRY(3854),
            REG_ENTRY(3855),
            REG_ENTRY(3856),
            REG_ENTRY(3857),
            REG_ENTRY(3858),
            REG_ENTRY(3859),
            REG_ENTRY(3860),
            REG_ENTRY(3861),
            REG_ENTRY(3862),
            REG_ENTRY(3863),
            REG_ENTRY(3864),
            REG_ENTRY(3865),
            REG_ENTRY(3866),
            REG_ENTRY(3867),
            REG_ENTRY(3868),
            REG_ENTRY(3869),
            REG_ENTRY(3870),
            REG_ENTRY(3871),
            REG_ENTRY(3872),
            REG_ENTRY(3873),
            REG_ENTRY(3874),
            REG_ENTRY(3875),
            REG_ENTRY(3876),
            REG_ENTRY(3877),
            REG_ENTRY(3878),
            REG_ENTRY(3879),
            REG_ENTRY(3880),
            REG_ENTRY(3881),
            REG_ENTRY(3882),
            REG_ENTRY(3883),
            REG_ENTRY(3884),
            REG_ENTRY(3885),
            REG_ENTRY(3886),
            REG_ENTRY(3887),
            REG_ENTRY(3888),
            REG_ENTRY(3889),
            REG_ENTRY(3890),
            REG_ENTRY(3891),
            REG_ENTRY(3892),
            REG_ENTRY(3893),
            REG_ENTRY(3894),
            REG_ENTRY(3895),
            REG_ENTRY(3896),
            REG_ENTRY(3897),
            REG_ENTRY(3898),
            REG_ENTRY(3899),
            REG_ENTRY(3900),
            REG_ENTRY(3901),
            REG_ENTRY(3902),
            REG_ENTRY(3903),
            REG_ENTRY(3904),
            REG_ENTRY(3905),
            REG_ENTRY(3906),
            REG_ENTRY(3907),
            REG_ENTRY(3908),
            REG_ENTRY(3909),
            REG_ENTRY(3910),
            REG_ENTRY(3911),
            REG_ENTRY(3912),
            REG_ENTRY(3913),
            REG_ENTRY(3914),
            REG_ENTRY(3915),
            REG_ENTRY(3916),
            REG_ENTRY(3917),
            REG_ENTRY(3918),
            REG_ENTRY(3919),
            REG_ENTRY(3920),
            REG_ENTRY(3921),
            REG_ENTRY(3922),
            REG_ENTRY(3923),
            REG_ENTRY(3924),
            REG_ENTRY(3925),
            REG_ENTRY(3926),
            REG_ENTRY(3927),
            REG_ENTRY(3928),
            REG_ENTRY(3929),
            REG_ENTRY(3930),
            REG_ENTRY(3931),
            REG_ENTRY(3932),
            REG_ENTRY(3933),
            REG_ENTRY(3934),
            REG_ENTRY(3935),
            REG_ENTRY(3936),
            REG_ENTRY(3937),
            REG_ENTRY(3938),
            REG_ENTRY(3939),
            REG_ENTRY(3940),
            REG_ENTRY(3941),
            REG_ENTRY(3942),
            REG_ENTRY(3943),
            REG_ENTRY(3944),
            REG_ENTRY(3945),
            REG_ENTRY(3946),
            REG_ENTRY(3947),
            REG_ENTRY(3948),
            REG_ENTRY(3949),
            REG_ENTRY(3950),
            REG_ENTRY(3951),
            REG_ENTRY(3952),
            REG_ENTRY(3953),
            REG_ENTRY(3954),
            REG_ENTRY(3955),
            REG_ENTRY(3956),
            REG_ENTRY(3957),
            REG_ENTRY(3958),
            REG_ENTRY(3959),
            REG_ENTRY(3960),
            REG_ENTRY(3961),
            REG_ENTRY(3962),
            REG_ENTRY(3963),
            REG_ENTRY(3964),
            REG_ENTRY(3965),
            REG_ENTRY(3966),
            REG_ENTRY(3967),
            REG_ENTRY(3968),
            REG_ENTRY(3969),
            REG_ENTRY(3970),
            REG_ENTRY(3971),
            REG_ENTRY(3972),
            REG_ENTRY(3973),
            REG_ENTRY(3974),
            REG_ENTRY(3975),
            REG_ENTRY(3976),
            REG_ENTRY(3977),
            REG_ENTRY(3978),
            REG_ENTRY(3979),
            REG_ENTRY(3980),
            REG_ENTRY(3981),
            REG_ENTRY(3982),
            REG_ENTRY(3983),
            REG_ENTRY(3984),
            REG_ENTRY(3985),
            REG_ENTRY(3986),
            REG_ENTRY(3987),
            REG_ENTRY(3988),
            REG_ENTRY(3989),
            REG_ENTRY(3990),
            REG_ENTRY(3991),
            REG_ENTRY(3992),
            REG_ENTRY(3993),
            REG_ENTRY(3994),
            REG_ENTRY(3995),
            REG_ENTRY(3996),
            REG_ENTRY(3997),
            REG_ENTRY(3998),
            REG_ENTRY(3999),
            REG_ENTRY(4000),
            REG_ENTRY(4001),
            REG_ENTRY(4002),
            REG_ENTRY(4003),
            REG_ENTRY(4004),
            REG_ENTRY(4005),
            REG_ENTRY(4006),
            REG_ENTRY(4007),
            REG_ENTRY(4008),
            REG_ENTRY(4009),
            REG_ENTRY(4010),
            REG_ENTRY(4011),
            REG_ENTRY(4012),
            REG_ENTRY(4013),
            REG_ENTRY(4014),
            REG_ENTRY(4015),
            REG_ENTRY(4016),
            REG_ENTRY(4017),
            REG_ENTRY(4018),
            REG_ENTRY(4019),
            REG_ENTRY(4020),
            REG_ENTRY(4021),
            REG_ENTRY(4022),
            REG_ENTRY(4023),
            REG_ENTRY(4024),
            REG_ENTRY(4025),
            REG_ENTRY(4026),
            REG_ENTRY(4027),
            REG_ENTRY(4028),
            REG_ENTRY(4029),
            REG_ENTRY(4030),
            REG_ENTRY(4031),
            REG_ENTRY(4032),
            REG_ENTRY(4033),
            REG_ENTRY(4034),
            REG_ENTRY(4035),
            REG_ENTRY(4036),
            REG_ENTRY(4037),
            REG_ENTRY(4038),
            REG_ENTRY(4039),
            REG_ENTRY(4040),
            REG_ENTRY(4041),
            REG_ENTRY(4042),
            REG_ENTRY(4043),
            REG_ENTRY(4044),
            REG_ENTRY(4045),
            REG_ENTRY(4046),
            REG_ENTRY(4047),
            REG_ENTRY(4048),
            REG_ENTRY(4049),
            REG_ENTRY(4050),
            REG_ENTRY(4051),
            REG_ENTRY(4052),
            REG_ENTRY(4053),
            REG_ENTRY(4054),
            REG_ENTRY(4055),
            REG_ENTRY(4056),
            REG_ENTRY(4057),
            REG_ENTRY(4058),
            REG_ENTRY(4059),
            REG_ENTRY(4060),
            REG_ENTRY(4061),
            REG_ENTRY(4062),
            REG_ENTRY(4063),
            REG_ENTRY(4064),
            REG_ENTRY(4065),
            REG_ENTRY(4066),
            REG_ENTRY(4067),
            REG_ENTRY(4068),
            REG_ENTRY(4069),
            REG_ENTRY(4070),
            REG_ENTRY(4071),
            REG_ENTRY(4072),
            REG_ENTRY(4073),
            REG_ENTRY(4074),
            REG_ENTRY(4075),
            REG_ENTRY(4076),
            REG_ENTRY(4077),
            REG_ENTRY(4078),
            REG_ENTRY(4079),
            REG_ENTRY(4080),
            REG_ENTRY(4081),
            REG_ENTRY(4082),
            REG_ENTRY(4083),
            REG_ENTRY(4084),
            REG_ENTRY(4085),
            REG_ENTRY(4086),
            REG_ENTRY(4087),
            REG_ENTRY(4088),
            REG_ENTRY(4089),
            REG_ENTRY(4090),
            REG_ENTRY(4091),
            REG_ENTRY(4092),
            REG_ENTRY(4093),
            REG_ENTRY(4094),
            REG_ENTRY(4095),
            /* sorry for this; but since it's a hacker module, it shouldn't be that bad */
            /* 256 is not enough... */
            MENU_EOL,
        }
    }
};

static MENU_UPDATE_FUNC(show_update)
{
    static struct tm tm;
    if (show_what == SHOW_MODIFIED_SINCE_TIMESTAMP)
        MENU_SET_VALUE("Modified since %02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    else
        LoadCalendarFromRTC(&tm);

    int changed = 0;
    
    //~ int err = checktree(regs_tree.root, regs_tree.compar);
    //~ MENU_SET_WARNING(MENU_WARN_ADVICE, "AVL tree %x", err);
    
    int current_time = get_ms_clock();
    static int last_update = 0;
    static int prev_show_what = 0;
    int show_what_changed = show_what != prev_show_what;
    prev_show_what = show_what;
    
    for (int reg = 0; reg < reg_num; reg++)
    {
        /* XXX: change this if you ever add or remove menu entries */
        struct menu_entry * entry = &(adtg_gui_menu[0].children[reg + 2]);
        
        if (get_reg_from_priv(entry->priv) != reg)
            break;

        int visible = 0;

        switch (show_what)
        {
            case SHOW_KNOWN_ONLY:
            {
                for (int i = 0; i < COUNT(known_regs); i++)
                {
                    if (known_match(i, reg))
                    {
                        visible = 1;
                        break;
                    }
                }
                break;
            }
            case SHOW_MODIFIED_SINCE_TIMESTAMP:
            {
                visible = regs[reg].val != regs[reg].prev_val;
                break;
            }
            case SHOW_MODIFIED_AT_LEAST_TWICE:
            {
                visible = regs[reg].num_changes > 1;
                break;
            }
            case SHOW_OVERRIDEN:
            {
                visible = regs[reg].override_enabled;
                break;
            }
            case SHOW_FPS_TIMERS:
            {
                visible = (regs[reg].dst == 0xC0F0) && (regs[reg].reg == 0x6014 || regs[reg].reg == 0x6008);
                break;
            }
            case SHOW_DISPLAY_REGS:
            {
                /* C0F14nnn */
                visible = (regs[reg].dst == 0xC0F1) && ((regs[reg].reg & 0xF000) == 0x4000);
                break;
            }
            case SHOW_IMAGE_SIZE_REGS:
            {
                if ((regs[reg].dst == 0xC0F0) && ((regs[reg].reg & 0xF000) == 0x6000))
                {
                    visible = 1;
                    break;
                }
                for (int i = 0; i < COUNT(known_regs); i++)
                {
                    if (known_match(i, reg))
                    {
                        if (
                            strstr(known_regs[i].description, "esolution") ||
                            strstr(known_regs[i].description, "idth") ||
                            strstr(known_regs[i].description, "eight") ||
                            strstr(known_regs[i].description, "ine count") ||
                            strstr(known_regs[i].description, "dwSrFstAdtg") ||
                        0)
                        {
                            visible = 1;
                            break;
                        }
                    }
                }
                break;
            }
            case SHOW_ALL:
            {
                visible = 1;
                break;
            }
        }
        
        if (regs[reg].num_changes > 100 && !regs[reg].override_enabled)
        {
            /* das ist noise */
            visible = 0;
        }
        
        if (!digic_intercept && (regs[reg].dst & 0xF000) == 0xC000)
        {
            /* hide previously-intercepted DIGIC registers if we disabled the option */
            visible = 0;
        }

        if (entry->shidden != !visible)
        {
            if (current_time - last_update < 3000 && !show_what_changed)
            {
                /* prevent update flood in menu */
                MENU_SET_RINFO("Wait");
                return;
            }

            entry->shidden = !visible;
            changed = 1;
        }

        if (show_what != SHOW_MODIFIED_SINCE_TIMESTAMP)
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
        last_update = current_time;
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
        ADTG_WRITE_FUNC = 0x11640;
        CMOS_WRITE_FUNC = 0x119CC;
        CMOS2_WRITE_FUNC = 0x11784;
        CMOS16_WRITE_FUNC = 0x11AB8;
        ENGIO_WRITE_FUNC = 0xFF290F98;
        ENG_DRV_OUT_FUNC = 0xFF290C80;
    }
    else if (is_camera("5D2", "2.1.2"))
    {
        ADTG_WRITE_FUNC = 0xffa35cbc;
        CMOS_WRITE_FUNC = 0xffa35e70;
        ENGIO_WRITE_FUNC = 0xFF9A5618;  // from stubs
        //~ ENG_DRV_OUT_FUNC = 0xff9a54a8;  /* this causes ADTG hook to stop working (why?) */
        ENG_DRV_OUTS_FUNC = 0xff9a5554;
        SEND_DATA_TO_DFE_FUNC = 0xff9b1d94;
        //~ SCS_DUMMY_READOUT_DONE_FUNC = 0xff880600;
    }
    else if (is_camera("500D", "1.1.1")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg70325#msg70325
    {
        ADTG_WRITE_FUNC = 0xFF22F8F4; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0xFF22F9DC; //"[REG] ############ Start CMOS"
        ENGIO_WRITE_FUNC = 0xFF190CF4;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF190B84;
    }
    else if (is_camera("550D", "1.0.9")) // http://www.magiclantern.fm/forum/index.php?topic=6751.msg61551#msg61551
    {
        ADTG_WRITE_FUNC = 0xff27ee34;
        CMOS_WRITE_FUNC = 0xff27f028;
        ENGIO_WRITE_FUNC = 0xFF1C15CC;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF1C1260;
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
        ENGIO_WRITE_FUNC = 0xFF97D904;  // from stubs
        ENG_DRV_OUT_FUNC = 0xff97d794;
        SEND_DATA_TO_DFE_FUNC = 0xffa71598;
    }
    else if (is_camera("6D", "1.1.6")) // from 1% (match 6D.113), JL, checked by Maqs
    {
        CMOS_WRITE_FUNC = 0x2445C; //"[REG] ############ Start CMOS OC_KICK"
        CMOS2_WRITE_FUNC = 0x2420C; //"[REG] ############ Start CMOS"
        ADTG_WRITE_FUNC = 0x24108; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS16_WRITE_FUNC = 0x24548; //"[REG] ############ Start CMOS16 OC_KICK"
        ENGIO_WRITE_FUNC = 0xFF2AE134;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF2ADE1C;
    }
    else if (is_camera("EOSM", "2.0.2")) // from 1%
    {
        ADTG_WRITE_FUNC = 0x2986C;
        CMOS_WRITE_FUNC = 0x2998C;
	ENGIO_WRITE_FUNC = 0xFF2C19AC;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF2C1694;
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
    else if (is_camera("700D", "1.1.5"))
    {
        ADTG_WRITE_FUNC = 0x178FC; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0x17A1C; //"[REG] ############ Start CMOS"
    }
    else if (is_camera("100D", "1.0.1"))
    {
        ADTG_WRITE_FUNC = 0x47144; //"[REG] @@@@@@@@@@@@ Start ADTG[CS:%lx]"
        CMOS_WRITE_FUNC = 0x475B8; //"[REG] ############ Start CMOS"
        ENGIO_WRITE_FUNC = 0xFF2B2460;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF2B2148;
    }
    else if (is_camera("1100D", "1.0.5"))
    {
        ADTG_WRITE_FUNC = 0xFF2DBB28;
        CMOS_WRITE_FUNC = 0xFF2DBD1C;
        ENGIO_WRITE_FUNC = 0xFF1D4C8C;  // from stubs
        ENG_DRV_OUT_FUNC = 0xFF1D48C8;
    }
    else return CBR_RET_ERROR;

    regs_tree.compar = cmp_reg;
    regs_tree.root = 0;

    menu_add("Debug", adtg_gui_menu, COUNT(adtg_gui_menu));
    ASSERT(adtg_gui_menu->children->parent_menu);
    adtg_gui_menu->children->parent_menu->no_name_lookup = 1;

    /* check known registers for duplicates */
    for (int i = 0; i < COUNT(known_regs); i++)
    {
        for (int j = 0; j < i; j++)
        {
            if ((known_regs[i].dst == known_regs[j].dst) &&
                (known_regs[i].reg == known_regs[j].reg))
            {
                NotifyBox(2000, "Duplicate reg: %x %x\n", known_regs[i].dst, known_regs[i].reg);
                return CBR_RET_ERROR;
            }
        }
    }

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

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_GUI_STATE)
MODULE_PROPHANDLERS_END()
