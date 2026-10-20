#!/usr/bin/env python3
"""Extract the production renderer, instrument pixel writes, test damage bounds.
Usage: python3 test_renderer.py /path/to/ATOS
No project writes; requires cc.
"""
import pathlib, subprocess, sys, tempfile
root=pathlib.Path(sys.argv[1]).resolve()
s=(root/'system/kernel_graphics.c').read_text()
def function(name):
    a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;i=b+1
    while depth:
        depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[a:i]
code=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gui_damage.h"
#include "icons_color.h"
static struct {int width,height;} graphics={64,64};
typedef struct {int chunky;} surface_t;
typedef int cursor_guard_t;
static unsigned writes,violations;
static int dl,dt,dw,dh;
static uint8_t emutos_system_font[256][16];
static surface_t active_surface(void){return (surface_t){1};}
static int cursor_guard_begin(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;return 0;}
static void cursor_guard_end(int g){(void)g;}
static void mark_dirty(int x,int y,int w,int h){dl=x;dt=y;dw=w;dh=h;}
static void surface_pixel_set(const surface_t *s,uint16_t x,uint16_t y,uint8_t c){
 (void)s;(void)c;writes++;
 if(x>=graphics.width||y>=graphics.height)violations++;
}
'''+function('static int clip_rect(')+'\n'+function('void kgfx_icon_color(')+r'''
int main(int argc,char **argv){
 (void)argv;
 if(argc>1){
  int coords[]={-32,-8,-1,0,1,31,48,63,64,80};
  memset(emutos_system_font,255,sizeof(emutos_system_font));
  for(unsigned id=0;id<ICON_COLOR_COUNT;id++)for(unsigned x=0;x<10;x++)for(unsigned y=0;y<10;y++)
   kgfx_icon_color(coords[x],coords[y],id,"MY ATARI");
  printf("pixel writes=%u, outside surface=%u\n",writes,violations);
  assert(violations==0);
  kgfx_icon_color(16,8,0,"");assert(dl<=16&&dl+dw>=48&&dt==8&&dh==48);
 }else{
  gui_draw_t c={0};int l,t,r,b;
  c.kind=GUI_ICON_RGB;c.x=16;c.y=8;
  assert(gui_damage(&c,1,64,64,&l,&t,&r,&b));
  assert(l<=16&&r>=48&&t==8&&b==56);
  c.kind=GUI_ICON;assert(gui_damage(&c,1,64,64,&l,&t,&r,&b));
  c.kind=GUI_BEVEL;c.w=20;c.h=10;assert(gui_damage(&c,1,64,64,&l,&t,&r,&b));
  assert(l==16&&r==36&&t==8&&b==18);
  c.kind=GUI_ICON_RGB;c.x=-8;c.y=60;strcpy(c.text,"MY ATARI");
  assert(gui_damage(&c,1,64,64,&l,&t,&r,&b));assert(l==0&&t==60&&r<=64&&b==64);
  c.x=90;assert(!gui_damage(&c,1,64,64,&l,&t,&r,&b));
  puts("damage bounds OK");
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-I'+str(root/'include'),'-I'+str(root/'system'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 results=[subprocess.run([str(p/'test')]+args).returncode for args in [[],['pixels']]]
 print('Return codes (damage, clipping):',results)
 sys.exit(any(results))
