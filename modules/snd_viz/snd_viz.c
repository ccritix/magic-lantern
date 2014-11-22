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
#include <sound.h>
#include <zebra.h>
#include <math.h>
#include <edmac-memcpy.h>
#include "../trace/trace.h"

#include "kiss_fft.h"

#define MLV_SND_BUFFERS 4

#define QUAD(x) ((x)*(x))
#define FIX_TO_FLOAT(x) ((float)(x) / 32768.0f)
#define FLOAT_TO_FIX(x) ((int16_t)(x * 32768.0f))

/* use BMP_W_PLUS and BMP_H_PLUS instead? */
#define BMP_WIDTH  720
#define BMP_HEIGHT 480


enum windowing_function
{
    WINDOW_None = 0,
    WINDOW_RaisedCosine = 1,
    WINDOW_Blackman = 2,
    WINDOW_BlackmanHarris = 3,
    WINDOW_BlackmanNuttal = 4,
    WINDOW_FlatTop = 5,
    WINDOW_Hamming = 6
};

enum snd_viz_mode
{
    VIZ_MODE_FFT_LINE,
    VIZ_MODE_FFT_BARS,
    VIZ_MODE_WATERFALL,
    VIZ_MODE_CORRELATION,
};

static uint32_t trace_ctx = TRACE_ERROR;

static CONFIG_INT("snd.viz.mode", snd_viz_mode, VIZ_MODE_WATERFALL);
static CONFIG_INT("snd.viz.fft_size", snd_viz_fft_size, 1024);
static CONFIG_INT("snd.viz.fft_window", snd_viz_fft_window, WINDOW_BlackmanHarris);
static CONFIG_INT("snd.viz.enable_tracing", snd_viz_enable_tracing, 0);


static int32_t snd_viz_running = 0;
static uint32_t snd_viz_in_active = 0;

static uint32_t snd_viz_fps = 20;
static uint32_t snd_viz_in_sample_rate = 48000;
static uint32_t snd_viz_correl_size = 400;

static struct sound_ctx *snd_viz_sound_ctx = NULL;
struct sound_buffer *snd_viz_buffers[10];
struct msg_queue *snd_viz_buffers_empty = NULL;
struct msg_queue *snd_viz_buffers_done = NULL;

static uint8_t *snd_viz_waterfall = NULL;
static uint32_t snd_viz_waterfall_pos = 0;
static uint32_t snd_viz_waterfall_height = 400;
static uint32_t snd_viz_waterfall_width = 512;



static void flush_queue(struct msg_queue *queue)
{
    uint32_t messages = 0;

    msg_queue_count(queue, &messages);
    while(messages > 0)
    {
        uint32_t tmp_buf = 0;
        msg_queue_receive(queue, &tmp_buf, 0);
        msg_queue_count(queue, &messages);
    }
}

static enum sound_flow snd_viz_buf_processed (struct sound_buffer *buffer)
{
    msg_queue_post(snd_viz_buffers_done, (uint32_t)buffer);
    
    if(!msg_queue_receive(snd_viz_buffers_empty, &buffer, 20))
    {
        snd_viz_sound_ctx->ops.enqueue(snd_viz_sound_ctx, buffer);
    }
    
    return SOUND_FLOW_CONTINUE;
}

static void snd_viz_stop_audio()
{
    snd_viz_sound_ctx->ops.stop(snd_viz_sound_ctx);
    snd_viz_sound_ctx->ops.unlock(snd_viz_sound_ctx);
}

static uint32_t snd_viz_start_audio()
{
    if(snd_viz_sound_ctx->ops.lock(snd_viz_sound_ctx, SOUND_LOCK_EXCLUSIVE) != SOUND_RESULT_OK)
    {
        trace_write(trace_ctx, "  lock failed");
        return 0;
    }
    
    /* enqueue buffers to sound system */
    for(uint32_t buf = 0; buf < COUNT(snd_viz_buffers); buf++)
    {
        struct sound_buffer *buffer = NULL;
        if(msg_queue_receive(snd_viz_buffers_empty, &buffer, 20))
        {
            static uint32_t timeouts = 0;
            trace_write(trace_ctx, "   --> WRITER: message timed out %d times now", ++timeouts);
            continue;
        }
        snd_viz_sound_ctx->ops.enqueue(snd_viz_sound_ctx, buffer);
    }
    
    if(snd_viz_sound_ctx->ops.start(snd_viz_sound_ctx) != SOUND_RESULT_OK)
    {
        trace_write(trace_ctx, "  start failed");
        return 0;
    }
    
    return 1;
}

