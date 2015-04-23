/*
 * Copyright (C) 2011 LAPIS Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef ML26121_H
#define ML26121_H

#define ML26121_REGS 0xFF


enum ml26121_op
{
    ML26121_OP_READ  = 0,
    ML26121_OP_WRITE = 1
};

struct ml26121_cache_entry
{
    /* set if this cache entry is valid */
    uint8_t cached;
    /* set if it was changed */
    uint8_t dirty;
    /* current value */
    uint8_t value;
};

/* alternative register list, taken from linux/sound/soc/codecs/ml26124.h */
enum ml26121_regs
{
    /* Clock Control Register */
    ML26121_SMPLING_RATE           = 0x00, /* Sampling Rate */
    ML26121_PLLNL                  = 0x02, /* PLL NL The value can be set from 0x001 to 0x1FF. */
    ML26121_PLLNH                  = 0x04, /* PLL NH The value can be set from 0x001 to 0x1FF. */
    ML26121_PLLML                  = 0x06, /* PLL ML The value can be set from 0x0020 to 0x3FFF. */
    ML26121_PLLMH                  = 0x08, /* MLL MH The value can be set from 0x0020 to 0x3FFF. */
    ML26121_PLLDIV                 = 0x0a, /* PLL DIV The value can be set from 0x01 to 0x1F. */
    ML26121_CLK_EN                 = 0x0c, /* MCLKEN + PLLEN +PLLOE, Clock Enable */
    ML26121_CLK_CTL                = 0x0e, /* CLK Input/Output Control */

    /* System Control Register */
    ML26121_SW_RST                 = 0x10, /* Software RESET */
    ML26121_RECPLAY_STATE          = 0x12, /* Record/Playback Run */
    ML26121_MIC_TIM                = 0x14, /* This register is to select the wait time for microphone input load charge when starting reording or playback using AutoStart mode. The setting of this register is ignored except Auto Start mode.*/

    /* Power Mnagement Register */
    ML26121_PW_REF_PW_MNG          = 0x20, /* MICBIAS */ /* Reference Power Management */
    ML26121_PW_IN_PW_MNG           = 0x22, /* ADC "Capture" + PGA Power Management */
    ML26121_PW_DAC_PW_MNG          = 0x24, /* DAC Power Management */
    ML26121_PW_SPAMP_PW_MNG        = 0x26, /* Speaker AMP Power Management */
    ML26121_PW_LOUT_PW_MNG         = 0x28,
    ML26121_PW_VOUT_PW_MNG         = 0x2a,
    ML26121_PW_ZCCMP_PW_MNG        = 0x2e, /* ZC-CMP Power Management */

    /* Analog Reference Control Register */
    ML26121_PW_MICBIAS_VOL         = 0x30, /* MICBIAS Voltage Control */

    /* Input/Output Amplifier Control Register */
    ML26121_PW_MIC_IN_VOL          = 0x32, /* MIC Input Volume */
    ML26121_PW_MIC_BOOST_VOL1      = 0x38, /* Mic Boost Volume 1 */
    ML26121_PW_SPK_AMP_VOL         = 0x3a, /* Speaker AMP Volume */
    ML26121_PW_HP_AMP_VOL          = 0x3a, /* headphone amp vol control*/
    ML26121_PW_AMP_VOL_FUNC        = 0x48, /* AMP Volume Control Function Enable */
    ML26121_PW_AMP_VOL_FADE        = 0x4a, /* Amplifier Volume Fader Control */

    /* Analog Path Control Register */
    ML26121_SPK_AMP_OUT            = 0x54, /* DAC Switch + Line in loopback Switch + PGA Switch */ /* Speaker AMP Output Control */
    ML26121_HP_AMP_OUT_CTL         = 0x56, /* headphone amp control*/
    ML26121_MIC_IF_CTL             = 0x5a, /* Mic IF Control */

    /* Audio Interface Control Register */
    ML26121_SAI_TRANS_CTL          = 0x60, /* SAI-Trans Control */
    ML26121_SAI_RCV_CTL            = 0x62, /* SAI-Receive Control */
    ML26121_SAI_MODE_SEL           = 0x64, /* SAI Mode select */

