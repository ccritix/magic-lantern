#include "dryos.h"

/* bit 0:     ****.... */
/* bit 1:     **********.... */

#define BIT_DURATION 500000
#define BIT_ZERO_DURATION BIT_DURATION
#define BIT_ONE_DURATION (BIT_DURATION * 5/2)

void __attribute__((optimize("-O0"))) busy_wait(int count)
{
    for (int i = 0; i < count; i++)
        asm("nop");
}

void blink_bit(int bit)
{
    if (bit)
    {
        _card_led_on();
        busy_wait(BIT_ONE_DURATION);
        _card_led_off();
        busy_wait(BIT_ZERO_DURATION);
    }
    else
    {
        _card_led_on();
        busy_wait(BIT_ZERO_DURATION);
        _card_led_off();
        busy_wait(BIT_ZERO_DURATION);
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
    for (char* c = str; *c; c++)
    {
        blink_char(*c);
    }
}

void blink_init()
{ 
    /* send a sync signal, and give you some time to position the second camera to look at the blinking LED */
    for (int i = 0; i < 5; i++)
    {
        blink_char(0xA5);
        busy_wait(50000000);
    }
}
