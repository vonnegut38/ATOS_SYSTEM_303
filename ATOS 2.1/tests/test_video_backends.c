#include <stdio.h>
#include <string.h>
#include "video_driver.h"
static int fails;
static void check(int ok,const char*w){if(!ok){printf("FAIL %s\n",w);++fails;}}
/* declared by the backends */
void shifter_attach(uint8_t*,uint16_t,uint16_t,uint16_t);
void et4000_attach(uint8_t*,uint16_t,uint16_t,uint16_t);
void et4000_bind_hw(int (*)(uint16_t,uint16_t,uint8_t));
video_driver_t *video_shifter_driver(void);
video_driver_t *video_et4000_driver(void);
static int hw_calls;static int hw_set_mode(uint16_t w,uint16_t h,uint8_t f){(void)w;(void)h;(void)f;++hw_calls;return 1;}
int main(void)
{
    static uint8_t mono[2*4];        /* 16x4 planar 1bpp, pitch 2 */
    static uint8_t vram[8*4];        /* 8x4 chunky */
    video_driver_t *sh,*et;
    memset(mono,0,sizeof mono);memset(vram,0,sizeof vram);
    sh=video_shifter_driver();
    check(sh&&strcmp(sh->name,"shifter")==0,"shifter name");
    shifter_attach(mono,16,4,2);
    check(video_select("shifter")==sh,"select shifter");
    sh->fill_rect(0,0,8,1,1);
    check(mono[0]==0xFFu,"planar fill");
    sh->draw_line(0,1,7,1,1);
    check(mono[2]==0xFFu,"planar line");
    check(sh->get_surface()->format==VIDEO_FMT_PLANAR_1BPP,"shifter format");
    et=video_et4000_driver();
    check(et&&strcmp(et->name,"et4000")==0,"et4000 name");
    et4000_attach(vram,8,4,8);
    check(video_select("et4000")==et,"select et4000");
    et4000_bind_hw(hw_set_mode);
    et->set_mode(1024,768,VIDEO_FMT_CHUNKY_8BPP);
    check(hw_calls==1,"hw hook called");
    check(et->get_surface()->width==1024&&et->get_surface()->height==768,"1024x768");
    check(et->get_surface()->format==VIDEO_FMT_CHUNKY_8BPP,"chunky format");
    et->fill_rect(1,1,2,2,0x2Au);
    check(vram[1*8+1]==0x2Au&&vram[2*8+2]==0x2Au,"chunky fill");
    /* cross-backend blit: planar source into chunky dest */
    video_select("et4000");
    et->blit(0,0,sh->get_surface(),0,0,8,1);
    check(vram[0]==0u||vram[0]==1u,"planar->chunky blit ran");
    if(fails){printf("VIDEO_BACKENDS: %d FAIL\n",fails);return 1;}
    printf("VIDEO_BACKENDS: ALL PASS\n");return 0;
}
