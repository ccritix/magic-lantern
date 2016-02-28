#include <dryos.h>
#include <module.h>
#include <config.h>
#include <menu.h>
#include <beep.h>
#include <patch.h>

static CONFIG_INT("card_fs", card_fs, 0);
#define FORCE_FAT32 1
#define FORCE_EXFAT 2

/* addresses to be patched */
static uint32_t addr_fat_or_exfat = 0;
static uint32_t addr_fat_type_or_exfat = 0;
static uint32_t addr_partition_id = 0;
static uint32_t addr_exfat_table = 0;
static uint32_t addr_cf_check = 0;

/* values */

/* CF check (cards greater than a certain size are formatted using SD routines) */
#define CMP_R0_0x10000000 0xE3500201    /* treat CF cards > 128GB as SD cards (original) */
#define CMP_R0_0x00FC0000 0xE350073F    /* treat CF cards > 7.9GB as SD cards (to force exFAT) */
#define CMP_R0_0x40000000 0xE3500101    /* treat CF cards > 512GB as SD cards (to force FAT32) */

/* first patch (a small function that chooses between FAT and exFAT) */
#define CMP_R1_0x04000000 0xE3510301    /* exFAT on cards > 32GB (original)*/
#define CMP_R1_0x00FC0000 0xE351073F    /* exFAT on cards > 7.9GB */
#define CMP_R1_0x40000000 0xE3510101    /* exFAT on cards > 512GB (in other words, use FAT32) */

/* second and third patch (when selecting detailed filesystem parameters for SD cards) */
#define CMP_R6_0x04000000 0xE3560301    /* exFAT on cards > 32GB (original)*/
#define CMP_R6_0x40000000 0xE3560101    /* exFAT on cards > 512GB (in other words, use FAT32) */
#define CMP_R6_0x00FC0000 0xE356073F    /* exFAT on cards > 7.9GB */

/* Note: Canon code uses an extra check for cards < 0xFB0400 blocks (7.84 GB),
 * so formatting smaller cards as exFAT would require a more complex patch.
 * 
 * To cover 8GB cards, we will change the threshold for choosing exFAT
 * as blocks >= 0xFC0000.
 */

static void update_patch()
{
    static int patch_active = 0;

    /* undo active patches, if any */
    if (patch_active)
    {
        unpatch_memory(addr_cf_check);
        unpatch_memory(addr_fat_or_exfat);
        unpatch_memory(addr_partition_id);
    }
    if (patch_active == FORCE_FAT32)
    {
        unpatch_memory(addr_fat_type_or_exfat);
    }
    else if (patch_active == FORCE_EXFAT)
    {
        unpatch_memory(addr_exfat_table);
    }

    if (card_fs)
    {
        uint32_t patched_insn1 = 
            card_fs == FORCE_FAT32 ? CMP_R1_0x40000000 :
            card_fs == FORCE_EXFAT ? CMP_R1_0x00FC0000 : 0;

        uint32_t patched_insn2 = 
            card_fs == FORCE_FAT32 ? CMP_R6_0x40000000 :
            card_fs == FORCE_EXFAT ? CMP_R6_0x00FC0000 : 0;

        uint32_t patched_insn_cf = 
            card_fs == FORCE_FAT32 ? CMP_R0_0x40000000 :
            card_fs == FORCE_EXFAT ? CMP_R0_0x00FC0000 : 0;

        /* common patches */
        patch_instruction(addr_cf_check,                CMP_R0_0x10000000, patched_insn_cf, "CardFS: handle large CF cards as SD (to use exFAT)");
        patch_instruction(addr_fat_or_exfat,            CMP_R1_0x04000000, patched_insn1,   "CardFS: filesystem decision 1 (FAT or exFAT)");
        patch_instruction(addr_partition_id,            CMP_R6_0x04000000, patched_insn2,   "CardFS: filesystem decision 2a (FAT12/16/32/exFAT)");
        
        if (card_fs == FORCE_FAT32)
        {
            /* patches only needed for forcing FAT32 */
            patch_instruction(addr_fat_type_or_exfat,   CMP_R6_0x04000000, patched_insn2,   "CardFS: filesystem decision 2b (FAT12/16/32/exFAT)");
        }
        else if (card_fs == FORCE_EXFAT)
        {
            /* patches only needed for forcing exFAT */
            patch_memory     (addr_exfat_table,         0x04000000, 0x00FC0000,             "CardFS: exFAT parameters table (valid from 7.9GB rather than 32GB)");
        }
        patch_active = card_fs;
    }
}

static MENU_SELECT_FUNC(card_fs_toggle)
{
    card_fs = MOD(card_fs + delta, 3);
    update_patch();
}

static struct menu_entry card_fmt_menu[] =
{
    /* note: some cameras already have this menu, others don't */
    /* if it's there, this one will be merged with the existing one */
    /* if it's not, it will be created from scratch */
    {
        .name = "Card settings",
        .select = menu_open_submenu,
        .help = "Preferences related to SD/CF card operation.",
        .children =  (struct menu_entry[]) {
            {
                .name       = "Card FS on format",
                .priv       = &card_fs,
                .max        = 2,
                .choices    = CHOICES("OFF", "FAT32", "exFAT (8GB+)"),
                .select     = card_fs_toggle,
                .help       = "Force exFAT or FAT32 when formatting the card.",
                .help2      = "Use it only if you like playing Russian Roulette with your data.",
            },
            MENU_EOL,
        },
    },
};

static unsigned int card_fmt_init()
{
    if (is_camera("5D3",  "1.1.3"))
    {
        addr_cf_check           = 0xFF5BB870;
        addr_fat_or_exfat       = 0xFF5BBAE0;
        addr_fat_type_or_exfat  = 0xFF729C60;
        addr_partition_id       = 0xFF729D60;
        addr_exfat_table        = 0xFF9D2BD4;
    }

    /* check stubs */
    if ((MEM(addr_cf_check)          != CMP_R0_0x10000000) ||
        (MEM(addr_fat_or_exfat)      != CMP_R1_0x04000000) ||
        (MEM(addr_fat_type_or_exfat) != CMP_R6_0x04000000) ||
        (MEM(addr_partition_id)      != CMP_R6_0x04000000) ||
        (MEM(addr_exfat_table)       != 0x04000000) ||
        (MEM(addr_exfat_table+0x04)  != 0) ||
        (MEM(addr_exfat_table+0x08)  != 0) ||
        (MEM(addr_exfat_table+0x0C)  != 0x10000000) ||
        (MEM(addr_exfat_table+0x10)  != 0x100) ||
        (MEM(addr_exfat_table+0x14)  != 0x8000))
    {
        return CBR_RET_ERROR;
    }
    
    menu_add("Prefs", card_fmt_menu, COUNT(card_fmt_menu));
    
    if (card_fs)
    {
        /* apply the patch */
        update_patch();
    }
    return 0;
}

static unsigned int card_fmt_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(card_fmt_init)
    MODULE_DEINIT(card_fmt_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(card_fs)
MODULE_CONFIGS_END()
