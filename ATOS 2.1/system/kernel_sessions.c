#include "kernel_sessions.h"
#include "kernel_console.h"
#include "kernel_graphics.h"
#include "kernel_wm.h"
extern int kernel_user_session_busy(unsigned);
extern int kernel_input_pending(void);
extern int kernel_queued_session_busy(unsigned);
extern int kernel_user_desktop_visible(void);
extern int kernel_user_gui_keyboard_active(void);
extern void kernel_user_terminal_reset_modifiers(void);
extern int kernel_raw_mouse_event(uint32_t *,int32_t *,int32_t *);
/* Same unit as the apps' hires()/syscall 67, so a client can time a double
   click from when the edge HAPPENED, not from when it was drained. */
extern uint32_t kernel_user_hires_clock(void);
/* Pointer range follows the ACTIVE video mode.  It used to be clamped to
   640x400, so in 800x600 and 1024x768 the cursor stopped three quarters of the
   way across the screen and could not reach the right/bottom edges. */
static int pointer_max_x(void)
{const kgfx_info_t *g=kgfx_get_info();int w=(g&&g->width)?(int)g->width:640;return w>0?w-1:639;}
static int pointer_max_y(void)
{const kgfx_info_t *g=kgfx_get_info();int h=(g&&g->height)?(int)g->height:400;return h>0?h-1:399;}
static const kernel_shell_service_t *shell;
static unsigned current,mask=1,request,modifiers;
/* Set when the user picks "Add Desk": the new session must get its own desktop.
   The spawn happens in the main loop, once the context switch has been applied
   there, so the process is born into the new session. */
static uint8_t autostart_desk;
static unsigned autostart_target;
uint32_t kdbg_desk_set,kdbg_desk_take;
static int enabled,menu,mouse_x=600,mouse_y=8;
static void (*session_reset)(void);
void ksessions_set_reset(void (*fn)(void)){session_reset=fn;}
static uint32_t mouse_buttons;
/* Ordered interaction queue.  The live pointer (mouse_x/mouse_y/mouse_buttons)
   drives presentation and the compositor; a GUI client instead receives the
   ORDERED edges from this queue, each carrying its own absolute position and
   timestamp.  The older code merged every edge inside one pump into a single
   "forward" state, so a press+release that arrived before the app drained its
   mailbox collapsed into the released state and the click was lost. */
#define KSESS_MOUSE_QUEUE 32u
typedef struct {uint32_t buttons,tick;int32_t x,y;uint8_t edge;} ksess_mouse_ev_t;
static ksess_mouse_ev_t mouse_queue[KSESS_MOUSE_QUEUE];
static unsigned mouse_q_head,mouse_q_tail;
static uint32_t mouse_q_dropped;
static uint8_t f10_down,escape_down,mouse_capture;
unsigned ksessions_current(void){return current;}
void ksessions_autostart_desktop(void)
{autostart_target=request?request-1u:current;autostart_desk=1u;++kdbg_desk_set;}
int ksessions_take_autostart(void)
{
    if(!autostart_desk||request)return 0; /* selection can be deferred by input */
    autostart_desk=0u;
    if(current!=autostart_target)return 0; /* cancelled or failed selection */
    ++kdbg_desk_take;return 1;
}
unsigned ksessions_mask(void){return mask;}
int ksessions_mouse_x(void){return mouse_x;}
uint32_t ksessions_mouse_buttons(void){return mouse_buttons;}
int ksessions_mouse_y(void){return mouse_y;}
static int32_t mouse_press_x=600,mouse_press_y=8;
static uint8_t mouse_press_pending;
/* While a scripted self-test injects synthetic position/button state, real IKBD
   mouse packets must not clobber it (Hatari keeps sending host motion).  The
   desktop clears this at the end of each script with GFX opcode 26. */
