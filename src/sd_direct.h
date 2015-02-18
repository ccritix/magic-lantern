
#ifndef _SD_DIRECT_H_
#define _SD_DIRECT_H_


enum sd_error
{
    SD_ERROR_RESPONSE_TIMEOUT = 0x03,
    SD_ERROR_RESPONSE_CRC_ERROR = 0x04,
    SD_ERROR_RESPONSE_COMPARE_ERROR = 0x05
};


struct sd_ctx
{
    uint32_t sd_port;
    uint32_t dma_port;
};

uint32_t sd_read(struct sd_ctx *ctx, uint32_t sector, uint32_t count, uint8_t *buffer);
uint32_t sd_write(struct sd_ctx *ctx, uint32_t sector, uint32_t count, uint8_t *buffer);
uint32_t sd_init(struct sd_ctx *ctx);

#endif
