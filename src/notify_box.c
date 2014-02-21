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
        notify_job_t *job_msg = NULL;
        notify_job_t job;

        /* fetch a new encryption job */
        if(msg_queue_receive(notify_box_queue, &job, timeout))
        {
            continue;
        }
        
        /* free job, it was allocated from the calling thread. we are keeping it local */
        job = *job_msg;
        free(job_msg);
        
        uint32_t border_width = 10;
        uint32_t font_height = 50;
        uint32_t msg_width = bmp_string_width(FONT_CANON, job.message);
        uint32_t msg_height = font_height;
        uint32_t width = 2 * border_width + msg_width;
        uint32_t height = 2 * border_width + msg_height;
        
        /* when there was an icon filename specified, open the bitmap */
        if(job.icon_file)
        {
            job.icon_data = bmp_load(job.icon_file, 0);
        }
        
        /* when the icon was successfully opened or passed, render it */
        if(job.icon_data)
        {
            if(!job.icon_width || !job.icon_height)
            {
                job.icon_width = job.icon_data->width;
                job.icon_height = job.icon_data->height;
            }
            
            /* if there is an icon, the space between icon and text is also 'border_width' */
            width += job.icon_width + border_width;
            height = MAX(job.icon_height + 2 * border_width, msg_height + 2 * border_width);
        }
        
        /* default position is top left */
        if(job.pos_x < 0 || job.pos_y < 0)
        {
            job.pos_x = 10;
            job.pos_y = 10;
        }
        
        /* ToDo: detect number of lines and increase size */
        
        /* make sure we reach zero */
        job.timeout -= job.timeout % delay;
        
        while(job.timeout > 0)
        {
            job.timeout -= delay;
            
            bmp_fill(COLOR_BG, job.pos_x, job.pos_y, width, height);
            bmp_draw_rect(COLOR_WHITE, job.pos_x, job.pos_y, width, height);

            if(job.icon_data)
            {
                bmp_draw_scaled_ex(job.icon_data, job.pos_x + border_width, job.pos_y + border_width, job.icon_width, job.icon_height, 0);
                bmp_printf(FONT_CANON, job.pos_x + job.icon_width + 2 * border_width, job.pos_y + border_width + job.icon_height / 2 - msg_height / 2, job.message);
            }
            else
            {
                bmp_printf(FONT_CANON, job.pos_x + border_width, job.pos_y + border_width, job.message);
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
        bmp_fill(COLOR_EMPTY, job.pos_x, job.pos_y, width, height);
        redraw();
        
        /* free all elements in the job */
        if(job.icon_file)
        {
            free(job.icon_file);
            
            /* dont free icon_data if it was not allocated by us, so put it under filename check */
            if(job.icon_data)
            {
                free(job.icon_data);
            }
        }
        
        if(job.message)
        {
            free(job.message);
        }
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

static void notify_box_process(notify_job_t job_data, char *fmt, va_list ap)
{
    char *tmp_msg = malloc(NOTIFY_BOX_TEXT_LENGTH);
    
    if(!tmp_msg)
    {
        return;
    }
    
    notify_job_t *job = malloc(sizeof(notify_job_t));
    if(!job)
    {
        free(tmp_msg);
        return;
    }
    
    *job = job_data;
    vsnprintf(tmp_msg, NOTIFY_BOX_TEXT_LENGTH - 1, fmt, ap);
    job->message = strdup(tmp_msg);
    
    free(tmp_msg);
    
    msg_queue_post(notify_box_queue, job);
}

void NotifyBox(uint32_t timeout, char *fmt, ...) 
{
    notify_job_t job;
    
    /* fill the job for notify_box_task */
    job.timeout = timeout;
    job.icon_file = NULL;
    job.icon_data = NULL;
    job.icon_width = 0;
    job.icon_height = 0;
    job.pos_x = -1;
    job.pos_y = -1;
    
    va_list ap;
    va_start(ap, fmt);
    notify_box_process(job, fmt, ap);
    va_end(ap);
}

void NotifyBoxIcon(uint32_t timeout, char *icon, char *fmt, ...) 
{
    notify_job_t job;
    
    /* fill the job for notify_box_task */
    job.timeout = timeout;
    job.icon_file = strdup(icon);
    job.icon_data = NULL;
    job.icon_width = 0;
    job.icon_height = 0;
    job.pos_x = -1;
    job.pos_y = -1;
    
    va_list ap;
    va_start(ap, fmt);
    notify_box_process(job, fmt, ap);
    va_end(ap);
}

void NotifyBoxIconFileAdv(uint32_t timeout, char *icon, int32_t icon_width, uint32_t icon_height, uint32_t pos_x, uint32_t pos_y, char *fmt, ...) 
{
    notify_job_t job;
    
    /* fill the job for notify_box_task */
    job.timeout = timeout;
    job.icon_file = strdup(icon);
    job.icon_data = NULL;
    job.icon_width = 0;
    job.icon_height = 0;
    job.pos_x = -1;
    job.pos_y = -1;
    
    va_list ap;
    va_start(ap, fmt);
    notify_box_process(job, fmt, ap);
    va_end(ap);
}

void NotifyBoxIconDataAdv(uint32_t timeout, struct bmp_file_t *icon, int32_t icon_width, uint32_t icon_height, uint32_t pos_x, uint32_t pos_y, char *fmt, ...) 
{
    notify_job_t job;
    
    /* fill the job for notify_box_task */
    job.timeout = timeout;
    job.icon_file = NULL;
    job.icon_data = icon;
    job.icon_width = icon_width;
    job.icon_height = icon_height;
    job.pos_x = pos_x;
    job.pos_y = pos_y;
    
    va_list ap;
    va_start(ap, fmt);
    notify_box_process(job, fmt, ap);
    va_end(ap);
}

static void dlg_init()
{
    notify_box_queue = (struct msg_queue *) msg_queue_create("notify_box_queue", 100);
}

INIT_FUNC(__FILE__, dlg_init);
