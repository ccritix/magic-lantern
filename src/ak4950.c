
#include <dryos.h>
#include <bmp.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <console.h>
#include <property.h>
#include <sound.h>
#include <ak4950.h>

/* cached registers */
static struct ak4950_cache_entry ak4950_cached_registers[AK4950_REGS];
static uint32_t ak4950_need_rewrite = 0;

static const char *ak4950_src_names[] = { "Default", "Off", "Int.Mic", "Ext.Mic", "HDMI", "Auto", "L.int R.ext" };
static const char *ak4950_dst_names[] = { "Default", "Off", "Speaker", "Line Out", "A/V", "HDMI", "Spk+LineOut", "Spk+A/V" };

extern uint32_t sound_trace_ctx;

struct codec_ops default_codec_ops =
{
    .apply_mixer = &ak4950_op_apply_mixer,
    .set_rate = &ak4950_op_set_rate,
    .poweron = &ak4950_op_poweron,
    .poweroff = &ak4950_op_poweroff,
    .get_source_name = &ak4950_op_get_source_name,
    .get_destination_name = &ak4950_op_get_destination_name,
};

static int ak4950_is_valid_reg(enum ak4950_regs reg)
{
    return ( 
            // reg >= 0x00 && // Warning: comparison is always true due to limited range of data type
            !( reg >= 0x10 && reg < 0x20) &&
            !( reg >= 0x28 && reg < 0x2A) &&
            reg < AK4950_REG_END
           );
}

static uint32_t ak4950_build_cmd(enum ak4950_regs reg, enum ak4950_op op)
{
    if (op == AK4950_OP_READ)
        return(reg);

    return ((reg & 0xFF) << 0x18);
}

/* refresh register in cache */
static void ak4950_cache(enum ak4950_regs reg)
{
    if(!ak4950_is_valid_reg(reg))
    {
        trace_write(sound_trace_ctx, "ak4950_cache: invalid register %d", reg);
        return;
    }

    uint32_t cmd = ak4950_build_cmd(reg, AK4950_OP_READ);

    ak4950_cached_registers[reg].value = audio_ic_read(cmd);
    ak4950_cached_registers[reg].cached = 1;
    ak4950_cached_registers[reg].dirty = 0;

    trace_write(sound_trace_ctx, "ak4950_cache: OP 0x%x RES 0x%x", cmd, ak4950_cached_registers[reg].value);
}

void _audio_ic_write(unsigned cmd) 
{
    extern void _audio_ic_write_bulk(uint32_t *spell);
    uint32_t spell[2];

    spell[0] = cmd;
    spell[1] = 0xFFFFFFFF;

    _audio_ic_write_bulk(spell);
}

/* write register data from cache into chip */
static void ak4950_write_reg(enum ak4950_regs reg)
{
    if(!ak4950_is_valid_reg(reg))
    {
        return;
    }

    uint32_t cmd;
    uint32_t val = ak4950_cached_registers[reg].value; 
    cmd  = ak4950_build_cmd(reg, AK4950_OP_WRITE); 
    cmd |= (val & (0xFF << 0x10)); // Mask out bits D16-D23, assumes the register value is at least 24bit
    trace_write(sound_trace_ctx, "ak4950_write_reg: reg: 0x%x val: 0x%x -> OP 0x%08x", reg, val, cmd);
    audio_ic_write(cmd);
    ak4950_cached_registers[reg].dirty = 0;
}

static void ak4950_write(struct ak4950_parameter parameter[], uint32_t value)
{
    uint32_t pos = 0;

    trace_write(sound_trace_ctx, "ak4950_write: %s=%d", parameter[0].name, value);

    /* read current register content into cache */
    trace_write(sound_trace_ctx, "ak4950_write: ~~~ cache start ~~~");
    while(parameter[pos].reg != AK4950_REG_END)
    {
        if(!ak4950_cached_registers[parameter[pos].reg].cached)
        {
            ak4950_cache(parameter[pos].reg);
        }
        pos++;
    }
    trace_write(sound_trace_ctx, "ak4950_write: ~~~ cache end ~~~");

    pos = 0;
    while(parameter[pos].reg != AK4950_REG_END)
    {
        /* build masks */
        uint32_t field_mask = (1 << parameter[pos].width) - 1;
        uint32_t reg_mask = field_mask << parameter[pos].pos;
        uint32_t reg_value = ((value >> parameter[pos].shift) & field_mask) << parameter[pos].pos;

        /* udpate cache and mark dirty */
        ak4950_cached_registers[parameter[pos].reg].value &= ~reg_mask;
        ak4950_cached_registers[parameter[pos].reg].value |= reg_value;
        ak4950_cached_registers[parameter[pos].reg].dirty = 1;

        pos++;
    }
}

