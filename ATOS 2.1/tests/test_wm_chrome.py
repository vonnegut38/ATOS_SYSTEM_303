"""Clicks on resize controls and scrollbars never belong to the client."""
from pathlib import Path
import subprocess,tempfile,unittest
class ChromeHit(unittest.TestCase):
 def test_actual_window_geometry(self):
  src=Path('system/kernel_wm.c').read_text()
  start=src.index('static int sb_visible(unsigned w)\n{')
  end=src.index('/* Handle a click in window w',start)
  code='''#include <assert.h>
#include "wm_core.h"
static wm_state_t wm;
static int wm_ready=1;
static int task_chrome_at(int32_t x,int32_t y){(void)x;(void)y;return 0;}
#define WM_SB_W 16u
'''+src[start:end]+'''
int main(void){
 wm.screen_w=800;wm.screen_h=600;wm.count=1;
 wm.win[0].flags=1;
 wm.win[0].frame=(wm_rect_t){140,120,520,360};
 wm.win[0].client=(wm_rect_t){140,136,520,344};
 wm.win[0].content_h=344;
 assert(!kernel_wm_chrome_at(50,255));
 assert(!kernel_wm_chrome_at(350,250));
 assert(!kernel_wm_chrome_at(600,466)); /* app's QUIT */
 assert(kernel_wm_chrome_at(645,467)); /* same button, but under the sizer */
 assert(kernel_wm_chrome_at(652,128)); /* close */
 assert(kernel_wm_chrome_at(250,128)); /* title */
 assert(kernel_wm_chrome_at(141,200)); /* left resize edge */
 assert(!kernel_wm_chrome_at(648,250));
 wm.win[0].content_h=600;
 assert(kernel_wm_chrome_at(648,250)); /* scrollbar */
 assert(kernel_wm_chrome_at(648,466)); /* down arrow */
 assert(!kernel_wm_chrome_at(630,250));
 assert(!kernel_wm_chrome_at(-1,128));
 assert(!kernel_wm_chrome_at(900,128));
 wm.win[0].flags|=8;wm.win[0].flags&=~1u;
 assert(!kernel_wm_chrome_at(645,467)); /* hidden */
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'t.c';p.write_text(code);exe=Path(d)/'t'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-Iinclude',str(p),'system/wm_core.c','-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
