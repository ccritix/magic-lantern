

#include <dryos.h>
#include <bmp.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <console.h>

#include "sound.h"
#include "ml26121.h"

/* cached registers */
static struct ml26121_cache_entry ml26121_cached_registers[ML26121_REGS];
static uint32_t ml26121_need_rewrite = 0;

extern void SetSamplingRate(uint32_t);

static const char *ml26121_src_names[] = { "Default", "Off", "Int.Mic", "Ext.Mic", "HDMI", "Auto", "L.int R.ext", "L.int R.bal" };
static const char *ml26121_dst_names[] = { "Default", "Off", "Off", "Speaker", "A/V", "HDMI", "Spk+LineOut", "Spk+A/V" };

struct codec_ops default_codec_ops =
{
    .apply_mixer = &ml26121_op_apply_mixer,
    .set_rate = &ml26121_op_set_rate,
    .poweron = &ml26121_op_poweron,
    .poweroff = &ml26121_op_poweroff,
    .get_source_name = &ml26121_op_get_source_name,
    .get_destination_name = &ml26121_op_get_destination_name,
};


static uint32_t ml26121_build_cmd(enum ml26121_regs reg, enum ml26121_op op)
{
    uint32_t cmd = 0;
    
    cmd |= (reg & 0xFF) << 8;
    
    if(op == ML26121_OP_WRITE)
    {
        cmd |= 0x0100;
    }
    
    return cmd;
}

/* refresh register in cache */
static void ml26121_cache(enum ml26121_regs reg)
{
    uint32_t cmd = ml26121_build_cmd(reg, ML26121_OP_READ);
    
    ml26121_cached_registers[reg].value = audio_ic_read(cmd);
    ml26121_cached_registers[reg].cached = 1;
    ml26121_cached_registers[reg].dirty = 0;
}

/* write register data from cache into chip */
static void ml26121_write_reg(enum ml26121_regs reg)
{
    uint32_t cmd = ml26121_build_cmd(reg, ML26121_OP_WRITE);
    cmd |= (ml26121_cached_registers[reg].value & 0xFF);
    
    audio_ic_write(cmd);
    ml26121_cached_registers[reg].dirty = 0;
}

static void ml26121_write(uint32_t reg, uint32_t value)
{
    /* udpate cache and mark dirty */
    ml26121_cached_registers[reg].value = value;
    ml26121_cached_registers[reg].dirty = 1;
}

static void ml26121_write_masked(uint32_t reg, uint32_t mask, uint32_t value)
{
    /* udpate cache and mark dirty */
    ml26121_cached_registers[reg].value &= ~mask;
    ml26121_cached_registers[reg].value |= value;
    ml26121_cached_registers[reg].dirty = 1;
}

static void ml26121_write_changed()
{
    for(uint32_t reg = 0; reg < COUNT(ml26121_cached_registers); reg++)
    {
        /* udpate cache and mark dirty */
        if(ml26121_cached_registers[reg].dirty)
        {
            ml26121_write_reg(reg);
        }
    }
}

static void ml26121_readall()
{
    for(uint32_t reg = 0; reg < COUNT(ml26121_cached_registers); reg++)
    {
        ml26121_cache(reg);
    }
}

static void ml26121_set_loop(uint32_t state)
{
    ml26121_write(ML26121_SPK_AMP_OUT, ML26121_SPK_AMP_OUT_PGA_SWITCH | ML26121_SPK_AMP_OUT_LOOP_SWITCH | ML26121_SPK_AMP_OUT_DAC_SWITCH);
    ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_MON, ML26121_RECPLAY_STATE_MON);
    ml26121_write_changed();
}

static void ml26121_unpower_mic()
{
}

static void ml26121_power_mic()
{
}

static void ml26121_unpower_out()
{
    ml26121_write(ML26121_HP_AMP_OUT_CTL, 0);
    ml26121_write(ML26121_SPK_AMP_OUT, 0);
    ml26121_write(ML26121_PW_SPAMP_PW_MNG, ML26121_PW_SPAMP_PW_MNG_OFF);
    ml26121_write_changed();
}

static void ml26121_power_speaker()
{
    /* speaker output */
    ml26121_write(ML26121_SPK_AMP_OUT, ML26121_SPK_AMP_OUT_PGA_SWITCH | ML26121_SPK_AMP_OUT_DAC_SWITCH);
    ml26121_write(ML26121_PW_AMP_VOL_FUNC, ML26121_PW_AMP_VOL_FUNC_ENA_FADE_ON | ML26121_PW_AMP_VOL_FUNC_ENA_AVMUTE);
    ml26121_write(ML26121_PW_AMP_VOL_FADE, ML26121_AMP_VOL_FADE_0);
    ml26121_write(ML26121_PW_SPAMP_PW_MNG, ML26121_PW_SPAMP_PW_MNG_P_ON);
    ml26121_write_changed();
    
    ml26121_write(ML26121_PW_SPAMP_PW_MNG, ML26121_PW_SPAMP_PW_MNG_ON);
    ml26121_write(ML26121_PW_AMP_VOL_FUNC, ML26121_PW_AMP_VOL_FUNC_ENA_FADE_ON);
    ml26121_write(ML26121_MIXER_VOL_CTL, ML26121_MIXER_VOL_CTL_LCH_USE_L_ONLY);
    ml26121_write_changed();
}

