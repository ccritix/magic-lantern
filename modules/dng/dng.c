/*
 * Copyright (C) 2014 David Milligan
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef _dng_c_
#define _dng_c_

#include <module.h>
#include <dryos.h>
#include <string.h>
#include <math.h>
#include <raw.h>
#include <lens.h>
#include <property.h>
#include <fps.h>

#include "dng_tag_codes.h"
#include "dng_tag_types.h"
#include "dng_tag_values.h"

#include "dng.h"

extern int WEAK_FUNC(ret_1) PROPAD_GetPropertyData(uint32_t property, void** addr, size_t* len);

#define IFD0_COUNT 38
#define SOFTWARE_NAME "Magic Lantern"
#define EXIF_IFD_COUNT 8
#define PACK(a) (((uint16_t)a[1] << 16) | ((uint16_t)a[0]))
#define PACK2(a,b) (((uint16_t)b << 16) | ((uint16_t)a))
#define STRING_ENTRY(a,b,c) (uint32_t)(strlen(a) + 1), add_string(a, b, c)
#define RATIONAL_ENTRY(a,b,c,d) (d/2), add_array(a, b, c, d)
#define RATIONAL_ENTRY2(a,b,c,d) 1, add_rational(a, b, c, d)
#define ARRAY_ENTRY(a,b,c,d) d, add_array(a, b, c, d)
#define HEADER_SIZE 65536
#define COUNT(x) ((int)(sizeof(x)/sizeof((x)[0])))

struct cam_matrices {
    char * camera;
    int32_t ColorMatrix1[18];
    int32_t ColorMatrix2[18];
    int32_t ForwardMatrix1[18];
    int32_t ForwardMatrix2[18];
};

//credits to Andy600 for gleaning these from Adobe DNG converter
static const struct cam_matrices cam_matrices[] =
{
    {
        "Canon EOS 5D Mark III",
        { 7234, 10000, -1413, 10000, -600, 10000, -3631, 10000, 11150, 10000, 2850, 10000, -382, 10000, 1335, 10000, 6437, 10000 },
        { 6722, 10000, -635, 10000, -963, 10000, -4287, 10000, 12460, 10000, 2028, 10000, -908, 10000, 2162, 10000, 5668, 10000 },
        { 7868, 10000, 92, 10000, 1683, 10000, 2291, 10000, 8615, 10000, -906, 10000, 27, 10000, -4752, 10000, 12976, 10000 },
        { 7637, 10000, 805, 10000, 1201, 10000, 2649, 10000, 9179, 10000, -1828, 10000, 137, 10000, -2456, 10000, 10570, 10000 }
    },
    {
        "Canon EOS 5D Mark II",
        { 5309, 10000, -229, 10000, -336, 10000, -6241, 10000, 13265, 10000, 3337, 10000, -817, 10000, 1215, 10000, 6664, 10000 },
        { 4716, 10000, 603, 10000, -830, 10000, -7798, 10000, 15474, 10000, 2480, 10000, -1496, 10000, 1937, 10000, 6651, 10000 },
        { 8924, 10000, -1041, 10000, 1760, 10000, 4351, 10000, 6621, 10000, -972, 10000, 505, 10000, -1562, 10000, 9308, 10000 },
        { 8924, 10000, -1041, 10000, 1760, 10000, 4351, 10000, 6621, 10000, -972, 10000, 505, 10000, -1562, 10000, 9308, 10000 }
    },
    {
        "Canon EOS 7D",
        { 11620, 10000, -6350, 10000, 5, 10000, -2558, 10000, 10146, 10000, 2813, 10000, 24, 10000, 858, 10000, 6926, 10000 },
        { 6844, 10000, -996, 10000, -856, 10000, -3876, 10000, 11761, 10000, 2396, 10000, -593, 10000, 1772, 10000, 6198, 10000 },
        { 5445, 10000, 3536, 10000, 662, 10000, 1106, 10000, 10136, 10000, -1242, 10000, -374, 10000, -3559, 10000, 12184, 10000 },
        { 7415, 10000, 1533, 10000, 695, 10000, 2499, 10000, 9997, 10000, -2497, 10000, -22, 10000, -1933, 10000, 10207, 10000 }
    },
    {
        "Canon EOS 6D",
        { 7546, 10000, -1435, 10000, -929, 10000, -3846, 10000, 11488, 10000, 2692, 10000, -332, 10000, 1209, 10000, 6370, 10000 },
        { 7034, 10000, -804, 10000, -1014, 10000, -4420, 10000, 12564, 10000, 2058, 10000, -851, 10000, 1994, 10000, 5758, 10000 },
        { 7763, 10000, 65, 10000, 1815, 10000, 2364, 10000, 8351, 10000, -715, 10000, -59, 10000, -4228, 10000, 12538, 10000 },
        { 7464, 10000, 1044, 10000, 1135, 10000, 2648, 10000, 9173, 10000, -1820, 10000, 113, 10000, -2154, 10000, 10292, 10000 }
    },
    {
        "Canon EOS 60D",
        { 7428, 10000, -1897, 10000, -491, 10000, -3505, 10000, 10963, 10000, 2929, 10000, -337, 10000, 1242, 10000, 6413, 10000 },
        { 6719, 10000, -994, 10000, -925, 10000, -4408, 10000, 12426, 10000, 2211, 10000, -887, 10000, 2129, 10000, 6051, 10000 },
        { 7550, 10000, 645, 10000, 1448, 10000, 2138, 10000, 8936, 10000, -1075, 10000, -5, 10000, -4306, 10000, 12562, 10000 },
        { 7286, 10000, 1385, 10000, 972, 10000, 2600, 10000, 9468, 10000, -2068, 10000, 93, 10000, -2268, 10000, 10426, 10000 }
    },
    {
        "Canon EOS 50D",
        { 5852, 10000, -578, 10000, -41, 10000, -4691, 10000, 11696, 10000, 3427, 10000, -886, 10000, 2323, 10000, 6879, 10000 },
        { 4920, 10000, 616, 10000, -593, 10000, -6493, 10000, 13964, 10000, 2784, 10000, -1774, 10000, 3178, 10000, 7005, 10000 },
        { 8716, 10000, -692, 10000, 1618, 10000, 3408, 10000, 8077, 10000, -1486, 10000, -13, 10000, -6583, 10000, 14847, 10000 },
        { 9485, 10000, -1150, 10000, 1308, 10000, 4313, 10000, 7807, 10000, -2120, 10000, 293, 10000, -2826, 10000, 10785, 10000 }
    },
    {
        "Canon EOS 550D",
        { 7755, 10000, -2449, 10000, -349, 10000, -3106, 10000, 10222, 10000, 3362, 10000, -156, 10000, 986, 10000, 6409, 10000 },
        { 6941, 10000, -1164, 10000, -857, 10000, -3825, 10000, 11597, 10000, 2534, 10000, -416, 10000, 1540, 10000, 6039, 10000 },
        { 7163, 10000, 1301, 10000, 1179, 10000, 1926, 10000, 9543, 10000, -1469, 10000, -278, 10000, -3830, 10000, 12359, 10000 },
        { 7239, 10000, 1838, 10000, 566, 10000, 2467, 10000, 10246, 10000, -2713, 10000, -112, 10000, -1754, 10000, 10117, 10000 }
        
    },
    {
        "Canon EOS 600D",
        { 7164, 10000, -1916, 10000, -431, 10000, -3361, 10000, 10600, 10000, 3200, 10000, -272, 10000, 1058, 10000, 6442, 10000 },
        { 6461, 10000, -907, 10000, -882, 10000, -4300, 10000, 12184, 10000, 2378, 10000, -819, 10000, 1944, 10000, 5931, 10000 },
        { 7486, 10000, 835, 10000, 1322, 10000, 2099, 10000, 9147, 10000, -1245, 10000, -12, 10000, -3822, 10000, 12085, 10000 },
        { 7359, 10000, 1365, 10000, 918, 10000, 2610, 10000, 9687, 10000, -2297, 10000, 98, 10000, -2155, 10000, 10309, 10000 }
        
    },
    {
        "Canon EOS 650D",
        { 6985, 10000, -1611, 10000, -397, 10000, -3596, 10000, 10749, 10000, 3295, 10000, -349, 10000, 1136, 10000, 6512, 10000 },
        { 6602, 10000, -841, 10000, -939, 10000, -4472, 10000, 12458, 10000, 2247, 10000, -975, 10000, 2039, 10000, 6148, 10000 },
        { 7747, 10000, 485, 10000, 1411, 10000, 2340, 10000, 8840, 10000, -1180, 10000, 105, 10000, -4147, 10000, 12293, 10000 },
        { 7397, 10000, 1199, 10000, 1047, 10000, 2650, 10000, 9355, 10000, -2005, 10000, 193, 10000, -2113, 10000, 10171, 10000 }
        
    },
    {
        "Canon EOS 700D",
        { 6985, 10000, -1611, 10000, -397, 10000, -3596, 10000, 10749, 10000, 3295, 10000, -349, 10000, 1136, 10000, 6512, 10000 },
        { 6602, 10000, -841, 10000, -939, 10000, -4472, 10000, 12458, 10000, 2247, 10000, -975, 10000, 2039, 10000, 6148, 10000 },
        { 7747, 10000, 485, 10000, 1411, 10000, 2340, 10000, 8840, 10000, -1180, 10000, 105, 10000, -4147, 10000, 12293, 10000 },
        { 7397, 10000, 1199, 10000, 1047, 10000, 2650, 10000, 9355, 10000, -2005, 10000, 193, 10000, -2113, 10000, 10171, 10000 }
        
    },
    {
        "Canon EOS 1100D",
        { 6873, 10000, -1696, 10000, -529, 10000, -3659, 10000, 10795, 10000, 3313, 10000, -362, 10000, 1165, 10000, 7234, 10000 },
        { 6444, 10000, -904, 10000, -893, 10000, -4563, 10000, 12308, 10000, 2535, 10000, -903, 10000, 2016, 10000, 6728, 10000 },
        { 7607, 10000, 647, 10000, 1389, 10000, 2337, 10000, 8876, 10000, -1213, 10000, 93, 10000, -3625, 10000, 11783, 10000 },
        { 7357, 10000, 1377, 10000, 909, 10000, 2729, 10000, 9630, 10000, -2359, 10000, 104, 10000, -1940, 10000, 10087, 10000 }
        
    },
    {
        "Canon EOS M",
        { 7357, 10000, 1377, 10000, 909, 10000, 2729, 10000, 9630, 10000, -2359, 10000, 104, 10000, -1940, 10000, 10087, 10000 },
        { 6602, 10000, -841, 10000, -939, 10000, -4472, 10000, 12458, 10000, 2247, 10000, -975, 10000, 2039, 10000, 6148, 10000 },
        { 7747, 10000, 485, 10000, 1411, 10000, 2340, 10000, 8840, 10000, -1180, 10000, 105, 10000, -4147, 10000, 12293, 10000 },
        { 7397, 10000, 1199, 10000, 1047, 10000, 2650, 10000, 9355, 10000, -2005, 10000, 193, 10000, -2113, 10000, 10171, 10000 }
    }
};


/*****************************************************************************************************
 * Kelvin/Green to RGB Multipliers from UFRAW
 *****************************************************************************************************/

