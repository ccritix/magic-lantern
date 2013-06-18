/**
 * DNG saving routines ported from CHDK
 * Code stripped down a bit, since we don't care about GPS and advanced exif stuff (at least for now)
 * 
 * TODO: make it platform-independent and move it to modules.
 */

/*
 * Original code copyright (C) CHDK (GPLv2); ported to Magic Lantern (2013)
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


#undef RAW_DEBUG_BLACK

#ifdef CONFIG_MAGICLANTERN
#include "dryos.h"
#include "property.h"
#define umalloc alloc_dma_memory
#define ufree free_dma_memory
#define pow powf

static int get_tick_count() { return get_ms_clock_value_fast(); }

#else // if we compile it for desktop
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
#include <sys/types.h>
#define FAST
#define UNCACHEABLE(x) (x)
#define umalloc malloc
#define ufree free

#define FIO_CreateFileEx(name) fopen(name, "wb")
#define FIO_WriteFile(f, ptr, count) fwrite(ptr, 1, count, f)
#define FIO_CloseFile(f) fclose(f)

#endif

#include "raw.h"

/* adaptations from CHDK to ML */
#define camera_sensor raw_info
#define get_raw_pixel raw_get_pixel
#define raw_rowpix width
#define raw_rows height
#define raw_size frame_size
#define write FIO_WriteFile

static void FAST reverse_bytes_order(char* buf, int count)
{
    short* buf16 = (short*) buf;
    int i;
    for (i = 0; i < count/2; i++)
    {
        short x = buf16[i];
        buf[2*i+1] = x;
        buf[2*i] = x >> 8;
    }
}

//thumbnail
#define DNG_TH_WIDTH 128
#define DNG_TH_HEIGHT 96
// higly recommended that DNG_TH_WIDTH*DNG_TH_HEIGHT would be divisible by 512

struct dir_entry{unsigned short tag; unsigned short type; unsigned int count; unsigned int offset;};

#define T_BYTE      1
#define T_ASCII     2
#define T_SHORT     3
#define T_LONG      4
#define T_RATIONAL  5
#define T_SBYTE     6
#define T_UNDEFINED 7
#define T_SSHORT    8
#define T_SLONG     9
#define T_SRATIONAL 10
#define T_FLOAT     11
#define T_DOUBLE    12
#define T_PTR       0x100   // Stored as long/short etc in DNG header, referenced by pointer in IFD (must be pointer to int variable)
#define T_SKIP      0x200   // Tag value to be skipped (for marking GPS entries if camera does not have GPS)

static const int cam_BaselineNoise[]           = {1,1};
static const int cam_BaselineSharpness[]       = {4,3};
static const int cam_LinearResponseLimit[]     = {1,1};
static const int cam_AnalogBalance[]           = {1,1,1,1,1,1};
static char cam_name[32]                    = "Canikon";
static char dng_artist_name[64]             = "";
static char dng_copyright[64]               = "";
static const short cam_PreviewBitsPerSample[]  = {8,8,8};
static const int cam_Resolution[]              = {180,1};
static int cam_AsShotNeutral[]          = {1000,1000,1000,1000,1000,1000};
static char cam_datetime[20]            = "";                   // DateTimeOriginal
static char cam_subsectime[4]           = "";                   // DateTimeOriginal (milliseconds component)
static int cam_shutter[2]               = { 0, 1000000 };       // Shutter speed
static int cam_aperture[2]              = { 0, 10 };            // Aperture
static int cam_apex_shutter[2]          = { 0, 96 };            // Shutter speed in APEX units
static int cam_apex_aperture[2]         = { 0, 96 };            // Aperture in APEX units
static int cam_exp_bias[2]              = { 0, 96 };
static int cam_max_av[2]                = { 0, 96 };
static int cam_focal_length[2]          = { 0, 1000 };
static char* software_ver = "Magic Lantern";

