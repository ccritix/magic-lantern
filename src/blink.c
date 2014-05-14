#include "dryos.h"

/* bit 0:     ........****........ */
/* bit 1:     ..****************.. */

#define BIT_DURATION 4000000
#define BIT_ZERO_DURATION (BIT_DURATION/5)
#define BIT_ONE_DURATION (BIT_DURATION - BIT_ZERO_DURATION)

void __attribute__((optimize("-O0"))) busy_wait(int count)
{
    for (int i = 0; i < count; i++)
        asm("nop");
}

void blink_bit(int bit)
{
    if (bit)
    {
        busy_wait(BIT_ZERO_DURATION/2);
        _card_led_on();
        busy_wait(BIT_ONE_DURATION);
        _card_led_off();
        busy_wait(BIT_ZERO_DURATION/2);
    }
    else
    {
        busy_wait(BIT_ONE_DURATION/2);
        _card_led_on();
        busy_wait(BIT_ZERO_DURATION);
        _card_led_off();
        busy_wait(BIT_ONE_DURATION/2);
    }
}

void blink_char(char ch)
{
    for (int i = 0; i < 8; i++)
    {
        blink_bit(ch & (1 << i));
    }
}

void blink_str(char* str)
{
    blink_char(0xA5);
    for (char* c = str; *c; c++)
    {
        blink_char(*c);
    }
    blink_char(0xA5);
}
