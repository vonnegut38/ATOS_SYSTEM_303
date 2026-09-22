#include "wm_core.h"

int wm_rect_valid(const wm_rect_t *r){return r&&r->w&&r->h;}

int wm_rect_intersect(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out)
{
    int16_t x0,y0,x1,y1;
    if(!a||!b||!out)return 0;
    x0 = a->x > b->x ? a->x : b->x;
    y0 = a->y > b->y ? a->y : b->y;
    x1 = (int16_t)(a->x+a->w) < (int16_t)(b->x+b->w) ? (int16_t)(a->x+a->w) : (int16_t)(b->x+b->w);
    y1 = (int16_t)(a->y+a->h) < (int16_t)(b->y+b->h) ? (int16_t)(a->y+a->h) : (int16_t)(b->y+b->h);
    if(x1<=x0||y1<=y0)return 0;
    out->x=x0;out->y=y0;out->w=(uint16_t)(x1-x0);out->h=(uint16_t)(y1-y0);
    return 1;
}

int wm_rect_union(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out)
{
    int16_t x0,y0,x1,y1;wm_rect_t tmp;
    if(!a||!b||!out)return 0;
    x0 = a->x < b->x ? a->x : b->x;
    y0 = a->y < b->y ? a->y : b->y;
    x1 = (int16_t)(a->x+a->w) > (int16_t)(b->x+b->w) ? (int16_t)(a->x+a->w) : (int16_t)(b->x+b->w);
    y1 = (int16_t)(a->y+a->h) > (int16_t)(b->y+b->h) ? (int16_t)(a->y+a->h) : (int16_t)(b->y+b->h);
    out->x=x0;out->y=y0;out->w=(uint16_t)(x1-x0);out->h=(uint16_t)(y1-y0);
    return wm_rect_intersect(a,b,&tmp);
}

void wm_init(wm_state_t *s,int16_t w,int16_t h)
{
    unsigned i;
    if(!s)return;
    s->count=0;s->damage_count=0;s->screen_w=w;s->screen_h=h;
    for(i=0;i<WM_MAX;++i){s->clip_count[i]=0;s->win[i].flags=0;s->win[i].pid=0;}
}

/* a minus b -> up to four rectangles (classic strip decomposition). */
static unsigned wm_subtract(const wm_rect_t *a,const wm_rect_t *b,wm_rect_t *out,unsigned max)
{
    wm_rect_t i;unsigned n=0;
    int16_t ax1=(int16_t)(a->x+a->w),ay1=(int16_t)(a->y+a->h);
    if(!wm_rect_intersect(a,b,&i)){if(max){out[0]=*a;return 1;}return 0;}
    if(i.y>a->y&&n<max){out[n].x=a->x;out[n].y=a->y;out[n].w=a->w;out[n].h=(uint16_t)(i.y-a->y);++n;}
    if((int16_t)(i.y+i.h)<ay1&&n<max){out[n].x=a->x;out[n].y=(int16_t)(i.y+i.h);out[n].w=a->w;out[n].h=(uint16_t)(ay1-(i.y+i.h));++n;}
    if(i.x>a->x&&n<max){out[n].x=a->x;out[n].y=i.y;out[n].w=(uint16_t)(i.x-a->x);out[n].h=i.h;++n;}
    if((int16_t)(i.x+i.w)<ax1&&n<max){out[n].x=(int16_t)(i.x+i.w);out[n].y=i.y;out[n].w=(uint16_t)(ax1-(i.x+i.w));out[n].h=i.h;++n;}
    return n;
}

