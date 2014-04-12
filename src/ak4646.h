#ifndef __ak4646_h__
#define __ak4646_h__

enum ak4646_op
{
    AK4646_OP_READ  = 0,
    AK4646_OP_WRITE = 1
};

enum ak4646_regs
{
    AK4646_REG_PM1    = 0x00,
    AK4646_REG_PM2    = 0x01,
    AK4646_REG_SIG1   = 0x02,
    AK4646_REG_SIG2   = 0x03,
    AK4646_REG_MODE1  = 0x04,
    AK4646_REG_MODE2  = 0x05,
    AK4646_REG_TIMER  = 0x06,
    AK4646_REG_ALC1   = 0x07,
    AK4646_REG_ALC2   = 0x08,
    AK4646_REG_LIVC   = 0x09,
    AK4646_REG_LDVC   = 0x0A,
    AK4646_REG_ALC3   = 0x0B,
    AK4646_REG_RIVC   = 0x0C,
    AK4646_REG_ALCLVL = 0x0D,
    AK4646_REG_MODE3  = 0x0E,
    AK4646_REG_MODE4  = 0x0F,
    AK4646_REG_PM3    = 0x10,
    AK4646_REG_FIL1   = 0x11,
    AK4646_REG_FIL3_0 = 0x12,
    AK4646_REG_FIL3_1 = 0x13,
    AK4646_REG_FIL3_2 = 0x14,
    AK4646_REG_FIL3_3 = 0x15,
    AK4646_REG_EQ0_0  = 0x16,
    AK4646_REG_EQ0_1  = 0x17,
    AK4646_REG_EQ0_2  = 0x18,
    AK4646_REG_EQ0_3  = 0x19,
    AK4646_REG_EQ0_4  = 0x1A,
    AK4646_REG_EQ0_5  = 0x1B,
    AK4646_REG_HPF0   = 0x1C,
    AK4646_REG_HPF1   = 0x1D,
    AK4646_REG_HPF2   = 0x1E,
    AK4646_REG_HPF3   = 0x1F,
    AK4646_REG_RDVC   = 0x25,
    AK4646_REG_LPF0   = 0x2C,
    AK4646_REG_LPF1   = 0x2D,
    AK4646_REG_LPF2   = 0x2E,
    AK4646_REG_LPF3   = 0x2F,
    AK4646_REG_FIL2   = 0x30,
    AK4646_REG_END    = 0xFF,
};

/* number of registers */
#define AK4646_REGS 0x4F

#define REG_END { .reg = AK4646_REG_END }

/* 00H  Power Management */
#define AK4646_PAR_PMADL    {{ .name = "PMADL", .reg = AK4646_REG_PM1, .pos = 0, .width = 1 }, REG_END }
#define AK4646_PAR_PMDAC    {{ .name = "PMDAC", .reg = AK4646_REG_PM1, .pos = 2, .width = 1 }, REG_END }
#define AK4646_PAR_PMLO     {{ .name = "PMLO", .reg = AK4646_REG_PM1, .pos = 3, .width = 1 }, REG_END }
#define AK4646_PAR_PMSPK    {{ .name = "PMSPK", .reg = AK4646_REG_PM1, .pos = 4, .width = 1 }, REG_END }
#define AK4646_PAR_PMBP     {{ .name = "PMBP", .reg = AK4646_REG_PM1, .pos = 5, .width = 1 }, REG_END }
#define AK4646_PAR_PMVCM    {{ .name = "PMVCM", .reg = AK4646_REG_PM1, .pos = 6, .width = 1 }, REG_END }

/* 01H  Power Management 2 */
#define AK4646_PAR_PMPLL    {{ .name = "PMPLL", .reg = AK4646_REG_PM2, .pos = 0, .width = 1 }, REG_END }
#define AK4646_PAR_MCKO     {{ .name = "MCKO", .reg = AK4646_REG_PM2, .pos = 1, .width = 1 }, REG_END }
#define AK4646_PAR_MS       {{ .name = "MS", .reg = AK4646_REG_PM2, .pos = 3, .width = 1 }, REG_END }

/* 02H  Signal Select 1 */
#define AK4646_PAR_MGAIN    {{ .name = "MGAIN", .reg = AK4646_REG_SIG1, .pos = 0, .width = 1 }, { .reg = AK4646_REG_SIG2, .pos = 5, .width = 1, .shift = 1 }, { .reg = AK4646_REG_SIG1, .pos = 1, .width = 1, .shift = 2 }, REG_END }
#define AK4646_PAR_PMMP     {{ .name = "PMMP", .reg = AK4646_REG_SIG1, .pos = 2, .width = 1 }, REG_END }
#define AK4646_PAR_DACL     {{ .name = "DACL", .reg = AK4646_REG_SIG1, .pos = 4, .width = 1 }, REG_END }
#define AK4646_PAR_DACS     {{ .name = "DACS", .reg = AK4646_REG_SIG1, .pos = 5, .width = 1 }, REG_END }
#define AK4646_PAR_BEEPS    {{ .name = "BEEPS", .reg = AK4646_REG_SIG1, .pos = 6, .width = 1 }, REG_END }
#define AK4646_PAR_SPPSN    {{ .name = "SPPSN", .reg = AK4646_REG_SIG1, .pos = 7, .width = 1 }, REG_END }

