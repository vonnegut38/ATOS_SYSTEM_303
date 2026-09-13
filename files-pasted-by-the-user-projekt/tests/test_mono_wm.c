#include "mono_wm.h"
#include <assert.h>
#include <stdio.h>

static void center(const wm_state *s,int i,int *x,int *y)
{wm_rect r=wm_bounds(s,i);*x=r.x+r.w/2;*y=r.y+r.h/2;}

int main(void)
{
    wm_state s;int i,x,y;
    wm_init(&s);
    assert(s.count==0&&s.active==-1&&s.full==-1);

    for(i=0;i<4;i++)assert(wm_open(&s,(uint32_t)(100+i),"WIN")==i);
    assert(s.count==4&&s.active==3);
    center(&s,3,&x,&y);assert(wm_hit(&s,x,y)==3);

    assert(wm_select(&s,0));assert(s.active==0);assert(wm_top(&s)==0);
    center(&s,0,&x,&y);assert(wm_hit(&s,x,y)==0);
    assert(wm_cycle(&s));assert(s.active==1);
    assert(wm_next_open(&s,0)==1);

    s.full=2;assert(wm_bounds(&s,2).w==640&&wm_bounds(&s,2).h==400);
    assert(wm_hit(&s,320,200)==2);
    s.full=-1;
    assert(wm_bounds(&s,2).w<640);

    wm_close(&s,1);assert(!s.win[1].open);
    assert(s.active>=0&&wm_is_open(&s,s.active));

    wm_select(&s,3);
    for(i=0;i<10000;i++){
        int px=(int)(i*127u%2000)-700,py=(int)(i*79u%1400)-500;
        s.gesture=1;s.dx=20;s.dy=5;wm_motion(&s,px,py);
        {wm_rect r=s.win[s.active].rect;assert(r.x>=0&&r.y>=20&&r.x+r.w<=640&&r.y+r.h<=376);}
        s.gesture=2;s.dx=6;s.dy=6;wm_motion(&s,px,py);
        {wm_rect r=s.win[s.active].rect;assert(r.w>=240&&r.h>=160&&r.x+r.w<=640&&r.y+r.h<=376);}
    }

    {int n=0;while(wm_open(&s,1,"X")>=0){++n;if(n>WM_MAX+1)break;}assert(n==WM_MAX-3);}
    {int t=wm_top(&s);assert(t>=0);wm_close(&s,t);assert(s.active==wm_top(&s));}
    puts("WM N-okien: Z-order, fokus, cykl, pelny ekran, zamykanie, pojemnosc, 10000 drag/resize PASS");
    return 0;
}