    /* DSP Control Register */
    ML26121_FILTER_EN              = 0x66, /* Filter Func Enable */ /*DC High Pass Filter Switch+ Noise High Pass Filter Switch + EQ Band0/1/2/3/4 Switch  V: '01' '00' */
    ML26121_DVOL_CTL               = 0x68, /* Digital Volume Control Func Enable */ /* Play Limiter + Capture Limiter + Digital Volume Fade Switch +Digital Switch */
    ML26121_MIXER_VOL_CTL          = 0x6a, /* Mixer & Volume Control*/
    ML26121_RECORD_DIG_VOL         = 0x6c, /* Capture/Record Digital Volume */
    ML26121_RECORD_DIG_VOL2        = 0x6e, /* Capture/Record Digital Volume 2 ?*/
    ML26121_PLBAK_DIG_VOL          = 0x70, /* Playback Digital Volume */
    ML26121_DIGI_BOOST_VOL         = 0x72,
    ML26121_EQ_GAIN_BRAND0         = 0x74, /* EQ Band0 Volume */
    ML26121_EQ_GAIN_BRAND1         = 0x76, /* EQ Band1 Volume */
    ML26121_EQ_GAIN_BRAND2         = 0x78, /* EQ Band2 Volume */
    ML26121_EQ_GAIN_BRAND3         = 0x7a, /* EQ Band3 Volume */
    ML26121_EQ_GAIN_BRAND4         = 0x7c, /* EQ Band4 Volume */
    ML26121_HPF2_CUTOFF            = 0x7e, /* HPF2 CutOff*/
    ML26121_EQBRAND0_F0L           = 0x80, /* EQ Band0 Coef0L */
    ML26121_EQBRAND0_F0H           = 0x82,
    ML26121_EQBRAND0_F1L           = 0x84,
    ML26121_EQBRAND0_F1H           = 0x86,
    ML26121_EQBRAND1_F0L           = 0x88,
    ML26121_EQBRAND1_F0H           = 0x8a,
    ML26121_EQBRAND1_F1L           = 0x8c,
    ML26121_EQBRAND1_F1H           = 0x8e,
    ML26121_EQBRAND2_F0L           = 0x90,
    ML26121_EQBRAND2_F0H           = 0x92,
    ML26121_EQBRAND2_F1L           = 0x94,
    ML26121_EQBRAND2_F1H           = 0x96,
    ML26121_EQBRAND3_F0L           = 0x98,
    ML26121_EQBRAND3_F0H           = 0x9a,
    ML26121_EQBRAND3_F1L           = 0x9c,
    ML26121_EQBRAND3_F1H           = 0x9e,
    ML26121_EQBRAND4_F0L           = 0xa0,
    ML26121_EQBRAND4_F0H           = 0xa2,
    ML26121_EQBRAND4_F1L           = 0xa4,
    ML26121_EQBRAND4_F1H           = 0xa6,
    ML26121_MIC_PARAM10            = 0xa8, /* MC parameter10 */
    ML26121_MIC_PARAM11            = 0xaa, /* MC parameter11 */
    ML26121_SND_EFFECT_MODE        = 0xac, /* Sound Effect Mode */

    /* ALC Control Register */
    ML26121_ALC_MODE               = 0xb0, /* ALC Mode */
    ML26121_ALC_ATTACK_TIM         = 0xb2, /* ALC Attack Time */
    ML26121_ALC_DECAY_TIM          = 0xb4, /* ALC Decay Time */
    ML26121_ALC_HOLD_TIM           = 0xb6, /* ALC Hold Time */
    ML26121_ALC_TARGET_LEV         = 0xb8, /* ALC Target Level */
    ML26121_ALC_MAXMIN_GAIN        = 0xba, /* ALC Min/Max Input Volume/Gain 2 sets of bits */
    ML26121_NOIS_GATE_THRSH        = 0xbc, /* Noise Gate Threshold */
    ML26121_ALC_ZERO_TIMOUT        = 0xbe, /* ALC ZeroCross TimeOut */

