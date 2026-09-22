#include "video_driver.h"

#define VIDEO_MAX_DRIVERS 4
static video_driver_t *video_drivers[VIDEO_MAX_DRIVERS];
static int video_driver_count;
static video_driver_t *video_current;

void video_register(video_driver_t *driver)
{
    int i;
    if(!driver)return;
    for(i=0;i<video_driver_count;++i){if(video_drivers[i]==driver)return;}
    if(video_driver_count<VIDEO_MAX_DRIVERS)video_drivers[video_driver_count++]=driver;
}
video_driver_t *video_select(const char *name)
{
    int i;
    if(!name){video_current=0;return 0;}
    for(i=0;i<video_driver_count;++i){
        const char *a=video_drivers[i]->name;const char *b=name;int j=0;
        while(a[j]&&b[j]&&a[j]==b[j])++j;
        if(!a[j]&&!b[j]){video_current=video_drivers[i];if(video_current->init)video_current->init();return video_current;}
    }
    return 0;
}
video_driver_t *video_active(void){return video_current;}

/* ---- shared pixel helper (planar 1bpp and chunky 8bpp) ---- */
void video_put_pixel(video_surface_t *s,int x,int y,uint8_t color)
{
    uint8_t *row;
    if(!s||!s->pixels||x<0||y<0||x>=(int)s->width||y>=(int)s->height)return;
    row=s->pixels+(uint32_t)y*s->pitch;
    if(s->format==VIDEO_FMT_CHUNKY_8BPP){row[x]=color;return;}
    if(s->format==VIDEO_FMT_PLANAR_1BPP){
        uint8_t mask=(uint8_t)(0x80u>>(x&7));
        if(color)row[x>>3]|=mask;else row[x>>3]&=(uint8_t)~mask;
        return;
    }
    /* planar 2/4bpp not needed by the first backend */
}

/* ---- software backend (desktop/compositor) ---- */
static video_surface_t sw_surface;

void video_sw_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch,uint8_t format)
{
    sw_surface.pixels=pixels;sw_surface.width=width;sw_surface.height=height;
    sw_surface.pitch=pitch;sw_surface.format=format;
}
static int sw_init(void){return sw_surface.pixels?1:0;}
static void sw_shutdown(void){}
static int sw_set_mode(uint16_t w,uint16_t h,uint8_t fmt){sw_surface.width=w;sw_surface.height=h;sw_surface.format=fmt;return 1;}
static const video_surface_t *sw_get_surface(void){return &sw_surface;}
static void sw_set_palette(const uint16_t *rgb,uint16_t first,uint16_t count){(void)rgb;(void)first;(void)count;}
static void sw_fill_rect(int x,int y,int w,int h,uint8_t color)
{
    int j;
    if(!sw_surface.pixels||w<=0||h<=0)return;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(x+w>(int)sw_surface.width)w=(int)sw_surface.width-x;
    if(y+h>(int)sw_surface.height)h=(int)sw_surface.height-y;
    for(j=0;j<h;++j){
        int i;
        uint8_t *row=sw_surface.pixels+(uint32_t)(y+j)*sw_surface.pitch+x;
        if(sw_surface.format==VIDEO_FMT_CHUNKY_8BPP){
            for(i=0;i<w;++i)row[i]=color;
        }else{
            for(i=0;i<w;++i)video_put_pixel(&sw_surface,x+i,y+j,color);
        }
    }
}
static void sw_draw_line(int x0,int y0,int x1,int y1,uint8_t color)
{
    int dx=x1-x0,dy=y1-y0,sx=1,sy=1,err;
    if(dx<0){dx=-dx;sx=-1;}
    if(dy<0){dy=-dy;sy=-1;}
    err=dx-dy;
    for(;;){
        video_put_pixel(&sw_surface,x0,y0,color);
        if(x0==x1&&y0==y1)break;
        {int e2=err+err;if(e2>-dy){err-=dy;x0+=sx;}if(e2<dx){err+=dx;y0+=sy;}}
    }
}
static void sw_blit(int dx,int dy,const video_surface_t *src,int sx,int sy,int w,int h)
{
    int j;
    if(!src||!src->pixels||!sw_surface.pixels||w<=0||h<=0)return;
    for(j=0;j<h;++j){
        int i;
        int srow=sy+j,drow=dy+j;
        if(srow<0||srow>=(int)src->height)continue;
        if(drow<0||drow>=(int)sw_surface.height)continue;
        for(i=0;i<w;++i){
            int scol=sx+i,dcol=dx+i;
            if(scol<0||scol>=(int)src->width)continue;
            if(dcol<0||dcol>=(int)sw_surface.width)continue;
            {uint8_t v;
             const uint8_t *srowp=src->pixels+(uint32_t)srow*src->pitch;
             if(src->format==VIDEO_FMT_CHUNKY_8BPP)v=srowp[scol];
             else v=(uint8_t)((srowp[scol>>3]>>(7-(scol&7)))&1u);
             video_put_pixel(&sw_surface,dcol,drow,v);}
        }
    }
}
static void sw_show_cursor(int x,int y){(void)x;(void)y;}
static void sw_hide_cursor(void){}
static void sw_vsync(void){}

static video_driver_t sw_driver={
    "software",
    sw_init,sw_shutdown,sw_set_mode,sw_get_surface,sw_set_palette,
    sw_fill_rect,sw_draw_line,sw_blit,sw_show_cursor,sw_hide_cursor,sw_vsync
};
video_driver_t *video_sw_driver(void){video_register(&sw_driver);return &sw_driver;}

/* ---- active screen surface (compositor destination) ---- */
static video_surface_t screen_surface;
void video_screen_attach(uint8_t *pixels,uint16_t width,uint16_t height,uint16_t pitch,uint8_t format)
{
    screen_surface.pixels=pixels;screen_surface.width=width;screen_surface.height=height;
    screen_surface.pitch=pitch;screen_surface.format=format;
}
const video_surface_t *video_screen_surface(void){return &screen_surface;}