static void ml26121_power_lineout()
{
    ml26121_write(ML26121_HP_AMP_OUT_CTL, ML26121_HP_AMP_OUT_CTL_ALL_ON);
    ml26121_write(ML26121_PW_AMP_VOL_FUNC, ML26121_PW_AMP_VOL_FUNC_ENA_FADE_ON | ML26121_PW_AMP_VOL_FUNC_ENA_AVMUTE);
    ml26121_write(ML26121_PW_AMP_VOL_FADE, ML26121_AMP_VOL_FADE_0);
    ml26121_write_changed();
    
    ml26121_write(ML26121_PW_AMP_VOL_FUNC, ML26121_PW_AMP_VOL_FUNC_ENA_FADE_ON);
    ml26121_write(ML26121_MIXER_VOL_CTL, ML26121_MIXER_VOL_CTL_LCH_USE_LR);
    ml26121_write_changed();
}

static void ml26121_power_avline()
{
}

static void ml26121_set_in_vol(uint32_t volume)
{
}

static void ml26121_set_out_vol(uint32_t volume)
{
    ml26121_write(ML26121_PW_SPK_AMP_VOL, 0x0E + (volume * 0x31) / 100);
    ml26121_write(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_RECPLAY);
    ml26121_write_changed();
}

static void ml26121_set_lineout_vol(uint32_t volume)
{
    ml26121_write(ML26121_PW_HP_AMP_VOL, volume);
    ml26121_write(ML26121_PLYBAK_BOST_VOL, ML26121_PLYBAK_BOST_VOL_DEF);
    ml26121_write(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_RECPLAY);
    ml26121_write_changed();
}

static void ml26121_set_mic_gain(uint32_t gain)
{
    ml26121_write(ML26121_PW_MIC_IN_VOL, gain);
    ml26121_write(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_RECPLAY);
    ml26121_write_changed();
}

static const char *ml26121_op_get_destination_name(enum sound_destination line)
{
    if(line >= COUNT(ml26121_dst_names))
    {
        return "N/A";
    }
    
    return ml26121_dst_names[line];
}

static const char *ml26121_op_get_source_name(enum sound_source line)
{
    if(line >= COUNT(ml26121_src_names))
    {
        return "N/A";
    }
    
    return ml26121_src_names[line];
}

static enum sound_result ml26121_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next)
{
    /* described in pdf p71 */
    ml26121_write(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_STOP);
    
    if(prev->speaker_gain != next->speaker_gain || ml26121_need_rewrite)
    {
        ml26121_set_out_vol(COERCE(next->speaker_gain * 0x40 / 100, 0x0E, 0x3F));
    }
    
    if(prev->headphone_gain != next->headphone_gain || ml26121_need_rewrite)
    {
        ml26121_set_lineout_vol(COERCE(next->headphone_gain * 0x40 / 100, 0x0E, 0x3F));
    }
    
    if(prev->mic_gain != next->mic_gain || ml26121_need_rewrite)
    {
        ml26121_set_mic_gain(COERCE(next->mic_gain * 0x40 / 100, 0, 0x3F));
    }
    
    if(prev->loop_mode != next->loop_mode || ml26121_need_rewrite)
    {
        ml26121_set_loop(next->loop_mode);
    }
    
