/* Memory patching */

#include <dryos.h>
#include <menu.h>
#include <lvinfo.h>
#include <cache_hacks.h>
#include <patch.h>
#include <bmp.h>
#include <console.h>

#undef PATCH_DEBUG

#ifdef PATCH_DEBUG
#define dbg_printf(fmt,...) { console_printf(fmt, ## __VA_ARGS__); }
#else
#define dbg_printf(fmt,...) {}
#endif

#define MAX_PATCHES 32
#define MAX_LOGGING_HOOKS 32

/* for patching a single 32-bit integer (RAM or ROM, data or code) */
struct patch_info
{
    uint32_t* addr;                 /* first memory address to patch (RAM or ROM) */
    uint32_t backup;                /* old value (to undo the patch) */
    uint32_t patched_value;         /* value after patching */
    const char * description;       /* will be displayed in the menu as help text */
    unsigned is_instruction: 1;     /* if 1, we have patched an instruction (needs extra care with the instruction cache) */
};

/* ASM code from Maqs */
struct logging_hook_code
{
    uint32_t save_regs;         /* e92d5fff: STMFD  SP!, {R0-R12,LR} */
    uint32_t mov_regs;          /* e1a0000d: MOV    R0, SP */
    uint32_t mov_stack;         /* e28d1038: ADD    R1, SP, #56 */
    uint32_t mov_pc;            /* e59f200c: LDR    R2, [PC,#12] */
    uint32_t call_logger;       /*           BL     logging_function */
    uint32_t restore_regs;      /* e8bd5fff: LDMFD  SP!, {R0-R12,LR} */
    uint32_t original_instr;    /*           original ASM instruction (which was patched to jump here) */
    uint32_t b_return;          /*           B      patched_address + 4 */
    uint32_t addr;              /* patched address (for identification) */
    uint32_t fixup;             /* for relocating instructions that do PC-relative addressing */
};

static struct patch_info patches[MAX_PATCHES] = {{0}};
static int num_patches = 0;

/* at startup we don't have malloc, so we allocate it statically */
static struct logging_hook_code logging_hooks[MAX_LOGGING_HOOKS] = {{0}};

static char last_error[70];

static void check_cache_lock_still_needed();
static int patch_sync_cache(int also_data);

/* lock or unlock the cache as needed */
static void cache_require(int lock)
{
    if (lock)
    {
        if (!cache_locked())
        {
            printf("Locking cache\n");
            cache_lock();
        }
    }
    else
    {
        printf("Unlocking cache\n");
        cache_unlock();
    }
}

static int patch_sync_cache(int also_data)
{
    int err = 0;
    
    int locked = cache_locked();
    if (locked)
    {
        /* without this, reading from ROM right away may return the value patched in the I-Cache (5D2) */
        /* as a result, ROM patches may not be restored */
        cache_unlock();
    }

    if (also_data)
    {
        dbg_printf("Syncing caches...\n");
        sync_caches();
    }
    else
    {
        dbg_printf("Flushing ICache...\n");
        flush_i_cache();
    }
    
    if (locked)
    {
        cache_lock();
        err = reapply_cache_patches();
    }
    
    return err;
}

/* low-level routines */
static uint32_t read_value(uint32_t* addr, int is_instruction)
{
    uint32_t cached_value;
    
    if (is_instruction && IS_ROM_PTR(addr) && cache_locked()
        && cache_is_patchable((uint32_t)addr, TYPE_ICACHE, &cached_value) == 2)
    {
        /* return the value patched in the instruction cache */
        dbg_printf("Read from ICache: %x -> %x\n", addr, cached_value);
        return cached_value;
    }

    if (is_instruction)
    {
        /* when patching RAM instructions, the cached value might be incorrect - get it directly from RAM */
        addr = UNCACHEABLE(addr);
    }

    if (IS_ROM_PTR(addr))
    {
        dbg_printf("Read from ROM: %x -> %x\n", addr, MEM(addr));
    }

    return MEM(addr);
}

