/* A very simple module
 * (example for module authors)
 */
#include <dryos.h>
#include <module.h>
#include <menu.h>
#include <config.h>
#include <focus.h>
#include <property.h>

/* Config variables. They are used for persistent variables (usually settings).
 * 
 * In modules, these variables also have to be declared as MODULE_CONFIG.
 */
static CONFIG_INT("efm_mods.enabled", efm_mods_enabled, 1);

static unsigned int efm_mods_keypress_cbr(unsigned int key)
{
    if(efm_mods_enabled){
        if(key == MODULE_KEY_INFO){
            printf("INFO PRESSED\n");

            int new_af_mode = is_manual_focus() ? AF_MODE_ONE_SHOT : AF_MODE_MANUAL_FOCUS;
            
            // Toggle MF now
            // The problem is, the new value doesn't stick...
            // Stays at old value but it does trigger the PROP_HANDLER for PROP_AF_MODE
            // Seems like this isn't possible after all:
            // https://www.magiclantern.fm/forum/index.php?topic=21807.0
            prop_request_change(PROP_AF_MODE, &new_af_mode, 2);
            return 0;
        }
    }
    return 1;
}

static struct menu_entry efm_mods_menu[] =
{
    {
        .name       = "Enable one shot AF during MF",
        .priv       = &efm_mods_enabled,
        .max        = 1,
        .help       = "Enable one shot AF during MF",
    },
};

/* This function is called when the module loads. */
/* All the module init functions are called sequentially,
 * in alphabetical order. */
static unsigned int efm_mods_init()
{
    menu_add("Focus", efm_mods_menu, COUNT(efm_mods_menu));
    return 0;
}


PROP_HANDLER(PROP_AF_MODE)
{
    // Log to see if changes were applied
    printf("PROP_AF_MODE %d, len %d\n", buf[0], len);
}

/* Note: module unloading is not yet supported;
 * this function is provided for future use.
 */
static unsigned int efm_mods_deinit()
{
    return 0;
}

/* All modules have some metadata, specifying init/deinit functions,
 * config variables, event hooks, property handlers etc.
 */
MODULE_INFO_START()
    MODULE_INIT(efm_mods_init)
    MODULE_DEINIT(efm_mods_deinit)
MODULE_INFO_END()

MODULE_PROPHANDLERS_START()
    MODULE_PROPHANDLER(PROP_AF_MODE)
MODULE_PROPHANDLERS_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_KEYPRESS, efm_mods_keypress_cbr, 0)
MODULE_CBRS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(efm_mods_enabled)
MODULE_CONFIGS_END()