

#include <dryos.h>
#include <bmp.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <console.h>

#include "sound.h"
#include "ak4646.h"

/* cached registers */
static struct ak4646_cache_entry ak4646_cached_registers[AK4646_REGS];
static uint32_t ak4646_need_rewrite = 0;


static const char *ak4646_src_names[] = { "Default", "Off", "Int.Mic", "Ext.Mic", "HDMI", "Auto", "L.int R.ext", "L.int R.bal" };
static const char *ak4646_dst_names[] = { "Default", "Off", "Speaker", "Line Out", "A/V", "HDMI", "Spk+LineOut", "Spk+A/V" };

struct codec_ops default_codec_ops =
{
    .apply_mixer = &ak4646_op_apply_mixer,
    .set_rate = &ak4646_op_set_rate,
    .poweron = &ak4646_op_poweron,
    .poweroff = &ak4646_op_poweroff,
    .get_source_name = &ak4646_op_get_source_name,
    .get_destination_name = &ak4646_op_get_destination_name,
};

#if defined(CONFIG_AUDIO_IC_QUEUED)

void _audio_ic_write(unsigned cmd)
{
    extern void _audio_ic_write_bulk(uint32_t spell[]);
    uint32_t spell[] = { cmd << 16, 0xFFFFFFFF };
    _audio_ic_write_bulk(spell);
}

static uint32_t ak4646_build_cmd(enum ak4646_regs reg, enum ak4646_op op)
{
    if (op == AK4646_OP_READ)
        return(reg);

    return((reg & 0xFF) << 8);
}

#else

static uint32_t ak4646_build_cmd(enum ak4646_regs reg, enum ak4646_op op)
{
    uint32_t cmd = 0;
    
    cmd |= (reg & 0x1F) << 8;
    cmd |= (reg & 0x60) << 9;
    
    if(op == AK4646_OP_WRITE)
    {
        cmd |= (1<<13);
    }
    
    return cmd;
}

#endif

/* refresh register in cache */
static void ak4646_cache(enum ak4646_regs reg)
{
    uint32_t cmd = ak4646_build_cmd(reg, AK4646_OP_READ);
    
    ak4646_cached_registers[reg].value = audio_ic_read(cmd);
    ak4646_cached_registers[reg].cached = 1;
    ak4646_cached_registers[reg].dirty = 0;
}

/* write register data from cache into chip */
static void ak4646_write_reg(enum ak4646_regs reg)
{
    uint32_t cmd = ak4646_build_cmd(reg, AK4646_OP_WRITE);
    cmd |= (ak4646_cached_registers[reg].value & 0xFF);
    
    audio_ic_write(cmd);
    ak4646_cached_registers[reg].dirty = 0;
}

static void ak4646_write(struct ak4646_parameter parameter[], uint32_t value)
{
    uint32_t pos = 0;
    
    //console_printf("ak4646_write: %s=%d\n", parameter[0].name, value);
    
    /* read current register content into cache */
    while(parameter[pos].reg != AK4646_REG_END)
    {
        if(!ak4646_cached_registers[parameter[pos].reg].cached)
        {
            ak4646_cache(parameter[pos].reg);
        }
        pos++;
    }
    
    pos = 0;
    while(parameter[pos].reg != AK4646_REG_END)
    {
        /* build masks */
        uint32_t field_mask = (1 << parameter[pos].width) - 1;
        uint32_t reg_mask = field_mask << parameter[pos].pos;
        uint32_t reg_value = ((value >> parameter[pos].shift) & field_mask) << parameter[pos].pos;
        
        /* udpate cache and mark dirty */
        ak4646_cached_registers[parameter[pos].reg].value &= ~reg_mask;
        ak4646_cached_registers[parameter[pos].reg].value |= reg_value;
        ak4646_cached_registers[parameter[pos].reg].dirty = 1;
        
        pos++;
    }
}

static void ak4646_write_changed()
{
    for(uint32_t reg = 0; reg < COUNT(ak4646_cached_registers); reg++)
    {
        /* udpate cache and mark dirty */
        if(ak4646_cached_registers[reg].dirty)
        {
            ak4646_write_reg(reg);
        }
    }
}

static void ak4646_readall()
{
    for(uint32_t reg = 0; reg < COUNT(ak4646_cached_registers); reg++)
    {
        ak4646_cache(reg);
    }
}

static void ak4646_set_loop(uint32_t state)
{
    AK4646_SET(AK4646_PAR_LOOP, state);
    ak4646_write_changed();
}

static void ak4646_set_mic_pwr(uint32_t state)
{
    AK4646_SET(AK4646_PAR_PMMP, state);
    ak4646_write_changed();
}

