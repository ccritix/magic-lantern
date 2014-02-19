/** \file
 * Dialog test code
 * Based on http://code.google.com/p/400plus/source/browse/trunk/menu_developer.c
 */
#include "dryos.h"
#include "bmp.h"
#include "tasks.h"
#include "debug.h"
#include "menu.h"
#include "property.h"
#include "config.h"
#include "gui.h"
#include "lens.h"

static struct semaphore * notify_box_show_sem = 0;
static struct semaphore * notify_box_main_sem = 0;

static int notify_box_timeout = 0;
static int notify_box_stop_request = 0;
static int notify_box_dirty = 0;
static char notify_box_icon[100];
static char notify_box_msg[100];
static char notify_box_msg_tmp[100];

/*int handle_notifybox_bgmt(struct event * event)
{
    if (event->param == MLEV_NOTIFY_BOX_OPEN)
    {
        //~ BMP_LOCK ( bfnt_puts(notify_box_msg, 50, 50, COLOR_WHITE, COLOR_BLACK); )
        BMP_LOCK ( bmp_printf(FONT_LARGE, 50, 50, notify_box_msg); )
    }
    else if (event->param == MLEV_NOTIFY_BOX_CLOSE)
    {
        redraw();
        give_semaphore(notify_box_sem);
    }
    return 0;
}*/

static void NotifyBox_task(void* priv)
{
    uint32_t delay = 250;
    
    TASK_LOOP
    {
        // wait until some other task asks for a notification
        if(!notify_box_dirty)
        {
            int err = take_semaphore(notify_box_show_sem, 500);
            if (err)
            {
                continue;
            }
        }
        notify_box_dirty = 0;
        
        if (!notify_box_timeout) 
        {
            continue;
        }
        
        struct bmp_file_t *icon = NULL;
        uint32_t font_height = 50;
        uint32_t msg_width = bmp_string_width(FONT_CANON, notify_box_msg);
        uint32_t msg_height = font_height;
        uint32_t width = 20 + msg_width;
        uint32_t height = 20 + msg_height;
        
        /* ToDo: detect number of lines and increase size */
        
        /* try to load an icon */
        if(strlen(notify_box_icon))
        {
            icon = bmp_load(notify_box_icon, 0);
            if(icon)
            {
                width += icon->width + 10;
                height = MAX(icon->height + 20, msg_height + 20);
            }
        }
        
        uint32_t pos_y = (480 - height) / 2;
        uint32_t pos_x = (720 - width) / 2;
        
        // show notification for a while, then redraw to erase it
        notify_box_stop_request = 0;
        
        /* make sure we reach zero */
        notify_box_timeout -= notify_box_timeout % delay;
        
        for ( ; notify_box_timeout > 0; notify_box_timeout -= delay)
        {
            bmp_fill(COLOR_BG, pos_x, pos_y, width, height);

            if(icon)
            {
                bmp_draw_scaled_ex(icon, pos_x + 10, pos_y + 10, 128, 128, 0);
                bmp_printf(FONT_CANON, pos_x + 128 + 20, pos_y + 10 + icon->height / 2 - msg_height / 2, notify_box_msg);
            }
            else
            {
                bmp_printf(FONT_CANON, pos_x + 10, pos_y + 10, notify_box_msg);
            }
            
            msleep(delay);
            
            if(notify_box_stop_request || notify_box_dirty)
            {
                break;
            }
        }
        
        if(icon)
        {
            free(icon);
            icon = NULL;
        }
        
        bmp_fill(COLOR_EMPTY, pos_x, pos_y, width, height);
        redraw();
    }
}

TASK_CREATE( "notifybox_task", NotifyBox_task, 0, 0x1b, 0x1000 );

void NotifyBoxHide()
{
    notify_box_stop_request = 1;
}

void NotifyBox(int timeout, char* fmt, ...) 
{
    // make sure this is thread safe
    take_semaphore(notify_box_main_sem, 0);
    
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( notify_box_msg_tmp, sizeof(notify_box_msg_tmp)-1, fmt, ap );
    va_end( ap );
    
    if (notify_box_timeout && streq(notify_box_msg_tmp, notify_box_msg)) 
        goto end; // same message: do not redraw, just increase the timeout

    // new message
    memcpy(notify_box_msg, notify_box_msg_tmp, sizeof(notify_box_msg));
    notify_box_msg[sizeof(notify_box_msg)-1] = '\0';
    notify_box_timeout = MAX(timeout, 100);
    strncpy(notify_box_icon, "", sizeof(notify_box_icon));
    if (notify_box_timeout) notify_box_dirty = 1; // ask for a redraw, message changed

    give_semaphore(notify_box_show_sem); // request displaying the notification box

end:
    give_semaphore(notify_box_main_sem); // done, other call can be made now
}

void NotifyBoxIcon(int timeout, char *icon, char* fmt, ...) 
{
    // make sure this is thread safe
    take_semaphore(notify_box_main_sem, 0);
    
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( notify_box_msg_tmp, sizeof(notify_box_msg_tmp)-1, fmt, ap );
    va_end( ap );
    
    if (notify_box_timeout && streq(notify_box_msg_tmp, notify_box_msg)) 
        goto end; // same message: do not redraw, just increase the timeout

    // new message
    memcpy(notify_box_msg, notify_box_msg_tmp, sizeof(notify_box_msg));
    notify_box_msg[sizeof(notify_box_msg)-1] = '\0';
    notify_box_timeout = MAX(timeout, 100);
    
    strncpy(notify_box_icon, icon, sizeof(notify_box_icon));
    
    if (notify_box_timeout) notify_box_dirty = 1; // ask for a redraw, message changed

    give_semaphore(notify_box_show_sem); // request displaying the notification box

end:
    give_semaphore(notify_box_main_sem); // done, other call can be made now
}

static void dlg_init()
{
    notify_box_show_sem = create_named_semaphore("nbox_show_sem", 0);
    notify_box_main_sem = create_named_semaphore("nbox_done_sem", 1);
}

INIT_FUNC(__FILE__, dlg_init);