static uint8_t mouse_scripted;
void ksessions_mouse_script(int on){mouse_scripted=on?1u:0u;}
int ksessions_mouse_scripted(void){return mouse_scripted!=0u;}
int ksessions_mouse_press_x(void){return mouse_press_x;}
int ksessions_mouse_press_y(void){return mouse_press_y;}
int ksessions_mouse_press_pending(void){return mouse_press_pending!=0u;}
void ksessions_mouse_press_consume(void){mouse_press_pending=0u;}
void ksessions_mouse_flush(void){mouse_q_head=mouse_q_tail=0;}
/* Test hook: inject a synthetic mouse state as if the IKBD produced it.
   Used by the scripted compositor test (drag/resize) so it can run without a
   host mouse.  Sets both the absolute position and the pending button event. */
uint32_t kdbg_inject_calls,kdbg_inject_xy,kdbg_inject_btn,kdbg_inject_ok;
uint32_t kdbg_real_mouse,kdbg_real_dxdy,kdbg_real_btn;
uint32_t kdbg_inject_ring[32],kdbg_inject_ring_n,kdbg_frames;
uint32_t kdbg_sleep_calls,kdbg_sleep_park,kdbg_sleep_zero,kdbg_yield_calls,kdbg_yield_ok,kdbg_yield_bad;
/* Append one interaction event.  A button EDGE always gets its own entry; pure
   motion merges into a preceding motion entry (only the newest position
   matters) so a moving pointer cannot crowd edges out of the client mailbox. */
static void mouse_q_push(uint32_t buttons,int32_t x,int32_t y,uint8_t edge)
{
    if(!edge&&mouse_q_tail!=mouse_q_head){
        unsigned last=(mouse_q_tail+KSESS_MOUSE_QUEUE-1u)%KSESS_MOUSE_QUEUE;
        if(!mouse_queue[last].edge&&((mouse_queue[last].buttons&3u)==(buttons&3u))){
            mouse_queue[last].x=x;mouse_queue[last].y=y;
            mouse_queue[last].tick=kernel_user_hires_clock();
            return;
        }
    }
    {unsigned next=(mouse_q_tail+1u)%KSESS_MOUSE_QUEUE;
     if(next==mouse_q_head){
         /* Full: keep the newest events, the release is what matters most. */
         mouse_q_head=(mouse_q_head+1u)%KSESS_MOUSE_QUEUE;
         ++mouse_q_dropped;
     }}
    mouse_queue[mouse_q_tail].buttons=buttons;
    mouse_queue[mouse_q_tail].x=x;mouse_queue[mouse_q_tail].y=y;
    mouse_queue[mouse_q_tail].tick=kernel_user_hires_clock();
    mouse_queue[mouse_q_tail].edge=edge;
    mouse_q_tail=(mouse_q_tail+1u)%KSESS_MOUSE_QUEUE;
}
void ksessions_mouse_inject(int32_t x,int32_t y,uint32_t buttons)
{
    ++kdbg_inject_calls;kdbg_inject_ok=enabled?1u:0u;
    if(!enabled)return;
    {int mx=pointer_max_x(),my=pointer_max_y();
     if(x<0)x=0;else if(x>mx)x=mx;
     if(y<0)y=0;else if(y>my)y=my;}
    kdbg_inject_xy=((uint32_t)(uint16_t)x<<16)|(uint16_t)y;kdbg_inject_btn=buttons;
    {uint32_t q=(kdbg_inject_ring_n&15u)*2u;
     kdbg_inject_ring[q]=kdbg_frames;
     kdbg_inject_ring[q+1u]=((uint32_t)(uint16_t)x<<16)|(uint16_t)y;
     ++kdbg_inject_ring_n;}
    {uint32_t prev=mouse_buttons;mouse_x=x;mouse_y=y;mouse_buttons=buttons;
     if((buttons&1u)&&!(prev&1u)){mouse_press_x=x;mouse_press_y=y;mouse_press_pending=1u;}
     if((buttons&3u)!=(prev&3u))mouse_q_push(buttons,x,y,1u);}
}
/* Peek the oldest queued event WITHOUT consuming it: the caller must confirm
   the real recipient has mailbox room first, so a full client mailbox can never
   turn an ordered DOWN/UP pair into a silent loss. */
