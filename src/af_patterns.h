#ifndef AF_PATTERNS_H_
#define AF_PATTERNS_H_

// some definitions from main.h 
#define IS_EOL(item) (item->_eol_)
#define END_OF_LIST  {_eol_ : TRUE}
#define TRUE  1
// done 

// AF patterns, from 400plus's main.h (codes are slightly different)
// These might be camera-specific, but codes are the same on 550D and 60D
// [5] Values for "af_point" (can be ORed together to form patterns)

#ifdef CONFIG_50D
#define AF_POINT_C  0x01 // Center
#define AF_POINT_T  0x02 // Top
#define AF_POINT_B  0x04 // Bottom
#define AF_POINT_TL 0x08 // Top-left
#define AF_POINT_TR 0x10 // Top-right
#define AF_POINT_BL 0x20 // Bottom-left
#define AF_POINT_BR 0x40 // Bottom-right
#define AF_POINT_L  0x80 // Left
#define AF_POINT_R  0x100 // Right
#else
#define AF_POINT_C  0x0100 // Center
#define AF_POINT_T  0x0200 // Top
#define AF_POINT_B  0x0400 // Bottom
#define AF_POINT_TL 0x0800 // Top-left
#define AF_POINT_TR 0x1000 // Top-right
#define AF_POINT_BL 0x2000 // Bottom-left
#define AF_POINT_BR 0x4000 // Bottom-right
#define AF_POINT_L  0x8000 // Left
#define AF_POINT_R  0x0001 // Right
#endif

#define AF_PATTERN_NONE            0

#define AF_PATTERN_CENTER          AF_POINT_C
#define AF_PATTERN_SQUARE          AF_POINT_C | AF_POINT_TL | AF_POINT_TR | AF_POINT_BL | AF_POINT_BR

#define AF_PATTERN_TOP             AF_POINT_T
#define AF_PATTERN_TOPTRIANGLE     AF_POINT_T | AF_POINT_TL | AF_POINT_TR
#define AF_PATTERN_TOPDIAMOND      AF_POINT_T | AF_POINT_TL | AF_POINT_TR | AF_POINT_C
#define AF_PATTERN_TOPHALF         AF_POINT_T | AF_POINT_TL | AF_POINT_TR | AF_POINT_C | AF_POINT_L | AF_POINT_R

#define AF_PATTERN_BOTTOM          AF_POINT_B
#define AF_PATTERN_BOTTOMTRIANGLE  AF_POINT_B | AF_POINT_BL | AF_POINT_BR
#define AF_PATTERN_BOTTOMDIAMOND   AF_POINT_B | AF_POINT_BL | AF_POINT_BR | AF_POINT_C
#define AF_PATTERN_BOTTOMHALF      AF_POINT_B | AF_POINT_BL | AF_POINT_BR | AF_POINT_C | AF_POINT_L | AF_POINT_R

#define AF_PATTERN_TOPLEFT         AF_POINT_TL
#define AF_PATTERN_TOPRIGHT        AF_POINT_TR
#define AF_PATTERN_BOTTOMLEFT      AF_POINT_BL
#define AF_PATTERN_BOTTOMRIGHT     AF_POINT_BR

#define AF_PATTERN_LEFT            AF_POINT_L
#define AF_PATTERN_LEFTTRIANGLE    AF_POINT_L | AF_POINT_TL | AF_POINT_BL
#define AF_PATTERN_LEFTDIAMOND     AF_POINT_L | AF_POINT_TL | AF_POINT_BL | AF_POINT_C
#define AF_PATTERN_LEFTHALF        AF_POINT_L | AF_POINT_TL | AF_POINT_BL | AF_POINT_C | AF_POINT_T | AF_POINT_B

#define AF_PATTERN_RIGHT           AF_POINT_R
#define AF_PATTERN_RIGHTTRIANGLE   AF_POINT_R | AF_POINT_TR | AF_POINT_BR
#define AF_PATTERN_RIGHTDIAMOND    AF_POINT_R | AF_POINT_TR | AF_POINT_BR | AF_POINT_C
#define AF_PATTERN_RIGHTHALF       AF_POINT_R | AF_POINT_TR | AF_POINT_BR | AF_POINT_C | AF_POINT_T | AF_POINT_B

#define AF_PATTERN_HLINE           AF_POINT_C | AF_POINT_L | AF_POINT_R
#define AF_PATTERN_VLINE           AF_POINT_C | AF_POINT_T | AF_POINT_B

#define AF_PATTERN_ALL             AF_POINT_C | AF_POINT_T | AF_POINT_B | AF_POINT_TL | AF_POINT_TR | AF_POINT_BL | AF_POINT_BR | AF_POINT_L | AF_POINT_R

typedef struct {
	int pattern;
	int next_center;
	int next_top;
	int next_bottom;
	int next_left;
	int next_right;
	int _eol_;
} type_PATTERN_MAP_ITEM;

typedef enum {
	DIRECTION_CENTER,
	DIRECTION_UP,
	DIRECTION_DOWN,
	DIRECTION_LEFT,
	DIRECTION_RIGHT
} type_DIRECTION;

extern void afp_enter ();

extern void afp_center ();
extern void afp_top ();
extern void afp_bottom ();
extern void afp_left ();
extern void afp_right ();

#endif /* AF_PATTERNS_H_ */
