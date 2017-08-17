#include "dryos.h"
#include "module.h"
#include "lens.h"
#include "raw.h"
#include "edmac.h"

struct TwoInTwoOutLosslessPath_args
{ 
  uint32_t * ResLockKey;
  uint32_t   ResLockKeySize;
  uint32_t * engio_cmd_1_prepare;
  uint32_t * engio_cmd_2_prepare;
  uint32_t * engio_cmd_pre_jpcore;
  uint32_t * engio_cmd_post_jpcore;
  uint32_t * engio_cmd_stop;
  uint32_t * engio_cmd_6_prepare;
  uint32_t * engio_cmd_7_prepare;
  uint32_t * engio_cmd_8_HIV_PPR;
  uint16_t * PPR_table;
  uint32_t * goes_to_obinteg_C0F0E000_size_0x188;
  uint32_t * goes_to_C0F21000_size_0x7E0;
  uint32_t * LuckyEnable_goes_to_C0F20000_size_0x1000;
  uint16_t   LuckyEnable;
  uint16_t   xRes;
  uint16_t   yRes;
  uint16_t   off_0x3E;
  uint32_t   SamplePrecision;
  uint32_t   off_0x44;
  uint32_t   RD1_Channel;
  uint32_t   RD1_StartFlags;
  uint32_t   RD1_Flags;
  uint32_t   RD1_Connection;
  void *     RD1_Address;
  uint32_t   RD1_Offset;
  uint32_t   RD2_Channel;
  uint32_t   RD2_StartFlags;
  uint32_t   RD2_Flags;
  uint32_t   RD2_Connection;
  void *     RD2_Address;
  uint32_t   RD2_Offset;
  uint32_t   WR1_Channel;
  uint32_t   WR1_StartFlags;
  uint32_t   WR1_Flags;
  uint32_t   WR1_Connection;
  struct memSuite * WR1_MemSuite;
  uint32_t   off_0x8C;
  uint32_t   WR2_Channel;
  uint32_t   WR2_StartFlags;
  uint32_t   WR2_Flags;
  uint32_t   WR2_Connection;
  void *     WR2_Address;
  uint32_t   WR2_Offset;
};

static struct TwoInTwoOutLosslessPath_args TTL_Args;
static struct LockEntry * TTL_ResLock = 0;
static struct semaphore * lossless_sem = 0;
static int verbose = 0;

static void LosslessCompleteCBR()
{
    DryosDebugMsg(0, 0, "LosslessCompleteCBR\n");
    give_semaphore(lossless_sem);
}

/* pack two 16-bit values into a 32-bit one */
#define PACK32(lo,hi) (((uint32_t)(lo) & 0xFFFF) | ((uint32_t)(hi) << 16))

static void (*TTL_SetArgs)(int unused, void * TTL_Args, int PictureSize);
static void (*TTL_Prepare)(void * ResLock, void * TTL_Args);
static void (*TTL_RegisterCBR)(int id, void * cbr, void * cbr_arg);
static void (*TTL_SetFlags)(int PictureType);
static void (*TTL_Start)(void * TTL_Args);
static void (*TTL_Stop)(void * TTL_Args);
static void (*TTL_Finish)(void * ResLock, void * TTL_Args, uint32_t * output_size);

static void set_flags_700D()
{
    EngDrvOut(0xC0F37010, (shamem_read(0xC0F37010) & 0xFFE3FFFF) | 0xC0000);
    EngDrvOut(0xC0F3704C, (shamem_read(0xC0F3704C) & 0xFFFFFFC0) | 0x20);
    EngDrvOut(0xC0F37074, (shamem_read(0xC0F37074) & 0xFFC0FFFF) | 0x180000);
    EngDrvOut(0xC0F37078, (shamem_read(0xC0F37078) & 0xFFC0FFFF) | 0x180000);
}