#define COLORS 3

/* Convert between Temperature and RGB.
 * Base on information from http://www.brucelindbloom.com/
 * The fit for D-illuminant between 4000K and 23000K are from CIE
 * The generalization to 2000K < T < 4000K and the blackbody fits
 * are my own and should be taken with a grain of salt.
 */
static const double XYZ_to_RGB[3][3] = {
    { 3.24071,	-0.969258,  0.0556352 },
    { -1.53726,	1.87599,    -0.203996 },
    { -0.498571,	0.0415557,  1.05707 }
};

static const double xyz_rgb[3][3] = {
    { 0.412453, 0.357580, 0.180423 },
    { 0.212671, 0.715160, 0.072169 },
    { 0.019334, 0.119193, 0.950227 }
};

static inline void temperature_to_RGB(double T, double RGB[3])
{
    int c;
    double xD, yD, X, Y, Z, max;
    // Fit for CIE Daylight illuminant
    if (T <= 4000)
    {
        xD = 0.27475e9 / (T * T * T) - 0.98598e6 / (T * T) + 1.17444e3 / T + 0.145986;
    }
    else if (T <= 7000)
    {
        xD = -4.6070e9 / (T * T * T) + 2.9678e6 / (T * T) + 0.09911e3 / T + 0.244063;
    }
    else
    {
        xD = -2.0064e9 / (T * T * T) + 1.9018e6 / (T * T) + 0.24748e3 / T + 0.237040;
    }
    yD = -3 * xD * xD + 2.87 * xD - 0.275;
    
    // Fit for Blackbody using CIE standard observer function at 2 degrees
    //xD = -1.8596e9/(T*T*T) + 1.37686e6/(T*T) + 0.360496e3/T + 0.232632;
    //yD = -2.6046*xD*xD + 2.6106*xD - 0.239156;
    
    // Fit for Blackbody using CIE standard observer function at 10 degrees
    //xD = -1.98883e9/(T*T*T) + 1.45155e6/(T*T) + 0.364774e3/T + 0.231136;
    //yD = -2.35563*xD*xD + 2.39688*xD - 0.196035;
    
    X = xD / yD;
    Y = 1;
    Z = (1 - xD - yD) / yD;
    max = 0;
    for (c = 0; c < 3; c++) {
        RGB[c] = X * XYZ_to_RGB[0][c] + Y * XYZ_to_RGB[1][c] + Z * XYZ_to_RGB[2][c];
        if (RGB[c] > max) max = RGB[c];
    }
    for (c = 0; c < 3; c++) RGB[c] = RGB[c] / max;
}