    /* Playback Limiter Control Register */
    ML26121_PL_ATTACKTIME          = 0xc0, /* PL Attack Time */
    ML26121_PL_DECAYTIME           = 0xc2, /* PL Decay Time */
    ML26121_PL_TARGETTIME          = 0xc4, /* PL Target Level */
    ML26121_PL_MAXMIN_GAIN         = 0xc6, /* Playback Limiter Min/max Input Volume/Gain 2 sets */
    ML26121_PLYBAK_BOST_VOL        = 0xc8, /* Playback Boost Volume */
    ML26121_PL_0CROSS_TIMOUT       = 0xca, /* PL ZeroCross TimeOut */

    /* Video Amplifer Control Register */
    ML26121_VIDEO_AMP_GAIN_CTL     = 0xd0,
    ML26121_VIDEO_AMP_SETUP1       = 0xd2,
    ML26121_VIDEO_AMP_CTL2         = 0xd4,

    ML26121_PW_MIC_BOOST_VOL2      = 0xe2, /* Mic Boost Volume 2 */
    ML26121_RECORD_PATH            = 0xe4, /* analog record path */
    ML26121_RCH_MIXER_INPUT        = 0xe0, /* R-ch mic select. PGA control for MINR */
    ML26121_LCH_MIXER_INPUT        = 0xe8, /* L-ch mic select. PGA control for MINL */
};

enum ml26121_smpling_rate
{
    ML26121_SMPLING_RATE_8kHz  = 0x00, /* Sampling Rate */
    ML26121_SMPLING_RATE_11kHz = 0x01, /* 11,025 Sampling Rate */
    ML26121_SMPLING_RATE_12kHz = 0x02, /* Sampling Rate */
    ML26121_SMPLING_RATE_16kHz = 0x03, /* Sampling Rate */
    ML26121_SMPLING_RATE_22kHz = 0x04, /* 22,05 Sampling Rate */
    ML26121_SMPLING_RATE_24kHz = 0x05, /* Sampling Rate */
    ML26121_SMPLING_RATE_32kHz = 0x06, /* Sampling Rate */
    ML26121_SMPLING_RATE_44kHz = 0x07, /* 44,1 Sampling Rate */
    ML26121_SMPLING_RATE_48kHz = 0x08, /* Sampling Rate */
};

enum ml26121_recplay_state
{
    ML26121_RECPLAY_STATE_STOP     = 0x00,
    ML26121_RECPLAY_STATE_REC      = 0x01,
    ML26121_RECPLAY_STATE_PLAY     = 0x02,
    ML26121_RECPLAY_STATE_RECPLAY  = 0x03,
    ML26121_RECPLAY_STATE_MON      = 0x07,
    ML26121_RECPLAY_STATE_AUTO_ON  = 0x10,
};

enum ml26121_pw_ref
{
    ML26121_PW_REF_PW_MNG_ALL_OFF = 0x00,
    ML26121_PW_REF_PW_HISPEED     = 0x01,  /* 000000xx */
    ML26121_PW_REF_PW_NORMAL      = 0x02,  /* 000000xx */
    ML26121_PW_REF_PW_MICBEN_ON   = 0x04,  /* 00000x00 */
    ML26121_PW_REF_PW_HP_FIRST    = 0x10,  /* 00xx0000 */
    ML26121_PW_REF_PW_HP_STANDARD = 0x20,  /* 00xx0000 */
};

enum ml26121_pw_in
{
    ML26121_PW_IN_PW_MNG_OFF  = 0x00, /*  OFF */
    ML26121_PW_IN_PW_MNG_DAC  = 0x02, /* ADC "Capture" ON */
    ML26121_PW_IN_PW_MNG_PGA  = 0x08, /* PGA ON */
    ML26121_PW_IN_PW_MNG_BOTH = 0x0a, /* ADC "Capture" + PGA ON */
};

enum ml26121_pw_dac
{
    ML26121_PW_DAC_PW_MNG_PWROFF = 0x00,
    ML26121_PW_DAC_PW_MNG_PWRON  = 0x02,
};

