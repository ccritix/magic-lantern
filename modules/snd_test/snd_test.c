
#define __snd_test_c__

#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <menu.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <console.h>

#include "sound.h"
#include "../plot/plot.h"


static void generate_beep_tone(int16_t* buf, int N, int freq)
{
    // for sine wave: 1 hz => t = i * 2*pi*MUL / 48000
    int twopi = 102944;
    float factor = (int)roundf((float)twopi / 48000.0f * freq);
    
    for (int i = 0; i < N; i++)
    {
        int t = (int)(factor * i);
        int ang = t % twopi;
        int s = sinf((float)ang / 16384) * 16384;

        buf[i] = COERCE(s*2, -32767, 32767);
    }
}

#define BEEP_BUFFERS 14
static uint32_t beep_freqs[BEEP_BUFFERS] = { 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000, 13000, 14000 };
static struct sound_buffer *beep_buffers[BEEP_BUFFERS];
static uint32_t beep_buffer_num = 0;
static struct sound_ctx *playback_ctx = NULL;

#define REC_BUFFERS 8
static struct sound_buffer *rec_buffers[REC_BUFFERS];
static uint32_t rec_buffer_num = 0;
static struct sound_ctx *record_ctx = NULL;
static plot_coll_t *rec_data = NULL;
static plot_graph_t *rec_plot = NULL;


