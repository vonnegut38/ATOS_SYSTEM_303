#include "wm_blit.h"

static int inside(const wm_fb_t *fb,int x,int y)
{return fb&&x>=0&&y>=0&&x<(int)fb->w&&y<(int)fb->h;}

void wm_fill_rect(wm_fb_t *fb,const wm_rect_t *r,uint8_t level)
{
    int y,x0,y0,x1,y1;
    if(!fb||!fb->data||!r)return;
    x0=r->x;y0=r->y;x1=r->x+r->w;y1=r->y+r->h;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)fb->w)x1=fb->w;
    if(y1>(int)fb->h)y1=fb->h;
    if(x1<=x0||y1<=y0)return;
    for(y=y0;y<y1;++y){
        uint8_t *row=fb->data+(uint32_t)y*fb->pitch;
        if(fb->bpp==8){
            uint32_t n=(uint32_t)(x1-x0);uint8_t *p=row+x0;
            uint32_t v=(uint32_t)level*0x01010101u;
            /* The 68000/030 raises an address error on unaligned long access,
               so align the row before using 32-bit stores. */
            while(n&&((uintptr_t)p&3u)){*p++=level;--n;}
            while(n>=4u){*(uint32_t *)(void *)p=v;p+=4;n-=4;}
            while(n--)*p++=level;
        }else{
            /* Byte-wise span fill: the bit loop was 8x more work per pixel. */
            int b0,b1;
            if(x0<0)x0=0;
            if(x1>(int)fb->w)x1=(int)fb->w;
            if(x1<=x0)continue;
            b0=x0>>3;b1=(x1-1)>>3;
            if(b0==b1){
                uint8_t m=(uint8_t)((0xFFu>>(x0&7))&(0xFFu<<(7-((x1-1)&7))));
                if(level)row[b0]|=m;else row[b0]&=(uint8_t)~m;
            }else{
                uint8_t m0=(uint8_t)(0xFFu>>(x0&7));
                uint8_t m1=(uint8_t)(0xFFu<<(7-((x1-1)&7)));
                int k;
                if(level)row[b0]|=m0;else row[b0]&=(uint8_t)~m0;
                if(level)row[b1]|=m1;else row[b1]&=(uint8_t)~m1;
                for(k=b0+1;k<b1;++k)row[k]=level?0xFFu:0x00u;
            }
        }
    }
}

void wm_bevel_rect(wm_fb_t *fb,const wm_rect_t *r,uint8_t ink,uint8_t paper,uint8_t sunken)
{
    wm_rect_t e;uint8_t hi=sunken?ink:paper,lo=sunken?paper:ink;
    if(!fb||!r||r->w<5||r->h<5)return;
    e.x=r->x;e.y=r->y;e.w=r->w;e.h=1;wm_fill_rect(fb,&e,ink);
    e.y=(int16_t)(r->y+r->h-1);wm_fill_rect(fb,&e,ink);
    e.y=r->y;e.w=1;e.h=r->h;wm_fill_rect(fb,&e,ink);
    e.x=(int16_t)(r->x+r->w-1);wm_fill_rect(fb,&e,ink);
    e.x=(int16_t)(r->x+1);e.y=(int16_t)(r->y+1);e.w=(uint16_t)(r->w-2);e.h=1;wm_fill_rect(fb,&e,hi);
    e.w=1;e.h=(uint16_t)(r->h-2);wm_fill_rect(fb,&e,hi);
    e.x=(int16_t)(r->x+r->w-2);e.y=(int16_t)(r->y+1);e.h=(uint16_t)(r->h-2);wm_fill_rect(fb,&e,lo);
    e.x=(int16_t)(r->x+1);e.y=(int16_t)(r->y+r->h-2);e.w=(uint16_t)(r->w-2);e.h=1;wm_fill_rect(fb,&e,lo);
}
/* --- Mono span copy, arbitrary bit alignment -------------------------------
   The compositor blits a window from its backing surface to the screen: the
   source offset and the destination x usually differ, so the old code fell back
   to a per-pixel loop (~53k iterations for a 320x168 window).  This walks whole
   destination bytes and, when the two offsets share the same byte phase, copies
   32 bits at a time. */
