#ifndef __ak4950_h__
#define __ak4950_h__

enum ak4950_op
{
    AK4950_OP_READ  = 0,
    AK4950_OP_WRITE = 1
};

enum ak4950_regs
{
    AK4950_REG_PM1    = 0x00,
    AK4950_REG_PM2    = 0x01,
    AK4950_REG_MGAIN1 = 0x02,
    AK4950_REG_GAIN   = 0x03,
    AK4950_REG_MODE1  = 0x04,
    AK4950_REG_MODE2  = 0x05,
    AK4950_REG_MODE3  = 0x06,
    AK4950_REG_PLL1   = 0x07,
    AK4950_REG_PLL2   = 0x08,
    AK4950_REG_DMIC   = 0x09,
    AK4950_REG_BEEP   = 0x0A,
    AK4950_REG_HPF    = 0x0B,
    AK4950_REG_VID    = 0x0C,
    AK4950_REG_MODE4  = 0x0D,
    AK4950_REG_MODE5  = 0x0E,
    AK4950_REG_ALCLVL = 0x0F,
    AK4950_REG_LIVC   = 0x20,
    AK4950_REG_RIVC   = 0x21,
    AK4950_REG_LOVC   = 0x22,
    AK4950_REG_ROVC   = 0x23,
    AK4950_REG_ALC1   = 0x24,
    AK4950_REG_ALC2   = 0x25,
    AK4950_REG_ALC3   = 0x26,
    AK4950_REG_ALC4   = 0x27,
    AK4950_REG_DVOL   = 0x2A,
    AK4950_REG_MGAIN2 = 0x2B,
    AK4950_REG_FIL1   = 0x2C,
    AK4950_REG_FIL2   = 0x2D,
    AK4950_REG_FIL3   = 0x2E,
    AK4950_REG_END    = 0xFF,
};

/* number of registers */
#define AK4950_REGS 0x2F

#define REG_END { .reg = AK4950_REG_END }

/* 00H  Power Management */
#define AK4950_PAR_PMADL    {{ .name = "PMADL", .reg = AK4950_REG_PM1, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_PMADR    {{ .name = "PMADR", .reg = AK4950_REG_PM1, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_PMDAC    {{ .name = "PMDAC", .reg = AK4950_REG_PM1, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_PMLO     {{ .name = "PMLO",  .reg = AK4950_REG_PM1, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_PMSPK    {{ .name = "PMSPK", .reg = AK4950_REG_PM1, .pos = 20, .width = 1 }, REG_END }
#define AK4950_PAR_PMBP     {{ .name = "PMBP",  .reg = AK4950_REG_PM1, .pos = 21, .width = 1 }, REG_END }
#define AK4950_PAR_PMFIL    {{ .name = "PMFIL", .reg = AK4950_REG_PM1, .pos = 23, .width = 1 }, REG_END }


/* 01H  Power Management 2 */
#define AK4950_PAR_PMPLL    {{ .name = "PMPLL", .reg = AK4950_REG_PM2, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_MCKO     {{ .name = "MCKO",  .reg = AK4950_REG_PM2, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_PMMP     {{ .name = "PMMP",  .reg = AK4950_REG_PM2, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_MS       {{ .name = "MS",    .reg = AK4950_REG_PM2, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_ADRST    {{ .name = "ADRST", .reg = AK4950_REG_PM2, .pos = 23, .width = 1 }, REG_END }

/* 02H  MIC Gain Control */
#define AK4950_PAR_MGAIN    {{ .name = "MGAIN", .reg = AK4950_REG_MGAIN1, .pos = 16, .width = 4 }, REG_END }

/* 03H  Gain Control */
#define AK4950_PAR_LOVL     {{ .name = "LOVL", .reg = AK4950_REG_GAIN, .pos = 16, .width = 2 }, REG_END }
#define AK4950_PAR_SPKG     {{ .name = "SPKG", .reg = AK4950_REG_GAIN, .pos = 20, .width = 2 }, REG_END }
#define AK4950_PAR_MICL     {{ .name = "MICL", .reg = AK4950_REG_GAIN, .pos = 23, .width = 1 }, REG_END }