struct t_data_for_exif{
    short iso;
    int exp_program;
    int effective_focal_length;
    short orientation;
    short flash_mode;
    short flash_fired;
    short metering_mode;
};


#define BE(v)   ((v&0x000000FF)<<24)|((v&0x0000FF00)<<8)|((v&0x00FF0000)>>8)|((v&0xFF000000)>>24)   // Convert to big_endian

#define BADPIX_CFA_INDEX    6   // Index of CFAPattern value in badpixel_opcodes array

static unsigned int badpixel_opcode[] =
{
    // *** all values must be in big endian order

    BE(1),              // Count = 1

    BE(4),              // FixBadPixelsConstant = 4
    BE(0x01030000),     // DNG version = 1.3.0.0
    BE(1),              // Flags = 1
    BE(8),              // Opcode length = 8 bytes
    BE(0),              // Constant = 0
    BE(0),              // CFAPattern (set in code below)
};


static struct t_data_for_exif exif_data;

#define BE(v)   ((v&0x000000FF)<<24)|((v&0x0000FF00)<<8)|((v&0x00FF0000)>>8)|((v&0xFF000000)>>24)   // Convert to big_endian

// warning: according to TIFF format specification, elements must be sorted by tag value in ascending order!

// Index of specific entries in ifd0 below.
// *** warning - if entries are added or removed these should be updated ***
#define CAMERA_NAME_INDEX           8       // tag 0x110
#define THUMB_DATA_INDEX            9       // tag 0x111
#define ORIENTATION_INDEX           10      // tag 0x112
#define CHDK_VER_INDEX              15      // tag 0x131
#define ARTIST_NAME_INDEX           17      // tag 0x13B
#define SUBIFDS_INDEX               18      // tag 0x14A
#define COPYRIGHT_INDEX             19      // tag 0x8298
#define EXIF_IFD_INDEX              20      // tag 0x8769
#define DNG_VERSION_INDEX           22      // tag 0xC612
#define UNIQUE_CAMERA_MODEL_INDEX   24      // tag 0xC614

#define CAM_MAKE                    "Canon"

struct dir_entry ifd0[]={
    {0xFE,   T_LONG,       1,  1},                                 // NewSubFileType: Preview Image
    {0x100,  T_LONG,       1,  DNG_TH_WIDTH},                      // ImageWidth
    {0x101,  T_LONG,       1,  DNG_TH_HEIGHT},                     // ImageLength
    {0x102,  T_SHORT,      3,  (int)cam_PreviewBitsPerSample},     // BitsPerSample: 8,8,8
    {0x103,  T_SHORT,      1,  1},                                 // Compression: Uncompressed
    {0x106,  T_SHORT,      1,  2},                                 // PhotometricInterpretation: RGB
    {0x10E,  T_ASCII,      1,  0},                                 // ImageDescription
    {0x10F,  T_ASCII,      sizeof(CAM_MAKE), (int)CAM_MAKE},       // Make
    {0x110,  T_ASCII,      32, (int)cam_name},                     // Model: Filled at header generation.
    {0x111,  T_LONG,       1,  0},                                 // StripOffsets: Offset
    {0x112,  T_SHORT,      1,  1},                                 // Orientation: 1 - 0th row is top, 0th column is left
    {0x115,  T_SHORT,      1,  3},                                 // SamplesPerPixel: 3
    {0x116,  T_SHORT,      1,  DNG_TH_HEIGHT},                     // RowsPerStrip
    {0x117,  T_LONG,       1,  DNG_TH_WIDTH*DNG_TH_HEIGHT*3},      // StripByteCounts = preview size
    {0x11C,  T_SHORT,      1,  1},                                 // PlanarConfiguration: 1
    {0x131,  T_ASCII|T_PTR,32, 0},                                 // Software
    {0x132,  T_ASCII,      20, (int)cam_datetime},                 // DateTime
    {0x13B,  T_ASCII|T_PTR,64, (int)dng_artist_name},              // Artist: Filled at header generation.
    {0x14A,  T_LONG,       1,  0},                                 // SubIFDs offset
    {0x8298, T_ASCII|T_PTR,64, (int)dng_copyright},                // Copyright
    {0x8769, T_LONG,       1,  0},                                 // EXIF_IFD offset
    {0x9216, T_BYTE,       4,  0x00000001},                        // TIFF/EPStandardID: 1.0.0.0
    {0xC612, T_BYTE,       4,  0x00000301},                        // DNGVersion: 1.3.0.0
    {0xC613, T_BYTE,       4,  0x00000301},                        // DNGBackwardVersion: 1.1.0.0
    {0xC614, T_ASCII,      32, (int)cam_name},                     // UniqueCameraModel. Filled at header generation.
    {0xC621, T_SRATIONAL,  9,  (int)&camera_sensor.color_matrix1},
    {0xC627, T_RATIONAL,   3,  (int)cam_AnalogBalance},
    {0xC628, T_RATIONAL,   3,  (int)cam_AsShotNeutral},
    {0xC62A, T_SRATIONAL,  1,  (int)&camera_sensor.exposure_bias},
    {0xC62B, T_RATIONAL,   1,  (int)cam_BaselineNoise},
    {0xC62C, T_RATIONAL,   1,  (int)cam_BaselineSharpness},
    {0xC62E, T_RATIONAL,   1,  (int)cam_LinearResponseLimit},
};

