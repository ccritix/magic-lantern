#ifndef __UNPACK_BLOCK_H__
#define __UNPACK_BLOCK_H__

uint32_t unpack_block(uint8_t *block_data, uint8_t *out_data, uint32_t *out_length, uint32_t *out_address);
uint32_t test_correct_header(uint8_t *header);
uint32_t test_correct_data(uint8_t *block_data);


#endif
