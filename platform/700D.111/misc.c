// misc functions specific to 700D

#include <dryos.h>
#include <property.h>
#include <bmp.h>
#include <config.h>
#include <consts.h>
#include <lens.h>

int new_LiveViewApp_handler = 0xff123456;



// dummy stubs
int lcd_release_running = 0;
void lcd_release_step() {};
int get_lcd_sensor_shortcuts() { return 0; }
void display_lcd_remote_icon(int x0, int y0) {}
void WAV_StartRecord(void) {}
void WAV_StopRecord(void) {}
