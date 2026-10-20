#include "video_driver.h"
/* ET4000/W32 backend: chunky 8bpp, default 1024x768x8.  This file owns the
 * framebuffer contract only.  Real VGA register programming stays in
 * system/vofa_et4000.c and is installed through et4000_bind_hw(), so nothing
 * here knows about card registers. */
static video_surface_t et_surface;
static int (*et_hw_set_mode)(uint16_t,uint16_t,uint8_t);
void et4000_bind_hw(int (*set_mode)(uint16_t,uint16_t,uint8_t)){et_hw_set_mode=set_mode;}
void et4000_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch)
{
    et_surface.pixels=pixels;et_surface.width=width;et_surface.height=height;
    et_surface.pitch=pitch;et_surface.format=VIDEO_FMT_CHUNKY_8BPP;
}
static int et_init(void){return et_surface.pixels?1:0;}
static void et_shutdown(void){}
static int et_set_mode(uint16_t w,uint16_t h,uint8_t fmt)
{
    et_surface.width=w;et_surface.height=h;et_surface.format=fmt;
    if(et_hw_set_mode)return et_hw_set_mode(w,h,fmt);
    return 1;
}
static const video_surface_t *et_get_surface(void){return &et_surface;}
static void et_set_palette(const uint16_t *rgb,uint16_t first,uint16_t count){(void)rgb;(void)first;(void)count;}
static void et_fill_rect(int x,int y,int w,int h,uint8_t color)
{
    int j;
    if(!et_surface.pixels||w<=0||h<=0)return;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(x+w>(int)et_surface.width)w=(int)et_surface.width-x;
    if(y+h>(int)et_surface.height)h=(int)et_surface.height-y;
    for(j=0;j<h;++j){
        uint8_t *row=et_surface.pixels+(uint32_t)(y+j)*et_surface.pitch+x;
        int i;
        for(i=0;i<w;++i)row[i]=color;
    }
}
static void et_draw_line(int x0,int y0,int x1,int y1,uint8_t color)
{
    int dx=x1-x0,dy=y1-y0,sx=1,sy=1,err;
    if(dx<0){dx=-dx;sx=-1;}
    if(dy<0){dy=-dy;sy=-1;}
    err=dx-dy;
    for(;;){
        video_put_pixel(&et_surface,x0,y0,color);
        if(x0==x1&&y0==y1)break;
        {int e2=err+err;if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}
    }
}
static void et_blit(int dx,int dy,const video_surface_t *src,int sx,int sy,int w,int h)
{
    int j,i;
    if(!src||!src->pixels)return;
    for(j=0;j<h;++j){
        int drow=dy+j,srow=sy+j;
        if(srow<0||srow>=(int)src->height||drow<0||drow>=(int)et_surface.height)continue;
        for(i=0;i<w;++i){
            int dcol=dx+i,scol=sx+i;
            uint8_t v;
            if(scol<0||scol>=(int)src->width||dcol<0||dcol>=(int)et_surface.width)continue;
            if(src->format==VIDEO_FMT_CHUNKY_8BPP){v=src->pixels[(uint32_t)srow*src->pitch+scol];}
            else{v=(uint8_t)((src->pixels[(uint32_t)srow*src->pitch+(scol>>3)]>>(7-(scol&7)))&1u);}
            video_put_pixel(&et_surface,dcol,drow,v);
        }
    }
}
static void et_show_cursor(int x,int y){(void)x;(void)y;}
static void et_hide_cursor(void){}
static void et_vsync(void){}
static video_driver_t et_driver={"et4000",et_init,et_shutdown,et_set_mode,et_get_surface,et_set_palette,
    et_fill_rect,et_draw_line,et_blit,et_show_cursor,et_hide_cursor,et_vsync};
video_driver_t *video_et4000_driver(void){video_register(&et_driver);return &et_driver;}