// Index of specific entries in ifd1 below.
// *** warning - if entries are added or removed these should be updated ***
#define RAW_DATA_INDEX              6       // tag 0x111
#define BADPIXEL_OPCODE_INDEX       21      // tag 0xC740

struct dir_entry ifd1[]={
    {0xFE,   T_LONG,       1,  0},                                 // NewSubFileType: Main Image
    {0x100,  T_LONG|T_PTR, 1,  (int)&camera_sensor.raw_rowpix},    // ImageWidth
    {0x101,  T_LONG|T_PTR, 1,  (int)&camera_sensor.raw_rows},      // ImageLength
    {0x102,  T_SHORT|T_PTR,1,  (int)&camera_sensor.bits_per_pixel},// BitsPerSample
    {0x103,  T_SHORT,      1,  1},                                 // Compression: Uncompressed
    {0x106,  T_SHORT,      1,  0x8023},                            // PhotometricInterpretation: CFA
    {0x111,  T_LONG,       1,  0},                                 // StripOffsets: Offset
    {0x115,  T_SHORT,      1,  1},                                 // SamplesPerPixel: 1
    {0x116,  T_SHORT|T_PTR,1,  (int)&camera_sensor.raw_rows},      // RowsPerStrip
    {0x117,  T_LONG|T_PTR, 1,  (int)&camera_sensor.raw_size},      // StripByteCounts = CHDK RAW size
    {0x11A,  T_RATIONAL,   1,  (int)cam_Resolution},               // XResolution
    {0x11B,  T_RATIONAL,   1,  (int)cam_Resolution},               // YResolution
    {0x11C,  T_SHORT,      1,  1},                                 // PlanarConfiguration: 1
    {0x128,  T_SHORT,      1,  2},                                 // ResolutionUnit: inch
    {0x828D, T_SHORT,      2,  0x00020002},                        // CFARepeatPatternDim: Rows = 2, Cols = 2
    {0x828E, T_BYTE|T_PTR, 4,  (int)&camera_sensor.cfa_pattern},
    {0xC61A, T_LONG|T_PTR, 1,  (int)&camera_sensor.black_level},   // BlackLevel
    {0xC61D, T_LONG|T_PTR, 1,  (int)&camera_sensor.white_level},   // WhiteLevel
    {0xC61F, T_LONG,       2,  (int)&camera_sensor.crop.origin},
    {0xC620, T_LONG,       2,  (int)&camera_sensor.crop.size},
    {0xC68D, T_LONG,       4,  (int)&camera_sensor.dng_active_area},
    {0xC740, T_UNDEFINED|T_PTR, sizeof(badpixel_opcode),  (int)&badpixel_opcode},
};