enum ml26121_pw_spamp
{
    ML26121_PW_SPAMP_PW_MNG_P_ON = 0x13, /* BTL mode Pre amplifier power ON Can not make output only pre amplifier. Output can be available at 05h or 1Fh. */
    ML26121_PW_SPAMP_PW_MNG_ON   = 0x1f, /* BTL mode Speaker amplifier power ON */
    ML26121_PW_SPAMP_PW_MNG_OFF  = 0x00, /* OFF */
    ML26121_PW_SPAMP_PW_MNG_RON  = 0x01, /* R Channel HeadPhone amplifier ON */
    ML26121_PW_SPAMP_PW_MNG_LON  = 0x04, /* L Channel HeadPhone amplifier ON */
    ML26121_PW_SPAMP_PW_MNG_STR  = 0x05, /* Stereo HeadPhone amp ON(HPMID not used) */
    ML26121_PW_SPAMP_PW_MNG_HPM  = 0x07, /* Stereo HeadPhone amp ON(HPMID used) */
};

enum ml26121_pw_zccmp
{
    ML26121_PW_ZCCMP_PW_MNG_OFF = 0x00, /* PGA zero cross comparator power off PGA volume is effective right after setting value change. */
    ML26121_PW_ZCCMP_PW_MNG_ON  = 0x01, /* PGA zero cross comparator power on PGA volume change is applied for zero cross. */
};

enum ml26121_mic_in_vol
{
    ML26121_MIC_IN_VOL_0   = 0x00, /** -12+0.75*num */
    ML26121_MIC_IN_VOL_1   = 0x0c, /** - 3dB */
    ML26121_MIC_IN_VOL_2   = 0x10, /**   0dB */
    ML26121_MIC_IN_VOL_3   = 0x18, /** + 6dB */
    ML26121_MIC_IN_VOL_4   = 0x24, /** +15dB */
    ML26121_MIC_IN_VOL_5   = 0x30, /** +24dB */
    ML26121_MIC_IN_VOL_6   = 0x3c, /** +33dB */
    ML26121_MIC_IN_VOL_MAX = 0x3f, /** +35.25dB */
};

enum ml26121_mic_boost_vol1
{
    ML26121_MIC_BOOST_VOL1_OFF = 0x00, // 0
    ML26121_MIC_BOOST_VOL1_1   = 0x10, // +9.75dB
    ML26121_MIC_BOOST_VOL1_2   = 0x20, // +19.50dB
    ML26121_MIC_BOOST_VOL1_3   = 0x30, // +29.25dB (not valid with Boost vol2)
};

enum ml26121_mic_boost_vol2
{
    ML26121_MIC_BOOST_VOL2_ON  = 0x01, // increase +4.875dB to boost vol1
    ML26121_MIC_BOOST_VOL2_OFF = 0x00, // unchange boost vol1
};

enum ml26121_pw_amp_vol_func
{
    ML26121_PW_AMP_VOL_FUNC_ENA_FADE_ON = 0x01,
    ML26121_PW_AMP_VOL_FUNC_ENA_AVMUTE  = 0x02,
};

enum ml26121_amp_volfunc_fade
{
    ML26121_AMP_VOL_FADE_0   = 0x00, //     1/fs     20.8us
    ML26121_AMP_VOL_FADE_1   = 0x01, //     4/fs     83.3us
    ML26121_AMP_VOL_FADE_2   = 0x02, //    16/fs      333us
    ML26121_AMP_VOL_FADE_3   = 0x03, //    64/fs     1.33ms
    ML26121_AMP_VOL_FADE_4   = 0x04, //   256/fs     5.33ms
    ML26121_AMP_VOL_FADE_5   = 0x05, //  1024/fs     21.3ms
    ML26121_AMP_VOL_FADE_6   = 0x06, //  4096/fs     85.3ms
    ML26121_AMP_VOL_FADE_7   = 0x07, // 16384/fs      341ms
};

enum ml26121_spk_amp_out_ctl
{
    ML26121_SPK_AMP_OUT_DAC_SWITCH     = 0x01,
    ML26121_SPK_AMP_OUT_LOOP_SWITCH    = 0x08,
    ML26121_SPK_AMP_OUT_PGA_SWITCH     = 0x20,
};

