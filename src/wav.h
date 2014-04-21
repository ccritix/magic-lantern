#ifndef __wav_h__
#define __wav_h__


struct wav_data
{
    char *filename;
    FILE *handle;
    struct sound_ctx *sound;
    
    uint32_t processed;
    uint32_t buffer_num;
    uint32_t buffers;
    struct sound_buffer **buffer;
};

/* call wav_record_init to allocate all stuff and prepare recording. you can alter audio settings using ctx->sound field */
struct wav_data *wav_record_init(char *filename);
/* wav_record_start starts recording and saves data to the file specified upon init */
void wav_record_start(struct wav_data *ctx);
/* stop recording and free context */
void wav_record_stop(struct wav_data *ctx);

#endif