// Index of specific entries in exif_ifd below.
// *** warning - if entries are added or removed these should be updated ***
#define EXPOSURE_PROGRAM_INDEX      2       // tag 0x8822
#define METERING_MODE_INDEX         10      // tag 0x9207
#define FLASH_MODE_INDEX            11      // tag 0x9209
#define SSTIME_INDEX                13      // tag 0x9290
#define SSTIME_ORIG_INDEX           14      // tag 0x9291

struct dir_entry exif_ifd[]={
    {0x829A, T_RATIONAL,   1,  (int)cam_shutter},          // Shutter speed
    {0x829D, T_RATIONAL,   1,  (int)cam_aperture},         // Aperture
    {0x8822, T_SHORT,      1,  0},                         // ExposureProgram
    {0x8827, T_SHORT|T_PTR,1,  (int)&exif_data.iso},       // ISOSpeedRatings
    {0x9000, T_UNDEFINED,  4,  0x31323230},                // ExifVersion: 2.21
    {0x9003, T_ASCII,      20, (int)cam_datetime},         // DateTimeOriginal
    {0x9201, T_SRATIONAL,  1,  (int)cam_apex_shutter},     // ShutterSpeedValue (APEX units)
    {0x9202, T_RATIONAL,   1,  (int)cam_apex_aperture},    // ApertureValue (APEX units)
    {0x9204, T_SRATIONAL,  1,  (int)cam_exp_bias},         // ExposureBias
    {0x9205, T_RATIONAL,   1,  (int)cam_max_av},           // MaxApertureValue
    {0x9207, T_SHORT,      1,  0},                         // Metering mode
    {0x9209, T_SHORT,      1,  0},                         // Flash mode
    {0x920A, T_RATIONAL,   1,  (int)cam_focal_length},     // FocalLength
    {0x9290, T_ASCII|T_PTR,4,  (int)cam_subsectime},       // DateTime milliseconds
    {0x9291, T_ASCII|T_PTR,4,  (int)cam_subsectime},       // DateTimeOriginal milliseconds
    {0xA405, T_SHORT|T_PTR,1,  (int)&exif_data.effective_focal_length},    // FocalLengthIn35mmFilm
};

static int get_type_size(int type)
{
    switch(type & 0xFF)
    {
    case T_BYTE:
    case T_SBYTE:
    case T_UNDEFINED:
    case T_ASCII:     return 1; 
    case T_SHORT:
    case T_SSHORT:    return 2;
    case T_LONG:
    case T_SLONG:
    case T_FLOAT:     return 4;
    case T_RATIONAL:
    case T_SRATIONAL:
    case T_DOUBLE:    return 8;
    default:          return 0;
    }
}

#define DIR_SIZE(ifd)   (sizeof(ifd)/sizeof(ifd[0]))

struct
{
    struct dir_entry* entry;
    int count;                  // Number of entries to be saved
    int entry_count;            // Total number of entries
} ifd_list[] = 
{
    {ifd0,      DIR_SIZE(ifd0),     DIR_SIZE(ifd0)}, 
    {ifd1,      DIR_SIZE(ifd1),     DIR_SIZE(ifd1)}, 
    {exif_ifd,  DIR_SIZE(exif_ifd), DIR_SIZE(exif_ifd)}, 
};

#define TIFF_HDR_SIZE (8)

static char* dng_header_buf;
static int dng_header_buf_size;
static int dng_header_buf_offset;
static char *thumbnail_buf;

static void add_to_buf(void* var, int size)
{
    memcpy(dng_header_buf+dng_header_buf_offset,var,size);
    dng_header_buf_offset += size;
}

