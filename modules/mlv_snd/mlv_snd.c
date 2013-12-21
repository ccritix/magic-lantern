/**
 * MLV Sound addon module
 */

/*
 * Copyright (C) 2013 Magic Lantern Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <module.h>
#include <dryos.h>
#include <menu.h>
#include <config.h>
#include <bmp.h>
#include "raw.h"
#include "../trace/trace.h"
#include "../mlv_rec/mlv.h"

#include "kiss_fft.h"

#define MLV_SND_BUFFERS 4

#define QUAD(x) ((x)*(x))
#define FIX_TO_FLOAT(x) ((float)(x) / 32768.0f)

static uint32_t trace_ctx = TRACE_ERROR;

static CONFIG_INT("mlv.snd.enabled", mlv_snd_enabled, 0);
static CONFIG_INT("mlv.snd.mlv_snd_enable_tracing", mlv_snd_enable_tracing, 0);

extern WEAK_FUNC(ret_0) int StartASIFDMAADC(void *, uint32_t, void *, uint32_t, void (*)(), uint32_t);
extern WEAK_FUNC(ret_0) int SetNextASIFADCBuffer(void *, uint32_t);
extern WEAK_FUNC(ret_0) int PowerAudioOutput();
extern WEAK_FUNC(ret_0) int audio_configure(uint32_t);
extern WEAK_FUNC(ret_0) int SetAudioVolumeOut(uint32_t);
extern uint64_t get_us_clock_value();


extern void mlv_rec_get_slot_info(int32_t slot, uint32_t *size, void **address);
extern int32_t mlv_rec_get_free_slot();
extern void mlv_rec_release_slot(int32_t slot, uint32_t write);

static volatile int32_t mlv_snd_running = 0;
static volatile int32_t mlv_writer_running = 0;
static volatile int32_t mlv_snd_recording = 0;
static volatile uint32_t mlv_snd_in_started = 0;
static volatile uint32_t mlv_snd_in_active = 0;
static volatile uint32_t mlv_snd_in_calls = 0;
static struct msg_queue * volatile mlv_snd_buffers_empty = NULL;
static struct msg_queue * volatile mlv_snd_buffers_done = NULL;
static volatile uint32_t mlv_snd_in_buffers = 64;
static volatile uint32_t mlv_snd_frame_number = 0;
static volatile uint32_t mlv_snd_in_buffer_size = 0;
static volatile uint32_t mlv_snd_in_sample_rate = 44100;

typedef struct
{
    uint16_t *data;
    uint32_t length;
    uint32_t frameNumber;
    uint64_t timestamp;
    
    /* these are filled when the ASIF buffer comes from a MLV frame slot */
    void *mlv_slot_buffer;
    int32_t mlv_slot_id;
    int32_t mlv_slot_end;
} audio_data_t;

audio_data_t *mlv_snd_current_buffer = NULL;
audio_data_t *mlv_snd_next_buffer = NULL;


static void mlv_snd_asif_in_cbr()
{
    //trace_write(trace_ctx, "mlv_snd_asif_in_cbr: entered");
    
    /* the next buffer is now being filled, so update timestamp */
    mlv_snd_next_buffer->timestamp = get_us_clock_value();
    
    /* and pass the filled buffer into done queue */
    if(mlv_snd_current_buffer)
    {
        mlv_snd_current_buffer->frameNumber = mlv_snd_frame_number;
        mlv_snd_frame_number++;
        msg_queue_post(mlv_snd_buffers_done, mlv_snd_current_buffer);
        mlv_snd_current_buffer = NULL;
    }
    
    if(mlv_snd_running)
    {
        mlv_snd_in_active = 1;
        mlv_snd_in_calls++;

        uint32_t count = 0;
        if(msg_queue_count(mlv_snd_buffers_empty, &count))
        {
            trace_write(trace_ctx, "mlv_snd_asif_in_cbr: msg_queue_count failed");
            return;
        }
        if(count < 1)
        {
            trace_write(trace_ctx, "mlv_snd_asif_in_cbr: no free buffers available");
            return;
        }
        
        /* the "next" buffer is the current one being filled */
        mlv_snd_current_buffer = mlv_snd_next_buffer;
        
        /* get the new "next" and queue */
        if(msg_queue_receive(mlv_snd_buffers_empty, &mlv_snd_next_buffer, 10))
        {
            trace_write(trace_ctx, "mlv_snd_asif_in_cbr: msg_queue_receive(mlv_snd_buffers_empty, ) failed");
            return;
        }
        trace_write(trace_ctx, "mlv_snd_asif_in_cbr: queueing buffer in slot %d", mlv_snd_next_buffer->mlv_slot_id);
        SetNextASIFADCBuffer(mlv_snd_next_buffer->data, mlv_snd_next_buffer->length);
    }
    else
    {
        mlv_snd_in_active = 0;
        trace_write(trace_ctx, "mlv_snd_asif_in_cbr: stopping");
    }
    //trace_write(trace_ctx, "mlv_snd_asif_in_cbr: returned");
}

