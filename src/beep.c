
#include "dryos.h"
#include "menu.h"
#include "bmp.h"
#include "math.h"
#include "config.h"
#define _beep_c_
#include "property.h"

#include "sound.h"
#include "beep.h"


volatile int beep_playing = 0;

#ifdef CONFIG_BEEP


static struct sound_ctx *beep_ctx = NULL;
static struct msg_queue *beep_queue = NULL;

CONFIG_INT("beep.enabled", beep_enabled, 1);
static CONFIG_INT("beep.out_line", beep_out_line, SOUND_DESTINATION_SPK);
static CONFIG_INT("beep.volume", beep_volume, 3);
static CONFIG_INT("beep.freq.idx", beep_freq_idx, 11); // 1 KHz
static CONFIG_INT("beep.wavetype", beep_wavetype, 0); // square, sine, white noise

static int beep_freq_values[] = {55, 110, 220, 262, 294, 330, 349, 392, 440, 494, 880, 1000, 1760, 2000, 3520, 5000, 12000};


static int beep_is_possible()
{
    /* no restrictions? sound system should take care. */
    return 1;
}

static void beep_generate(int16_t* buf, uint32_t samples, uint32_t freq, uint32_t rate, enum wave_type wavetype)
{
    const uint32_t scaling = 16384;

    switch(wavetype)
    {
        case BEEP_WAVEFORM_SQUARE: // square
        {
            uint32_t factor = rate / freq / 2;
            
            for(uint32_t i = 0; i < samples; i++)
            {
                buf[i] = ((i/factor) % 2) ? 32767 : -32767;
            }
            break;
        }
        
        case BEEP_WAVEFORM_SINE: // sine
        {
            float factor = roundf(2.0f * M_PI * scaling / (float)rate * (float)freq);
            uint32_t mod_ang = (uint32_t)(2.0f * M_PI * scaling);
            
            for(uint32_t i = 0; i < samples; i++)
            {
                uint32_t t = (uint32_t)(factor * i);
                uint32_t ang = t % mod_ang;
                int32_t s = sinf((float)ang / (float)scaling) * scaling;

                buf[i] = COERCE(s*2, -32767, 32767);
            }
            break;
        }
        
        case BEEP_WAVEFORM_NOISE: // white noise
        {
            for(uint32_t i = 0; i < samples; i++)
            {
                buf[i] = rand();
            }
            break;
        }
    }
}

void beep_advanced(uint32_t duration, uint32_t frequency, uint32_t wait, uint32_t times, struct sound_mixer *mixer_settings)
{
    volatile uint32_t wait_local = wait;
    struct beep_msg *msg = malloc(sizeof(struct beep_msg));
    
    msg->type = BEEP_CUSTOM;
    msg->frequency = frequency;
    msg->waveform = BEEP_WAVEFORM_SQUARE;
    msg->duration = duration;
    msg->times = times;
    msg->wait = (uint32_t *)&wait_local;
    msg->mixer_settings = mixer_settings;
    
    msg_queue_post(beep_queue, (uint32_t)msg);
    
    /* wait until tone is played */
    while(wait_local)
    {
        msleep(50);
    }
}

void beep_custom(uint32_t duration, uint32_t frequency, uint32_t wait)
{
    beep_advanced(duration, frequency, wait, 1, NULL);
}

void beep_times(uint32_t times)
{
    beep_advanced(200, 1000, 0, times, NULL);
}

void beep()
{
    beep_advanced(200, 1000, 0, 1, NULL);
}

void beep_unsafe()
{
    beep_advanced(200, 1000, 0, 1, NULL);
}

enum sound_flow beep_cbr (struct sound_buffer *buffer)
{
    beep_playing = 0;
    return SOUND_FLOW_STOP;
}

void beep_play(int16_t *buf, int samples)
{
    struct sound_buffer *snd_buf = sound_alloc_buffer();
    
    snd_buf->size = samples * 2;
    snd_buf->data = buf;
    snd_buf->processed = &beep_cbr;
    snd_buf->cleanup = &beep_cbr;
    snd_buf->requeued = &beep_cbr;
    
    beep_playing = 1;
    beep_ctx->ops.lock(beep_ctx, SOUND_LOCK_PRIORITY);
    beep_ctx->ops.enqueue(beep_ctx, snd_buf);
    beep_ctx->ops.enqueue(beep_ctx, snd_buf);
    beep_ctx->ops.start(beep_ctx);
    
    while(beep_playing)
    {
        msleep(50);
    }
    
    beep_ctx->ops.stop(beep_ctx);
    beep_ctx->ops.unlock(beep_ctx);
    
    free(snd_buf);
}

