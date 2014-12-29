
#include <module.h>
#include <dryos.h>
#include <bmp.h>
#include <mem.h>
#include <menu.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <console.h>

#include "sound.h"
#include "mad.h"
#include "../trace/trace.h"
#include "../file_man/file_man.h"

#define MAX_PATH                 64

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct mp3_state
{
    unsigned char *data;
    uint32_t size;
    uint32_t pos;
    FILE *file;
    struct sound_ctx *sound_ctx;
    uint32_t flush;
};

typedef struct
{
    char fullPath[MAX_PATH];
    uint32_t fileSize;
    uint32_t timestamp;
} playlist_entry_t;

/* queue for playlist item submits by scanner tasks */
static struct msg_queue *mp3_play_playlist_queue;
static struct msg_queue *mp3_play_playlist_scan_queue;
static playlist_entry_t *mp3_play_playlist = NULL;
static uint32_t mp3_play_playlist_entries = 0;

static char mp3_play_next_filename[MAX_PATH];
static char mp3_play_current_filename[MAX_PATH];
static playlist_entry_t mp3_play_playlist_next(playlist_entry_t current);
static playlist_entry_t mp3_play_playlist_prev(playlist_entry_t current);
static void mp3_play_playlist_delete(playlist_entry_t current);
static void mp3_play_build_playlist(uint32_t priv);

static uint32_t mp3_play_audio_started = 0;
static uint32_t mp3_play_audio_stop = 0;
static uint32_t mp3_play_volume = 30;
static uint32_t mp3_play_nextfile = 0;
static int32_t mp3_play_out_line = SOUND_DESTINATION_SPK;

uint32_t trace_ctx = TRACE_ERROR;

static struct mp3_state mp3_state;


/* ================================== helpers to link libmad ================================== */
/* malloc/free helper that are called from libmad code. rewrite libmad using patch? */
void *mp3_play_malloc(uint32_t size)
{
    return malloc(size);
}

void mp3_play_free(void *ptr)
{
    return free(ptr);
}

/* also used by libmad */
void abort()
{
}

/* quite a bit nasty to globally define that function, but there is no other way except patching libmad 
 * this function is only visible within modules when linking them.
*/
int sprintf(char *ptr, const char *fmt, ...)
{
    va_list ap;
    int str_l;

    va_start(ap, fmt);
    str_l = vsnprintf(ptr, (size_t)128, fmt, ap);
    va_end(ap);

    return str_l;
}



/* ================================== libmad callbacks ================================== */

/* helper: scale libmad sample to a signed integer for writing to sound device */
static signed int mp3_play_sample_scale(mad_fixed_t value)
{
    value += (1 << (MAD_F_FRACBITS - 16));
    value = COERCE(value, -MAD_F_ONE, MAD_F_ONE - 1);
    value >>= (MAD_F_FRACBITS - 15);
    
    return value;
}

/* (description copied from libmad for clarification)
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or
 * libmad/stream.h) header file.
 */
static enum mad_flow mp3_play_cbr_error(void *data, struct mad_stream *stream, struct mad_frame *frame)
{
    if(mp3_play_audio_stop)
    {
        return MAD_FLOW_BREAK;
    }
    return MAD_FLOW_CONTINUE;
}

/* (description copied from libmad for clarification)
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */
static enum mad_flow mp3_play_cbr_input(void *data, struct mad_stream *stream)
{
    struct mp3_state *mp3_state = data;
    uint32_t remaining = 0;
    
    if(mp3_play_audio_stop || mp3_play_nextfile)
    {
        return MAD_FLOW_BREAK;
    }
    
    remaining = stream->bufend - stream->this_frame;
    memmove(mp3_state->data, stream->this_frame, remaining);

    int32_t ret = FIO_ReadFile(mp3_state->file, &mp3_state->data[remaining], mp3_state->size - remaining);

    if(ret <= 0)
    {
        bmp_printf(FONT_MED, 30, 40, "Read file... (%d) ERROR", ret);
        return MAD_FLOW_STOP;
    }

    mad_stream_buffer(stream, mp3_state->data, ret + remaining);

    return MAD_FLOW_CONTINUE;
}

static enum sound_flow mp3_play_processed_cbr(struct sound_buffer *buffer)
{
    if(!buffer->data)
    {
        return SOUND_FLOW_STOP;
    }
    free(buffer->data);
    buffer->data = NULL;
    free(buffer);
    
    return SOUND_FLOW_CONTINUE;
}

