
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "unpack_block.h"


struct dump_header
{
    uint32_t address;
    uint32_t crc_data;
    uint16_t length;
    uint16_t crc_header;
} __attribute__((packed));

uint32_t blocksize = 0;

/*
 * CRC code seems to come from linux's crc16.c, which is GPL v2.
 * So all of CHDK's LED blink code has to be GPLed.
 */

/** CRC table for the CRC-16. The poly is 0x8005 (x^16 + x^15 + x^2 + 1) */
static const uint16_t crc16_table[256] = {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

static uint16_t crc16_byte(uint16_t crc, const uint8_t data)
{
    return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

static uint16_t crc16(uint16_t crc, void *addr, uint32_t len)
{
    uint8_t *buffer = (uint8_t *)addr;

	while(len--)
    {
		crc = crc16_byte(crc, *buffer++);
    }
	return crc;
}

#define CRC32MASKREV 0xEDB88320
static uint32_t crc32_table[256];
static uint32_t crc32(uint32_t crc, void *addr, uint32_t len)
{
    uint8_t *buffer = (uint8_t *)addr;
    
    /* only run the first time. index 0 has zero value content, so check index 1 */
    if(!crc32_table[1])
    {
        for(uint32_t byte = 0; byte < 256; byte++)
        {
            uint32_t value = byte;
            for (int32_t j = 7; j >= 0; j--)
            {
                uint32_t mask = -(value & 1);
                value = (value >> 1) ^ (CRC32MASKREV & mask);
            }
            crc32_table[byte] = value;
        }
    }

    crc = ~crc;
    for(uint32_t pos = 0; pos < len; pos++)
    {
        crc = (crc >> 8) ^ crc32_table[(crc ^ buffer[pos]) & 0xFF];
    }
    return ~crc;
}

static void flip_bit(uint8_t *block_data, uint32_t bitnum)
{
    block_data[bitnum / 8] ^= 0x80 >> (bitnum % 8);
}

uint32_t test_correct_data(uint8_t *block_data)
{
    struct dump_header *header = (struct dump_header *)block_data;
    uint8_t *payload = &block_data[sizeof(struct dump_header)];
    uint32_t ret = 0;
    
    /* calc CRC for the block being sent */
    uint32_t crc = crc32(0, payload, header->length);
    
    if(header->crc_data == crc)
    {
        ret = 1;
    }
    
    return ret;
}

static uint32_t try_correct_data(uint8_t *block_data, uint32_t startbit, uint32_t depth)
{
    uint32_t bitnum = startbit;
    
    while(bitnum < blocksize * 8)
    {
        flip_bit(block_data, bitnum);
        
        if(!depth)
        {
            if(test_correct_data(block_data))
            {
                printf("  Bit #%d error, fixed\n", bitnum);
                return 1;
            }
        }
        else
        {
            if(try_correct_data(block_data, bitnum + 1, depth - 1))
            {
                printf("  Bit #%d error, fixed\n", bitnum);
                return 1;
            }
        }
        
        flip_bit(block_data, bitnum);
        
        bitnum++;
    }
    
    return 0;
}

uint32_t unpack_block(uint8_t *block_data, uint8_t *out_data, uint32_t *out_length, uint32_t *out_address)
{
    struct dump_header *header = (struct dump_header *)block_data;
    uint8_t *payload = &block_data[sizeof(struct dump_header)];

    if(!test_correct_data(block_data))
    {
#if !defined(MODULE)
        printf("0x%08X: Data CRC error, trying to correct\n", header->address);
        
        if(!try_correct_data(block_data, sizeof(struct dump_header) * 8, 0))
        {
            if(!try_correct_data(block_data, sizeof(struct dump_header) * 8, 1))
            {
                printf("  [ERROR] correction failed\n");
                return 0;
            }
        }
#else
        return 0;
#endif
    }
    
    if(out_address)
    {
        *out_address = header->address;
    }
    if(out_length)
    {
        *out_length = header->length;
    }
    if(out_data)
    {
        memcpy(out_data, payload, header->length);
    }

    return header->length;
}

uint32_t test_correct_header(uint8_t *header)
{
    return ((struct dump_header *)header)->crc_header == crc16(0, header, 10);
}

#if !defined(MODULE)


uint32_t try_correct_header(uint8_t *header, uint32_t startbit, uint32_t depth)
{
    uint32_t bitnum = 0;
    
    while(bitnum < sizeof(struct dump_header) * 8)
    {
        flip_bit(header, bitnum);
        
        if(!depth)
        {
            if(test_correct_header(header))
            {
                printf("  Bit #%d error, fixed\n", bitnum);
                return 1;
            }
        }
        else
        {
            if(try_correct_header(header, bitnum + 1, depth - 1))
            {
                printf("  Bit #%d error, fixed\n", bitnum);
                return 1;
            }
        }
        
        flip_bit(header, bitnum);
        
        bitnum++;
    }
}

uint32_t check_header(struct dump_header *header)
{
    if(test_correct_header((uint8_t *)header))
    {
        if(!blocksize)
        {
            blocksize = header->length;
            printf("First header received, block size is %d byte\n", blocksize);
        }
        return 1;
    }
    
    printf("0x%08X: Header CRC error, trying to correct\n", header->address);
    
    if(!try_correct_header((uint8_t *)header, 0, 0))
    {
        if(!try_correct_header((uint8_t *)header, 0, 1))
        {
            if(!try_correct_header((uint8_t *)header, 0, 2))
            {
                printf("  [ERROR] could not fix header\n");
                return 0;
            }
        }
    }
    
    return 1;
}

uint32_t unpack_file(FILE *in_file, FILE *out_file)
{
    struct dump_header header;
    uint32_t block = 0;
    uint32_t base_address = 0xFFFFFFFF;
    uint32_t next_address = 0xFFFFFFFF;
    
    while(!feof(in_file))
    {
        block++;
        
        uint32_t read_pos = ftell(in_file);
        if(fread(&header, sizeof(struct dump_header), 1, in_file) != 1)
        {
            printf("Dumped 0x%08X-0x%08X\n", base_address, next_address);
            return 0;
        }
        fseek(in_file, read_pos, SEEK_SET);
        
        if(!check_header(&header))
        {
            printf("Header CRC failed\n");
            fseek(in_file, read_pos + blocksize + sizeof(struct dump_header), SEEK_SET);
            continue;
        }
        
        uint32_t in_block_size = blocksize + sizeof(struct dump_header);
        uint8_t *in_buffer = malloc(in_block_size);
        uint8_t *out_buffer = malloc(header.length);
        uint32_t length = 0;
        uint32_t address = 0;

        if(fread(in_buffer, in_block_size, 1, in_file) != 1)
        {
            printf("Read failed at block %d\n", block);
            printf("Dumped 0x%08X-0x%08X\n", base_address, next_address);
            return 0;
        }
        
        if(!unpack_block(in_buffer, out_buffer, &length, &address))
        {
            if(next_address == header.address)
            {
                address = header.address;
                length = blocksize;
                printf("  => Unpack failed for 0x%X bytes at 0x%08X-0x%08X, assuming 0xA5\n", length, next_address, next_address + blocksize - 1);
                memset(out_buffer, 0xA5, length);
            }
            else
            {
                printf("Unpack failed for block with probably incorrect address 0x%08X\n", header.address);
                continue;
            }
        }
        
        if(base_address == 0xFFFFFFFF)
        {
            base_address = address;
            printf("Base address is 0x%08X\n", base_address);
        }
        else
        {
            if(address > next_address)
            {
                uint32_t missing_len = address - next_address;
                printf("Missing block of 0x%X bytes at 0x%08X-0x%08X, assuming 0xFF\n", missing_len, next_address, address - 1);
                
                fseek(out_file, next_address - base_address, SEEK_SET);
                uint8_t *missing = malloc(missing_len);
                
                memset(missing, 0xFF, missing_len);
                
                if(fwrite(missing, missing_len, 1, out_file) != 1)
                {
                    printf("Write failed\n");
                    return 0;
                }
                free(missing);
            }
            else if(address < next_address)
            {
                printf("Seeked back from expected 0x%08X to 0x%08X, assuming this is a concatenated file\n", next_address, address);
            }
        }
        
        fseek(out_file, address - base_address, SEEK_SET);
        if(fwrite(out_buffer, length, 1, out_file) != 1)
        {
            printf("Write failed\n");
            return 0;
        }
        
        free(in_buffer);
        free(out_buffer);
        
        next_address = address + length;
    }
    
    return 1;
}


int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Please call with <infile> <outfile>\n");
        return -1;
    }
    
    FILE *in_file = fopen(argv[1], "rb");
    FILE *out_file = fopen(argv[2], "w+");
    
    if(!in_file)
    {
        printf("Failed to open %s\n", argv[1]);
        return -1;
    }
    
    if(!out_file)
    {
        printf("Failed to open %s\n", argv[1]);
        return -1;
    }
    
    unpack_file(in_file, out_file);
    
    fclose(in_file);
    fclose(out_file);
}


#endif

