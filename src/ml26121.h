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


//600D Audio write Registers
/* Clock Control Register */
#define ML_SMPLING_RATE			0x0100 /* Sampling Rate */
 #define ML_SMPLING_RATE_8kHz		0x00 /* Sampling Rate */
 #define ML_SMPLING_RATE_11kHz		0x01 /* 11,025 Sampling Rate */
 #define ML_SMPLING_RATE_12kHz		0x02 /* Sampling Rate */
 #define ML_SMPLING_RATE_16kHz		0x03 /* Sampling Rate */
 #define ML_SMPLING_RATE_22kHz		0x04 /* 22,05 Sampling Rate */
 #define ML_SMPLING_RATE_24kHz		0x05 /* Sampling Rate */
 #define ML_SMPLING_RATE_32kHz		0x06 /* Sampling Rate */
 #define ML_SMPLING_RATE_44kHz		0x07 /* 44,1 Sampling Rate */
 #define ML_SMPLING_RATE_48kHz		0x08 /* Sampling Rate */

#define ML_PLLNL				0x0300 /* PLL NL The value can be set from 0x001 to 0x1FF. */
#define ML_PLLNH				0x0500 /* PLL NH The value can be set from 0x001 to 0x1FF. */
#define ML_PLLML				0x0700 /* PLL ML The value can be set from 0x0020 to 0x3FFF. */
#define ML_PLLMH				0x0900 /* MLL MH The value can be set from 0x0020 to 0x3FFF. */
#define ML_PLLDIV				0x0b00 /* PLL DIV The value can be set from 0x01 to 0x1F. */
#define ML_CLK_EN				0x0d00 /* MCLKEN + PLLEN +PLLOE */ /* Clock Enable */
#define ML_CLK_CTL				0x0f00 /* CLK Input/Output Control */

/* System Control Register */
#define ML_SW_RST				0x1100 /* Software RESET */
#define ML_RECPLAY_STATE        0x1300 /* Record/Playback Run */
 #define ML_RECPLAY_STATE_STOP       0x00
 #define ML_RECPLAY_STATE_REC        0x01
 #define ML_RECPLAY_STATE_PLAY       0x02
 #define ML_RECPLAY_STATE_RECPLAY    0x03
 #define ML_RECPLAY_STATE_MON        0x07
 #define ML_RECPLAY_STATE_AUTO_ON    0x10

#define ML_MIC_IN_CHARG_TIM		0x1500 /* This register is to select the wait time for microphone input load charge when starting reording or playback using AutoStart mode. The setting of this register is ignored except Auto Start mode.*/

/* Power Management Register */
#define ML_PW_REF_PW_MNG		0x2100 /* MICBIAS */ /* Reference Power Management */
 #define ML_PW_REF_PW_MNG_ALL_OFF 0x00
 #define ML_PW_REF_PW_HISPEED    0x01  /* 000000xx */
 #define ML_PW_REF_PW_NORMAL     0x02  /* 000000xx */
 #define ML_PW_REF_PW_MICBEN_ON  0x04  /* 00000x00 */
 #define ML_PW_REF_PW_HP_FIRST   0x10  /* 00xx0000 */
 #define ML_PW_REF_PW_HP_STANDARD 0x20 /* 00xx0000 */

#define ML_PW_IN_PW_MNG			0x2300 /* ADC "Capture" + PGA Power Management */
 #define ML_PW_IN_PW_MNG_OFF     0x00 /*  OFF */
 #define ML_PW_IN_PW_MNG_DAC	 0x02 /* ADC "Capture" ON */
 #define ML_PW_IN_PW_MNG_PGA	 0x08 /* PGA ON */
 #define ML_PW_IN_PW_MNG_BOTH	 0x0a /* ADC "Capture" + PGA ON */

#define ML_PW_DAC_PW_MNG		0x2500 /* DAC Power Management */
 #define ML_PW_DAC_PW_MNG_PWROFF	0x00 
 #define ML_PW_DAC_PW_MNG_PWRON	0x02 