static void ak4950_write_changed()
{
    trace_write(sound_trace_ctx, "ak4950_write_changed: ~~~ write start ~~~");
    for(uint32_t reg = 0; reg < COUNT(ak4950_cached_registers); reg++)
    {
        /* udpate cache and mark dirty */
        if(ak4950_cached_registers[reg].dirty)
        {
            ak4950_write_reg(reg);
        }
    }
    trace_write(sound_trace_ctx, "ak4950_write_changed: ~~~ write end ~~~");
}

static void ak4950_readall()
{
    trace_write(sound_trace_ctx, "ak4950_readall: ~~~ cache start ~~~");
    for(uint32_t reg = 0; reg < COUNT(ak4950_cached_registers); reg++)
    {
        ak4950_cache(reg);
    }
    trace_write(sound_trace_ctx, "ak4950_readall: ~~~ cache end ~~~");
}

static void ak4950_set_loop(uint32_t state)
{
    // FIXME: See p.50 of datasheet
    //~    AK4950_SET(AK4950_PAR_LOOP, state != 0);
    //~    ak4950_write_changed();
}

static void ak4950_unpower_mic()
{
    trace_write(sound_trace_ctx, "ak4950_unpower_mic");
    AK4950_SET(AK4950_PAR_PMMP, 0);
    AK4950_SET(AK4950_PAR_PMADL, 0);
    AK4950_SET(AK4950_PAR_PMADR, 0);
    ak4950_write_changed();
}

static void ak4950_power_mic()
{
    trace_write(sound_trace_ctx, "ak4950_power_mic");
    AK4950_SET(AK4950_PAR_PMMP, 1);
    AK4950_SET(AK4950_PAR_PMADL, 1);
    AK4950_SET(AK4950_PAR_PMADR, 1);
    ak4950_write_changed();
}

static void ak4950_unpower_out()
{
    trace_write(sound_trace_ctx, "ak4950_unpower_out");
    AK4950_SET(AK4950_PAR_PMSPK, 0);
    AK4950_SET(AK4950_PAR_PMDAC, 0);
    AK4950_SET(AK4950_PAR_LOPS, 1);
    AK4950_SET(AK4950_PAR_DACS, 0);
    AK4950_SET(AK4950_PAR_DACL, 0);
    AK4950_SET(AK4950_PAR_SPPSN, 0);
    AK4950_SET(AK4950_PAR_SPKG, 1);
    ak4950_write_changed();
}

static void ak4950_power_speaker()
{
    trace_write(sound_trace_ctx, "ak4950_power_speaker");
    AK4950_SET(AK4950_PAR_DACS, 1);
    AK4950_SET(AK4950_PAR_SPPSN, 0);
    AK4950_SET(AK4950_PAR_SPKG, 1);
    AK4950_SET(AK4950_PAR_PMSPK, 1);
    AK4950_SET(AK4950_PAR_PMDAC, 1);
    ak4950_write_changed();

    msleep(50);

    AK4950_SET(AK4950_PAR_SPPSN, 1);
    ak4950_write_changed();
}

/* used internally for headphone and AV line too */
static void ak4950_power_line()
{
    trace_write(sound_trace_ctx, "ak4950_power_line");
    AK4950_SET(AK4950_PAR_DACL, 1);
    AK4950_SET(AK4950_PAR_LOPS, 1);
    AK4950_SET(AK4950_PAR_PMLO, 1);
    AK4950_SET(AK4950_PAR_PMDAC, 1);
    ak4950_write_changed();

    msleep(10);

    AK4950_SET(AK4950_PAR_LOPS, 0);
    ak4950_write_changed();
}

static void ak4950_power_lineout()
{
    trace_write(sound_trace_ctx, "ak4950_power_lineout");
    ak4950_power_line();
}

static void ak4950_power_avline()
{
    trace_write(sound_trace_ctx, "ak4950_power_avline");
    ak4950_power_line();
}

static void ak4950_set_in_vol(uint32_t volume)
{
    trace_write(sound_trace_ctx, "ak4950_set_in_vol: %d", volume);
    AK4950_SET(AK4950_PAR_IVL, volume);
    AK4950_SET(AK4950_PAR_IVR, volume);
    ak4950_write_changed();
}

static void ak4950_set_out_vol(uint32_t volume)
{
    trace_write(sound_trace_ctx, "ak4950_set_out_vol: %d", volume);
    AK4950_SET(AK4950_PAR_OVL, volume);
    AK4950_SET(AK4950_PAR_OVR, volume);
    ak4950_write_changed();
}

static void ak4950_set_lineout_vol(uint32_t volume)
{
    trace_write(sound_trace_ctx, "ak4950_set_lineout_vol: %d", volume);
    AK4950_SET(AK4950_PAR_LOVL, volume);
    ak4950_write_changed();
}

