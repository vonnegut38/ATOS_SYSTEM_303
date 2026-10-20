#include <stdio.h>
#include <string.h>
#include "video_driver.h"
static int fails;
static void check(int ok,const char*w){if(!ok){printf("FAIL %s\n",w);++fails;}}
int main(void)
{
    static uint8_t fb[64*32];
    video_surface_t src;
    static uint8_t srcbuf[4*4];
    video_driver_t *d;
    int i;
    memset(fb,0,sizeof fb);
    for(i=0;i<4*4;++i)srcbuf[i]=(uint8_t)(i+1);
    src.width=4;src.height=4;src.pitch=4;src.format=VIDEO_FMT_CHUNKY_8BPP;src.pixels=srcbuf;
    video_sw_attach(fb,64,32,64,VIDEO_FMT_CHUNKY_8BPP);
    d=video_sw_driver();
    check(d!=0&&d->name&&strcmp(d->name,"software")==0,"driver name");
    check(video_select("software")==d,"select software");
    check(video_active()==d,"active");
    check(!video_select("nope"),"unknown driver");
    video_select("software");
    d->fill_rect(2,2,4,3,7);
    check(fb[2*64+2]==7&&fb[4*64+5]==7,"fill_rect");
    check(fb[1*64+1]==0,"fill_rect bounds");
    d->fill_rect(-5,-5,3,3,9);   /* must clip, not crash */
    check(fb[0]==0,"clip no crash");
    d->draw_line(0,0,5,0,3);
    check(fb[0]==3&&fb[5]==3,"draw_line");
    d->blit(10,10,&src,0,0,4,4);
    check(fb[10*64+10]==1&&fb[13*64+13]==16,"blit");
    d->blit(62,30,&src,0,0,4,4); /* partial off-screen */
    check(fb[31*64+63]==6,"blit partial");
    d->set_palette(0,0,0);d->show_cursor(1,1);d->hide_cursor();d->vsync();
    check(d->get_surface()->width==64&&d->get_surface()->height==32,"surface");
    /* planar 1bpp pixel helper */
    {uint8_t p[8];video_surface_t s2;s2.width=8;s2.height=1;s2.pitch=1;s2.format=VIDEO_FMT_PLANAR_1BPP;s2.pixels=p;
     memset(p,0,8);video_put_pixel(&s2,0,0,1);video_put_pixel(&s2,7,0,1);check(p[0]==0x81u,"planar put_pixel");}
    if(fails){printf("VIDEO_DRIVER: %d FAIL\n",fails);return 1;}
    printf("VIDEO_DRIVER: ALL PASS\n");return 0;
}
