#include "dryos.h"
#include "version.h"
#include "bmp.h"
#include "menu.h"
#include "menuhelp.h"

extern void menu_easy_advanced_display(void* priv, int x0, int y0, int selected);

static void menu_nav_help_open(void* priv, int delta)
{
    menu_help_go_to_label("Magic Lantern menu", 0);
}

static MENU_UPDATE_FUNC(user_guide_display)
{
    MENU_SET_VALUE("");
}

static MENU_UPDATE_FUNC(set_scrollwheel_display)
{
    if (info->can_custom_draw)
    {
        int x = bmp_string_width(MENU_FONT, entry->name) + 40;
        bfnt_draw_char(ICON_MAINDIAL, x, info->y - 5, COLOR_WHITE, NO_BG_ERASE);
    }
}

static struct menu_entry help_menus[] = {
    {
        .select = menu_nav_help_open,
        .name = "Press " INFO_BTN_NAME,
        .choices = CHOICES("Context help"),
    },
    {
        .select = menu_nav_help_open,
        #if defined(CONFIG_500D)
        .name = "Press LiveView " SYM_LV,
        .choices = CHOICES("Open submenu (Q)"),
        #elif defined(CONFIG_50D)
        .name = "Press FUNC",
        .choices = CHOICES("Open submenu (Q)"),
        #elif defined(CONFIG_5D2)
        .name = "Press Pict.Style",
        .choices = CHOICES("Open submenu (Q)"),
        #elif defined(CONFIG_5DC) || defined(CONFIG_40D)
        .name = "Press JUMP",
        .choices = CHOICES("Open submenu (Q)"),
        #elif defined(CONFIG_EOSM)
        .name = "1-finger Tap",
        .choices = CHOICES("Open submenu (Q)"),
        #elif defined(CONFIG_100D)
        .name = "Press Av",
        .choices = CHOICES("Open submenu (Q)"),
        #else
        .name = "Press Q",
        .choices = CHOICES("Open submenu"),
        #endif
        
        .children =  (struct menu_entry[]) {
            {
                .name = "... like this :)",
            },
            MENU_EOL,
        },
    },
    #if defined(CONFIG_5D2) || defined(CONFIG_50D)
    {
        .name = "Joystick long-press",
        .select = menu_nav_help_open,
        .choices = CHOICES("Open submenu"),
        
        .children =  (struct menu_entry[]) {
            {
                .name = "... like this :)",
            },
            MENU_EOL,
        },
    },
    #endif
    {
        .select  = menu_nav_help_open,
        .name    = "SET / ",
        .update  = set_scrollwheel_display,
        .choices = CHOICES("Edit values"),
    },
    {
        .select = menu_nav_help_open,
        #ifdef CONFIG_500D
        .name = "Zoom In",
        #else
        .name = SYM_LV" or Zoom In",
        #endif
        .choices = CHOICES("Edit in LiveView"),
    },
    {
        .select = menu_nav_help_open,
        .name = "Press MENU",
        .choices = CHOICES("Junkie mode"),
    },
    #if defined(FEATURE_OVERLAYS_IN_PLAYBACK_MODE) && defined(BTN_ZEBRAS_FOR_PLAYBACK_NAME) && defined(BTN_ZEBRAS_FOR_PLAYBACK)
    /* if BTN_ZEBRAS_FOR_PLAYBACK_NAME is undefined, you must define it (or undefine FEATURE_OVERLAYS_IN_PLAYBACK_MODE) */
    {
        .select = menu_nav_help_open,
        .name = "Press "BTN_ZEBRAS_FOR_PLAYBACK_NAME,
        .choices = CHOICES("Overlays (PLAY only)"),
    },
    #endif
    #ifdef ARROW_MODE_TOGGLE_KEY
    {
        .select = menu_nav_help_open,
        .name = "Press "ARROW_MODE_TOGGLE_KEY,
        .choices = CHOICES("Shortcuts (LV only)"),
    },
    #endif
    {
        .name = "Key Shortcuts",
        .select = menu_help_go_to_label,
    },
    {
        .name = "Complete user guide",
        .select = menu_open_submenu,
        .children =  (struct menu_entry[]) {
            #include "menuindexentries.h"
            MENU_EOL
        },
    },
    {
        .name = "About Magic Lantern",
        .select = menu_help_go_to_label,
    },
};

static void
help_menu_init( void* unused )
{
    menu_add("Help", help_menus, COUNT(help_menus));
}

INIT_FUNC( "help_menu", help_menu_init );