void wm_rebuild_clip(wm_state_t *s)
{
    unsigned i,j;
    wm_rect_t screen;
    if(!s)return;
    screen.x=0;screen.y=0;screen.w=(uint16_t)s->screen_w;screen.h=(uint16_t)s->screen_h;
    for(i=0;i<s->count;++i){
        wm_rect_t list[WM_CLIP_MAX];unsigned n=0;
        if(!(s->win[i].flags&1u)){s->clip_count[i]=0;continue;}
        if(!wm_rect_intersect(&s->win[i].client,&screen,&list[n])){s->clip_count[i]=0;continue;}
        n=1;
        for(j=i+1;j<s->count&&n;++j){
            wm_rect_t next[WM_CLIP_MAX];unsigned m=0,k;
            if(!(s->win[j].flags&1u))continue;
            for(k=0;k<n&&m<WM_CLIP_MAX;++k)m+=wm_subtract(&list[k],&s->win[j].frame,&next[m],WM_CLIP_MAX-m);
            for(k=0;k<m;++k)list[k]=next[k];
            n=m;
        }
        s->clip_count[i]=n;
        for(j=0;j<n;++j)s->clip[i][j]=list[j];
    }
}

static void window_copy(wm_window_t *dst,const wm_window_t *src)
{
    /* Field by field: the kernel is freestanding, so struct assignment would
       pull in memcpy(). */
    dst->frame.x=src->frame.x;dst->frame.y=src->frame.y;
    dst->frame.w=src->frame.w;dst->frame.h=src->frame.h;
    dst->client.x=src->client.x;dst->client.y=src->client.y;
    dst->client.w=src->client.w;dst->client.h=src->client.h;
    dst->pid=src->pid;dst->flags=src->flags;
    dst->src_x=src->src_x;dst->src_y=src->src_y;
}
void wm_raise(wm_state_t *s,unsigned i)
{
    wm_window_t tmp;
    unsigned j;
    if(!s||i>=s->count||i+1u==s->count)return;
    window_copy(&tmp,&s->win[i]);
    for(j=i;j+1u<s->count;++j)window_copy(&s->win[j],&s->win[j+1]);
    window_copy(&s->win[s->count-1u],&tmp);
    wm_damage_add(s,s->win[s->count-1u].frame);   /* repaint what moved up */
    wm_rebuild_clip(s);
}
void wm_drag_begin(wm_drag_t *g,const wm_state_t *s,int16_t x,int16_t y)
{
    unsigned i;
    if(!g)return;
    g->active=0;
    i=wm_hit(s,x,y);
    if(i==WM_NO_WINDOW)return;
    g->window=i;
    g->part=(uint8_t)wm_hit_part(s,i,x,y);
    g->grab_x=x;g->grab_y=y;
    g->x0=s->win[i].frame.x;g->y0=s->win[i].frame.y;
    g->w0=s->win[i].frame.w;g->h0=s->win[i].frame.h;
    g->active=1;
}
void wm_drag_update(wm_drag_t *g,wm_state_t *s,int16_t x,int16_t y)
{
    if(!g||!g->active||!s)return;
    if(g->part==WM_PART_TITLE){
        int32_t nx=(int32_t)g->x0+(x-g->grab_x);
        int32_t ny=(int32_t)g->y0+(y-g->grab_y);
        if(nx<0)nx=0;
        if(ny<0)ny=0;
        wm_move(s,g->window,(int16_t)nx,(int16_t)ny);
    }else{
        int32_t nw=(int32_t)g->w0+(x-g->grab_x);
        int32_t nh=(int32_t)g->h0+(y-g->grab_y);
        if(nw<32)nw=32;
        if(nh<24)nh=24;
        if(nw>(int32_t)s->screen_w)nw=(int32_t)s->screen_w;
        if(nh>(int32_t)s->screen_h)nh=(int32_t)s->screen_h;
        wm_resize(s,g->window,(uint16_t)nw,(uint16_t)nh);
    }
}
void wm_drag_end(wm_drag_t *g){if(g)g->active=0;}
unsigned wm_hit(const wm_state_t *s,int16_t x,int16_t y)
{
    int i;
    if(!s)return WM_NO_WINDOW;
    for(i=(int)s->count-1;i>=0;--i){        /* topmost window first */
        const wm_window_t *w=&s->win[i];
        if(!(w->flags&1u))continue;
        if(x<(int16_t)w->frame.x||y<(int16_t)w->frame.y)continue;
        if(x>=(int16_t)(w->frame.x+w->frame.w))continue;
        if(y>=(int16_t)(w->frame.y+w->frame.h))continue;
        return (unsigned)i;
    }
    return WM_NO_WINDOW;
}
int wm_hit_part(const wm_state_t *s,unsigned i,int16_t x,int16_t y)
{
    const wm_window_t *w;
    if(!s||i>=s->count)return 0;
    w=&s->win[i];
    if(x>=(int16_t)w->client.x&&y>=(int16_t)w->client.y&&
       x<(int16_t)(w->client.x+w->client.w)&&y<(int16_t)(w->client.y+w->client.h))
        return WM_PART_CLIENT;
    if(y<(int16_t)(w->frame.y+16))return WM_PART_TITLE;
    return WM_PART_FRAME;
}
void wm_resize(wm_state_t *s,unsigned i,uint16_t w,uint16_t h)
{
    wm_rect_t old;
    if(!s||i>=s->count||!(s->win[i].flags&1u))return;
    old=s->win[i].frame;
    s->win[i].frame.w=w;s->win[i].frame.h=h;
    {uint16_t cx=(uint16_t)(s->win[i].client.x-s->win[i].frame.x);
     uint16_t cy=(uint16_t)(s->win[i].client.y-s->win[i].frame.y);
     uint16_t cw=s->win[i].client.w,ch=s->win[i].client.h;
     if((uint32_t)cx+cw>w)cw=(w>cx)?(uint16_t)(w-cx):0u;
     if((uint32_t)cy+ch>h)ch=(h>cy)?(uint16_t)(h-cy):0u;
     s->win[i].client.w=cw;s->win[i].client.h=ch;}
    wm_damage_add(s,old);
    wm_damage_add(s,s->win[i].frame);
    wm_rebuild_clip(s);
}
void wm_move(wm_state_t *s,unsigned i,int16_t nx,int16_t ny)
{
    wm_rect_t old;
    if(!s||i>=s->count||!(s->win[i].flags&1u))return;
    old=s->win[i].frame;
    {uint16_t dx=(uint16_t)(s->win[i].client.x-s->win[i].frame.x);
     uint16_t dy=(uint16_t)(s->win[i].client.y-s->win[i].frame.y);
     s->win[i].frame.x=nx;s->win[i].frame.y=ny;
     s->win[i].client.x=(int16_t)(nx+(int16_t)dx);
     s->win[i].client.y=(int16_t)(ny+(int16_t)dy);}
    wm_damage_add(s,old);                 /* where it was */
    wm_damage_add(s,s->win[i].frame);     /* where it is now */
    wm_rebuild_clip(s);
}
void wm_damage_add(wm_state_t *s,wm_rect_t r)
{
    wm_rect_t screen;unsigned i;
    if(!s)return;
    screen.x=0;screen.y=0;screen.w=(uint16_t)s->screen_w;screen.h=(uint16_t)s->screen_h;
    if(!wm_rect_intersect(&r,&screen,&r))return;
    for(i=0;i<s->damage_count;++i){
        wm_rect_t u;
        if(wm_rect_union(&s->damage[i],&r,&u)){s->damage[i]=u;return;}
    }
    if(s->damage_count<WM_DAMAGE_MAX)s->damage[s->damage_count++]=r;
}

void wm_damage_clear(wm_state_t *s){if(s)s->damage_count=0;}

void wm_recompose(wm_state_t *s,wm_blit_fn blit,void *ctx)
{
    unsigned d,i,j;
    if(!s||!blit)return;
    for(d=0;d<s->damage_count;++d){
        wm_rect_t R=s->damage[d];
        blit(ctx,0u,&R,0,0);                      /* background first */
        for(i=0;i<s->count;++i){
            if(!(s->win[i].flags&1u))continue;
            for(j=0;j<s->clip_count[i];++j){
                wm_rect_t rc;
                if(!wm_rect_intersect(&R,&s->clip[i][j],&rc))continue;
                blit(ctx,s->win[i].pid,&rc,
                     (uint16_t)(s->win[i].src_x+rc.x-s->win[i].client.x),
                     (uint16_t)(s->win[i].src_y+rc.y-s->win[i].client.y));
            }
        }
    }
    wm_damage_clear(s);
}