/* 03H  Signal Select 2 */
#define AK4646_PAR_LOVL     {{ .name = "LOVL", .reg = AK4646_REG_SIG2, .pos = 0, .width = 2 }, REG_END }
#define AK4646_PAR_BEEPL    {{ .name = "BEEPL", .reg = AK4646_REG_SIG2, .pos = 2, .width = 1 }, REG_END }
#define AK4646_PAR_SPKG     {{ .name = "SPKG", .reg = AK4646_REG_SIG2, .pos = 3, .width = 2 }, REG_END }
#define AK4646_PAR_LOPS     {{ .name = "LOPS", .reg = AK4646_REG_SIG2, .pos = 6, .width = 1 }, REG_END }
#define AK4646_PAR_DAFIL    {{ .name = "DAFIL", .reg = AK4646_REG_SIG2, .pos = 7, .width = 1 }, REG_END }

/* 04H  Mode Control 1 */
#define AK4646_PAR_DIF      {{ .name = "DIF", .reg = AK4646_REG_MODE1, .pos = 0, .width = 2 }, REG_END }
#define AK4646_PAR_BCKO     {{ .name = "BCKO", .reg = AK4646_REG_MODE1, .pos = 3, .width = 1 }, REG_END }
#define AK4646_PAR_PLL      {{ .name = "PLL", .reg = AK4646_REG_MODE1, .pos = 4, .width = 4 }, REG_END }

/* 05H  Mode Control 2 */
#define AK4646_PAR_FS       {{ .name = "FS", .reg = AK4646_REG_MODE2, .pos = 0, .width = 3}, { .reg = AK4646_REG_MODE2, .pos = 5, .width = 1, .shift = 3 }, REG_END }
#define AK4646_PAR_PS       {{ .name = "PS", .reg = AK4646_REG_MODE2, .pos = 6, .width = 2 }, REG_END }

/* 06H Timer Select */
#define AK4646_PAR_RFST     {{ .name = "RFST", .reg = AK4646_REG_TIMER, .pos = 0, .width = 2 }, REG_END }
#define AK4646_PAR_WTM      {{ .name = "WTM", .reg = AK4646_REG_TIMER, .pos = 2, .width = 2 }, { .reg = AK4646_REG_TIMER, .pos = 6, .width = 1, .shift = 2 }, REG_END }
#define AK4646_PAR_ZTM      {{ .name = "ZTM", .reg = AK4646_REG_TIMER, .pos = 4, .width = 2 }, REG_END }

/* 07H  ALC Mode Control 1 */
#define AK4646_PAR_LMTH     {{ .name = "LMTH", .reg = AK4646_REG_ALC1, .pos = 0, .width = 1 }, { .reg = AK4646_REG_ALC3, .pos = 6, .width = 1, .shift = 1 }, REG_END }
#define AK4646_PAR_RGAIN    {{ .name = "RGAIN", .reg = AK4646_REG_ALC1, .pos = 1, .width = 1 }, { .reg = AK4646_REG_ALC3, .pos = 7, .width = 1, .shift = 2 }, REG_END }
#define AK4646_PAR_LMAT     {{ .name = "LMAT", .reg = AK4646_REG_ALC1, .pos = 2, .width = 2 }, REG_END }
#define AK4646_PAR_ZELMN    {{ .name = "ZELMN", .reg = AK4646_REG_ALC1, .pos = 4, .width = 1 }, REG_END }
#define AK4646_PAR_ALC1     {{ .name = "ALC1", .reg = AK4646_REG_ALC1, .pos = 5, .width = 1 }, REG_END }
#define AK4646_PAR_ALC2     {{ .name = "ALC2", .reg = AK4646_REG_ALC1, .pos = 6, .width = 1 }, REG_END }
#define AK4646_PAR_LFST     {{ .name = "LFST", .reg = AK4646_REG_ALC1, .pos = 7, .width = 1 }, REG_END }

/* 08H  ALC Mode Control 2 */
#define AK4646_PAR_IREF     {{ .name = "IREF", .reg = AK4646_REG_ALC2, .pos = 0, .width = 8 }, REG_END }

