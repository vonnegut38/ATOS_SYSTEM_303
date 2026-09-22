#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gui_damage.h"
static uint16_t src[64002],dst[64002];
int main(void){gui_draw_t c[2];int l,t,r,b;unsigned plane,x,y,i,first,words,stride;
 memset(c,0,sizeof(c));c[0].kind=GUI_FILL;c[0].x=8;c[0].y=26;c[0].w=304;c[0].h=294;
 assert(gui_damage(c,1,640,400,&l,&t,&r,&b)&&l==8&&t==26&&r==312&&b==320);
 for(plane=1;plane<=4;plane*=2){stride=40*plane;
 for(x=0;x<640;x+=13)for(y=0;y<400;y+=17){
  c[0].x=x-10;c[0].y=y-8;c[0].w=19;c[0].h=13;assert(gui_damage(c,1,640,400,&l,&t,&r,&b));
  first=(unsigned)(l/16)*plane;words=(unsigned)((r+15)/16-l/16)*plane;
  for(i=0;i<64002;i++){src[i]=(uint16_t)(i*17);dst[i]=0xa55a;}
  gui_damage_copy(dst+1,src+1,stride,first,words,t,b);
  for(i=0;i<stride*400;i++){unsigned yy=i/stride,xx=i%stride;assert(dst[1+i]==((yy>=(unsigned)t&&yy<(unsigned)b&&xx>=first&&xx<first+words)?src[1+i]:0xa55a));}
  assert(dst[0]==0xa55a&&dst[stride*400+1]==0xa55a);
 }}
 memset(c,0,sizeof(c));c[0].kind=GUI_TEXT;c[0].x=631;c[0].y=391;strcpy(c[0].text,"a\nbc");assert(gui_damage(c,1,640,400,&l,&t,&r,&b)&&l==631&&t==391&&r==640&&b==400);
 c[0].x=-32768;c[0].y=-32768;assert(!gui_damage(c,1,640,400,&l,&t,&r,&b));
 c[0].kind=GUI_WINDOW;c[0].w=64;c[0].h=40;assert(gui_damage(c,1,640,400,&l,&t,&r,&b)&&l==0&&r==640&&b==400);
 puts("Damage PASS: clipping, text newlines, conservative windows, word copy for 1/2/4 planes and edge guards");return 0;}
