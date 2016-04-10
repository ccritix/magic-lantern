/* Playback-related functions 
 * (photo or movie, including QuickReview)
 */

#include "dryos.h" 
#include "play.h"

#ifdef IMGPLAY_ZOOM_LEVEL_ADDR

static struct play_zoom play_zoom;

static int zoom_factor(int zoom_level)
{
    /* this might be camera-specific; only tested on 5D3 */
    /* 1000 = 10x, 800 = 8x and so on */
    /* ratios guessed from the top-left position:
     * IMGPLAY_ZOOM_POS_X_CENTER / IMGPLAY_ZOOM_POS_X and similar for Y */
    const int zoom_ratios[] = {100, 155, 166, 186, 200, 231, 259, 300, 321, 370, 429, 522, 600, 700, 800, 1000 };
    int zoom_ratio = zoom_ratios[COERCE(zoom_level+1, 0, 15)];
    ASSERT(IMGPLAY_ZOOM_LEVEL_MAX == 14);
    return zoom_ratio;
}

/* scale from 0 ... center ... 2*center to -1000 ... 1000 */
static int scale_pos(int pos, int center)
{
    return COERCE((pos - center) * 1000 / center, -1000, 1000);
}

struct play_zoom * play_zoom_info()
{
    play_zoom.level = MEM(IMGPLAY_ZOOM_LEVEL_ADDR);
    play_zoom.max_level = IMGPLAY_ZOOM_LEVEL_MAX;
    play_zoom.factor = zoom_factor(play_zoom.level);
    play_zoom.pos_x = scale_pos(IMGPLAY_ZOOM_POS_X, IMGPLAY_ZOOM_POS_X_CENTER);
    play_zoom.pos_y = scale_pos(IMGPLAY_ZOOM_POS_Y, IMGPLAY_ZOOM_POS_Y_CENTER);
    return &play_zoom;
}

#endif
