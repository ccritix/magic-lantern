#include "edmac-memcpy.h"
#include "dryos.h"
#include "edmac.h"
#include "bmp.h"

#ifdef CONFIG_EDMAC_MEMCPY

static struct semaphore * edmac_memcpy_sem = 0; /* to allow only one memcpy running at a time */
static struct semaphore * edmac_read_done_sem = 0; /* to know when memcpy is finished */

static struct edmac_info src_edmac_info;
static struct edmac_info dst_edmac_info;

/* pick some free (check using debug menu) EDMAC channels write: 0x00-0x06, 0x10-0x16, 0x20-0x21. read: 0x08-0x0D, 0x18-0x1D,0x28-0x2B */
#if defined(CONFIG_5D2)
uint32_t edmac_read_chan = 0x19;
uint32_t edmac_write_chan = 0x3;
#elif defined(CONFIG_650D) || defined(CONFIG_EOSM) || defined(CONFIG_700D) || defined(CONFIG_100D)
uint32_t edmac_read_chan = 0x19;
uint32_t edmac_write_chan = 0x13;
#else
uint32_t edmac_read_chan = 0x19;
uint32_t edmac_write_chan = 0x11;
#endif

/* both channels get connected to this... lets call it service. it will just output the data it gets as input */
uint32_t dmaConnection = 6;

static void edmac_memcpy_init()
{
    edmac_memcpy_sem = create_named_semaphore("edmac_memcpy_sem", 1);
    edmac_read_done_sem = create_named_semaphore("edmac_read_done_sem", 0);
}

INIT_FUNC("edmac_memcpy", edmac_memcpy_init);

static void edmac_read_complete_cbr (int ctx)
{
    give_semaphore(edmac_read_done_sem);
}

static void edmac_write_complete_cbr (int ctx)
{
}

void* edmac_copy_rectangle_adv_start(void* dst, void* src, int src_width, int src_x, int src_y, int dst_width, int dst_x, int dst_y, int w, int h)
{
    take_semaphore(edmac_memcpy_sem, 0);
    
    /* see wiki, register map, EDMAC what the flags mean. they are for setting up copy block size */
    uint32_t dmaFlags = 0x20001000;

    /* create a memory suite from a already existing (continuous) memory block with given size. */
    uint32_t src_adjusted = ((uint32_t)src & 0x1FFFFFFF) + src_x + src_y * src_width;
    uint32_t dst_adjusted = ((uint32_t)dst & 0x1FFFFFFF) + dst_x + dst_y * dst_width;
    
    /* only read channel will emit a callback when reading from memory is done. write channels would just continue */
    RegisterEDmacCompleteCBR(edmac_read_chan, &edmac_read_complete_cbr, 0);
    RegisterEDmacAbortCBR(edmac_read_chan, &edmac_read_complete_cbr, 0);
    RegisterEDmacPopCBR(edmac_read_chan, &edmac_read_complete_cbr, 0);
    RegisterEDmacCompleteCBR(edmac_write_chan, &edmac_write_complete_cbr, 0);
    RegisterEDmacAbortCBR(edmac_write_chan, &edmac_write_complete_cbr, 0);
    RegisterEDmacPopCBR(edmac_write_chan, &edmac_write_complete_cbr, 0);
    
    /* connect the selected channels to 6 so any data read from RAM is passed to write channel */
    ConnectWriteEDmac(edmac_write_chan, dmaConnection);
    ConnectReadEDmac(edmac_read_chan, dmaConnection);
    
    /* xb is width */
    /* yb is height-1 (number of repetitions) */
    /* off1b is the number of bytes to skip after every xb bytes being transferred */
    src_edmac_info.xb = w;
    src_edmac_info.yb = h-1;
    src_edmac_info.off1b = src_width - w;
    
    /* destination setup has no special cropping */
    dst_edmac_info.xb = w;
    dst_edmac_info.yb = h-1;
    dst_edmac_info.off1b = dst_width - w;
    
    SetEDmac(edmac_read_chan, (void*)src_adjusted, &src_edmac_info, dmaFlags);
    SetEDmac(edmac_write_chan, (void*)dst_adjusted, &dst_edmac_info, dmaFlags);
    
    /* start transfer. no flags for write, 2 for read channels */
    StartEDmac(edmac_write_chan, 0);
    StartEDmac(edmac_read_chan, 2);
    
    return dst;
}