#define ML_PW_SPAMP_PW_MNG		0x2700 /* Speaker AMP Power Management */
 #define ML_PW_SPAMP_PW_MNG_P_ON 0x13 /* BTL mode Pre amplifier power ON Can not make output only pre amplifier. Output can be available at 05h or 1Fh. */
 #define ML_PW_SPAMP_PW_MNG_ON   0x1f /* BTL mode Speaker amplifier power ON */
 #define ML_PW_SPAMP_PW_MNG_OFF	 0x00 /* OFF */
 #define ML_PW_SPAMP_PW_MNG_RON	 0x01 /* R Channel HeadPhone amplifier ON */
 #define ML_PW_SPAMP_PW_MNG_LON	 0x04 /* L Channel HeadPhone amplifier ON */
 #define ML_PW_SPAMP_PW_MNG_STR	 0x05 /* Stereo HeadPhone amp ON(HPMID not used) */
 #define ML_PW_SPAMP_PW_MNG_HPM	 0x07 /* Stereo HeadPhone amp ON(HPMID used) */

#define ML_PW_ZCCMP_PW_MNG		0x2f00 /* ZC-CMP Power Management */
 #define ML_PW_ZCCMP_PW_MNG_OFF	 0x00 /* PGA zero cross comparator power off
PGA volume is effective right after setting value change. */
 #define ML_PW_ZCCMP_PW_MNG_ON	 0x01 /* PGA zero cross comparator power on
PGA volume change is applied for zero cross. */


/* Analog Reference Control Register */
#define ML_MICBIAS_VOLT		0x3100 /* MICBIAS Voltage Control */

/* Input/Output Amplifier Control Register */
#define ML_MIC_IN_VOL		0x3300 /* MIC Input Volume */
 #define ML_MIC_IN_VOL_0		0x00 /** -12+0.75*num */
 #define ML_MIC_IN_VOL_1		0x0c /** - 3dB */
 #define ML_MIC_IN_VOL_2		0x10 /**   0dB */
 #define ML_MIC_IN_VOL_3		0x18 /** + 6dB */ 
 #define ML_MIC_IN_VOL_4		0x24 /** +15dB */
 #define ML_MIC_IN_VOL_5		0x30 /** +24dB */
 #define ML_MIC_IN_VOL_6		0x3c /** +33dB */
 #define ML_MIC_IN_VOL_MAX		0x3f /** +35.25dB */

#define ML_MIC_BOOST_VOL1		0x3900 /* Mic Boost Volume 1 */
 #define ML_MIC_BOOST_VOL1_OFF	0x00 // 0
 #define ML_MIC_BOOST_VOL1_1	0x10 // +9.75dB
 #define ML_MIC_BOOST_VOL1_2    0x20 // +19.50dB
 #define ML_MIC_BOOST_VOL1_3	0x30 // +29.25dB (not valid with Boost vol2)

#define ML_MIC_BOOST_VOL2		0xe300 /* Mic Boost Volume 2 */
 #define ML_MIC_BOOST_VOL2_ON	0x01 // increase +4.875dB to boost vol1
 #define ML_MIC_BOOST_VOL2_OFF	0x00 // unchange boost vol1

#define ML_SPK_AMP_VOL		0x3b00 /* Speaker AMP Volume */
 #define ML_SPK_AMP_VOL_MUTE		0x0e
