
#ifndef __sound_h__
#define __sound_h__

/* lets play with enums but in macro style, it will just add type safeness without the downside of namespace pollution */
enum sound_flow
{
    SOUND_FLOW_CONTINUE = 0,	/* continue recording/playback */
    SOUND_FLOW_STOP     = 1,	/* stop recording/playback */
};

struct sound_buffer
{
    /* data content depends on selected audio mode. buffer was allocated by queuer */
    void *data;
    /* size of data content in bytes */
    uint32_t size;
    /* filled by enqueue function */
    uint32_t sequence;
    /* put any data there */
    void *priv;
    /* when playback/recording is done, this cbr is called to clean up contents */
    enum sound_flow (*processed) (struct sound_buffer *buffer);
    /* this buffer has been played/recorded but is being requeued as there is no new block available */
    enum sound_flow (*requeued) (struct sound_buffer *buffer);
    /* buffer has not been played/recorded and will be freed */
    enum sound_flow (*cleanup) (struct sound_buffer *buffer);
};

enum sound_sampletype
{
    SOUND_SAMPLETYPE_UINT8  = 0,
    SOUND_SAMPLETYPE_SINT16 = 1,
};

struct sound_format
{
    /* standard audio settings */
    uint32_t rate;
    uint32_t channels;
    enum sound_sampletype sampletype;
};

/* possible parameters for mixer settings. default means 'i dont care/dont want to touch/choose what is currently set' */
enum sound_windcut
{
    SOUND_WINDCUT_DEFAULT = 0,
    SOUND_WINDCUT_ON      = 1,
    SOUND_WINDCUT_OFF     = 2
};

enum sound_source
{
    SOUND_SOURCE_DEFAULT    = 0,
    SOUND_SOURCE_OFF        = 1,
    SOUND_SOURCE_INT_MIC    = 2,
    SOUND_SOURCE_EXT_MIC    = 3,
    SOUND_SOURCE_HDMI       = 4,
    SOUND_SOURCE_AUTO       = 5,
    SOUND_SOURCE_EXTENDED_1 = 6,
    SOUND_SOURCE_EXTENDED_2 = 7,
    SOUND_SOURCE_EXTENDED_3 = 8,
    SOUND_SOURCE_EXTENDED_4 = 9,
    SOUND_SOURCE_EXTENDED_5 = 10,
    SOUND_SOURCE_EXTENDED_6 = 11
};

enum sound_destination
{
    SOUND_DESTINATION_DEFAULT     = 0,
    SOUND_DESTINATION_OFF         = 1,
    SOUND_DESTINATION_SPK         = 2,
    SOUND_DESTINATION_LINE        = 3,
    SOUND_DESTINATION_AV          = 4,
    SOUND_DESTINATION_HDMI        = 5,
    SOUND_DESTINATION_EXTENDED_1  = 6,
    SOUND_DESTINATION_EXTENDED_2  = 7,
    SOUND_DESTINATION_EXTENDED_3  = 8,
    SOUND_DESTINATION_EXTENDED_4  = 9,
    SOUND_DESTINATION_EXTENDED_5  = 10,
    SOUND_DESTINATION_EXTENDED_6  = 11
};

enum agc_status
{
    SOUND_AGC_DEFAULT  = 0,
    SOUND_AGC_DISABLED = 1,
    SOUND_AGC_ENABLED  = 2
};

enum power_status
{
    SOUND_POWER_DEFAULT  = 0,
    SOUND_POWER_DISABLED = 1,
    SOUND_POWER_ENABLED  = 2
};
enum loop_status
{
    SOUND_LOOP_DEFAULT  = 0,
    SOUND_LOOP_DISABLED = 1,
    SOUND_LOOP_ENABLED  = 2
};

#define SOUND_GAIN_DISABLED 0xFFFFFFFF
#define SOUND_GAIN_DEFAULT  0

struct sound_mixer
{
    /* advanced settings, camera dependent */
    enum sound_windcut windcut_mode;
    
    /* those can be made more selective for left and right channels, depending on audio chip */
    enum sound_source source_line;
    enum sound_destination destination_line;
    
    enum loop_status loop_mode;
    
    /* amp in db, range depends on audio chip */
    enum agc_status headphone_agc;
    enum agc_status mic_agc;
    
    /* all gains are 0-100, scaling t.b.d */
    uint32_t headphone_gain;
    uint32_t mic_gain;
    uint32_t speaker_gain;
    /* output amp, also depends on audio chip if that is available */
    uint32_t out_gain;
    
    /* to enable/disable power supply for microphones */
    enum power_status mic_power;
};

enum sound_mode
{
    SOUND_MODE_PLAYBACK = 0,
    SOUND_MODE_RECORD   = 1
};

enum sound_state
{
    SOUND_STATE_IDLE      = 0, /* nothing done yet */
    SOUND_STATE_STARTED   = 1, /* currently in startup, waiting for buffers to fill/play */
    SOUND_STATE_RUNNING   = 2, /* ASIF has been started and is running */
    SOUND_STATE_STOPPING  = 3, /* stop requested, shut down ASIF ASAP */
    SOUND_STATE_CLEANUP   = 4  /* ASIF has finished, cleanup remaining stuff */
};