static enum sound_flow beep_processed (struct sound_buffer *buffer)
{
    playback_ctx->ops.enqueue(playback_ctx, beep_buffers[beep_buffer_num++]);
    beep_buffer_num %= BEEP_BUFFERS;
    
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow beep_cleanup (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow beep_requeued (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

static void beep_init()
{
    playback_ctx = sound_alloc();
    
    playback_ctx->mode = SOUND_MODE_PLAYBACK;
    playback_ctx->format.rate = 48000;
    playback_ctx->format.channels = 1;
    playback_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    playback_ctx->mixer.speaker_gain = 30;
    playback_ctx->mixer.headphone_gain = 30;
    playback_ctx->mixer.destination_line = SOUND_DESTINATION_SPK;
    
    record_ctx = sound_alloc();
    
    record_ctx->mode = SOUND_MODE_RECORD;
    record_ctx->format.rate = 48000;
    record_ctx->format.channels = 1;
    record_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    record_ctx->mixer.source_line = SOUND_SOURCE_INT_MIC;
}

static void beep_start()
{
    if(beep_buffers[0])
    {
        return;
    }
    
    /* setup buffers for beeping */
    for(uint32_t buf = 0; buf < BEEP_BUFFERS; buf++)
    {
        beep_buffers[buf] = sound_alloc_buffer();
        
        /* just make a 250 ms beep, we can for sure pimp that to any duration/frequency */
        beep_buffers[buf]->size = playback_ctx->format.rate / 4;
        beep_buffers[buf]->data = malloc(beep_buffers[buf]->size);
        
        beep_buffers[buf]->processed = &beep_processed;
        beep_buffers[buf]->cleanup = &beep_cleanup;
        beep_buffers[buf]->requeued = &beep_requeued;
        beep_buffers[buf]->priv = (void *)buf;
        
        generate_beep_tone(beep_buffers[buf]->data, beep_buffers[buf]->size / 2, beep_freqs[buf]);
    }
    
    /* okay, we are going to play now. all others have to shut up. (they get paused and resumed later) */
    playback_ctx->ops.lock(playback_ctx, SOUND_LOCK_EXCLUSIVE);
    
    beep_buffer_num = 0;
    playback_ctx->ops.enqueue(playback_ctx, beep_buffers[beep_buffer_num++]);
    playback_ctx->ops.enqueue(playback_ctx, beep_buffers[beep_buffer_num++]);
    playback_ctx->ops.enqueue(playback_ctx, beep_buffers[beep_buffer_num++]);
    
    playback_ctx->ops.start(playback_ctx);
}
    
static void beep_stop()
{
    if(!beep_buffers[0])
    {
        return;
    }
    
    /* its enough, stop */
    playback_ctx->ops.stop(playback_ctx);
    
    /* now others may play again, if any. */
    playback_ctx->ops.unlock(playback_ctx);
    
    for(uint32_t buf = 0; buf < BEEP_BUFFERS; buf++)
    {
        free(beep_buffers[buf]->data);
        free(beep_buffers[buf]);
        beep_buffers[buf] = NULL;
    }
}

static enum sound_flow rec_processed (struct sound_buffer *buffer)
{
    record_ctx->ops.enqueue(record_ctx, rec_buffers[rec_buffer_num++]);
    rec_buffer_num %= REC_BUFFERS;
    
    plot_clear(rec_data);
    
    for(uint32_t pos = 0; pos < buffer->size / 2; pos++)
    {
        plot_add(rec_data, (float) (((int16_t *)buffer->data)[pos]));
    }
    
    plot_autorange(rec_data, rec_plot);
    plot_graph_draw(rec_data, rec_plot);
    
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow rec_cleanup (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow rec_requeued (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

static void rec_init()
{
    record_ctx = sound_alloc();
    
    record_ctx->mode = SOUND_MODE_PLAYBACK;
    record_ctx->format.rate = 48000;
    record_ctx->format.channels = 1;
    record_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    record_ctx->mixer.speaker_gain = 30;
    record_ctx->mixer.headphone_gain = 30;
    record_ctx->mixer.destination_line = SOUND_DESTINATION_SPK;
}

static void rec_start()
{
    if(rec_buffers[0])
    {
        return;
    }
    
    rec_data = plot_alloc_data(1);
    rec_plot = plot_alloc_graph(60, 60, 600, 400);
    
    if(rec_plot)
    {
        rec_plot->dot_size = 1;
        rec_plot->color_dots = COLOR_GREEN1;
        rec_plot->color_lines = PLOT_COLOR_NONE;
        rec_plot->color_bg = COLOR_GRAY(10);
        rec_plot->color_border = COLOR_WHITE;
        rec_plot->color_axis = COLOR_YELLOW;
    }
    
    /* setup buffers for beeping */
    for(uint32_t buf = 0; buf < REC_BUFFERS; buf++)
    {
        rec_buffers[buf] = sound_alloc_buffer();
        
        rec_buffers[buf]->size = record_ctx->format.rate / 25;
        rec_buffers[buf]->data = malloc(rec_buffers[buf]->size);
        
        rec_buffers[buf]->processed = &rec_processed;
        rec_buffers[buf]->cleanup = &rec_cleanup;
        rec_buffers[buf]->requeued = &rec_requeued;
        rec_buffers[buf]->priv = (void *)buf;
    }
    
    /* okay, we are going to play now. all others have to shut up. (they get paused and resumed later) */
    record_ctx->ops.lock(record_ctx, SOUND_LOCK_EXCLUSIVE);
    
    rec_buffer_num = 0;
    record_ctx->ops.enqueue(record_ctx, rec_buffers[rec_buffer_num++]);
    record_ctx->ops.enqueue(record_ctx, rec_buffers[rec_buffer_num++]);
    record_ctx->ops.enqueue(record_ctx, rec_buffers[rec_buffer_num++]);
    
    record_ctx->ops.start(record_ctx);
}
    
static void rec_stop()
{
    if(!rec_buffers[0])
    {
        return;
    }
    
    /* its enough, stop */
    record_ctx->ops.stop(record_ctx);
    
    /* now others may play again, if any. */
    record_ctx->ops.unlock(record_ctx);
    
    for(uint32_t buf = 0; buf < REC_BUFFERS; buf++)
    {
        free(rec_buffers[buf]->data);
        free(rec_buffers[buf]);
        rec_buffers[buf] = NULL;
    }
}

static MENU_SELECT_FUNC(snd_test_menu_rec_start)
{
    rec_start();
}

static MENU_SELECT_FUNC(snd_test_menu_rec_stop)
{
    rec_stop();
}

static MENU_SELECT_FUNC(snd_test_menu_beep_start)
{
    beep_start();
}

static MENU_SELECT_FUNC(snd_test_menu_beep_stop)
{
    beep_stop();
}

static MENU_UPDATE_FUNC(snd_test_menu_vol_update)
{
    MENU_SET_VALUE("%d %%", playback_ctx->mixer.headphone_gain);
}

static MENU_UPDATE_FUNC(snd_test_in_line_update)
{
    enum sound_source line = record_ctx->mixer.source_line;
    const char *name = record_ctx->device->codec_ops.get_source_name(line);
    
    MENU_SET_VALUE("%s (#%d)", name, line);
}

static MENU_UPDATE_FUNC(snd_test_out_line_update)
{
    enum sound_destination line = playback_ctx->mixer.destination_line;
    const char *name = playback_ctx->device->codec_ops.get_destination_name(line);
    
    MENU_SET_VALUE("%s (#%d)", name, line);
}

static MENU_UPDATE_FUNC(snd_test_loop_update)
{
    MENU_SET_VALUE("%s", playback_ctx->mixer.loop_mode?"ON":"OFF");
}

static MENU_SELECT_FUNC(snd_test_menu_vol_select)
{
    playback_ctx->mixer.headphone_gain = COERCE(playback_ctx->mixer.headphone_gain + delta, 0, 100);
    playback_ctx->mixer.speaker_gain = playback_ctx->mixer.headphone_gain;
    playback_ctx->ops.apply(playback_ctx);
}

static MENU_SELECT_FUNC(snd_test_in_line_select)
{
    record_ctx->mixer.source_line = (enum sound_source)COERCE(record_ctx->mixer.source_line + delta, 0, SOUND_DESTINATION_EXTENDED_6);
    record_ctx->ops.apply(record_ctx);
}

static MENU_SELECT_FUNC(snd_test_out_line_select)
{
    playback_ctx->mixer.destination_line = (enum sound_destination)COERCE(playback_ctx->mixer.destination_line + delta, 0, SOUND_DESTINATION_EXTENDED_6);
    playback_ctx->ops.apply(playback_ctx);
}

static MENU_SELECT_FUNC(snd_test_loop_select)
{
    playback_ctx->mixer.loop_mode = !playback_ctx->mixer.loop_mode;
    playback_ctx->ops.apply(playback_ctx);
}

static struct menu_entry snd_test_menu[] =
{
    {
        .name = "Sound test",
        .help = "",
        .submenu_width = 710,
        .children = (struct menu_entry[])
        {
            {
                .name = "Start Beep",
                .select = &snd_test_menu_beep_start,
                .help = "Start beeping",
            },
            {
                .name = "Stop Beep",
                .select = &snd_test_menu_beep_stop,
                .help = "Stop playback",
            },
            {
                .name = "Start Rec",
                .select = &snd_test_menu_rec_start,
                .help = "Start beeping",
            },
            {
                .name = "Stop Rec",
                .select = &snd_test_menu_rec_stop,
                .help = "Stop playback",
            },
            {
                .name = "Output volume",
                .update = &snd_test_menu_vol_update,
                .select = &snd_test_menu_vol_select,
                .help = "Audio output volume (0-100).",
            },
            {
                .name = "Output line",
                .update = &snd_test_out_line_update,
                .select = &snd_test_out_line_select,
                .max = SOUND_DESTINATION_EXTENDED_6,
                .help = "Audio output line (0-3).",
            },
            {
                .name = "Input line",
                .update = &snd_test_in_line_update,
                .select = &snd_test_in_line_select,
                .max = SOUND_SOURCE_EXTENDED_6,
                .help = "Audio input line (0-3).",
            },
            {
                .name = "Loopback",
                .update = &snd_test_loop_update,
                .select = &snd_test_loop_select,
                .max = 1,
                .help = "Enable loopback (no effect)",
            },
            MENU_EOL,
        },
    },
};

static unsigned int snd_test_init()
{
    beep_init();

    menu_add("Games", snd_test_menu, COUNT(snd_test_menu));
    return 0;
}

static unsigned int snd_test_deinit()
{
    menu_remove("Games", snd_test_menu, COUNT(snd_test_menu));
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT(snd_test_init)
    MODULE_DEINIT(snd_test_deinit)
MODULE_INFO_END()

