/* Host test for the exclusive game mode (system/kernel_game.c): mode switch
   and restore, 2x/1x copy with clipping, rectangle lists, damage, palette
   caching across a session switch, ownership rules. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "kernel_graphics.h"
#include "kernel_theme.h"
#include "kernel_video.h"
#include "kernel_sessions.h"
#include "kernel_wm.h"
static kgfx_info_t info;
static uint8_t fb[1024*768];
static int vofa=1,desktop=1,cursor_hidden,overlay_hidden,restored_console,damaged_all,bar,flushes,defaults,theme;
static unsigned session;
static int dmg_n;static int32_t dmg[64][4];
static uint8_t dac[256][3];
const kgfx_info_t *kgfx_get_info(void){return &info;}
uint8_t *kgfx_present_target(uint16_t *pitch,uint8_t *bpp){*pitch=info.line_bytes;*bpp=info.mode==8u?8u:1u;return fb;}
int kgfx_desktop_active(void){return desktop;}
int kgfx_enter_desktop(void){desktop=1;return 1;}
void kgfx_restore_console(void){restored_console++;desktop=0;}
int kgfx_video_reconfigure_format(uint16_t w,uint16_t h,uint8_t bpp){
 if(w==999)return -0x3f4;
 info.width=w;info.height=h;info.mode=bpp==8u?8u:2u;info.line_bytes=bpp==8u?w:(uint16_t)(w/8u);return 0;}
void kgfx_cursor_hide(void){cursor_hidden++;}
void kgfx_overlay_hide(void){overlay_hidden++;}
void kgfx_palette_defaults(void){defaults++;}
unsigned kgfx_theme_current(void){return 3;}
int kgfx_theme_apply(unsigned i){theme=(int)i;return 0;}
int kvideo_is_vofa(void){return vofa;}
void kvideo_select(const volatile uint8_t *s){assert(s==fb);}
void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h){if(dmg_n<64){dmg[dmg_n][0]=x;dmg[dmg_n][1]=y;dmg[dmg_n][2]=w;dmg[dmg_n][3]=h;}dmg_n++;}
void kvideo_flush(void){flushes++;}
int kvideo_set_palette_rgb(uint8_t i,uint8_t r,uint8_t g,uint8_t b){dac[i][0]=r;dac[i][1]=g;dac[i][2]=b;return 1;}
static uint8_t cardmem[1024*768];static int use_card;static int (*gate)(void);
volatile uint8_t *kvideo_card8(uint16_t *pitch){if(!use_card||info.mode!=8)return 0;*pitch=info.line_bytes;return cardmem;}
void kvideo_set_flush_gate(int (*g)(void)){gate=g;}
unsigned ksessions_current(void){return session;}
void kernel_wm_damage_all(void){damaged_all++;}
void kernel_wm_refresh_bar(void){bar++;}
#include "../system/kernel_game.c"
int main(void){
 static uint8_t src[224][256];static atos_game_frame_t f;static atos_game_palette_t p;
 int x,y;
 info.width=800;info.height=600;info.mode=8;info.line_bytes=800;
 for(y=0;y<224;y++)for(x=0;x<256;x++)src[y][x]=(uint8_t)(x^y);
 /* not ET4000 -> refused, nothing changed */
 vofa=0;assert(kgame_begin(5,0,640,480)==-0x3f3&&!kgame_active());vofa=1;
 /* refused mode keeps the old one */
 assert(kgame_begin(5,0,999,480)==-0x3f4&&!kgame_active()&&info.width==800);
 memset(fb,0xee,sizeof fb);
 assert(kgame_begin(5,0,640,480)==(int32_t)((640u<<16)|480u));
 assert(kgame_active()&&kgame_owner()==5&&kgame_owns_screen()&&info.width==640&&info.mode==8);
 assert(fb[0]==0&&fb[640*480-1]==0&&cursor_hidden);           /* cleared */
 assert(kgame_begin(6,0,640,480)==-0x3e5);                       /* second game */
 assert(kgame_frame(6,&f,&src[0][0])==-0x3e5);
 /* full frame, doubled, centred */
 f.pixels=0;f.width=256;f.height=224;f.pitch=256;f.scale=2;f.dst_x=64;f.dst_y=16;f.count=0;
 dmg_n=0;assert(kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_OK);
 for(y=0;y<448;y++)for(x=0;x<512;x++)assert(fb[(16+y)*640+64+x]==src[y/2][x/2]);
 assert(fb[16*640+63]==0&&fb[15*640+64]==0&&fb[464*640+64]==0&&fb[16*640+576]==0);
 assert(dmg_n==1&&dmg[0][0]==64&&dmg[0][1]==16&&dmg[0][2]==512&&dmg[0][3]==448);
 /* rectangle list: only those pixels change */
 memset(fb,0,sizeof fb);
 f.count=2;f.rect[0]=(atos_game_rect_t){8,8,16,8};f.rect[1]=(atos_game_rect_t){-4,200,10,40};
 dmg_n=0;assert(kgame_frame(5,&f,&src[0][0])==0);
 assert(fb[(16+16)*640+64+16]==src[8][8]&&fb[(16+31)*640+64+47]==src[15][23]&&fb[(16+32)*640+64+16]==0);
 assert(fb[(16+400)*640+64]==src[200][0]&&fb[(16+447)*640+64+11]==src[223][5]&&fb[(16+400)*640+64+12]==0);
 assert(dmg_n==2&&dmg[1][0]==64&&dmg[1][1]==416&&dmg[1][2]==12&&dmg[1][3]==48);
 /* clipping against the screen at a negative / overflowing origin */
 memset(fb,0,sizeof fb);f.count=0;f.dst_x=-10;f.dst_y=400;
 assert(kgame_frame(5,&f,&src[0][0])==0);
 assert(fb[400*640+0]==src[0][5]&&fb[479*640+501]==src[39][255]&&fb[479*640+502]==0);
 /* odd destination -> byte path, scale 1 */
 memset(fb,0,sizeof fb);f.scale=1;f.dst_x=3;f.dst_y=1;
 assert(kgame_frame(5,&f,&src[0][0])==0&&fb[1*640+3]==src[0][0]&&fb[224*640+258]==src[223][255]&&fb[225*640+3]==0);
 /* palette: cached, applied while on screen */
 p.first=10;p.count=2;p.rgb[0]=1;p.rgb[1]=2;p.rgb[2]=3;p.rgb[3]=4;p.rgb[4]=5;p.rgb[5]=6;
 assert(kgame_palette(5,&p)==0&&dac[11][2]==6);
 p.first=255;p.count=2;assert(kgame_palette(5,&p)==-0x3e0);
 /* session switch: hidden, system palette back, then repaint request */
 session=1;defaults=0;
 assert(!kgame_owns_screen()&&kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_HIDDEN&&defaults==1&&theme==3);
 p.first=10;p.count=1;p.rgb[0]=99;dac[10][0]=0;
 assert(kgame_palette(5,&p)==0&&dac[10][0]==0);               /* cached only */
 session=0;memset(dac,0,sizeof dac);
 assert(kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_REPAINT&&dac[10][0]==99&&dac[11][2]==6);
 assert(kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_OK);
 /* direct scanout: frames go to the card, the flush gate is closed meanwhile */
 use_card=1;memset(cardmem,0x77,sizeof cardmem);memset(fb,0x55,sizeof fb);
 f.scale=2;f.dst_x=64;f.dst_y=16;f.count=0;flushes=0;dmg_n=0;
 assert(kgame_frame(5,&f,&src[0][0])==0&&flushes==0&&dmg_n==0&&gate&&gate());
 for(y=0;y<448;y++)for(x=0;x<512;x++)assert(cardmem[(16+y)*640+64+x]==src[y/2][x/2]);
 assert(cardmem[16*640+63]==0x77&&fb[16*640+64]==0x55);
 memset(cardmem,0,sizeof cardmem);f.count=1;f.rect[0]=(atos_game_rect_t){3,5,5,2};   /* odd width */
 assert(kgame_frame(5,&f,&src[0][0])==0);
 assert(cardmem[(16+10)*640+64+6]==src[5][3]&&cardmem[(16+13)*640+64+15]==src[6][7]&&cardmem[(16+13)*640+64+16]==0);
 /* session switch: gate opens (compositor repaints), clearing the card on return */
 session=1;assert(!gate());assert(kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_HIDDEN);
 session=0;memset(cardmem,0x33,sizeof cardmem);
 assert(kgame_frame(5,&f,&src[0][0])==ATOS_GAME_FRAME_REPAINT&&cardmem[0]==0&&cardmem[640*480-1]==0&&gate());
 use_card=0;
 /* END restores mode, palette, desktop */
 assert(kgame_end(6)==-0x3e5);
 defaults=0;assert(kgame_end(5)==0&&!kgame_active()&&info.width==800&&info.height==600&&defaults==1&&damaged_all==1);
 /* started from the text console: console restored; exit releases */
 desktop=0;restored_console=0;
 assert(kgame_begin(7,0,640,480)>0&&desktop==1);
 kgame_release_pid(8);assert(kgame_active());
 kgame_release_pid(7);assert(!kgame_active()&&restored_console==1);
 puts("KERNEL GAME PASS: ET4000 gate, mode switch/restore, 2x/1x copy, rect lists, clipping, damage, palette cache, session switch, ownership");
 return 0;
}
