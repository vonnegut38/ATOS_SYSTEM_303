#!/usr/bin/env python3
"""Run the production VT52 parser and AES event branch under ASan/UBSan."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'system/tos_prg.c').read_text()
console=s[s.index('/* Guest console bytes'):s.index('static void tos_line(int ')]
event=s[s.index('    case 25: {'):s.index('    case 120:',s.index('    case 25: {'))]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "st_raster.h"
#include "tos_damage.h"
static tos_damage_t damage;
static uint16_t tos_src_width(void){return 640;}
static uint16_t tos_src_height(void){return 400;}
static void tos_note_dirty_rect(int x0,int y0,int x1,int y1){tos_damage_note(&damage,x0,y0,x1+1,y1+1);}
static uint8_t screen[32000],*tos_screen=screen,tos_mode=2;
/* Stub for the AES message queue.  The kernel posts WM_REDRAW when a window
   opens and evnt_multi takes it from the queue; the console test has no window,
   so the queued message is emulated here (it used to be synthesised inline). */
static int tos_msg_take(int16_t *out)
{
    out[0]=20;out[1]=0;out[2]=0;out[3]=1;out[4]=0;out[5]=0;out[6]=640;out[7]=400;
    return 1;
}
static uint8_t tos_window_enabled=1u;   /* 1 = private screen is on display */
static uint16_t tos_src_stride(void){return 80;}
static uint8_t tos_src_bpp(void){return 1;}
static void tos_putc(char c){(void)c;assert(0);}
static void tos_putpixel(int x,int y,uint8_t c){assert(x>=0&&x<640&&y>=0&&y<400);st_raster_set_pixel(screen,80,1,x,y,c);}
static const uint8_t *font_glyph(char c,uint8_t tall){static uint8_t g[16]={0x80,0x40};(void)c;(void)tall;return g;}
'''+console+r'''
static void tos_con_publish(void){}
static unsigned polls;
static uint32_t tos_mfp_ticks,tos_evmulti_n,tos_evkbd_n,tos_evmes_n;
static int tos_redraw_sent __attribute__((unused)),tos_exit_code,key_ready;
static int16_t mouse_x,mouse_y;
static uint32_t mouse_buttons;
static void tos_clock_tick(void){++tos_mfp_ticks;}
static uint32_t tos_mouse_buttons;
static int tos_key_lastmods=4,tos_kbshift,tos_win_x=80,tos_win_y=92,tos_client_w=640,tos_win_h=400;
static void *tos_client_surface;
#define TOS_TITLE 18
static void kernel_wm_client_origin(void *s,int32_t*x,int32_t*y){(void)s;*x=80;*y=110;}
static int ksessions_mouse_x(void){return mouse_x+80;}
static int ksessions_mouse_y(void){return mouse_y+110;}
''' + s[s.index('static void tos_mouse_state(int16_t *out)\n{'):s.index('/* Source geometry',s.index('static void tos_mouse_state(int16_t *out)\n{'))] + r'''
static void tos_poll_input(void){assert(++polls<1000);tos_mouse_buttons=mouse_buttons;}
static int tos_key_ready(void){return key_ready;}
static uint32_t tos_getkey(void){key_ready=0;return 0x00390020;}
static void tos_composite_work(void){}
static int tos_exit_pending(void){return 0;}
static void asf_tos_exit(void){assert(0);}
static int event_multi(const int16_t *intin,int16_t *intout,void **addrin){switch(25){
'''+event+r'''
}return -1;}
int main(void){
 int16_t in[16]={0},out[8],msg[8];void *addr[1]={msg};
 tos_damage_reset(&damage,640,400);
 tos_con_reset();tos_console_string("\033Y#!A");
 assert(st_raster_pixel(screen,80,1,8,48)==1);
 assert(st_raster_pixel(screen,80,1,9,49)==1);
 assert(tos_con_x==2&&tos_con_y==3);
 assert(damage.left[48]==8&&damage.right[48]==16&&damage.right[64]==0);
 tos_console_string("\033K");assert(st_raster_pixel(screen,80,1,8,48)==1);
 tos_console_string("\033Y#!\033pA\033q");
 assert(st_raster_pixel(screen,80,1,8,48)==0&&st_raster_pixel(screen,80,1,9,48)==1);
 tos_console_string("\033E");assert(tos_con_x==0&&tos_con_y==0);
 for(unsigned i=0;i<sizeof screen;i++)assert(screen[i]==0);
 tos_console_string("\033Y\377\377\033wA");assert(tos_con_x==79&&tos_con_y==24);
 tos_console_string("\n\033H\033I"); /* both scroll directions stay in bounds */
 assert(damage.left[0]==0&&damage.right[399]==640);
 memset(out,0x55,sizeof out);mouse_x=120;mouse_y=90;
 in[0]=1;key_ready=1;
 assert(event_multi(in,out,addr)==1&&out[1]==120&&out[2]==90&&out[5]==0x3920&&out[4]==4); /* Ctrl was released after key-down */
 assert((uint16_t)out[7]==0x5555); /* evnt_multi has SEVEN output words */
 in[0]=32;in[14]=10;key_ready=1;polls=0;
 assert(event_multi(in,out,addr)==32&&key_ready==1); /* don't consume unrequested keys */
 tos_kbshift=4;in[0]=2;in[2]=1;in[3]=1;mouse_buttons=13; /* native left press includes edge flags */
 assert(event_multi(in,out,addr)==2&&out[3]==1&&out[4]==4&&out[6]==1);
 in[0]=4;in[4]=0;in[5]=100;in[6]=80;in[7]=30;in[8]=20;
 assert(event_multi(in,out,addr)==4);
 in[4]=1;mouse_x=130;assert(event_multi(in,out,addr)==4); /* exclusive right edge */
 in[0]=16;assert(event_multi(in,out,addr)==16&&msg[0]==20&&msg[3]==1&&msg[6]==640);
 puts("VT52 screen output/scroll/erase and AES keyboard/button/timer/mouse/output bounds: PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='atos-console-test-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
