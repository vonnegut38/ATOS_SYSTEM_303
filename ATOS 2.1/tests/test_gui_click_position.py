"""A full app mailbox must not consume input or relocate a queued click."""
from pathlib import Path
import subprocess,tempfile,unittest
class GuiClickPosition(unittest.TestCase):
 def test_press_position_survives_motion(self):
  s=Path('system/kernel_exception.c').read_text();s=s[s.index('int kernel_user_gui_keyboard_wake('):s.index('int kernel_user_terminal_wake(')]
  code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#define USER_PROCESS_SLOTS 1
#define USER_PROCESS_FREE 0
#define USER_PROCESS_TERMINATED 5
#define USER_PROCESS_MESSAGE 8
#define USER_PROCESS_READY 1
#define USER_MAILBOX_DEPTH 16
#define USER_MAILBOX_INPUT_DEPTH 12
typedef struct {uint32_t sender,value[4];} user_message_t;
typedef struct {uint32_t pid,state,saved_registers[15];user_message_t *wait_message_pointer,mailbox[16];uint8_t mailbox_count,mailbox_tail;} user_process_t;
typedef struct {unsigned width,height;} kgfx_info_t;
static user_process_t process_table[1];
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
static unsigned mouse_buttons_return,mouse_consumes;
static uint32_t mouse_tick_return;
static int event_x=321,event_y=123;
/* The EVENT carries its own position, deliberately different from the LIVE
   cursor (ksessions_mouse_x/y = 600,390) so a queued click cannot be
   relabelled with a newer pointer position. */
static int ksessions_mouse_peek(uint32_t *b,int32_t *x,int32_t *y,uint32_t *t){mouse_reads++;*b=mouse_buttons_return;*x=event_x;*y=event_y;if(t)*t=mouse_tick_return;return 1;}
static void ksessions_mouse_consume(void){mouse_consumes++;}
static const kgfx_info_t *kgfx_get_info(void){static const kgfx_info_t g={640,400};return &g;}
static int ksessions_mouse_x(void){return 600;}
static int ksessions_mouse_y(void){return 390;}
static int kernel_wm_dragging(void){return 0;}
/* Focus manager: no application window under the pointer and no focused app,
   so the session keyboard owner stays the recipient (the pre-focus behaviour
   this test pins down). */
static uint32_t kernel_wm_pid_at(int32_t x,int32_t y){(void)x;(void)y;return 0u;}
static int chrome_return;
static int kernel_wm_chrome_at(int32_t x,int32_t y){(void)x;(void)y;return chrome_return;}
static int kgame_owns_screen(void){return 0;}
static uint32_t kgame_owner(void){return 0u;}
static uint32_t kernel_gui_focus_pid(void){return 0u;}
static int kernel_user_keyboard_event(uint32_t *t,uint32_t *s,uint32_t *m,uint32_t *c){(void)t;(void)s;(void)m;(void)c;key_reads++;return 0;}
'''+s+r'''
int main(void){user_process_t *p=process_table;
 p->pid=7;p->state=USER_PROCESS_READY;
 mouse_buttons_return=1;event_x=200;event_y=168;
 assert(kernel_user_gui_keyboard_wake());
 /* App is painting: it has not received the press yet. */
 event_x=600;event_y=466;
 assert(kernel_user_gui_keyboard_wake());
 /* HARDWARE press must stay HARDWARE even when motion reaches QUIT. */
 assert(p->mailbox[0].value[1]==((200u<<16)|168));
 assert(p->mailbox_count==2);
 event_x=610;mouse_tick_return=3;
 assert(kernel_user_gui_keyboard_wake());
 assert(p->mailbox_count==2); /* still coalesce actual motion */
 assert(p->mailbox[0].value[1]==((200u<<16)|168));
 assert(p->mailbox[1].value[1]==((610u<<16)|466));
 mouse_buttons_return=0;event_x=620;
 assert(kernel_user_gui_keyboard_wake());
 assert(p->mailbox_count==3);
 event_x=630;
 assert(kernel_user_gui_keyboard_wake());
 assert(p->mailbox_count==4);
 assert(p->mailbox[2].value[1]==((620u<<16)|466)); /* release stays put */
 event_x=640;
 assert(kernel_user_gui_keyboard_wake());
 assert(p->mailbox_count==4);
 assert(p->mailbox[3].value[1]==((640u<<16)|466));
 return 0;}

'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(code)
   subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