enum ml26121_hp_amp_out_ctl
{
    ML26121_HP_AMP_OUT_CTL_LCH_ON     = 0x20,
    ML26121_HP_AMP_OUT_CTL_RCH_ON     = 0x10,
    ML26121_HP_AMP_OUT_CTL_LCH_PGA_ON = 0x02,
    ML26121_HP_AMP_OUT_CTL_RCH_PGA_ON = 0x01,
    ML26121_HP_AMP_OUT_CTL_ALL_ON     = 0x33,
};

enum ml26121_mic_if_ctl
{
    ML26121_MIC_IF_CTL_ANALOG_SINGLE   = 0x00,
    ML26121_MIC_IF_CTL_DIGITAL_SINGLE  = 0x01,
    ML26121_MIC_IF_CTL_ANALOG_DIFFER   = 0x02,
    ML26121_MIC_IF_CTL_DIGITAL_DIFFER  = 0x03,
};

enum ml26121_rch_mixer_input
{
    ML26121_RCH_MIXER_INPUT_SINGLE_HOT  = 0x01,
    ML26121_RCH_MIXER_INPUT_SINGLE_COLD = 0x02,
};

enum ml26121_lch_mixer_input
{
    ML26121_LCH_MIXER_INPUT_SINGLE_COLD = 0x01,
    ML26121_LCH_MIXER_INPUT_SINGLE_HOT  = 0x02,
};

enum ml26121_record_path
{
    ML26121_RECORD_PATH_MICR2LCH_MICL2RCH  = 0x04, // 0b100 /* 0x04 is always on*/
    ML26121_RECORD_PATH_MICL2LCH_MICL2RCH  = 0x05, // 0b101
    ML26121_RECORD_PATH_MICR2LCH_MICR2RCH  = 0x06, // 0b110
    ML26121_RECORD_PATH_MICL2LCH_MICR2RCH  = 0x07, // 0b111
};

enum ml26121_filter_en
{
    ML26121_FILTER_DIS_ALL      = 0x00,
    ML26121_FILTER_EN_HPF1_ONLY = 0x01, // DC cut first-order high pass filter ON
    ML26121_FILTER_EN_HPF2_ONLY = 0x02, // Noise cut second-order high pass filter ON
    ML26121_FILTER_EN_HPF_BOTH  = 0x03, // Both ON
    ML26121_FILTER_EN_EQ0_ON    = 0x04, // Equalizer band 0 ON
    ML26121_FILTER_EN_EQ1_ON    = 0x08, // Equalizer band 1 ON
    ML26121_FILTER_EN_EQ2_ON    = 0x10, // Equalizer band 2 ON
    ML26121_FILTER_EN_EQ3_ON    = 0x20, // Equalizer band 3 ON
    ML26121_FILTER_EN_EQ4_ON    = 0x40, // Equalizer band 4 ON
    ML26121_FILTER_EN_HPF2OD    = 0x80, // first-order high pass filter
};

enum ml26121_dvol_ctl_func_en
{
    ML26121_DVOL_CTL_FUNC_EN_ALL_OFF   = 0x00, /* =all off */
    ML26121_DVOL_CTL_FUNC_EN_ALC_ON    = 0x02, /* =AGC Auto Level Control when recording */
    ML26121_DVOL_CTL_FUNC_EN_MUTE      = 0x10, /* =mute */
    ML26121_DVOL_CTL_FUNC_EN_ALC_FADE  = 0x2c, /* =ALC after all sound proces. + fade ON */
    ML26121_DVOL_CTL_FUNC_EN_ALL       = 0x2f, /* =all ON */
};

enum ml26121_mixer_vol_ctl
{
    ML26121_MIXER_VOL_CTL_LCH_USE_L_ONLY  = 0x00, //0b00xx xx is 0
    ML26121_MIXER_VOL_CTL_LCH_USE_R_ONLY  = 0x01,
    ML26121_MIXER_VOL_CTL_LCH_USE_LR      = 0x02,
    ML26121_MIXER_VOL_CTL_LCH_USE_LR_HALF = 0x03,
    ML26121_MIXER_VOL_CTL_RCH_USE_L_ONLY  = 0x00, //0bxx00 xx is 0
    ML26121_MIXER_VOL_CTL_RCH_USE_R_ONLY  = 0x05,
    ML26121_MIXER_VOL_CTL_RCH_USE_LR      = 0x06,
    ML26121_MIXER_VOL_CTL_RCH_USE_LR_HALF = 0x07,
};

