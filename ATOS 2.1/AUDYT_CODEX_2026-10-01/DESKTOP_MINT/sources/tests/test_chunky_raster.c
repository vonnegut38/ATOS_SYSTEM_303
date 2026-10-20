#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chunky_raster.h"
static uint8_t a[4096],b[4096],tmp[4096];
int main(void){
 unsigned off,w,h,pitch,r,x,fg,bg,opaque;uint8_t glyph[16];int sx,sy,dx,dy;
 for(pitch=32;pitch<=35;++pitch)for(off=0;off<8;++off)for(w=0;w<=24;++w){
  memset(a,0xa5,sizeof a);memcpy(b,a,sizeof a);
  chunky_fill(a+16+off,pitch,w,17,0x63);
  for(r=0;r<17;++r)for(x=0;x<w;++x)b[16+off+r*pitch+x]=0x63;
  assert(!memcmp(a,b,sizeof a));
 }
 for(pitch=32;pitch<=35;++pitch)for(off=0;off<4;++off)for(fg=0;fg<3;++fg)for(bg=0;bg<3;++bg)for(opaque=0;opaque<2;++opaque){
  for(w=0;w<256;w+=16){
   for(r=0;r<16;++r)glyph[r]=(uint8_t)(w+r);
   memset(a,0xa5,sizeof a);memcpy(b,a,sizeof a);
   chunky_glyph(a+16+off,pitch,glyph,(uint8_t)(fg*127),(uint8_t)(bg*127),opaque);
   for(r=0;r<16;++r)for(x=0;x<8;++x){if(glyph[r]&(128u>>x))b[16+off+r*pitch+x]=(uint8_t)(fg*127);else if(opaque)b[16+off+r*pitch+x]=(uint8_t)(bg*127);}
   assert(!memcmp(a,b,sizeof a));
  }
 }
 for(sx=0;sx<8;++sx)for(sy=0;sy<5;++sy)for(dx=0;dx<8;++dx)for(dy=0;dy<5;++dy)for(w=1;w<17;w+=3)for(h=1;h<9;h+=3){
  for(x=0;x<sizeof a;++x)a[x]=(uint8_t)(x*37+x/32);
  memcpy(b,a,sizeof a);memcpy(tmp,a,sizeof a);
  chunky_copy(a+16,32,sx,sy,w,h,dx,dy);
  for(r=0;r<h;++r)for(x=0;x<w;++x)b[16+(dy+r)*32+dx+x]=tmp[16+(sy+r)*32+sx+x];
  assert(!memcmp(a,b,sizeof a));
 }
 puts("PASS: chunky fills, every glyph mask, palette changes, alignment, transparent text, overlapping copies and guard bytes");
}