static void ak4646_unpower_out()
{
    AK4646_SET(AK4646_PAR_PMSPK, 0);
    AK4646_SET(AK4646_PAR_PMDAC, 0);
    AK4646_SET(AK4646_PAR_LOPS, 1);
    AK4646_SET(AK4646_PAR_DACS, 0);
    AK4646_SET(AK4646_PAR_DACL, 0);
    AK4646_SET(AK4646_PAR_SPPSN, 0);
    AK4646_SET(AK4646_PAR_SPKG, 1);
    ak4646_write_changed();
}

static void ak4646_power_speaker()
{
    AK4646_SET(AK4646_PAR_DACS, 1);
    AK4646_SET(AK4646_PAR_SPPSN, 0);
    AK4646_SET(AK4646_PAR_SPKG, 1);
    AK4646_SET(AK4646_PAR_PMSPK, 1);
    AK4646_SET(AK4646_PAR_PMDAC, 1);
    ak4646_write_changed();
    
    msleep(50);
    
    AK4646_SET(AK4646_PAR_SPPSN, 1);
    ak4646_write_changed();
}

/* used internally for headphone and AV line too */
static void ak4646_power_line()
{
    AK4646_SET(AK4646_PAR_DACL, 1);
    AK4646_SET(AK4646_PAR_LOPS, 1);
    AK4646_SET(AK4646_PAR_PMLO, 1);
    AK4646_SET(AK4646_PAR_PMDAC, 1);
    ak4646_write_changed();
    
    msleep(10);
    
    AK4646_SET(AK4646_PAR_LOPS, 0);
    ak4646_write_changed();
}

static void ak4646_power_lineout()
{
    ak4646_power_line();
    
#if defined(CONFIG_5D3)
    /* 5D3 uses this line to select headphone/AV */
    MEM(0xC0220160) |= 2;
#endif
}

static void ak4646_power_avline()
{
    ak4646_power_line();
    
#if defined(CONFIG_5D3)
    /* 5D3 uses this line to select headphone/AV */
    MEM(0xC0220160) &= ~2;
#endif
}


static void ak4646_set_in_vol(uint32_t volume)
{
    AK4646_SET(AK4646_PAR_IVL, volume);
    AK4646_SET(AK4646_PAR_IVR, volume);
    ak4646_write_changed();
}

static void ak4646_set_out_vol(uint32_t volume)
{
    AK4646_SET(AK4646_PAR_OVL, volume);
    AK4646_SET(AK4646_PAR_OVR, volume);
    ak4646_write_changed();
}

static void ak4646_set_lineout_vol(uint32_t volume)
{
    AK4646_SET(AK4646_PAR_LOVL, volume);
    ak4646_write_changed();
}

static void ak4646_set_mic_gain(uint32_t gain)
{
    AK4646_SET(AK4646_PAR_MGAIN, gain);
    ak4646_write_changed();
}

static const char *ak4646_op_get_destination_name(enum sound_destination line)
{
    if(line >= COUNT(ak4646_dst_names))
    {
        return "N/A";
    }
    
    return ak4646_dst_names[line];
}

static const char *ak4646_op_get_source_name(enum sound_source line)
{
    if(line >= COUNT(ak4646_src_names))
    {
        return "N/A";
    }
    
    return ak4646_src_names[line];
}

static enum sound_result ak4646_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next)
{
    if(prev->speaker_gain != next->speaker_gain || ak4646_need_rewrite)
    {
        ak4646_set_out_vol(COERCE(next->speaker_gain * 0xF1 / 100, 0, 0xF1));
    }
    
    if(prev->headphone_gain != next->headphone_gain || ak4646_need_rewrite)
    {
        ak4646_set_lineout_vol(COERCE(next->headphone_gain * 3 / 100, 0, 3));
    }
    
    if(prev->mic_gain != next->mic_gain || ak4646_need_rewrite)
    {
        ak4646_set_mic_gain(COERCE(next->mic_gain * 7 / 100, 0, 7));
    }
    
    if(prev->mic_power != next->mic_power || ak4646_need_rewrite)
    {
        ak4646_set_mic_pwr(next->mic_power == SOUND_POWER_ENABLED);
#if 0
        /* these are from canon firmware, what do they do? seem to have no effect */
        MEM(0xC0220188) |= 2; /* ext not selected */
        MEM(0xC0220188) &= ~2; /* ext selected */
#endif
    }
    
    if(prev->loop_mode != next->loop_mode || ak4646_need_rewrite)
    {
        ak4646_set_loop(next->loop_mode == SOUND_LOOP_ENABLED);
    }
    