int ksessions_mouse_peek(uint32_t *b,int32_t *x,int32_t *y,uint32_t *tick)
{
    if(mouse_q_head==mouse_q_tail)return 0;
    if(b)*b=mouse_queue[mouse_q_head].buttons;
    if(x)*x=mouse_queue[mouse_q_head].x;
    if(y)*y=mouse_queue[mouse_q_head].y;
    if(tick)*tick=mouse_queue[mouse_q_head].tick;
    return 1;
}
void ksessions_mouse_consume(void)
{if(mouse_q_head!=mouse_q_tail)mouse_q_head=(mouse_q_head+1u)%KSESS_MOUSE_QUEUE;}
int ksessions_mouse_event(uint32_t *b,int32_t *x,int32_t *y)
{
    if(!ksessions_mouse_peek(b,x,y,0))return 0;
    ksessions_mouse_consume();return 1;
}
/* Kernel-side session hotkey (Shift+Alt+F10).  While a TOS GUI owns the
   keyboard the shell module never sees the keys, so the switch has to be
   requested from the kernel's own keyboard funnel. */
void ksessions_hotkey_cycle(void)
{
    unsigned i;
    if(!enabled)return;
    for(i=1u;i<=SESSION_COUNT;i++){
        unsigned n=(current+i)%SESSION_COUNT;
        if(mask&(1u<<n)){request=n+1u;return;}
    }
}
int32_t ksessions_control(uint32_t op,uint32_t value)
{
 (void)value;
 if(!op)return (int32_t)current+1;
 if(!enabled||op>4)return -1;
 if(op==4&&(current==0||kernel_user_session_busy(current)||kernel_queued_session_busy(current)))return -1;
 request=op;return 0;
}
int ksessions_key(uint32_t type,uint32_t scan)
{
 unsigned bit=0,i;int down=type==1;scan&=127;
 if(!enabled)return 0;
 if(scan==0x1d)bit=4;else if(scan==0x2a)bit=1;else if(scan==0x36)bit=2;else if(scan==0x38)bit=8;
 if(bit){if(down)modifiers|=bit;else modifiers&=~bit;}
 if(kernel_wm_key(type,scan,modifiers))return 1;
 /* Ctrl+Alt+Del reboots even while an application owns the keyboard. */
 if(scan==0x53&&down&&(modifiers&4u)&&(modifiers&8u)&&session_reset)session_reset();
 if(scan==0x44){
  if(!down){int was=f10_down;f10_down=0;return was;}
  if(f10_down)return 1;
  if((modifiers&3)&&(modifiers&8)){
   f10_down=1;menu=0;
   for(i=1;i<=SESSION_COUNT;i++)if(mask&(1u<<((current+i)%SESSION_COUNT))){request=(current+i)%SESSION_COUNT+1;break;}
   return 1;
  }
 }
 if(scan==1&&escape_down){if(!down)escape_down=0;return 1;}
 if(menu){if(scan==1&&down){menu=0;escape_down=1;}return 1;}
 return 0;
}
void ksessions_init(const kernel_shell_service_t *service,int mono)
{shell=service;enabled=mono&&service&&service->select_session;current=0;mask=1;request=0;}
void ksessions_unpaint(void)
{if(enabled){kgfx_overlay_hide();kgfx_cursor_hide();}}
static void apply_request(void)
{
 unsigned next,reset=0,old=current;
 if(!request||kernel_input_pending())return;
 if(request==4){
  request=0;
  if(!current||kernel_user_session_busy(current)||kernel_queued_session_busy(current))return;
  mask&=~(1u<<current);next=0;
 }else{next=request-1;request=0;reset=!(mask&(1u<<next));}
 if(next==old&&!reset)return;
 /* Only change contexts at a kernel pump boundary, never inside SHELL.SYS. */
 if(!shell->select_session(next,reset))return;
 current=next;mask|=1u<<next;menu=0;ksessions_mouse_flush();
 /* A drag started in another session cannot click controls in this one. */
 mouse_capture=mouse_buttons!=0;
 kernel_user_terminal_reset_modifiers();kconsole_select(next,reset);
 kgfx_session_view(kernel_user_desktop_visible());
 /* The graphics-mode session badge lives in the top bar; repaint it now so the
    filled/hollow squares follow the newly selected tts. */
 if(kernel_wm_enabled())kernel_wm_refresh_bar();
 if(reset)shell->start();
}
/* ISR-safe pointer tracking (Timer C / TOS trap path).  While a TOS program
   runs, kernel_tos_run_program() blocks the kernel main loop, and with it
   ksessions_pump() - the only thing that normally feeds the IKBD deltas into
   mouse_x/mouse_y.  Without this the desktop pointer, and so the cursor the
   compositor draws, stood still for the whole run ("our mouse cursor is
   switched off").  Position only: no event is queued, so no click is ever
   invented while the desktop is not running. */
