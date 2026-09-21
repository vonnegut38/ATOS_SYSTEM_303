#include "wm_blit.h"

static int inside(const wm_fb_t *fb,int x,int y)
{return fb&&x>=0&&y>=0&&x<(int)fb->w&&y<(int)fb->h;}

void wm_fill_rect(wm_fb_t *fb,const wm_rect_t *r,uint8_t level)
{
    int y,x0,y0,x1,y1;
    if(!fb||!r)return;
    x0=r->x;y0=r->y;x1=r->x+r->w;y1=r->y+r->h;
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

void wm_copy_rect(wm_fb_t *dst,const wm_fb_t *src,const wm_rect_t *d,uint16_t sx,uint16_t sy)
{
    int row,col;
    if(!dst||!src||!d)return;
    for(row=0;row<(int)d->h;++row){
        int dy=d->y+row,sy2=(int)sy+row;
        const uint8_t *srow;uint8_t *drow;
        if(!inside(dst,d->x,dy))continue;
        if(sy2<0||sy2>=(int)src->h)continue;
        srow=src->data+(uint32_t)sy2*src->pitch;
        drow=dst->data+(uint32_t)dy*dst->pitch;
        if(dst->bpp==8&&src->bpp==8){
            /* chunky: 32-bit row copy, with byte ends for the unaligned parts */
            int n=d->w,c=0;uint8_t *dp=drow+d->x;const uint8_t *sp=srow+sx;
            while(n>=(int)4&&(((uintptr_t)dp&3u)==0)){*(uint32_t *)(void *)dp=*(const uint32_t *)(const void *)sp;dp+=4;sp+=4;n-=4;}
            while(n--){*dp++=*sp++;}
            (void)c;
        }else if(dst->bpp==1&&src->bpp==1&&((sx&7u)==0u)&&((d->x&7u)==0u)&&((d->w&7u)==0u)){
            /* Aligned mono span (the usual window case): one memcpy per row. */
            {uint32_t n=(uint32_t)(d->w>>3);const uint8_t *s=srow+(sx>>3);uint8_t *p=drow+(d->x>>3);
             while(n--)*p++=*s++;}
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
