/* Co MUSI byc uszkodzone przy przesuwaniu okna w kompozytorze z powierzchniami.
   Test na hoscie (bez emulatora) odpowiada na pytanie, ktore na emulatorze
   kosztowalo kilka zrzutow: czy warstwa wideo gubi cienkie pasy, czy to
   kompozytor nie odmalowuje czesci wspolnej.
   Wniosek (dwa przypadki ponizej):
     A) pasy + przesuniecie pikseli  -> karta NIE zgadza sie z frontem:
        czesc wspolna starej i nowej ramki tez sie zmienia (tresc okna jedzie
        razem z ramka), wiec KAZDY przesuniety piksel musi trafic do VRAM,
     B) pelna suma (stara + nowa ramka) -> karta == front, BAJT W BAJT.
   Czyli "tani drag" nie jest kwestia naprawy flushu - po prostu kazdy piksel
   okna trzeba zapisac do karty; kompozytor robi to dobrze. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../system/kernel_video.c"

static uint8_t heap[2*1024*1024];static uint32_t heap_used;static uint16_t *shadow_ptr;
void *kmem_alloc(uint32_t bytes,uint32_t align,uint16_t owner,uint16_t flags,int region)
{
 (void)owner;(void)flags;(void)region;
 if(!align)align=1u;
 {uintptr_t base=(uintptr_t)heap+heap_used;
  uintptr_t aligned=(base+align-1u)&~(uintptr_t)(align-1u);
  if(aligned+bytes>(uintptr_t)heap+sizeof heap)return 0;
  heap_used=(uint32_t)(aligned+bytes-(uintptr_t)heap);
  if(align==16u)shadow_ptr=(uint16_t *)aligned;
  return (void *)aligned;}
}
int kmem_pin(void *p,uint16_t owner){(void)owner;return p==(void *)shadow_ptr;}
int kmem_free(void *p,uint16_t owner){(void)p;(void)owner;return 1;}
void kvideo_shifter_base(const volatile uint8_t *p){(void)p;}

#define W 800u
#define H 600u
static uint8_t fbuf[W*H];
static uint8_t cbuf[W*H+2];

/* Wzor NIEJEDNORODNY: dzieki temu przesuniecie okna naprawde zmienia piksele
   w czesci wspolnej (jednolite wypelnienie maskowalo blad). */
static void fill_pattern(uint8_t *f,int x,int y,int w,int h)
{
 int yy,xx;for(yy=0;yy<h;++yy)for(xx=0;xx<w;++xx)
  f[(y+yy)*W+x+xx]=(uint8_t)(((x+xx)*7+(y+yy)*13)&0xff);
}
static void damage_outside(int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh)
{
 int x0=ax>bx?ax:bx,y0=ay>by?ay:by;
 int x1=(ax+aw)<(bx+bw)?(ax+aw):(bx+bw);
 int y1=(ay+ah)<(by+bh)?(ay+ah):(by+bh);
 if(x1<=x0||y1<=y0){kvideo_damage(ax,ay,aw,ah);return;}
 if(y0>ay)kvideo_damage(ax,ay,aw,y0-ay);
 if(y1<ay+ah)kvideo_damage(ax,y1,aw,(ay+ah)-y1);
 if(x0>ax)kvideo_damage(ax,y0,x0-ax,y1-y0);
 if(x1<ax+aw)kvideo_damage(x1,y0,(ax+aw)-x1,y1-y0);
}
/* Przesuniecie prostokata w buforze frontu - to samo, co fb_drag_scroll(). */
static void scroll_front(int x,int y,int w,int h,int dx,int dy)
{
 int yy;
 if(dy>0)for(yy=h-1;yy>=0;--yy)
   memmove(fbuf+(y+yy+dy)*W+x+dx,fbuf+(y+yy)*W+x,(size_t)w);
 else for(yy=0;yy<h;++yy)
   memmove(fbuf+(y+yy+dy)*W+x+dx,fbuf+(y+yy)*W+x,(size_t)w);
}
/* Karta i front trzymaja te SAMA dane, ale host jest little-endian: flush
   czyta front jako slowa (para bajtow) i zapisuje je w kolejnosci karty
   (starszy bajt najpierw).  Dlatego porownujemy z tym samym przesunieciem
   bajtow w parze - inaczej test "lapie" kolejnosc bajtow hosta, nie blad. */