/* definition for spkr volume value, needed?
 #define ML_SPK_AMP_VOL_1		0x0e // -28
 #define ML_SPK_AMP_VOL_2		0x10 // -26
 #define ML_SPK_AMP_VOL_3		0x11 // -24
 #define ML_SPK_AMP_VOL_4		0x12 // -22
 #define ML_SPK_AMP_VOL_5		0x13 // -20
 #define ML_SPK_AMP_VOL_6		0x14 // -18
 #define ML_SPK_AMP_VOL_7		0x15 // -16
 #define ML_SPK_AMP_VOL_8		0x16 // -14
 #define ML_SPK_AMP_VOL_9		0x17 // -12
 #define ML_SPK_AMP_VOL_10		0x18 // -10
 #define ML_SPK_AMP_VOL_11		0x19 // -08
 #define ML_SPK_AMP_VOL_12		0x1a // -07
 #define ML_SPK_AMP_VOL_13		0x1b // -06
 #define ML_SPK_AMP_VOL_14		0x1c // -05
 #define ML_SPK_AMP_VOL_15		0x1d // -04
 #define ML_SPK_AMP_VOL_16		0x1e // -03
 #define ML_SPK_AMP_VOL_17		0x1f // -02
 #define ML_SPK_AMP_VOL_18		0x20 // -01
 #define ML_SPK_AMP_VOL_19		0x21 // 0
 #define ML_SPK_AMP_VOL_20		0x22 // 1
 #define ML_SPK_AMP_VOL_21		0x23 // 2
 #define ML_SPK_AMP_VOL_22		0x24 // 3
 #define ML_SPK_AMP_VOL_23		0x25 // 4
 #define ML_SPK_AMP_VOL_24		0x26 // 5
 #define ML_SPK_AMP_VOL_25		0x27 // 6
 #define ML_SPK_AMP_VOL_26		0x28 // 6,5
 #define ML_SPK_AMP_VOL_27		0x29 // 7
 #define ML_SPK_AMP_VOL_28		0x2a // 7,5
 #define ML_SPK_AMP_VOL_29		0x2b // 8
 #define ML_SPK_AMP_VOL_30		0x2c // 8,5
 #define ML_SPK_AMP_VOL_31      0x2d // 9
 #define ML_SPK_AMP_VOL_32		0x2e // 9,5
 #define ML_SPK_AMP_VOL_33		0x2f // 10
 #define ML_SPK_AMP_VOL_34		0x30 // 10,5
 #define ML_SPK_AMP_VOL_35		0x31 // 11
 #define ML_SPK_AMP_VOL_36		0x32 // 11,5
 #define ML_SPK_AMP_VOL_37		0x33 // 12 <-default?
 #define ML_SPK_AMP_VOL_38		0x34 // 12,5
 #define ML_SPK_AMP_VOL_39		0x35 // 13
 #define ML_SPK_AMP_VOL_40		0x36 // 13,5
 #define ML_SPK_AMP_VOL_41		0x37 // 14
 #define ML_SPK_AMP_VOL_42		0x38 // 14,5
 #define ML_SPK_AMP_VOL_43		0x39 // 15
 #define ML_SPK_AMP_VOL_44		0x3a // 15,5
 #define ML_SPK_AMP_VOL_45		0x3b // 16
 #define ML_SPK_AMP_VOL_46		0x3c // 16,5
 #define ML_SPK_AMP_VOL_47		0x3d // 17
 #define ML_SPK_AMP_VOL_48		0x3e // 17,5
 #define ML_SPK_AMP_VOL_49		0x3f // 18
 */
#define ML_HP_AMP_VOL       0x3f00 /* headphone amp vol control*/
 #define ML_HP_AMP_VOL_MUTE		0x0e // mute
/* definition for headphone volume value, needed?
 #define ML_HP_AMP_VOL_1		0x0f // -40
 #define ML_HP_AMP_VOL_2		0x10 // -38
 #define ML_HP_AMP_VOL_3		0x11 // -36
 #define ML_HP_AMP_VOL_4		0x12 // -34
 #define ML_HP_AMP_VOL_5		0x13 // -32
 #define ML_HP_AMP_VOL_6		0x14 // -30
 #define ML_HP_AMP_VOL_7		0x15 // -28
 #define ML_HP_AMP_VOL_8		0x16 // -26
 #define ML_HP_AMP_VOL_9		0x17 // -24
 #define ML_HP_AMP_VOL_10		0x18 // -22
 #define ML_HP_AMP_VOL_11		0x19 // -20
 #define ML_HP_AMP_VOL_12		0x1a // -19
 #define ML_HP_AMP_VOL_13		0x1b // -18
 #define ML_HP_AMP_VOL_14		0x1c // -17
 #define ML_HP_AMP_VOL_15		0x1d // -16
 #define ML_HP_AMP_VOL_16		0x1e // -15
 #define ML_HP_AMP_VOL_17		0x1f // -14
 #define ML_HP_AMP_VOL_18		0x20 // -13
 #define ML_HP_AMP_VOL_19		0x21 // -12
 #define ML_HP_AMP_VOL_20		0x22 // -11
 #define ML_HP_AMP_VOL_21		0x23 // -10
 #define ML_HP_AMP_VOL_22		0x24 // -9
 #define ML_HP_AMP_VOL_23		0x25 // -8
 #define ML_HP_AMP_VOL_24		0x26 // -7
 #define ML_HP_AMP_VOL_25		0x27 // -6
 #define ML_HP_AMP_VOL_26		0x28 // -5,5
 #define ML_HP_AMP_VOL_27		0x29 // 5
 #define ML_HP_AMP_VOL_28		0x2a // 4,5
 #define ML_HP_AMP_VOL_29		0x2b // -4
 #define ML_HP_AMP_VOL_30		0x2c // -3,5
 #define ML_HP_AMP_VOL_31       0x2d // -3
 #define ML_HP_AMP_VOL_32		0x2e // -2,5
 #define ML_HP_AMP_VOL_33		0x2f // -2
 #define ML_HP_AMP_VOL_34		0x30 // -1,5
 #define ML_HP_AMP_VOL_35		0x31 // -1
 #define ML_HP_AMP_VOL_36		0x32 // -0,5
 #define ML_HP_AMP_VOL_37		0x33 // 0 <-default?
 #define ML_HP_AMP_VOL_38		0x34 // 0,5
 #define ML_HP_AMP_VOL_39		0x35 // 1
 #define ML_HP_AMP_VOL_40		0x36 // 1,5
 #define ML_HP_AMP_VOL_41		0x37 // 2
 #define ML_HP_AMP_VOL_42		0x38 // 2,5
 #define ML_HP_AMP_VOL_43		0x39 // 3
 #define ML_HP_AMP_VOL_44		0x3a // 3,5
 #define ML_HP_AMP_VOL_45		0x3b // 4
 #define ML_HP_AMP_VOL_46		0x3c // 4,5
 #define ML_HP_AMP_VOL_47		0x3d // 5
 #define ML_HP_AMP_VOL_48		0x3e // 5,5
 #define ML_HP_AMP_VOL_49		0x3f // 6
*/

