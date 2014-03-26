#ifndef _ML_DNG_ML_SUPPORT_H_
#define _ML_DNG_ML_SUPPORT_H_

#include <dryos.h>
#include <property.h>
#include <module.h>
#include <math.h>
#include <string.h>
#define umalloc fio_malloc
#define ufree fio_free
#define pow powf

static int get_tick_count() { return get_ms_clock_value_fast(); }

#endif

