"""Exercise the actual supervisor input routing for foreground GUI and text jobs."""
from pathlib import Path
import subprocess,tempfile,unittest
class ForegroundInput(unittest.TestCase):
 def test_foreground_gui_does_not_lose_keys_to_console(self):
  s=Path('system/kernel_main.c').read_text();a=s.index('                if(kernel_user_gui_keyboard_active()){');b=s.index('\n                ksessions_pump();',a)
  code=r'''
#include <assert.h>
#include <stdint.h>
static unsigned gui,terminal,overlay,focused=1,gcalls,tcalls,scalls,prompts,wake_result;
static unsigned kdbg_route_gui,kdbg_route_shell,batch_limit=1,pending;
static void ksessions_pump(void){}
static int kernel_user_gui_keyboard_active(void){return gui;}
static int kernel_user_terminal_active(void){return terminal;}
static int kernel_wm_console_visible(void){return overlay;}
static int kernel_wm_console_focused(void){return focused;}
static int kernel_user_gui_keyboard_wake(void){gcalls++;if(pending){--pending;return 1;}return 0;}
static unsigned kernel_user_terminal_wake(void){tcalls++;return wake_result;}
static int kernel_user_console_dirty(void){return 1;}
static void pump(unsigned unused){assert(!unused);scalls++;}
static void redraw(void){prompts++;}
struct shell {void (*pump)(unsigned);void (*redraw_prompt)(void);};
static void route(void){uint32_t terminal_result;struct shell api={pump,redraw},*shell=&api;
 for(unsigned i=0;i<batch_limit;++i){
'''+s[a:b]+r'''
 }}
int main(void){gui=terminal=1;route();assert(gcalls==1&&!tcalls&&!scalls);
 gui=0;route();assert(tcalls==1&&!scalls);
 wake_result=2;route();assert(prompts==1&&tcalls==2);
 terminal=0;route();assert(scalls==1);
 gui=terminal=overlay=1;route();assert(scalls==2&&gcalls==1&&tcalls==2);
 focused=0;route();assert(gcalls==2&&scalls==2);   /* click on the desktop: back to the GUI */
 /* Idle queues stop immediately; a burst is drained before stopping. */
 overlay=0;batch_limit=32;unsigned before=gcalls;
 route();assert(gcalls==before+1);
 pending=3;before=gcalls;route();assert(!pending&&gcalls==before+4);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'test.c';p.write_text(code);exe=Path(d)/'test'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
