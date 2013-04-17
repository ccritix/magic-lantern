/** 
 * For decoding 14-bit RAW
 * 
 **/

/**
* RAW pixels (document mode, as with dcraw -D -o 0):

    01 23 45 67 89 AB ... (SENSOR_RES_X-1)
    ab cd ef gh ab cd ...

    v-------------------------- first pixel should be red
0   RG RG RG RG RG RG ...   <-- first line (even)
1   GB GB GB GB GB GB ...   <-- second line (odd)
2   RG RG RG RG RG RG ...
3   GB GB GB GB GB GB ...
...
SENSOR_RES_Y-1
*/

/**
* 14-bit encoding:

hi          lo
aaaaaaaaaaaaaabb
bbbbbbbbbbbbcccc
ccccccccccdddddd
ddddddddeeeeeeee
eeeeeeffffffffff
ffffgggggggggggg
gghhhhhhhhhhhhhh
*/

/* group 8 pixels in 14 bytes to simplify decoding */
struct raw_pixblock
{
    unsigned int b_hi: 2;
    unsigned int a: 14;     // even lines: red; odd lines: green
    unsigned int c_hi: 4;
    unsigned int b_lo: 12;
    unsigned int d_hi: 6;
    unsigned int c_lo: 10;
    unsigned int e_hi: 8;
    unsigned int d_lo: 8;
    unsigned int f_hi: 10;
    unsigned int e_lo: 6;
    unsigned int g_hi: 12;
    unsigned int f_lo: 4;
    unsigned int h: 14;     // even lines: green; odd lines: blue
    unsigned int g_lo: 2;
} __attribute__((packed));

/* a full line of pixels */
typedef struct raw_pixblock raw_pixline[SENSOR_RES_X / 8];

/* get a red pixel near the specified coords (approximate) */
static inline int raw_red_pixel(struct raw_pixblock * buf, int x, int y)
{
    y = (y/2) * 2;
    int i = ((y * SENSOR_RES_X + x) / 8);
    return buf[i].a;
}

/* get a green pixel near the specified coords (approximate) */
static inline int raw_green_pixel(struct raw_pixblock * buf, int x, int y)
{
    y = (y/2) * 2;
    int i = ((y * SENSOR_RES_X + x) / 8);
    return buf[i].h;
}

/* get a blue pixel near the specified coords (approximate) */
static inline int raw_blue_pixel(struct raw_pixblock * buf, int x, int y)
{
    y = (y/2) * 2 + 1;
    int i = ((y * SENSOR_RES_X + x) / 8);
    return buf[i].h;
}