static int do_patch(uint32_t* addr, uint32_t value, int is_instruction)
{
    dbg_printf("Patching %x from %x to %x\n", addr, read_value(addr, is_instruction), value);
    
    if (IS_ROM_PTR(addr))
    {
        /* todo: check for conflicts (@g3gg0?) */
        cache_require(1);
        
        int cache_type = is_instruction ? TYPE_ICACHE : TYPE_DCACHE;
        if (cache_is_patchable((uint32_t)addr, cache_type, 0))
        {
            cache_fake((uint32_t)addr, value, cache_type);
            
            /* did it actually work? */
            if (read_value(addr, is_instruction) != value)
            {
                return E_PATCH_CACHE_ERROR;
            }
        }
        else
        {
            return E_PATCH_CACHE_COLLISION;
        }
    }

    if (is_instruction)
    {
        /* when patching RAM instructions, bypass the data cache and write directly to RAM */
        /* (will flush the instruction cache later) */
        addr = UNCACHEABLE(addr);
    }

    MEM(addr) = value;
    
    return 0;
}

static uint32_t get_patch_current_value(struct patch_info * p)
{
    return read_value(p->addr, p->is_instruction);
}

static int patch_memory_work(
    uintptr_t _addr,
    uint32_t old_value,
    uint32_t new_value,
    uint32_t is_instruction,
    const char* description
)
{
    uint32_t* addr = (uint32_t*)_addr;
    int err = E_PATCH_OK;
    
    /* ensure thread safety */
    uint32_t old_int = cli();

    dbg_printf("patch_memory_work(%x)\n", addr);

    /* is this address already patched? refuse to patch it twice */
    for (int i = 0; i < num_patches; i++)
    {
        if (patches[i].addr == addr)
        {
            err = E_PATCH_ALREADY_PATCHED;
            goto end;
        }
    }

    /* fill metadata */
    patches[num_patches].addr = addr;
    patches[num_patches].patched_value = new_value;
    patches[num_patches].is_instruction = is_instruction;
    patches[num_patches].description = description;

    /* are we patching the right thing? */
    uint32_t old = get_patch_current_value(&patches[num_patches]);

    /* safety check */
    if (old != old_value)
    {
        err = E_PATCH_OLD_VALUE_MISMATCH;
        goto end;
    }

    /* save backup value */
    patches[num_patches].backup = old;
    
    /* checks done, backups saved, now patch */
    err = do_patch(patches[num_patches].addr, new_value, is_instruction);
    if (err) goto end;
    
    /* RAM instructions are patched in RAM (to minimize collisions and only lock down the cache when needed),
     * but we need to clear the cache and re-apply any existing ROM patches */
    if (is_instruction && !IS_ROM_PTR(addr))
    {
        err = patch_sync_cache(0);
    }
    
    num_patches++;
    
end:
    if (err)
    {
        snprintf(last_error, sizeof(last_error), "Patch error at %x (err %x)", addr, err);
    }
    sei(old_int);
    return err;
}

static int is_patch_still_applied(int p)
{
    uint32_t current = get_patch_current_value(&patches[p]);
    uint32_t patched = patches[p].patched_value;
    return (current == patched);
}

static int reapply_cache_patch(int p)
{
    uint32_t current = get_patch_current_value(&patches[p]);
    uint32_t patched = patches[p].patched_value;
    
    if (current != patched)
    {
        void* addr = patches[p].addr;
        dbg_printf("Re-applying %x -> %x (changed to %x)\n", addr, patched, current);
        cache_fake((uint32_t) addr, patched, patches[p].is_instruction ? TYPE_ICACHE : TYPE_DCACHE);

        /* did it actually work? */
        if (read_value(addr, patches[p].is_instruction) != patched)
        {
            return E_PATCH_CACHE_ERROR;
        }
    }
    
    return 0;
}

int reapply_cache_patches()
{
    int err = 0;
    
    /* this function is also public */
    uint32_t old_int = cli();
    
    for (int i = 0; i < num_patches; i++)
    {
        if (IS_ROM_PTR(patches[i].addr))
        {
            err |= reapply_cache_patch(i);
        }
    }
    
    sei(old_int);
    
    return err;
}

static void check_cache_lock_still_needed()
{
    if (!cache_locked())
    {
        return;
    }
    
    /* do we still need the cache locked? */
    int rom_patches = 0;
    for (int i = 0; i < num_patches; i++)
    {
        if (IS_ROM_PTR(patches[i].addr))
        {
            rom_patches = 1;
            break;
        }
    }
    
    if (!rom_patches)
    {
        /* nope, we don't */
        cache_require(0);
    }
}