static enum sound_flow mp3_play_cleanup_cbr(struct sound_buffer *buffer)
{
    if(!buffer->data)
    {
        return SOUND_FLOW_STOP;
    }
    free(buffer->data);
    buffer->data = NULL;
    free(buffer);
    
    return SOUND_FLOW_CONTINUE;
}


/* (description copied from libmad for clarification)
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */
static enum mad_flow mp3_play_cbr_output(void *data, struct mad_header const *header, struct mad_pcm *pcm)
{
    struct mp3_state *mp3_state = data;
    static int16_t *fill_buf = NULL;
    static uint32_t fill_buf_used = 0;
    static uint32_t fill_buf_size = 0;
    unsigned int samplerate = 0;
    unsigned int nchannels = 0;
    unsigned int nsamples = 0;
    mad_fixed_t const *left_ch;
    mad_fixed_t const *right_ch;

    if(mp3_play_audio_stop || mp3_play_nextfile)
    {
        fill_buf_used = 0;
        return MAD_FLOW_BREAK;
    }
    
    /* pcm->samplerate contains the sampling frequency */
    samplerate = pcm->samplerate;
    nchannels  = pcm->channels;
    nsamples   = pcm->length;
    left_ch    = pcm->samples[0];
    right_ch   = pcm->samples[1];

    if(!fill_buf)
    {
        fill_buf_used = 0;
        fill_buf_size = samplerate * nchannels / 4;
        fill_buf = malloc(fill_buf_size * sizeof(int16_t));
    }

    if(mp3_state->sound_ctx->device->state == SOUND_STATE_IDLE)
    {
        mp3_state->sound_ctx->mode = SOUND_MODE_PLAYBACK;
        mp3_state->sound_ctx->format.rate = samplerate;
        mp3_state->sound_ctx->format.channels = MIN(nchannels, 2);
        mp3_state->sound_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
        mp3_state->sound_ctx->mixer.speaker_gain = mp3_play_volume;
        mp3_state->sound_ctx->mixer.headphone_gain = mp3_play_volume;
        mp3_state->sound_ctx->mixer.destination_line = mp3_play_out_line;
        
        mp3_state->sound_ctx->ops.start(mp3_state->sound_ctx);
    }
    
    if(fill_buf_used + (nchannels * nsamples) > fill_buf_size)
    {
        enum sound_result res;
        struct sound_buffer *snd_buf = malloc(sizeof(struct sound_buffer));
        
        snd_buf->size = fill_buf_used * sizeof(int16_t);
        snd_buf->data = malloc(snd_buf->size);
        memcpy(snd_buf->data, fill_buf, snd_buf->size);
        
        snd_buf->processed = &mp3_play_processed_cbr;
        snd_buf->cleanup = &mp3_play_cleanup_cbr;
        snd_buf->requeued = NULL;
        
        if(mp3_state->flush)
        {
            mp3_state->sound_ctx->ops.stop(mp3_state->sound_ctx);
            mp3_state->sound_ctx->ops.start(mp3_state->sound_ctx);
            mp3_state->flush = 0;
        }
        
retry_enqueue:
        res = mp3_state->sound_ctx->ops.enqueue(mp3_state->sound_ctx, snd_buf);
        
        if(res == SOUND_RESULT_TRYAGAIN)
        {
            /* ToDo: are we allowed to do that? */
            msleep(200);
            goto retry_enqueue;
        }
        
        fill_buf_used = 0;
    }
    
    while(nsamples--)
    {
        fill_buf[fill_buf_used++] = (mp3_play_sample_scale(*left_ch++) & 0xFFFF);
        if(nchannels > 1)
        {
            fill_buf[fill_buf_used++] = (mp3_play_sample_scale(*right_ch++) & 0xFFFF);
        }
    }

    return MAD_FLOW_CONTINUE;
}

