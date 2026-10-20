"""Blocking GEMDOS input must present mouse/content and honour window close."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'system/tos_prg.c').read_text()
a=s.index('static int32_t tos_getkey(void)\n{')
b=s.index('static int32_t tos_key_ready',a)
code=r'''
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
static int tos_key_pending=-2,tos_exit_code,closing;
static unsigned polls,frames,wakes;
static jmp_buf exited;
static int input(void){return 0;}
static int (*tos_input_pop)(void)=input;
static int tos_keyq_pop(void){return polls>=3?0x3920:-1;}
static void tos_poll_input(void){++polls;}
static void tos_mark_present(void){++wakes;}
static void tos_composite_work(void){++frames;}
static int tos_exit_pending(void){return closing;}
static void asf_tos_exit(void){longjmp(exited,1);}
'''+s[a:b]+r'''
int main(void){
 assert(tos_getkey()==0x3920&&polls==3&&frames==3&&wakes==3);
 polls=frames=wakes=0;closing=1;
 if(!setjmp(exited)){tos_getkey();assert(!"close request ignored");}
 assert(polls==1&&frames==1&&tos_exit_code==0);
 closing=0;tos_key_pending=0x011b;polls=frames=0;
 assert(tos_getkey()==0x011b&&polls==0&&frames==0);
 puts("Blocking TOS key wait: presentation, close and queued key PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='atos-keywait-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
