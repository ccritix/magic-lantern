

#include "dryos.h"
#include "menu.h"
#include "bmp.h"
#include "config.h"
#include <string.h>

#include "sound.h"
#include "wav.h"

struct wav_data *wav_test_rec = NULL;

// https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
// http://www.sonicspot.com/guide/wavefiles.html
static uint8_t wav_header[] = {
    0x52, 0x49, 0x46, 0x46, // RIFF
    0xff, 0xff, 0xff, 0xff, // chunk size: (file size) - 8
    0x57, 0x41, 0x56, 0x45, // WAVE
    0x66, 0x6d, 0x74, 0x20, // fmt 
    0x10, 0x00, 0x00, 0x00, // subchunk size = 16
    0x01, 0x00,             // PCM uncompressed
    0x02, 0x00,             // stereo
    0x80, 0xbb, 0x00, 0x00, // 48000 Hz
    0x00, 0x77, 0x01, 0x00, // 96000 bytes / second
    0x02, 0x00,             // 2 bytes / sample
    0x10, 0x00,             // 16 bits / sample
    0x64, 0x61, 0x74, 0x61, // data
    0xff, 0xff, 0xff, 0xff, // data size (bytes)
};


static void wav_set_size(uint8_t *header, uint32_t size)
{
    uint32_t *data_size = (uint32_t *)&header[40];
    uint32_t *main_chunk_size = (uint32_t *)&header[4];
    *data_size = size;
    *main_chunk_size = size + 36;
}

struct wav_data *wav_record_init(char *filename)
{
    struct wav_data *ctx = malloc(sizeof(struct wav_data));
    
    ctx->sound = sound_alloc();
    ctx->sound->mode = SOUND_MODE_RECORD;
    ctx->sound->format.rate = 48000;
    ctx->sound->format.channels = 2;
    ctx->sound->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    ctx->sound->mixer.mic_gain = 100;
    ctx->sound->mixer.source_line = SOUND_SOURCE_INT_MIC;
    
    ctx->buffer_num = 0;
    ctx->buffers = 5;
    ctx->filename = strdup(filename);
    ctx->handle = FIO_CreateFile(filename);
    
    if(ctx->handle == INVALID_PTR)
    {
        sound_free(ctx->sound);
        free(ctx);
        return NULL;
    }
    
    FIO_WriteFile(ctx->handle, wav_header, sizeof(wav_header));
    
    return ctx;
}

static enum sound_flow wav_processed (struct sound_buffer *buffer)
{
    struct wav_data *ctx = (struct wav_data *)buffer->priv;
    
    ctx->sound->ops.enqueue(ctx->sound, ctx->buffer[ctx->buffer_num++]);
    ctx->buffer_num %= ctx->buffers;
    ctx->processed += buffer->size;
    
    FIO_WriteFile(ctx->handle, buffer->data, buffer->size);
    
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow wav_cleanup (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow wav_requeued (struct sound_buffer *buffer)
{
    return SOUND_FLOW_CONTINUE;
}

void wav_record_start(struct wav_data *ctx)
{
    ctx->buffer = malloc(sizeof(struct sound_buffer *) * ctx->buffers);
    
    /* setup buffers for recording */
    for(uint32_t buf = 0; buf < ctx->buffers; buf++)
    {
        ctx->buffer[buf] = sound_alloc_buffer();
        
        /* setup 250ms buffers -> /4 */
        ctx->buffer[buf]->size = ctx->sound->format.channels * ctx->sound->format.rate * 2 / 4;
        ctx->buffer[buf]->data = malloc(ctx->buffer[buf]->size);
        
        ctx->buffer[buf]->processed = &wav_processed;
        ctx->buffer[buf]->cleanup = &wav_cleanup;
        ctx->buffer[buf]->requeued = &wav_requeued;
        ctx->buffer[buf]->priv = (void *)ctx;
    }
    
    ctx->sound->ops.lock(ctx->sound, SOUND_LOCK_EXCLUSIVE);
    ctx->sound->ops.enqueue(ctx->sound, ctx->buffer[ctx->buffer_num++]);
    ctx->sound->ops.enqueue(ctx->sound, ctx->buffer[ctx->buffer_num++]);
    ctx->sound->ops.enqueue(ctx->sound, ctx->buffer[ctx->buffer_num++]);
    ctx->sound->ops.start(ctx->sound);
}

void wav_record_stop(struct wav_data *ctx)
{
    ctx->sound->ops.stop(ctx->sound);
    
    while(sound_get_state(ctx->sound) != SOUND_STATE_IDLE)
    {
        msleep(100);
    }
    
    /* rewrite header */
    uint8_t header[44];
    
    memcpy(header, wav_header, sizeof(header));
    wav_set_size(header, ctx->processed);

    FIO_SeekFile(ctx->handle, 0, SEEK_SET);
    FIO_WriteFile(ctx->handle, header, sizeof(header));
    FIO_CloseFile(ctx->handle);
    
    /* free allocated memories */
    for(uint32_t buf = 0; buf < ctx->buffers; buf++)
    {
        sound_free_buffer(ctx->buffer[buf]);
    }
    
    sound_free(ctx->sound);
    free(ctx);
}

#if 1
static MENU_SELECT_FUNC(wav_rec_start)
{
    if(!wav_test_rec)
    {
        wav_test_rec = wav_record_init("TEST.WAV");
        wav_record_start(wav_test_rec);
    }
}

static MENU_SELECT_FUNC(wav_rec_stop)
{
    if(wav_test_rec)
    {
        wav_record_stop(wav_test_rec);
    }
}

static struct menu_entry wav_menus[] = {
    {
        .name = "WAV recording",
        .select = menu_open_submenu,
        .submenu_width = 680,
        .help = "",
        .children = (struct menu_entry[]) {
            {
                .name = "Start recording",
                .select = &wav_rec_start,
                .help = "",
            },
            {
                .name = "Stop recording", 
                .select = &wav_rec_stop,
                .help = "",
            },
            MENU_EOL,
        }
    },
};


static void wav_init()
{
    menu_add("Audio", wav_menus, COUNT(wav_menus) );
}

INIT_FUNC("wav.init", wav_init);
#endif