static void mp3_play_task(char *filename)
{
    struct mad_decoder decoder;
    
    if(mp3_play_audio_started)
    {
        int wait_loops = 0;
        
        mp3_play_audio_stop = 1;
        
        while(mp3_play_audio_started)
        {
            msleep(100);
            if(wait_loops++ > 20)
            {
                bmp_printf(FONT_MED, 30, 60, "Failed to stop previous playback");
                return;
            }
        }
    }

    mp3_play_audio_started = 1;
    mp3_play_audio_stop = 0;

    strncpy(mp3_play_current_filename, filename, sizeof(mp3_play_current_filename));
    
    FILE *f = FIO_OpenFile(filename, O_RDONLY | O_SYNC );
    if(f == NULL)
    {
        bmp_printf(FONT_MED, 30, 90, "Failed to open file");
        return;
    }

    /* setup read mp3_state data */
    mp3_state.file = f;
    mp3_state.flush = 0;
    mp3_state.pos = 0;
    mp3_state.size = 256 * 1024;
    mp3_state.data = malloc(mp3_state.size);
    
    mp3_state.sound_ctx = sound_alloc();
    mp3_state.sound_ctx->ops.lock(mp3_state.sound_ctx, SOUND_LOCK_EXCLUSIVE);
    
    /* configure input, output, and error functions */
    while(!mp3_play_audio_stop)
    {
        mp3_state.flush = 1;
        mad_decoder_init(&decoder, &mp3_state, &mp3_play_cbr_input, NULL, NULL, &mp3_play_cbr_output, &mp3_play_cbr_error, NULL);
        mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
        mad_decoder_finish(&decoder);
        
        FIO_CloseFile(mp3_state.file);
        
        if(mp3_play_nextfile)
        {
            mp3_play_nextfile = 0;
            strncpy(mp3_play_current_filename, mp3_play_next_filename, sizeof(mp3_play_current_filename));
        }
        else
        {
            /* try to play the next file */
            playlist_entry_t current;
            playlist_entry_t next;
            
            strncpy(current.fullPath, mp3_play_current_filename, sizeof(current.fullPath));
            next = mp3_play_playlist_next(current);
            
            if(!strlen(next.fullPath))
            {
                bmp_printf(FONT_MED, 30, 90, "Failed to get next file");
                break;
            }
            strncpy(mp3_play_current_filename, next.fullPath, sizeof(mp3_play_current_filename));
        }
        
        FILE *f = FIO_OpenFile(mp3_play_current_filename, O_RDONLY | O_SYNC );
        if(f == NULL)
        {
            bmp_printf(FONT_MED, 30, 90, "Failed to open file");
            break;
        }
        
        mp3_state.file = f;
    }
    
    mp3_state.sound_ctx->ops.stop(mp3_state.sound_ctx);
    mp3_state.sound_ctx->ops.unlock(mp3_state.sound_ctx);
    
    free(mp3_state.data);
    
    mp3_play_audio_stop = 0;
    mp3_play_audio_started = 0;
}

static void mp3_play_build_playlist_path(char *directory)
{
    struct fio_file file;
    struct fio_dirent * dirent = NULL;
    
    dirent = FIO_FindFirstEx(directory, &file);
    if(IS_ERROR(dirent))
    {
        return;
    }
    
    char *full_path = malloc(MAX_PATH);

    do
    {
        if(file.name[0] == '.')
        {
            continue;
        }

        snprintf(full_path, MAX_PATH, "%s%s", directory, file.name);
        
        /* do not recurse here, but enqueue next directory */
        if(file.mode & ATTR_DIRECTORY)
        {
            strcat(full_path, "/");
            
            msg_queue_post(mp3_play_playlist_scan_queue, (uint32_t) strdup(full_path));
        }
        else
        {
            char *suffix = &file.name[strlen(file.name) - 3];
            
            if(!strcmp("MP3", suffix))
            {
                playlist_entry_t *entry = malloc(sizeof(playlist_entry_t));
                
                strncpy(entry->fullPath, full_path, sizeof(entry->fullPath));
                entry->fileSize = file.size;
                entry->timestamp = file.timestamp;
                
                /* update playlist */
                msg_queue_post(mp3_play_playlist_queue, (uint32_t) entry);
            }
        }
    }
    while(FIO_FindNextEx(dirent, &file) == 0);
    
    FIO_FindClose(dirent);
    free(full_path);
}

static void mp3_play_free_playlist()
{
    /* clear the playlist */
    mp3_play_playlist_entries = 0;
    if(mp3_play_playlist)
    {
        free(mp3_play_playlist);
    }
    mp3_play_playlist = NULL;
    
    /* clear items in queues */
    void *entry = NULL;
    while(!msg_queue_receive(mp3_play_playlist_queue, &entry, 50))
    {
        free(entry);
    }
    while(!msg_queue_receive(mp3_play_playlist_scan_queue, &entry, 50))
    {
        free(entry);
    }
}