static void mlv_snd_flush_entries(struct msg_queue *queue, uint32_t clear)
{
    uint32_t msgs = 0;
    
    msg_queue_count(queue, &msgs);
    
    trace_write(trace_ctx, "mlv_snd_flush_entries: %d entries to free in queue", msgs);
    while(msgs > 0)
    {
        audio_data_t *entry = NULL;
        if(msg_queue_receive(queue, &entry, 10))
        {
            trace_write(trace_ctx, "mlv_snd_flush_entries: msg_queue_receive(queue, ) failed");
            return;
        }
        
        if(entry->mlv_slot_buffer)
        {
            trace_write(trace_ctx, "mlv_snd_flush_entries: entry is MLV slot");
            mlv_audf_hdr_t *hdr = (mlv_audf_hdr_t *)entry->mlv_slot_buffer;
            
            if(clear)
            {
                trace_write(trace_ctx, "mlv_snd_flush_entries: NULL slot %d entry", entry->mlv_slot_id);
                mlv_set_type((mlv_hdr_t *)hdr, "NULL");
            }
            else
            {
                hdr->timestamp = entry->timestamp;
                hdr->frameNumber = entry->frameNumber;
            }
            
            if(entry->mlv_slot_end)
            {
                trace_write(trace_ctx, "mlv_snd_flush_entries: entry is MLV slot %d (last buffer, so release)", entry->mlv_slot_id);
                mlv_rec_release_slot(entry->mlv_slot_id, 1);
            }
        }
        else
        {
            trace_write(trace_ctx, "mlv_snd_flush_entries: entry is allocated mem");
            free_dma_memory(entry->data);
        }
        free(entry);
        
        msg_queue_count(queue, &msgs);
    }
    trace_write(trace_ctx, "mlv_snd_flush_entries: done");
}

static void mlv_snd_stop()
{
    if(!mlv_snd_running)
    {
        return;
    }

    trace_write(trace_ctx, "mlv_snd_stop: stopping worker and audio");
    
    /* wait until audio stopped */
    uint32_t loops = 100;
    while((mlv_snd_in_active || mlv_writer_running) && (--loops > 0))
    {
        mlv_snd_running = 0;
        msleep(20);
    }

    if(mlv_snd_in_active)
    {
        bmp_printf(FONT(FONT_MED, COLOR_RED, COLOR_BLACK), 10, 130, "audio failed to stop");
        trace_write(trace_ctx, "mlv_snd_stop: failed to stop audio");
    }
    if(mlv_writer_running)
    {
        bmp_printf(FONT(FONT_MED, COLOR_RED, COLOR_BLACK), 10, 130, "audio failed to stop");
        trace_write(trace_ctx, "mlv_snd_stop: failed to stop writer");
    }

    mlv_writer_running = 0;
    mlv_snd_in_active = 0;
    mlv_snd_in_started = 0;
    
    /* now flush the buffers */
    trace_write(trace_ctx, "mlv_snd_stop: flush mlv_snd_buffers_done");
    mlv_snd_flush_entries(mlv_snd_buffers_done, 0);
    trace_write(trace_ctx, "mlv_snd_stop: flush mlv_snd_buffers_empty");
    mlv_snd_flush_entries(mlv_snd_buffers_empty, 1);
}

