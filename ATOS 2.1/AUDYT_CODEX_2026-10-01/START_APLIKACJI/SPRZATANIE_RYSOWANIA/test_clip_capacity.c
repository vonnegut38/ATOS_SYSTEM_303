/* Diagnostic: expected to FAIL until the separate clip-overflow issue is fixed. */
#include <stdio.h>
#include "wm_core.h"
static int in(wm_rect_t r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
int main(void){wm_state_t s;unsigned i,lost=0;
 wm_init(&s,640,400);s.count=8;
 s.win[0].frame=s.win[0].client=(wm_rect_t){0,0,640,400};
 for(i=1;i<4;i++)s.win[i].frame=s.win[i].client=(wm_rect_t){i*100,0,10,400};
 for(i=4;i<8;i++)s.win[i].frame=s.win[i].client=(wm_rect_t){0,(i-3)*80,640,10};
 for(i=0;i<8;i++)s.win[i].flags=1;
 wm_rebuild_clip(&s);
 for(int y=0;y<400;y++)for(int x=0;x<640;x++){
  int covered=0,found=0;
  for(i=1;i<8;i++)covered|=in(s.win[i].frame,x,y);
  for(i=0;i<s.clip_count[0];i++)found|=in(s.clip[0][i],x,y);
  if(!covered&&!found)lost++;
 }
 printf("visible pixels lost from clip list: %u, rectangles retained: %u\n",lost,s.clip_count[0]);
 return lost!=0;
}
