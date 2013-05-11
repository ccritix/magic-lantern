#include "dryos.h"
#include "imgconv.h"
#include "bmp.h"

// precompute some parts of YUV to RGB computations
int yuv2rgb_RV[256];
int yuv2rgb_GU[256];
int yuv2rgb_GV[256];
int yuv2rgb_BU[256];

/** http://www.martinreddy.net/gfx/faqs/colorconv.faq
 * BT 601:
 * R'= Y' + 0.000*U' + 1.403*V'
 * G'= Y' - 0.344*U' - 0.714*V'
 * B'= Y' + 1.773*U' + 0.000*V'
 * 
 * BT 709:
 * R'= Y' + 0.0000*Cb + 1.5701*Cr
 * G'= Y' - 0.1870*Cb - 0.4664*Cr
 * B'= Y' - 1.8556*Cb + 0.0000*Cr
 */

void precompute_yuv2rgb()
{
#ifdef CONFIG_REC709
    /*
    *R = *Y + 1608 * V / 1024;
    *G = *Y -  191 * U / 1024 - 478 * V / 1024;
    *B = *Y + 1900 * U / 1024;
    */
    for (int u = 0; u < 256; u++)
    {
        int8_t U = u;
        yuv2rgb_GU[u] = (-191 * U) >> 10;
        yuv2rgb_BU[u] = (1900 * U) >> 10;
    }

    for (int v = 0; v < 256; v++)
    {
        int8_t V = v;
        yuv2rgb_RV[v] = (1608 * V) >> 10;
        yuv2rgb_GV[v] = (-478 * V) >> 10;
    }
#else // REC 601
    /*
    *R = *Y + ((1437 * V) >> 10);
    *G = *Y -  ((352 * U) >> 10) - ((731 * V) >> 10);
    *B = *Y + ((1812 * U) >> 10);
    */
    for (int u = 0; u < 256; u++)
    {
        int8_t U = u;
        yuv2rgb_GU[u] = (-352 * U) >> 10;
        yuv2rgb_BU[u] = (1812 * U) >> 10;
    }

    for (int v = 0; v < 256; v++)
    {
        int8_t V = v;
        yuv2rgb_RV[v] = (1437 * V) >> 10;
        yuv2rgb_GV[v] = (-731 * V) >> 10;
    }
#endif
}

/*inline void uyvy2yrgb(uint32_t uyvy, int* Y, int* R, int* G, int* B)
{
    uint32_t y1 = (uyvy >> 24) & 0xFF;
    uint32_t y2 = (uyvy >>  8) & 0xFF;
    *Y = (y1+y2) / 2;
    uint8_t u = (uyvy >>  0) & 0xFF;
    uint8_t v = (uyvy >> 16) & 0xFF;
    *R = MIN(*Y + yuv2rgb_RV[v], 255);
    *G = MIN(*Y + yuv2rgb_GU[u] + yuv2rgb_GV[v], 255);
    *B = MIN(*Y + yuv2rgb_BU[u], 255);
} */

void yuv2rgb(int Y, int U, int V, int* R, int* G, int* B)
{
    const int v_and_ff = V & 0xFF;
    const int u_and_ff = U & 0xFF;
    int v = Y + yuv2rgb_RV[v_and_ff];
    *R = COERCE(v, 0, 255);
    v = Y + yuv2rgb_GU[u_and_ff] + yuv2rgb_GV[v_and_ff];
    *G = COERCE(v, 0, 255);
    v = Y + yuv2rgb_BU[u_and_ff];
    *B = COERCE(v, 0, 255);
}

void uyvy_split(uint32_t uyvy, int* Y, int* U, int* V)
{
    *Y = UYVY_GET_AVG_Y(uyvy);
    *U = (int)(int8_t)UYVY_GET_U(uyvy);
    *V = (int)(int8_t)UYVY_GET_V(uyvy);
}