static void add_val_to_buf(int val, int size)
{
    add_to_buf(&val,size);
}

static void create_dng_header(){
    int i,j;
    int extra_offset;
    int raw_offset;

    ifd0[DNG_VERSION_INDEX].offset = BE(0x01030000);
    
    ifd1[BADPIXEL_OPCODE_INDEX].type &= ~T_SKIP;
        // Set CFAPattern value
        switch (camera_sensor.cfa_pattern)
        {
        case 0x02010100:
            badpixel_opcode[BADPIX_CFA_INDEX] = BE(1);              // BayerPhase = 1 (top left pixel is green in a green/red row)
            break;
        case 0x01020001:
            badpixel_opcode[BADPIX_CFA_INDEX] = BE(0);              // BayerPhase = 0 (top left pixel is red)
            break;
        case 0x01000201:
            badpixel_opcode[BADPIX_CFA_INDEX] = BE(3);              // BayerPhase = 3 (top left pixel is blue)
            break;
        case 0x00010102:
            badpixel_opcode[BADPIX_CFA_INDEX] = BE(2);              // BayerPhase = 2 (top left pixel is green in a green/blue row)
            break;
        }

    // filling EXIF fields
    int ifd_count = DIR_SIZE(ifd_list);

    // Fix the counts and offsets where needed
    ifd0[CAMERA_NAME_INDEX].count = ifd0[UNIQUE_CAMERA_MODEL_INDEX].count = strlen(cam_name) + 1;
    ifd0[CHDK_VER_INDEX].offset = (int)software_ver;
    ifd0[CHDK_VER_INDEX].count = strlen(software_ver) + 1;
    ifd0[ARTIST_NAME_INDEX].count = strlen(dng_artist_name) + 1;
    ifd0[COPYRIGHT_INDEX].count = strlen(dng_copyright) + 1;
    //~ ifd0[ORIENTATION_INDEX].offset = get_orientation_for_exif(exif_data.orientation);

    //~ exif_ifd[EXPOSURE_PROGRAM_INDEX].offset = get_exp_program_for_exif(exif_data.exp_program);
    //~ exif_ifd[METERING_MODE_INDEX].offset = get_metering_mode_for_exif(exif_data.metering_mode);
    //~ exif_ifd[FLASH_MODE_INDEX].offset = get_flash_mode_for_exif(exif_data.flash_mode, exif_data.flash_fired);
    //~ exif_ifd[SSTIME_INDEX].count = exif_ifd[SSTIME_ORIG_INDEX].count = strlen(cam_subsectime)+1;

    // calculating offset of RAW data and count of entries for each IFD
    raw_offset=TIFF_HDR_SIZE;

    for (j=0;j<ifd_count;j++)
    {
        raw_offset+=6; // IFD header+footer
        for(i=0; i<ifd_list[j].entry_count; i++)
        {
            if ((ifd_list[j].entry[i].type & T_SKIP) == 0)  // Exclude skipped entries (e.g. GPS info if camera doesn't have GPS)
            {
                raw_offset+=12; // IFD directory entry size
                int size_ext=get_type_size(ifd_list[j].entry[i].type)*ifd_list[j].entry[i].count;
                if (size_ext>4) raw_offset+=size_ext+(size_ext&1);
            }
        }
    }

    // creating buffer for writing data
    raw_offset=(raw_offset/512+1)*512; // exlusively for CHDK fast file writing
    dng_header_buf_size=raw_offset;
    dng_header_buf=umalloc(raw_offset);
    dng_header_buf_offset=0;
    if (!dng_header_buf) return;

    // create buffer for thumbnail
    thumbnail_buf = malloc(DNG_TH_WIDTH*DNG_TH_HEIGHT*3);
    if (!thumbnail_buf)
    {
        ufree(dng_header_buf);
        dng_header_buf = 0;
        return;
    }

    //  writing offsets for EXIF IFD and RAW data and calculating offset for extra data

    extra_offset=TIFF_HDR_SIZE;

    ifd0[SUBIFDS_INDEX].offset = TIFF_HDR_SIZE + ifd_list[0].count * 12 + 6;                            // SubIFDs offset
    ifd0[EXIF_IFD_INDEX].offset = TIFF_HDR_SIZE + (ifd_list[0].count + ifd_list[1].count) * 12 + 6 + 6; // EXIF IFD offset
    ifd0[THUMB_DATA_INDEX].offset = raw_offset;                                     //StripOffsets for thumbnail
    ifd1[RAW_DATA_INDEX].offset = raw_offset + DNG_TH_WIDTH * DNG_TH_HEIGHT * 3;    //StripOffsets for main image

    for (j=0;j<ifd_count;j++)
    {
        extra_offset += 6 + ifd_list[j].count * 12; // IFD header+footer
    }

    // TIFF file header

    add_val_to_buf(0x4949, sizeof(short));      // little endian
    add_val_to_buf(42, sizeof(short));          // An arbitrary but carefully chosen number that further identifies the file as a TIFF file.
    add_val_to_buf(TIFF_HDR_SIZE, sizeof(int)); // offset of first IFD

    // writing IFDs

    for (j=0;j<ifd_count;j++)
    {
        int size_ext;
        add_val_to_buf(ifd_list[j].count, sizeof(short));
        for(i=0; i<ifd_list[j].entry_count; i++)
        {
            if ((ifd_list[j].entry[i].type & T_SKIP) == 0)
            {
                add_val_to_buf(ifd_list[j].entry[i].tag, sizeof(short));
                add_val_to_buf(ifd_list[j].entry[i].type & 0xFF, sizeof(short));
                add_val_to_buf(ifd_list[j].entry[i].count, sizeof(int));
                size_ext=get_type_size(ifd_list[j].entry[i].type)*ifd_list[j].entry[i].count;
                if (size_ext<=4) 
                {
                    if (ifd_list[j].entry[i].type & T_PTR)
                    {
                        add_to_buf((void*)ifd_list[j].entry[i].offset, sizeof(int));
                    }
                    else
                    {
                        add_val_to_buf(ifd_list[j].entry[i].offset, sizeof(int));
                    }
                }
                else
                {
                    add_val_to_buf(extra_offset, sizeof(int));
                    extra_offset += size_ext+(size_ext&1);    
                }
            }
        }
        add_val_to_buf(0, sizeof(int));
    }

    // writing extra data

    for (j=0;j<ifd_count;j++)
    {
        int size_ext;
        for(i=0; i<ifd_list[j].entry_count; i++)
        {
            if ((ifd_list[j].entry[i].type & T_SKIP) == 0)
            {
                size_ext=get_type_size(ifd_list[j].entry[i].type)*ifd_list[j].entry[i].count;
                if (size_ext>4)
                {
                    add_to_buf((void*)ifd_list[j].entry[i].offset, size_ext);
                    if (size_ext&1) add_val_to_buf(0, 1);
                }
            }
        }
    }

    // writing zeros to tail of dng header (just for fun)
    for (i=dng_header_buf_offset; i<dng_header_buf_size; i++) dng_header_buf[i]=0;
}

