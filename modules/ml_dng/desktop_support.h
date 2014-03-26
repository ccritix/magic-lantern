#ifndef _ML_DNG_DESKTOP_SUPPORT_H_
#define _ML_DNG_DESKTOP_SUPPORT_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#define FAST
#define UNCACHEABLE(x) (x)
#define umalloc malloc
#define ufree free

#define FIO_CreateFile(name) fopen(name, "wb")
#define FIO_WriteFile(f, ptr, count) fwrite(ptr, 1, count, f)
#define FIO_CloseFile(f) fclose(f)

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define COERCE(x,lo,hi) MAX(MIN((x),(hi)),(lo))

#endif