static inline void pseudoinverse (double (*in)[3], double (*out)[3], int size)
{
    double work[3][6], num;
    int i, j, k;
    
    for (i=0; i < 3; i++) {
        for (j=0; j < 6; j++)
            work[i][j] = j == i+3;
        for (j=0; j < 3; j++)
            for (k=0; k < size; k++)
                work[i][j] += in[k][i] * in[k][j];
    }
    for (i=0; i < 3; i++) {
        num = work[i][i];
        for (j=0; j < 6; j++)
            work[i][j] /= num;
        for (k=0; k < 3; k++) {
            if (k==i) continue;
            num = work[k][i];
            for (j=0; j < 6; j++)
                work[k][j] -= work[i][j] * num;
        }
    }
    for (i=0; i < size; i++)
        for (j=0; j < 3; j++)
            for (out[i][j]=k=0; k < 3; k++)
                out[i][j] += work[j][k+3] * in[i][k];
}

static inline void cam_xyz_coeff (double cam_xyz[4][3], float pre_mul[4], float rgb_cam[3][4])
{
    double cam_rgb[4][3], inverse[4][3], num;
    int i, j, k;
    
    for (i=0; i < COLORS; i++)                /* Multiply out XYZ colorspace */
        for (j=0; j < 3; j++)
            for (cam_rgb[i][j] = k=0; k < 3; k++)
                cam_rgb[i][j] += cam_xyz[i][k] * xyz_rgb[k][j];
    
    for (i=0; i < COLORS; i++) {                /* Normalize cam_rgb so that */
        for (num=j=0; j < 3; j++)                /* cam_rgb * (1,1,1) is (1,1,1,1) */
            num += cam_rgb[i][j];
        for (j=0; j < 3; j++)
            cam_rgb[i][j] /= num;
        pre_mul[i] = 1 / num;
    }
    pseudoinverse (cam_rgb, inverse, COLORS);
    for (i=0; i < 3; i++)
        for (j=0; j < COLORS; j++)
            rgb_cam[i][j] = inverse[j][i];
}