#define ML_AMP_VOLFUNC_ENA	0x4900 /* AMP Volume Control Function Enable */
 #define ML_AMP_VOLFUNC_ENA_FADE_ON	0x01 
 #define ML_AMP_VOLFUNC_ENA_AVMUTE	0x02 

#define ML_AMP_VOL_FADE		0x4b00 /* Amplifier Volume Fader Control */
 #define ML_AMP_VOL_FADE_0		0x00 //    1/fs ￼	20.8us
 #define ML_AMP_VOL_FADE_1		0x01 //     4/fs ￼	83.3us
 #define ML_AMP_VOL_FADE_2		0x02 //    16/fs ￼	 333us
 #define ML_AMP_VOL_FADE_3		0x03 //    64/fs ￼	1.33ms
 #define ML_AMP_VOL_FADE_4		0x04 //   256/fs ￼	5.33ms
 #define ML_AMP_VOL_FADE_5		0x05 //  1024/fs ￼	21.3ms
 #define ML_AMP_VOL_FADE_6		0x06 //  4096/fs ￼	85.3ms
 #define ML_AMP_VOL_FADE_7		0x07 // 16384/fs 	 341ms

/* Analog Path Control Register */
#define ML_SPK_AMP_OUT		0x5500 /* DAC Switch + Line in loopback Switch + PGA Switch */ /* Speaker AMP Output Control */
#define ML_HP_AMP_OUT_CTL       0x5700 /* headphone amp control*/
 #define ML_HP_AMP_OUT_CTL_LCH_ON       0x20
 #define ML_HP_AMP_OUT_CTL_RCH_ON       0x10
 #define ML_HP_AMP_OUT_CTL_LCH_PGA_ON   0x02
 #define ML_HP_AMP_OUT_CTL_RCH_PGA_ON   0x01
 #define ML_HP_AMP_OUT_CTL_ALL_ON   0x33

#define ML_MIC_IF_CTL		0x5b00 /* Mic IF Control */
 #define ML_MIC_IF_CTL_ANALOG_SINGLE  0x00
 #define ML_MIC_IF_CTL_ANALOG_DIFFER  0x02
 #define ML_MIC_IF_CTL_DIGITAL_SINGLE 0x01
 //#define ML_MIC_IF_CTL_DIGITAL_DIFFER 0x03 "This register is to select the usage of analog microphone input interface(MIN)" so I suspect this will not work...

#define ML_RCH_MIXER_INPUT	0xe100 /* R-ch mic select. PGA control for MINR*/
 #define ML_RCH_MIXER_INPUT_SINGLE_HOT      0x01
 #define ML_RCH_MIXER_INPUT_SINGLE_COLD     0x02
// only 2bit. If you want to use differential, combine ML_MIC_IF_CTL MICDIF bit

#define ML_LCH_MIXER_INPUT	0xe900 /* L-ch mic select. PGA control for MINL */
 #define ML_LCH_MIXER_INPUT_SINGLE_COLD     0x01
 #define ML_LCH_MIXER_INPUT_SINGLE_HOT      0x02
// only 2bit

#define ML_RECORD_PATH      0xe500 /* analog record path */
 /* 0x04 is always on*/
 #define ML_RECORD_PATH_MICR2LCH_MICL2RCH 0x04 // 0b100
 #define ML_RECORD_PATH_MICL2LCH_MICL2RCH 0x05 // 0b101
 #define ML_RECORD_PATH_MICR2LCH_MICR2RCH 0x06 // 0b110
 #define ML_RECORD_PATH_MICL2LCH_MICR2RCH 0x07 // 0b111

