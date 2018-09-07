#ifndef _log_h_
#define _log_h_

#include "dryos.h"

/* define start logging before Canon's init_task
 * caveat: no malloc working at this stage */
#undef LOG_EARLY_STARTUP

void log_start();
void log_finish();

#endif // _log_h_
