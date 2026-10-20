"""Execute production view/present code with real rasterizer and guarded bitmap."""
from pathlib import Path
import subprocess
import tempfile
import unittest

class SessionRedrawTest(unittest.TestCase):
    def test_retained_frames_and_exit(self):
        root=Path(__file__).resolve().parents[1]
        source=(root/'system/kernel_graphics.c').read_text()
        view=source[source.index('void kgfx_session_view('):source.index('const kgfx_service_t *kgfx_get_service(')]
        present=source[source.index('int kgfx_terminal_submit('):source.index('int kgfx_console_present(')]
        harness=r'''
#include <assert.h>
#include <string.h>
#include "text_terminal.h"
#define SESSION_COUNT 3
static unsigned current,presented_session,invalidations;
static int boot_hold;
static int kconsole_boot_active(void){return boot_hold;}
static unsigned ksessions_current(void){return current;}
static term_cell_t session_cells[SESSION_COUNT][TERM_CELLS];
static uint8_t session_cells_valid[SESSION_COUNT];
static term_display_t terminal_display;
static uint8_t terminal_font[256][16],emutos_system_font[256][16],terminal_font_ready=1;
static uint16_t pixels[16002],expected[16000];
#define SCREEN_BASE ((uintptr_t)(pixels+1))
static volatile uint8_t *desktop_framebuffer=(volatile uint8_t *)(pixels+1);
static struct {unsigned planes,width,height;volatile uint8_t *framebuffer;} graphics={1,640,400,(volatile uint8_t *)(pixels+1)};
static uint8_t desktop_is_active=1,desktop_hidden;
static int16_t cursor_x,cursor_y;
static int kgfx_desktop_active(void){return desktop_is_active&&!desktop_hidden;}
static void kgfx_terminal_invalidate(void){term_display_invalidate(&terminal_display);}
static void kconsole_invalidate(void){invalidations++;}
static void kgfx_overlay_hide(void){}
static void kgfx_cursor_hide(void){}
static int kgfx_cursor_visible(void){return 0;}
static void kgfx_cursor_show(int x,int y){(void)x;(void)y;}
static int kvideo_is_vofa(void){return 0;}
static void kvideo_select(const volatile uint8_t *p){assert(p==(void *)(pixels+1));}
static void terminal_video_damage(const term_display_t *d,const term_cell_t *c){(void)d;(void)c;}
static void kernel_ikbd_poll_capture(void){}
int kgfx_terminal_present(const term_cell_t *cells);
'''
        main=r'''
int main(void){
 text_terminal_t a={0},b={0};
 for(unsigned c=0;c<256;c++)for(unsigned y=0;y<16;y++)terminal_font[c][y]=(uint8_t)(c*19+y*13);
 pixels[0]=pixels[16001]=0xbeef;
 term_clear(&a,0);term_text(&a,4,7,"COMMANDER A",1,11);
 term_clear(&b,0);term_text(&b,21,15,"COMMANDER B",0,11);
 assert(kgfx_terminal_present(a.cells)==1000);
 memcpy(expected,pixels+1,sizeof expected);
 boot_hold=1;kgfx_session_view(0);assert(!desktop_hidden);
 boot_hold=0;current=1;kgfx_session_view(0);
 memset(pixels+1,0,sizeof expected); /* shell owns visible pixels */
 current=0;kgfx_session_view(1);
 assert(!memcmp(expected,pixels+1,sizeof expected));
 assert(kgfx_terminal_present(a.cells)==0); /* restored cache is coherent */
 assert(kgfx_terminal_submit(1,b.cells)==0);
 assert(!memcmp(expected,pixels+1,sizeof expected)); /* background cannot paint */
 current=1;kgfx_session_view(1);assert(kgfx_terminal_present(b.cells)==0);
 current=0;kgfx_session_view(1);
 assert(!memcmp(expected,pixels+1,sizeof expected));
 current=1;kgfx_session_view(1);kgfx_terminal_present(b.cells);
 for(unsigned n=0;n<100;n++){
  current=0;kgfx_session_view(1);assert(!memcmp(expected,pixels+1,sizeof expected));
  current=1;kgfx_session_view(1);assert(kgfx_terminal_present(b.cells)==0);
 }
 desktop_is_active=0;kgfx_session_view(0); /* app B exits */
 memset(pixels+1,0,sizeof expected);
 current=0;kgfx_session_view(1);
 assert(kgfx_desktop_active()&&!memcmp(expected,pixels+1,sizeof expected));
 unsigned before=invalidations;kgfx_session_view(1);assert(before==invalidations);
 assert(pixels[0]==0xbeef&&pixels[16001]==0xbeef);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'test.c').write_text(harness+view+present+main)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),str(p/'test.c'),str(root/'system/text_terminal.c'),'-o',str(p/'test')],check=True)
            subprocess.run([str(p/'test')],check=True)
