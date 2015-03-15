
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "disp_direct.h"
#include "sd_direct.h"
#include "compiler.h"
#include "consts.h"

#define MEM(x) (*(volatile uint32_t *)(x))

/* external functions */
void print_line(uint32_t color, uint32_t scale, char *txt);
uint32_t data_abort_occurred();


volatile uint32_t *sd_register(struct sd_ctx *ctx, uint32_t offset)
{
    return (uint32_t *)(0xC0C00000 + (ctx->sd_port * 0x00010000) + offset);
}

volatile uint32_t *sd_dma_register(struct sd_ctx *ctx, uint32_t offset)
{
    return (uint32_t *)(0xC0500060 + (ctx->dma_port * 0x00010000) + offset);
}

void sd_write_cmd(struct sd_ctx *ctx, uint32_t cmd, uint32_t param, uint32_t flags)
{
    if((flags & 0x0F) == 0x02)
    {
        /* some config for parametrizing response wait time/format? */
        *sd_register(ctx, 0x0028) = 0x30;
        *sd_register(ctx, 0x002C) = 0x2701;
    }
    
    *sd_register(ctx, 0x0024) = (cmd << 8) | (param >> 24) | 0x4000;
    *sd_register(ctx, 0x0020) = (param << 8) | 1;
    
    /* reset flags */
    *sd_register(ctx, 0x0010) = 0;
    *sd_register(ctx, 0x0014) = 3;
    
    /* start transfer */
    *sd_register(ctx, 0x000C) = flags;
}

uint32_t sd_get_error(struct sd_ctx *ctx)
{
    return *sd_register(ctx, 0x0084);
}

/* returns 0 on success */
uint32_t sd_wait_transfer(struct sd_ctx *ctx)
{
    volatile uint32_t loops = 50000;

    /* wait for either ERROR or DONE bit being set */
    while(!(*sd_register(ctx, 0x0010) & 3))
    {
        if(!loops--)
        {
            *sd_register(ctx, 0x0014) = 0;
            return 2;
        }
    }

    /* reset some flags */
    *sd_register(ctx, 0x0014) = 0;
    
    /* check if there was an error */
    if(*sd_register(ctx, 0x0010) & 2)
    {
        return 1;
    }

    return 0;
}

/* returns 0 on success */
uint32_t sd_wait_dma_transfer(struct sd_ctx *ctx)
{
    volatile uint32_t loops = 5000000;

    /* wait until DMA is finished. if the card is slow, this might take some time. but do not lock up. */
    while(*sd_dma_register(ctx, 0x0010) & 1)
    {
        if(!loops--)
        {
            return 1;
        }
    }

    return 0;
}

uint32_t sd_write_cmd_wait(struct sd_ctx *ctx, uint32_t cmd, uint32_t param, uint32_t flags)
{
    sd_write_cmd(ctx, cmd, param, flags);

    uint32_t err = sd_wait_transfer(ctx);

    /* no error */
    if(!err)
    {
        return 0;
    }
    
    /* something went wrong */
    uint32_t ret = 0;
    ret |= 1;
    ret |= err << 8;
    ret |= sd_get_error(ctx) << 16;
    
    return ret;
}

void sd_dma_start(struct sd_ctx *ctx, void *buffer, uint32_t size, uint32_t write)
{
    /* set DMA memory address */
    *sd_dma_register(ctx, 0x0000) = ((uint32_t)buffer);
    /* set DMA byte count */
    *sd_dma_register(ctx, 0x0004) = size;
    /* unknown */
    *sd_dma_register(ctx, 0x0018) = 0;
    /* arm DMA */
    *sd_dma_register(ctx, 0x0010) = 0x29 | (write ? 0x04 : 0x00) | ((((uint32_t)buffer) & 0x0F) ? 0x00 : 0x10);
}

void sd_transfer_setup(struct sd_ctx *ctx, uint32_t block_count, uint32_t block_size, uint32_t write)
{
    /* set DMA transfer block size */
    *sd_register(ctx, 0x005C) = block_size;
    *sd_register(ctx, 0x0068) = block_size;

    /* set DMA transfer block count */
    *sd_register(ctx, 0x007C) = block_count;

    /* unknown */
    *sd_register(ctx, 0x0088) = 3;
    /* use DMA transfer */
    *sd_register(ctx, 0x0008) = 1;
}

/* returns the upper 32 bits of the card response, thus cutting off the CRC */
uint32_t sd_get_response(struct sd_ctx *ctx)
{
    uint64_t response = 0;

    response |= (uint64_t)*sd_register(ctx, 0x0034);
    response |= (uint64_t)*sd_register(ctx, 0x0038) << 32;

    return (uint32_t)(response >> 8);
}


uint32_t sd_stop_transmission(struct sd_ctx *ctx, uint32_t rca)
{
    uint32_t loops = 10000;

    sd_write_cmd_wait(ctx, 12, 0, 0x12);

    while(loops-- > 0)
    {
        sd_write_cmd_wait(ctx, 13, rca << 16, 0x12);
        uint32_t response = sd_get_response(ctx);

        /* wait for card being ready for data again */
        if(response & 0x100)
        {
            return 0;
        }
    }

    return 1;
}