int unpatch_memory(uintptr_t _addr)
{
    uint32_t* addr = (uint32_t*) _addr;
    int err = E_UNPATCH_OK;
    uint32_t old_int = cli();

    dbg_printf("unpatch_memory(%x)\n", addr);

    /* find the patch in our data structure */
    int p = -1;
    for (int i = 0; i < num_patches; i++)
    {
        if (patches[i].addr == addr)
        {
            p = i;
            break;
        }
    }
    
    if (p < 0)
    {
        err = E_UNPATCH_NOT_PATCHED;
        goto end;
    }
    
    /* is the patch still applied? */
    if (!is_patch_still_applied(p))
    {
        err = E_UNPATCH_OVERWRITTEN;
        goto end;
    }
    
    /* not needed for ROM patches - there we will re-apply all the remaining ones from scratch */
    /* (slower, but old reverted patches should no longer give collisions) */
    if (!IS_ROM_PTR(addr))
    {
        err = do_patch(patches[p].addr, patches[p].backup, patches[p].is_instruction);
        if (err) goto end;
    }

    /* remove from our data structure (shift the other array items) */
    for (int i = p + 1; i < num_patches; i++)
    {
        patches[i-1] = patches[i];
    }
    num_patches--;

    /* also look in up in the logging hooks, and zero it out if found */
    for (int i = 0; i < MAX_LOGGING_HOOKS; i++)
    {
        if (logging_hooks[i].addr == _addr)
        {
            memset(&logging_hooks[i], 0, sizeof(struct logging_hook_code));
        }
    }

    if (IS_ROM_PTR(addr))
    {
        /* unlock and re-apply only the remaining patches */
        cache_unlock();
        cache_lock();
        err = reapply_cache_patches();
    }
    else if (patches[p].is_instruction)
    {
        err = patch_sync_cache(0);
    }

    check_cache_lock_still_needed();

end:
    if (err)
    {
        snprintf(last_error, sizeof(last_error), "Unpatch error at %x (err %x)", addr, err);
    }
    sei(old_int);
    return err;
}

int patch_memory(
    uintptr_t addr,
    uint32_t old_value,
    uint32_t new_value,
    const char* description
)
{
    return patch_memory_work(addr, old_value, new_value, 0, description);
}

int patch_instruction(
    uintptr_t addr,
    uint32_t old_value,
    uint32_t new_value,
    const char* description
)
{
    return patch_memory_work(addr, old_value, new_value, 1, description);
}

int patch_engio_list(uint32_t * engio_list, uint32_t patched_register, uint32_t patched_value, const char * description)
{
    while (*engio_list != 0xFFFFFFFF)
    {
        uint32_t reg = *engio_list;
        if (reg == patched_register)
        {
            /* evil hack that relies on unused LSB bits on the register address */
            /* this will cause Canon code to ignore this register and keep our patched value */
            *(engio_list) = patched_register + 1;
            return patch_memory((uintptr_t)(engio_list+1), engio_list[1], patched_value, description);
        }
        engio_list += 2;
    }
    return E_PATCH_REG_NOT_FOUND;
}

int unpatch_engio_list(uint32_t * engio_list, uint32_t patched_register)
{
    while (*engio_list != 0xFFFFFFFF)
    {
        uint32_t reg = *engio_list;
        if (reg == patched_register + 1)
        {
            *(engio_list) = patched_register;
            return unpatch_memory((uintptr_t)(engio_list+1));
        }
        engio_list += 2;
    }
    return E_UNPATCH_REG_NOT_FOUND;
}

#define REG_PC      15
#define LOAD_MASK   0x0C000000
#define LOAD_INSTR  0x04000000

static uint32_t reloc_instr(uint32_t pc, uint32_t new_pc)
{
    uint32_t instr = MEM(pc);
    uint32_t fixup = new_pc + 0xC;
    uint32_t load = instr & LOAD_MASK;

    // Check for load from %pc
    if( load == LOAD_INSTR )
    {
        uint32_t reg_base   = (instr >> 16) & 0xF;
        int32_t offset      = (instr >>  0) & 0xFFF;

        if( reg_base != REG_PC )
            return instr;

        // Check direction bit and flip the sign
        if( (instr & (1<<23)) == 0 )
            offset = -offset;

        // Compute the destination, including the change in pc
        uint32_t dest       = pc + offset + 8;

        // Find the data that is being used and
        // compute a new offset so that it can be
        // accessed from the relocated space.
        uint32_t data = MEM(dest);
        int32_t new_offset = fixup - new_pc - 8;

        uint32_t new_instr = 0
            | ( 1<<23 )                 /* our fixup is always forward */
            | ( instr & ~0xFFF )        /* copy old instruction, without offset */
            | ( new_offset & 0xFFF )    /* replace offset */
            ;

        // Copy the data to the offset location
        MEM(fixup) = data;
        return new_instr;
    }
    
    return instr;
}