/* Audio Interface Control Register -DAC/ADC */
#define ML_SAI_TRANS_CTL		0x6100 /* SAI-Trans Control */
#define ML_SAI_RCV_CTL			0x6300 /* SAI-Receive Control */
#define ML_SAI_MODE_SEL			0x6500 /* SAI Mode select */

/* DSP Control Register */
#define ML_FILTER_EN			0x6700 /* Filter Func Enable */ /*DC High Pass Filter Switch+ Noise High Pass Filter Switch + EQ Band0/1/2/3/4 Switch  V: '01' '00' */
 /* not sure please check... */
 #define ML_FILTER_EN_HPF1_ONLY		0x01 // DC cut first-order high pass filter ON
 #define ML_FILTER_EN_HPF2_ONLY		0x02 // Noise cut second-order high pass filter ON
 #define ML_FILTER_EN_HPF_BOTH		0x03 // Both ON
 #define ML_FILTER_EN_EQ0_ON        0x04 // Equalizer band 0 ON
 #define ML_FILTER_EN_EQ1_ON        0x08 // Equalizer band 1 ON
 #define ML_FILTER_EN_EQ2_ON        0x10 // Equalizer band 2 ON
 #define ML_FILTER_EN_EQ3_ON        0x20 // Equalizer band 3 ON
 #define ML_FILTER_EN_EQ4_ON        0x40 // Equalizer band 4 ON
 #define ML_FILTER_EN_HPF2OD        0x80 // first-order high pass filter

#define ML_FILTER_DIS_ALL			0x00

#define ML_DVOL_CTL_FUNC_EN				0x6900 /* Digital Volume Control Func Enable */ /* Play Limiter + Capture Limiter + Digital Volume Fade Switch +Digital Switch */ 
 #define ML_DVOL_CTL_FUNC_EN_ALL_OFF	0x00 /* =all off */
 #define ML_DVOL_CTL_FUNC_EN_ALC_ON		0x02 /* =AGC Auto Level Control when recording */
 #define ML_DVOL_CTL_FUNC_EN_MUTE		0x10 /* =mute */
 #define ML_DVOL_CTL_FUNC_EN_ALC_FADE   0x2c /* =ALC after all sound proces. + fade ON */
 #define ML_DVOL_CTL_FUNC_EN_ALL        0x2f /* =all ON */

#define ML_MIXER_VOL_CTL		0x6b00 /* Mixer & Volume Control*/
 #define ML_MIXER_VOL_CTL_LCH_USE_L_ONLY	0x00 //0b00xx xx is 0
 #define ML_MIXER_VOL_CTL_LCH_USE_R_ONLY	0x01
 #define ML_MIXER_VOL_CTL_LCH_USE_LR 		0x02
 #define ML_MIXER_VOL_CTL_LCH_USE_LR_HALF	0x03
 #define ML_MIXER_VOL_CTL_RCH_USE_L_ONLY	0x00 //0bxx00 xx is 0
 #define ML_MIXER_VOL_CTL_RCH_USE_R_ONLY	0x05
 #define ML_MIXER_VOL_CTL_RCH_USE_LR 		0x06
 #define ML_MIXER_VOL_CTL_RCH_USE_LR_HALF	0x07

#define ML_REC_DIGI_VOL		0x6d00 /* Capture/Record Digital Volume */
 #define ML_REC_DIGI_VOL_MUTE	0x00 //00-6f mute
 #define ML_REC_DIGI_VOL_MIN	0x70 //-71.5dB +0.5db step
 #define ML_REC_DIGI_VOL_1		0x80 //-63.5dB
 #define ML_REC_DIGI_VOL_2		0x90 //-55.5dB
 #define ML_REC_DIGI_VOL_3		0xa0 //-47.5dB
 #define ML_REC_DIGI_VOL_4		0xb0 //-39.5dB
 #define ML_REC_DIGI_VOL_5		0xc0 //-31.5dB
 #define ML_REC_DIGI_VOL_6		0xd0 //-23.5dB
 #define ML_REC_DIGI_VOL_7		0xe0 //-15.5dB
 #define ML_REC_DIGI_VOL_8		0xf0 //-7.5dB
 #define ML_REC_DIGI_VOL_MAX	0xFF // 0dB