static int card_matches_front(void)
{
 unsigned i;
 for(i=0;i<sizeof fbuf/2u;++i)
  if(cbuf[1+i*2]!=fbuf[i*2+1]||cbuf[2+i*2]!=fbuf[i*2])return 0;
 return 1;
}

int main(void)
{
 int fx=104,fy=40;const int fw=420,fh=300,dx=10,dy=8;
 /* ---- A: pasy + przesuniete piksele ---- */
 memset(cbuf,0xA5,sizeof cbuf);
 assert(kvideo_activate_vofa(cbuf+1));
 kvideo_set_mode_format(W,H,8u);
 kvideo_select(fbuf);
 memset(fbuf,0x11,sizeof fbuf);
 fill_pattern(fbuf,fx,fy,fw,fh);
 kvideo_damage(0,0,(int32_t)W,(int32_t)H);kvideo_flush();
 if(!card_matches_front()){
  int x,y;for(y=0;y<(int)H;++y)for(x=0;x<(int)W;++x)
   if(cbuf[1+y*W+x]!=fbuf[(y*W+x)^1u]){
    printf("A init ROZJAZD (%d,%d) karta=%02x front=%02x\n",x,y,cbuf[1+y*W+x],fbuf[(y*W+x)^1u]);
    return 1; }
 }
 scroll_front(fx,fy,fw,fh,dx,dy);
 damage_outside(fx,fy,fw,fh,fx+dx,fy+dy,fw,fh);
 damage_outside(fx+dx,fy+dy,fw,fh,fx,fy,fw,fh);
 kvideo_flush();
 if(card_matches_front()){
  puts("UWAGA: pasy wystarczyly mimo wzoru - zalozenie o czesci wspolnej jest zle");
  return 1;
 }
 printf("A) pasy+scroll: karta != front (oczekiwane - czesc wspolna tez sie zmienia)\n");
 /* ---- B: pelna suma starej i nowej ramki ---- */
 fx=104;fy=40;
 memset(cbuf,0xA5,sizeof cbuf);memset(fbuf,0x11,sizeof fbuf);
 fill_pattern(fbuf,fx,fy,fw,fh);
 /* Wymus pelne odswiezenie (zmiana trybu ustawia full=1): inaczej cien
    z poprzedniego przypadku pominalby bajty, ktore przypadkiem sie zgadzaja. */
 kvideo_set_mode_format(640u,480u,8u);
 kvideo_set_mode_format(W,H,8u);
 kvideo_select(fbuf);kvideo_damage(0,0,(int32_t)W,(int32_t)H);kvideo_flush();
 assert(card_matches_front());
 scroll_front(fx,fy,fw,fh,dx,dy);
 kvideo_damage(fx,fy,fw+dx,fh+dy);            /* suma: stara + nowa ramka */
 kvideo_flush();
 if(!card_matches_front()){
  int x,y;
  for(y=0;y<(int)H;++y)for(x=0;x<(int)W;++x)
   if(cbuf[1+y*W+x]!=fbuf[y*W+x]){
    printf("B) ROZJAZD (%d,%d) karta=%02x front=%02x\n",x,y,cbuf[1+y*W+x],fbuf[y*W+x]);
    return 1;
   }
 }
 puts("B) pelna suma: karta == front BAJT W BAJT");
 puts("Video strips PASS: warstwa wideo jest poprawna; pasy same NIE wystarcza");
 return 0;
}