enum ml26121_rec_digi_vol
{
    ML26121_REC_DIGI_VOL_MUTE  = 0x00, //00-6f mute
    ML26121_REC_DIGI_VOL_MIN   = 0x70, //-71.5dB +0.5db step
    ML26121_REC_DIGI_VOL_1     = 0x80, //-63.5dB
    ML26121_REC_DIGI_VOL_2     = 0x90, //-55.5dB
    ML26121_REC_DIGI_VOL_3     = 0xa0, //-47.5dB
    ML26121_REC_DIGI_VOL_4     = 0xb0, //-39.5dB
    ML26121_REC_DIGI_VOL_5     = 0xc0, //-31.5dB
    ML26121_REC_DIGI_VOL_6     = 0xd0, //-23.5dB
    ML26121_REC_DIGI_VOL_7     = 0xe0, //-15.5dB
    ML26121_REC_DIGI_VOL_8     = 0xf0, //-7.5dB
    ML26121_REC_DIGI_VOL_MAX   = 0xff, // 0dB
};

enum ml26121_hpf2_cutoff
{
    ML26121_HPF2_CUTOFF_FREQ80  = 0x00,
    ML26121_HPF2_CUTOFF_FREQ100 = 0x01,
    ML26121_HPF2_CUTOFF_FREQ130 = 0x02,
    ML26121_HPF2_CUTOFF_FREQ160 = 0x03,
    ML26121_HPF2_CUTOFF_FREQ200 = 0x04,
    ML26121_HPF2_CUTOFF_FREQ260 = 0x05,
    ML26121_HPF2_CUTOFF_FREQ320 = 0x06,
    ML26121_HPF2_CUTOFF_FREQ400 = 0x07,
};

enum ml26121_snd_effect_mode
{
    ML26121_SND_EFFECT_MODE_NOTCH           = 0x00,
    ML26121_SND_EFFECT_MODE_EQ              = 0x85,
    ML26121_SND_EFFECT_MODE_NOTCHEQ         = 0x02,
    ML26121_SND_EFFECT_MODE_ENHANCE_REC     = 0x5a,
    ML26121_SND_EFFECT_MODE_ENHANCE_RECPLAY = 0xda,
    ML26121_SND_EFFECT_MODE_LOUD            = 0xbd,
};

enum ml26121_pl_target_level
{
    ML26121_PL_TARGET_LEVEL_MIN = 0x00, // Min -22.5dBFS
    ML26121_PL_TARGET_LEVEL_DEF = 0x0d, // Default -3.0dBFS
    ML26121_PL_TARGET_LEVEL_MAX = 0x0e, // Max -1.5dBFS
};

enum ml26121_pl_maxmin_gain
{
    //MINGAIN
    ML26121_PL_MAXMIN_GAIN_M12 = 0x00, // -12dB
    ML26121_PL_MAXMIN_GAIN_M6  = 0x01, // -6dB
    ML26121_PL_MAXMIN_GAIN_0   = 0x02, // 0
    ML26121_PL_MAXMIN_GAIN_6   = 0x03, // 6dB
    ML26121_PL_MAXMIN_GAIN_12  = 0x04, // 12dB
    ML26121_PL_MAXMIN_GAIN_18  = 0x05, // 18dB
    ML26121_PL_MAXMIN_GAIN_24  = 0x06, // 24dB
    ML26121_PL_MAXMIN_GAIN_30  = 0x07, // 30dB
    //MAXGAIN
    ML26121_PL_MAXMIN_GAIN_M7  = 0x00, // -6.75dB
    ML26121_PL_MAXMIN_GAIN_M1  = 0x10, // -0.75dB
    ML26121_PL_MAXMIN_GAIN_5   = 0x20, // 5.25dB
    ML26121_PL_MAXMIN_GAIN_11  = 0x30, // 11.25dB
    ML26121_PL_MAXMIN_GAIN_17  = 0x40, // 17.25dB
    ML26121_PL_MAXMIN_GAIN_23  = 0x50, // 23.25dB
    ML26121_PL_MAXMIN_GAIN_29  = 0x60, // 29.25dB
    ML26121_PL_MAXMIN_GAIN_35  = 0x70, // 35.25dB
};