/* 09H  Lch Input Volume Control */
/* 0CH  Rch Input Volume Control */
#define AK4646_PAR_IVL      {{ .name = "IVL", .reg = AK4646_REG_LIVC, .pos = 0, .width = 8 }, REG_END }
#define AK4646_PAR_IVR      {{ .name = "IVR", .reg = AK4646_REG_RIVC, .pos = 0, .width = 8 }, REG_END }

/* 0AH  Lch Digital Volume Control */
/* 25H  Rch Digital Volume Control */
#define AK4646_PAR_OVL      {{ .name = "OVL", .reg = AK4646_REG_LDVC, .pos = 0, .width = 8 }, REG_END }
#define AK4646_PAR_OVR      {{ .name = "OVR", .reg = AK4646_REG_RDVC, .pos = 0, .width = 8 }, REG_END }

/* 0BH  ALC Mode Control 3 */
#define AK4646_PAR_OREF     {{ .name = "OREF", .reg = AK4646_REG_ALC3, .pos = 0, .width = 6 }, REG_END }

/* 0DH  ALC LEVEL */
#define AK4646_PAR_VOL      {{ .name = "VOL", .reg = AK4646_REG_ALCLVL, .pos = 0, .width = 8 }, REG_END }

/* 0EH  Mode Control 3 */
#define AK4646_PAR_DEM      {{ .name = "DEM", .reg = AK4646_REG_MODE3, .pos = 0, .width = 2 }, REG_END }
#define AK4646_PAR_DATT     {{ .name = "DATT", .reg = AK4646_REG_MODE3, .pos = 2, .width = 2 }, REG_END }
#define AK4646_PAR_OVOLC    {{ .name = "OVOLC", .reg = AK4646_REG_MODE3, .pos = 4, .width = 1 }, REG_END }
#define AK4646_PAR_SMUTE    {{ .name = "SMUTE", .reg = AK4646_REG_MODE3, .pos = 5, .width = 1 }, REG_END }
#define AK4646_PAR_LOOP     {{ .name = "LOOP", .reg = AK4646_REG_MODE3, .pos = 6, .width = 1 }, REG_END }
#define AK4646_PAR_READ     {{ .name = "READ", .reg = AK4646_REG_MODE3, .pos = 7, .width = 1 }, REG_END }

/* 0FH  Mode Control 4 */
#define AK4646_PAR_IVOLC    {{ .name = "IVOLC", .reg = AK4646_REG_MODE4, .pos = 3, .width = 1 }, REG_END }
#define AK4646_PAR_FR       {{ .name = "FR", .reg = AK4646_REG_MODE4, .pos = 4, .width = 1 }, REG_END }

/* 10H  Power Management 3 */
#define AK4646_PAR_PMADR    {{ .name = "PMADR", .reg = AK4646_REG_PM3, .pos = 0, .width = 1 }, REG_END }
#define AK4646_PAR_INL      {{ .name = "INL", .reg = AK4646_REG_PM3, .pos = 1, .width = 1 }, REG_END }
#define AK4646_PAR_INR      {{ .name = "INR", .reg = AK4646_REG_PM3, .pos = 2, .width = 1 }, REG_END }
#define AK4646_PAR_MDIF1    {{ .name = "MDIF1", .reg = AK4646_REG_PM3, .pos = 3, .width = 1 }, REG_END }
#define AK4646_PAR_MDIF2    {{ .name = "MDIF2", .reg = AK4646_REG_PM3, .pos = 4, .width = 1 }, REG_END }


#define AK4646_SET(par, val) do { struct ak4646_parameter par_loc[] = par; ak4646_write(par_loc, val); } while(0)

struct ak4646_cache_entry
{
    /* set if this cache entry is valid */
    uint8_t cached;
    /* set if it was changed */
    uint8_t dirty;
    /* current value */
    uint8_t value;
};


struct ak4646_parameter
{
    /* name for debugging purpose */
    const char *name;
    /* register in which this parameter is */
    enum ak4646_regs reg;
    /* bit position 0..7 */
    uint8_t pos;
    /* number of bits */
    uint8_t width;
    /* shift right source value, defaults to zero */
    uint8_t shift;
};

void ak4646_init(struct codec_ops *ops);

/* call this with e.g. AK4646_PAR_PMDAC and the value you want it to be set */
static void ak4646_write(struct ak4646_parameter parameter[], uint32_t value);
static void ak4646_power_speaker();
static void ak4646_power_lineout();
static void ak4646_power_avline();
static void ak4646_set_out_vol(uint32_t volume);
static void ak4646_unpower_out();
static void ak4646_unpower_in();
static void ak4646_set_loop(uint32_t state);

static enum sound_result ak4646_op_set_rate(uint32_t rate);
static enum sound_result ak4646_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next);
static enum sound_result ak4646_op_poweron();
static enum sound_result ak4646_op_poweroff();
static const char *ak4646_op_get_destination_name(enum sound_destination line);
static const char *ak4646_op_get_source_name(enum sound_source line);


#endif