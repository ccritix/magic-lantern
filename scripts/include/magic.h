/**
 * Magic Lantern scripting API
 * 
 * Current state: proof of concept.
 * 
 * TODO: auto-generate from main ML headers?
 **/

#include "stdio.h"
#include "math.h"
#include "time.h"
#include "bmp.h"

/** Common properties **/
extern int lv;
extern int recording;

/** Menu interaction */
int menu_get(char* tab, char* entry);
int menu_set(char* tab, char* entry, int value);

/** Double-buffering */
void double_buffering_start();
void double_buffering_end();

/** Canon GUI mode */
int get_gui_mode();
void set_gui_mode(int mode);

/** Drawing */
void draw_line_polar(int x, int y, int radius, float angle, int color);


/** Floating-point hacks */
#include "flt-hack.h"