int patch_hook_function(uintptr_t addr, uint32_t orig_instr, patch_hook_function_cbr logging_function, char* description)
{
    int err = 0;

    /* ensure thread safety */
    uint32_t old_int = cli();
    
    /* find a free slot in logging_hooks */
    int logging_slot = -1;
    for (int i = 0; i < COUNT(logging_hooks); i++)
    {
        if (logging_hooks[i].save_regs == 0)
        {
            logging_slot = i;
            break;
        }
    }
    
    if (logging_slot < 0)
    {
        snprintf(last_error, sizeof(last_error), "No logging slot for %x", addr);
        err = E_PATCH_TOO_MANY_PATCHES;
        goto end;
    }
    
    /* create the logging code */
    struct logging_hook_code * hook = &logging_hooks[logging_slot];
    hook->save_regs       = 0xe92d5fff;                                     /* e92d5fff: STMFD  SP!, {R0-R12,LR} */
    hook->mov_regs        = 0xe1a0000d;                                     /* e1a0000d: MOV    R0, SP */
    hook->mov_stack       = 0xe28d1038;                                     /* e28d1038: ADD    R1, SP, #56 */
    hook->mov_pc          = 0xe59f200c;                                     /* e59f200c: LDR    R2, [PC,#12] */
    hook->call_logger     = BL_INSTR(&hook->call_logger, logging_function); /*           BL     logging_function */
    hook->restore_regs    = 0xe8bd5fff;                                     /* e8bd5fff: LDMFD  SP!, {R0-R12,LR} */
    hook->original_instr  = reloc_instr(addr, (uint32_t)&hook->original_instr); /*       original ASM instruction, relocated */
    hook->b_return        = B_INSTR (&hook->b_return, addr + 4);            /*           B      patched_address + 4 */
    hook->addr            = addr;                                           /*           patched address (for identification) */

    /* since we have modified some code in RAM, sync the caches */
    patch_sync_cache(1);
    
    /* patch the original instruction to jump to the logging code */
    err = patch_instruction(addr, orig_instr, B_INSTR(addr, hook), description);
    
    if (err)
    {
        /* something went wrong? */
        memset(hook, 0, sizeof(struct logging_hook_code));
        goto end;
    }

end:
    sei(old_int);
    return err;
}

static MENU_UPDATE_FUNC(patch_update)
{
    int p = (int) entry->priv;
    if (p < 0 || p >= num_patches)
    {
        entry->shidden = 1;
        return;
    }

    /* long description */
    MENU_SET_HELP("%s.", patches[p].description);

    /* short description: assume the long description is formatted as "module_name: it does this and that" */
    /* => extract module_name and display it as short description */
    char short_desc[16];
    snprintf(short_desc, sizeof(short_desc), "%s", patches[p].description);
    char* sep = strchr(short_desc, ':');
    if (sep) *sep = 0;
    MENU_SET_RINFO("%s", short_desc);

    /* ROM patches are considered invasive, display them with red icon */
    MENU_SET_ICON(IS_ROM_PTR(patches[p].addr) ? MNI_RECORD : MNI_ON, 0);

    char name[20];
    snprintf(name, sizeof(name), "%s: %X", patches[p].is_instruction ? "Code" : "Data", patches[p].addr);
    MENU_SET_NAME("%s", name);

    int val = get_patch_current_value(&patches[p]);
    int backup = patches[p].backup;

    /* patch value: do we have enough space to print before and after? */
    if ((val & 0xFFFF0000) == 0 && (backup & 0xFFFF0000) == 0)
    {
        MENU_SET_VALUE("%X -> %X", backup, val);
    }
    else
    {
        MENU_SET_VALUE("%X", val);
    }

    /* some detailed info */
    void* addr = patches[p].addr;
    MENU_SET_WARNING(MENU_WARN_INFO, "0x%X: 0x%X -> 0x%X.", addr, backup, val);
    
    /* was this patch overwritten by somebody else? */
    if (!is_patch_still_applied(p))
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING,
            "Patch %x overwritten (expected %x, got %x).",
            patches[p].addr, patches[p].patched_value, get_patch_current_value(&patches[p])
        );
    }
}

