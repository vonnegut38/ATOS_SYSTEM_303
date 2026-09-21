#include <stdio.h>
#include <string.h>
#include "wm_blit.h"

static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL %s:%d %s\n",__FILE__,__LINE__,#c);fails++;}}while(0)
static uint8_t fb8[64*8], src8[64*8], fbm[64*8], srcm[64*8];

int main(void)
{
    wm_fb_t d={fb8,64,64,8,8},s={src8,64,64,8,8};
    wm_fb_t dm={fbm,8,64,8,1},sm={srcm,8,64,8,1};
    wm_rect_t r={10,2,8,2};

    /* chunky copy with source offset */
    memset(fb8,0,sizeof fb8);memset(src8,0xAA,sizeof src8);
    src8[3*64+5]=0x11;src8[3*64+6]=0x22;src8[4*64+5]=0x33;src8[4*64+6]=0x44;
    wm_copy_rect(&d,&s,&r,5,3);
    CHECK(fb8[2*64+10]==0x11&&fb8[2*64+11]==0x22&&fb8[3*64+10]==0x33&&fb8[3*64+11]==0x44);
    CHECK(fb8[2*64+9]==0x00&&fb8[4*64+10]==0x00);

    /* chunky fill */
    wm_fill_rect(&d,&r,0x55);
    CHECK(fb8[2*64+10]==0x55&&fb8[3*64+17]==0x55&&fb8[2*64+9]==0);

    /* planar mono copy: bit 0 of source row 1 -> dst (10,2) */
    memset(fbm,0,sizeof fbm);memset(srcm,0,sizeof srcm);
    srcm[1*8+0]=0x80;                    /* leftmost pixel of row 1 */
    srcm[2*8+0]=0x40;                    /* second pixel of row 2 */
    wm_copy_rect(&dm,&sm,&r,0,1);
    CHECK((fbm[2*8+(10>>3)]>>(7-(10&7)))&1);      /* first source pixel at dst x=10 */
    CHECK((fbm[3*8+(11>>3)]>>(7-(11&7)))&1);      /* second source row -> dst row 3, x=11 */
    CHECK(!((fbm[2*8+(12>>3)]>>(7-(12&7)))&1));

    /* planar fill + clear */
    wm_fill_rect(&dm,&r,1);CHECK((fbm[2*8+(10>>3)]>>(7-(10&7)))&1);
    wm_fill_rect(&dm,&r,0);CHECK(!((fbm[2*8+(10>>3)]>>(7-(10&7)))&1));

    printf(fails?"WM_BLIT: %d FAIL\n":"WM_BLIT: ALL PASS\n",fails);
    return fails?1:0;
}