static void kelvin_green_to_multipliers(double temperature, double green, double chanMulArray[3], struct cam_matrices * cam_matrices)
{
    float pre_mul[4], rgb_cam[3][4];
    double cam_xyz[4][3];
    double rgbWB[3];
    double cam_rgb[3][3];
    double rgb_cam_transpose[4][3];
    int c, cc, i, j;
    
    for (i = 0; i < 9; i++)
    {
        cam_xyz[i/3][i%3] = (double)cam_matrices->ColorMatrix2[i*2] / (double)cam_matrices->ColorMatrix2[i*2 + 1];
    }
    
    for (i = 9; i < 12; i++)
    {
        cam_xyz[i/3][i%3] = 0;
    }
    
    cam_xyz_coeff (cam_xyz, pre_mul, rgb_cam);
    
    for (i = 0; i < 4; i++) for (j = 0; j < 3; j++)
    {
        rgb_cam_transpose[i][j] = rgb_cam[j][i];
    }
    
    pseudoinverse(rgb_cam_transpose, cam_rgb, 3);
    
    temperature_to_RGB(temperature, rgbWB);
    rgbWB[1] = rgbWB[1] / green;
    
    for (c = 0; c < 3; c++)
    {
        double chanMulInv = 0;
        for (cc = 0; cc < 3; cc++)
            chanMulInv += 1 / pre_mul[c] * cam_rgb[c][cc] * rgbWB[cc];
        chanMulArray[c] = 1 / chanMulInv;
    }
    
    /* normalize green multiplier */
    chanMulArray[0] /= chanMulArray[1];
    chanMulArray[2] /= chanMulArray[1];
    chanMulArray[1] = 1;
}

static void get_white_balance(struct dng_info * dng_info, int32_t *wbal, struct cam_matrices * cam_matrices)
{
    if(dng_info->lens_info->wb_mode == WB_CUSTOM)
    {
        wbal[0] = dng_info->lens_info->WBGain_R; wbal[1] = dng_info->lens_info->WBGain_G;
        wbal[2] = dng_info->lens_info->WBGain_G; wbal[3] = dng_info->lens_info->WBGain_G;
        wbal[4] = dng_info->lens_info->WBGain_B; wbal[5] = dng_info->lens_info->WBGain_G;
    }
    else
    {
        double kelvin = 5500;
        double green = 1.0;
        
        //TODO: G/M shift, not sure how this relates to "green" parameter
        if(dng_info->lens_info->wb_mode == WB_AUTO || dng_info->lens_info->wb_mode == WB_KELVIN)
        {
            kelvin = dng_info->lens_info->kelvin;
        }
        else if(dng_info->lens_info->wb_mode == WB_SUNNY)
        {
            kelvin = 5500;
        }
        else if(dng_info->lens_info->wb_mode == WB_SHADE)
        {
            kelvin = 7000;
        }
        else if(dng_info->lens_info->wb_mode == WB_CLOUDY)
        {
            kelvin = 6000;
        }
        else if(dng_info->lens_info->wb_mode == WB_TUNGSTEN)
        {
            kelvin = 3200;
        }
        else if(dng_info->lens_info->wb_mode == WB_FLUORESCENT)
        {
            kelvin = 4000;
        }
        else if(dng_info->lens_info->wb_mode == WB_FLASH)
        {
            kelvin = 5500;
        }
        double chanMulArray[3];
        kelvin_green_to_multipliers(kelvin, green, chanMulArray, cam_matrices);
        wbal[0] = 1000000; wbal[1] = (int32_t)(chanMulArray[0] * 1000000);
        wbal[2] = 1000000; wbal[3] = (int32_t)(chanMulArray[1] * 1000000);
        wbal[4] = 1000000; wbal[5] = (int32_t)(chanMulArray[2] * 1000000);
    }
}

