#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,sys
r=Path(sys.argv[1]).resolve()
def function(s,name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);i=b+1;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
s=(r/'system/kernel_wm.c').read_text();sync=function(s,'static void wm_sync_view(') if 'static void wm_sync_view(' in s else ''
sess=(r/'system/kernel_sessions.c').read_text()
code=r'''
#include <stdio.h>
#include <stdint.h>
#include "wm_core.h"
static wm_state_t wm;
static unsigned current,request,autostart_target;
static uint8_t wm_ready=1,wm_force_compose,autostart_desk;
static uint32_t kdbg_desk_set,kdbg_desk_take,seen;
static unsigned ksessions_current(void){return current;}
static void task_draw(void){}
'''+sync+function(s,'void kernel_wm_refresh_bar(')+function(sess,'void ksessions_autostart_desktop(')+function(sess,'int ksessions_take_autostart(')+r'''
static void draw(void *p,uint32_t pid,const wm_rect_t *r,uint16_t x,uint16_t y){(void)p;(void)r;(void)x;(void)y;if(pid)seen=pid;}
int main(void){int fail=0;wm_init(&wm,640,400);wm.view_session=0;wm.count=2;
 for(unsigned i=0;i<2;i++){wm.win[i].session=i;wm.win[i].flags=7;wm.win[i].pid=i+1;wm.win[i].frame=wm.win[i].client=(wm_rect_t){0,0,640,400};}
 wm_rebuild_clip(&wm);
 for(unsigned i=1;i<5;i++){
  current=i%2;seen=0;kernel_wm_refresh_bar();
  wm.view_session=current; /* legacy late assignment in the composition path */
  wm_damage_add(&wm,(wm_rect_t){0,0,640,400});wm_recompose(&wm,draw,0);
  printf("session=%u visible canvas=%u expected=%u\n",current,seen,current+1);fail|=seen!=current+1;
 }
 current=0;request=2;ksessions_autostart_desktop();
 int early=ksessions_take_autostart();request=0;current=1;int ready=ksessions_take_autostart();
 printf("autostart early=%d after switch=%d (expected 0,1)\n",early,ready);fail|=early!=0||ready!=1;
 return fail;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-std=c11','-I'+str(r/'include'),str(p/'t.c'),str(r/'system/wm_core.c'),'-o',str(p/'test')],check=True)
 sys.exit(subprocess.run([str(p/'test')]).returncode)
