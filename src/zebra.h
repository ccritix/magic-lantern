// Temporary, until the one from Tobias will arrive
#include "dryos.h"
int liveview_display_idle();
OS_FUNCTION( 0x0B00001,	int,	get_global_draw);
OS_FUNCTION( 0x0B00002, void,	NotifyBox, int timeout, char* fmt, ...);
