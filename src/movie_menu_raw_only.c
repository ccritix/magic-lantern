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
    // {
    //     .name = "startoff presets",
    //     .placeholder = 1,
    // },
    {
        .name = "presets",
        .placeholder = 1,
    },
    {
        .name = "custom modes",
        .placeholder = 1,
    },
    {
        .name = "raw video",
        .placeholder = 1,
    },
    {
        .name = "ratio",
        .placeholder = 1,
    },
    {
        .name = "bitdepth",
        .placeholder = 1,
    },
    {
        .name = "set 25fps",
        .placeholder = 1,
    },
    {
        .name = "white balance",
        .placeholder = 1,
    },
    {
        .name = "fps override",
        .placeholder = 1,
    },
    {
        .name = "shutter lock",
        .placeholder = 1,
    },
    {
        .name = "shutter fine-tuning",
        .placeholder = 1,
    },
    {
        .name = "shutter range",
        .placeholder = 1,
    },
    {
        .name = "sound recording",
        .placeholder = 1,
    },
    {
        .name = "customized buttons",
        .placeholder = 1,
    },
};

static void movie_menu_raw_only_init()
{
    menu_add( "Movie", movie_menu_raw_toggle, COUNT(movie_menu_raw_toggle) );
}

INIT_FUNC(__FILE__, movie_menu_raw_only_init);
