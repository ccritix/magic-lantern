/** \file
 * Reorganise Movie menu for RAW/h264 recording modes
 */
#include "dryos.h"
#include "math.h"
#include "version.h"
#include "bmp.h"
#include "gui.h"
#include "config.h"
#include "property.h"
#include "lens.h"
#include "font.h"
#include "menu.h"
#include "beep.h"
#include "zebra.h"
#include "focus.h"
#include "menuhelp.h"
#include "console.h"
#include "debug.h"
#include "lvinfo.h"
#include "powersave.h"

// Explicitly define the order of the Movie menu
// Also decides where entries from modules will end up
static struct menu_entry movie_menu_raw_toggle[] =
{
    {
        .name = "Startoff Presets",
        .placeholder = 1,
    },
    {
        .name = "RAW video",
        .placeholder = 1,
    },
    {
        .name = "Ratio",
        .placeholder = 1,
    },
    {
        .name = "Bitdepth",
        .placeholder = 1,
    },
    {
        .name = "Set 25fps",
        .placeholder = 1,
    },
    {
        .name = "Max ISO",
        .placeholder = 1,
    },
    {
        .name = "FPS override",
        .placeholder = 1,
    },
    {
        .name = "Shutter Lock",
        .placeholder = 1,
    },
    {
        .name = "Shutter fine-tuning",
        .placeholder = 1,
    },
    {
        .name = "Customized Buttons",
        .placeholder = 1,
    },
    {
        .name = "Crop mode",
        .placeholder = 1,
    },
};

static void movie_menu_raw_only_init()
{
    menu_add( "Movie", movie_menu_raw_toggle, COUNT(movie_menu_raw_toggle) );
}

INIT_FUNC(__FILE__, movie_menu_raw_only_init);
