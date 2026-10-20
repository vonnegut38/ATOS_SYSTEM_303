#!/usr/bin/env python3
"""Diagnostic of the actual session forwarding functions. It FAILED before the
fix: a press+release inside one pump collapsed into a single "released" state.
Now every button edge is delivered in order. Usage: script PROJECT"""
import pathlib,subprocess,tempfile,sys
s=(pathlib.Path(sys.argv[1])/'system/kernel_sessions.c').read_text()
def fn(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);i=b+1;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
# Mocks for everything the extracted slice references.  The queue type/state
# mirror the source; the assertions below are unchanged.
mocks=r'''
#include <stdint.h>
#include <stdio.h>
#define SESSION_COUNT 8
#define KSESS_MOUSE_QUEUE 32u
typedef struct {uint32_t buttons,tick;int32_t x,y;uint8_t edge;} ksess_mouse_ev_t;
static ksess_mouse_ev_t mouse_queue[KSESS_MOUSE_QUEUE];
static unsigned mouse_q_head,mouse_q_tail;
static uint32_t mouse_q_dropped;
static uint32_t mouse_buttons,kdbg_real_mouse,kdbg_real_dxdy,kdbg_real_btn;
static int32_t mouse_x=100,mouse_y=100,mouse_press_x,mouse_press_y;
static uint8_t mouse_scripted,mouse_capture,mouse_press_pending;
static unsigned menu,mask=1,current,request;
static int pointer_max_x(void){return 799;}static int pointer_max_y(void){return 599;}
static int kgfx_desktop_active(void){return 1;}static int kernel_user_gui_keyboard_active(void){return 1;}
static uint32_t kernel_user_hires_clock(void){static uint32_t t;return t+=123u;}
'''
slice_=('\n'.join([fn('void ksessions_mouse_flush('),fn('static void mouse_q_push('),
                   fn('static void handle_mouse('),fn('int ksessions_mouse_peek('),
                   fn('void ksessions_mouse_consume('),fn('int ksessions_mouse_event(')]))
driver=r'''
static unsigned read_all(void){uint32_t b;int32_t x,y;unsigned n=0;
 while(ksessions_mouse_event(&b,&x,&y)){printf("delivered buttons=%u dx=%d dy=%d\n",b,x,y);n++;}return n;
}
int main(void){unsigned n=0;
 puts("control: drain after each edge");handle_mouse(13,0,0);n+=read_all();handle_mouse(0,0,0);n+=read_all();
 printf("control events=%u (expected 2)\n",n);
 puts("two clicks before drain");
 handle_mouse(13,0,0);handle_mouse(0,0,0);handle_mouse(13,0,0);handle_mouse(0,0,0);
 n=read_all();printf("burst events=%u (expected 4)\n",n);
 return n==4?0:1;
}
'''
code=mocks+slice_+driver
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-std=c11',str(p/'t.c'),'-o',str(p/'test')],check=True)
 sys.exit(subprocess.run([str(p/'test')]).returncode)
