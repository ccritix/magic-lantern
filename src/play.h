/* Playback-related functions */

#ifndef play_qr_h
#define play_qr_h

struct play_zoom
{
    int level;          /* -1 = no zoom, otherwise 0 ... imgplay_zoom_level_max() */
    int max_level;      /* usually 14; to be tested */
    int factor;         /* 100 = full image (no zoom); higher values = zoom (e.g. 1000 = 10x) */
    int pos_x;          /* -1000 ... 1000 */
    int pos_y;          /* -1000 ... 1000 */
};

/* update and return playback zoom information */
struct play_zoom * play_zoom_info();

#endif
