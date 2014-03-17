/**
 * Magic Lantern scripting API 
 * Implementation of some basic wrappers
 * 
 * Current state: proof of concept.
 */

#include "flt-hack.c"
#include "magic.h"

void sleep(float seconds)
{
    msleep(seconds * 1000.0);
}

struct tm {
        int     tm_sec;         /* seconds after the minute [0-60] */
        int     tm_min;         /* minutes after the hour [0-59] */
        int     tm_hour;        /* hours since midnight [0-23] */
        int     tm_mday;        /* day of the month [1-31] */
        int     tm_mon;         /* months since January [0-11] */
        int     tm_year;        /* years since 1900 */
        int     tm_wday;        /* days since Sunday [0-6] */
        int     tm_yday;        /* days since January 1 [0-365] */
        int     tm_isdst;       /* Daylight Savings Time flag */
        long    tm_gmtoff;      /* offset from CUT in seconds */
        char    *tm_zone;       /* timezone abbreviation */
};

struct time * get_time()
{
    struct tm now;

    void LoadCalendarFromRTC( struct tm * tm);
    LoadCalendarFromRTC(&now);
    
    static struct time t;
    t.hour = now.tm_hour;
    t.minute = now.tm_min;
    t.second = now.tm_sec;
    t.year = now.tm_year + 1900;
    t.month = now.tm_mon + 1;
    t.day = now.tm_mday;
    return &t;
}

int menu_get(char* tab, char* entry)
{
    int menu_get_value_from_script(const char * name, const char * entry_name);
    return menu_get_value_from_script(tab, entry);
}

int menu_set(char* tab, char* entry, int value)
{
    int menu_set_value_from_script(const char * name, const char * entry_name, int value);
    return menu_set_value_from_script(tab, entry, value);
}

void double_buffering_start()
{
    void bmp_draw_to_idle(int value);
    bmp_draw_to_idle(1);
}

void double_buffering_end()
{
    void bmp_draw_to_idle(int value);
    void console_show_status();
    void bmp_idle_copy(int direction, int fullsize);
    console_show_status();
    bmp_draw_to_idle(0);
    bmp_idle_copy(1,0);
}

void draw_line_polar(int x, int y, int radius, float angle, int color)
{
    int angle_x10 = (int)(angle * 10.0f);
    void draw_angled_line(int x, int y, int r, int ang, int cl);
    draw_angled_line(x, y, radius, angle_x10, color);
}