/* 04H  Mode Control 1 */
#define AK4950_PAR_DACL     {{ .name = "DACL",  .reg = AK4950_REG_MODE1, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_DACS     {{ .name = "DACS",  .reg = AK4950_REG_MODE1, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_BEEPL    {{ .name = "BEEPL", .reg = AK4950_REG_MODE1, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_BEEPS    {{ .name = "BEEPS", .reg = AK4950_REG_MODE1, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_LOPS     {{ .name = "LOPS",  .reg = AK4950_REG_MODE1, .pos = 22, .width = 1 }, REG_END }
#define AK4950_PAR_SPPSN    {{ .name = "SPPSN", .reg = AK4950_REG_MODE1, .pos = 23, .width = 1 }, REG_END }

/* 05H  Mode Control 2 */
#define AK4950_PAR_INL      {{ .name = "INL",   .reg = AK4950_REG_MODE2, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_INR      {{ .name = "INR",   .reg = AK4950_REG_MODE2, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_MLOUT    {{ .name = "MLOUT", .reg = AK4950_REG_MODE2, .pos = 22, .width = 1 }, REG_END }
#define AK4950_PAR_READ     {{ .name = "READ",  .reg = AK4950_REG_MODE2, .pos = 23, .width = 1 }, REG_END }

/* 06H  Mode Control 3 */
#define AK4950_PAR_DEM      {{ .name = "DEM",  .reg = AK4950_REG_MODE3, .pos = 16, .width = 2 }, REG_END }
#define AK4950_PAR_MONO     {{ .name = "MONO", .reg = AK4950_REG_MODE3, .pos = 18, .width = 2 }, REG_END }
#define AK4950_PAR_EXTC     {{ .name = "EXTC", .reg = AK4950_REG_MODE3, .pos = 21, .width = 1 }, REG_END }

/* 07H  PLL Control 1 */
#define AK4950_PAR_DIF      {{ .name = "DIF",  .reg = AK4950_REG_PLL1, .pos = 16, .width = 2 }, REG_END }
#define AK4950_PAR_BCKO     {{ .name = "BCKO", .reg = AK4950_REG_PLL1, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_PLL      {{ .name = "PLL",  .reg = AK4950_REG_PLL1, .pos = 20, .width = 4 }, REG_END }

/* 08H  PLL Control 2 */
#define AK4950_PAR_FS       {{ .name = "FS", .reg = AK4950_REG_PLL2, .pos = 16, .width = 4 }, REG_END }
#define AK4950_PAR_PS       {{ .name = "PS", .reg = AK4950_REG_PLL2, .pos = 22, .width = 2 }, REG_END }

/* 09H  Digital MIC */
#define AK4950_PAR_DMIC     {{ .name = "DMIC",  .reg = AK4950_REG_DMIC, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_DCLKP    {{ .name = "DCLKP", .reg = AK4950_REG_DMIC, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_DCLKE    {{ .name = "DCLKE", .reg = AK4950_REG_DMIC, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_PMDML    {{ .name = "PMDML", .reg = AK4950_REG_DMIC, .pos = 20, .width = 1 }, REG_END }
#define AK4950_PAR_PMDMR    {{ .name = "PMDMR", .reg = AK4950_REG_DMIC, .pos = 21, .width = 1 }, REG_END }

/* 0AH  BEEP Control */
#define AK4950_PAR_BPLVL    {{ .name = "BPLVL", .reg = AK4950_REG_BEEP, .pos = 16, .width = 3 }, REG_END }
#define AK4950_PAR_BPVCM    {{ .name = "BPVCM", .reg = AK4950_REG_BEEP, .pos = 20, .width = 1 }, REG_END }
#define AK4950_PAR_BPM      {{ .name = "BPM",   .reg = AK4950_REG_BEEP, .pos = 23, .width = 1 }, REG_END }

