#ifndef _fw_signature_h_
#define _fw_signature_h_

#include <config-defines.h>

#define SIG_LEN 0x10000

#if defined(CONFIG_DIGIC_V) || defined(CONFIG_1200D)
#define SIG_START 0xFF0C0000
#else
#define SIG_START 0xFF010000
#endif

#define SIG_60D_111  0xaf91b602 // from FF010000
#define SIG_550D_109 0x851320e6 // from FF010000
#define SIG_600D_102 0x27fc03de // from FF010000
#define SIG_600D_101 0x290106d8 // from FF010000 // firmwares are identical
#define SIG_500D_110 0x4c0e5a7e // from FF010000
#define SIG_50D_109  0x4673ef59 // from FF010000
#define SIG_500D_111 0x44f49aef // from FF010000
#define SIG_5D2_212  0xae78b938 // from FF010000
#define SIG_7D_203   0x50163E93 // from FF010000
#define SIG_7D_MASTER_203 0x640BF4D1 // from FF010000
#define SIG_1100D_105 0x46de7624 // from FF010000
#define SIG_1200D_100 0x9d609575 // from FF0C0000 (QEMU HelloWorld)
#define SIG_1200D_101 0x9d618f81 // identical to 1.0.0, except version and build date
#define SIG_6D_116   0x11cb1ed2 // from FF0C0000
#define SIG_5D3_113  0x2e2f65f5 // from FF0C0000
#define SIG_EOSM_202 0x2D7c6dcf // from FF0C0000
#define SIG_650D_104 0x4B7FC4D0 // from FF0C0000
#define SIG_700D_114 0x4b35ce13 // from FF0C0000
// TODO: multiple 100D.100 out there :(
#define SIG_100D_100 0x34443B7F

#ifdef CONFIG_1200D
/* 1200D: accept both firmware versions (1.0.0 and 1.0.1), since they are identical */
#define CURRENT_CAMERA_SIGNATURE_ALT SIG_1200D_101
#endif

#ifndef CURRENT_CAMERA_SIGNATURE_ALT
#define CURRENT_CAMERA_SIGNATURE_ALT CURRENT_CAMERA_SIGNATURE
#endif

static uint32_t compute_signature(uint32_t * start, uint32_t num)
{
    uint32_t c = 0;
    for (uint32_t * p = start; p < start + num; p++)
    {
        c += *p;
    }
    return c;
}

#ifdef CONFIG_QEMU
/* we are using patched ROMs, so the signature will fail for sure; skip it */
#undef CURRENT_CAMERA_SIGNATURE
#endif

#endif //_fw_signature_h_
