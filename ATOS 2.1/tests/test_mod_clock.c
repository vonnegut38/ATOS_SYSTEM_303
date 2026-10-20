#include <stdio.h>
#include <string.h>
#include "desktop_module.h"
extern void clock_format(uint32_t seconds,char out[6]);
extern void clock_set_time(uint32_t day_seconds);
extern desktop_module_t desktop_mod_clock;
static int fails;
static void check(int ok,const char *w){if(!ok){printf("FAIL %s\n",w);++fails;}}
int main(void)
{
    static uint8_t fb[64*8];
    video_surface_t s;char buf[6];int nz=0,i;
    clock_format(3661u,buf); check(strcmp(buf,"01:01")==0,"format 01:01");
    clock_format(0u,buf);check(strcmp(buf,"00:00")==0,"format zero");
    clock_format(86399u,buf);check(strcmp(buf,"23:59")==0,"format max");
    s.width=64;s.height=8;s.pitch=64;s.format=VIDEO_FMT_CHUNKY_8BPP;s.pixels=fb;
    memset(fb,0,sizeof fb);
    clock_set_time(3661u);                      /* RTC seed -> 01:01 */
    desktop_mod_clock.tick(50u*3661u);          /* 50 Hz tick base */
    desktop_mod_clock.draw(&s,2,1,60,6);
    for(i=0;i<64*8;++i)if(fb[i])++nz;
    check(nz>0,"clock drew pixels");
    /* A once-a-minute redraw must not ghost: the '0' top row is 111 and the '1'
       top row is 010, so 00:00 -> 10:00 has to clear both top corners. */
    memset(fb,0,sizeof fb);
    desktop_mod_clock.tick(0u);desktop_mod_clock.draw(&s,2,1,60,6);
    check(fb[1*64+2]!=0&&fb[1*64+4]!=0,"zero glyph lights both top corners");
    desktop_mod_clock.tick(10u*3600u*50u);desktop_mod_clock.draw(&s,2,1,60,6);
    check(fb[1*64+2]==0&&fb[1*64+4]==0,"one glyph clears the old corners");
    /* clipping: must not crash off-screen */
    desktop_mod_clock.draw(&s,-100,-100,60,6);
    check(1,"clipped draw ok");
    if(fails){printf("MOD_CLOCK: %d FAIL\n",fails);return 1;}
    printf("MOD_CLOCK: ALL PASS\n");return 0;
}