static uint32_t snd_viz_alloc_buffers()
{
    /* 16 bit/sample, 2 channels */
    uint32_t buffer_size = (snd_viz_in_sample_rate * 2 * 2) / snd_viz_fps;

    /* prepare empty buffers */
    for(uint32_t pos = 0; pos < COUNT(snd_viz_buffers); pos++)
    {
        struct sound_buffer *buffer = sound_alloc_buffer();
        
        if(!buffer)
        {
            return 0;
        }
        buffer->processed = &snd_viz_buf_processed;
        buffer->size = buffer_size;
        buffer->data = malloc(buffer->size);
        if(!buffer->data)
        {
            return 0;
        }
        
        snd_viz_buffers[pos] = buffer;
        msg_queue_post(snd_viz_buffers_empty, (uint32_t)buffer);
    }
    
    return 1;
}

static void snd_viz_free_buffers()
{
    /* make sure all queues are empty */
    flush_queue(snd_viz_buffers_done);
    flush_queue(snd_viz_buffers_empty);

    /* prepare empty buffers */
    for(uint32_t pos = 0; pos < COUNT(snd_viz_buffers); pos++)
    {
        if(snd_viz_buffers[pos]->data)
        {
            free(snd_viz_buffers[pos]->data);
        }
        if(snd_viz_buffers[pos])
        {
            sound_free_buffer(snd_viz_buffers[pos]);
        }
        snd_viz_buffers[pos] = NULL;
    }
}


static void snd_viz_show_fft(kiss_fft_cpx *fft_data, uint32_t fft_size, float windowing_constant, uint32_t x_start, uint32_t y_start, uint32_t width, uint32_t height)
{
    /* print FFT plot */
    float bar_width = COERCE((float)width / ((float)fft_size / 2), 1, 100);
    static float log10_val = 0.0f;
    uint32_t last_x = 0;
    uint32_t last_y = 0;
    
    /* direct comparison possible because set to constant */
    if(log10_val == 0.0f)
    {
        log10_val = logf(10.0f);
    }
    
    bmp_draw_to_idle(1);
    
    /* clear canvas */
    switch(snd_viz_mode)
    {
        case VIZ_MODE_FFT_BARS:
        case VIZ_MODE_FFT_LINE:
        {
            bmp_fill(COLOR_BLACK, x_start, y_start, width, height);
            break;
        }
        
        case VIZ_MODE_WATERFALL:
        {
            bmp_fill(COLOR_BLACK, x_start, y_start, snd_viz_waterfall_width, height);
            /* clear current waterfall line */
            memset(&snd_viz_waterfall[snd_viz_waterfall_pos * snd_viz_waterfall_width], COLOR_BLACK, snd_viz_waterfall_width);
            break;
        }
    }
    
    /* process FFT result */
    for(uint32_t pos = 0; pos < fft_size / 2; pos++)
    {
        float val_r = FIX_TO_FLOAT(fft_data[pos].r) + FIX_TO_FLOAT(fft_data[fft_size - 1 - pos].r);
        float val_i = FIX_TO_FLOAT(fft_data[pos].i) + FIX_TO_FLOAT(fft_data[fft_size - 1 - pos].i);
        
        switch(snd_viz_mode)
        {
            case VIZ_MODE_FFT_BARS:
            {
                uint32_t ampl = (uint32_t)MIN(height, sqrtf(QUAD(val_r) + QUAD(val_i)) / windowing_constant * height * 8);
                int x = x_start + pos * bar_width;
                int y = y_start + (height - ampl);
                
                bmp_fill(COLOR_RED, x, y, bar_width, ampl);
                break;
            }
            
            case VIZ_MODE_FFT_LINE:
            {
                uint32_t ampl = (uint32_t)MIN(height, sqrtf(QUAD(val_r) + QUAD(val_i)) / windowing_constant * height * 8);
                int x = x_start + pos * bar_width;
                int y = y_start + (height - ampl);
                
                if(last_x && last_y)
                {
                    draw_line(last_x, last_y, x, y, COLOR_WHITE);
                }
                last_x = x;
                last_y = y;
                break;
            }
            
            case VIZ_MODE_WATERFALL:
            {
                float squared = QUAD(val_r) + QUAD(val_i) / QUAD(windowing_constant);
                float db_val = 10 * logf(squared) / log10_val;
                uint32_t ampl = (uint32_t)MIN(100, db_val + 100);
                uint32_t x = pos * snd_viz_waterfall_width / (fft_size / 2);
                
                snd_viz_waterfall[snd_viz_waterfall_pos * snd_viz_waterfall_width + x] = COLOR_GRAY(COERCE(ampl, 0, 100));
                break;
            }
        }
    }
    
    /* finish drawing */
    switch(snd_viz_mode)
    {
        case VIZ_MODE_FFT_BARS:
        case VIZ_MODE_FFT_LINE:
        {
            for(uint32_t freq = 0; freq < snd_viz_in_sample_rate / 2; freq += 5000)
            {
                int x = x_start + (freq * width) / (snd_viz_in_sample_rate / 2);
                draw_line(x, y_start + 30, x, y_start, COLOR_WHITE);
                draw_line(x, y_start + height - 30, x, y_start, COLOR_WHITE);
            }
            bmp_draw_rect(COLOR_WHITE, x_start, y_start, width, height);
            break;
        }
        
        case VIZ_MODE_WATERFALL:
        {
            uint8_t *bvram = bmp_vram_idle();
            
            for(uint32_t line = 0; line < height; line++)
            {
                uint32_t waterfall_line = (snd_viz_waterfall_pos + line) % snd_viz_waterfall_height;
                memcpy(&bvram[(y_start + line) * BMPPITCH + x_start], &snd_viz_waterfall[waterfall_line * snd_viz_waterfall_width], snd_viz_waterfall_width);
            }
            
            /* next time render into previous line to have a waterfall effect */
            snd_viz_waterfall_pos = (snd_viz_waterfall_pos + snd_viz_waterfall_height - 1) % snd_viz_waterfall_height;
            
            /* draw a border around the FFT area */
            bmp_draw_rect(COLOR_WHITE, x_start, y_start, snd_viz_waterfall_width, height);
            break;
        }
    }
    
    bmp_draw_to_idle(0);
    bmp_idle_copy(1,0);
}

