#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "emutos_vdi_mono.h"

#define NOINLINE __attribute__((noinline))
typedef void (*copyfn)(volatile uint16_t*,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t,uint16_t);

/* Previous implementation (reverse base + neg/add per word). */
NOINLINE static void old_words(volatile uint16_t *words,uint16_t line_words,
    uint16_t source_x,uint16_t source_y,uint16_t width,uint16_t height,
    uint16_t destination_x,uint16_t destination_y)
{
    uint16_t groups=(uint16_t)(width>>4),row,group;
    volatile uint16_t *source=words+(uint32_t)source_y*line_words+(source_x>>4);
    volatile uint16_t *destination=words+(uint32_t)destination_y*line_words+(destination_x>>4);
    if(source==destination)return;
    if(source<destination){
        source+=(uint32_t)(height-1u)*line_words+groups-1u;
        destination+=(uint32_t)(height-1u)*line_words+groups-1u;
        for(row=0;row<height;++row){volatile uint16_t *s=source,*d=destination;
            for(group=0;group<groups;++group)*d--=*s--;source-=line_words;destination-=line_words;}
    }else{
        for(row=0;row<height;++row){volatile uint16_t *s=source,*d=destination;
            for(group=0;group<groups;++group)*d++=*s++;source+=line_words;destination+=line_words;}
    }
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+1e-9*t.tv_nsec;}
int main(void)
{
    static uint16_t screen[400*40];
    static copyfn volatile f_old=old_words;
    static copyfn volatile f_new=emutos_vdi_mono_copy_words;
    static copyfn volatile f_sh=emutos_vdi_mono_copy_shifted;
    const int N=20000;double a,b,e;int i;
    for(i=0;i<400*40;i++)screen[i]=(uint16_t)(i*2654435761u);
    a=now();for(i=0;i<N;i++)f_old(screen,40,0,0,320,64,320,0);a=(now()-a)/N*1e9;
    b=now();for(i=0;i<N;i++)f_new(screen,40,0,0,320,64,320,0);b=(now()-b)/N*1e9;
    e=now();for(i=0;i<N;i++)f_sh(screen,40,0,100,300,64,7,200);e=(now()-e)/N*1e9;
    printf("old_words   %8.1f ns/call\nnew(emutos) %8.1f ns/call\nshifted     %8.1f ns/call\n",a,b,e);
    return 0;
}
