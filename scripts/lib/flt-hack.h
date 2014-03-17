/**
 * Hack for different calling convention between TCC and core floating-point routines.
 * 
 * Problem: TCC can only generate FPA and VFP float instructions, neither of which we have.
 * FPA calls can be trapped by g3gg0's fpu_emu.
 * 
 * However, calling a floating-point function from core requires passing the arguments
 * in the soft-float (AEABI) convention (that is, in integer registers).
 * 
 * TCC doesn't know about that, it puts arguments and expects result in the floating-point registers.
 * 
 * Workaround: fake the declaration of the floating-point functions to integer, e.g. int sinf(int x), 
 * and cast the arguments without binary conversion.
 * 
 * This works for single-precision floats, where sizeof(float) == sizeof(int) == 4.
 */

#ifdef ML_SCRIPT
/* force integer calling convention for float routines (for ML only) */

float sinf_wrap(float x);
float cosf_wrap(float x);
#define sinf(x) sinf_wrap(x)
#define cosf(x) cosf_wrap(x)

#endif

/* redirect double functions to single-precison ones (also for desktop) */
#define sin(x)  sinf(x)
#define cos(x)  cosf(x)
