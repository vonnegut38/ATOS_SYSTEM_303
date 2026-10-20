#ifndef WM_REFORMAT_H
#define WM_REFORMAT_H
#include <stdint.h>
static inline void repitch_surface(uint8_t *s,unsigned oldpitch,unsigned oldw,unsigned oldh,
                             unsigned oldbpp,unsigned pitch,unsigned w,unsigned h,unsigned bpp)
{
    unsigned row,col,ch=oldh<h?oldh:h,cw=oldw<w?oldw:w;
    if(!s)return;
    /* Expand backwards, contract forwards, including a mono/chunky conversion. */
    for(row=0;row<ch;++row){
        unsigned y=pitch>oldpitch?ch-1-row:row;
        uint8_t *dst=s+y*pitch,*src=s+y*oldpitch;
        if(bpp==oldbpp){
            unsigned bytes=bpp==8u?cw:(cw+7u)/8u;
            if(dst>src){for(col=bytes;col;--col)dst[col-1]=src[col-1];}
            else{for(col=0;col<bytes;++col)dst[col]=src[col];}
            for(col=bytes;col<pitch;++col)dst[col]=0;
        }else if(bpp==8u){
            for(col=cw;col;--col){unsigned x=col-1;dst[x]=(src[x>>3]&(0x80u>>(x&7)))?1u:0u;}
            for(col=cw;col<pitch;++col)dst[col]=0;
        }else{
            for(col=0;col<pitch;++col){unsigned bit;uint8_t v=0;
                for(bit=0;bit<8;++bit){unsigned x=col*8+bit;if(x<cw&&src[x])v|=(uint8_t)(0x80u>>bit);}
                dst[col]=v;
            }
        }
    }
    for(row=ch;row<h;++row)for(col=0;col<pitch;++col)s[row*pitch+col]=0;
}
#endif
