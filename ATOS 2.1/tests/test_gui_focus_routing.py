"""Focus manager: keys go to the focused app, the mouse to the window owner."""
from pathlib import Path
import subprocess,tempfile,unittest
class GuiFocusRouting(unittest.TestCase):
 def test_keys_follow_focus_mouse_follows_window(self):
  s=Path('system/kernel_exception.c').read_text();s=s[s.index('int kernel_user_gui_keyboard_wake('):s.index('int kernel_user_terminal_wake(')]
  code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#define USER_PROCESS_SLOTS 2
#define USER_PROCESS_FREE 0
#define USER_PROCESS_TERMINATED 5
#define USER_PROCESS_MESSAGE 8
#define USER_PROCESS_READY 1
#define USER_MAILBOX_DEPTH 16
#define USER_MAILBOX_INPUT_DEPTH 12
typedef struct {uint32_t sender,value[4];} user_message_t;
typedef struct {uint32_t pid,state,saved_registers[15];user_message_t *wait_message_pointer,mailbox[16];uint8_t mailbox_count,mailbox_tail;} user_process_t;
typedef struct {unsigned width,height;} kgfx_info_t;
static user_process_t process_table[2];
static unsigned kdbg_wake_calls,kdbg_wake_keys;
static uint32_t gui_keyboard_pid=7,gui_keyboard_ready_tick,gui_keyboard_dropped;
/* The wake path is session-aware; these stubs are the slice's own view of the
   session layer (Sender: the routing fix that keeps a click capture from
   leaking into another desktop). */
#define SESSION_COUNT 3
static uint32_t kdbg_mouse_routed[SESSION_COUNT];
static unsigned ksessions_current(void){return 0;}
static unsigned kernel_user_pid_terminal(uint32_t pid){(void)pid;return 0;}
static int gui_keyboard_persistent=1,gui_mouse_x,gui_mouse_y;
static unsigned mouse_reads,key_reads;
static void kgfx_cursor_hide(void){}
static void kgfx_cursor_move(int x,int y){(void)x;(void)y;}
static int32_t kernel_user_yield(void){return 100;}
static int32_t kernel_user_terminal_getchar(void){return -1;}
static void kernel_user_mouse_flush(void){}
static unsigned mouse_buttons_return,mouse_event_pending,mouse_consumes;
static uint32_t mouse_tick_return;
static int ksessions_mouse_peek(uint32_t *b,int32_t *x,int32_t *y,uint32_t *t){mouse_reads++;if(!mouse_event_pending)return 0;*b=mouse_buttons_return;*x=100;*y=100;if(t)*t=mouse_tick_return;return 1;}
static void ksessions_mouse_consume(void){mouse_consumes++;}
static const kgfx_info_t *kgfx_get_info(void){static const kgfx_info_t g={640,400};return &g;}
static int ksessions_mouse_x(void){return 400;}
static int ksessions_mouse_y(void){return 120;}
static int kernel_wm_dragging(void){return 0;}
/* Focus manager under test. */
static uint32_t pid_at_return,focus_return;
static uint32_t kernel_wm_pid_at(int32_t x,int32_t y){(void)x;(void)y;return pid_at_return;}
static int chrome_return;
static int kernel_wm_chrome_at(int32_t x,int32_t y){(void)x;(void)y;return chrome_return;}
static int game_return;
static int kgame_owns_screen(void){return game_return;}
static uint32_t kgame_owner(void){return 9u;}
static uint32_t kernel_gui_focus_pid(void){return focus_return;}
static unsigned key_event_pending;
static int kernel_user_keyboard_event(uint32_t *t,uint32_t *s,uint32_t *m,uint32_t *c){(void)t;(void)s;(void)m;(void)c;key_reads++;return key_event_pending;}
'''+s+r'''
static void reset(void){unsigned i;
 for(i=0;i<2;i++){process_table[i].pid=0;process_table[i].state=USER_PROCESS_MESSAGE;
  process_table[i].wait_message_pointer=0;process_table[i].mailbox_count=0;process_table[i].mailbox_tail=0;
  memset(process_table[i].mailbox,0,sizeof process_table[i].mailbox);}
 process_table[0].pid=7;process_table[1].pid=9;
 mouse_reads=key_reads=0;pid_at_return=focus_return=0;key_event_pending=0;}