    if(prev->source_line != next->source_line || ml26121_need_rewrite)
    {
        switch(next->source_line)
        {
            default:
            case SOUND_SOURCE_OFF:
                ml26121_unpower_mic();
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_REC, 0);
                ml26121_write_changed();
                break;
                
            case SOUND_SOURCE_INT_MIC:
                ml26121_write(ML26121_RCH_MIXER_INPUT, ML26121_RCH_MIXER_INPUT_SINGLE_COLD);
                ml26121_write(ML26121_LCH_MIXER_INPUT, ML26121_LCH_MIXER_INPUT_SINGLE_COLD);
                ml26121_write(ML26121_RECORD_PATH, ML26121_RECORD_PATH_MICR2LCH_MICR2RCH);
                ml26121_write(ML26121_MIC_IF_CTL, ML26121_MIC_IF_CTL_ANALOG_SINGLE);
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_REC, ML26121_RECPLAY_STATE_REC);
                ml26121_write_changed();
                break;
            case SOUND_SOURCE_EXT_MIC:
                ml26121_write(ML26121_RCH_MIXER_INPUT, ML26121_RCH_MIXER_INPUT_SINGLE_HOT);
                ml26121_write(ML26121_LCH_MIXER_INPUT, ML26121_LCH_MIXER_INPUT_SINGLE_HOT);
                ml26121_write(ML26121_RECORD_PATH, ML26121_RECORD_PATH_MICL2LCH_MICR2RCH);
                ml26121_write(ML26121_MIC_IF_CTL, ML26121_MIC_IF_CTL_ANALOG_SINGLE);
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_REC, ML26121_RECPLAY_STATE_REC);
                ml26121_write_changed();
                break;
            case SOUND_SOURCE_EXTENDED_1: // L internal R external
                ml26121_write(ML26121_RCH_MIXER_INPUT, ML26121_RCH_MIXER_INPUT_SINGLE_HOT);
                ml26121_write(ML26121_LCH_MIXER_INPUT, ML26121_LCH_MIXER_INPUT_SINGLE_COLD);
                ml26121_write(ML26121_RECORD_PATH, ML26121_RECORD_PATH_MICL2LCH_MICR2RCH);
                ml26121_write(ML26121_MIC_IF_CTL, ML26121_MIC_IF_CTL_ANALOG_SINGLE);
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_REC, ML26121_RECPLAY_STATE_REC);
                ml26121_write_changed();
                break;
            case SOUND_SOURCE_EXTENDED_2: // L internal R balanced (used for test)
                ml26121_write(ML26121_RCH_MIXER_INPUT, ML26121_RCH_MIXER_INPUT_SINGLE_HOT);
                ml26121_write(ML26121_MIC_IF_CTL, ML26121_MIC_IF_CTL_ANALOG_DIFFER);
                ml26121_write(ML26121_LCH_MIXER_INPUT, ML26121_LCH_MIXER_INPUT_SINGLE_COLD);
                ml26121_write(ML26121_RECORD_PATH, ML26121_RECORD_PATH_MICL2LCH_MICR2RCH);
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_REC, ML26121_RECPLAY_STATE_REC);
                ml26121_write_changed();
                break;
        }
    }
    
    if(prev->destination_line != next->destination_line || ml26121_need_rewrite)
    {
        switch(next->destination_line)
        {
            default:
            case SOUND_DESTINATION_OFF:
                ml26121_unpower_out();
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_PLAY, 0);
                ml26121_write_changed();
                break;
            case SOUND_DESTINATION_SPK:
                ml26121_unpower_out();
                ml26121_power_speaker();
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_PLAY, ML26121_RECPLAY_STATE_PLAY);
                ml26121_write_changed();
                break;
            case SOUND_DESTINATION_LINE:
                ml26121_unpower_out();
                ml26121_power_lineout();
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_PLAY, ML26121_RECPLAY_STATE_PLAY);
                ml26121_write_changed();
                break;
            case SOUND_DESTINATION_AV:
                ml26121_unpower_out();
                ml26121_power_avline();
                ml26121_write_masked(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_PLAY, ML26121_RECPLAY_STATE_PLAY);
                ml26121_write_changed();
        }
    }
    
    ml26121_need_rewrite = 0;
    
    return SOUND_RESULT_OK;
}

static enum sound_result ml26121_op_set_rate(uint32_t rate)
{
    SetSamplingRate(rate);
    
    //ml26121_write(ML26121_SMPLING_RATE, ML26121_SMPLING_RATE_44kHz);
    //ml26121_write_changed();
    return SOUND_RESULT_OK;
}

static enum sound_result ml26121_op_poweron()
{
    ml26121_write(ML26121_PW_DAC_PW_MNG, ML26121_PW_DAC_PW_MNG_PWRON);
    ml26121_write(ML26121_DVOL_CTL, ML26121_DVOL_CTL_FUNC_EN_ALL);
    ml26121_write_changed();
    
    return SOUND_RESULT_OK;
}

static enum sound_result ml26121_op_poweroff()
{
    ml26121_write(ML26121_RECPLAY_STATE, ML26121_RECPLAY_STATE_STOP);
    ml26121_write_changed();
    
    ml26121_write(ML26121_PW_MIC_BOOST_VOL1, ML26121_MIC_BOOST_VOL1_OFF);
    ml26121_write(ML26121_PW_MIC_BOOST_VOL2, ML26121_MIC_BOOST_VOL2_OFF);
    ml26121_write(ML26121_PW_MIC_IN_VOL, ML26121_MIC_IN_VOL_2);
    ml26121_write(ML26121_PW_ZCCMP_PW_MNG, 0x00);
    ml26121_write(ML26121_PW_REF_PW_MNG, ML26121_PW_REF_PW_MNG_ALL_OFF);
    ml26121_write(ML26121_FILTER_EN, ML26121_FILTER_DIS_ALL);
    
    ml26121_write_changed();
    
    return SOUND_RESULT_OK;
}

void codec_init(struct codec_ops *ops)
{
    ml26121_readall();
    *ops = default_codec_ops;
}


