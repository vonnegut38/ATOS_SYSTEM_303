#include "mono_wm.h"
#include <assert.h>
#include <stdio.h>
int main(void){wm_state s;wm_rect before,r;unsigned i;
 wm_init(&s);s.win[0].open=s.win[1].open=1;
 assert(wm_hit(&s,250,160)==0);assert(wm_cycle(&s));assert(wm_hit(&s,250,160)==1);
 before=s.win[1].rect;s.full=1;assert(wm_cycle(&s));assert(s.full==0);
 r=wm_bounds(&s,0);assert(r.x==0&&r.y==0&&r.w==640&&r.h==400);
 s.full=-1;assert(s.win[1].rect.x==before.x&&s.win[1].rect.w==before.w);
 for(i=0;i<10000;i++){
  int x=(int)(i*127u%2000)-700,y=(int)(i*79u%1400)-500;
  s.gesture=1;s.dx=20;s.dy=5;wm_motion(&s,x,y);r=s.win[s.active].rect;
  assert(r.x>=0&&r.y>=20&&r.x+r.w<=640&&r.y+r.h<=376);
  s.gesture=2;s.dx=6;s.dy=6;wm_motion(&s,x,y);r=s.win[s.active].rect;
  assert(r.w>=240&&r.h>=160&&r.x+r.w<=640&&r.y+r.h<=376);
 }
 wm_close(&s,0);assert(s.active==1&&!s.win[0].open);assert(!wm_select(&s,0));
 wm_close(&s,1);assert(wm_hit(&s,250,160)==-1);
 puts("Mono WM: overlap, focus, fullscreen restore, 10000 drag/resize bounds, close PASS");return 0;}
