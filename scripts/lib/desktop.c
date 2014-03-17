/**
 * Magic Lantern scripting API
 * Dummy calls for desktop TCC, useful for running your scripts offline.
 */

#include "magic.h"
#include "stdarg.h"

#define NULL_WRAP(name) int name() { printf("*** " #name "\n"); return 0; }

void msleep(int x)
{
    printf("*** msleep(%d)\n", x);
}

void sleep(float x)
{
    printf("*** sleep(%g)\n", x);
    usleep((int)(x * 1000000));
}

void set_gui_mode(int x)
{
    printf("*** set_gui_mode(%d)\n", x);
}

int get_gui_mode()
{
    printf("*** get_gui_mode()\n");
    return 0;
}

NULL_WRAP(draw_circle)
NULL_WRAP(fill_circle)
NULL_WRAP(draw_line_polar)
NULL_WRAP(get_gui_mode)
NULL_WRAP(menu_get)
NULL_WRAP(menu_set)
NULL_WRAP(console_hide)

struct time * get_time()
{
    printf("*** get_time()\n");
    static struct time t;
    /* can't get time.h working, so... using hacked types for now */
    int now[2];
    time(now);
    t.second = now[0] % 60;
    t.minute = (now[0]/60) % 60;
    t.hour = (now[0]/3600) % 24;
    return &t;
}

int
bmp_printf(
           int fontspec,
           int x,
           int y,
           const char *fmt,
           ...
           )
{
    va_list            ap;

    char bmp_printf_buf[128];

    va_start( ap, fmt );
    vsnprintf( bmp_printf_buf, sizeof(bmp_printf_buf)-1, fmt, ap );
    va_end( ap );

    puts( bmp_printf_buf );
}

int lv;