#define ML_REC_LR_BAL_VOL	    0x6f00 /*0x0-0xf Rvol, 0x00-0f0 Lvol*/
#define ML_PLAY_DIG_VOL  		0x7100 /* Playback Digital Volume */
#define ML_EQ_GAIN_BRAND0		0x7500 /* EQ Band0 Volume */
#define ML_EQ_GAIN_BRAND1		0x7700 /* EQ Band1 Volume */
#define ML_EQ_GAIN_BRAND2		0x7900 /* EQ Band2 Volume */
#define ML_EQ_GAIN_BRAND3		0x7b00 /* EQ Band3 Volume */
#define ML_EQ_GAIN_BRAND4		0x7d00 /* EQ Band4 Volume */
#define ML_HPF2_CUTOFF          0x7f00 /* HPF2 CutOff*/
 #define ML_HPF2_CUTOFF_FREQ80		0x00
 #define ML_HPF2_CUTOFF_FREQ100		0x01
 #define ML_HPF2_CUTOFF_FREQ130		0x02
 #define ML_HPF2_CUTOFF_FREQ160		0x03
 #define ML_HPF2_CUTOFF_FREQ200		0x04
 #define ML_HPF2_CUTOFF_FREQ260		0x05
 #define ML_HPF2_CUTOFF_FREQ320		0x06
 #define ML_HPF2_CUTOFF_FREQ400		0x07

#define ML_EQBRAND0_F0L		0x8100 /* EQ Band0 Coef0L */
#define ML_EQBRAND0_F0H		0x8300
#define ML_EQBRAND0_F1L		0x8500
#define ML_EQBRAND0_F1H		0x8700
#define ML_EQBRAND1_F0L		0x8900
#define ML_EQBRAND1_F0H		0x8b00
#define ML_EQBRAND1_F1L		0x8d00
#define ML_EQBRAND1_F1H		0x8f00
#define ML_EQBRAND2_F0L		0x9100
#define ML_EQBRAND2_F0H		0x9300
#define ML_EQBRAND2_F1L		0x9500
#define ML_EQBRAND2_F1H		0x9700
#define ML_EQBRAND3_F0L		0x9900
#define ML_EQBRAND3_F0H		0x9b00
#define ML_EQBRAND3_F1L		0x9d00
#define ML_EQBRAND3_F1H		0x9f00
#define ML_EQBRAND4_F0L		0xa100
#define ML_EQBRAND4_F0H		0xa300
#define ML_EQBRAND4_F1L		0xa500
#define ML_EQBRAND4_F1H		0xa700

#define ML_MIC_PARAM10			0xa900 /* MC parameter10 */
#define ML_MIC_PARAM11			0xab00 /* MC parameter11 */
#define ML_SND_EFFECT_MODE		0xad00 /* Sound Effect Mode */
 #define ML_SND_EFFECT_MODE_NOTCH	0x00
 #define ML_SND_EFFECT_MODE_EQ		0x85
 #define ML_SND_EFFECT_MODE_NOTCHEQ	0x02
 #define ML_SND_EFFECT_MODE_ENHANCE_REC	0x5a
 #define ML_SND_EFFECT_MODE_ENHANCE_RECPLAY	0xda
 #define ML_SND_EFFECT_MODE_LOUD	0xbd


/* ALC Control Register */
#define ML_ALC_MODE				0xb100 /* ALC Mode */
#define ML_ALC_ATTACK_TIM		0xb300 /* ALC Attack Time */
#define ML_ALC_DECAY_TIM		0xb500 /* ALC Decay Time */
#define ML_ALC_HOLD_TIM			0xb700 /* ALC Hold Time */
#define ML_ALC_TARGET_LEV		0xb900 /* ALC Target Level */
#define ML_ALC_MAXMIN_GAIN		0xbb00 /* ALC Min/Max Input Volume/Gain 2 sets of bits */
#define ML_NOIS_GATE_THRSH		0xbd00 /* Noise Gate Threshold */
#define ML_ALC_ZERO_TIMOUT		0xbf00 /* ALC ZeroCross TimeOut */

/* Playback Limiter Control Register */
#define ML_PL_ATTACKTIME		0xc100 /* PL Attack Time */
#define ML_PL_DECAYTIME			0xc300 /* PL Decay Time */
//#define ML_PL_TARGETTIME		0xc500 /* PL Target Level */
#define ML_PL_TARGET_LEVEL		0xc500 /* PL Target Level */
 #define ML_PL_TARGET_LEVEL_MIN	0x00 // Min -22.5dBFS
 #define ML_PL_TARGET_LEVEL_DEF	0x0d // Default -3.0dBFS
 #define ML_PL_TARGET_LEVEL_MAX	0x0e // Max -1.5dBFS

