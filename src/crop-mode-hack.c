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

#ifndef FEATURE_RAW_VIDEO_ONLY
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
#endif

void crop_mode_hack_init()
{
    #ifndef FEATURE_RAW_VIDEO_ONLY
    menu_add( "Movie", crop_hack_menus, COUNT(crop_hack_menus) );
    #endif
}

INIT_FUNC(__FILE__, crop_mode_hack_init);

#endif
