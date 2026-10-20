#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/kernel_sessions.c"
static unsigned starts,selects,selected,resets,invalidates,paints,restored,bar_refreshes,graphic_session=99;
static int desktop_active;
static int busy,pending_keys;
static unsigned raw_reads,raw_available;
static uint32_t raw_buttons;
static int32_t raw_x,raw_y;
static term_cell_t painted[32];
static unsigned painted_rows;
static int select_shell(uint32_t n,uint32_t reset){selects++;selected=n;resets=reset;return 1;}
static void start_shell(void){starts++;}
int kernel_user_session_busy(unsigned n){(void)n;return busy;}
int kernel_queued_session_busy(unsigned n){(void)n;return 0;}
int kernel_input_pending(void){return pending_keys;}
int kernel_user_desktop_visible(void){return current==graphic_session;}
int kernel_user_gui_keyboard_active(void){return current==graphic_session;}
int kgfx_desktop_active(void){return desktop_active;}
int kconsole_boot_active(void){return 0;}
/* Geometry the pointer clamp follows; the test switches it to 800x600. */
static kgfx_info_t test_info;
const kgfx_info_t *kgfx_get_info(void){return &test_info;}
int kernel_wm_key(uint32_t t,uint32_t s,uint32_t m){(void)t;(void)s;(void)m;return 0;}
int kernel_wm_enabled(void){return desktop_active;}
void kernel_wm_refresh_bar(void){bar_refreshes++;}
void kernel_user_terminal_reset_modifiers(void){restored++;}
/* Count CONSUMED events, not polls: the pump now drains the whole queue, so it
   makes one extra call that legitimately finds nothing. */
int kernel_raw_mouse_event(uint32_t *b,int32_t *x,int32_t *y){if(!raw_available)return 0;raw_reads++;raw_available=0;*b=raw_buttons;*x=raw_x;*y=raw_y;return 1;}
void kconsole_select(unsigned n,int reset){assert(n==selected);assert((unsigned)reset==resets);invalidates++;}
void kgfx_session_view(int show){assert(show==(current==graphic_session));}
/* Each queued interaction event carries a hires timestamp (same unit as the
   apps' hires()), used to time a double click from the edge. */
static uint32_t test_hires;
uint32_t kernel_user_hires_clock(void){return test_hires+=123u;}
void kgfx_cursor_hide(void){}
void kgfx_cursor_move(int32_t x,int32_t y){assert(x>=0&&x<(int)test_info.width&&y>=0&&y<(int)test_info.height);}
void kgfx_overlay_hide(void){}
void kgfx_overlay_show(const term_cell_t *cells,unsigned rows){paints++;painted_rows=rows;memcpy(painted,cells,rows*8*sizeof(*cells));}
static void click(int x,int y){handle_mouse(20,x-mouse_x,y-mouse_y);ksessions_pump();}
/* The session layer no longer keeps one merged "forward" state: every button
   edge is queued in order.  "An event is pending for the app" is therefore
   "the interaction queue is not empty". */
static int forward_ready(void){uint32_t b;int32_t x,y;return ksessions_mouse_peek(&b,&x,&y,0);}
int main(void){
 kernel_shell_service_t svc={0};uint32_t b;int32_t x,y;
 test_info.width=640;test_info.height=400;
 svc.select_session=select_shell;svc.start=start_shell;
 ksessions_init(&svc,1);assert(ksessions_control(0,0)==1&&mask==1);
 ksessions_paint();assert(painted_rows==1&&(painted[1]&255)=='1');
 /* Menu creates tts2. Its opening click must never reach the application. */
 graphic_session=0;click(610,8);assert(menu&&!forward_ready());
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
 click(610,8);click(30,100);assert(!menu&&!forward_ready()&&current==0);
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
 ksessions_control(1,0);ksessions_pump();click(200,100);assert(!forward_ready());
 /* Raw IKBD packets always update the session state.  A pending forward only
    defers delivery to the app; skipping the packet froze the pointer and lost
    the button release (drag outline stuck on screen), so this is the contract. */
 graphic_session=0;handle_mouse(22,100-mouse_x,100-mouse_y);
 assert(forward_ready());unsigned before=raw_reads;
 raw_available=1;raw_buttons=2;raw_x=500;raw_y=290;
 ksessions_pump();assert(raw_reads==before+1&&mouse_x==600&&mouse_y==390);
 assert(ksessions_mouse_event(&b,&x,&y)&&b==22);
 ksessions_pump();assert(mouse_x==600&&mouse_y==390);
 /* Held button is quarantined across the session boundary until release. */
 ksessions_control(2,0);ksessions_pump();graphic_session=1;
 handle_mouse(2,0,0);assert(!forward_ready()&&mouse_capture);
 handle_mouse(0,0,0);assert(!forward_ready()&&!mouse_capture);
 handle_mouse(0,0,0);assert(ksessions_mouse_event(&b,&x,&y)&&b==0);
 /* Graphics mode: the text '[1]' marker is replaced by a top-bar badge, and a
    session switch repaints only that badge strip, not the whole desktop. */
 desktop_active=1;menu=0;
 {unsigned p0=paints;ksessions_paint();assert(paints==p0);} /* overlay suppressed */
 assert(current==1);
 ksessions_control(1,0);{unsigned b0=bar_refreshes;ksessions_pump();assert(bar_refreshes==b0+1);}
 assert(current==0);
 handle_mouse(2,540-mouse_x,8-mouse_y);assert(menu); /* badge region opens the menu */
 handle_mouse(0,0,0);menu=0;desktop_active=0;
 /* Pointer range follows the active mode: in 800x600 the cursor must reach
    the right/bottom edges instead of stopping at 639/399 (3/4 across). */
 test_info.width=800;test_info.height=600;
 ksessions_mouse_inject(900,700,0);
 assert(mouse_x==799&&mouse_y==599);
 test_info.width=640;test_info.height=400;
 puts("Sessions PASS: active-only menu, create/close, busy guard, deferred switching, both Shifts, autorepeat, Escape, outside click, GUI input isolation and graphics badge");
 return 0;
}
