#include "desktop_module.h"
/* Example desktop applet: a top-bar clock.  It formats HH:MM (no seconds, so
   the compositor only redraws it once a minute) from the 50 Hz tick counter and
   draws a compact 3x5 digit font into any video_surface_t.  No hardware
   knowledge.  Host-tested in tests/test_mod_clock.c. */
static uint32_t clk_seconds;
/* Wall-clock seconds at clk_base_tick: the kernel seeds this from the RTC (or
   leaves it 0 for the 12:00 AM default) and the applet advances with the 50 Hz
   tick counter, so the RTC is read once and never polled again. */
static uint32_t clk_day_seconds,clk_base_tick;
static uint8_t clk_base_set;
/* Mono palette: 1 = white, 0 = black.  The EmuTOS menu bar is white paper, so
   the kernel flips the clock to black ink on white; the defaults keep the
   module usable on its own (white ink on black). */
static uint8_t clk_ink=1u,clk_paper=0u,clk_transparent;
/* rows top->bottom, 3 bits each (bit2..bit0) */
static const uint8_t clk_font[11][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7},
 {0,2,0,2,0}};
static int clk_glyph(char c)
{
    if(c==':')return 10;
    if(c>='0'&&c<='9')return c-'0';
    return 0;
}
void clock_format(uint32_t seconds,char out[6])
{
    uint32_t h=(seconds/3600u)%100u,m=(seconds/60u)%60u;
    out[0]=(char)('0'+h/10u);out[1]=(char)('0'+h%10u);out[2]=':';
    out[3]=(char)('0'+m/10u);out[4]=(char)('0'+m%10u);out[5]='\0';
}
void clock_set_time(uint32_t day_seconds){clk_day_seconds=day_seconds%86400u;clk_base_set=0;}
void clock_set_colors(uint8_t ink,uint8_t paper){clk_ink=ink;clk_paper=paper;}
/* On a themed top bar the clock must not paint its own background. */
void clock_set_transparent(uint8_t on){clk_transparent=on?1u:0u;}
static int clk_init(void *config){(void)config;clk_seconds=0;clk_day_seconds=0;clk_base_set=0;return 1;}
static void clk_tick(uint32_t now_ticks)
{
    /* Re-anchor if the very first tick has not happened yet or the tick counter
       went backwards (a fresh seed or a wrap): never advance from a stale base. */
    if(!clk_base_set||now_ticks<clk_base_tick){clk_base_tick=now_ticks;clk_base_set=1;}
    clk_seconds=(clk_day_seconds+(now_ticks-clk_base_tick)/50u)%86400u;
}
static void clk_draw(video_surface_t *surface,int x,int y,int w,int h)
{
    char buf[6];int i,r,c;
    (void)w;(void)h;
    clock_format(clk_seconds,buf);
    for(i=0;buf[i];++i){
        const uint8_t *g=clk_font[clk_glyph(buf[i])];
        /* Paint the 0 as well as the 1: the clock is refreshed once a minute,
           so a digit whose glyph got simpler must erase its previous pixels. */
        for(r=0;r<5;++r)for(c=0;c<3;++c){
            int on=(g[r]&(uint8_t)(4>>c))?1:0;
            if(!on&&clk_transparent)continue;
            video_put_pixel(surface,x+i*4+c,y+r,on?clk_ink:clk_paper);
        }
    }
}
desktop_module_t desktop_mod_clock={"clock",1,clk_init,clk_draw,clk_tick,0,0};
