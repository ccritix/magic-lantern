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

#include "notify_box.h"

#include <string.h>


static struct msg_queue *notify_box_queue = NULL;

static void notify_box_task(void* priv)
{
    uint32_t delay = 250;
    uint32_t timeout = 500;
    
    TASK_LOOP
    {
        notify_job_t *job = NULL;

        /* fetch a new encryption job */
        if(msg_queue_receive(notify_box_queue, &job, timeout))
        {
            continue;
        }
        
        uint32_t font_height = 50;
        uint32_t msg_width = bmp_string_width(FONT_CANON, job->message);
        uint32_t msg_height = font_height;
        uint32_t width = 20 + msg_width;
        uint32_t height = 20 + msg_height;
        
        /* when there was an icon filename specified, open the bitmap */
        if(job->icon_file)
        {
            job->icon_data = bmp_load(job->icon_file, 0);
        }
        
        /* when the icon was successfully opened or passed, render it */
        if(job->icon_data)
        {
            width += job->icon_data->width + 10;
            height = MAX(job->icon_data->height + 20, msg_height + 20);
        }
        
        /* ToDo: detect number of lines and increase size */
        
        
        uint32_t pos_y = (480 - height) / 2;
        uint32_t pos_x = (720 - width) / 2;
        
        /* make sure we reach zero */
        job->timeout -= job->timeout % delay;
        
        while(job->timeout > 0)
        {
            job->timeout -= delay;
            
            bmp_fill(COLOR_BG, pos_x, pos_y, width, height);

            if(job->icon_data)
            {
                bmp_draw_scaled_ex(job->icon_data, pos_x + 10, pos_y + 10, 128, 128, 0);
                bmp_printf(FONT_CANON, pos_x + 128 + 20, pos_y + 10 + job->icon_data->height / 2 - msg_height / 2, job->message);
            }
            else
            {
                bmp_printf(FONT_CANON, pos_x + 10, pos_y + 10, job->message);
            }
            
            /* already another message arrived? if so, cancel loop */
            uint32_t msg_count = 0;
            msg_queue_count(notify_box_queue, &msg_count);
            
            if(msg_count)
            {
                break;
            }
            
            msleep(delay);
        }
        
        /* clean up the areas we printed on */
        bmp_fill(COLOR_EMPTY, pos_x, pos_y, width, height);
        redraw();
        
        /* free all elements in the job */
        if(job->icon_file)
        {
            free(job->icon_file);
            
            /* dont free icon_data if it was not allocated by us, so put it under filename check */
            if(job->icon_data)
            {
                free(job->icon_data);
            }
        }
        
        if(job->message)
        {
            free(job->message);
        }
        
        /* free job, it was allocated from the calling thread */
        free(job);
    }
}

TASK_CREATE("notifybox_task", notify_box_task, 0, 0x1b, 0x1000);

void NotifyBoxHide()
{
    /* displaying happens in other thread, so send a mesage with all information */
    notify_job_t *job = malloc(sizeof(notify_job_t));
    if(!job)
    {
        return;
    }
    
    /* fill the job for notify_box_task */
    job->timeout = 0;
    job->message = strdup("");
    job->icon_file = NULL;
    job->icon_data = NULL;
    
    msg_queue_post(notify_box_queue, job);
}

void NotifyBox(uint32_t timeout, char *fmt, ...) 
{
    char *tmp_msg = malloc(NOTIFY_BOX_TEXT_LENGTH);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp_msg, NOTIFY_BOX_TEXT_LENGTH - 1, fmt, ap);
    va_end(ap);
    
    /* displaying happens in other thread, so send a mesage with all information */
    notify_job_t *job = malloc(sizeof(notify_job_t));
    if(!job)
    {
        return;
    }
    
    /* fill the job for notify_box_task */
    job->timeout = timeout;
    job->message = strdup(tmp_msg);
    job->icon_file = NULL;
    job->icon_data = NULL;
    
    msg_queue_post(notify_box_queue, job);

    /* this buffer was only used for vsnprintf */
    free(tmp_msg);
}

void NotifyBoxIcon(uint32_t timeout, char *icon, char *fmt, ...) 
{
    char *tmp_msg = malloc(NOTIFY_BOX_TEXT_LENGTH);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp_msg, NOTIFY_BOX_TEXT_LENGTH - 1, fmt, ap);
    va_end(ap);
    
    /* displaying happens in other thread, so send a mesage with all information */
    notify_job_t *job = malloc(sizeof(notify_job_t));
    if(!job)
    {
        return;
    }
    
    /* fill the job for notify_box_task */
    job->timeout = timeout;
    job->message = strdup(tmp_msg);
    job->icon_file = strdup(icon);
    job->icon_data = NULL;
    
    msg_queue_post(notify_box_queue, job);

    /* this buffer was only used for vsnprintf */
    free(tmp_msg);
}

static void dlg_init()
{
    notify_box_queue = (struct msg_queue *) msg_queue_create("notify_box_queue", 100);
}

INIT_FUNC(__FILE__, dlg_init);