enum sound_lock
{
    SOUND_LOCK_UNLOCKED     = 0,
    SOUND_LOCK_NONEXCLUSIVE = 1,
    SOUND_LOCK_EXCLUSIVE    = 2,
    SOUND_LOCK_PRIORITY     = 3
};

enum sound_result
{
    SOUND_RESULT_OK       = 0,
    SOUND_RESULT_ERROR    = 1,
    SOUND_RESULT_TRYAGAIN = 2
};

/* declare it before use */
struct sound_ctx;

struct sound_ops
{
    /* init audio settings, setting all format and mixer settings */
    enum sound_result (*apply) (struct sound_ctx *ctx);

    /* reserve audio device or free up a reservation. any operation below requires it to be done on a locked state */
    enum sound_result (*lock) (struct sound_ctx *ctx, enum sound_lock type);
    enum sound_result (*unlock) (struct sound_ctx *ctx);

    /* start audio processing, this will set up ASIF and the next queued buffers will trigger playback/recording */
    enum sound_result (*start) (struct sound_ctx *ctx);

    /* enqueue a buffer to play/fill. you will have to queue two buffers before the action starts) */
    enum sound_result (*enqueue) (struct sound_ctx *ctx, struct sound_buffer *buffer);

    /* flush all buffers in queue */
    enum sound_result (*flush) (struct sound_ctx *ctx);

    /* this will stop playback/recording */
    enum sound_result (*pause) (struct sound_ctx *ctx);
    enum sound_result (*stop) (struct sound_ctx *ctx);
};

struct codec_ops
{
    /* we need some description here */
    enum sound_result (*apply_mixer) (struct sound_mixer *prev, struct sound_mixer *next);
    enum sound_result (*set_rate) (uint32_t rate);
    enum sound_result (*poweron) ();
    enum sound_result (*poweroff) ();
    const char *(*get_source_name) (enum sound_source line);
    const char *(*get_destination_name) (enum sound_destination line);
};

struct sound_ctx
{
    /* settings to apply when calling apply() */
    enum sound_mode mode;
    struct sound_format format;
    struct sound_mixer mixer;

    /* pointer to the device instance this context is from */
    struct sound_dev *device;
    struct msg_queue *buffer_queue;
    uint32_t buffers_played;
    
    /* when locking with higher priority, the interrupted ctx is stored here */
    struct sound_ctx *previous_ctx;
    enum sound_lock previous_lock;
    enum sound_state previous_state;
    
    uint32_t paused;

    /* if audio device was lost, this cbr is being called. device will be in state 'SOUND_STATE_CLEANUP' */
    void (*device_lost_cbr) (struct sound_ctx *ctx);

    struct sound_ops ops;
};

struct sound_dev
{
    /* the device's current lock state, must match to 'current_ctx' content */
    enum sound_lock lock_type;

    /* if a caller locked the device, here is the current context */
    struct sound_ctx *current_ctx;

    /* the device's operational state */
    enum sound_state state;
    
    /* user configured default mixer settings */
    struct sound_mixer mixer_defaults;
    
    /* the final resulting mixer settings, depending on current active sound playback/recording ctx */
    struct sound_mixer mixer_current;

    /* the buffers being filled/played next */
    struct sound_buffer *current_buffer;
    struct sound_buffer *next_buffer;

    struct msg_queue *wdg_queue;
    uint32_t max_queue_size;
    
    struct codec_ops codec_ops;
};

/* allocate a context for the default device */
struct sound_ctx *sound_alloc();
struct sound_buffer *sound_alloc_buffer();

/* free a previously allocated context */
void sound_free(struct sound_ctx *ctx);
void sound_init();

/* helper to call cleanup function of a buffer */
static enum sound_flow sound_buf_cleanup(struct sound_buffer *buf);
/* helper to call requeued function of a buffer */
static enum sound_flow sound_buf_requeued(struct sound_buffer *buf);
/* helper to call processed function of a buffer */
static enum sound_flow sound_buf_processed(struct sound_buffer *buf);
/* stop ASIF DMA transfer */
static void sound_stop_asif(struct sound_ctx *ctx);


static enum sound_result sound_op_lock (struct sound_ctx *ctx, enum sound_lock type);
static enum sound_result sound_op_unlock (struct sound_ctx *ctx);
static enum sound_result sound_op_apply (struct sound_ctx *ctx);
static enum sound_result sound_op_start (struct sound_ctx *ctx);
static enum sound_result sound_op_enqueue (struct sound_ctx *ctx, struct sound_buffer *buffer);
static enum sound_result sound_op_flush (struct sound_ctx *ctx);
static enum sound_result sound_op_pause (struct sound_ctx *ctx);
static enum sound_result sound_op_stop (struct sound_ctx *ctx);

#endif

