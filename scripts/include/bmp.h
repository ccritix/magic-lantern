/**
 * Magic Lantern scripting API
 * Bitmap routines
 */

/** Print text on the bitmap overlay, with the specified font */
/** returns width in pixels */
int bmp_printf( int fontspec, int x, int y, const char* fmt, ... );

/** Color codes, copied from bmp.h */
#define COLOR_EMPTY             0x00 // total transparent
#define COLOR_WHITE             0x01 // Normal white
#define COLOR_BLACK             0x02
#define COLOR_CYAN              0x05
#define COLOR_GREEN1            0x06
#define COLOR_GREEN2            0x07
#define COLOR_RED               0x08 // normal red
#define COLOR_LIGHT_BLUE        0x09
#define COLOR_BLUE              0x0B // normal blue
#define COLOR_DARK_RED          0x0C
#define COLOR_MAGENTA           0x0E
#define COLOR_YELLOW            0x0F // normal yellow
#define COLOR_ORANGE            0x13
#define COLOR_ALMOST_BLACK      0x26
#define COLOR_ALMOST_WHITE      0x4F
#define COLOR_GRAY(percent) (38 + (percent) * 41 / 100) // e.g. COLOR_GRAY(50) is 50% gray

/** Font specifiers include the font, the fg color and bg color, shadow flag, text alignment, expanded/condensed */
#define FONT_MASK              0x00070000

/* shadow flag */
#define SHADOW_MASK            0x00080000
#define SHADOW_FONT(fnt) ((fnt) | SHADOW_MASK)

/* font alignment macros */
#define FONT_ALIGN_MASK        0x03000000
#define FONT_ALIGN_LEFT        0x00000000   /* anchor: left   */
#define FONT_ALIGN_CENTER      0x01000000   /* anchor: center */
#define FONT_ALIGN_RIGHT       0x02000000   /* anchor: right  */
#define FONT_ALIGN_JUSTIFIED   0x03000000   /* anchor: left   */

/* optional text width (for clipping, filling and justified) */
/* default: longest line from the string */
#define FONT_TEXT_WIDTH_MASK   0xFC000000
#define FONT_TEXT_WIDTH(width)  ((((width+8) >> 4) << 26) & FONT_TEXT_WIDTH_MASK) /* range: 0-1015; round to 6 bits */

/* when aligning text, fill the blank space with background color (so you get a nice solid box) */
#define FONT_ALIGN_FILL        0x00800000

/* expanded/condensed */
/* not yet implemented */
#define FONT_EXPAND_MASK       0x00700000
#define FONT_EXPAND(pix)       (((pix) << 20) & FONT_EXPAND_MASK) /* range: -4 ... +3 pixels per character */

#define FONT(font,fg,bg)        ( 0 \
        | ((font) & (0xFFFF0000)) \
        | ((bg) & 0xFF) << 8 \
        | ((fg) & 0xFF) << 0 \
)

/* font by ID */
#define FONT_DYN(font_id,fg,bg) FONT((font_id)<<16,fg,bg)

/* should match the font loading order from rbf_font.c, rbf_init */
#define FONT_MONO_12  FONT_DYN(0, 0, 0)
#define FONT_MONO_20  FONT_DYN(1, 0, 0)
#define FONT_SANS_23  FONT_DYN(2, 0, 0)
#define FONT_SANS_28  FONT_DYN(3, 0, 0)
#define FONT_SANS_32  FONT_DYN(4, 0, 0)

#define FONT_CANON    FONT_DYN(7, 0, 0) /* uses a different backend */

/* common fonts */
#define FONT_SMALL      FONT_MONO_12
#define FONT_MED        FONT_SANS_23
#define FONT_MED_LARGE  FONT_SANS_28
#define FONT_LARGE      FONT_SANS_32

/* retrieve fontspec fields */
#define FONT_ID(font) (((font) >> 16) & 0x7)
#define FONT_BG(font) (((font) & 0xFF00) >> 8)
#define FONT_FG(font) (((font) & 0x00FF) >> 0)
#define FONT_GET_TEXT_WIDTH(font)    (((font) >> 22) & 0x3F0)
#define FONT_GET_EXPAND_AMOUNT(font) (((font) >> 20) & 0x7)
