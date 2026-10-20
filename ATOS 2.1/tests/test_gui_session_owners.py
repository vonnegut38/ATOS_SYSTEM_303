"""GUI capture/release targets the IPC client's session, including background exits."""
from pathlib import Path
import subprocess,tempfile,unittest
class GuiOwners(unittest.TestCase):
 def test_background_lifecycle(self):
  s=Path('system/kernel_exception.c').read_text()
  declarations=s[s.index('static uint32_t gui_keyboard_pids'):s.index('static int32_t gui_mouse_x')]
  release=s[s.index('static int user_gui_terminal_allowed('):s.index('typedef struct {uint32_t pid;int32_t status;uint8_t terminal;}')]
  dispatch=s[s.index('    if(call==45u||call==47u)'):s.index('    user_syscall_result=-0x2ff;')]
  code=r'''
#include <stdint.h>
#include <assert.h>
#define SESSION_COUNT 3
#define USER_PROCESS_SLOTS 3
#define USER_PROCESS_FREE 0
#define USER_PROCESS_TERMINATED 5
static unsigned active,restores,hides,flushes;
static unsigned ksessions_current(void){return active;}
static void kgfx_restore_console(void){restores++;}
static void kgfx_cursor_hide(void){hides++;}
static uint32_t gui_desktop_pids[3],terminal_owners[3];
static struct {uint32_t pid,state,parent_pid;unsigned terminal;} process_table[3],broker,*current_process=&broker;
static int32_t user_syscall_result;
static uint32_t kernel_user_service_pid(const char *s){(void)s;return 2;}
static int32_t kernel_user_yield(void){return 100;}
static void kernel_user_mouse_flush(void){flushes++;}
'''+declarations+release+r'''
static int dispatch(uint32_t call,uint32_t argument0){unsigned i;
'''+dispatch+r'''
return -1;}
int main(void){
 broker.pid=2;process_table[0].pid=7;process_table[0].state=1;process_table[0].terminal=1;
 gui_keyboard_pid=8;gui_keyboard_persistent=1;gui_keyboard_ready_tick=99;gui_keyboard_dropped=3;gui_desktop_pids[0]=8;
 dispatch(47,7);assert(!user_syscall_result&&gui_keyboard_pids[1]==7&&gui_persistent[1]&&gui_ready_ticks[1]==105&&!flushes);
 assert(gui_keyboard_pid==8&&gui_keyboard_persistent&&gui_keyboard_ready_tick==99&&gui_keyboard_dropped==3);
 gui_desktop_pids[1]=7;dispatch(48,7);
 assert(!gui_keyboard_pids[1]&&!gui_persistent[1]&&!gui_desktop_pids[1]);
 assert(gui_keyboard_pid==8&&gui_keyboard_persistent&&gui_desktop_pids[0]==8&&!restores&&!hides);
 dispatch(48,7);dispatch(48,999);assert(gui_keyboard_pid==8&&!restores);
 dispatch(47,7);assert(gui_keyboard_pids[1]==7);
 active=1;assert(gui_keyboard_pid==7&&gui_keyboard_persistent);
 gui_desktop_pids[1]=7;user_gui_release_session(1,7);assert(restores==1&&hides==1&&!gui_keyboard_pid);
 active=0;assert(gui_keyboard_pid==8&&gui_keyboard_persistent&&gui_keyboard_ready_tick==99);
 terminal_owners[1]=7;assert(user_gui_terminal_allowed(1,7));
 process_table[1].pid=9;process_table[1].state=1;process_table[1].terminal=1;process_table[1].parent_pid=7;
 assert(user_gui_terminal_allowed(1,9));
 process_table[1].parent_pid=0;assert(!user_gui_terminal_allowed(1,9));
 process_table[1].parent_pid=9;assert(!user_gui_terminal_allowed(1,9));
 process_table[1].parent_pid=7;process_table[1].terminal=2;assert(!user_gui_terminal_allowed(1,9));
 dispatch(47,7);assert(!user_syscall_result&&gui_keyboard_pids[1]==7);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