static void mp3_play_build_playlist(uint32_t priv)
{
    playlist_entry_t *entry = NULL;
    
    /* clear the playlist */
    mp3_play_free_playlist();
    
    /* set up initial directories to scan. try to not recurse, but use scan and result queues */
    msg_queue_post(mp3_play_playlist_scan_queue, (uint32_t) strdup("A:/"));
    msg_queue_post(mp3_play_playlist_scan_queue, (uint32_t) strdup("B:/"));
    
    char *directory = NULL;
    while(!msg_queue_receive(mp3_play_playlist_scan_queue, &directory, 50))
    {
        mp3_play_build_playlist_path(directory);
        free(directory);
    }
    
    /* pre-allocate the number of enqueued playlist items */
    uint32_t msg_count = 0;
    msg_queue_count(mp3_play_playlist_queue, &msg_count);
    playlist_entry_t *playlist = malloc(msg_count * sizeof(playlist_entry_t));
    
    /* add all items */
    while(!msg_queue_receive(mp3_play_playlist_queue, &entry, 50))
    {
        playlist[mp3_play_playlist_entries++] = *entry;
        free(entry);
    }
    
    mp3_play_playlist = playlist;
}

static int32_t mp3_play_playlist_find(playlist_entry_t current)
{
    for(uint32_t pos = 0; pos < mp3_play_playlist_entries; pos++)
    {
        if(!strcmp(current.fullPath, mp3_play_playlist[pos].fullPath))
        {
            return pos;
        }
    }
    
    return -1;
}

static void mp3_play_playlist_delete(playlist_entry_t current)
{
    int32_t pos = mp3_play_playlist_find(current);
    
    if(pos >= 0)
    {
        uint32_t remaining = mp3_play_playlist_entries - pos - 1;
        memcpy(&mp3_play_playlist[pos], &mp3_play_playlist[pos+1], remaining * sizeof(playlist_entry_t));
        mp3_play_playlist_entries--;
    }
}

static playlist_entry_t mp3_play_playlist_next(playlist_entry_t current)
{
    playlist_entry_t ret;
    
    strcpy(ret.fullPath, "");
    int32_t pos = mp3_play_playlist_find(current);
    
    if(pos >= 0 && (uint32_t)(pos + 1) < mp3_play_playlist_entries)
    {
        ret = mp3_play_playlist[pos + 1];
    }
    
    return ret;
}

static playlist_entry_t mp3_play_playlist_prev(playlist_entry_t current)
{
    playlist_entry_t ret;
    
    strcpy(ret.fullPath, "");
    int32_t pos = mp3_play_playlist_find(current);
    
    if(pos > 0)
    {
        ret = mp3_play_playlist[pos - 1];
    }
    
    return ret;
}

static void mp3_play_start(char *filename)
{
    task_create("mp3_play_task", 0x1A, 0x6000, mp3_play_task, filename);
}

static void mp3_play_next()
{
    playlist_entry_t current;
    playlist_entry_t next;
    
    strncpy(current.fullPath, mp3_play_current_filename, sizeof(current.fullPath));
    
    next = mp3_play_playlist_next(current);
    
    if(strlen(next.fullPath))
    {
        strncpy(mp3_play_next_filename, next.fullPath, sizeof(mp3_play_next_filename));
        mp3_play_nextfile = 1;
    }
}

static void mp3_play_prev()
{
    playlist_entry_t current;
    playlist_entry_t next;
    
    strncpy(current.fullPath, mp3_play_current_filename, sizeof(current.fullPath));
    
    next = mp3_play_playlist_prev(current);
    
    if(strlen(next.fullPath))
    {
        strncpy(mp3_play_next_filename, next.fullPath, sizeof(mp3_play_next_filename));
        mp3_play_nextfile = 1;
    }
}


FILETYPE_HANDLER(mp3_play_filehandler)
{
    switch(cmd)
    {
        case FILEMAN_CMD_INFO:
        {
            strcpy(data, "MP3 audio file (no ID3 yet)");
            return 1;
        }

        case FILEMAN_CMD_VIEW_OUTSIDE_MENU:
        {
            mp3_play_start(filename);
            return 1;
        }
    }

    return 0; /* command not handled */
}

static MENU_SELECT_FUNC(mp3_play_menu_next)
{
    if(mp3_play_audio_started)
    {
        mp3_play_next();
    }
}

static MENU_SELECT_FUNC(mp3_play_menu_prev)
{
    if(mp3_play_audio_started)
    {
        mp3_play_prev();
    }
}

static MENU_SELECT_FUNC(mp3_play_menu_stop)
{
    if(mp3_play_audio_started)
    {
        mp3_play_audio_stop = 1;
    }
}

static MENU_SELECT_FUNC(mp3_play_menu_play)
{
    if(!mp3_play_audio_started)
    {
        mp3_play_build_playlist(0);
        bmp_printf(FONT_MED, 30, 40, "Entries: %d ", mp3_play_playlist_entries);
        
        if(mp3_play_playlist && mp3_play_playlist_entries)
        {
            mp3_play_start(mp3_play_playlist[0].fullPath);
        }
    }
}

