#include "damage.h"

static damage_rect_t rects[DAMAGE_MAX];
static uint8_t count;

void damage_reset(void){count=0u;}

static int overlaps(const damage_rect_t *a,const damage_rect_t *b)
{
    return !(a->x+a->w<b->x || b->x+b->w<a->x || a->y+a->h<b->y || b->y+b->h<a->y);
}

static void merge_into(damage_rect_t *a,const damage_rect_t *b)
{
    uint16_t ax2=(uint16_t)(a->x+a->w), ay2=(uint16_t)(a->y+a->h);
    uint16_t bx2=(uint16_t)(b->x+b->w), by2=(uint16_t)(b->y+b->h);
    if(b->x<a->x)a->x=b->x;
    if(b->y<a->y)a->y=b->y;
    if(bx2>ax2)ax2=bx2;
    if(by2>ay2)ay2=by2;
    a->w=(uint16_t)(ax2-a->x);a->h=(uint16_t)(ay2-a->y);
}

void damage_add(int x,int y,int w,int h)
{
    damage_rect_t r;
    uint8_t i;
    if(w<=0||h<=0)return;
    r.x=(uint16_t)x;r.y=(uint16_t)y;r.w=(uint16_t)w;r.h=(uint16_t)h;
    for(i=0u;i<count;++i){
        if(overlaps(&rects[i],&r)){merge_into(&rects[i],&r);return;}
    }
    if(count<DAMAGE_MAX){rects[count++]=r;return;}
    rects[0].x=0u;rects[0].y=0u;rects[0].w=640u;rects[0].h=480u;count=1u;
}

uint8_t damage_count(void){return count;}

uint32_t damage_pixels(void)
{
    uint32_t p=0u;uint8_t i;
    for(i=0u;i<count;++i)p+=(uint32_t)rects[i].w*rects[i].h;
    return p;
}

const damage_rect_t *damage_rect(uint8_t index){return index<count?&rects[index]:0;}