static volatile uint8_t pointer_dirty;
void ksessions_pointer_track(int32_t dx,int32_t dy)
{
    int mx=pointer_max_x(),my=pointer_max_y();
    if(!dx&&!dy)return;
    mouse_x+=dx;mouse_y+=dy;
    if(mouse_x<0)mouse_x=0;else if(mouse_x>mx)mouse_x=mx;
    if(mouse_y<0)mouse_y=0;else if(mouse_y>my)mouse_y=my;
    pointer_dirty=1u;
}
/* 1 once after each tracked movement, so the TOS band path can force a present
   instead of waiting for its /5 divider (10 Hz made the pointer crawl). */
int ksessions_pointer_take_dirty(void)
{int d=pointer_dirty;pointer_dirty=0u;return d;}

/* Live button state for the window manager while a TOS program owns the
   screen.  The desktop's own pump (handle_mouse) is blocked for the whole run,
   so without this the compositor saw a permanently released button: the title
   bar, the close box, the task list and window dragging were dead even though
   the pointer moved.  ONLY the session state the compositor reads is updated -
   no event is queued for an application - so nothing can be replayed after the
   program returns. */
void ksessions_pointer_button(uint32_t down,int32_t x,int32_t y)
{
    down&=3u;
    if((down&1u)&&!(mouse_buttons&1u)){
        mouse_press_x=x;mouse_press_y=y;mouse_press_pending=1u;
    }
    mouse_buttons=down;
    /* A button edge must also make the compositor produce a frame: the TOS band
       path only composes when the program's pixels or the pointer changed, so a
       click with a stationary pointer produced NO frame and the window chrome
       (close box, title drag, task list) stayed dead.  Marking the pointer dirty
       reuses that existing forced-present path - and it stays out of the
       interrupt handler, because the compose happens on the next trap. */
    pointer_dirty=1u;
}

