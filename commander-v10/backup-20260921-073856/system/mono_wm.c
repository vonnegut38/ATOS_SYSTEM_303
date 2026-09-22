#include "mono_wm.h"
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
void wm_init(wm_state *s){unsigned i;uint8_t *p=(uint8_t *)s;for(i=0;i<sizeof(*s);i++)p[i]=0;
 s->win[0].rect=(wm_rect){96,52,280,180};s->win[1].rect=(wm_rect){224,140,320,200};s->full=-1;}
wm_rect wm_bounds(const wm_state *s,int i){if(s->full==i)return (wm_rect){0,0,640,400};return s->win[i].rect;}
int wm_select(wm_state *s,int i){if(i<0||i>1||!s->win[i].open)return 0;s->active=i;if(s->full>=0)s->full=i;s->gesture=0;return 1;}
int wm_cycle(wm_state *s){return wm_select(s,1-s->active);}
void wm_close(wm_state *s,int i){if(i<0||i>1)return;s->win[i].open=0;s->gesture=0;
 if(s->full==i)s->full=-1;
 if(s->active==i&&s->win[1-i].open)s->active=1-i;}
static int contains(wm_rect r,int x,int y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
int wm_hit(const wm_state *s,int x,int y){int a=s->active,b=1-a;
 if(s->win[a].open&&contains(wm_bounds(s,a),x,y))return a;
 if(s->full<0&&s->win[b].open&&contains(wm_bounds(s,b),x,y))return b;
 return -1;}
void wm_motion(wm_state *s,int x,int y){wm_rect *r=&s->win[s->active].rect;if(s->full>=0)return;
 if(s->gesture==1){r->x=clamp(x-s->dx,0,640-r->w);r->y=clamp(y-s->dy,20,376-r->h);}
 if(s->gesture==2){r->w=clamp(x-r->x+s->dx,240,640-r->x);r->h=clamp(y-r->y+s->dy,160,376-r->y);}}
