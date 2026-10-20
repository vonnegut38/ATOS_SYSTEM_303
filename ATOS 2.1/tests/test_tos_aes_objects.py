#!/usr/bin/env python3
"""Exercise the actual AES walkers with GEM's parent links and hidden subtrees."""
import pathlib, subprocess, tempfile, sys
# The repo root comes from this file.  Only honour an explicit argument when it
# really looks like the tree: `unittest discover` passes its own argv ("discover"
# and a start directory), which used to leave root="discover" and the test failed
# with FileNotFoundError instead of testing anything.
root=pathlib.Path(__file__).resolve().parents[1]
if len(sys.argv)>1:
    _arg=pathlib.Path(sys.argv[1])
    if _arg.is_dir() and (_arg/'system/tos_prg.c').exists():
        root=_arg
s=(root/'system/tos_prg.c').read_text()
a=s.index('static int aes_obj_offset(');b=s.index('static int32_t tos_aes_dispatch(',a)
form=s[s.index('    case 56: {'):s.index('    case 47: {',s.index('    case 56: {'))]
# objc_draw's dispatcher branch, so the zero-clip rule is tested where it lives
odraw=s[s.index('    case 42: '):s.index('    case 56: {')]
abi=s[s.index('static uint16_t aes_abi_in('):s.index('int32_t asf_tos_aesvdi(')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define AES_OBJ_SIZE 24u
static uint16_t aes_u(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static int16_t aes_w(const uint8_t *p){return (int16_t)aes_u(p);}
static uint32_t rd32(const uint8_t *p){return ((uint32_t)aes_u(p)<<16)|aes_u(p+2);}
static int fills;
static long req_px;   /* pixel area actually handed to VDI after clipping */
static int first_x0=9999,first_y0=9999,first_seen;
static void tos_vdi_fill_rect(int x,int y,int w,int h,int c){(void)c;fills++;
 if(w>0&&h>0){req_px+=(long)w*h;
  if(!first_seen){first_seen=1;first_x0=x;first_y0=y;}}}
static void tos_vdi_text(int x,int y,const char*s,int c,int t){(void)x;(void)y;(void)s;(void)c;(void)t;}
/* The AES text path hands its rectangle to the VDI glyph clip. */
static int text_clip_on;
static uint32_t aes_dbg_tree, aes_dbg_type;   /* diagnostics touched by objc_draw */
static void tos_vdi_text_clip(int x0,int y0,int x1,int y1){(void)x0;(void)y0;(void)x1;(void)y1;text_clip_on=1;}
static void tos_vdi_text_clip_off(void){text_clip_on=0;}
'''+s[a:b]+r'''
static int16_t mx,my;
static int tos_exit_code;
static void tos_poll_input(void){}
static void tos_mouse_state(int16_t*out){out[1]=mx;out[2]=my;out[3]=0;}
static void tos_composite_work(void){}
static int tos_exit_pending(void){return 0;}
static void asf_tos_exit(void){assert(0);}
static int form_button(const int16_t *intin,int16_t *intout,void **addrin){switch(56){
'''+form+r'''
}return -1;}
'''+abi+r'''
static int16_t objc_draw(const int16_t *intin,void **addrin){switch(42){
'''+odraw+r'''
default: return -1;}}

static void word(uint8_t *p,int v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void obj(uint8_t *t,int i,int next,int head,int tail,int flags,int x,int y){
 uint8_t *o=t+24*i;word(o,next);word(o+2,head);word(o+4,tail);word(o+6,0x7114);
 word(o+8,flags);word(o+16,x);word(o+18,y);word(o+20,20);word(o+22,20);
}
int main(void){
 uint8_t t[4*24]={0};int x,y,b=100;
 (void)aes_clip_visible;   /* used by objc_draw/objc_change, not extracted here */
 obj(t,0,-1,1,3,0,10,20);obj(t,1,2,-1,-1,0,3,4);
 obj(t,2,3,-1,-1,0x80,5,6);obj(t,3,0,-1,-1,0,7,8);
 assert(aes_obj_offset(t,0,3,0,0,&x,&y,&b)==1&&x==17&&y==28);
 b=100;assert(aes_obj_offset(t,0,8,0,0,&x,&y,&b)==0&&b==96);
 b=100;assert(aes_obj_find(t,0,0,0,99,18,29,&b)==3);
 b=100;assert(aes_obj_find(t,0,0,0,0,18,29,&b)==0);
 b=100;assert(aes_obj_find(t,0,0,0,99,30,40,&b)==-1);
 b=100;aes_obj_draw(t,0,0,0,99,&b);assert(fills==15&&b==96);
 {int16_t in[2]={1,1},out[2]={0};void *addr[1]={t};
  word(t+24+8,5);word(t+24+10,0);mx=14;my=25;
  assert(form_button(in,out,addr)==0&&out[1]==1&&aes_u(t+24+10)==1);
  word(t+24+10,0);mx=50;my=50;
  assert(form_button(in,out,addr)==1&&out[1]==0&&aes_u(t+24+10)==0);
  word(t+24+10,8);mx=14;my=25;
  assert(form_button(in,out,addr)==1&&out[1]==0&&aes_u(t+24+10)==8);
  word(t+24+8,64);word(t+24+10,0);in[1]=2;
  assert(form_button(in,out,addr)==0&&(uint16_t)out[1]==0x8001);
 }
 /* --- AES clipping (audit P1) -------------------------------------------
    objc_draw's clip rectangle used to be ignored: a request to repaint an 8x16
    fragment repainted the whole 640x400 panel.  Measured with the same counter
    as the audit: unclipped = 258080 requested pixels, clip (32,32,8,16) = 128. */
 {uint8_t t2[24]={0};int b2=100;
  word(t2+0,0xffff);word(t2+2,0xffff);word(t2+4,0xffff);
  word(t2+6,20);                                  /* G_BOX */
  word(t2+16,0);word(t2+18,0);word(t2+20,640);word(t2+22,400);
  aes_clip_clear();fills=0;req_px=0;
  aes_obj_draw(t2,0,0,0,0,&b2);
  assert(req_px==258080);                         /* the audit's number */
  aes_clip_set(32,32,8,16);fills=0;req_px=0;b2=100;
  aes_obj_draw(t2,0,0,0,0,&b2);
  assert(req_px==128);                            /* only the requested fragment */
  assert(fills==1);                               /* borders clipped away entirely */
  aes_clip_clear();
  /* a clip fully inside the panel paints exactly that fragment */
  aes_clip_set(100,100,4,4);fills=0;req_px=0;b2=100;
  aes_obj_draw(t2,0,0,0,0,&b2);
  assert(req_px==16&&fills==1);
  /* a clip completely outside the panel (and the screen) paints nothing */
  aes_clip_set(700,500,4,4);fills=0;req_px=0;b2=100;
  aes_obj_draw(t2,0,0,0,0,&b2);
  assert(req_px==0&&fills==0);
  aes_clip_clear();
 }
 /* --- child object position (audit P2) ---------------------------------- */
 {uint8_t t3[2*24]={0};int cx=0,cy=0,b3=100;
  word(t3+0,0xffff);word(t3+2,1);word(t3+4,1);word(t3+6,20);
  word(t3+16,100);word(t3+18,50);word(t3+20,300);word(t3+22,200);
  word(t3+24+0,0xffff);word(t3+24+2,0xffff);word(t3+24+4,0xffff);word(t3+24+6,20);
  word(t3+24+16,10);word(t3+24+18,20);word(t3+24+20,40);word(t3+24+22,10);
  assert(aes_obj_offset(t3,0,1,0,0,&cx,&cy,&b3)==1&&cx==110&&cy==70);
  /* and drawing that child now starts from its absolute position */
  fills=0;req_px=0;b3=100;aes_obj_draw(t3,1,100,50,0,&b3);
  assert(req_px==(40L*10L+40+40+10+10));
 }
 /* --- the same rules through the REAL objc_draw dispatcher branch --------- */
 {uint8_t t4[24]={0};int16_t in[6];void *addr[1]={t4};
  word(t4+0,0xffff);word(t4+2,0xffff);word(t4+4,0xffff);word(t4+6,20);
  word(t4+16,0);word(t4+18,0);word(t4+20,640);word(t4+22,400);
  in[0]=0;in[1]=0;in[2]=32;in[3]=32;in[4]=8;in[5]=16;      /* clip 8x16 */
  fills=0;req_px=0;assert(objc_draw(in,addr)==1&&req_px==128&&fills==1);
  in[4]=0;in[5]=0;                                          /* ZERO width  */
  fills=0;req_px=0;assert(objc_draw(in,addr)==1&&req_px==0&&fills==0);
  in[2]=0;in[3]=0;in[4]=640;in[5]=400;                      /* full panel  */
  fills=0;req_px=0;assert(objc_draw(in,addr)==1&&req_px==258080);
  /* child under a parent: drawing must start at the ABSOLUTE position */
  {uint8_t t5[2*24]={0};void *a5[1]={t5};
   word(t5+0,0xffff);word(t5+2,1);word(t5+4,1);word(t5+6,20);
   word(t5+16,100);word(t5+18,50);word(t5+20,300);word(t5+22,200);
   word(t5+24+0,0xffff);word(t5+24+2,0xffff);word(t5+24+4,0xffff);word(t5+24+6,20);
   word(t5+24+16,10);word(t5+24+18,20);word(t5+24+20,40);word(t5+24+22,10);
   in[0]=1;in[1]=0;in[2]=0;in[3]=0;in[4]=640;in[5]=400;
   fills=0;req_px=0;first_seen=0;
   assert(objc_draw(in,a5)==1);
   assert(first_x0==110&&first_y0==70);          /* (10,20) + (100,50) */
  }
 }
 /* --- logger parameter counts: AES control[1]/[2], VDI control[3]/[4] ----- */
 {int16_t ca[6]={42,4,1,2,0,0};      /* AES: 4 int_in, 1 int_out */
  int16_t cv[6]={6,3,0,8,2,0};       /* VDI: 3 ptsin, 8 intin, 2 intout */
  assert(aes_abi_in(ca,1)==4&&aes_abi_out(ca,1)==1);
  assert(aes_abi_in(cv,0)==8&&aes_abi_out(cv,0)==2);
 }
 puts("AES objects + form_button + clip + child offsets + ABI: PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='atos-aes-test-') as d:
 p=pathlib.Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
