#include <stdio.h>
#include "desktop_module.h"
static int fails;
static void check(int ok,const char *w){if(!ok){printf("FAIL %s\n",w);++fails;}}
static int inits,ticks,draws,events;
static int my_tick_seq;
static int m_init(void *c){(void)c;++inits;return 1;}
static void m_draw(video_surface_t *s,int x,int y,int w,int h){(void)s;(void)x;(void)y;(void)w;(void)h;++draws;}
static void m_tick(uint32_t n){(void)n;++ticks;my_tick_seq=1;}
static int m_event(int t,int a,int b){(void)t;(void)a;(void)b;++events;return 1;}
static void m_stop(void){}
static desktop_module_t mod_a={"clock",1,m_init,m_draw,m_tick,m_event,m_stop};
static desktop_module_t mod_b={"launcher",1,m_init,m_draw,m_tick,m_event,m_stop};
int main(void)
{
    video_surface_t s;
    desktop_module_reset();
    check(desktop_module_count()==0,"empty");
    desktop_module_register(&mod_a);
    desktop_module_register(&mod_b);
    desktop_module_register(&mod_a);          /* duplicate ignored */
    check(desktop_module_count()==2,"count 2");
    check(desktop_module_find("clock")==&mod_a,"find clock");
    check(desktop_module_find("nope")==0,"find missing");
    check(desktop_module_init_all(0)==2&&inits==2,"init all");
    desktop_module_tick_all(50u);
    check(ticks==2,"tick all");
    desktop_module_draw_all(&s,0,0,640,16);
    check(draws==2,"draw all");
    check(desktop_module_name(0)[0]=='c'||desktop_module_name(1)[0]=='c',"names");
    /* capacity: fill and reject overflow */
    {desktop_module_t *more[6];static desktop_module_t buf[6];int i;
     for(i=0;i<6;++i){buf[i]=mod_a;buf[i].name="x";more[i]=&buf[i];}
     for(i=0;i<6;++i){desktop_module_register(more[i]);}
     check(desktop_module_count()==8,"capacity 8");}
    desktop_module_reset();
    check(desktop_module_count()==0,"reset");
    if(fails){printf("DESKTOP_MODULE: %d FAIL\n",fails);return 1;}
    printf("DESKTOP_MODULE: ALL PASS\n");return 0;
}
