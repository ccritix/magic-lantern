/*
ctrlexpo
Under M mode, arrow keys function will control speed (1/20, 1/50, 1/100...) and aperture (F8.0, F9.0, F11...)
*/

#include <module.h>
#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <config.h>
#include <menu.h>
#include <lens.h>

static CONFIG_INT("ctrlexpo.enabled", ctrlexpo_enabled, 0);
static int last_key = 0;

static struct menu_entry ctrlexpo_menu[] =
{
    {
        .name = "Control expo",
        .priv = &ctrlexpo_enabled,
        .max = 1,
        .help = "Left and right arrow controls shutter, up and down aperture.",
    }
};

static unsigned int ctrlexpo_init()
{
    menu_add("Expo", ctrlexpo_menu, COUNT(ctrlexpo_menu));
    return 0;
}

static unsigned int ctrlexpo_shoot_task() {
    if(ctrlexpo_enabled) switch(last_key)
    {
        case MODULE_KEY_PRESS_LEFT:
            shutter_toggle((void*)-1, -1);
            break;
        case MODULE_KEY_PRESS_RIGHT:
            shutter_toggle((void*)-1, 1);
            break;
        case MODULE_KEY_PRESS_DOWN:
            aperture_toggle((void*)-1, -1);
            break;
        case MODULE_KEY_PRESS_UP:
            aperture_toggle((void*)-1, 1);
            break;
    }
    last_key = 0;
}

static unsigned int ctrlexpo_keypress(unsigned int key)
{
    last_key = key;
    return 1;
}

MODULE_INFO_START()
    MODULE_INIT(ctrlexpo_init)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_SHOOT_TASK, ctrlexpo_shoot_task, 0)
    MODULE_CBR(CBR_KEYPRESS, ctrlexpo_keypress, 0)
MODULE_CBRS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(ctrlexpo_enabled)
MODULE_CONFIGS_END()
