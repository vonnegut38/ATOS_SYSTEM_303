#include "kernel_sessions.h"
#include "kernel_console.h"
#include "kernel_graphics.h"
extern int kernel_user_session_busy(unsigned);
extern int kernel_input_pending(void);
extern int kernel_queued_session_busy(unsigned);
extern int kernel_user_desktop_visible(void);
extern int kernel_user_gui_keyboard_active(void);
extern void kernel_user_terminal_reset_modifiers(void);
extern int kernel_raw_mouse_event(uint32_t *,int32_t *,int32_t *);
static const kernel_shell_service_t *shell;
static unsigned current,mask=1,request,modifiers;
static int enabled,menu,mouse_x=600,mouse_y=8;
static uint32_t mouse_buttons,forward_buttons;
static int32_t forward_x,forward_y;
static uint8_t f10_down,escape_down,mouse_capture,forward_ready;
unsigned ksessions_current(void){return current;}
unsigned ksessions_mask(void){return mask;}
int ksessions_mouse_x(void){return mouse_x;}
int ksessions_mouse_y(void){return mouse_y;}
void ksessions_mouse_flush(void){forward_ready=0;forward_x=forward_y=0;forward_buttons=0;}
int ksessions_mouse_event(uint32_t *b,int32_t *x,int32_t *y)
{if(!forward_ready)return 0;*b=forward_buttons;*x=forward_x;*y=forward_y;ksessions_mouse_flush();return 1;}
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
 if(scan==0x2a)bit=1;else if(scan==0x36)bit=2;else if(scan==0x38)bit=8;
 if(bit){if(down)modifiers|=bit;else modifiers&=~bit;}
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
 kernel_user_terminal_reset_modifiers();kconsole_select(next,reset);
 kgfx_session_view(kernel_user_desktop_visible());
 if(reset)shell->start();
}
static void handle_mouse(uint32_t buttons,int32_t dx,int32_t dy)
{
 unsigned i,row,slot=0;uint32_t down=buttons&3u;
 uint32_t press=(down&~mouse_buttons)|((buttons>>3)&3u);
 mouse_buttons=down;mouse_x+=dx;mouse_y+=dy;
 if(mouse_x<0)mouse_x=0;
 if(mouse_x>639)mouse_x=639;
 if(mouse_y<0)mouse_y=0;
 if(mouse_y>399)mouse_y=399;
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
  if(mouse_x>=576&&mouse_y<16){if(press&2)menu=1;mouse_capture=down!=0;return;}
 }
 if(menu)return;
 if(kernel_user_gui_keyboard_active()){
  forward_buttons=buttons|(forward_buttons&0x1cu);forward_x+=dx;forward_y+=dy;forward_ready=1;
 }
}
void ksessions_pump(void)
{
 uint32_t buttons;int32_t dx,dy;
 if(!enabled)return;
 if(kernel_raw_mouse_event(&buttons,&dx,&dy))handle_mouse(buttons,dx,dy);
 apply_request();kgfx_session_view(kernel_user_desktop_visible());
}
void ksessions_paint(void)
{
 term_cell_t cells[32];unsigned i,j,rows=1;
 if(!enabled)return;
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