/*****************************************************************************************************/

static uint16_t tiff_header[] = { byteOrderII, magicTIFF, 8, 0};

struct directory_entry {
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value;
};

//CDNG tag codes
enum
{
    tcTimeCodes				= 51043,
    tcFrameRate             = 51044,
    tcTStop                 = 51058,
    tcReelName              = 51081,
    tcCameraLabel           = 51105,
};

static uint32_t add_array(int32_t * array, uint8_t * buffer, uint32_t * data_offset, size_t length)
{
    uint32_t result = *data_offset;
    memcpy(buffer + result, array, length * sizeof(int32_t));
    *data_offset += length * sizeof(int32_t);
    return result;
}

static uint32_t add_string(char * str, uint8_t * buffer, uint32_t * data_offset)
{
    uint32_t result = 0;
    size_t length = strlen(str) + 1;
    if(length <= 4)
    {
        //we can fit in 4 bytes, so just pack the string into result
        memcpy(&result, str, length);
    }
    else
    {
        result = *data_offset;
        memcpy(buffer + result, str, length);
        *data_offset += length;
        //align to 2 bytes
        if(*data_offset % 2) *data_offset += 1;
    }
    return result;
}

static uint32_t add_rational(int32_t numerator, int32_t denominator, uint8_t * buffer, uint32_t * data_offset)
{
    uint32_t result = *data_offset;
    *(int32_t*)(buffer + *data_offset) = numerator;
    *data_offset += sizeof(int32_t);
    *(int32_t*)(buffer + *data_offset) = denominator;
    *data_offset += sizeof(int32_t);
    return result;
}

static inline uint8_t to_tc_byte(int value)
{
    return (((value / 10) << 4) | (value % 10));
}

static uint32_t add_timecode(float framerate, int drop_frame, uint64_t frame, uint8_t * buffer, uint32_t * data_offset)
{
    uint32_t result = *data_offset;
    memset(buffer + *data_offset, 0, 8);
    
    //from raw2cdng, credits: chmee
    int hours = (int)((double)frame / framerate / 3600);
    frame = frame - (hours * 60 * 60 * (int)framerate);
    int minutes = (int)((double)frame / framerate / 60);
    frame = frame - (minutes * 60 * (int)framerate);
    int seconds = (int)((double)frame / framerate) % 60;
    frame = frame - (seconds * (int)framerate);
    int frames = frame % (int)roundf(framerate);
    
    buffer[*data_offset] = to_tc_byte(frames) & 0x3F;
    if(drop_frame) buffer[0] = buffer[0] | (1 << 7); //set the drop frame bit
    buffer[*data_offset + 1] = to_tc_byte(seconds) & 0x7F;
    buffer[*data_offset + 2] = to_tc_byte(minutes) & 0x7F;
    buffer[*data_offset + 3] = to_tc_byte(hours) & 0x3F;
    
    *data_offset += 8;
    return result;
}

static void add_ifd(struct directory_entry * ifd, uint8_t * header, size_t * position, int count)
{
    *(uint16_t*)(header + *position) = count;
    *position += sizeof(uint16_t);
    memcpy(header + *position, ifd, count * sizeof(struct directory_entry));
    *position += count * sizeof(struct directory_entry);
    *(uint32_t*)(header + *position) = 0;
    *position += sizeof(uint32_t);
}