uint32_t sd_transmit(struct sd_ctx *ctx, uint32_t sector, uint32_t count, uint8_t *buffer, uint32_t write)
{
    uint32_t ret = 0;
    uint32_t err = 0;

    #ifdef CARD_LED_ADDRESS
    *(volatile int*) (CARD_LED_ADDRESS) = (LEDON);
    #endif
    
    /* get the RCA. could be cached */
    static uint32_t rca = 0xFFFFFFFF;
    
#if defined(CONFIG_600D)
    rca = MEM(0x10D1B8);
#endif
#if defined(CONFIG_5D3)
    rca = MEM(0x10F394);
#endif
    
    if(rca == 0xFFFFFFFF)
    {
        err = sd_write_cmd_wait(ctx, 3, 0, 0x13);
        if(0&&err)
        {
            ret |= 0x10;
            ret |= err << 8;
            ret |= sd_get_error(ctx) << 16;
            return ret;
        }

        rca = (sd_get_response(ctx) >> 16) & 0xFFFF;
    }

    uint32_t flags = write ? 0x13 : 0x14;
    uint32_t command = (write ? 24 : 17) + ((count > 1) ? 1 : 0) ;
    
    sd_transfer_setup(ctx, count, 0x200, write);
    
    if(write)
    {
        sd_write_cmd(ctx, command, sector, flags);
        sd_dma_start(ctx, buffer, 0x200 * count, write);
    }
    else
    {
        sd_dma_start(ctx, buffer, 0x200 * count, write);
        sd_write_cmd(ctx, command, sector, flags);
    }

    err = sd_wait_dma_transfer(ctx);
    if(err)
    {
        ret |= 1;
        ret |= err << 8;
        ret |= sd_get_error(ctx) << 16;
        return ret;
    }

    err = sd_wait_transfer(ctx);
    if(err)
    {
        ret |= 2;
        ret |= err << 8;
        ret |= sd_get_error(ctx) << 16;
        return ret;
    }

    err = sd_stop_transmission(ctx, rca);
    if(err)
    {
        ret |= 4;
        ret |= err << 8;
        ret |= sd_get_error(ctx) << 16;
        return ret;
    }

    /* check how many blocks were processed */
    err = *sd_register(ctx, 0x0080);
    if(err != count)
    {
        ret |= 8;
        ret |= err << 8;
        ret |= sd_get_error(ctx) << 16;
        return ret;
    }

    /* not using DMA anymore */
    *sd_register(ctx, 0x0008) = 0;
    *sd_register(ctx, 0x0010) = 0;
    
    #ifdef CARD_LED_ADDRESS
    *(volatile int*) (CARD_LED_ADDRESS) = (LEDOFF);
    #endif
    
    return ret;
}


uint32_t sd_read(struct sd_ctx *ctx, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    return sd_transmit(ctx, sector, count, buffer, 0);
}

uint32_t sd_write(struct sd_ctx *ctx, uint32_t sector, uint32_t count, uint8_t *buffer)
{
    return sd_transmit(ctx, sector, count, buffer, 1);
}

uint32_t sd_init(struct sd_ctx *ctx)
{
#if defined(CONFIG_600D)
    ctx->dma_port = 1;
    ctx->sd_port = 1;
#endif

#if defined(CONFIG_5D3)
    ctx->dma_port = 1;
    ctx->sd_port = 1;
    
    /* not clean to call FROM code. but its about rescue operations, so its tolerable */
    void (*fromutil_card_init)() = (void (*)())0xFFFE34B0;
    fromutil_card_init();
#endif
    
    /* autodetection doesnt work */    
    return 0;
    

    char text[32];
    while(ctx->sd_port < 5)
    {
        
        /* access either works or it causes an exception that is catched */
        uint32_t ret = *sd_register(ctx, 0x10);
        
        /* no module there... */
        if(data_abort_occurred())
        {
            snprintf(text, 32, "   #%d: unused", ctx->sd_port);
            print_line(COLOR_RED, 1, text);
            ctx->sd_port++;
            ctx->dma_port++;
            continue;
        }
        
        /* yeah, there is a SD module, check SD card */
        ret = sd_write_cmd_wait(ctx, 2, 0, 0x14);
        
        if(ret)
        {
            snprintf(text, 32, "   #%d: CMD2 fail 0x%08X", ctx->sd_port, ret);
            print_line(COLOR_RED, 1, text);
            ctx->sd_port++;
            ctx->dma_port++;
            continue;
        }
        
        ret = sd_get_response(ctx);
        snprintf(text, 32, "   #%d: resp 0x%08X, using!", ctx->sd_port, ret);
        print_line(COLOR_GREEN, 1, text);
        
        return 0;
    }
    
    snprintf(text, 32, "   no suitable port found");
    print_line(COLOR_RED, 1, text);
    return 1;
}

