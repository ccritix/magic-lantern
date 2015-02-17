
#ifndef _SD_DIRECT_H_
#define _SD_DIRECT_H_

uint32_t sdReadBlocks(uint32_t sector, uint32_t count, uint8_t *buffer);
uint32_t sdWriteBlocks(uint32_t sector, uint32_t count, uint8_t *buffer);

#endif
