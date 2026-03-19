#include "libc64/math64.h"
#ifndef TARGET_PC
#include "MSL_C/w_math.h"
#endif

/* Under TARGET_PC these are replaced by macros in math64.h. */
#ifndef TARGET_PC
f32 fatan2(f32 x, f32 y) {
    return atan2(x, y);
}

f32 fsqrt(f32 x) {
    return sqrtf(x);
}

f32 facos(f32 x) {
    return acos(x);
}
#endif