static void free_dng_header(void)
{
    if (dng_header_buf)
    {
        ufree(dng_header_buf);
        dng_header_buf=NULL;
    }
    if (thumbnail_buf)
    {
        free(thumbnail_buf);
        thumbnail_buf = 0;
    }
}

static int pow_calc_2( int mult, int x, int x_div, double y, int y_div)
{
	double x1 = x;
	if ( x_div != 1 ) { x1=x1/x_div;}
	if ( y_div != 1 ) { y=y/y_div;}

	if ( mult==1 )
		return pow( x1, y );
                else
		return mult	* pow( x1, y );
}

//-------------------------------------------------------------------
// Functions for creating DNG thumbnail image

static unsigned char gammma[256];

static void fill_gamma_buf(void)
{
    int i;
    if (gammma[255]) return;
    for (i=0; i<12; i++) gammma[i]=pow_calc_2(255, i, 255, 0.5, 1);
    for (i=12; i<64; i++) gammma[i]=pow_calc_2(255, i, 255, 0.4, 1);
    for (i=64; i<=255; i++) gammma[i]=pow_calc_2(255, i, 255, 0.25, 1);
}

static void create_thumbnail()
{
    register int i, j, x, y, yadj, xadj;
    register char *buf = thumbnail_buf;
    register int shift = camera_sensor.bits_per_pixel - 8;

    // The sensor bayer patterns are:
    //  0x02010100  0x01000201  0x01020001
    //      R G         G B         G R
    //      G B         R G         B G
    // for the second pattern yadj shifts the thumbnail row down one line
    // for the third pattern xadj shifts the thumbnail row accross one pixel
    // these make the patterns the same
    yadj = (camera_sensor.cfa_pattern == 0x01000201) ? 1 : 0;
    xadj = (camera_sensor.cfa_pattern == 0x01020001) ? 1 : 0;

    for (i=0; i<DNG_TH_HEIGHT; i++)
        for (j=0; j<DNG_TH_WIDTH; j++)
        {
            x = ((camera_sensor.active_area.x1 + camera_sensor.jpeg.x + (camera_sensor.jpeg.width  * j) / DNG_TH_WIDTH)  & 0xFFFFFFFE) + xadj;
            y = ((camera_sensor.active_area.y1 + camera_sensor.jpeg.y + (camera_sensor.jpeg.height * i) / DNG_TH_HEIGHT) & 0xFFFFFFFE) + yadj;

            *buf++ = gammma[get_raw_pixel(x,y)>>shift];           // red pixel
            *buf++ = gammma[6*(get_raw_pixel(x+1,y)>>shift)/10];  // green pixel
            *buf++ = gammma[get_raw_pixel(x+1,y+1)>>shift];       // blue pixel
        }
}

