"""Actual overlay code: save-under, redraw after damage and unchanged-frame fast path."""
import pathlib
import subprocess
import tempfile
import unittest

class SessionOverlayTest(unittest.TestCase):
    def test_overlay_pixels_and_restore(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = (root/'system/kernel_graphics.c').read_text()
        overlay = source[source.index('static uint16_t overlay_saved'):]
        scanout = source[source.index('static volatile uint16_t *system_scanout(void)'):source.index('static const uint16_t cursor_shape')]
        harness = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint16_t term_cell_t;
static struct {unsigned mode;} graphics={2};
static uint16_t screen[16002],desktop[16002],original[16002];
#define SCREEN_BASE ((uintptr_t)(screen+1))
static int shown,hide_calls;
static const void *kvideo_front(void){return (const void *)(shown?desktop+1:screen+1);}
static void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h){assert(x==576&&y==0&&w==64&&h>=16&&h<=64);}
static void kgfx_cursor_hide(void){hide_calls++;}
static uint8_t terminal_font[256][16];
static int terminal_font_ready=1;
static uint8_t emutos_system_font[1];
static void term_font_prepare(uint8_t a[256][16],const uint8_t *b){(void)a;(void)b;}
'''
        main = r'''
int main(void){
 term_cell_t cells[32];
 for(unsigned c=0;c<256;c++)for(unsigned y=0;y<16;y++)terminal_font[c][y]=(uint8_t)(c^y);
 for(unsigned i=0;i<32;i++)cells[i]=(term_cell_t)(i+32)|0x100;
 for(shown=0;shown<2;shown++)for(unsigned rows=1;rows<=4;rows++){
  uint16_t *target=shown?desktop:screen;
  for(unsigned i=0;i<16002;i++)target[i]=original[i]=(uint16_t)(i*79);
  kgfx_overlay_show(cells,rows);
  for(unsigned i=0;i<16002;i++){
   int pixel=(int)i-1,y=pixel/40,x=pixel%40;
   if(i&&i<=16000&&y<(int)rows*16&&x>=36){
    unsigned cell=(unsigned)(y/16)*8+(unsigned)(x-36)*2;
    uint16_t expected=(uint16_t)((terminal_font[cells[cell]&255][y%16]<<8)|terminal_font[cells[cell+1]&255][y%16])^0xffff;
    assert(target[i]==expected);
   }else assert(target[i]==original[i]);
  }
  int calls=hide_calls;kgfx_overlay_show(cells,rows);assert(hide_calls==calls);
  kgfx_overlay_hide();assert(!memcmp(target,original,sizeof original));
  target[37]=original[37]=0x1234;
  kgfx_overlay_show(cells,rows);kgfx_overlay_hide();assert(!memcmp(target,original,sizeof original));
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp)
            (path/'overlay.c').write_text(harness+scanout+overlay+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path/'overlay.c'),'-o',str(path/'overlay')],check=True)
            subprocess.run([str(path/'overlay')],check=True)
