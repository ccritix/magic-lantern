#ifndef __blink_h__
#define __blink_h__

/* bit 0:     ........****........ */
/* bit 1:     ..****************.. */

void busy_wait(int count);
void blink_bit(int bit);
void blink_char(char ch);   /* LSB first */
void blink_str(char* str);

#endif