//-------------------------------------------------------------------
// Write DNG header, thumbnail and data to file

static void write_dng(FILE* fd, char* rawadr) 
{
    create_dng_header();

    if (dng_header_buf)
    {
        fill_gamma_buf();
        create_thumbnail();
        write(fd, dng_header_buf, dng_header_buf_size);
        write(fd, thumbnail_buf, DNG_TH_WIDTH*DNG_TH_HEIGHT*3);

        reverse_bytes_order(UNCACHEABLE(rawadr), camera_sensor.raw_size);
        write(fd, UNCACHEABLE(rawadr), camera_sensor.raw_size);

        free_dng_header();
    }
}

#ifdef CONFIG_MAGICLANTERN
PROP_HANDLER(PROP_CAM_MODEL)
{
    snprintf(cam_name, sizeof(cam_name), (const char *)buf);
}
#endif

int save_dng(char* filename)
{
    cam_AsShotNeutral[2] = 2477; /* 5D3 6000K */
    cam_AsShotNeutral[4] = 1462;
    
    #ifdef RAW_DEBUG_BLACK
    raw_info.active_area.x1 = 0;
    raw_info.active_area.x2 = raw_info.width;
    raw_info.active_area.y1 = 0;
    raw_info.active_area.y2 = raw_info.height;
    raw_info.jpeg.x = 0;
    raw_info.jpeg.y = 0;
    raw_info.jpeg.width = raw_info.width;
    raw_info.jpeg.height = raw_info.height;
    #endif
    
    FILE* f = FIO_CreateFileEx(filename);
    if (!f) return 0;
    write_dng(f, raw_info.buffer);
    FIO_CloseFile(f);
    return 1;
}
