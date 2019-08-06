#include "dryos.h"

#include "menu.h"
#include "property.h"

#include "crop-mode-hack.h"
#include "config.h"

#ifdef FEATURE_CROP_MODE_HACK

static int movie_crop_mode;
static int video_mode[10];

PROP_HANDLER(PROP_VIDEO_MODE)
{
    ASSERT(len <= sizeof(video_mode));
    memcpy(video_mode, buf, len);
}

unsigned int is_crop_hack_supported() 
{
    if(RECORDING || video_mode_resolution != 0)
    {
         return 0;
    }
    return 1;
}

unsigned int movie_crop_hack_enable() 
{
    if(!is_crop_hack_supported() || video_mode_crop) 
    {
        return 0;
    }
    video_mode[0] = 0xc;
    video_mode[4] = 2;
    prop_request_change(PROP_VIDEO_MODE, video_mode, 0);
    return 1;
}

unsigned int movie_crop_hack_disable() {
    if(!is_crop_hack_supported() || !video_mode_crop) 
    {
        return 0;
    }
    video_mode[0] = 0;
    video_mode[4] = 0;
    prop_request_change(PROP_VIDEO_MODE, video_mode, 0);
    return 1;
}


static void movie_crop_hack_toggle(void* priv, int sign)
{
    if(is_crop_hack_supported()) 
    {
        if(!video_mode_crop) 
        {
            movie_crop_hack_enable();
            movie_crop_mode = 1;
        } 
        else 
        {
            movie_crop_hack_disable();
            movie_crop_mode = 0;
        }
    }
}

static MENU_UPDATE_FUNC(movie_crop_hack_display)
{
    if(RECORDING)
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "You can't change crop mode while recording");
    }
    else if(video_mode_resolution != 0) 
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Crop video mode works in 1080p only");
    }
}

static struct menu_entry crop_hack_menus[] = {
    {
        .name = "Movie crop mode",
        .update = movie_crop_hack_display,
        .select = movie_crop_hack_toggle,
        .max = 1,
        .priv = &video_mode_crop,
        .help   = "Enables 600D movie crop-mode",
        .depends_on = DEP_MOVIE_MODE,
    },
};

void crop_mode_hack_init()
{
    menu_add( "Movie", crop_hack_menus, COUNT(crop_hack_menus) );
    
    // Load the config, because this will compute the path to the ML config file (even if no config exists)
    config_load();
    // Get path to ML config file
    char config_file[0x80];
    snprintf(config_file, sizeof(config_file), "%smagic.cfg", get_config_dir());

    // If config file doesn't exist, this is the first run: enable modules
    // Allows for modules to be disabled after first run
    int first_run = !config_flag_file_setting_load(config_file);

    if(first_run)
    {
        // Some fun stuff if we want modules working wihtout enabling from start 
        // in module.c set MENU_SET_SHIDDEN(0); to 1 if we want to hide modules altogether 
        FILE * file = FIO_CreateFile( "ML/SETTINGS/mlv_lite.en" );
        FILE * file2 = FIO_CreateFile( "ML/SETTINGS/crop_rec.en" );
        FILE * file3 = FIO_CreateFile( "ML/SETTINGS/lua.en" );
        FILE * file4 = FIO_CreateFile( "ML/SETTINGS/mlv_play.en" );
        FILE * file5 = FIO_CreateFile( "ML/SETTINGS/mlv_snd.en" );
        FILE * file6 = FIO_CreateFile( "ML/SETTINGS/sd_uhs.en" );
        // FILE * file7 = FIO_CreateFile( "ML/SETTINGS/adtg_gui.en" );
        FILE * file8 = FIO_CreateFile( "ML/SETTINGS/dual_iso.en" );
    }
}

INIT_FUNC(__FILE__, crop_mode_hack_init);

#endif