void yuv_resize(uint32_t* src, int src_w, int src_h, uint32_t* dst, int dst_w, int dst_h)
{
    int i,j;
    const int srcw_half = src_w >> 1;
    const int dstw_half = dst_w >> 1;
    for (i = 0; i < dst_h; i++)
    {
        const int src_off_part = (i*src_h/dst_h) * srcw_half;
        const int dst_off_y = i * dstw_half;
        int mult_srcw = 0;
        for (j = 0; j < dstw_half; j++, mult_srcw += src_w)
        {
            dst[dst_off_y + j] = src[src_off_part + mult_srcw/dst_w];
        }
    }
}

void yuv_halfcopy(uint32_t* dst, uint32_t* src, int w, int h, int top_half)
{
    int i,j;
    const int w_half = w >> 1;
    int pos = 0;
    for (i = 0; i < h; i++,pos += w_half)
    {
        for (j = 0; j < w/2; j++)
        {
            int sign = j - i * w_half/h;
            const int offset = pos + j;
            if ((top_half && sign > 0) || (!top_half && sign <= 0))
            {
                dst[offset] = src[offset];
            }
        }
    }
}

void bmp_zoom(uint8_t* dst, uint8_t* src, int x0, int y0, int denx, int deny)
{
    ASSERT(src);
    ASSERT(dst);
    if (!dst) return;
    int i,j;
    
    // only used for menu => 720x480
    static int16_t js_cache[720];
    
    for (j = 0; j < 720; j++)
        js_cache[j] = (j - x0) * denx / 128 + x0;
    
    for (i = 0; i < 480; i++)
    {
        int is = (i - y0) * deny / 128 + y0;
        uint8_t* dst_r = &dst[BM(0,i)];
        uint8_t* src_r = &src[BM(0,is)];
        
        if (is >= 0 && is < 480)
        {
            for (j = 0; j < 720; j++)
            {
                int js = js_cache[j];
                dst_r[j] = likely(js >= 0 && js < 720) ? src_r[js] : 0;
            }
        }
        else
            bzero32(dst_r, 720);
    }
}

static void FAST yuvcpy_x2(uint32_t* dst, uint32_t* src, int num_pix)
{
    dst = ALIGN32(dst);
    src = ALIGN32(src);
    uint32_t* last_s = src + (num_pix>>1);
    for (; src < last_s; src++, dst += 2)
    {
        uint32_t chroma = (*src)  & 0x00FF00FF;
        uint32_t luma1 = (*src >>  8) & 0xFF;
        uint32_t luma2 = (*src >> 24) & 0xFF;
        *(dst) = chroma | (luma1 << 8) | (luma1 << 24);
        *(dst+1) = chroma | (luma2 << 8) | (luma2 << 24);
    }
}

static void FAST yuvcpy_x3(uint32_t* dst, uint32_t* src, int num_pix)
{
    dst = ALIGN32(dst);
    src = ALIGN32(src);
    uint32_t* last_s = src + (num_pix>>1);
    for (; src < last_s; src++, dst += 3)
    {
        uint32_t chroma = (*src)  & 0x00FF00FF;
        uint32_t luma1 = (*src >>  8) & 0xFF;
        uint32_t luma2 = (*src >> 24) & 0xFF;
        const int l18 = luma1 << 8;
        const int l28 = luma2 << 8;
        const int l224 = luma2 << 24;
        *(dst)   = chroma | l18 | (luma1 << 24);
        *(dst+1) = chroma | l18 | l224;
        *(dst+2) = chroma | l28 | l224;
    }
}

void yuvcpy_main(uint32_t* dst, uint32_t* src, int num_pix, int X, int lut)
{
    dst = ALIGN32(dst);
    src = ALIGN32(src);
    
    if (X==1)
    {
        #ifdef CONFIG_DMA_MEMCPY
        dma_memcpy(dst, src, num_pix << 1);
        #else
        memcpy(dst, src, num_pix << 1);
        #endif
    }
    else if (X==2)
    {
        yuvcpy_x2(dst, src, num_pix >> 1);
    }
    else if (X==3)
    {
        yuvcpy_x3(dst, src, num_pix/3);
    }
}

void little_cleanup(void* BP, void* MP)
{
    uint8_t* bp = BP; uint8_t* mp = MP;
    if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
    bp++; mp++;
    if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
    bp++; mp++;
    if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
    bp++; mp++;
    if (*bp != 0 && *bp == *mp) *mp = *bp = 0;
}