static unsigned got(unsigned slot){return process_table[slot].mailbox_count;}
int main(void){
 /* Mouse: the window under the pointer owns the event, not the keyboard owner. */
 reset();mouse_event_pending=1;mouse_buttons_return=0x16u;pid_at_return=9;
 assert(kernel_user_gui_keyboard_wake());
 assert(got(1)==1&&got(0)==0);
 assert(process_table[1].mailbox[0].value[0]==0x4b4d4f55u&&((process_table[1].mailbox[0].value[2]>>24)&31u)==0x16u);
 /* No application window under the pointer: the session owner keeps the mouse. */
 reset();mouse_event_pending=1;mouse_buttons_return=0x16u;pid_at_return=0;
 assert(kernel_user_gui_keyboard_wake());
 assert(got(0)==1&&got(1)==0);
 /* Keys follow the focused application window ... */
 reset();mouse_event_pending=0;key_event_pending=1;focus_return=9;pid_at_return=0;
 assert(kernel_user_gui_keyboard_wake());
 assert(got(1)==1&&got(0)==0);
 assert(process_table[1].mailbox[0].value[0]==0x4b4b4559u);
 /* ... and fall back to the owner when no application window is focused. */
 reset();mouse_event_pending=0;key_event_pending=1;focus_return=0;
 assert(kernel_user_gui_keyboard_wake());
 assert(got(0)==1&&got(1)==0);
 /* A dead focused process must never swallow the key. */
 reset();mouse_event_pending=0;key_event_pending=1;focus_return=9;
 process_table[1].state=USER_PROCESS_TERMINATED;
 assert(kernel_user_gui_keyboard_wake());
 assert(got(0)==1&&got(1)==0);
 /* Click capture: the press target owns the whole click. */
 reset();mouse_event_pending=1;mouse_buttons_return=1u;pid_at_return=9;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==1&&got(0)==0);
 /* Pointer leaves the window while held: the app still owns the motion. */
 mouse_buttons_return=1u;pid_at_return=0;chrome_return=1;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==2&&got(0)==0);
 /* Release over the owner: still the app, and the release is appended. */
 mouse_buttons_return=0u;pid_at_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==3&&got(0)==0);
 /* Latch gone: plain motion follows the pointer again. */
 mouse_buttons_return=0u;pid_at_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(0)==1&&got(1)==3);
 /* WM gesture: no app receives a resize/scrollbar press or its release,
    even when the pointer leaves chrome or the app mailbox is full. */
 reset();mouse_event_pending=1;mouse_buttons_return=1;pid_at_return=9;chrome_return=1;
 process_table[1].mailbox_count=USER_MAILBOX_INPUT_DEPTH;
 unsigned consumed=mouse_consumes;
 assert(kernel_user_gui_keyboard_wake());assert(mouse_consumes==consumed+1);
 assert(got(1)==USER_MAILBOX_INPUT_DEPTH&&got(0)==0);
 process_table[1].mailbox_count=0;chrome_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==0&&got(0)==0);
 mouse_buttons_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==0&&got(0)==0);
 /* Next ordinary client click still works. */
 mouse_buttons_return=1;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==1&&got(0)==0);
 mouse_buttons_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==2&&got(0)==0);
 /* Exclusive game mode bypasses WM chrome entirely. */
 reset();game_return=1;chrome_return=1;mouse_event_pending=1;mouse_buttons_return=1;pid_at_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==1&&got(0)==0);
 mouse_buttons_return=0;
 assert(kernel_user_gui_keyboard_wake());assert(got(1)==2&&got(0)==0);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'test.c';p.write_text(code);exe=Path(d)/'test'
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
