"""Exercise the actual cursor implementation against a per-pixel oracle."""
import pathlib
import subprocess
import tempfile
import unittest

class CursorWordsTest(unittest.TestCase):
    def test_all_planes_offsets_and_edges(self):
        root = pathlib.Path(__file__).resolve().parents[1]
        source = (root / 'system/kernel_graphics.c').read_text()
        declarations = source[source.index('static volatile uint16_t cursor_mono_background'):source.index('static int clip_rect')]
        declarations = declarations.replace('static volatile uint8_t *desktop_backbuffer;', '').replace('static uint8_t desktop_is_active,desktop_hidden;', '').replace('static uint16_t console_palette[16];', '')
        functions = source[source.index('void kgfx_cursor_hide(void)'):source.index('\nvoid kgfx_cursor_move')]
        harness = '''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef struct {volatile uint16_t *words;uint16_t width,height,line_words;uint8_t planes,chunky;} surface_t;
static struct {uint16_t width,height; } graphics;
static surface_t screen;
static const void *kvideo_front(void){return (const void *)screen.words;}
#define SCREEN_BASE ((uintptr_t)screen.words)
static surface_t active_surface(void){return screen;}
static void mark_dirty(int32_t x,int32_t y,int32_t w,int32_t h){(void)x;(void)y;(void)w;(void)h;}
static int kgame_owns_screen(void){return 0;}
'''
        main = '''
int main(void){
 uint16_t actual[2050],expected[2050],original[2050];
 for(unsigned planes=1;planes<=4;planes*=2){
  graphics.width=64;graphics.height=32;
  screen=(surface_t){actual+1,64,32,4*planes,planes,0};
  desktop_framebuffer=(volatile uint8_t *)screen.words;
  for(int y=0;y<32;y++)for(int x=0;x<64;x++){
   for(unsigned i=0;i<2050;i++)actual[i]=expected[i]=original[i]=(uint16_t)(i*271+x*17+y);
   for(int dy=0;dy<16&&y+dy<32;dy++)for(int dx=0;dx<16&&x+dx<64;dx++)
    if(cursor_shape[dy]&(0x8000u>>dx))for(unsigned p=0;p<planes;p++)
     expected[1+(y+dy)*4*planes+((x+dx)/16)*planes+p]^=(uint16_t)(0x8000u>>((x+dx)&15));
   kgfx_cursor_show(x,y);assert(!memcmp(actual,expected,sizeof actual));
   kgfx_cursor_hide();assert(!memcmp(actual,original,sizeof actual));
  }
 }
 /* Chunky cursor against a pixel oracle, at every clipping size. */
 graphics.width=64;graphics.height=32;
 screen=(surface_t){actual+1,64,32,32,1,1};
 for(int y=0;y<32;y++)for(int x=0;x<64;x++){
  for(unsigned i=0;i<2050;i++)actual[i]=expected[i]=original[i]=(uint16_t)(i*271+x*17+y);
  uint8_t *want=(uint8_t *)(expected+1);
  for(int dy=0;dy<16&&y+dy<32;dy++)for(int dx=0;dx<16&&x+dx<64;dx++){
   int body=!!(cursor_shape[dy]&(0x8000u>>dx)),edge=0;
   const int neighbour[4][2]={{dy-1,dx},{dy+1,dx},{dy,dx-1},{dy,dx+1}};
   for(unsigned n=0;n<4;n++){
    int r=neighbour[n][0],c=neighbour[n][1];
    if(r>=0&&r<16&&c>=0&&c<16&&(cursor_shape[r]&(0x8000u>>c)))edge=1;
   }
   if(body||edge)want[(y+dy)*64+x+dx]=(uint8_t)(body?CURSOR_FILL:CURSOR_OUTLINE);
  }
  kgfx_cursor_show(x,y);assert(!memcmp(actual,expected,sizeof actual));
  kgfx_cursor_hide();assert(!memcmp(actual,original,sizeof actual));
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp); (path/'cursor.c').write_text(harness+declarations+functions+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path/'cursor.c'),'-o',str(path/'cursor')],check=True)
            subprocess.run([str(path/'cursor')],check=True)