static char * format_datetime(char * datetime, size_t max_len, struct tm * tm)
{
    snprintf(datetime, max_len, "%04d:%02d:%02d %02d:%02d:%02d",
            1900 + tm->tm_year,
            tm->tm_mon,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    return datetime;
}

size_t dng_write_header_data(struct dng_info * dng_info, uint8_t * header, size_t header_size)
{
    if(!header) return 0;
    
    size_t position = 0;
    memcpy(header, tiff_header, sizeof(tiff_header));
    position += sizeof(tiff_header);
    char make[32];
    char * model = (char*)dng_info->camera_name;
    if(!model) model = "???";
    //make is usually the first word of camera_name
    strncpy(make, model, 32);
    char * space = strchr(make, ' ');
    if(space) *space = 0x0;
    
    uint32_t exif_ifd_offset = (uint32_t)(position + sizeof(uint16_t) + IFD0_COUNT * sizeof(struct directory_entry) + sizeof(uint32_t));
    uint32_t data_offset = exif_ifd_offset + sizeof(uint16_t) + EXIF_IFD_COUNT * sizeof(struct directory_entry) + sizeof(uint32_t);
    
    int32_t frame_rate[2] = {dng_info->fps_numerator, dng_info->fps_denominator};
    double frame_rate_f = dng_info->fps_denominator == 0 ? 0 : (double)dng_info->fps_numerator / (double)dng_info->fps_denominator;
    char datetime[255];
    int32_t basline_exposure[2] = {dng_info->raw_info->exposure_bias[0],dng_info->raw_info->exposure_bias[1]};
    if(basline_exposure[1] == 0)
    {
        basline_exposure[0] = 0;
        basline_exposure[1] = 1;
    }
    
    int drop_frame = frame_rate[1] % 10 != 0;
    //number of frames since midnight
    uint64_t tc_frame = (uint64_t)dng_info->frame_number;
    
    
    struct cam_matrices matricies = cam_matrices[0];
    for(int i = 0; i < COUNT(cam_matrices); i++)
    {
        if(!strcmp(cam_matrices[i].camera, model))
        {
            matricies = cam_matrices[i];
            break;
        }
    }
    
    int32_t wbal[6];
    get_white_balance(dng_info, wbal, &matricies);
    
    struct directory_entry IFD0[IFD0_COUNT] =
    {
        {tcNewSubFileType,              ttLong,     1,      sfMainImage},
        {tcImageWidth,                  ttLong,     1,      dng_info->raw_info->width},
        {tcImageLength,                 ttLong,     1,      dng_info->raw_info->height},
        {tcBitsPerSample,               ttShort,    1,      14},
        {tcCompression,                 ttShort,    1,      ccUncompressed},
        {tcPhotometricInterpretation,   ttShort,    1,      piCFA},
        {tcFillOrder,                   ttShort,    1,      1},
        {tcMake,                        ttAscii,    STRING_ENTRY(make, header, &data_offset)},
        {tcModel,                       ttAscii,    STRING_ENTRY(model, header, &data_offset)},
        {tcStripOffsets,                ttLong,     1,      (uint32_t)header_size},
        {tcOrientation,                 ttShort,    1,      1},
        {tcSamplesPerPixel,             ttShort,    1,      1},
        {tcRowsPerStrip,                ttShort,    1,      dng_info->raw_info->height},
        {tcStripByteCounts,             ttLong,     1,      (uint32_t)dng_info->raw_info->frame_size},
        {tcPlanarConfiguration,         ttShort,    1,      pcInterleaved},
        {tcSoftware,                    ttAscii,    STRING_ENTRY(SOFTWARE_NAME, header, &data_offset)},
        {tcDateTime,                    ttAscii,    STRING_ENTRY(format_datetime(datetime, 255, dng_info->tm), header, &data_offset)},
        {tcCFARepeatPatternDim,         ttShort,    2,      0x00020002}, //2x2
        {tcCFAPattern,                  ttByte,     4,      0x02010100}, //RGGB
        {tcExifIFD,                     ttLong,     1,      exif_ifd_offset},
        {tcDNGVersion,                  ttByte,     4,      0x00000401}, //1.4.0.0 in little endian
        {tcUniqueCameraModel,           ttAscii,    STRING_ENTRY(model, header, &data_offset)},
        {tcBlackLevel,                  ttLong,     1,      dng_info->raw_info->black_level},
        {tcWhiteLevel,                  ttLong,     1,      dng_info->raw_info->white_level},
        {tcDefaultCropOrigin,           ttShort,    2,      PACK(dng_info->raw_info->crop.origin)},
        {tcDefaultCropSize,             ttShort,    2,      PACK(dng_info->raw_info->crop.size)},
        {tcColorMatrix1,                ttSRational,RATIONAL_ENTRY(matricies.ColorMatrix1, header, &data_offset, 18)},
        {tcColorMatrix2,                ttSRational,RATIONAL_ENTRY(matricies.ColorMatrix2, header, &data_offset, 18)},
        {tcAsShotNeutral,               ttRational, RATIONAL_ENTRY(wbal, header, &data_offset, 6)},
        {tcBaselineExposure,            ttSRational,RATIONAL_ENTRY(basline_exposure, header, &data_offset, 2)},
        {tcCalibrationIlluminant1,      ttShort,    1,      lsStandardLightA},
        {tcCalibrationIlluminant2,      ttShort,    1,      lsD65},
        {tcActiveArea,                  ttLong,     ARRAY_ENTRY(dng_info->raw_info->dng_active_area, header, &data_offset, 4)},
        {tcForwardMatrix1,              ttSRational,RATIONAL_ENTRY(matricies.ForwardMatrix1, header, &data_offset, 18)},
        {tcForwardMatrix2,              ttSRational,RATIONAL_ENTRY(matricies.ForwardMatrix2, header, &data_offset, 18)},
        {tcTimeCodes,                   ttByte,     8,      add_timecode(frame_rate_f, drop_frame, tc_frame, header, &data_offset)},
        {tcFrameRate,                   ttSRational,RATIONAL_ENTRY(frame_rate, header, &data_offset, 2)},
        {tcBaselineExposureOffset,      ttSRational,RATIONAL_ENTRY2(0, 1, header, &data_offset)},
    };
    
    struct directory_entry EXIF_IFD[EXIF_IFD_COUNT] =
    {
        {tcExposureTime,                ttRational, RATIONAL_ENTRY2((int32_t)dng_info->shutter, 1000, header, &data_offset)},
        {tcFNumber,                     ttRational, RATIONAL_ENTRY2(dng_info->lens_info->aperture, 10, header, &data_offset)},
        {tcISOSpeedRatings,             ttShort,    1,      dng_info->lens_info->iso ? dng_info->lens_info->iso : dng_info->lens_info->iso_auto},
        {tcSensitivityType,             ttShort,    1,      stISOSpeed},
        {tcExifVersion,                 ttUndefined,4,      0x30333230},
        {tcSubjectDistance,             ttRational, RATIONAL_ENTRY2(dng_info->lens_info->focus_dist, 1, header, &data_offset)},
        {tcFocalLength,                 ttRational, RATIONAL_ENTRY2(dng_info->lens_info->focal_len, 1, header, &data_offset)},
        {tcLensModelExif,               ttAscii,    STRING_ENTRY((char*)dng_info->lens_info->name, header, &data_offset)},
    };
    
    //update the StripOffsets to the correct location
    //the image data starts where our extra data ends
    IFD0[9].value = data_offset;
    
    add_ifd(IFD0, header, &position, IFD0_COUNT);
    add_ifd(EXIF_IFD, header, &position, EXIF_IFD_COUNT);
    
    return data_offset;
}

void FAST reverse_bytes_order(char* buf, int count)
{
#ifdef __ARM__
    /* optimized swap from g3gg0 */
    asm volatile ("\
                  /* r2 = A B C D */\r\n\
                  /* r5 = 0 B 0 D */\r\n\
                  /* r2 = 0 A 0 C */\r\n\
                  /* r2 = B 0 D 0 | 0 A 0 C */\r\n\
                  \
                  /* init swap mask */\r\n\
                  mov r4, #0xff\r\n\
                  orr r4, r4, #0xff0000\r\n\
                  \
                  _wswap128:\r\n\
                  cmp %1, #0x10\r\n\
                  blt _wswap64\r\n\
                  \
                  ldmia %0, {r2, r3, r6, r7}\r\n\
                  and r5, r4, r2\r\n\
                  and r2, r4, r2, ror #8\r\n\
                  orr r2, r2, r5, lsl #8\r\n\
                  and r5, r4, r3\r\n\
                  and r3, r4, r3, ror #8\r\n\
                  orr r3, r3, r5, lsl #8\r\n\
                  and r5, r4, r6\r\n\
                  and r6, r4, r6, ror #8\r\n\
                  orr r6, r6, r5, lsl #8\r\n\
                  and r5, r4, r7\r\n\
                  and r7, r4, r7, ror #8\r\n\
                  orr r7, r7, r5, lsl #8\r\n\
                  stmia %0!, {r2, r3, r6, r7}\r\n\
                  sub %1, #0x10\r\n\
                  b _wswap128\r\n\
                  \
                  _wswap64:\r\n\
                  cmp %1, #0x08\r\n\
                  blt _wswap32\r\n\
                  \
                  ldmia %0, {r2, r3}\r\n\
                  and r5, r4, r2\r\n\
                  and r2, r4, r2, ror #8\r\n\
                  orr r2, r2, r5, lsl #8\r\n\
                  and r5, r4, r3\r\n\
                  and r3, r4, r3, ror #8\r\n\
                  orr r3, r3, r5, lsl #8\r\n\
                  stmia %0!, {r2, r3}\r\n\
                  sub %1, #0x08\r\n\
                  b _wswap64\r\n\
                  \
                  _wswap32:\r\n\
                  cmp %1, #0x04\r\n\
                  blt _wswap16\r\n\
                  \
                  ldmia %0, {r2}\r\n\
                  and r5, r4, r2\r\n\
                  and r2, r4, r2, ror #8\r\n\
                  orr r2, r2, r5, lsl #8\r\n\
                  stmia %0!, {r2}\r\n\
                  sub %1, #0x04\r\n\
                  b _wswap32\r\n\
                  \
                  _wswap16:\r\n\
                  cmp %1, #0x00\r\n\
                  beq _wswap_end\r\n\
                  ldrh r2, [%0]\r\n\
                  mov r3, r2, lsr #8\r\n\
                  orr r2, r3, r2, lsl #8\r\n\
                  strh r2, [%0]\r\n\
                  \
                  _wswap_end:\
                  " : : "r"(buf), "r"(count) : "r2", "r3", "r4", "r5", "r6", "r7");
#else
    short* buf16 = (short*) buf;
    int i;
    for (i = 0; i < count/2; i++)
    {
        short x = buf16[i];
        buf[2*i+1] = x;
        buf[2*i] = x >> 8;
    }
#endif
}

struct dng_info * dng_get_info(struct raw_info * raw_info, int use_frame_shutter)
{
    char *model_data = NULL;
    uint64_t *body_data = NULL;
    size_t model_len = 0;
    size_t body_len = 0;
    int err = 0;
    
    struct dng_info * dng_info = malloc(sizeof(struct dng_info));
    if(dng_info)
    {
        dng_info->raw_info = malloc(sizeof(struct raw_info));
        dng_info->lens_info = malloc(sizeof(struct lens_info));
        dng_info->tm = malloc(sizeof(struct tm));
        
        /* default values */
        dng_info->camera_name[0] = '\000';
        dng_info->camera_serial[0] = '\000';
        
        /* get camera properties */
        err = PROPAD_GetPropertyData(PROP_CAM_MODEL, (void **) &model_data, &model_len);
        if(err || model_len < 36 || !model_data)
        {
            snprintf((char*)dng_info->camera_name, sizeof(dng_info->camera_name), "ERR:%d md:0x%8X ml:%d", err, model_data, model_len);
        }
        else
        {
            memcpy((char *)dng_info->camera_name, &model_data[0], 32);
        }
        
        err = PROPAD_GetPropertyData(PROP_BODY_ID, (void **) &body_data, &body_len);
        if(err || !body_data || body_len == 0)
        {
            snprintf((char*)dng_info->camera_serial, sizeof(dng_info->camera_serial), "ERR:%d bd:0x%8X bl:%d", err, body_data, body_len);
        }
        else
        {
            /* different camera serial lengths */
            if(body_len == 8)
            {
                snprintf((char *)dng_info->camera_serial, sizeof(dng_info->camera_serial), "%X%08X", (uint32_t)(*body_data & 0xFFFFFFFF), (uint32_t) (*body_data >> 32));
            }
            else if(body_len == 4)
            {
                snprintf((char *)dng_info->camera_serial, sizeof(dng_info->camera_serial), "%08X", *((uint32_t*)body_data));
            }
            else
            {
                snprintf((char *)dng_info->camera_serial, sizeof(dng_info->camera_serial), "(unknown len %d)", body_len);
            }
        }
        
        
        if(dng_info->raw_info && dng_info->lens_info && dng_info->tm)
        {
            memcpy(dng_info->raw_info, raw_info, sizeof(struct raw_info));
            memcpy(dng_info->lens_info, &lens_info, sizeof(struct lens_info));
            LoadCalendarFromRTC(dng_info->tm);
            dng_info->frame_number = 0;
            dng_info->fps_numerator = 0;
            dng_info->fps_denominator = 1;
            dng_info->shutter = use_frame_shutter ? (int32_t)(1000000.0f / (float)get_current_shutter_reciprocal_x1000()) : raw2shutter_ms(lens_info.raw_shutter);
        }
        else
        {
            dng_free(dng_info);
            dng_info = NULL;
        }
    }
    return dng_info;
}

void dng_free(struct dng_info * dng_info)
{
    if(!dng_info) return;
    
    free(dng_info->raw_info);
    free(dng_info->lens_info);
    free(dng_info->tm);
    free(dng_info);
}

int dng_save(char* filename, struct dng_info * dng_info)
{
    FILE* f = FIO_CreateFile(filename);
    if (!f) return 0;
    
    uint8_t * dng_header = fio_malloc(sizeof(uint8_t) * HEADER_SIZE);
    if(!dng_header) return 0;
    
    size_t actual_header_size = dng_write_header_data(dng_info, dng_header, HEADER_SIZE);
    char* rawadr = (void*)dng_info->raw_info->buffer;
    uint32_t raw_size = dng_info->raw_info->frame_size;
    
    FIO_WriteFile(f, dng_header, actual_header_size);
    
    reverse_bytes_order(UNCACHEABLE(rawadr), raw_size);
    FIO_WriteFile(f, UNCACHEABLE(rawadr), raw_size);
    
    //re-write the TIFF Header
    //TODO: Why is this necessary? If I don't do this, I always get 0xC949 instead of 0x4949 to start the file
    FIO_SeekSkipFile(f, 0, SEEK_SET);
    FIO_WriteFile(f, &tiff_header, 4);
    
    fio_free(dng_header);
    
    FIO_CloseFile(f);
    return 1;
}

static unsigned int dng_init()
{
    return 0;
}

static unsigned int dng_deinit()
{
    return 0;
}

MODULE_INFO_START()
    MODULE_INIT(dng_init)
    MODULE_DEINIT(dng_deinit)
MODULE_INFO_END()

MODULE_CBRS_START()
MODULE_CBRS_END()

#endif