/* forward reference */
static struct menu_entry patch_menu[];

static MENU_UPDATE_FUNC(patches_update)
{
    int ram_patches = 0;
    int rom_patches = 0;
    int errors = 0;

    for (int i = 0; i < MAX_PATCHES; i++)
    {
        if (i < num_patches)
        {
            if (IS_ROM_PTR(patches[i].addr))
            {
                rom_patches++;
            }
            else
            {
                ram_patches++;
            }
            patch_menu[0].children[i].shidden = 0;
            
            if (!is_patch_still_applied(i))
            {
                snprintf(last_error, sizeof(last_error), 
                    "Patch %x overwritten (expected %x, current value %x).",
                    patches[i].addr, patches[i].patched_value, get_patch_current_value(&patches[i])
                );
                errors++;
            }
        }
        else
        {
            patch_menu[0].children[i].shidden = 1;
        }
    }
    
    if (ram_patches == 0 && rom_patches == 0)
    {
        MENU_SET_RINFO("None");
        MENU_SET_ENABLED(0);
    }
    else
    {
        MENU_SET_ICON(MNI_SUBMENU, 1);
        if (errors) MENU_SET_RINFO("%d ERR", errors);
        if (rom_patches) MENU_APPEND_RINFO("%s%d ROM", info->rinfo[0] ? ", " : "", rom_patches);
        if (ram_patches) MENU_APPEND_RINFO("%s%d RAM", info->rinfo[0] ? ", " : "", ram_patches);
    }
    
    if (cache_locked())
    {
        MENU_SET_ICON(MNI_RECORD, 0); /* red dot */
        MENU_SET_WARNING(MENU_WARN_ADVICE, "Cache is locked down (not exactly clean).");
    }
    
    if (last_error[0])
    {
        MENU_SET_ICON(MNI_RECORD, 0); /* red dot */
        MENU_SET_WARNING(MENU_WARN_ADVICE, last_error);
    }
}

#define PATCH_ENTRY(i) \
        { \
            .priv = (void*)i, \
            .update = patch_update, \
            .shidden = 1, \
        }

static struct menu_entry patch_menu[] =
{
    {
        .name = "Simple patches",
        .update = patches_update,
        .select = menu_open_submenu,
        .icon_type = IT_SUBMENU,
        .help = "Show memory addresses patched in Canon code or data areas.",
        .submenu_width = 710,
        .children =  (struct menu_entry[]) {
            // for i in range(128): print "            PATCH_ENTRY(%d)," % i
            PATCH_ENTRY(0),
            PATCH_ENTRY(1),
            PATCH_ENTRY(2),
            PATCH_ENTRY(3),
            PATCH_ENTRY(4),
            PATCH_ENTRY(5),
            PATCH_ENTRY(6),
            PATCH_ENTRY(7),
            PATCH_ENTRY(8),
            PATCH_ENTRY(9),
            PATCH_ENTRY(10),
            PATCH_ENTRY(11),
            PATCH_ENTRY(12),
            PATCH_ENTRY(13),
            PATCH_ENTRY(14),
            PATCH_ENTRY(15),
            PATCH_ENTRY(16),
            PATCH_ENTRY(17),
            PATCH_ENTRY(18),
            PATCH_ENTRY(19),
            PATCH_ENTRY(20),
            PATCH_ENTRY(21),
            PATCH_ENTRY(22),
            PATCH_ENTRY(23),
            PATCH_ENTRY(24),
            PATCH_ENTRY(25),
            PATCH_ENTRY(26),
            PATCH_ENTRY(27),
            PATCH_ENTRY(28),
            PATCH_ENTRY(29),
            PATCH_ENTRY(30),
            PATCH_ENTRY(31),
            MENU_EOL,
        }
    }
};

static void patch_simple_init()
{
    menu_add("Debug", patch_menu, COUNT(patch_menu));
}

INIT_FUNC("patch", patch_simple_init);

