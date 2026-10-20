#include <stdint.h>
/* Separate translation unit: references to these become UNDEFINED externs, so
   the compiler emits 32-bit absolute (R_68K_32) relocations instead of 16-bit
   absolutes - the .PRG loader can only relocate 32-bit longwords. */
typedef struct { int16_t contrl[12]; int16_t intin[128]; int16_t ptsin[128]; int16_t intout[128]; int16_t ptsout[128]; } VDIPB;
typedef struct { int16_t contrl[12]; int16_t global[15]; int16_t intin[128]; int16_t intout[128]; int16_t addrin[128]; int16_t addrout[128]; } AESPB;
VDIPB vp; AESPB ap; int16_t handle;
