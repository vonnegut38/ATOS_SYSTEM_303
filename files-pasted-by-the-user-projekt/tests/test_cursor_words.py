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
        declarations = declarations.replace('static volatile uint8_t *desktop_framebuffer;', '').replace('static volatile uint8_t *desktop_backbuffer;', '').replace('static uint8_t desktop_is_active;', '').replace('static uint16_t console_palette[16];', '')
        functions = source[source.index('void kgfx_cursor_hide(void)'):source.index('\nvoid kgfx_cursor_move')]
        harness = '''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef struct {volatile uint16_t *words;uint16_t width,height,line_words;uint8_t planes;} surface_t;
static struct {uint16_t width,height; } graphics;
static surface_t screen;
static surface_t active_surface(void){return screen;}
static void mark_dirty(int32_t x,int32_t y,int32_t w,int32_t h){(void)x;(void)y;(void)w;(void)h;}
'''
        main = '''
int main(void){
 uint16_t actual[2050],expected[2050],original[2050];
 for(unsigned planes=1;planes<=4;planes*=2){
  graphics.width=64;graphics.height=32;
  screen=(surface_t){actual+1,64,32,4*planes,planes};
  for(int y=0;y<32;y++)for(int x=0;x<64;x++){
   for(unsigned i=0;i<2050;i++)actual[i]=expected[i]=original[i]=(uint16_t)(i*271+x*17+y);
   for(int dy=0;dy<16&&y+dy<32;dy++)for(int dx=0;dx<16&&x+dx<64;dx++)
    if(cursor_shape[dy]&(0x8000u>>dx))for(unsigned p=0;p<planes;p++)
     expected[1+(y+dy)*4*planes+((x+dx)/16)*planes+p]^=(uint16_t)(0x8000u>>((x+dx)&15));
   kgfx_cursor_show(x,y);assert(!memcmp(actual,expected,sizeof actual));
   kgfx_cursor_hide();assert(!memcmp(actual,original,sizeof actual));
  }
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            path=pathlib.Path(temp); (path/'cursor.c').write_text(harness+declarations+functions+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path/'cursor.c'),'-o',str(path/'cursor')],check=True)
            subprocess.run([str(path/'cursor')],check=True)