/* 0BH  HPF Control */
#define AK4950_PAR_HPFAD    {{ .name = "HPFAD", .reg = AK4950_REG_HPF, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_HPFC     {{ .name = "HPFC",  .reg = AK4950_REG_HPF, .pos = 17, .width = 2 }, REG_END }

/* 0CH  Video Control */
#define AK4950_PAR_PMV      {{ .name = "PMV",  .reg = AK4950_REG_VID, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_PMCP     {{ .name = "PMCP", .reg = AK4950_REG_VID, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_VG       {{ .name = "VG",   .reg = AK4950_REG_VID, .pos = 18, .width = 2 }, REG_END }

/* 0DH  Mode Control 4 */
#define AK4950_PAR_COEW     {{ .name = "COEW",  .reg = AK4950_REG_MODE4, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_THDET    {{ .name = "THDET", .reg = AK4950_REG_MODE4, .pos = 23, .width = 1 }, REG_END }
  
/* 0EH  Mode Control 5 */
#define AK4950_PAR_INIT     {{ .name = "INIT", .reg = AK4950_REG_MODE5, .pos = 16, .width = 1 }, REG_END }

/* 0FH  ALC LEVEL */
#define AK4950_PAR_VOL      {{ .name = "VOL", .reg = AK4950_REG_ALCLVL, .pos = 16, .width = 8 }, REG_END }

/* 20H-23H  Volume Control */
#define AK4950_PAR_IVL      {{ .name = "IVL", .reg = AK4950_REG_LIVC, .pos = 16, .width = 8 }, REG_END }
#define AK4950_PAR_IVR      {{ .name = "IVR", .reg = AK4950_REG_RIVC, .pos = 16, .width = 8 }, REG_END }
#define AK4950_PAR_OVL     {{ .name = "OVL",  .reg = AK4950_REG_LOVC, .pos = 16, .width = 8 }, REG_END }
#define AK4950_PAR_OVR     {{ .name = "OVR",  .reg = AK4950_REG_ROVC, .pos = 16, .width = 8 }, REG_END }

/* 24H/25H  ALC Mode Control 1/2 */
#define AK4950_PAR_IREF      {{ .name = "IREF", .reg = AK4950_REG_ALC1, .pos = 16, .width = 8 }, REG_END }
#define AK4950_PAR_OREF      {{ .name = "OREF", .reg = AK4950_REG_ALC2, .pos = 16, .width = 8 }, REG_END }

/* 26H  ALC Mode Control 3 */
#define AK4950_PAR_RFST      {{ .name = "RFST", .reg = AK4950_REG_ALC3, .pos = 16, .width = 2 }, REG_END }
#define AK4950_PAR_WTM       {{ .name = "WTM",  .reg = AK4950_REG_ALC3, .pos = 20, .width = 3 }, REG_END }

/* 27H  ALC Mode Control 4 */
#define AK4950_PAR_LMTH      {{ .name = "LMTH",  .reg = AK4950_REG_ALC4, .pos = 16, .width = 2 }, REG_END }
#define AK4950_PAR_RGAIN     {{ .name = "RGAIN", .reg = AK4950_REG_ALC4, .pos = 19, .width = 3 }, REG_END }
#define AK4950_PAR_ALC       {{ .name = "ALC",   .reg = AK4950_REG_ALC4, .pos = 22, .width = 1 }, REG_END }
#define AK4950_PAR_SMUTE     {{ .name = "SMUTE", .reg = AK4950_REG_ALC4, .pos = 23, .width = 1 }, REG_END }

/* 2AH  Digital Volume Control */
#define AK4950_PAR_DVOL      {{ .name = "DVOL", .reg = AK4950_REG_DVOL, .pos = 16, .width = 8 }, REG_END }

