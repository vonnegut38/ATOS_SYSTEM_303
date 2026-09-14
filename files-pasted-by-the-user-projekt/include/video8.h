#ifndef ASF_VIDEO8_H
#define ASF_VIDEO8_H
#include <stdint.h>

/* ET4000 640x480 chunky (8 bpp) graphics used by the desktop/window layer. */
void video8_enter(void);
void nova_enter_text_vga(void);
void video8_leave(void);
void video8_clear(uint8_t color);
void video8_blit_mono(int x,int y,int w,int h,const uint8_t *source,int stride,
                      uint8_t fg,uint8_t bg);
void video8_pixel(int x,int y,uint8_t color);
void video8_fill(int x,int y,int w,int h,uint8_t color);
void video8_hline(int x,int y,int w,uint8_t color);
void video8_vline(int x,int y,int h,uint8_t color);
void video8_rect(int x,int y,int w,int h,uint8_t color);
void video8_text(int x,int y,const char *s,uint8_t fg,uint8_t bg);
void video8_blit(int sx,int sy,int w,int h,int dx,int dy);
void video8_demo(void);
int  video8_is_w32i(void);
uint8_t  video8_mode(void);
uint16_t video8_width(void);
uint16_t video8_height(void);
uint16_t video8_stride(void);

#endif