static void mlv_snd_queue_slot()
{
    void *address = NULL;
    uint32_t queued = 0;
    uint32_t size = 0;
    uint32_t used = 0;
    uint32_t hdr_size = 0x100;
    uint32_t block_size = hdr_size + mlv_snd_in_buffer_size;
    
    int32_t slot = mlv_rec_get_free_slot();
    trace_write(trace_ctx, "mlv_snd_queue_slot: free slot %d", slot);
    
    /* get buffer memory address and available size */
    mlv_rec_get_slot_info(slot, &size, &address);
    
    if(!address)
    {
        trace_write(trace_ctx, "mlv_snd_queue_slot: failed to get address");
        return;
    }
    
    /* make sure that there is still place for a NULL block */
    while((used + block_size + sizeof(mlv_hdr_t) < size) && (queued < 128))
    {
        /* setup AUDF header for that block */
        mlv_audf_hdr_t *hdr = (mlv_audf_hdr_t *)((uint32_t)address + used);
        
        trace_write(trace_ctx, "mlv_snd_queue_slot: used:%d / %d, block_size:%d, address: 0x%08X", used, size, block_size, hdr);
        used += block_size;
        
        mlv_set_type((mlv_hdr_t *)hdr, "AUDF");
        hdr->blockSize = block_size;
        hdr->frameNumber = 0;
        hdr->frameSpace = hdr_size - sizeof(mlv_audf_hdr_t);
        hdr->timestamp = 0;
        
        /* store information about the buffer in the according queue entry */
        audio_data_t *entry = malloc(sizeof(audio_data_t));
        
        /* data is right after the header */
        entry->data = (void*)((uint32_t)hdr + hdr_size);
        entry->length = mlv_snd_in_buffer_size;
        entry->timestamp = 0;
        
        /* refer to the slot we are adding */
        entry->mlv_slot_buffer = hdr;
        entry->mlv_slot_id = slot;
        entry->mlv_slot_end = 0;
        
        /* check if this was the last frame and set end flag if so */
        if((used + block_size + sizeof(mlv_hdr_t) >= size) || (queued >= 128))
        {
            /* this tells the writer task that the buffer is filled with that entry being done and can be committed */
            entry->mlv_slot_end = 1;
        }
        
        msg_queue_post(mlv_snd_buffers_empty, entry);
        queued++;
    }
    
    /* now add a trailing NULL block */
    mlv_hdr_t *hdr = (mlv_hdr_t *)((uint32_t)address + used);
    
    mlv_set_type((mlv_hdr_t *)hdr, "NULL");
    hdr->blockSize = size - used;
    hdr->timestamp = 0xFFFFFFFFFFFFFFFF;
}


static void mlv_snd_prepare_audio()
{
    if(mlv_snd_enable_tracing && (trace_ctx == TRACE_ERROR))
    {
        char filename[100];
        snprintf(filename, sizeof(filename), "%smlv_snd.txt", MODULE_CARD_DRIVE);
        trace_ctx = trace_start("mlv_snd", filename);
        trace_format(trace_ctx, TRACE_FMT_TIME_REL | TRACE_FMT_COMMENT, ' ');
    }

    /* make sure we are not active */
    mlv_snd_stop();
    
    mlv_snd_frame_number = 0;
    mlv_snd_in_active = 0;
    mlv_snd_in_started = 0;
    mlv_snd_in_calls = 0;
    
    /* set up audio output */
    SetSamplingRate(mlv_snd_in_sample_rate, 0);
    MEM(0xC092011C) = 6;
    
    /* 5D3 */
    //void (*SetAudioChannels) (int rate, signed int bits, int channels, int output) = 0xFF10EFF4;
    //SetAudioChannels(mlv_snd_in_sample_rate, 16, 2, 0);
}

