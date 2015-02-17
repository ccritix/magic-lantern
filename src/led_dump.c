
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "compiler.h"
#include "consts.h"

/* LED dumper code from http://chdk.wikia.com/wiki/Obtaining_a_firmware_dump */

#define DELAY_SYNC   8
#define DELAY_SPACE  4
#define DELAY0       1
#define DELAY1       4

/* successfully dumped data with slowdown 4 */
#define SLOWDOWN     4

void led_on();
void led_off();


struct dump_header
{
    uint32_t address;
    uint16_t blocksize;
} __attribute__((packed));


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

	while (len--)
    {
		crc = crc16_byte(crc, *buffer++);
    }
	return crc;
}

/* ensure we get maximum speed */
#pragma GCC optimize ("s")

static inline void nop()
{
    asm volatile ("nop\n");
}

static inline void idle()
{
    uint32_t i;

    for(i=0;i<0x78800;i++)
    {
        nop();
        nop();
        nop();
        nop();
    }
}

/* approx 10us steps */
static inline void delay(uint32_t value)
{
    uint32_t loops = value * 277 * SLOWDOWN;
    for(uint32_t i=0; i < loops; ++i)
    {
        nop();
        nop();
    }
}

static inline void do_bit(uint32_t bit)
{
    led_on();

	if(bit)
    {
	    delay(DELAY1);
	}
    else
    {
        delay(DELAY0);
    }
	led_off();

    delay(DELAY_SPACE);
}

static inline void send_byte(uint32_t data)
{
    delay(DELAY_SYNC);

    for(uint32_t bit = 0; bit < 8; bit++)
    {
        do_bit(data & (1<<bit));
    }
}

/* sends the data in little endian mode */
void send_word(uint32_t data)
{
    send_byte(data);
    send_byte(data>>8);
    send_byte(data>>16);
    send_byte(data>>24);
}

/* sends the data in little endian mode */
void send_half(uint32_t data)
{
    send_byte(data);
    send_byte(data>>8);
}

int block_empty(uint8_t *data, uint32_t length)
{
    for(uint32_t pos = 0; pos < length; pos++)
    {
        if(data[pos] != 0xFF)
        {
            return 0;
        }
    }

    return 1;
}

void send_data(void *addr, uint32_t length)
{
    uint8_t *buffer = (uint8_t *)addr;

    /* send data */
    for(uint32_t i = 0; i < length; i++)
    {
        send_byte(buffer[i]);
    }
}

void led_dump(uint32_t address, uint32_t length)
{
    uint16_t block_size = 512;
    uint32_t block_count = length / block_size;
    struct dump_header header;

    idle();

    /* send alternating bits to pre-charge capacitors in sound card */
    for(uint32_t loop = 0; loop < 20; loop++)
    {
        send_byte(0xaa);
    }

    /* now send real data */
    for(uint32_t block = 0; block < block_count; block++)
    {
        uint32_t block_address = address + block * block_size;

        /* skip empty blocks */
        if(block_empty((void *)block_address, block_size))
        {
            continue;
        }

        /* sync header */
        send_byte(0x0a);
        send_byte(0x55);
        send_byte(0xaa);
        send_byte(0x50);

        /* build block header */
        header.address = block_address;
        header.blocksize = block_size;

        /* calc CRC for the block being sent */
        uint16_t crc = crc16(0, &header, sizeof(struct dump_header));
        crc = crc16(crc, (void *)block_address, block_size);

        /* send block header */
        send_half(crc);
        send_data(&header, sizeof(struct dump_header));
        send_data((void *)block_address, block_size);
    }

    return;
}