/* 2BH  MIC Gain Control 2 */
#define AK4950_PAR_MSGAINL   {{ .name = "MSGAINL", .reg = AK4950_REG_MGAIN2, .pos = 16, .width = 4 }, REG_END }
#define AK4950_PAR_MSGAINR   {{ .name = "MSGAINR", .reg = AK4950_REG_MGAIN2, .pos = 20, .width = 4 }, REG_END }

/* 2CH  Digital Filter Control 1 */
#define AK4950_PAR_IVOLC     {{ .name = "IVOLC", .reg = AK4950_REG_FIL1, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_OVOLC     {{ .name = "OVOLC", .reg = AK4950_REG_FIL1, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_ADCPF     {{ .name = "ADCPF", .reg = AK4950_REG_FIL1, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_PFDAC     {{ .name = "PFDAC", .reg = AK4950_REG_FIL1, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_PFSDO     {{ .name = "PFSDO", .reg = AK4950_REG_FIL1, .pos = 20, .width = 1 }, REG_END }

/* 2DH  Digital Filter Control 2 */
#define AK4950_PAR_FIL3      {{ .name = "FIL3", .reg = AK4950_REG_FIL2, .pos = 16, .width = 1 }, REG_END }
#define AK4950_PAR_GN        {{ .name = "GN",   .reg = AK4950_REG_FIL2, .pos = 17, .width = 2 }, REG_END }
#define AK4950_PAR_EQ0       {{ .name = "EQ0",  .reg = AK4950_REG_FIL2, .pos = 19, .width = 1 }, REG_END }
#define AK4950_PAR_HPF       {{ .name = "HPF",  .reg = AK4950_REG_FIL2, .pos = 20, .width = 1 }, REG_END }
#define AK4950_PAR_LPF       {{ .name = "LPF",  .reg = AK4950_REG_FIL2, .pos = 21, .width = 1 }, REG_END }

/* 2EH  Digital Filter Control 3 */
#define AK4950_PAR_EQ0       {{ .name = "EQ2", .reg = AK4950_REG_FIL3, .pos = 17, .width = 1 }, REG_END }
#define AK4950_PAR_EQ0       {{ .name = "EQ3", .reg = AK4950_REG_FIL3, .pos = 18, .width = 1 }, REG_END }
#define AK4950_PAR_EQ0       {{ .name = "EQ4", .reg = AK4950_REG_FIL3, .pos = 19, .width = 1 }, REG_END }

#define AK4950_SET(par, val) do { struct ak4950_parameter par_loc[] = par; ak4950_write(par_loc, val); } while(0)

struct ak4950_cache_entry
{
    /* set if this cache entry is valid */
    uint8_t cached;
    /* set if it was changed */
    uint8_t dirty;
    /* current value */
    uint32_t value;
};


struct ak4950_parameter
{
    /* name for debugging purpose */
    const char *name;
    /* register in which this parameter is */
    enum ak4950_regs reg;
    /* bit position 0..23 */
    uint8_t pos;
    /* number of bits */
    uint8_t width;
    /* shift right source value, defaults to zero */
    uint8_t shift;
};

void ak4950_init(struct codec_ops *ops);

/* call this with e.g. AK4950_PAR_PMDAC and the value you want it to be set */
static void ak4950_write(struct ak4950_parameter parameter[], uint32_t value);
static void ak4950_power_speaker();
static void ak4950_power_lineout();
static void ak4950_power_avline();
static void ak4950_set_out_vol(uint32_t volume);
static void ak4950_unpower_out();
static void ak4950_unpower_in();
static void ak4950_set_loop(uint32_t state);

static enum sound_result ak4950_op_set_rate(uint32_t rate);
static enum sound_result ak4950_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next);
static enum sound_result ak4950_op_poweron();
static enum sound_result ak4950_op_poweroff();
static const char *ak4950_op_get_destination_name(enum sound_destination line);
static const char *ak4950_op_get_source_name(enum sound_source line);


#endif