static void mlv_snd_alloc_buffers()
{
    /* when called from mlv_rec record start cbr, get slot from mlv_rec. else allocate on our own */
    if(!mlv_snd_recording)
    {
        /* calculate buffer size */
        mlv_snd_in_buffer_size = (44100 * 2 * 2) / 25;
        
        /* prepare empty buffers */
        for(uint32_t buf = 0; buf < mlv_snd_in_buffers; buf++)
        {
            audio_data_t *entry = malloc(sizeof(audio_data_t));
            
            entry->data = (uint16_t *)alloc_dma_memory(mlv_snd_in_buffer_size);
            entry->length = mlv_snd_in_buffer_size;
            entry->timestamp = 0;
            
            /* there is no mlv_rec slot buffer behind, so just set this to invalid */
            entry->mlv_slot_buffer = 0;
            entry->mlv_slot_id = -1;
            entry->mlv_slot_end = 0;
            
            msg_queue_post(mlv_snd_buffers_empty, entry);
        }
    }
    else
    {
        mlv_snd_in_buffer_size = 0x8000;
        trace_write(trace_ctx, "mlv_snd_alloc_buffers: running in MLV mode, mlv_snd_in_buffer_size = %d", mlv_snd_in_buffer_size);
        
        mlv_snd_queue_slot();
        mlv_snd_queue_slot();
    }
    
    /* now everything is ready to fire - real output activation happens as soon mlv_snd_running is set to 1 and mlv_snd_vsync() gets called */
}

static void mlv_snd_writer(int unused)
{
    int fft_size = 64;
    //char filename[100];
    //snprintf(filename, sizeof(filename), "%s/snd.raw", get_dcim_dir());
    //FILE* file = FIO_CreateFileEx(filename);
    
    kiss_fft_cfg cfg = kiss_fft_alloc(fft_size, 0, 0 ,0);
    kiss_fft_cpx *fft_in = malloc(fft_size * sizeof(kiss_fft_cpx));
    kiss_fft_cpx *fft_out = malloc(fft_size * sizeof(kiss_fft_cpx));
    
    mlv_writer_running = 1;

    TASK_LOOP
    {
        audio_data_t *buffer = NULL;
        
        if(!mlv_snd_running)
        {
            trace_write(trace_ctx, "   --> WRITER: exiting");
            break;
        }
        
        /* receive write job from dispatcher */
        if(msg_queue_receive(mlv_snd_buffers_done, &buffer, 100))
        {
            static uint32_t timeouts = 0;
            trace_write(trace_ctx, "   --> WRITER: message timed out %d times now", ++timeouts);
            continue;
        }
        
        if(!buffer)
        {
            static uint32_t timeouts = 0;
            trace_write(trace_ctx, "   --> WRITER: message NULL %d times now", ++timeouts);
            continue;
        }
        
        if(buffer->mlv_slot_buffer)
        {
            trace_write(trace_ctx, "   --> WRITER: entry is MLV slot %d", buffer->mlv_slot_id);
            
            mlv_audf_hdr_t *hdr = (mlv_audf_hdr_t *)buffer->mlv_slot_buffer;
            
            hdr->timestamp = buffer->timestamp;
            hdr->frameNumber = buffer->frameNumber;
            
            if(buffer->mlv_slot_end)
            {
                trace_write(trace_ctx, "   --> WRITER: entry is MLV slot %d (last buffer, so release)", buffer->mlv_slot_id);
                mlv_rec_release_slot(buffer->mlv_slot_id, 1);
                mlv_snd_queue_slot();
            }
            free(buffer);
        }
        else
        {
            trace_write(trace_ctx, "   --> WRITER: entry is dma buffer");
            
            if(!gui_menu_shown() && liveview_display_idle())
            {
                /* pepare data for KISS FFT */
                for(int pos = 0; pos < fft_size; pos++)
                {
                    fft_in[pos].r = buffer->data[2 * pos];
                    fft_in[pos].i = 0;
                }
                
                kiss_fft(cfg, fft_in, fft_out);
                
                /* print FFT plot */
                int x_start = 20;
                int y_start = 420;
                int height = 200;
                int width = (720 - (2 * x_start));
                float bar_width = (float)width / ((float)fft_size / 2);
                
                int last_x = 0;
                int last_y = 0;
                
                for(int pos = 0; pos < fft_size / 2; pos++)
                {
                    float val_r = FIX_TO_FLOAT(fft_out[pos].r) + FIX_TO_FLOAT(fft_out[fft_size - 1 - pos].r);
                    float val_i = FIX_TO_FLOAT(fft_out[pos].i) + FIX_TO_FLOAT(fft_out[fft_size - 1 - pos].i);
                    
                    uint32_t ampl = (uint32_t)MIN(height, sqrtf(QUAD(val_r) + QUAD(val_i)) * height * 8);
                    
                    int x = x_start + pos * bar_width;
                    int y = (y_start - height) + (height - ampl);
                    
                    if(1)
                    {
                        bmp_fill(COLOR_EMPTY, x, y_start - height, bar_width + 1, (height - ampl));
                        bmp_fill(COLOR_RED, x, y, bar_width - 1, ampl);
                    }
                    if(0)
                    {
                        if(last_x && last_y)
                        {
                            bmp_fill(COLOR_EMPTY, x, y_start - height, bar_width + 1, height);
                            draw_line(last_x, last_y, x, y, COLOR_WHITE);
                        }
                        last_x = x;
                        last_y = y;
                    }
                }
                
                for(uint32_t freq = 0; freq < mlv_snd_in_sample_rate / 2; freq += 5000)
                {
                    int x = x_start + (freq * width) / (mlv_snd_in_sample_rate / 2);
                    draw_line(x, y_start - 30, x, y_start, COLOR_WHITE);
                }
                
                bmp_draw_rect(COLOR_WHITE, x_start - 2, y_start - height - 2, (720 - (2 * x_start)) + 4, height + 4);
            }
            
            msg_queue_post(mlv_snd_buffers_empty, buffer);
        }
        if(0)
        {
            //FIO_WriteFile(file, buffer->data, buffer->length);
        }
    }
    
    //FIO_CloseFile(file);
    free(cfg);
    free(fft_in);
    free(fft_out);
    
    mlv_writer_running = 0;
}

