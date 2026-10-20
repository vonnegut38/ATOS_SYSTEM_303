#include <stdio.h>
#include "wm_core.h"
static int contains(wm_rect_t r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
int main(void){
 wm_state_t s;unsigned i,j,lost=0;int fail=0;
 wm_init(&s,800,600);
 for(i=0;i<80;i++)wm_damage_add(&s,(wm_rect_t){(int16_t)(5+(i%16)*45),(int16_t)(5+(i/16)*45),2,2});
 for(i=0;i<80;i++){
  int x=5+(i%16)*45,y=5+(i/16)*45,hit=0;
  for(j=0;j<s.damage_count;j++)if(contains(s.damage[j],x,y))hit=1;
  if(!hit)lost++;
 }
 printf("lost invalidations: %u/80\n",lost);fail|=lost!=0;
 wm_init(&s,800,600);s.count=2;
 for(i=0;i<2;i++){
  s.win[i].flags=1;s.win[i].pid=i+1;s.win[i].content_h=300+i*700;
  s.win[i].frame=s.win[i].client=(wm_rect_t){(int16_t)(i*100),30,80,80};
 }
 wm_raise(&s,0);
 printf("content heights after raise: %u, %u (expected 1000, 300)\n",s.win[0].content_h,s.win[1].content_h);
 fail|=s.win[0].content_h!=1000||s.win[1].content_h!=300;
 return fail;
}