static void beep_task()
{
    TASK_LOOP
    {
        struct beep_msg *msg = NULL;
        
        if(msg_queue_receive(beep_queue, &msg, 500))
        {
            continue;
        }
        
        if(!beep_enabled || !beep_is_possible())
        {
            free(msg);
            info_led_blink(1, 100, 10); // silent warning
            return;
        }
        
        switch(msg->type)
        {
            case BEEP_WAV:
                /* do we need that? playing a *ding* wave? */
                break;
                
            case BEEP_CUSTOM:
            {
                uint32_t samples = msg->duration * beep_ctx->format.rate / 1000;
                int16_t *beep_buf = malloc(samples * 2);
                if(!beep_buf)
                {
                    break;
                }
                beep_generate(beep_buf, samples, msg->frequency, beep_ctx->format.rate, msg->waveform);
                
                /* apply volume setting */
                beep_ctx->mixer.speaker_gain = 10 * beep_volume;
                beep_ctx->mixer.headphone_gain = 10 * beep_volume;
                beep_ctx->mixer.destination_line = (enum sound_destination)beep_out_line;
                
                /* backup old mixer settings and apply custom ones if necessary */
                struct sound_mixer old_mixer = beep_ctx->mixer;
                if(msg->mixer_settings)
                {
                    beep_ctx->mixer = *msg->mixer_settings;
                }
                
                for(uint32_t loop = 0; loop < msg->times; loop++)
                {
                    beep_play(beep_buf, samples);
                    msleep(20);
                }
                free(beep_buf);
                
                /* restore old settings */
                beep_ctx->mixer = old_mixer;
                break;
            }
        }
        
        /* poor man's semaphore, needs no alloc/free of semaphores */
        if(msg->wait)
        {
            *msg->wait = 0;
        }
        
        free(msg);
    }
}

static void play_test_tone()
{
    struct beep_msg *msg = malloc(sizeof(struct beep_msg));
    
    msg->type = BEEP_CUSTOM;
    msg->frequency = beep_freq_values[beep_freq_idx];
    msg->duration = 5000;
    msg->times = 1;
    msg->wait = 0;
    msg->waveform = beep_wavetype;
    msg->mixer_settings = NULL;
    
    msg_queue_post(beep_queue, (uint32_t)msg);
}

static MENU_UPDATE_FUNC(beep_update)
{
    MENU_SET_ENABLED(beep_enabled);
    
    if (!beep_is_possible())
    {
        MENU_SET_WARNING(MENU_WARN_NOT_WORKING, "Beeps disabled to prevent interference with audio recording.");
    }
}

static MENU_UPDATE_FUNC(beep_out_line_update)
{
    const char *name = beep_ctx->device->codec_ops.get_destination_name(beep_out_line);
    
    MENU_SET_VALUE("%s (#%d)", name, beep_out_line);
}

static MENU_SELECT_FUNC(beep_out_line_select)
{
    beep_out_line = COERCE(beep_out_line + delta, 0, SOUND_DESTINATION_EXTENDED_6);
}

static struct menu_entry beep_menus[] = {
    {
        .name = "Beep Volume",
        .priv = &beep_volume,
        .min = 1,
        .max = 10,
        .icon_type = IT_PERCENT,
        .help = "Volume for ML beeps (1-10).",
    },
    {
        .name = "Beep output line",
        .priv = &beep_out_line,
        .update = &beep_out_line_update,
        .select = &beep_out_line_select,
        .max = SOUND_DESTINATION_EXTENDED_6,
        .help = "Output line for beeps (0-n).",
    },
    {
        .name = "Beep, test tones",
        .select = menu_open_submenu,
        .update = beep_update,
        .submenu_width = 680,
        .help = "Configure ML beeps and play test tones (440Hz, 1kHz...)",
        .children =  (struct menu_entry[]) {
            {
                .name = "Enable Beeps",
                .priv = &beep_enabled,
                .max = 1,
                .help = "Enable beep signal for misc events in ML.",
            },
            {
                .name = "Tone Waveform", 
                .priv = &beep_wavetype,
                .max = 2,
                .choices = (const char *[]) {"Square", "Sine", "White Noise"},
                .help = "Type of waveform to be generated: square, sine, white noise.",
            },
            {
                .name = "Tone Frequency",
                .priv = &beep_freq_idx,
                .max = 16,
                .icon_type = IT_PERCENT,
                .choices = (const char *[]) {"55 Hz (A1)", "110 Hz (A2)", "220 Hz (A3)", "262 Hz (Do)", "294 Hz (Re)", "330 Hz (Mi)", "349 Hz (Fa)", "392 Hz (Sol)", "440 Hz (La-A4)", "494 Hz (Si)", "880 Hz (A5)", "1 kHz", "1760 Hz (A6)", "2 kHz", "3520 Hz (A7)", "5 kHz", "12 kHz"},
                .help = "Frequency for ML beep and test tones (Hz).",
            },
            {
                .name = "Play test tone",
                .icon_type = IT_ACTION,
                .select = play_test_tone,
                .help = "Play a 5-second test tone with current settings.",
            },
            {
                .name = "Test beep sound",
                .icon_type = IT_ACTION,
                .select = beep,
                .help = "Play a short beep which will be used for ML events.",
            },
            MENU_EOL,
        }
    },
};


static void beep_init()
{
    beep_ctx = sound_alloc();
    
    beep_ctx->mode = SOUND_MODE_PLAYBACK;
    beep_ctx->format.rate = 48000;
    beep_ctx->format.channels = 1;
    beep_ctx->format.sampletype = SOUND_SAMPLETYPE_SINT16;
    
    /* make this configurable? */
    beep_ctx->mixer.speaker_gain = 30;
    beep_ctx->mixer.headphone_gain = 30;
    beep_ctx->mixer.destination_line = SOUND_DESTINATION_SPK;
    
    beep_queue = (struct msg_queue *) msg_queue_create("beep_queue", 2);
    menu_add("Audio", beep_menus, COUNT(beep_menus) );
}

INIT_FUNC("beep.init", beep_init);
TASK_CREATE("beep_task", beep_task, 0, 0x18, 0x1000 );


#else // beep not working, keep dummy stubs

void beep(){}
void beep_times(uint32_t times){};
int beep_enabled = 0;
void beep_custom(uint32_t duration, uint32_t frequency, uint32_t wait) {}

#endif