static const char *ak4950_op_get_destination_name(enum sound_destination line)
{
    if(line >= COUNT(ak4950_dst_names))
    {
        return "N/A";
    }

    return ak4950_dst_names[line];
}

static const char *ak4950_op_get_source_name(enum sound_source line)
{
    if(line >= COUNT(ak4950_src_names))
    {
        return "N/A";
    }

    return ak4950_src_names[line];
}

static enum sound_result ak4950_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next)
{
    trace_write(sound_trace_ctx, "ak4950_op_apply_mixer");
    if(prev->speaker_gain != next->speaker_gain || ak4950_need_rewrite)
    {
        ak4950_set_out_vol(COERCE(next->speaker_gain * 5 / 2, 0, 0xF1));
    }

    if(prev->headphone_gain != next->headphone_gain || ak4950_need_rewrite)
    {
        ak4950_set_lineout_vol(COERCE(next->headphone_gain / 33, 0, 3));
    }

    if(prev->loop_mode != next->loop_mode || ak4950_need_rewrite)
    {
        ak4950_set_loop(next->loop_mode);
    }

    if(prev->source_line != next->source_line || ak4950_need_rewrite)
    {
        switch(next->source_line)
        {
            default:
            case SOUND_SOURCE_OFF:
                ak4950_unpower_mic();
                break;

            case SOUND_SOURCE_INT_MIC:
                ak4950_power_mic();
                AK4950_SET(AK4950_PAR_INR, 0);
                AK4950_SET(AK4950_PAR_INL, 0);
                ak4950_write_changed();
                break;

            case SOUND_SOURCE_EXT_MIC:
                ak4950_power_mic();
                AK4950_SET(AK4950_PAR_INR, 1);
                AK4950_SET(AK4950_PAR_INL, 1);
                ak4950_write_changed();
                break;

            case SOUND_SOURCE_EXTENDED_1:
                ak4950_power_mic();
                AK4950_SET(AK4950_PAR_INR, 1);
                AK4950_SET(AK4950_PAR_INL, 0);
                ak4950_write_changed();
                break;

        }
    }

    if(prev->destination_line != next->destination_line || ak4950_need_rewrite)
    {
        switch(next->destination_line)
        {
            default:
            case SOUND_DESTINATION_OFF:
                ak4950_unpower_out();
                break;

            case SOUND_DESTINATION_SPK:
                ak4950_unpower_out();
                ak4950_power_speaker();
                break;
            case SOUND_DESTINATION_LINE:
                ak4950_unpower_out();
                ak4950_power_lineout();
                break;
            case SOUND_DESTINATION_AV:
                ak4950_unpower_out();
                ak4950_power_avline();
                break;
            case SOUND_DESTINATION_EXTENDED_1:
                ak4950_power_speaker();
                ak4950_power_lineout();
                break;
            case SOUND_DESTINATION_EXTENDED_2:
                ak4950_power_speaker();
                ak4950_power_avline();
                break;
        }
    }

    ak4950_need_rewrite = 0;

    return SOUND_RESULT_OK;
}

static enum sound_result ak4950_op_set_rate(uint32_t rate)
{
    trace_write(sound_trace_ctx, "ak4950_op_set_rate: %d", rate);
    const uint32_t ak4950_rates[] = { 8000, 12000, 16000, 24000, 0, 11025, 0, 22050, 0, 0, 32000, 48000, 0, 0, 0, 44100 };

    for(uint32_t mode = 0; mode < 16; mode++)
    {
        if(ak4950_rates[mode] == rate)
        {
            AK4950_SET(AK4950_PAR_FS, mode);
            ak4950_write_changed();
        }
    }
    return SOUND_RESULT_OK;
}

static enum sound_result ak4950_op_poweron()
{
    trace_write(sound_trace_ctx, "ak4950_op_poweron");
    AK4950_SET(AK4950_PAR_INIT, 0);
    ak4950_write_changed();

    return SOUND_RESULT_OK;
}

static enum sound_result ak4950_op_poweroff()
{
    trace_write(sound_trace_ctx, "ak4950_op_poweroff");
    AK4950_SET(AK4950_PAR_PMBP, 0);
    AK4950_SET(AK4950_PAR_PMMP, 0);
    AK4950_SET(AK4950_PAR_PMDAC, 0);
    AK4950_SET(AK4950_PAR_PMLO, 0);
    AK4950_SET(AK4950_PAR_PMADL, 0);
    AK4950_SET(AK4950_PAR_PMADR, 0);
    AK4950_SET(AK4950_PAR_PMSPK, 0);
    ak4950_write_changed();

    ak4950_need_rewrite = 1;

    return SOUND_RESULT_OK;
}

void codec_init(struct codec_ops *ops)
{
    ak4950_readall();
    *ops = default_codec_ops;
}

