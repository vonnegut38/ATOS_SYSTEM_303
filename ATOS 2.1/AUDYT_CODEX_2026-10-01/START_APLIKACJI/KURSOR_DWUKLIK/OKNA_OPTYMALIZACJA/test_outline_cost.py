#!/usr/bin/env python3
"""Measure scan coverage, not elapsed performance, with the real kvideo_damage
and the real per-row disjoint span map. Usage: script PROJECT
Before the fix one [left,right) pair per row made a window OUTLINE scan its whole
interior (245 KB for a 600x400 border). After the fix the scan is the union of
the disjoint edge ranges (~5.6 KB for the old+new border)."""
from pathlib import Path
import subprocess,tempfile,sys
s=(Path(sys.argv[1])/'system/kernel_video.c').read_text()
def fn(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);i=b+1;d=1
 while d:d+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
span=fn('static void span_add(')
a=s.index('void kvideo_damage(');b=s.index('\nvoid kvideo_flush',a)
damage=s[a:b]
code=r'''
#include <stdint.h>
#include <stdio.h>
#define KVIDEO_SPANS 8u
static uint8_t sentinel,*card=&sentinel,mode_bpp=8,pending;
static uint16_t span_lo[600*KVIDEO_SPANS],span_hi[600*KVIDEO_SPANS];
static uint8_t span_n[600];
static uint16_t *row_span_lo=span_lo,*row_span_hi=span_hi;
static uint8_t *row_span_n=span_n;
static uint16_t mode_w=800,mode_h=600;
static unsigned row_min,row_max;
static uint16_t row_words(void){return 400;}
static uint8_t exact[600][400];
'''+span+'\n'+damage+r'''
static void rect(int x,int y,int w,int h){
 kvideo_damage(x,y,w,h);
 for(int j=y;j<y+h;j++)for(int i=x/2;i<(x+w+1)/2;i++)exact[j][i]=1;
}
static void border(int x,int y,int w,int h){rect(x,y,w,1);rect(x,y+h-1,w,1);rect(x,y,1,h);rect(x+w-1,y,1,h);}
static void show(const char *name){unsigned scan=0,touched=0;
 for(unsigned y=0;y<600;y++)for(unsigned k=0;k<span_n[y];k++)
  scan+=(span_hi[y*KVIDEO_SPANS+k]-span_lo[y*KVIDEO_SPANS+k])*2;
 for(unsigned y=0;y<600;y++)for(unsigned x=0;x<400;x++)touched+=exact[y][x]*2;
 printf("%s: current scan bytes=%u; exact union of aligned edge words=%u; ratio=%.2f\n",name,scan,touched,(double)scan/touched);
}
int main(void){for(int y=0;y<600;y++)span_n[y]=0;row_min=600;
 border(20,40,600,400);show("single border");
 border(26,44,600,400);show("old and new border");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-std=c11',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
