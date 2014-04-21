#ifndef _beep_h_
#define _beep_h_

enum beep_type
{
    BEEP_CUSTOM = 0,
    BEEP_WAV = 1,
};

enum wave_type
{
    BEEP_WAVEFORM_SQUARE,
    BEEP_WAVEFORM_SINE,
    BEEP_WAVEFORM_NOISE
};

struct beep_msg
{
    enum beep_type type;
    enum wave_type waveform;
    
    /* tone frequency */
    uint32_t frequency;
    /* how long one tone is played */
    uint32_t duration;
    /* number of repetitions (0 = one beep, 1 = two beeps) */
    uint32_t times;
    /* sleep time between two beeps */
    uint32_t sleep;
    
    /* internal signalling that beep has finished */
    uint32_t *wait;
};


/* beep playing */
void beep();
void beep_custom(uint32_t duration, uint32_t frequency, uint32_t wait);
void beep_times(uint32_t times);
void unsafe_beep(); /* also beeps while recording */

#endif