void edmac_copy_rectangle_adv_finish()
{
    /* wait until read is finished */
    take_semaphore(edmac_read_done_sem, 0);

    /* set default CBRs again and stop both DMAs */
    UnregisterEDmacCompleteCBR(edmac_read_chan);
    UnregisterEDmacAbortCBR(edmac_read_chan);
    UnregisterEDmacPopCBR(edmac_read_chan);
    UnregisterEDmacCompleteCBR(edmac_write_chan);
    UnregisterEDmacAbortCBR(edmac_write_chan);
    UnregisterEDmacPopCBR(edmac_write_chan);

    give_semaphore(edmac_memcpy_sem);
}

void* edmac_copy_rectangle_adv(void* dst, void* src, int src_width, int src_x, int src_y, int dst_width, int dst_x, int dst_y, int w, int h)
{
    void* ans = edmac_copy_rectangle_adv_start(dst, src, src_width, src_x, src_y, dst_width, dst_x, dst_y, w, h);
    edmac_copy_rectangle_adv_finish();
    return ans;
}

void* edmac_copy_rectangle(void* dst, void* src, int src_width, int x, int y, int w, int h)
{
    return edmac_copy_rectangle_adv(dst, src, src_width, x, y, w, 0, 0, w, h);
}

void* edmac_copy_rectangle_start(void* dst, void* src, int src_width, int x, int y, int w, int h)
{
    return edmac_copy_rectangle_adv_start(dst, src, src_width, x, y, w, 0, 0, w, h);
}

void edmac_copy_rectangle_finish()
{
    return edmac_copy_rectangle_adv_finish();
}

void* edmac_memset(void* dst, int value, size_t length)
{
    uint32_t blocksize = 64;
    uint32_t leading = MIN(length, (blocksize - ((uint32_t)dst % blocksize)) % blocksize);
    uint32_t trailing = (length - leading) % blocksize;
    uint32_t copyable = length - leading - trailing;
    
    /* less makes no sense as it would most probably be slower */
    if(copyable < 8 * blocksize)
    {
        return memset(dst, value, length);
    }
    
    uint32_t copies = copyable / blocksize - 1;
    
    /* fill the first line to have a copy source */
    memset((uint32_t)dst + leading, value, blocksize);
    
    /* now copy the first line over the next lines */
    edmac_copy_rectangle_adv_start((uint32_t)dst + leading + blocksize, (uint32_t)dst + leading, 0, 0, 0, blocksize, 0, 0, blocksize, copies);
    
    /* leading or trailing bytes that edmac cannot handle? */
    if(leading)
    {
        memset(dst, value, leading);
    }
    if(trailing)
    {
        memset((uint32_t)dst + length - trailing, value, trailing);
    }

    edmac_copy_rectangle_adv_finish();
    
    return dst;
}

uint32_t edmac_find_divider(size_t length)
{
    int blocksize = 4096;
    
    /* find a fitting 2^x divider */
    while((blocksize > 0) && (length % blocksize))
    {
        blocksize >>= 1;
    }
    
    /* could not find a fitting divider */
    if(!blocksize)
    {
        return 0;
    }
    
    return blocksize;
}

void* edmac_memcpy(void* dst, void* src, size_t length)
{
    int blocksize = edmac_find_divider(length);
    
    if(!blocksize)
    {
        return memcpy(dst, src, length);
    }
    
    return edmac_copy_rectangle_adv(dst, src, blocksize, 0, 0, blocksize, 0, 0, blocksize, length / blocksize);
}

void* edmac_memcpy_start(void* dst, void* src, size_t length)
{
    int blocksize = edmac_find_divider(length);
    
    if(!blocksize)
    {
        void * ret = memcpy(dst, src, length);
        /* simulate a started copy operation */
        take_semaphore(edmac_memcpy_sem, 0);
        give_semaphore(edmac_read_done_sem);
        return ret;
    }
    
    return edmac_copy_rectangle_adv_start(dst, src, blocksize, 0, 0, blocksize, 0, 0, blocksize, length / blocksize);
}

void edmac_memcpy_finish()
{
    return edmac_copy_rectangle_adv_finish();
}

#endif
