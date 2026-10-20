#ifndef TOS_DAMAGE_H
#define TOS_DAMAGE_H
#include <stdint.h>

#define TOS_DAMAGE_ROWS 512
typedef struct {
    uint16_t left[TOS_DAMAGE_ROWS],right[TOS_DAMAGE_ROWS];
    uint16_t width,height;
    /* Lowest row that may still be dirty.  tos_damage_take used to search from
       row 0 on every call, so taking N rectangles walked the same rows N times;
       the whole-screen scan (512 rows x 80 bytes) runs often enough for that to
       matter.  Notes lower this, takes raise it. */
    uint16_t first;
} tos_damage_t;
typedef struct { int x,y,w,h; } tos_damage_rect_t;

static inline void tos_damage_reset(tos_damage_t *d,unsigned w,unsigned h)
{
    unsigned y;
    d->width=(uint16_t)w;
    d->height=(uint16_t)(h>TOS_DAMAGE_ROWS?TOS_DAMAGE_ROWS:h);
    d->first=0;
    for(y=0;y<d->height;y++){d->left[y]=d->width;d->right[y]=0;}
}

/* Half-open rectangles. A row retains its entire pending span until taken. */
static inline void tos_damage_note(tos_damage_t *d,int x0,int y0,int x1,int y1)
{
    int y;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>d->width)x1=d->width;
    if(y1>d->height)y1=d->height;
    if(x0>=x1||y0>=y1)return;
    if((unsigned)y0<d->first)d->first=(uint16_t)y0;
    for(y=y0;y<y1;y++){
        if(x0<d->left[y])d->left[y]=(uint16_t)x0;
        if(x1>d->right[y])d->right[y]=(uint16_t)x1;
    }
}

static inline int tos_damage_take(tos_damage_t *d,tos_damage_rect_t *r)
{
    unsigned y,end,x0,x1,k;
    for(y=d->first;y<d->height;y++)if(d->left[y]<d->right[y])break;
    if(y==d->height){d->first=d->height;return 0;}
    x0=d->left[y];x1=d->right[y];end=y+1;
    while(end<d->height&&d->left[end]==x0&&d->right[end]==x1)++end;
    r->x=(int)x0;r->y=(int)y;r->w=(int)(x1-x0);r->h=(int)(end-y);
    for(k=y;k<end;k++){d->left[k]=d->width;d->right[k]=0;}
    d->first=(uint16_t)end;          /* single pass: never look above this again */
    return 1;
}

/* Exact comparison, no checksums and no rotating band acknowledgements.
   Mono tracks horizontal spans; interleaved colour marks the whole row.
   Scan scheduling is independent of writes reported by VDI/VT52/Line-A. */
static inline void tos_damage_scan(tos_damage_t *d,const uint8_t *screen,
                                  uint8_t *previous,unsigned stride,int mono)
{
    unsigned y,x;
    for(y=0;y<d->height;y++){
        const uint8_t *s=screen+y*stride;
        uint8_t *p=previous+y*stride;
        unsigned lo=stride,hi=0;
        if((((uintptr_t)s|(uintptr_t)p|stride)&1u)==0u){
            const uint16_t *sw=(const uint16_t *)(const void *)s;
            uint16_t *pw=(uint16_t *)(void *)p;
            for(x=0;x<stride/2;x++)if(sw[x]!=pw[x]){
                pw[x]=sw[x];if(lo==stride)lo=x*2;hi=x*2+2;
            }
        }else{
            for(x=0;x<stride;x++)if(s[x]!=p[x]){
                p[x]=s[x];if(lo==stride)lo=x;hi=x+1;
            }
        }
        if(lo<hi)tos_damage_note(d,mono?(int)lo*8:0,(int)y,
                                mono?(int)hi*8:d->width,(int)y+1);
    }
}

static inline int tos_present_due(uint32_t now,uint32_t last,uint32_t period)
{return (uint32_t)(now-last)>=period;}
#endif
