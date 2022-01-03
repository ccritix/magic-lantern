#ifndef _fw_signature_h_
#define _fw_signature_h_

#include <config-defines.h>

#define SIG_START ROMBASEADDR
#define SIG_LEN 0x10000

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
#define SIG_1200D_102 0xba19c526 // from FF0C0000
//#define SIG_1300D_110 0xcd1249b3 // from FE0C0000 (atypical)
#define SIG_1300D_110 0x3d8461b5 // from FE0C0000 (atypical)
#define SIG_1300D_120 0x378b58db // from FE0C0000 (atypical)
//#define SIG_1300D_110 0x37874414 // from FE0C0000 (atypical)
#define SIG_2000D_110 0x70ca4ca6 // from FE0C0000 (atypical)

#define SIG_6D_116   0x11cb1ed2 // from FF0C0000
#define SIG_5D3_113  0x2e2f65f5 // from FF0C0000
#define SIG_EOSM_202 0x2D7c6dcf // from FF0C0000
#define SIG_650D_104 0x4B7FC4D0 // from FF0C0000
#define SIG_700D_114 0x4b35ce13 // from FF0C0000
#define SIG_100D_101 0x3b82b55e // from FF0C0000
#define SIG_200D_101 0xf72c729a // from E0040000
#define SIG_M50_101  0x80bd9b4b // from E0040000

static uint32_t compute_signature(uint32_t * start, uint32_t num)
{
    uint32_t c = 0;
    for (uint32_t * p = start; p < start + num; p++)
    {
        c += *p;
    }
    return c;
}

#endif //_fw_signature_h_