static void mono_bit_copy(const uint8_t *src,uint8_t *dst,uint32_t s,uint32_t d)
{
    uint8_t bit=(uint8_t)((src[s>>3]>>(7u-(s&7u)))&1u);
    uint8_t m=(uint8_t)(0x80u>>(d&7u));
    if(bit)dst[d>>3]|=m;else dst[d>>3]&=(uint8_t)~m;
}
static void mono_span_copy(const uint8_t *src,uint8_t *dst,
                           uint16_t sx,uint16_t dx,uint16_t w,uint16_t src_row_bytes)
{
    uint32_t s=sx,d=dx,end=(uint32_t)dx+(uint32_t)w;
    if(!w)return;
    if(((sx&7u)==0u)&&((dx&7u)==0u)){
        /* Both byte-aligned: straight copy, 32 bits in the middle. */
        uint32_t n=(uint32_t)(w>>3);const uint8_t *sp=src+(sx>>3);uint8_t *dp=dst+(dx>>3);
        /* Align BOTH pointers before 32-bit moves: an unaligned long access
           raises an address error on the 68000 and is a fault on the 030. */
        while(n&&((((uintptr_t)dp|(uintptr_t)sp)&3u)!=0u)){*dp++=*sp++;--n;}
        while(n>=4u){*(uint32_t *)(void *)dp=*(const uint32_t *)(const void *)sp;dp+=4;sp+=4;n-=4;}
        while(n--)*dp++=*sp++;
        /* up to 7 trailing pixels (w not a multiple of 8) */
        {uint32_t done=(uint32_t)(w>>3)*8u;s=sx+done;d=dx+done;}
        while(d<end){mono_bit_copy(src,dst,s,d);++s;++d;}
        return;
    }
    while(d<end&&(d&7u)){mono_bit_copy(src,dst,s,d);++s;++d;}
    while(d+8u<=end){
        uint32_t so=s&7u;
        uint8_t a=src[s>>3];
        uint8_t b=(uint8_t)((so&&((s>>3)+1u<src_row_bytes))?src[(s>>3)+1u]:0u);
        uint32_t val=((uint32_t)a<<8)|(uint32_t)b;
        dst[d>>3]=(uint8_t)((val>>(8u-so))&0xffu);
        d+=8u;s+=8u;
    }
    while(d<end){mono_bit_copy(src,dst,s,d);++s;++d;}
}
void wm_copy_rect(wm_fb_t *dst,const wm_fb_t *src,const wm_rect_t *d,uint16_t sx,uint16_t sy)
{
    int row,col,dx,source_x,width,skip;
    wm_rect_t clipped;
    if(!dst||!src||!dst->data||!src->data||!d)return;
    dx=d->x;source_x=sx;width=d->w;
    if(dx<0){skip=-dx;dx=0;source_x+=skip;width-=skip;}
    if(width>(int)dst->w-dx)width=(int)dst->w-dx;
    if(width>(int)src->w-source_x)width=(int)src->w-source_x;
    if(width<=0)return;
    clipped=*d;clipped.x=(int16_t)dx;clipped.w=(uint16_t)width;
    d=&clipped;sx=(uint16_t)source_x;
    for(row=0;row<(int)d->h;++row){
        int dy=d->y+row,sy2=(int)sy+row;
        const uint8_t *srow;uint8_t *drow;
        if(!inside(dst,d->x,dy))continue;
        if(sy2<0||sy2>=(int)src->h)continue;
        srow=src->data+(uint32_t)sy2*src->pitch;
        drow=dst->data+(uint32_t)dy*dst->pitch;
        if(dst->bpp==8&&src->bpp==8){
            /* Keep both pointers aligned for portable 32-bit accesses.
               Equal alignment phases need only a short byte prefix. */
            int n=d->w;uint8_t *dp=drow+d->x;const uint8_t *sp=srow+sx;
            while(n&&(((uintptr_t)dp|(uintptr_t)sp)&3u)){*dp++=*sp++;--n;}
            while(n>=4){*(uint32_t *)(void *)dp=*(const uint32_t *)(const void *)sp;dp+=4;sp+=4;n-=4;}
            while(n--){*dp++=*sp++;}
        }else if(dst->bpp==1&&src->bpp==1){
            /* Window/canvas blit: fast bit-aligned mono copy. */
            mono_span_copy(srow,drow,sx,(uint16_t)d->x,(uint16_t)d->w,(uint16_t)src->pitch);
        }else{
            for(col=0;col<(int)d->w;++col){
                int dx=d->x+col,sx2=(int)sx+col;uint8_t bit;
                if(sx2<0||sx2>=(int)src->w)continue;
                if(!inside(dst,dx,dy))continue;
                bit=(uint8_t)((srow[sx2>>3]>>(7-(sx2&7)))&1u);
                {uint8_t *p=drow+(dx>>3);uint8_t m=(uint8_t)(0x80u>>(dx&7));
                 if(bit)*p|=m;else *p&=(uint8_t)~m;}
            }
        }
    }
}
