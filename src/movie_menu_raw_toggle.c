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

static CONFIG_INT( "movie.recording_mode", recording_mode, 0 );

bool should_hide_entry(struct menu_entry * entry){

    // if(entry->children && COUNT(entry->children) == 0){
    //     // Hide menu entries that define an empty submenu
    //     // This will be the case when e.g. Customized Buttons is not filled up by movtweaks, modules, etc.
    //     return true;
    // }

    char * name = (char *) entry->name;

    if (!name){
        return false;
    }
    
    if (recording_mode == 0)
    {
        // Raw recording mode
        return
            streq( name, "Bit Rate" ) ||
            streq( name, "Bit Rate (CBR)" ) ||
            streq( name, "Bit Rate (VBR)" ) ||
            streq( name, "Gradual Exposure" ) || // Does this work properly with RAW video?
            streq( name, "Vignetting" ) ||
            streq( name, "Time Indicator" ) ||
            streq( name, "Movie Logging" ) ||
            streq( name, "Movie Restart" ) ||
            streq( name, "REC/STBY notify" ) ||
            streq( name, "ML Digital ISO" ) ||
            streq( name, "Movie crop mode" );
    } else
    {
        // h264 recording mode
        return
            streq( name, "Ratio" ) ||
            streq( name, "Crop mode" ) ||
            streq( name, "RAW video" );
    }
}

static void hide_recursive(struct menu_entry * menu_children){
    for (struct menu_entry * entry = menu_children; entry; entry = entry->next)
    {
        if(should_hide_entry(entry)){
            entry->shidden = 1;
        } else if(!entry->placeholder){
            // Show this entry
            entry->shidden = 0;
            // Check children
            hide_recursive(entry->children);
        }
    }
}

static struct menu * menu_find_by_name_simple(char * name ){
    struct menu * menu = menu_get_root();

    for( ; menu ; menu = menu->next )
    {
        ASSERT(menu->name);
        if( streq( menu->name, name ) )
        {
            return menu;
        }

        // Stop just before we get to the end
        if( !menu->next )
            break;
    }

    return NULL;
}

static void hide_show_movie_menu_entries_for_recording_mode(){
    
    struct menu * menu = menu_find_by_name_simple("Movie");

    if( !menu )
        return;

    hide_recursive(menu->children);

    if(recording_mode == 0){
        // RAW recording
        menu_set_value_from_script("Movie", "RAW video", 1);
        menu_set_value_from_script("Movie", "Crop mode", 1);
    } else {
        menu_set_value_from_script("Movie", "RAW video", 0);
        menu_set_value_from_script("Movie", "Crop mode", 0);
    }
}

// TODO also make placeholders for h264 mode to give them a neat ordering as well
// TODO fix icons of Movie Tweaks etc, it's green because it doesn't ignore hidden entries
static struct menu_entry movie_menu_raw_toggle[] =
{
    {
        .name = "Recording mode",
        .priv = &recording_mode,
        .max = 1,
        .choices = CHOICES("RAW", "h264"),
        .shidden = 1,
    },
    {
        .name = "Ratio",
        .placeholder = 1,
        .shidden = 1,
    },
    {
        .name = "FPS override",
        .placeholder = 1,
        .shidden = 1,
    },
    {
        .name = "RAW video",
        .placeholder = 1,
        .shidden = 1,
    },
    {
        .name = "Crop mode",
        .placeholder = 1,
        .shidden = 1,
    },
    {
        .name = "Customized Buttons",
        .select = menu_open_submenu,
        .children =  (struct menu_entry[]) {
            MENU_EOL,
        },
    },
};

MENU_SELECT_FUNC(movie_menu_raw_toggle_select){

    recording_mode = 1 - recording_mode;
    hide_show_movie_menu_entries_for_recording_mode();
    
    // TODO: wait for user to exit ML menu, then refresh LV
    gui_stop_menu();
    PauseLiveView();
    ResumeLiveView();
    if(recording_mode == 0){
        NotifyBox(2000, "Enabled RAW");
    } else {
        NotifyBox(2000, "Enabled h264");
    }
}

static void movie_menu_raw_toggle_init()
{
    movie_menu_raw_toggle[0].select = movie_menu_raw_toggle_select;
    menu_add( "Movie", movie_menu_raw_toggle, COUNT(movie_menu_raw_toggle) );
}

// "Recording mode" menu entry is declared as hidden
// Will be unhidden when this method is called from mlv_lite.c
// Otherwise it will be shown if modules aren't loaded (and there'll be no RAW)
void movie_menu_raw_toggle_init_after_modules_loaded()
{
    for (struct menu_entry * entry = movie_menu_raw_toggle; entry; entry = entry->next)
    {
        char * name = (char *) entry->name;
        // Check if Customized Buttons has been filled, if not hide it
        if(name && streq( name, "Customized Buttons" ) && COUNT(entry->children) == 0){
            entry->shidden = 1;
            entry->placeholder = 1; // Also set preset state so it won't be unhidden again
        } else if(name && streq( name, "Recording mode" )){
            entry->shidden = 0;
        }
    }
    
    hide_show_movie_menu_entries_for_recording_mode();
}

INIT_FUNC(__FILE__, movie_menu_raw_toggle_init);