enum ml26121_plybak_boost_vol
{
    ML26121_PLYBAK_BOST_VOL_MIN  = 0x00,  // -12dB
    ML26121_PLYBAK_BOST_VOL_DEF  = 0x10,  // default 12dB
    ML26121_PLYBAK_BOST_VOL_MAX  = 0x3f,  // Max +35.25dB
};

enum ml26121_pl_0cross_timeout
{
    ML26121_PL_0CROSS_TIMEOUT_0 = 0x00, //  128/fs
    ML26121_PL_0CROSS_TIMEOUT_1 = 0x01, //  256/fs
    ML26121_PL_0CROSS_TIMEOUT_2 = 0x02, //  512/fs
    ML26121_PL_0CROSS_TIMEOUT_3 = 0x03, // 1024/fs
};

enum ml26121_clock
{
    /* Clock select for machine driver */
    ML26121_USE_PLL          = 0,
    ML26121_USE_MCLKI_256FS  = 1,
    ML26121_USE_MCLKI_512FS  = 2,
    ML26121_USE_MCLKI_1024FS = 3,
};

enum ml26121_mask
{
    /* Register Mask */
    ML26121_R0_MASK  = 0x0f,
    ML26121_R2_MASK  = 0xff,
    ML26121_R4_MASK  = 0x01,
    ML26121_R6_MASK  = 0x0f,
    ML26121_R8_MASK  = 0x3f,
    ML26121_Ra_MASK  = 0x1f,
    ML26121_Rc_MASK  = 0x1f,
    ML26121_Re_MASK  = 0x07,
    ML26121_R10_MASK = 0x01,
    ML26121_R12_MASK = 0x17,
    ML26121_R14_MASK = 0x3f,
    ML26121_R20_MASK = 0x47,
    ML26121_R22_MASK = 0x0a,
    ML26121_R24_MASK = 0x02,
    ML26121_R26_MASK = 0x1f,
    ML26121_R28_MASK = 0x02,
    ML26121_R2a_MASK = 0x02,
    ML26121_R2e_MASK = 0x02,
    ML26121_R30_MASK = 0x07,
    ML26121_R32_MASK = 0x3f,
    ML26121_R38_MASK = 0x38,
    ML26121_R3a_MASK = 0x3f,
    ML26121_R48_MASK = 0x03,
    ML26121_R4a_MASK = 0x07,
    ML26121_R54_MASK = 0x2a,
    ML26121_R5a_MASK = 0x03,
    ML26121_Re8_MASK = 0x03,
    ML26121_R60_MASK = 0xff,
    ML26121_R62_MASK = 0xff,
    ML26121_R64_MASK = 0x01,
    ML26121_R66_MASK = 0xff,
    ML26121_R68_MASK = 0x3b,
    ML26121_R6a_MASK = 0xf3,
    ML26121_R6c_MASK = 0xff,
    ML26121_R70_MASK = 0xff,
};

void ml26121_init(struct codec_ops *ops);

static void ml26121_write_reg(enum ml26121_regs reg);
static void ml26121_write(uint32_t reg, uint32_t value);
static void ml26121_write_masked(uint32_t reg, uint32_t mask, uint32_t value);
static void ml26121_power_speaker();
static void ml26121_power_lineout();
static void ml26121_power_avline();
static void ml26121_set_out_vol(uint32_t volume);
static void ml26121_unpower_out();
static void ml26121_unpower_in();
static void ml26121_set_loop(uint32_t state);

static enum sound_result ml26121_op_set_rate(uint32_t rate);
static enum sound_result ml26121_op_apply_mixer(struct sound_mixer *prev, struct sound_mixer *next);
static enum sound_result ml26121_op_poweron();
static enum sound_result ml26121_op_poweroff();
static const char *ml26121_op_get_destination_name(enum sound_destination line);
static const char *ml26121_op_get_source_name(enum sound_source line);

#endif