int lossless_init()
{
    if (is_camera("5D3", "1.1.3"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 5D3 1.1.3 */
        TTL_SetArgs     = (void *) 0xFF32330C;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF3D4680;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF3D3774;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF32B418;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF3D46F0;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF3D4728;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF3D4760;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("5D3", "1.2.3"))
    {
        /* ProcessTwoInTwoOutLosslessPath, 5D3 1.2.3 */
        TTL_SetArgs     = (void *) 0xFF327DE8;  /* fills TTL_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF3DD574;  /* called right after ProcessTwoInTwoOutLosslessPath(R) Start; */
                                                /* calls [TTL] GetPathResources and sets up the encoder for RAW/SRAW/MRAW */
        TTL_RegisterCBR = (void *) 0xFF3DC668;  /* RegisterTwoInTwoOutLosslessPathCompleteCBR */
        TTL_SetFlags    = (void *) 0xFF32FF2C;  /* called next, with PictureType as arguments */
        TTL_Start       = (void *) 0xFF3DD5E4;  /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF3DD61C;  /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF3DD654;  /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */
    }

    if (is_camera("700D", "1.1.4"))
    {
        /* ProcessTwoInTwoOutJpegath, 700D 1.1.4 */
        TTL_SetArgs     = (void *) 0xFF35F510;      /* fills TTJ_Args struct; PictureSize(Mem1ToRaw) */
        TTL_Prepare     = (void *) 0xFF424BA4;      /* called right after ProcessTwoInTwoOutJpegath(R) Start(%d); */
                                                    /* calls [TTJ] GetPathResources and sets up the encoder for RAW */
        TTL_RegisterCBR = (void *) 0xFF423B88;      /* RegisterTwoInTwoOutJpegPathCompleteCBR */
        TTL_SetFlags    = (void *) set_flags_700D;  /* this function is inline on 700D */
        TTL_Start       = (void *) 0xFF424C4C;      /* called next; starts the EDmac transfers */
        TTL_Stop        = (void *) 0xFF423DD4;      /* called right after sssStopMem1ToRawPath */
        TTL_Finish      = (void *) 0xFF424CBC;      /* called next; calls UnlockEngineResources and returns output size from JpCoreCompleteCBR */

//        TTL_ResLock     = (void *) MEM(0x25A60);    /* this should work outside LiveView (e.g. full-res silent pics) */
    }

    lossless_sem = create_named_semaphore(0, 0);
    
    if (is_camera("700D", "*")) {
        uint32_t resources[] = {
            0x10002,                        /* read channel 0x8 */
            edmac_channel_to_index(0x20),   /* write channel 0x20 */
            0x20005,
            0x20016,
            0x30002,
            0x50034,
            0x5002d,
            0x50010,
            0x90001,
            0x90000,
            0xa0000,
            0x160000,
            0x260000,
            0x260001,
            0x260002,
            0x260003,
        };

        TTL_ResLock = CreateResLockEntry(resources, COUNT(resources));
    } else 
    if (is_camera("5D3", "*")) {
        uint32_t resources[] = {
            0x10000,                        /* read channel 0x8 */
            edmac_channel_to_index(0x11),   /* write channel 0x11 */
            0x2002d,
            0x20016,
            0x50034,
            0x5002d,
            0x50010,
            0x90001,
            0x230000,
            0x160000,
            0x260000,
            0x260001,
            0x260002,
            0x260003,
        };
        
        TTL_ResLock = CreateResLockEntry(resources, COUNT(resources));
    }
    
    /* return 1 if everything looks alright */
    return TTL_Start && lossless_sem && TTL_ResLock;
}

static uint32_t start_time = 0;

/* returns output size if successful, negative on error */
/* dummy operation also allowed (dst_suite = 0) */
int lossless_compress_raw_rectangle(
    struct memSuite * dst_suite, void * src,
    int src_width, int src_x, int src_y,
    int width, int height
)
{
    if (!TTL_ResLock || !lossless_sem || !TTL_Start)
    {
        /* not initialized */
        return -1;
    }

    /* setup photo quality (valid values: 0=RAW, 1=MRAW, 2=SRAW, 14, 15) */
    TTL_SetArgs(0, &TTL_Args, 0);

    /* trick the encoder so it configures slice width = image width */
    /* we'll have two slices on top of each other; this will give
     * valid lossless DNG as well, if we prepend a header :)
     */
    TTL_Args.xRes = width * 2;
    TTL_Args.yRes = height / 2;

    /* Output channel 22 appears used in LiveView; use 17 instead */
    /* fixme: 5D3/6D only */
    if (is_camera("5D3", "1.1.3") || is_camera("5D3", "1.2.3"))
    {
        TTL_Args.WR1_Channel = 0x11;
    }
    if (is_camera("700D", "1.1.4"))
    {
        TTL_Args.WR1_Channel = 0x20;
    }
    /* cleanup write channel from previous usage */
    SetEDmac(TTL_Args.WR1_Channel, 0, 0, 0);
    UnregisterEDmacCompleteCBR(TTL_Args.WR1_Channel);

    /* set starting point (top-left corner) */
    /* we need to skip a multiple of 8 pixels horizontally for raw_pixblock alignment
     * and an even number of pixels vertically, to preserve the Bayer pattern
     */
    uint32_t src_adjusted = ((uint32_t)src & 0x1FFFFFFF)
        + ((src_x/8*8) + (src_y/2*2) * src_width) * 14/8;

    /* set buffers */
    TTL_Args.WR1_MemSuite = dst_suite;
    TTL_Args.WR2_Address  = 0;
    TTL_Args.RD1_Address  = (void *) src_adjusted;

    /* configure the processing modules */
    TTL_Prepare(TTL_ResLock, &TTL_Args);

    if (is_camera("5D3", "1.1.3") || is_camera("5D3", "1.2.3"))
    {
        /* resolution is hardcoded in some places; patch them */
        EngDrvOut(0xC0F375B4, PACK32(width    - 1,  height/2  - 1));  /* 0xF6D0B8F */
        EngDrvOut(0xC0F13068, PACK32(width*2  - 1,  height/2  - 1));  /* 0xF6D171F */
        EngDrvOut(0xC0F12010,        width    - 1                 );  /* 0xB8F     */
        EngDrvOut(0xC0F12014, PACK32(width    - 1,  height/2  - 1));  /* 0xF6D0B8F */
        EngDrvOut(0xC0F1201C,        width/10 - 1                 );  /* 0x127     */
        EngDrvOut(0xC0F12020, PACK32(width/10 - 1,  height/20 - 1));  /* 0x18A0127 */
    }

    /* need to read the image data in 2 slices
     * default configuration is 2 vertical slices;
     * however, using 2 horizontal slices makes it easy
     * to just slap a DNG header, resulting in valid output.
     * 
     * => the input EDMAC will simply read the image as usual.
     */
    struct edmac_info RD1_info = {
        .xb     = width * 14/8,
        .yb     = height - 1,
        .off1b  = src_width * 14/8 - width * 14/8,
    };

    SetEDmac(TTL_Args.RD1_Channel, TTL_Args.RD1_Address, &RD1_info, TTL_Args.RD1_Flags);

    if (verbose >= 2)
    {
        printf("[TTL] %dx%d %dbpp\n", TTL_Args.xRes, TTL_Args.yRes, TTL_Args.SamplePrecision);

        if (TTL_Args.WR1_MemSuite)
        {
            void * WR1_Address = GetMemoryAddressOfMemoryChunk(GetFirstChunkFromSuite(TTL_Args.WR1_MemSuite));
            const char * WR1_SizeFmt = format_memory_size(GetSizeOfMemoryChunk(GetFirstChunkFromSuite(dst_suite)));
            printf(" WR1: %x EDMAC#%d<%d> (%x %s)\n",  WR1_Address,  TTL_Args.WR1_Channel, TTL_Args.WR1_Connection, TTL_Args.WR1_MemSuite, WR1_SizeFmt);
        }

        printf(" WR2: %x EDMAC#%d<%d>\n", TTL_Args.WR2_Address,  TTL_Args.WR2_Channel, TTL_Args.WR2_Connection);
        printf(" RD1: %x EDMAC#%d<%d>\n", TTL_Args.RD1_Address,  TTL_Args.RD1_Channel, TTL_Args.RD1_Connection);
        printf(" RD2: %x EDMAC#%d<%d>\n", TTL_Args.RD2_Address,  TTL_Args.RD1_Channel, TTL_Args.RD2_Connection);
    }

    /* register our CBR, to be called when finished */
    /* 0=RD1, 1=RD2, 4=WR1 (Pop), 5 = WR2 */
    TTL_RegisterCBR(0, LosslessCompleteCBR, 0);
    
    /* this changes a few registers that appear to be bit fields */
    TTL_SetFlags(0x10000);

    /* time the operation */
    start_time = get_us_clock_value();

    /* this starts the EDmac channels */
    TTL_Start(&TTL_Args);

    /* wait until finished */
    int err = take_semaphore(lossless_sem, 1000);

    if (verbose >= 2)
    {
        uint32_t stop_time = get_us_clock_value();
        printf("[TTL] Elapsed time: %d us\n", (int)(stop_time - start_time));
    }

    /* stop processing; this will report output size */
    uint32_t output_size = 0;
    TTL_Stop(&TTL_Args);
    TTL_Finish(TTL_ResLock, &TTL_Args, &output_size);

    if (verbose >= 1)
    {
        /* compute input size (uncompressed) */
        uint32_t input_size = RD1_info.xb * (RD1_info.yb + 1);

        int ratio_x100 = output_size * 10000.0 / input_size;
        printf("[TTL] Output size : %s (%s%d.%02d%%)\n", format_memory_size(output_size), FMT_FIXEDPOINT2(ratio_x100));
    }

    if (err)
    {
        return -2;
    }

    return output_size;
}

int lossless_compress_raw(struct raw_info * raw_info, struct memSuite * output_memsuite)
{
    return lossless_compress_raw_rectangle(
        output_memsuite, raw_info->buffer,
        raw_info->width, 0, 0,
        raw_info->width, raw_info->height
    );
}

