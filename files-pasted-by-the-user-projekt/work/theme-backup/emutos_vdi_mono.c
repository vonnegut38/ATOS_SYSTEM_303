/*
 * Minimal MC68000 Shifter rectangle backend adapted from EmuTOS
 * vdi/vdi_line.c draw_rect_setup() and swblit_rect_common().
 *
 * Upstream revision: e0f0c2333d39a487e73d8b17f62b70a95441cd32
 * Copyright 2001-2026 The EmuTOS development team
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "emutos_vdi_mono.h"

void emutos_vdi_mono_fill(volatile uint16_t *words,uint16_t line_words,
                          uint16_t x,uint16_t y,uint16_t width,uint16_t height,
                          uint8_t color)
{
    uint16_t leftmask=(uint16_t)(0xffffu>>(x&15u));
    uint16_t x2=(uint16_t)(x+width-1u);
    uint16_t rightmask=(uint16_t)(0xffffu<<(15u-(x2&15u)));
    uint16_t groups=(uint16_t)((x2>>4)-(x>>4)+1u),row,n;
    uint16_t pattern=color?0xffffu:0u;
    volatile uint16_t *work=words+(uint32_t)y*line_words+(x>>4);

    if(groups==1u){leftmask&=rightmask;rightmask=0u;}
    for(row=0;row<height;++row,work+=line_words){
        volatile uint16_t *destination=work;
        *destination=(uint16_t)((*destination&~leftmask)|(pattern&leftmask));
        ++destination;
        for(n=2u;n<groups;++n)*destination++=pattern;
        if(rightmask)
            *destination=(uint16_t)((*destination&~rightmask)|(pattern&rightmask));
    }
}

/* Aligned one-plane source-only blit.  EmuTOS bit_blt() selects reverse
 * traversal when an overlapping destination follows the source in screen
 * memory; this reduced form needs no skew or end masks because all bounds are
 * complete 16-pixel words. */
void emutos_vdi_mono_copy_words(volatile uint16_t *words,uint16_t line_words,
                                uint16_t source_x,uint16_t source_y,
                                uint16_t width,uint16_t height,
                                uint16_t destination_x,uint16_t destination_y)
{
    uint16_t groups=(uint16_t)(width>>4),row,group;
    volatile uint16_t *source=words+(uint32_t)source_y*line_words+(source_x>>4);
    volatile uint16_t *destination=words+(uint32_t)destination_y*line_words+
                                   (destination_x>>4);
    if(source==destination)return;
    if(source<destination){
        source+=(uint32_t)(height-1u)*line_words+groups-1u;
        destination+=(uint32_t)(height-1u)*line_words+groups-1u;
        for(row=0;row<height;++row){
            volatile uint16_t *s=source,*d=destination;
            for(group=0;group<groups;++group)*d--=*s--;
            source-=line_words;destination-=line_words;
        }
    }else{
        for(row=0;row<height;++row){
            volatile uint16_t *s=source,*d=destination;
            for(group=0;group<groups;++group)*d++=*s++;
            source+=line_words;destination+=line_words;
        }
    }
}

static uint16_t mono_word(const volatile uint16_t *words,uint16_t line_words,
                          uint16_t row,int32_t group)
{
    if(group<0||group>=line_words)return 0u;
    return words[(uint32_t)row*line_words+(uint32_t)group];
}

/* Reduced skew/end-mask form of EmuTOS vdi/vdi_raster.c bit_blt().  A whole
 * destination row is assembled before it is stored, making horizontal
 * overlap safe without exposing the hardware Blitter to untrusted bounds. */
void emutos_vdi_mono_copy_shifted(volatile uint16_t *words,uint16_t line_words,
                                  uint16_t source_x,uint16_t source_y,
                                  uint16_t width,uint16_t height,
                                  uint16_t destination_x,uint16_t destination_y)
{
    uint16_t assembled[40],first=(uint16_t)(destination_x>>4);
    uint16_t last=(uint16_t)((destination_x+width-1u)>>4);
    uint16_t count=(uint16_t)(last-first+1u),source_row,group,mask;
    uint16_t left=(uint16_t)(0xffffu>>(destination_x&15u));
    uint16_t right=(uint16_t)(0xffffu<<(15u-((destination_x+width-1u)&15u)));
    uint16_t high,low;int32_t row,row_end,row_step,x,source_group;uint8_t shift;
    volatile uint16_t *destination;
    if(destination_y>source_y){row=(int32_t)height-1;row_end=-1;row_step=-1;}
    else{row=0;row_end=height;row_step=1;}
    for(;row!=row_end;row+=row_step){
        source_row=(uint16_t)(source_y+row);
        for(group=0;group<count;++group){
            x=(int32_t)source_x+(int32_t)(first+group)*16-(int32_t)destination_x;
            if(x<0){source_group=-1;shift=(uint8_t)(x+16);}
            else{source_group=x>>4;shift=(uint8_t)(x&15);}
            high=mono_word(words,line_words,source_row,source_group);
            if(!shift)assembled[group]=high;
            else{low=mono_word(words,line_words,source_row,source_group+1);
                assembled[group]=(uint16_t)((uint16_t)(high<<shift)|
                                            (uint16_t)(low>>(16u-shift)));}
        }
        destination=words+(uint32_t)(destination_y+row)*line_words+first;
        for(group=0;group<count;++group){
            mask=0xffffu;if(!group)mask&=left;if(group==count-1u)mask&=right;
            destination[group]=(uint16_t)((destination[group]&~mask)|
                                          (assembled[group]&mask));
        }
    }
}
