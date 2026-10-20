from pathlib import Path
import subprocess,tempfile,unittest
class WindowControls(unittest.TestCase):
 def test_geometry_focus_and_shortcut(self):
  s=Path('system/kernel_wm.c').read_text()
  code='''#include <assert.h>
#include "wm_core.h"
#include "text_terminal.h"          /* TERM_ROWS */
/* Wycinki ponizej wola console_fit(); jej definicja lezy poza wycinkami, wiec
   dokladamy ja do tej samej jednostki kompilacji (nizej), a tutaj uzupelniamy
   symbole, ktorych ona uzywa: powierzchnia konsoli, stale siatki i dwie
   funkcje pomocnicze (stuby - w tescie geometrii nie wnosza nic wiecej). */
static int console_fit(unsigned i);
static uint8_t *console_surface;
#define CON_GRID_W 640u
#define CON_GRID_H ((uint16_t)(TERM_ROWS*16u))
#define CON_PAD 2u
static void console_follow_cursor(unsigned i,int f){(void)i;(void)f;}
static int32_t sb_range(unsigned i){(void)i;return 0;}
static wm_state_t wm;
static unsigned session,focus,notices;
static int wm_ready=1,wm_force_compose,task_strip_repaint;
static unsigned task_rows[WM_MAX],task_row_count;
static unsigned ksessions_current(void){return session;}
static uint32_t window_owner_pid(unsigned i){return wm.win[i].pid/10;}
static uint32_t kernel_gui_focus_pid(void){return focus;}
static void kernel_gui_focus_set(uint32_t v){focus=v;}
static void notify_geometry(unsigned i){assert(i<wm.count);++notices;}
static int kgame_owns_screen(void){return 0;}
static void window_zoom(unsigned i);
'''+s[s.index('static void task_build_rows(void)\n{'):s.index('\nvoid kernel_wm_set_title')]+s[s.index('static void window_minimize(unsigned i)\n{'):s.index('/* Freestanding kernel:')]+s[s.index('static int console_fit(unsigned i)\n{'):s.index('\n/* "Open terminal"',s.index('static int console_fit(unsigned i)\n{'))]+'''
int main(void){
 wm_init(&wm,800,600);wm.count=2;
 wm.win[0].pid=50;wm.win[0].flags=1;wm.win[0].frame=(wm_rect_t){100,65,600,470};
 wm.win[0].client=(wm_rect_t){100,81,600,454};wm.win[0].content_h=454;
 wm.win[1]=wm.win[0];wm.win[1].pid=60;wm.win[1].frame.x=150;
 focus=5;window_zoom(0);assert(wm.win[1].pid==50&&focus==5&&notices==1);
 assert(wm.win[1].frame.y==16&&wm.win[1].frame.h==584);
 window_minimize(1);assert(!focus&&(wm.win[1].flags&24u)==24u);
 assert(wm.win[0].pid==60&&wm.win[0].frame.x==150); /* no stale index */
 task_build_rows();assert(task_row_count==1);
 session=1;task_build_rows();assert(task_row_count==0);session=0;
 window_restore(1);assert(focus==5&&wm.win[1].frame.h==584&&(wm.win[1].flags&16));
 assert(!kernel_wm_key(1,1,0)); /* Escape remains application input */
 assert(!kernel_wm_key(1,0x1c,0));
 assert(kernel_wm_key(1,0x1c,12));assert(kernel_wm_key(1,0x1c,12));
 window_pending_zoom();assert(wm.win[1].frame.x==100&&wm.win[1].frame.h==470);
 window_pending_zoom();assert(notices==2); /* key repeat doesn't re-toggle */
 assert(kernel_wm_key(2,0x1c,0));assert(!kernel_wm_key(2,0x1c,0));
 /* A real document keeps its extent and clamps scroll after enlargement. */
 wm.win[1].content_h=700;wm.win[1].src_y=246;window_zoom(1);
 assert(wm.win[1].content_h==700&&wm.win[1].src_y==132);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'t.c';p.write_text(code);exe=Path(d)/'t'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-Iinclude',str(p),'system/wm_core.c','-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