static void mlv_snd_start()
{
    trace_write(trace_ctx, "mlv_snd_start: starting");
    mlv_snd_prepare_audio();
    mlv_snd_running = 1;
    task_create("mlv_snd", 0x16, 0x1000, mlv_snd_writer, NULL);
}


/* public functions for raw_rec */
uint32_t raw_rec_cbr_starting()
{
    if(mlv_snd_enabled)
    {
        trace_write(trace_ctx, "raw_rec_cbr_starting: starting mlv_snd");
        mlv_snd_recording = 1;
        mlv_snd_start();
    }
    return 0;
}

uint32_t raw_rec_cbr_started()
{
    if(mlv_snd_recording)
    {
        trace_write(trace_ctx, "raw_rec_cbr_started: allocating buffers");
        mlv_snd_alloc_buffers();
    }
    return 0;
}

uint32_t raw_rec_cbr_stopping()
{
    if(mlv_snd_recording)
    {
        trace_write(trace_ctx, "raw_rec_cbr_stopping: stopping");
        mlv_snd_stop();
        mlv_snd_recording = 0;
    }
    return 0;
}

uint32_t raw_rec_cbr_mlv_block(mlv_hdr_t *hdr)
{
    return 0;
}

static void mlv_snd_trace_buf(char *caption, uint8_t *buffer, uint32_t length)
{
    char *str = malloc(length * 2 + 1);

    for(uint32_t pos = 0; pos < length; pos++)
    {
        snprintf(&str[pos * 2], 3, "%02X", buffer[pos]);
    }

    trace_write(trace_ctx, "%s: %s", caption, str);

    free(str);
}