#define ML_PL_MAXMIN_GAIN		0xc700 /* Playback Limiter Min/max Input Volume/Gain 2 sets */
//MINGAIN
 #define ML_PL_MAXMIN_GAIN_M12	0x00 // -12dB
 #define ML_PL_MAXMIN_GAIN_M6	0x01 // -6dB
 #define ML_PL_MAXMIN_GAIN_0     0x02 // 0
 #define ML_PL_MAXMIN_GAIN_6     0x03 // 6dB
 #define ML_PL_MAXMIN_GAIN_12	0x04 // 12dB
 #define ML_PL_MAXMIN_GAIN_18    0x05 // 18dB
 #define ML_PL_MAXMIN_GAIN_24	0x06 // 24dB
 #define ML_PL_MAXMIN_GAIN_30	0x07 // 30dB
//MAXGAIN
 #define ML_PL_MAXMIN_GAIN_M7	0x00 // -6.75dB
 #define ML_PL_MAXMIN_GAIN_M1	0x10 // -0.75dB
 #define ML_PL_MAXMIN_GAIN_5     0x20 // 5.25dB
 #define ML_PL_MAXMIN_GAIN_11	0x30 // 11.25dB 
 #define ML_PL_MAXMIN_GAIN_17	0x40 // 17.25dB
 #define ML_PL_MAXMIN_GAIN_23    0x50 // 23.25dB
 #define ML_PL_MAXMIN_GAIN_29	0x60 // 29.25dB
 #define ML_PL_MAXMIN_GAIN_35	0x70 // 35.25dB

#define ML_PLYBAK_BOST_VOL		0xc900 /* Playback Boost Volume */
 #define ML_PLYBAK_BOST_VOL_MIN	0x00  // -12dB
 #define ML_PLYBAK_BOST_VOL_DEF	0x10  // default 12dB
 #define ML_PLYBAK_BOST_VOL_MAX	0x3f  // Max +35.25dB
#define ML_PL_0CROSS_TIMEOUT	0xcb00 /* PL ZeroCross TimeOut */
 #define ML_PL_0CROSS_TIMEOUT_0	0x00 //  128/fs
 #define ML_PL_0CROSS_TIMEOUT_1	0x01 //  256/fs
 #define ML_PL_0CROSS_TIMEOUT_2	0x02 //  512/fs
 #define ML_PL_0CROSS_TIMEOUT_3 0x03 // 1024/fs


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
    ML26121_SMPLING_RATE           = 0x00,
    ML26121_PLLNL                  = 0x02,
    ML26121_PLLNH                  = 0x04,
    ML26121_PLLML                  = 0x06,
    ML26121_PLLMH                  = 0x08,
    ML26121_PLLDIV                 = 0x0a,
    ML26121_CLK_EN                 = 0x0c,
    ML26121_CLK_CTL                = 0x0e,

    /* System Control Register */
    ML26121_SW_RST                 = 0x10,
    ML26121_REC_PLYBAK_RUN         = 0x12,
    ML26121_MIC_TIM                = 0x14,

    /* Power Mnagement Register */
    ML26121_PW_REF_PW_MNG          = 0x20,
    ML26121_PW_IN_PW_MNG           = 0x22,
    ML26121_PW_DAC_PW_MNG          = 0x24,
    ML26121_PW_SPAMP_PW_MNG        = 0x26,
    ML26121_PW_LOUT_PW_MNG         = 0x28,
    ML26121_PW_VOUT_PW_MNG         = 0x2a,
    ML26121_PW_ZCCMP_PW_MNG        = 0x2e,

    /* Analog Reference Control Register */
    ML26121_PW_MICBIAS_VOL         = 0x30,

    /* Input/Output Amplifier Control Register */
    ML26121_PW_MIC_IN_VOL          = 0x32,
    ML26121_PW_MIC_BOST_VOL        = 0x38,
    ML26121_PW_SPK_AMP_VOL         = 0x3a,
    ML26121_PW_AMP_VOL_FUNC        = 0x48,
    ML26121_PW_AMP_VOL_FADE        = 0x4a,

    /* Analog Path Control Register */
    ML26121_SPK_AMP_OUT            = 0x54,
    ML26121_MIC_IF_CTL             = 0x5a,
    ML26121_MIC_SELECT             = 0xe8,

    /* Audio Interface Control Register */
    ML26121_SAI_TRANS_CTL          = 0x60,
    ML26121_SAI_RCV_CTL            = 0x62,
    ML26121_SAI_MODE_SEL           = 0x64,

    /* DSP Control Register */
    ML26121_FILTER_EN              = 0x66,
    ML26121_DVOL_CTL               = 0x68,
    ML26121_MIXER_VOL_CTL          = 0x6a,
    ML26121_RECORD_DIG_VOL         = 0x6c,
    ML26121_PLBAK_DIG_VOL          = 0x70,
    ML26121_DIGI_BOOST_VOL         = 0x72,
    ML26121_EQ_GAIN_BRAND0         = 0x74,
    ML26121_EQ_GAIN_BRAND1         = 0x76,
    ML26121_EQ_GAIN_BRAND2         = 0x78,
    ML26121_EQ_GAIN_BRAND3         = 0x7a,
    ML26121_EQ_GAIN_BRAND4         = 0x7c,
    ML26121_HPF2_CUTOFF            = 0x7e,
    ML26121_EQBRAND0_F0L           = 0x80,
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

    /* ALC Control Register */
    ML26121_ALC_MODE               = 0xb0,
    ML26121_ALC_ATTACK_TIM         = 0xb2,
    ML26121_ALC_DECAY_TIM          = 0xb4,
    ML26121_ALC_HOLD_TIM           = 0xb6,
    ML26121_ALC_TARGET_LEV         = 0xb8,
    ML26121_ALC_MAXMIN_GAIN        = 0xba,
    ML26121_NOIS_GATE_THRSH        = 0xbc,
    ML26121_ALC_ZERO_TIMOUT        = 0xbe,

    /* Playback Limiter Control Register */
    ML26121_PL_ATTACKTIME          = 0xc0,
    ML26121_PL_DECAYTIME           = 0xc2,
    ML26121_PL_TARGETTIME          = 0xc4,
    ML26121_PL_MAXMIN_GAIN         = 0xc6,
    ML26121_PLYBAK_BOST_VOL        = 0xc8,
    ML26121_PL_0CROSS_TIMOUT       = 0xca,

    /* Video Amplifer Control Register */
    ML26121_VIDEO_AMP_GAIN_CTL     = 0xd0,
    ML26121_VIDEO_AMP_SETUP1       = 0xd2,
    ML26121_VIDEO_AMP_CTL2         = 0xd4,
};



