#include "desktop_module.h"
/* Example desktop applet: a top-bar clock.  It formats HH:MM:SS from the
   50 Hz tick counter and draws a compact 3x5 digit font into any
   video_surface_t.  No hardware knowledge.  Host-tested in tests/test_mod_clock.c. */
static uint32_t clk_seconds;
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
void clock_format(uint32_t seconds,char out[9])
{
    uint32_t h=(seconds/3600u)%100u,m=(seconds/60u)%60u,s=seconds%60u;
    out[0]=(char)('0'+h/10u);out[1]=(char)('0'+h%10u);out[2]=':';
    out[3]=(char)('0'+m/10u);out[4]=(char)('0'+m%10u);out[5]=':';
    out[6]=(char)('0'+s/10u);out[7]=(char)('0'+s%10u);out[8]='\0';
}
static int clk_init(void *config){(void)config;clk_seconds=0;return 1;}
static void clk_tick(uint32_t now_ticks){clk_seconds=now_ticks/50u;}
static void clk_draw(video_surface_t *surface,int x,int y,int w,int h)
{
    char buf[9];int i,r,c;
    (void)w;(void)h;
    clock_format(clk_seconds,buf);
    for(i=0;buf[i];++i){
        const uint8_t *g=clk_font[clk_glyph(buf[i])];
        for(r=0;r<5;++r)for(c=0;c<3;++c)
            if(g[r]&(uint8_t)(4>>c))video_put_pixel(surface,x+i*4+c,y+r,1);
    }
}
desktop_module_t desktop_mod_clock={"clock",1,clk_init,clk_draw,clk_tick,0,0};
