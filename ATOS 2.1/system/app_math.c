/* app_math.c - position-independent integer helpers for runtime-loaded apps.
 *
 * Why this file exists
 * --------------------
 * ATOS applications are linked at address 0 (linker/app-sys.ld) but the loader
 * places them at an arbitrary heap address; -mpcrel makes the app's own code
 * and data references PC-relative, so the app image is position independent.
 *
 * The stock libgcc that -lgcc pulls in is NOT built with -mpcrel, so its
 * division helpers call each other with absolute long jumps:
 *
 *     __divsi3:   jsr 0x00000DD0   (should be base+0xDD0)
 *     __umodsi3:  jsr 0x00000DD0 / jsr 0x00000EBC
 *     __modsi3:   jsr 0x00000E2C / jsr 0x00000EBC
 *
 * As soon as an app loaded at, say, 0x76030 does 'q / 10', the call lands in
 * low RAM and the process dies with an address/bus error (DESKTOP.APP did
 * exactly that on the first 'ROW nn' repaint).  These replacement helpers are
 * compiled with the app flags, so every call between them is PC-relative.
 *
 * They deliberately avoid '/', '%' and 64-bit arithmetic so no libgcc helper
 * is pulled in to build them.
 */

typedef unsigned int u32;
typedef int s32;

/* Restoring division: first scale `den` up by the largest power of two that
   still fits num, then walk the bits back down subtracting.  Scaling den (and
   the matching quotient bit) together is what makes this correct - an earlier
   version shifted only den, which returned wrong quotients AND remainders
   (visible as garbled digits in the desktop's file list). */
u32 __udivsi3(u32 num, u32 den)
{
    u32 quot = 0u, bit = 1u;
    if(!den)return 0u;
    while(!(den & 0x80000000u) && (den << 1) <= num){den <<= 1;bit <<= 1;}
    while(bit){
        if(num >= den){num -= den;quot |= bit;}
        den >>= 1;bit >>= 1;
    }
    return quot;
}

u32 __umodsi3(u32 num, u32 den)
{
    u32 bit = 1u;
    if(!den)return num;
    while(!(den & 0x80000000u) && (den << 1) <= num){den <<= 1;bit <<= 1;}
    while(bit){
        if(num >= den)num -= den;
        den >>= 1;bit >>= 1;
    }
    return num;
}

u32 __mulsi3(u32 a, u32 b)
{
    u32 result = 0u;
    while(b){
        if(b & 1u)result += a;
        a += a;
        b >>= 1;
    }
    return result;
}

s32 __divsi3(s32 num, s32 den)
{
    u32 ua, ub, quot;
    int neg = 0;
    if(!den)return 0;
    ua = (u32)num;ub = (u32)den;
    if(num < 0){ua = (u32)(-num);neg = !neg;}
    if(den < 0){ub = (u32)(-den);neg = !neg;}
    quot = __udivsi3(ua, ub);
    return neg ? -(s32)quot : (s32)quot;
}

s32 __modsi3(s32 num, s32 den)
{
    u32 ua, ub, rem;
    int neg = 0;
    if(!den)return 0;
    ua = (u32)num;ub = (u32)den;
    if(num < 0){ua = (u32)(-num);neg = 1;}
    if(den < 0)ub = (u32)(-den);
    rem = __umodsi3(ua, ub);
    return neg ? -(s32)rem : (s32)rem;
}