static void handle_mouse(uint32_t buttons,int32_t dx,int32_t dy)
{
 unsigned i,row,slot=0;uint32_t down=buttons&3u;
 if(mouse_scripted)return;
 uint32_t prev_down=mouse_buttons;
 uint32_t press=(down&~mouse_buttons)|((buttons>>3)&3u);
 ++kdbg_real_mouse;kdbg_real_dxdy=((uint32_t)(uint16_t)dx<<16)|(uint16_t)dy;kdbg_real_btn=buttons;
 mouse_buttons=down;mouse_x+=dx;mouse_y+=dy;
 {int mx=pointer_max_x(),my=pointer_max_y();
  if(mouse_x<0)mouse_x=0;else if(mouse_x>mx)mouse_x=mx;
  if(mouse_y<0)mouse_y=0;else if(mouse_y>my)mouse_y=my;}
 if(press&1u){mouse_press_x=mouse_x;mouse_press_y=mouse_y;mouse_press_pending=1u;}
 if(mouse_capture){if(!down)mouse_capture=0;return;}
 if(press){
  if(menu){
   if((press&2)&&mouse_x>=576&&mouse_y>=16){
    row=(unsigned)mouse_y/16;
    for(i=0;i<SESSION_COUNT;i++)if(mask&(1u<<i)){
     if(i==current)continue;
     if(--row==0){slot=i+1;break;}
    }
    if(!slot&&row==1)for(i=0;i<SESSION_COUNT;i++)if(!(mask&(1u<<i))){slot=i+1;break;}
    if(slot)request=slot;
   }
   menu=0;mouse_capture=down!=0;return;
  }
  if(((kgfx_desktop_active()?(mouse_x>=534&&mouse_x<560):(mouse_x>=576)))&&mouse_y<16){if(press&2)menu=1;mouse_capture=down!=0;return;}
 }
 if(menu)return;
 if(kernel_user_gui_keyboard_active())
  mouse_q_push(buttons,mouse_x,mouse_y,(uint8_t)(down!=prev_down));
}
void ksessions_pump(void)
{
 uint32_t buttons;int32_t dx,dy;
 if(!enabled)return;
 /* Always fold raw IKBD mouse packets into the session state.  The old
    !forward_ready guard skipped handle_mouse() while a forward was pending;
    when the app could not receive it (full mailbox) the flag stayed set, so
    every later packet - including the button RELEASE - was ignored.  The
    pointer froze and the drag outline stayed on screen forever.  handle_mouse
    still accumulates the delta for the app. */
 /* Drain EVERY queued packet, not one per call.  This pump runs about once per
    frame (50 Hz) while the host mouse can deliver well over 100 packets/s:
    handling a single packet per frame let a drag fall further and further
    behind, so the window kept resizing after the button was released and a
    click took seconds to register.  Motion is accumulated; each button edge is
    flushed with the motion that preceded it, so a fast press+release inside one
    frame is still seen as a click. */
 {uint32_t last=0;int32_t ax=0,ay=0;int any=0,valid=0,guard;
  for(guard=0;guard<64&&kernel_raw_mouse_event(&buttons,&dx,&dy);++guard){
   if(!valid){last=buttons;valid=1;}
   else if(buttons!=last){handle_mouse(last,ax,ay);ax=ay=0;last=buttons;}
   ax+=dx;ay+=dy;any=1;
  }
  if(any)handle_mouse(last,ax,ay);}
 apply_request();kgfx_session_view(kernel_user_desktop_visible());
}
void ksessions_paint(void)
{
 if(kconsole_boot_active())return;
 term_cell_t cells[32];unsigned i,j,rows=1;
 if(!enabled)return;
 /* Graphics mode: the session indicator is a graphic badge painted by the
    compositor (kernel_wm), so the text marker '[1]v' must not be drawn here.
    The expanded menu stays textual so session switching keeps working. */
 if(kgfx_desktop_active()&&!menu){kgfx_overlay_hide();return;}
 for(i=0;i<32;i++)cells[i]=' '|0x100;
 cells[0]='['|0x100;cells[1]=('1'+current)|0x100;cells[2]=']'|0x100;cells[4]='v'|0x100;
 if(menu){
  for(i=0;i<SESSION_COUNT;i++)if(i!=current&&(mask&(1u<<i))){
   j=rows++*8;cells[j]='['|0x100;cells[j+1]=('1'+i)|0x100;cells[j+2]=']'|0x100;
   cells[j+4]='t'|0x100;cells[j+5]='t'|0x100;cells[j+6]='s'|0x100;cells[j+7]=('1'+i)|0x100;
  }
  if(mask!=7){j=rows++*8;cells[j]='['|0x100;cells[j+1]='+'|0x100;cells[j+2]=']'|0x100;
   cells[j+4]='N'|0x100;cells[j+5]='o'|0x100;cells[j+6]='w'|0x100;cells[j+7]='y'|0x100;}
 }
 kgfx_overlay_show(cells,rows);kgfx_cursor_move(mouse_x,mouse_y);
}
