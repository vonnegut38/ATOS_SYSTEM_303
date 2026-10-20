#!/usr/bin/env python3
"""Host test of actual plasma source. Stubs IPC/clock; does not emulate Atari.
Usage: python3 test_plasma.py PROJECT"""
from pathlib import Path
import subprocess,tempfile,sys,json
r=Path(sys.argv[1]).resolve()
code='#include <stdio.h>\n#include <string.h>\n#include <assert.h>\n#include '+json.dumps(str(r/'system/plasma.c'))+'\n'+r'''
static unsigned test_pending,test_dest,test_op,test_clock,test_blocked,test_batches,test_sleeps;
static unsigned test_pixels[PH][PW];
int32_t gui_call(uint32_t call,uint32_t a,uint32_t b,void *p,void *q){
 (void)b;(void)q;
 if(call==40)return strcmp(p,"SYSTEM.GFX")==0?1:2;
 if(call==67)return test_clock;
 if(call==11){test_clock+=246;test_sleeps++;return 0;}
 if(call==36){uint32_t *v=p;test_pending=1;test_dest=a;test_op=v[0];
  if(a==1&&v[0]==18){test_batches++;
   for(unsigned k=0;k<n;k++){gui_draw_t *d=&list[k];
    assert(d->kind==GUI_FILL);
    for(unsigned y=d->y;y<(unsigned)d->y+d->h&&y<PH;y++)
     for(unsigned x=d->x;x<(unsigned)d->x+d->w&&x<PW;x++)test_pixels[y][x]=d->color;
   }
  }return 0;
 }
 if(call==38||call==37){gui_message_t *out=p;memset(out,0,sizeof(*out));
  if(test_pending){test_pending=0;out->sender=test_dest;
   out->v[1]=test_dest==2?(uint32_t)-1:test_op==30?(800u<<16)|600u:test_op==35?8u:0u;
   return 0;
  }
  if(call==38){test_blocked++;out->v[0]=WCLS;return 0;}
  if(test_clock>=12288){out->v[0]=WCLS;return 0;}
  return -1;
 }
 return 0;
}
int main(void){
 gfx_pid=1;chunky=1;prepare();
 for(unsigned mode=0;mode<2;mode++)for(unsigned ph=0;ph<ICON_PAL_COLORS;ph++){
  chunky=mode;frame(ph);
  for(unsigned y=0;y<PH;y++)for(unsigned x=0;x<PW;x++){
   unsigned v=((((x/10)*(x/10)*3+(y/10)*(y/10)*2)>>4)+ph)%ICON_PAL_COLORS;
   assert(test_pixels[y][x]==(mode?ICON_PAL_BASE+v:(v>>3)&1));
  }
 }
 puts("PASS: every phase, every pixel, colour and mono");
 test_batches=0;frame(0);printf("batches per frame: %u\n",test_batches);
 test_batches=test_clock=test_blocked=test_sleeps=0;
 assert(app_main()==0);
 printf("idle animation: batches=%u sleeps=%u blocking input waits=%u\n",test_batches,test_sleeps,test_blocked);
 assert(test_batches>20&&test_sleeps>0&&test_blocked==0);
 puts("PASS: advances without mouse/keyboard and closes on WCLS");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-O2','-std=c11','-I'+str(r/'include'),str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
