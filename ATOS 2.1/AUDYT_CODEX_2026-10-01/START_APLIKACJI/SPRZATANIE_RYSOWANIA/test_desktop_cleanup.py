#!/usr/bin/env python3
"""Run the actual paint_icons function with an instrumented draw backend.
Usage: python3 test_desktop_cleanup.py PROJECT
Tests geometry and ordering, not the emulator or real glyph bitmaps.
"""
import pathlib,subprocess,tempfile,sys
root=pathlib.Path(sys.argv[1]);s=(root/'system/desktop_wm.c').read_text()
a=s.index('static void paint_icons(',s.index('static uint8_t icon_rgb_id('));b=s.index('\n/* Run the action',a);fn=s[a:b]
args='1' if 'int clear_cells' in fn else ''
code=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui_draw.h"
#define ICON_MAX 11
#define CELL_W 84
#define CELL_H 52
#define GRID_X 16
#define GRID_Y 28
static struct {int gx,gy,res;char label[13];} icons[ICON_MAX];
static unsigned icon_count,dirty_count,n;
static int dirty_gx[13],dirty_gy[13],chunky=1,hover_icon=-1,drag_icon=-1,sel_icon=-1;
static gui_draw_t list[96];
static uint8_t pixels[300][800];
static int icon_rgb_id(int i,int h){(void)i;(void)h;return 0;}
static int icon_ink(int i){(void)i;return 0;}
static void rect(int x,int y,int w,int h,int c){
 for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if(i>=0&&i<800&&j>=0&&j<300)pixels[j][i]=c;
}
static void add(uint16_t kind,int16_t x,int16_t y,uint16_t w,uint16_t h,uint16_t color,const char *text){
 (void)color;list[n++].kind=kind;
 if(kind==GUI_BG)rect(x,y,w,h,0);
 if(kind==GUI_ICON_RGB||kind==GUI_ICON){rect(x,y,32,32,1);rect(x-8,y+32,strlen(text)*8,16,1);}
}
'''+fn+r'''
int main(void){
 int fails=0,remaining=0;
 icon_count=1;strcpy(icons[0].label,"ABCDEFGHIJKL");
 PAINT;
 dirty_count=1;dirty_gx[0]=0;dirty_gy[0]=0;icons[0].gx=3;n=0;
 PAINT;
 for(int y=60;y<76;y++)for(int x=8;x<104;x++)remaining+=pixels[y][x]!=0;
 printf("old label pixels after move: %d\n",remaining);fails|=remaining!=0;
 memset(pixels,0,sizeof(pixels));n=0;icon_count=2;
 icons[0].gx=0;icons[1].gx=1;strcpy(icons[1].label,"B");
 PAINT;
 printf("long label survives neighbour clear: %d\n",pixels[60][102]!=0);
 fails|=pixels[60][102]==0;
 return fails;
}
'''
code=code.replace('PAINT',f'paint_icons({args})')
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'t.c').write_text(code)
 subprocess.run(['cc','-std=c11','-I'+str(root/'include'),str(p/'t.c'),'-o',str(p/'test')],check=True)
 sys.exit(subprocess.run([str(p/'test')]).returncode)
