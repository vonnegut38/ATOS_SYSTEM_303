#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../system/kernel_video.c"
static int allocated,fail_alloc,shifter_calls;static uint32_t shadow_bytes_seen;
/* Mock kernel heap.  kvideo_activate_vofa allocates TWO things: the shadow and,
   since the per-row damage map landed, two 800-byte row arrays.  One fixed
   buffer made the row arrays alias the shadow, while an assert on a single size
   (bytes==32000) tripped over the row allocations - so the test never reached a
   single pixel check.  This is a bump arena over one static block: every
   allocation gets its own storage, and the shadow - the only 16-byte-aligned
   request - still has its exact size checked, so a geometry regression still
   fails the test instead of being ignored. */
static uint8_t heap[2*1024*1024];   /* 800x600x16: cien ~960 KB + damage */static uint32_t heap_used;static uint16_t *shadow_ptr;
void *kmem_alloc(uint32_t bytes,uint32_t align,uint16_t owner,uint16_t flags,int region)
{
 (void)owner;(void)flags;(void)region;
 if(!align)align=1u;
 if(fail_alloc)return 0;
 {uintptr_t base=(uintptr_t)heap+heap_used;
  uintptr_t aligned=(base+align-1u)&~(uintptr_t)(align-1u);
  if(aligned+bytes>(uintptr_t)heap+sizeof heap)return 0;
  heap_used=(uint32_t)(aligned+bytes-(uintptr_t)heap);
  if(align==16u){shadow_bytes_seen=bytes;shadow_ptr=(uint16_t *)aligned;}
  allocated=1;return (void *)aligned;}
}
int kmem_pin(void *p,uint16_t owner){(void)owner;assert(p==(void *)shadow_ptr);return 1;}
int kmem_free(void *p,uint16_t owner){(void)p;(void)owner;return 1;}
void kvideo_shifter_base(const volatile uint8_t *p){assert(p);shifter_calls++;}
int main(void){
 static uint16_t a[16000],b[16000];static uint8_t vram[38402];
 kvideo_reset((uint8_t *)a);kvideo_select((uint8_t *)b);assert(shifter_calls==1&&!kvideo_is_vofa());
 kvideo_reset((uint8_t *)a);fail_alloc=1;assert(!kvideo_activate_vofa(vram+1));fail_alloc=0;
 memset(vram,0xa5,sizeof vram);assert(kvideo_activate_vofa(vram+1)&&allocated);
 assert(shadow_bytes_seen==32000u);   /* 640x400 mono shadow, exact size */
 for(unsigned i=0;i<16000;i++)a[i]=(uint16_t)(i*19);
 kvideo_flush();assert(!full&&!pending);
 for(unsigned i=0;i<16000;i++){assert(vram[1+i*2]==(a[i]>>8));assert(vram[2+i*2]==(a[i]&255));}
 assert(vram[0]==0xa5);for(unsigned i=32001;i<38402;i++)assert(vram[i]==0xa5);
 /* Idle and unchanged damage must not rewrite VRAM. */
 vram[1]=0x57;kvideo_flush();assert(vram[1]==0x57);
 kvideo_damage(0,0,16,1);kvideo_flush();assert(vram[1]==0x57);
 a[0]=0xabcd;a[1]=0x1234;kvideo_damage(-1,-1,17,2);kvideo_flush();
 assert(vram[1]==0xab&&vram[2]==0xcd&&vram[3]!=(a[1]>>8));
 a[15999]=0xfe01;kvideo_damage(639,399,0x7fffffff,0x7fffffff);kvideo_flush();
 assert(vram[31999]==0xfe&&vram[32000]==1);
 kvideo_damage(INT32_MIN,INT32_MIN,1,1);assert(!pending);
 kvideo_select((uint8_t *)b);kvideo_flush();assert(shifter_calls==1);
 for(unsigned i=1;i<=32000;i++)assert(!vram[i]);
 /* Every bit position/edge in a bounded dirty line. */
 for(unsigned x=0;x<640;x++){
  unsigned y=x%400,offset=y*40+x/16;b[offset]^=(uint16_t)(0x8000u>>(x%16));
  kvideo_damage((int32_t)x,(int32_t)y,1,1);kvideo_flush();
  assert(vram[1+offset*2]==(b[offset]>>8)&&vram[2+offset*2]==(b[offset]&255));
 }
 /* 8 bpp damage is measured in two-pixel words, not 16-pixel words. */
 {static uint16_t colour[800*600/2];static uint8_t colour_vram[800*600+2];
  kvideo_set_mode_format(800,600,8);kvideo_select((uint8_t *)colour);
  memset(colour_vram,0xa5,sizeof colour_vram);
  assert(kvideo_activate_vofa(colour_vram+1));kvideo_flush();
  for(unsigned x=0;x<800;++x){
   unsigned y=x%600,offset=y*400+x/2;
   colour[offset]^=0x4321;
   kvideo_damage((int32_t)x,(int32_t)y,1,1);kvideo_flush();
   assert(colour_vram[1+offset*2]==(colour[offset]>>8));
   assert(colour_vram[2+offset*2]==(colour[offset]&255));
  }
  colour[239999]=0x4567;kvideo_damage(799,599,99,99);kvideo_flush();
  assert(colour_vram[479999]==0x45 && colour_vram[480000]==0x67);
  assert(colour_vram[0]==0xa5 && colour_vram[480001]==0xa5);
  /* Direct console rendering must not read/write the aperture again. */
  {unsigned copies=kvideo_dbg_copies;
   kvideo_select(colour_vram+1);colour_vram[1]=0x39;
   kvideo_damage(0,0,800,600);kvideo_flush();
   assert(kvideo_dbg_copies==copies && colour_vram[1]==0x39);
   assert(!full && !pending);
   kvideo_select((uint8_t *)colour);kvideo_flush();
   assert(kvideo_dbg_copies==copies+1);
   assert(colour_vram[1]==(colour[0]>>8));}

 }
 /* ---- tryb 4 bpp PLANARNY (VOFA_MODE_640_4) --------------------------------
    Desktop rysuje 8 bpp CHUNKY, a karta oczekuje czterech planow: 4 bajty na
    grupe 8 pikseli, po jednym na plan, bit 7-(x&7).  Ten blok sprawdza wlasnie
    te galaz kvideo_flush() - bez niego zmiana bylaby nietestowana. */
 {
  static uint8_t front4[640u*480u];      /* 8 bpp chunky front */
  static uint8_t card4[320u*480u];       /* 4 plany: 320 B na linie */
  unsigned x,y,k;
  kvideo_reset((uint8_t *)front4);
  kvideo_select((uint8_t *)front4);
  memset(card4,0x5A,sizeof card4);
  memset(front4,0,sizeof front4);
  kvideo_set_mode_format(640u,480u,4u);
  assert(kvideo_activate_vofa(card4));
  /* Paleta 0..15 = rosnace szarosci.  Mape 256->16 buduje warstwa wideo:
     indeksy 0..15 zostaja BEZ ZMIAN (dlatego tekst i tlo sa czytelne), a wyzsze
     trafiaja na najblizszy z 16 wpisow.  Wczesniej uzywano stale ">>4", przez co
     indeksy pulpitu zlewaly sie w jeden kolor. */
  {unsigned pk;for(pk=0;pk<16u;++pk)kvideo_set_palette565((uint8_t)pk,(uint16_t)(pk*0x111u));}
  /* Piksel 0: indeks 15 (systemowy, bez zmian) -> wszystkie 4 plany.
     Piksel 1: indeks 200 (nieustawiony = czarny) -> najblizszy wpis to 0,
     czyli ZADNEGO planu.  Stare ">>4" daloby tu indeks 12. */
  front4[0]=15u;front4[1]=200u;
  kvideo_damage(0,0,640,1);
  kvideo_flush();
  assert(card4[0]==(uint8_t)0x80u);   /* plan 0: piksel 0 (bit 7) */
  assert(card4[1]==(uint8_t)0x80u);   /* plan 1 */
  assert(card4[2]==(uint8_t)0x80u);   /* plan 2 */
  assert(card4[3]==(uint8_t)0x80u);   /* plan 3 */
  for(k=4u;k<320u;++k)assert(card4[k]==0u);
  for(y=1u;y<480u;++y)for(x=0u;x<320u;++x)assert(card4[y*320u+x]==0u);
  kvideo_damage(0,2,8,1);front4[2u*640u]=15u;kvideo_flush();
  assert(card4[2u*320u]==0x80u&&card4[2u*320u+1u]==0x80u
         &&card4[2u*320u+2u]==0x80u&&card4[2u*320u+3u]==0x80u);
  assert(card4[1u*320u]==0u);
  assert(card4[1u*320u]==0u);
  puts("Video output 4bpp PASS: planarna konwersja flushu, 4 plany, damage wiersza");
 }
 /* ---- tryb 16 bpp HICOLOR (VOFA_MODE_800_16) ------------------------------
    Front jest 8 bpp chunky, a karta oczekuje 5:6:5 pakowanego po 2 bajty
    little-endian.  Kolor bierze sie z TABELI palety, ktora w tym trybie zastepuje
    DAC karty - dlatego test ustawia ja sam przez kvideo_set_palette565(). */
 {
  static uint8_t front16[800u*600u];
  static uint8_t card16[1600u*600u];
  unsigned y;
  kvideo_reset((uint8_t *)front16);
  kvideo_select((uint8_t *)front16);
  memset(card16,0x5A,sizeof card16);
  memset(front16,0,sizeof front16);
  kvideo_set_mode_format(800u,600u,16u);
  assert(kvideo_activate_vofa(card16));
  kvideo_set_palette565(1u,0x0F00u);        /* czysta czerwien -> 0xF800 */
  kvideo_set_palette565(2u,0x00F0u);        /* czysta zielen   -> 0x07E0 */
  front16[0]=1u;front16[1]=2u;
  kvideo_damage(0,0,800,1);
  kvideo_flush();
  assert(card16[0]==0x00u&&card16[1]==0xF8u);   /* 0xF800 little-endian */
  assert(card16[2]==0xE0u&&card16[3]==0x07u);   /* 0x07E0 little-endian */
  assert(card16[4]==0x00u&&card16[5]==0x00u);   /* indeks 0 = czarny */
  for(y=1600u;y<3200u;++y)assert(card16[y]==0u);  /* wiersz 1 nadal pusty */
  puts("Video output 16bpp PASS: konwersja 5:6:5 do karty, little-endian, damage wiersza");
 }
 puts("Video output PASS: Shifter selection, allocation fallback, big-endian pixels, changed words, idle, clipping, edges and session buffer switches");
 return 0;
}