static MENU_UPDATE_FUNC(mp3_play_menu_status)
{
    if(mp3_play_audio_stop)
    {
        MENU_SET_VALUE("Stopping");
    }
    else if(mp3_play_audio_started)
    {
        MENU_SET_VALUE("Playing");
    }
    else
    {
        MENU_SET_VALUE("Idle");
    }
    MENU_SET_HELP("Playlist size: %d files", mp3_play_playlist_entries);
}

static MENU_UPDATE_FUNC(mp3_play_menu_vol_update)
{
    struct sound_ctx *ctx = mp3_state.sound_ctx;
    
    if(ctx)
    {
        MENU_SET_VALUE("%d %%", ctx->mixer.headphone_gain);
    }
    else
    {
        MENU_SET_VALUE("(stopped)");
    }
}

static MENU_UPDATE_FUNC(mp3_play_out_line_update)
{
    struct sound_ctx *ctx = mp3_state.sound_ctx;
    
    if(ctx)
    {
        enum sound_destination line = ctx->mixer.destination_line;
        const char *name = ctx->device->codec_ops.get_destination_name(line);
        
        MENU_SET_VALUE("%s (#%d)", name, line);
    }
    else
    {
        MENU_SET_VALUE("(stopped)");
    }
}

static MENU_SELECT_FUNC(mp3_play_menu_vol_select)
{
    struct sound_ctx *ctx = mp3_state.sound_ctx;
    
    if(ctx)
    {
        ctx->mixer.headphone_gain = COERCE((int)ctx->mixer.headphone_gain + delta, 0, 100);
        ctx->mixer.speaker_gain = ctx->mixer.headphone_gain;
        ctx->ops.apply(ctx);
    }
}

static MENU_SELECT_FUNC(mp3_play_out_line_select)
{
    struct sound_ctx *ctx = mp3_state.sound_ctx;
    
    if(ctx)
    {
        mp3_play_out_line = COERCE(mp3_play_out_line + delta, 0, SOUND_DESTINATION_EXTENDED_6);
        ctx->mixer.destination_line = (enum sound_destination)mp3_play_out_line;
        ctx->ops.apply(ctx);
    }
}

static struct menu_entry mp3_play_menu[] =
{
    {
        .name = "MP3 Player",
        .help = "Use file_man to select a MP3 file",
        .submenu_width = 710,
        .children = (struct menu_entry[])
        {
            {
                .name = "Status",
                .update = &mp3_play_menu_status,
                .help = "Current status",
                .icon_type = MNI_NONE,
            },
            {
                .name = "[|>] Play",
                .select = &mp3_play_menu_play,
                .help = "Start playback",
            },
            {
                .name = "[>>] Next",
                .select = &mp3_play_menu_next,
                .help = "Play next file",
            },
            {
                .name = "[<<] Prev",
                .select = &mp3_play_menu_prev,
                .help = "Play previous file",
            },
            {
                .name = "[ ]  Stop",
                .select = &mp3_play_menu_stop,
                .help = "Stop playback",
            },
            {
                .name = "Output volume",
                .update = &mp3_play_menu_vol_update,
                .select = &mp3_play_menu_vol_select,
                .help = "Audio output volume (0-100).",
            },
            {
                .name = "Output line",
                .priv = &mp3_play_out_line,
                .update = &mp3_play_out_line_update,
                .select = &mp3_play_out_line_select,
                .max = SOUND_DESTINATION_EXTENDED_6,
                .help = "Audio output line (0-n).",
            },
            MENU_EOL,
        },
    },
};

static unsigned int mp3_play_init()
{
    fileman_register_type("MP3", "MP3 Audio File", mp3_play_filehandler);
    mp3_play_playlist_queue = (struct msg_queue *) msg_queue_create("mp3_play_playlist_queue", 500);
    mp3_play_playlist_scan_queue = (struct msg_queue *) msg_queue_create("mp3_play_playlist_scan_queue", 500);

    menu_add("Games", mp3_play_menu, COUNT(mp3_play_menu));
    return 0;
}

static unsigned int mp3_play_deinit()
{
    menu_remove("Games", mp3_play_menu, COUNT(mp3_play_menu));
    return 0;
}


MODULE_INFO_START()
    MODULE_INIT(mp3_play_init)
    MODULE_DEINIT(mp3_play_deinit)
MODULE_INFO_END()