static unsigned int mlv_snd_vsync(unsigned int unused)
{
    if(!mlv_snd_enabled || !mlv_snd_running)
    {
        return 0;
    }
    
    //trace_write(trace_ctx, "mlv_snd_vsync: frame %d", mlv_snd_frame_number);
    
    /* in running mode, start audio output here */
    if(!mlv_snd_in_started)
    {
        uint32_t msgs = 0;
        msg_queue_count(mlv_snd_buffers_empty, &msgs);
        
        if(msgs)
        {
            trace_write(trace_ctx, "mlv_snd_vsync: starting audio");
            
            /* get two buffers and queue them to ASIF */
            mlv_snd_current_buffer = NULL;
            mlv_snd_next_buffer = NULL;
            
            msg_queue_receive(mlv_snd_buffers_empty, &mlv_snd_current_buffer, 10);
            msg_queue_receive(mlv_snd_buffers_empty, &mlv_snd_next_buffer, 10);
            
            if(mlv_snd_current_buffer && mlv_snd_next_buffer)
            {
                mlv_snd_in_started = 1;
                StartASIFDMAADC(mlv_snd_current_buffer->data, mlv_snd_current_buffer->length, mlv_snd_next_buffer->data, mlv_snd_next_buffer->length, mlv_snd_asif_in_cbr, 0);
                
                /* the current one will get filled right now */
                mlv_snd_current_buffer->timestamp = get_us_clock_value();
                trace_write(trace_ctx, "mlv_snd_vsync: starting audio DONE");
            }
            else
            {
                trace_write(trace_ctx, "mlv_snd_vsync: msg_queue_receive(mlv_snd_buffers_empty, ...) failed, retry next time");
            }
        }
    }
    
    mlv_snd_frame_number++;
    
    return 0;
}


static MENU_SELECT_FUNC(mlv_snd_test_select)
{
    if(mlv_snd_running)
    {
        mlv_snd_stop();
    }
    else
    {
        mlv_snd_start();
        mlv_snd_alloc_buffers();
    }
}

static struct menu_entry mlv_snd_menu[] =
{
    {
        .name = "MLV Sound",
        .priv = &mlv_snd_enabled,
        .max = 1,
        .help = "Enable sound recording for MLV.",
        .submenu_width = 710,
        .children = (struct menu_entry[])
        {
            {
                .name = "Start/Stop FFT",
                .select = &mlv_snd_test_select,
                .help = "Start and stop FFT display.",
            },
            {
                .name = "Trace output",
                .priv = &mlv_snd_enable_tracing,
                .min = 0,
                .max = 1,
                .help = "Enable log file tracing. Needs camera restart.",
            },
            MENU_EOL,
        },
    },
};

static unsigned int mlv_snd_init()
{
    /* causes ERR70 ?! */
    //if(mlv_snd_enable_tracing)
    //{
    //    char filename[100];
    //    snprintf(filename, sizeof(filename), "%smlv_snd.txt", MODULE_CARD_DRIVE);
    //    trace_ctx = trace_start("mlv_snd", filename);
    //    trace_format(trace_ctx, TRACE_FMT_TIME_REL | TRACE_FMT_COMMENT, ' ');
    //}
    
    trace_write(trace_ctx, "mlv_snd_init: init queues");
    mlv_snd_buffers_empty = (struct msg_queue *) msg_queue_create("mlv_snd_buffers_empty", 257);
    mlv_snd_buffers_done = (struct msg_queue *) msg_queue_create("mlv_snd_buffers_done", 257);
    
    /* enable tracing */
    menu_add("Audio", mlv_snd_menu, COUNT(mlv_snd_menu));
    trace_write(trace_ctx, "mlv_snd_init: done");
    
    return 0;
}

static unsigned int mlv_snd_deinit()
{
    if(trace_ctx != TRACE_ERROR)
    {
        trace_stop(trace_ctx, 0);
        trace_ctx = TRACE_ERROR;
    }
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(mlv_snd_init)
    MODULE_DEINIT(mlv_snd_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
    MODULE_CBR(CBR_VSYNC, mlv_snd_vsync, 0)
MODULE_CBRS_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(mlv_snd_enabled)
    MODULE_CONFIG(mlv_snd_enable_tracing)
MODULE_CONFIGS_END()
