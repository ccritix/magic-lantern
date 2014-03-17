/**
 * Hack for different calling convention between TCC and core floating-point routines.
 * See flt-hack.h for a detailed description.
 * 
 * This must be included before magic.h.
 */

union floatint
{
   float f;
   int i;
};

#define WRAP(fun) \
    float fun##_wrap(float x) \
    { \
        union floatint a; \
        a.f = x; \
        extern int fun(int x); \
        a.i = fun(a.i); \
        return a.f; \
    } \

WRAP(sinf)
WRAP(cosf)
