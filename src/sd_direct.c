
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "consts.h"

#define MEM(x) (*(volatile uint32_t *)(x))

void sdWriteCmd(uint32_t cmd, uint32_t param, uint32_t flags)
{
    if(flags & 0x02)
    {
        /* some config for parametrizing response wait time/format? */
        MEM(0xC0C10028) = 0x30;
        MEM(0xC0C1002C) = 0x2701;
    }
    MEM(0xC0C10024) = (cmd << 8) | (param >> 24) | 0x4000;
    MEM(0xC0C10020) = (param << 8) | 1;
    /* reset flags */
    MEM(0xC0C10010) = 0;
    MEM(0xC0C1000C) = flags;
}

/* returns 0 on success */
uint32_t sdWaitTransfer()
{
    volatile uint32_t loops = 200000;

    while(!(MEM(0xC0C10010) & 3))
    {
        if(!loops--)
        {
            return 2;
        }
    }

    if(MEM(0xC0C10010) & 2)
    {
        return 1;
    }

    return 0;
}

/* returns 0 on success */
uint32_t sdWaitDmaTransfer()
{
    volatile uint32_t loops = 200000;

    while(MEM(0xC0510070) & 1)
    {
        if(!loops--)
        {
            return 1;
        }
    }

    return 0;
}

uint32_t sdWriteCmdWait(uint32_t cmd, uint32_t param, uint32_t flags)
{
    sdWriteCmd(cmd, param, flags);

    return sdWaitTransfer();
}

void sdDmaSetup(void *buffer, uint32_t size, uint32_t write)
{
    /* set DMA memory address */
    MEM(0xC0510060) = ((uint32_t)buffer);
    /* set DMA byte count */
    MEM(0xC0510064) = size;
    /* unknown */
    MEM(0xC0510078) = 0;
    /* arm DMA */
    MEM(0xC0510070) = 0x29 | (write ? 0x04 : 0x00) | ((((uint32_t)buffer) & 0x0F) ? 0x00 : 0x10);
}

void sdTransferSetup(uint32_t block_count, uint32_t block_size, uint32_t write)
{
    /* set DMA transfer block size */
    if(write)
    {
        MEM(0xC0C1005C) = block_size;
    }
    else
    {
        MEM(0xC0C10068) = block_size;
    }

    /* set DMA transfer block count */
    MEM(0xC0C1007C) = block_count;

    /* unknown */
    MEM(0xC0C10088) = 3;
    /* use DMA transfer */
    MEM(0xC0C10008) = 1;
}

/* returns the upper 32 bits of the card response, thus cutting off the CRC */
uint32_t sdGetResponse()
{
    uint64_t response = 0;

    response |= (uint64_t)MEM(0xC0C10034);
    response |= (uint64_t)MEM(0xC0C10038) << 32;

    return (uint32_t)(response >> 8);
}


uint32_t sdStopTransmission(uint32_t rca)
{
    uint32_t loops = 10000;

    sdWriteCmdWait(12, 0, 0x12);

    while(loops-- > 0)
    {
        sdWriteCmdWait(13, rca << 16, 0x12);
        uint32_t response = sdGetResponse();

        /* wait for card being ready for data again */
        if(response & 0x100)
        {
            return 0;
        }
    }

    return 1;
}

uint32_t sdTransmitBlocks(uint32_t sector, uint32_t count, uint8_t *buffer, uint32_t write)
{
    uint32_t ret = 0;

    *(volatile int*) (CARD_LED_ADDRESS) = (LEDON);
    
    /* get the RCA. could be cached */
    static uint32_t rca = 0xFFFFFFFF;
    
    if(rca == 0xFFFFFFFF)
    {
        sdWriteCmdWait(3, 0, 0x12);
        rca = (sdGetResponse() >> 16) & 0xFFFF;
    }

    sdTransferSetup(count, 0x200, write);
    sdDmaSetup(buffer, 0x200 * count, write);

    uint32_t command = write ? 24 : 17;
    uint32_t flags = write ? 0x13 : 0x14;
    
    if(count > 1)
    {
        sdWriteCmd(command + 1, sector, flags);
    }
    else
    {
        sdWriteCmd(command, sector, flags);
    }

    if(sdWaitDmaTransfer())
    {
        ret |= 1;
    }
    *(volatile int*) (CARD_LED_ADDRESS) = (LEDOFF);

    if(sdWaitTransfer())
    {
        ret |= 2;
    }

    if(sdStopTransmission(rca))
    {
        ret |= 4;
    }

    /* check how many blocks were processed */
    if(MEM(0xC0C10080) != count)
    {
        ret |= 8;
    }

    MEM(0xC0C10008) = 0;
    MEM(0xC0C10010) = 0;

    
    return ret;
}


uint32_t sdReadBlocks(uint32_t sector, uint32_t count, uint8_t *buffer)
{
    //return sdTransmitBlocks(sector, count, buffer, 0);
    
    /* multi block transfers do not work well for some reason */
    uint32_t ret = 0;
    for(uint32_t num = 0; num < count; num++)
    {
        ret |= sdTransmitBlocks(sector + num, 1, &buffer[num * 0x200], 0);
    }
    return ret;
    
}

uint32_t sdWriteBlocks(uint32_t sector, uint32_t count, uint8_t *buffer)
{
    //return sdTransmitBlocks(sector, count, buffer, 1);
    
    /* multi block transfers do not work well for some reason */
    uint32_t ret = 0;
    for(uint32_t num = 0; num < count; num++)
    {
        ret |= sdTransmitBlocks(sector + num, 1, &buffer[num * 0x200], 1);
    }
    return ret;
    
}
