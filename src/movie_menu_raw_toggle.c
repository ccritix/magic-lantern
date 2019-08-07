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

bool should_hide_entry(char * name){
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
            streq( name, "Crop mode" ) ||
            streq( name, "RAW video" );
    }
}

static void hide_recursive(struct menu_entry * menu_children){
    for (struct menu_entry * entry = menu_children; entry; entry = entry->next)
    {
        if(should_hide_entry((char *) entry->name)){
            entry->shidden = 1;
        } else {
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

void hide_show_movie_menu_entries_for_recording_mode(){
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

static struct menu_entry movie_menu_raw_toggle[] = {
    {
        .name = "Recording mode",
        .priv = &recording_mode,
        .max = 1,
        .choices = CHOICES("RAW", "h264"),
        .select = movie_menu_raw_toggle_select,
    },
};

static void movie_menu_raw_toggle_init()
{
    menu_add( "Movie", movie_menu_raw_toggle, COUNT(movie_menu_raw_toggle) );
}

INIT_FUNC(__FILE__, movie_menu_raw_toggle_init);

// TODO init menu entry hidden first, then call from mlv_lite to unhide 
// Otherwise it will be there if modules aren't loaded (and there'll be no RAW)