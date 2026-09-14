#include "mono_wm.h"

static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}

static void z_remove(wm_state *s,int i)
{
    int8_t k,j;
    for(k=0;k<s->count;++k)if(s->order[k]==i){
        for(j=k;j+1<s->count;++j)s->order[j]=s->order[j+1];
        --s->count;return;
    }
}

static void z_push_top(wm_state *s,int i)
{
    z_remove(s,i);
    if(s->count<WM_MAX)s->order[s->count++]=(int8_t)i;
}

static void place(wm_state *s,int i)
{
    /* Kaskada: kazde kolejne okno przesuniete i nieco mniejsze. */
    int step=i%4;
    s->win[i].rect=(wm_rect){(int16_t)(64+step*45),(int16_t)(69+step*46),
                             (int16_t)(512-step*26),(int16_t)(384-step*19)};
}

void wm_init(wm_state *s)
{
    unsigned i;uint8_t *p=(uint8_t *)s;
    for(i=0;i<sizeof(*s);i++)p[i]=0;
    for(i=0;i<WM_MAX;++i){place(s,(int)i);s->order[i]=-1;}
    s->count=0;s->active=-1;s->full=-1;
}

int wm_is_open(const wm_state *s,int i)
{return i>=0&&i<WM_MAX&&s->win[i].open;}

wm_rect wm_bounds(const wm_state *s,int i)
{
    if(s->full==i)return (wm_rect){0,0,1024,768};
    return s->win[i].rect;
}

int wm_open(wm_state *s,uint32_t pid,const char *title)
{
    int i;
    unsigned n=0;
    for(i=0;i<WM_MAX;++i)if(!s->win[i].open)break;
    if(i>=WM_MAX)return -1;
    place(s,i);
    for(n=0;n<sizeof(s->win[i].title)-1u&&title&&title[n];++n)s->win[i].title[n]=title[n];
    s->win[i].title[n]='\0';
    s->win[i].pid=pid;s->win[i].open=1;s->win[i].kind=0;
    s->win[i].count=0;s->win[i].paused=0;s->win[i].parts=0;
    s->win[i].text[0]='\0';s->win[i].staging[0]='\0';
    z_push_top(s,i);
    s->active=(int8_t)i;
    if(s->full>=0)s->full=(int8_t)i;
    s->gesture=0;
    return i;
}

int wm_select(wm_state *s,int i)
{
    if(!wm_is_open(s,i))return 0;
    s->active=(int8_t)i;
    z_push_top(s,i);
    if(s->full>=0)s->full=(int8_t)i;
    s->gesture=0;
    return 1;
}

int wm_top(const wm_state *s)
{
    int8_t k;
    for(k=s->count-1;k>=0;--k)if(s->win[s->order[k]].open)return s->order[k];
    return -1;
}

int wm_next_open(const wm_state *s,int from)
{
    int k;
    for(k=1;k<=WM_MAX;++k){
        int i=(from+k)%WM_MAX;
        if(i>=0&&s->win[i].open)return i;
    }
    return from;
}

int wm_cycle(wm_state *s)
{
    int next=wm_next_open(s,s->active<0?0:s->active);
    return wm_select(s,next);
}

void wm_close(wm_state *s,int i)
{
    if(!wm_is_open(s,i))return;
    s->win[i].open=0;
    z_remove(s,i);
    s->gesture=0;
    if(s->full==i)s->full=-1;
    if(s->active==i)s->active=(int8_t)wm_top(s);
}

static int contains(wm_rect r,int x,int y)
{return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}

int wm_hit(const wm_state *s,int x,int y)
{
    int8_t k;
    if(s->full>=0){
        int f=s->full;
        return (s->win[f].open&&contains(wm_bounds(s,f),x,y))?f:-1;
    }
    for(k=s->count-1;k>=0;--k){
        int i=s->order[k];
        if(s->win[i].open&&contains(wm_bounds(s,i),x,y))return i;
    }
    return -1;
}

void wm_motion(wm_state *s,int x,int y)
{
    wm_rect *r;
    if(s->active<0||s->full>=0)return;
    r=&s->win[s->active].rect;
    if(s->gesture==1){
        r->x=(int16_t)clamp(x-s->dx,0,1024-r->w);
        r->y=(int16_t)clamp(y-s->dy,20,744-r->h);
    }
    if(s->gesture==2){
        r->w=(int16_t)clamp(x-r->x+s->dx,240,1024-r->x);
        r->h=(int16_t)clamp(y-r->y+s->dy,160,744-r->y);
    }
}
