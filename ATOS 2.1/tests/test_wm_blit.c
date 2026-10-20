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

    /* Copy may cross either surface edge; untouched bytes are guards. */
    memset(fb8,0,sizeof fb8);memset(src8,0xAA,sizeof src8);
    {wm_rect_t edge={62,7,8,2};wm_copy_rect(&d,&s,&edge,63,7);
     CHECK(fb8[7*64+62]==0xAA && fb8[7*64+63]==0);}
    {wm_rect_t edge={-2,-1,4,3};
     memset(fb8,0,sizeof fb8);wm_fill_rect(&d,&edge,0x55);
     CHECK(fb8[0]==0x55 && fb8[1]==0x55 && fb8[2]==0);
     CHECK(fb8[64]==0x55 && fb8[128]==0);}

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

    /* Fast bit-aligned mono copy: every (sx,dx) phase against a per-pixel
       reference.  This is the compositor's window blit path. */
    {
        static uint8_t sbig[16*80],dbig[16*80],ref[16*80];
        wm_fb_t ds={dbig,80,640,16,1},ss={sbig,80,640,16,1};
        unsigned sx,dx,x,seed=12345u;
        for(x=0;x<sizeof sbig;x++){seed=seed*1664525u+1013904223u;sbig[x]=(uint8_t)(seed>>24);}
        for(sx=0;sx<40u;sx+=7u)for(dx=0;dx<40u;dx+=5u){
            wm_rect_t rc;rc.x=(int16_t)(60u+dx);rc.y=3;rc.w=200;rc.h=5;
            int row,col;
            memset(dbig,0x5A,sizeof dbig);memcpy(ref,dbig,sizeof dbig);
            wm_copy_rect(&ds,&ss,&rc,(uint16_t)sx,2u);
            for(row=0;row<5;++row)for(col=0;col<200;++col){
                unsigned syp=2u+(unsigned)row,sxp=sx+(unsigned)col;
                unsigned dyp=3u+(unsigned)row,dxp=(unsigned)rc.x+(unsigned)col;
                uint8_t bit=(uint8_t)((sbig[syp*80u+(sxp>>3)]>>(7u-(sxp&7u)))&1u);
                uint8_t *p=ref+dyp*80u+(dxp>>3);uint8_t m=(uint8_t)(0x80u>>(dxp&7u));
                if(bit)*p|=m;else *p&=(uint8_t)~m;
            }
            CHECK(memcmp(dbig,ref,sizeof dbig)==0);
        }
    }

    /* Clipped fill/copy for both formats against a pixel reference.
       Includes negative destinations, empty rectangles, all alignment phases,
       source overflow and all screen edges. */
    {
        uint8_t actual[8*64], expected[8*64], source[8*64];
        unsigned mode,iteration,seed=9187u,i;
        for(i=0;i<sizeof source;++i)source[i]=(uint8_t)(i*37u+11u);
        for(mode=0;mode<2;++mode){
            unsigned pitch=mode?64u:8u;
            wm_fb_t df={actual,(uint16_t)pitch,64,8,mode?8:1};
            wm_fb_t sf={source,(uint16_t)pitch,64,8,mode?8:1};
            for(iteration=0;iteration<2000;++iteration){
                wm_rect_t rc;unsigned sx,sy,op;int x,y;
                seed=seed*1664525u+1013904223u;
                rc.x=(int16_t)((int)(seed%90u)-15);
                rc.y=(int16_t)((int)((seed>>8)%16u)-4);
                rc.w=(uint16_t)((seed>>16)%80u);rc.h=(uint16_t)((seed>>24)%12u);
                sx=(seed>>4)%72u;sy=(seed>>12)%12u;op=iteration&1u;
                memset(actual,0x5A,sizeof actual);memcpy(expected,actual,sizeof actual);
                for(y=0;y<8;++y)for(x=0;x<64;++x){
                    int xx=(int)sx+x-rc.x,yy=(int)sy+y-rc.y;unsigned value;
                    if(x<rc.x||x>=rc.x+(int)rc.w||y<rc.y||y>=rc.y+(int)rc.h)continue;
                    if(op&&(xx<0||xx>=64||yy<0||yy>=8))continue;
                    value=op?(mode?source[yy*pitch+xx]:((source[yy*pitch+(xx>>3)]>>(7-(xx&7)))&1u)):1u;
                    if(mode)expected[y*pitch+x]=(uint8_t)value;
                    else{uint8_t mask=(uint8_t)(0x80u>>(x&7));
                         if(value)expected[y*pitch+(x>>3)]|=mask;
                         else expected[y*pitch+(x>>3)]&=(uint8_t)~mask;}
                }
                if(op)wm_copy_rect(&df,&sf,&rc,(uint16_t)sx,(uint16_t)sy);
                else wm_fill_rect(&df,&rc,1);
                CHECK(memcmp(actual,expected,sizeof actual)==0);
            }
        }
    }

    printf(fails?"WM_BLIT: %d FAIL\n":"WM_BLIT: ALL PASS\n",fails);
    return fails?1:0;
}
