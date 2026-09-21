#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/kernel_sessions.c"
static unsigned starts,selects,selected,resets,invalidates,paints,restored,graphic_session=99;
static int busy,pending_keys;
static term_cell_t painted[32];
static unsigned painted_rows;
static int select_shell(uint32_t n,uint32_t reset){selects++;selected=n;resets=reset;return 1;}
static void start_shell(void){starts++;}
int kernel_user_session_busy(unsigned n){(void)n;return busy;}
int kernel_queued_session_busy(unsigned n){(void)n;return 0;}
int kernel_input_pending(void){return pending_keys;}
int kernel_user_desktop_visible(void){return current==graphic_session;}
int kernel_user_gui_keyboard_active(void){return current==graphic_session;}
void kernel_user_terminal_reset_modifiers(void){restored++;}
int kernel_raw_mouse_event(uint32_t *b,int32_t *x,int32_t *y){(void)b;(void)x;(void)y;return 0;}
void kconsole_select(unsigned n,int reset){assert(n==selected);assert((unsigned)reset==resets);invalidates++;}
void kgfx_session_view(int show){assert(show==(current==graphic_session));}
void kgfx_cursor_hide(void){}
void kgfx_cursor_move(int32_t x,int32_t y){assert(x>=0&&x<640&&y>=0&&y<400);}
void kgfx_overlay_hide(void){}
void kgfx_overlay_show(const term_cell_t *cells,unsigned rows){paints++;painted_rows=rows;memcpy(painted,cells,rows*8*sizeof(*cells));}
static void click(int x,int y){handle_mouse(20,x-mouse_x,y-mouse_y);ksessions_pump();}
int main(void){
 kernel_shell_service_t svc={0};uint32_t b;int32_t x,y;
 svc.select_session=select_shell;svc.start=start_shell;
 ksessions_init(&svc,1);assert(ksessions_control(0,0)==1&&mask==1);
 ksessions_paint();assert(painted_rows==1&&(painted[1]&255)=='1');
 /* Menu creates tts2. Its opening click must never reach the application. */
 graphic_session=0;click(610,8);assert(menu&&!forward_ready);
 ksessions_paint();assert(painted_rows==2&&(painted[9]&255)=='+');
 click(610,24);assert(!menu&&current==1&&mask==3&&starts==1&&resets==1);
 /* Switching preserves existing shell state and waits for older key events. */
 pending_keys=1;assert(!ksessions_control(1,0));ksessions_pump();assert(current==1);
 pending_keys=0;ksessions_pump();assert(current==0&&resets==0&&starts==1);
 assert(!ksessions_key(1,0x44));assert(!request); /* plain F10 */
 ksessions_key(1,0x38);ksessions_key(1,0x36);assert(ksessions_key(1,0x44));
 ksessions_pump();assert(current==1);assert(ksessions_key(1,0x44));ksessions_pump();assert(current==1);
 assert(ksessions_key(2,0x44));ksessions_key(2,0x36);ksessions_key(2,0x38);
 /* Both Shift keys work; cycling only lists active terminals. */
 ksessions_key(1,0x38);ksessions_key(1,0x2a);assert(ksessions_key(1,0x44));
 ksessions_pump();assert(current==0);ksessions_key(2,0x44);ksessions_key(2,0x2a);ksessions_key(2,0x38);
 click(610,8);assert(menu);assert(ksessions_key(1,1));assert(!menu);assert(ksessions_key(2,1));
 click(610,8);click(30,100);assert(!menu&&!forward_ready&&current==0);
 /* tts3 appears only after activation, then the menu contains no New row. */
 ksessions_control(3,0);ksessions_pump();assert(current==2&&mask==7);
 click(610,8);ksessions_paint();assert(painted_rows==3&&(painted[9]&255)=='1'&&(painted[17]&255)=='2');
 click(610,40);assert(current==1);
 busy=1;assert(ksessions_control(4,0)<0);busy=0;
 assert(!ksessions_control(4,0));ksessions_pump();assert(current==0&&mask==5);
 assert(ksessions_control(4,0)<0); /* never close primary shell */
 ksessions_control(2,0);ksessions_pump();assert(current==1&&resets==1);
 /* Desktop clicks pass through only in the owning terminal. */
 graphic_session=1;click(200,100);assert(ksessions_mouse_event(&b,&x,&y));assert(b&16);
 assert(!ksessions_mouse_event(&b,&x,&y));
 ksessions_control(1,0);ksessions_pump();click(200,100);assert(!forward_ready);
 puts("Sessions PASS: active-only menu, create/close, busy guard, deferred switching, both Shifts, autorepeat, Escape, outside click and GUI input isolation");
 return 0;
}