static void snd_viz_create_window(float *table, uint32_t entries, enum windowing_function function, float *windowing_constant)
{
    switch(function)
    {
        case WINDOW_Hamming:
            *windowing_constant = 0.54f;
            break;

        case WINDOW_RaisedCosine:
            *windowing_constant = 0.5f;
            break;

        case WINDOW_Blackman:
            *windowing_constant = 0.42f;
            break;

        case WINDOW_BlackmanHarris:
            *windowing_constant = 0.35875f;
            break;

        case WINDOW_BlackmanNuttal:
            *windowing_constant = 0.3635819f;
            break;

        case WINDOW_FlatTop:
            *windowing_constant = 1.0f;
            break;

        case WINDOW_None:
        default:
            *windowing_constant = 1.0f;
            break;
    }

    for(uint32_t n = 0; n < entries; n++)
    {
        float M = entries - 1;
        float value = 0;
        float pi = 3.1415926f;

        switch (function)
        {
            case WINDOW_Hamming:
                value = *windowing_constant - 0.46f * cosf(2 * n * pi / M);
                break;

            case WINDOW_RaisedCosine:
                value = *windowing_constant - 0.5f * cosf(2 * n * pi / M);
                break;

            case WINDOW_Blackman:
                value = *windowing_constant - 0.5f * cosf(2 * n * pi / M) + 0.08f * cosf(4 * n * pi / M);
                break;

            case WINDOW_BlackmanHarris:
                value = *windowing_constant - 0.48829f * cosf(2 * n * pi / M) + 0.14128f * cosf(4 * n * pi / M) - 0.01168f * cosf(6 * n * pi / M);
                break;

            case WINDOW_BlackmanNuttal:
                value = *windowing_constant - 0.4891775f * cosf(2 * n * pi / M) + 0.1365995f * cosf(4 * n * pi / M) - 0.0106411f * cosf(6 * n * pi / M);
                break;

            case WINDOW_FlatTop:
                value = *windowing_constant - 1.93f * cosf(2 * n * pi / M) + 1.29f * cosf(4 * n * pi / M) - 0.388f * cosf(6 * n * pi / M) + 0.032f * cosf(8 * n * pi / M);
                break;

            case WINDOW_None:
            default:
                value = 1.0f;
                break;
        }
        table[n] = value;
    }
}

