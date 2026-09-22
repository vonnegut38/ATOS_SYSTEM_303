#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "emutos_vdi_mono.h"
static uint16_t actual[1282],expect[1282];
static unsigned seed=123;
static unsigned rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static void pixel(uint16_t *p,unsigned stride,unsigned planes,unsigned x,unsigned y,unsigned c){
    unsigned q;uint16_t bit=0x8000u>>(x&15);
    for(q=0;q<planes;q++){uint16_t *v=p+y*stride+(x/16)*planes+q;
        *v=(uint16_t)((*v&~bit)|((c&(1u<<q))?bit:0));}
}
int main(void){
    static const uint16_t patterns[4]={0xaaaa,0x5555,0x8888,0x2222};
    unsigned planes,n,i,x,y,w,h,fg,bg,xx,yy,stride;
    for(planes=1;planes<=4;planes*=2)for(n=0;n<10000;n++){
        stride=5*planes;x=rnd()%80;y=rnd()%16;w=1+rnd()%(80-x);h=1+rnd()%(16-y);
        fg=rnd()%16;bg=rnd()%16;
        for(i=0;i<1282;i++)actual[i]=expect[i]=(uint16_t)rnd();
        emutos_vdi_planar_fill(actual+1,stride,planes,x,y,w,h,fg,bg,patterns,3);
        for(yy=y;yy<y+h;yy++)for(xx=x;xx<x+w;xx++)
            pixel(expect+1,stride,planes,xx,yy,(patterns[yy&3]&(0x8000u>>(xx&15)))?fg:bg);
        if(memcmp(actual,expect,sizeof actual)){printf("fill failed %u %u\n",planes,n);return 1;}
        emutos_vdi_vertical(actual+1,stride,planes,x,y,h,fg);
        for(yy=y;yy<y+h;yy++)pixel(expect+1,stride,planes,x,yy,fg);
        if(memcmp(actual,expect,sizeof actual)){puts("vertical failed");return 1;}
    }
    puts("EmuTOS planar: 30000 differential fills + 30000 vertical lines, guards OK");return 0;
}
