#include "edmac-memcpy.h"
#include "dryos.h"
#include "edmac.h"

#ifdef CONFIG_EDMAC_MEMCPY

static struct semaphore * edmac_memcpy_sem = 0; /* to allow only one memcpy running at a time */
static struct semaphore * edmac_memcpy_done_sem = 0; /* to know when memcpy is finished */

static void edmac_memcpy_init()
{
    edmac_memcpy_sem = create_named_semaphore("edmac_memcpy_sem", 1);
    edmac_memcpy_done_sem = create_named_semaphore("edmac_memcpy_done_sem", 0);
}

INIT_FUNC("edmac_memcpy", edmac_memcpy_init);

static void edmac_memcpy_complete_cbr (int ctx)
{
    give_semaphore(edmac_memcpy_done_sem);
}

void* edmac_memcpy(void* dst, void* src, size_t length)
{
    take_semaphore(edmac_memcpy_sem, 0);

    if(length % 4096)
    {
        length &= ~4095;
    }
    
    /* pick some free (check using debug menu) EDMAC channels write: 0x00-0x06, 0x10-0x16, 0x20-0x21. read: 0x08-0x0D, 0x18-0x1D,0x28-0x2B */
    uint32_t dmaChannelRead = 0x19;
    uint32_t dmaChannelWrite = 0x11;

    /* both channels get connected to this... lets call it service. it will just output the data it gets as input */
    uint32_t dmaConnection = 6;

    /* see wiki, register map, EDMAC what the flags mean. they are for setting up copy block size */
    uint32_t dmaFlags = 0x20001000;

    /* create a memory suite from a already existing (continuous) memory block with given size. */
    struct memSuite *memSuiteSource = CreateMemorySuite(src, length, 0);
    struct memSuite *memSuiteDest = CreateMemorySuite(dst, length, 0);

    /* only read channel will emit a callback when reading from memory is done. write channels would just silently wrap */
    PackMem_RegisterEDmacCompleteCBRForMemorySuite(dmaChannelRead, &edmac_memcpy_complete_cbr, 0);

    /* connect the selected channels to 6 so any data read from RAM is passed to write channel */
    ConnectWriteEDmac(dmaChannelWrite, dmaConnection);
    ConnectReadEDmac(dmaChannelRead, dmaConnection);

    /* setup EDMAC driver to handle memory suite copy. check return codes for being zero (OK)! if !=0 then the suite size was not a multiple of 4096 */
    int err = PackMem_SetEDmacForMemorySuite(dmaChannelWrite, memSuiteDest , dmaFlags);
    err |= PackMem_SetEDmacForMemorySuite(dmaChannelRead, memSuiteSource , dmaFlags);

    if(err)
    {
        return 0;
    }
    
    /* start transfer. no flags for write, 2 for read channels */
    PackMem_StartEDmac(dmaChannelWrite, 0);
    PackMem_StartEDmac(dmaChannelRead, 2);
    take_semaphore(edmac_memcpy_done_sem, 0);

    /* cleanup */
    PackMem_PopEDmacForMemorySuite(dmaChannelRead);
    PackMem_PopEDmacForMemorySuite(dmaChannelWrite);
    DeleteMemorySuite(memSuiteSource);
    DeleteMemorySuite(memSuiteDest);

    give_semaphore(edmac_memcpy_sem);
    return dst;
}

void* edmac_copy_rectangle(void* dst, void* src, int src_width, int x, int y, int w, int h)
{
    take_semaphore(edmac_memcpy_sem, 0);
    
    /* pick some free (check using debug menu) EDMAC channels write: 0x00-0x06, 0x10-0x16, 0x20-0x21. read: 0x08-0x0D, 0x18-0x1D,0x28-0x2B */
    uint32_t dmaChannelRead = 0x19;
    uint32_t dmaChannelWrite = 0x11;

    /* both channels get connected to this... lets call it service. it will just output the data it gets as input */
    uint32_t dmaConnection = 6;

    /* see wiki, register map, EDMAC what the flags mean. they are for setting up copy block size */
    uint32_t dmaFlags = 0x20001000;

    /* create a memory suite from a already existing (continuous) memory block with given size. */
    int src_length = (src_width * h) & ~4095;
    int dst_length = (w * h) & ~4095;
    int src_adjusted = src + x + y * src_width;
    struct memSuite *memSuiteSource = CreateMemorySuite(src_adjusted, src_length, 0);
    struct memSuite *memSuiteDest = CreateMemorySuite(dst, dst_length, 0);

    /* only read channel will emit a callback when reading from memory is done. write channels would just silently wrap */
    PackMem_RegisterEDmacCompleteCBRForMemorySuite(dmaChannelRead, &edmac_memcpy_complete_cbr, 0);

    /* connect the selected channels to 6 so any data read from RAM is passed to write channel */
    ConnectWriteEDmac(dmaChannelWrite, dmaConnection);
    ConnectReadEDmac(dmaChannelRead, dmaConnection);

    /* setup EDMAC driver to handle memory suite copy. check return codes for being zero (OK)! if !=0 then the suite size was not a multiple of 4096 */
    int err = PackMem_SetEDmacForMemorySuite(dmaChannelWrite, memSuiteDest , dmaFlags);
    err |= PackMem_SetEDmacForMemorySuite(dmaChannelRead, memSuiteSource , dmaFlags);

    if(err)
    {
        beep();
        return 0;
    }

    /* xb is width */
    /* yb is height-1 */
    /* off1b is cropping (or padding) on the right side of the image */
    static struct edmac_info edmac_info;
    memset(&edmac_info, sizeof(struct edmac_info), 0);
    edmac_info.xb = w;
    edmac_info.yb = h-1;
    edmac_info.off1b = src_width - w;
    SetEDmac(dmaChannelRead, src_adjusted, &edmac_info, dmaFlags);
    
    /* start transfer. no flags for write, 2 for read channels */
    PackMem_StartEDmac(dmaChannelWrite, 0);
    PackMem_StartEDmac(dmaChannelRead, 2);
    take_semaphore(edmac_memcpy_done_sem, 0);

    /* cleanup */
    PackMem_PopEDmacForMemorySuite(dmaChannelRead);
    PackMem_PopEDmacForMemorySuite(dmaChannelWrite);
    DeleteMemorySuite(memSuiteSource);
    DeleteMemorySuite(memSuiteDest);

    give_semaphore(edmac_memcpy_sem);
    return dst;
}

#endif
