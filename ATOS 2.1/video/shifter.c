#include "video_driver.h"
/* Shifter backend: planar 1bpp (ST high mono).  It owns only the surface
 * contract; palette/base registers are programmed by hw/shifter.c. */
static video_surface_t sh_surface;
void shifter_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch)
{
    sh_surface.pixels=pixels;sh_surface.width=width;sh_surface.height=height;
    sh_surface.pitch=pitch;sh_surface.format=VIDEO_FMT_PLANAR_1BPP;
}
static int sh_init(void){return sh_surface.pixels?1:0;}
static void sh_shutdown(void){}
static int sh_set_mode(uint16_t w,uint16_t h,uint8_t fmt){(void)fmt;sh_surface.width=w;sh_surface.height=h;return 1;}
static const video_surface_t *sh_get_surface(void){return &sh_surface;}
static void sh_set_palette(const uint16_t *rgb,uint16_t first,uint16_t count){(void)rgb;(void)first;(void)count;}
static void sh_fill_rect(int x,int y,int w,int h,uint8_t color)
{
    int j,i;
    if(w<=0||h<=0)return;
    for(j=0;j<h;++j){
        for(i=0;i<w;++i)video_put_pixel(&sh_surface,x+i,y+j,color);
    }
}
static void sh_draw_line(int x0,int y0,int x1,int y1,uint8_t color)
{
    int dx=x1-x0,dy=y1-y0,sx=1,sy=1,err;
    if(dx<0){dx=-dx;sx=-1;}
    if(dy<0){dy=-dy;sy=-1;}
    err=dx-dy;
    for(;;){
        video_put_pixel(&sh_surface,x0,y0,color);
        if(x0==x1&&y0==y1)break;
        {int e2=err+err;if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}
    }
}
static void sh_blit(int dx,int dy,const video_surface_t *src,int sx,int sy,int w,int h)
{
    int j,i;
    if(!src||!src->pixels)return;
    for(j=0;j<h;++j){
        int drow=dy+j,srow=sy+j;
        if(srow<0||srow>=(int)src->height||drow<0||drow>=(int)sh_surface.height)continue;
        for(i=0;i<w;++i){
            int dcol=dx+i,scol=sx+i;
            uint8_t v;
            if(scol<0||scol>=(int)src->width||dcol<0||dcol>=(int)sh_surface.width)continue;
            if(src->format==VIDEO_FMT_CHUNKY_8BPP){v=src->pixels[(uint32_t)srow*src->pitch+scol];}
            else{v=(uint8_t)((src->pixels[(uint32_t)srow*src->pitch+(scol>>3)]>>(7-(scol&7)))&1u);}
            video_put_pixel(&sh_surface,dcol,drow,v);
        }
    }
}
static void sh_show_cursor(int x,int y){(void)x;(void)y;}
static void sh_hide_cursor(void){}
static void sh_vsync(void){}
static video_driver_t sh_driver={"shifter",sh_init,sh_shutdown,sh_set_mode,sh_get_surface,sh_set_palette,
    sh_fill_rect,sh_draw_line,sh_blit,sh_show_cursor,sh_hide_cursor,sh_vsync};
video_driver_t *video_shifter_driver(void){video_register(&sh_driver);return &sh_driver;}