static void snd_viz_task(int unused)
{
    if(!snd_viz_alloc_buffers())
    {
        snd_viz_free_buffers();
        snd_viz_in_active = 0;
        return;
    }
    
    if(!snd_viz_start_audio())
    {
        snd_viz_free_buffers();
        snd_viz_in_active = 0;
        return;
    }
    
    /* keep local to make sure menu changes wont cause buffer overflows */
    uint32_t fft_size = snd_viz_fft_size;
    
    kiss_fft_cfg cfg = kiss_fft_alloc(fft_size, 0, 0 ,0);
    kiss_fft_cpx *fft_in = malloc(fft_size * sizeof(kiss_fft_cpx));
    kiss_fft_cpx *fft_out = malloc(fft_size * sizeof(kiss_fft_cpx));
    
    float *windowing_function = malloc(sizeof(float) * fft_size);
    float windowing_constant = 1.0f;
    snd_viz_create_window(windowing_function, fft_size, snd_viz_fft_window, &windowing_constant);
    
    snd_viz_waterfall_pos = 0;
    snd_viz_waterfall = malloc(snd_viz_waterfall_width * snd_viz_waterfall_height);

    TASK_LOOP
    {
        struct sound_buffer *buffer = NULL;
        
        if(!snd_viz_running)
        {
            trace_write(trace_ctx, "   --> snd_viz_task: exiting");
            break;
        }
        
        /* receive write job from dispatcher */
        if(msg_queue_receive(snd_viz_buffers_done, &buffer, 100))
        {
            static uint32_t timeouts = 0;
            trace_write(trace_ctx, "   --> snd_viz_task: message timed out %d times now", ++timeouts);
            continue;
        }
        
        if(!gui_menu_shown())
        {
            switch(snd_viz_mode)
            {
                case VIZ_MODE_FFT_BARS:
                case VIZ_MODE_FFT_LINE:
                case VIZ_MODE_WATERFALL:
                {
                    uint32_t channels = 2;
                    
                    for(uint32_t chan = 0; chan < channels; chan++)
                    {
                        /* pepare data for KISS FFT */
                        for(uint32_t pos = 0; pos < fft_size; pos++)
                        {
                            int16_t *data = (int16_t *)buffer->data;
                            float sample = FIX_TO_FLOAT(data[channels * pos + chan]);
                            
                            fft_in[pos].r = FLOAT_TO_FIX(sample * windowing_function[pos]);
                            fft_in[pos].i = 0;
                        }
                        
                        /* process FFT */
                        kiss_fft(cfg, fft_in, fft_out);
                        
                        /* show */
                        snd_viz_show_fft(fft_out, fft_size, windowing_constant, 10, 50 + chan * 400 / channels, 700, 400/channels);
                    }
                    break;
                }
                
                case VIZ_MODE_CORRELATION:
                {
                    int16_t *data = (int16_t *)buffer->data;
                    
                    bmp_draw_to_idle(1);
                    
                    /* align the X/Y plot to the center of the scren */
                    bmp_fill(COLOR_BLACK, BMP_WIDTH / 2 - snd_viz_correl_size / 2, BMP_HEIGHT / 2 - snd_viz_correl_size / 2, snd_viz_correl_size, snd_viz_correl_size);
                    /* draw the diagonal on which perfectly corellating signals would be */
                    draw_line(BMP_WIDTH / 2 + snd_viz_correl_size / 2, BMP_HEIGHT / 2 + snd_viz_correl_size / 2, BMP_WIDTH / 2 - snd_viz_correl_size / 2, BMP_HEIGHT / 2 - snd_viz_correl_size / 2, COLOR_GRAY(10));
                    
                    uint32_t samples = buffer->size / 2;
                    
                    for(uint32_t pos = 0; pos < samples / 2; pos += 2)
                    {
                        int16_t sample_l = data[2 * pos + 0];
                        int16_t sample_r = data[2 * pos + 1];
                        
                        /* scale sample values to rect size */
                        uint32_t x = (sample_l * snd_viz_correl_size / 2) / 32768;
                        uint32_t y = (sample_r * snd_viz_correl_size / 2) / 32768;
                        
                        bmp_putpixel(BMP_WIDTH / 2 + x, BMP_HEIGHT / 2 + y, COLOR_RED);
                    }
                    
                    bmp_draw_rect(COLOR_WHITE, BMP_WIDTH / 2 - snd_viz_correl_size / 2, BMP_HEIGHT / 2 - snd_viz_correl_size / 2, snd_viz_correl_size, snd_viz_correl_size);
                    
                    bmp_draw_to_idle(0);
                    bmp_idle_copy(1,0);
                    break;
                }
            }
        }
        
        msg_queue_post(snd_viz_buffers_empty, (uint32_t)buffer);

        /* ensure we are not blocking the system too much */
        msleep(10);
    }
    
    snd_viz_stop_audio();
    snd_viz_free_buffers();
    
    free(cfg);
    free(fft_in);
    free(fft_out);
    free(windowing_function);
    free(snd_viz_waterfall);
    
    snd_viz_in_active = 0;
}