enum ml26121_clock
{
    /* Clock select for machine driver */
    ML26121_USE_PLL             = 0,
    ML26121_USE_MCLKI_256FS     = 1,
    ML26121_USE_MCLKI_512FS     = 2,
    ML26121_USE_MCLKI_1024FS    = 3,
};

enum ml26121_mask
{
    /* Register Mask */
    ML26121_R0_MASK        = 0x0f,
    ML26121_R2_MASK        = 0xff,
    ML26121_R4_MASK        = 0x01,
    ML26121_R6_MASK        = 0x0f,
    ML26121_R8_MASK        = 0x3f,
    ML26121_Ra_MASK        = 0x1f,
    ML26121_Rc_MASK        = 0x1f,
    ML26121_Re_MASK        = 0x07,
    ML26121_R10_MASK       = 0x01,
    ML26121_R12_MASK       = 0x17,
    ML26121_R14_MASK       = 0x3f,
    ML26121_R20_MASK       = 0x47,
    ML26121_R22_MASK       = 0x0a,
    ML26121_R24_MASK       = 0x02,
    ML26121_R26_MASK       = 0x1f,
    ML26121_R28_MASK       = 0x02,
    ML26121_R2a_MASK       = 0x02,
    ML26121_R2e_MASK       = 0x02,
    ML26121_R30_MASK       = 0x07,
    ML26121_R32_MASK       = 0x3f,
    ML26121_R38_MASK       = 0x38,
    ML26121_R3a_MASK       = 0x3f,
    ML26121_R48_MASK       = 0x03,
    ML26121_R4a_MASK       = 0x07,
    ML26121_R54_MASK       = 0x2a,
    ML26121_R5a_MASK       = 0x03,
    ML26121_Re8_MASK       = 0x03,
    ML26121_R60_MASK       = 0xff,
    ML26121_R62_MASK       = 0xff,
    ML26121_R64_MASK       = 0x01,
    ML26121_R66_MASK       = 0xff,
    ML26121_R68_MASK       = 0x3b,
    ML26121_R6a_MASK       = 0xf3,
    ML26121_R6c_MASK       = 0xff,
    ML26121_R70_MASK       = 0xff,
};

void ml26121_init(struct codec_ops *ops);

/* call this with e.g. AK4646_PAR_PMDAC and the value you want it to be set */
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