    /* only update mic settings when playback is disabled */
    if((prev->source_line != next->source_line || ak4646_need_rewrite))
    {
        switch(next->source_line)
        {
            default:
            case SOUND_SOURCE_OFF:
                AK4646_SET(AK4646_PAR_PMADL, 0);
                AK4646_SET(AK4646_PAR_PMADR, 0);
                break;
                
            case SOUND_SOURCE_INT_MIC:
                AK4646_SET(AK4646_PAR_PMADL, 1);
                AK4646_SET(AK4646_PAR_PMADR, 1);
                AK4646_SET(AK4646_PAR_MDIF1, 0);
                AK4646_SET(AK4646_PAR_MDIF2, 0);
                AK4646_SET(AK4646_PAR_INL, 0);
                AK4646_SET(AK4646_PAR_INR, 0);
                ak4646_write_changed();
                break;
                
            case SOUND_SOURCE_EXT_MIC:
                AK4646_SET(AK4646_PAR_PMADL, 1);
                AK4646_SET(AK4646_PAR_PMADR, 1);
                AK4646_SET(AK4646_PAR_MDIF1, 0);
                AK4646_SET(AK4646_PAR_MDIF2, 0);
                AK4646_SET(AK4646_PAR_INL, 1);
                AK4646_SET(AK4646_PAR_INR, 1);
                ak4646_write_changed();
                break;
                
            case SOUND_SOURCE_EXTENDED_1:
                AK4646_SET(AK4646_PAR_PMADL, 1);
                AK4646_SET(AK4646_PAR_PMADR, 1);
                AK4646_SET(AK4646_PAR_MDIF1, 0);
                AK4646_SET(AK4646_PAR_MDIF2, 0);
                AK4646_SET(AK4646_PAR_INL, 0);
                AK4646_SET(AK4646_PAR_INR, 1);
                ak4646_write_changed();
                break;
                
            case SOUND_SOURCE_EXTENDED_2:
                AK4646_SET(AK4646_PAR_PMADL, 1);
                AK4646_SET(AK4646_PAR_PMADR, 1);
                AK4646_SET(AK4646_PAR_MDIF1, 0);
                AK4646_SET(AK4646_PAR_MDIF2, 1);
                AK4646_SET(AK4646_PAR_INL, 0);
                AK4646_SET(AK4646_PAR_INR, 0);
                ak4646_write_changed();
                break;
        }
    }
    
    if(prev->destination_line != next->destination_line || ak4646_need_rewrite)
    {
        switch(next->destination_line)
        {
            default:
            case SOUND_DESTINATION_OFF:
                ak4646_unpower_out();
                break;
                
            case SOUND_DESTINATION_SPK:
                ak4646_unpower_out();
                ak4646_power_speaker();
                break;
            case SOUND_DESTINATION_LINE:
                ak4646_unpower_out();
                ak4646_power_lineout();
                break;
            case SOUND_DESTINATION_AV:
                ak4646_unpower_out();
                ak4646_power_avline();
                break;
            case SOUND_DESTINATION_EXTENDED_1:
                ak4646_power_speaker();
                ak4646_power_lineout();
                break;
            case SOUND_DESTINATION_EXTENDED_2:
                ak4646_power_speaker();
                ak4646_power_avline();
                break;
        }
    }
    
    ak4646_need_rewrite = 0;
    
    return SOUND_RESULT_OK;
}

static enum sound_result ak4646_op_set_rate(uint32_t rate)
{
    const uint32_t ak4646_rates[] = { 8000, 12000, 16000, 24000, 7350, 11025, 14700, 22050, 0, 0, 32000, 48000, 0, 0, 29400, 44100 };
    
    for(uint32_t mode = 0; mode < 16; mode++)
    {
        if(ak4646_rates[mode] == rate)
        {
            AK4646_SET(AK4646_PAR_FS, mode);
            ak4646_write_changed();
        }
    }
    return SOUND_RESULT_OK;
}

static enum sound_result ak4646_op_poweron()
{
    ak4646_need_rewrite = 1;
    AK4646_SET(AK4646_PAR_PMVCM, 1);
    AK4646_SET(AK4646_PAR_DAFIL, 0);
    ak4646_write_changed();
    
    return SOUND_RESULT_OK;
}

static enum sound_result ak4646_op_poweroff()
{
    AK4646_SET(AK4646_PAR_PMBP, 0);
    AK4646_SET(AK4646_PAR_PMMP, 0);
    AK4646_SET(AK4646_PAR_PMDAC, 0);
    AK4646_SET(AK4646_PAR_PMLO, 0);
    AK4646_SET(AK4646_PAR_PMADL, 0);
    AK4646_SET(AK4646_PAR_PMADR, 0);
    AK4646_SET(AK4646_PAR_PMSPK, 0);
    ak4646_write_changed();
    
    AK4646_SET(AK4646_PAR_PMVCM, 0);
    ak4646_write_changed();
    
    ak4646_need_rewrite = 1;
    
    return SOUND_RESULT_OK;
}


void codec_init(struct codec_ops *ops)
{
    ak4646_readall();
    *ops = default_codec_ops;
}