static void snd_viz_start()
{
    if(snd_viz_enable_tracing && (trace_ctx == TRACE_ERROR))
    {
        char filename[100];
        snprintf(filename, sizeof(filename), "snd_viz.txt");
        trace_ctx = trace_start("snd_viz", filename);
        trace_format(trace_ctx, TRACE_FMT_TIME_REL | TRACE_FMT_COMMENT, ' ');
    }
    
    trace_write(trace_ctx, "snd_viz_start: starting");
    snd_viz_running = 1;
    snd_viz_in_active = 1;
    task_create("snd_viz", 0x16, 0x1000, snd_viz_task, NULL);
}

static void snd_viz_stop()
{
    if(!snd_viz_running)
    {
        return;
    }
    snd_viz_running = 0;
    
    /* wait until audio stopped */
    uint32_t loops = 100;
    while(snd_viz_in_active && loops > 0)
    {
        loops--;
        msleep(20);
    }

    if(snd_viz_in_active)
    {
        bmp_printf(FONT(FONT_MED, COLOR_RED, COLOR_BLACK), 10, 130, "audio failed to stop");
        trace_write(trace_ctx, "snd_viz_stop: failed to stop audio");
    }

    snd_viz_in_active = 0;
}

static MENU_SELECT_FUNC(snd_viz_test_select)
{
    if(snd_viz_running)
    {
        snd_viz_stop();
    }
    else
    {
        snd_viz_start();
    }
}


static struct menu_entry snd_viz_menu[] =
{
    {
        .name = "Sound visualization",
        .priv = &snd_viz_running,
        .max = 1,
        .select = &snd_viz_test_select,
        .help = "Live sound views like FFT spectrogram or scope.",
        .submenu_width = 710,
        .children = (struct menu_entry[])
        {
            {
                .name = "Mode",
                .priv = &snd_viz_mode,
                .max = 3,
                .choices = (const char *[]) {"FFT Lines", "FFT Bars", "Waterfall", "L/R correlation"},
                .help = "Select visualization type",
            },
            MENU_EOL,
        },
    },
};

static unsigned int snd_viz_init()
{    
    menu_add("Audio", snd_viz_menu, COUNT(snd_viz_menu));
    
    /* we have two queues that contain free and filled buffers */
    snd_viz_buffers_empty = (struct msg_queue *) msg_queue_create("snd_viz_buffers_empty", 20);
    snd_viz_buffers_done = (struct msg_queue *) msg_queue_create("snd_viz_buffers_done", 20);
    
    /* setup audio handle */
    snd_viz_sound_ctx = sound_alloc();
    snd_viz_sound_ctx->mode = SOUND_MODE_RECORD;
    
    snd_viz_sound_ctx->format.rate = snd_viz_in_sample_rate;
    snd_viz_sound_ctx->format.channels = 2;
    snd_viz_sound_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    
    return 0;
}

static unsigned int snd_viz_deinit()
{
    menu_remove("Audio", snd_viz_menu, COUNT(snd_viz_menu));
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT(snd_viz_init)
    MODULE_DEINIT(snd_viz_deinit)
MODULE_INFO_END()

MODULE_CONFIGS_START()
    MODULE_CONFIG(snd_viz_mode)
    MODULE_CONFIG(snd_viz_enable_tracing)
    MODULE_CONFIG(snd_viz_fft_size)
    MODULE_CONFIG(snd_viz_fft_window)
MODULE_CONFIGS_END()


