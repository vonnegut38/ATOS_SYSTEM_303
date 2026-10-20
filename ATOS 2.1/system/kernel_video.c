#include "kernel_video.h"
#include "kgfx_planar4.h"
#include "kgfx_present16.h"
#include "kernel_graphics.h"
#include "kernel_memory.h"
/* Comparing against ST-RAM avoids slow VRAM reads and writes. Each dirty
 * scanline is bounded in word units; no scan happens on an idle frame.
 *
 * The geometry follows the active adapter mode: the Shifter and the ET4000
 * mono modes all store one byte per 8 pixels, so the compositor's front buffer
 * is width/8 bytes per row.  Both the Shifter profile (640x400) and the adapter
 * modes (640x480 view 640x400, 800x600, 1024x768) feed the same code. */
static const volatile uint8_t *front;
static volatile uint8_t *card;
/* Shadow copy of the card image. Allocated lazily from the kernel heap only
   when a VOFA adapter is actually activated, so machines without the card pay
   zero RAM for it. */
static uint16_t *shadow;
static uint32_t shadow_bytes;
/* Per-row damage map, in 16-bit words: up to KVIDEO_SPANS disjoint [lo..hi)
   ranges, kept sorted and merged.  ONE [left,right) pair per row turned a
   window OUTLINE into a scan of the whole interior: the left and right edges
   widened the single range to span the window (a 600x400 border scanned 245 KB
   instead of ~5.6 KB).  Empty rows have row_span_n == 0. */
#define KVIDEO_SPANS 8u
static uint16_t *row_span_lo,*row_span_hi;   /* [mode_h*KVIDEO_SPANS] words */
static uint8_t *row_span_n;                  /* damaged range count per row */
/* Tryb 4 bpp PLANARNY (VOFA_MODE_640_4): obraz trzeba konwertowac z 8 bpp
   chunky do czterech planow, a nie kopiowac slowami jak tryby ST/mono. */
static uint8_t mode_planar4;
/* Tryb 16 bpp HICOLOR (VOFA_MODE_800_16): 5:6:5 pakowane, 2 bajty na piksel. */
static uint8_t mode_packed16;
/* Paleta 256 -> 5:6:5 dla trybu 16 bpp.  Nalezy do TEJ warstwy, bo tylko ona
   konwertuje obraz; kernel_graphics wola kvideo_set_palette565() przy kazdej
   zmianie palety systemowej.  Dzieki temu konwerter jest inline z naglowka
   (kgfx_present16.h) i test warstwy wideo nie musi linkowac kernel_graphics.c. */
static uint16_t pal565[256];
/* Tablica 256 -> 16 dla trybu 4 bpp (16 kolorow).  Budowana leniwie przy
   pierwszym uzyciu po zmianie palety.  Indeksy 0..15 zostaja BEZ ZMIAN, zeby
   kolory systemowe (tekst, tlo, ramki) byly dokladne; wyzsze mapujemy na
   najblizszy z 16 wpisow (odleglosc w 5:6:5 z waga oka). */
static uint8_t map16[256];
static uint8_t map16_dirty=1u;
static uint16_t row_capacity;
static uint16_t mode_w=640u,mode_h=400u,mode_pitch=80u; /* pitch: bytes/row */
static uint8_t mode_bpp=1u;                              /* 1 = planar, 8 = chunky */
static uint8_t full,pending;
/* Damaged row range, so a flush touches only the rows that changed instead of
   walking every scanline (768 of them at 1024x768) on each small repaint. */
static unsigned row_min=400u,row_max;
uint32_t kvideo_dbg_active,kvideo_dbg_calls,kvideo_dbg_copies;
uint32_t kdbg_dmg_px,kdbg_flush_words,kdbg_flush_calls,kdbg_flush_area;

static uint16_t row_words(void){return (uint16_t)(mode_pitch>>1);}

/* The card hangs off the 68000 bus, so a 16-bit store puts the HIGH byte at the
   lower address.  On the big-endian target the native store already produces
   exactly that, which is why the fast path can stay one wide access per
   word/pair.  On a little-endian host (the host test) the same native store
   would swap the bytes, so the two bytes are written explicitly instead - the
   VRAM byte contract stays checked rather than becoming an artefact of the
   toolchain.  Byte stores are order-independent, so the fallback is correct on
   any host; it is only skipped when big endian is known at compile time. */
#if defined(__BYTE_ORDER__)&&(__BYTE_ORDER__==__ORDER_BIG_ENDIAN__)
#define KVIDEO_CARD_BIG_ENDIAN 1u
#else
#define KVIDEO_CARD_BIG_ENDIAN 0u
#endif
static void card_put16(volatile uint8_t *card,unsigned offset,uint16_t v)
{
#if KVIDEO_CARD_BIG_ENDIAN
 *(volatile uint16_t *)(card+offset*2)=v;
#else
 card[offset*2]=(uint8_t)(v>>8);card[offset*2+1]=(uint8_t)v;
#endif
}
static void card_put32(volatile uint8_t *card,unsigned offset,uint16_t lo,uint16_t hi)
{
#if KVIDEO_CARD_BIG_ENDIAN
 *(volatile uint32_t *)(card+offset*2)=((uint32_t)lo<<16)|(uint32_t)hi;
#else
 card[offset*2]=(uint8_t)(lo>>8);card[offset*2+1]=(uint8_t)lo;
 card[offset*2+2]=(uint8_t)(hi>>8);card[offset*2+3]=(uint8_t)hi;
#endif
}

static void damage_clear_rows(void)
{
 uint16_t y;
 if(!row_span_n)return;
 for(y=0;y<mode_h;y++)row_span_n[y]=0;
 row_min=mode_h;row_max=0;
}
/* Insert [first,last) into the damaged set of one row, merging anything it
   touches and keeping the list sorted.  When all KVIDEO_SPANS ranges are in
   use the pair with the smallest gap is bridged - that widens the scan by the
   gap, but never loses a change (an "emergency widen", as the audit asked). */
static void span_add(uint16_t y,uint16_t first,uint16_t last)
{
 uint16_t *lo=row_span_lo+(uint32_t)y*KVIDEO_SPANS;
 uint16_t *hi=row_span_hi+(uint32_t)y*KVIDEO_SPANS;
 uint8_t n=row_span_n[y],i,j;
 if(first>=last)return;
 for(;;){
  for(i=0;i<n;){
   if(last<lo[i]||first>hi[i]){++i;continue;}
   if(lo[i]<first)first=lo[i];
   if(hi[i]>last)last=hi[i];
   for(j=i;j+1u<n;++j){lo[j]=lo[j+1u];hi[j]=hi[j+1u];}
   --n;
  }
  if(n<KVIDEO_SPANS)break;
  {uint8_t best=0;uint32_t bg=0xffffffffu;
   for(i=0;i+1u<n;++i){uint32_t gap=(uint32_t)lo[i+1u]-(uint32_t)hi[i];if(gap<bg){bg=gap;best=i;}}
   hi[best]=hi[best+1u];
   for(j=best+1u;j+1u<n;++j){lo[j]=lo[j+1u];hi[j]=hi[j+1u];}
   --n;}
 }
 for(i=0;i<n&&lo[i]<first;++i){}
 for(j=n;j>i;--j){lo[j]=lo[j-1u];hi[j]=hi[j-1u];}
 lo[i]=first;hi[i]=last;
 row_span_n[y]=(uint8_t)(n+1u);
}

static int damage_rows_ensure(uint16_t rows)
{
 if(rows<=row_capacity)return 1;
 if(row_span_lo){kmem_free(row_span_lo,KMEM_OWNER_KERNEL);row_span_lo=0;}
 if(row_span_hi){kmem_free(row_span_hi,KMEM_OWNER_KERNEL);row_span_hi=0;}
 if(row_span_n){kmem_free(row_span_n,KMEM_OWNER_KERNEL);row_span_n=0;}
 row_capacity=0;
 {uint32_t spans=(uint32_t)rows*KVIDEO_SPANS;
  row_span_lo=(uint16_t *)kmem_alloc(spans*(uint32_t)sizeof(uint16_t),4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);
  row_span_hi=(uint16_t *)kmem_alloc(spans*(uint32_t)sizeof(uint16_t),4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);
  row_span_n=(uint8_t *)kmem_alloc(rows,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);}
 if(!row_span_lo||!row_span_hi||!row_span_n){
  if(row_span_lo){kmem_free(row_span_lo,KMEM_OWNER_KERNEL);row_span_lo=0;}
  if(row_span_hi){kmem_free(row_span_hi,KMEM_OWNER_KERNEL);row_span_hi=0;}
  if(row_span_n){kmem_free(row_span_n,KMEM_OWNER_KERNEL);row_span_n=0;}
  return 0;}
 row_capacity=rows;
 return 1;
}

static int shadow_ensure(void)
{
 uint32_t need=(uint32_t)mode_pitch*(uint32_t)mode_h;
 if(shadow&&shadow_bytes>=need)return 1;
 if(shadow){kmem_free(shadow,KMEM_OWNER_KERNEL);shadow=0;shadow_bytes=0;}
 /* TT-RAM: cien jest czystym cache'em CPU (porownanie slow i zapis tylko
    zmian do karty), bez DMA i bez blittera.  W 8 bpp to 480000 B, czyli
    ponad 10% ST-RAM, a na akceleratorze TT-RAM jest szybszy od ST. */
 shadow=(uint16_t *)kmem_alloc(need,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
 if(!shadow)return 0;
 shadow_bytes=need;
 return 1;
}

void kvideo_reset(const volatile uint8_t *screen)
{
 front=screen;card=0;full=0;pending=0;damage_clear_rows();
}
int kvideo_is_vofa(void){return card!=0;}
const void *kvideo_front(void){return (const void *)front;}
const void *kvideo_scanout(void){return (const void *)card;}
uint8_t kvideo_mode_bpp(void){return mode_bpp;}
uint16_t kvideo_mode_width(void){return mode_w;}
uint16_t kvideo_mode_height(void){return mode_h;}
/* Adopt a mode before (or after) the adapter scans it out.  The row arrays and
   the shadow are resized to the new geometry; the next flush repaints all of
   it, which is exactly what a mode switch needs. */
void kvideo_set_mode_format(uint16_t width,uint16_t height,uint8_t bpp)
{
 if(width<16u)width=16u;
 if(height<16u)height=16u;
 if(width==mode_w&&height==mode_h&&mode_bpp==bpp&&row_span_lo)return;
 if(!damage_rows_ensure(height))return;
 mode_w=width;mode_h=height;mode_bpp=bpp;
 /* 8 bpp chunky: one byte per pixel, so the pitch is the pixel width. */
 /* 4 bpp PLANARNY: wiersz karty to 4 bajty na grupe 8 pikseli. */
 mode_planar4=(uint8_t)(bpp==4u);
 mode_packed16=(uint8_t)(bpp==16u);
 mode_pitch=(uint16_t)((bpp==8u)?width:(mode_packed16?((uint32_t)width*2u):(mode_planar4?(((width+7u)/8u)*4u):((width+7u)/8u))));
 if(card)(void)shadow_ensure();
 full=1;pending=0;damage_clear_rows();
}
int kvideo_activate_vofa(volatile uint8_t *vram)
{
 if(!shadow_ensure())return 0; /* no shadow -> stay on Shifter */
 if(!damage_rows_ensure(mode_h))return 0; /* no damage map -> cannot flush */
 card=vram;full=1;pending=0;damage_clear_rows();kvideo_dbg_active=1u;return 1;
}
void kvideo_set_mode(uint16_t width,uint16_t height){kvideo_set_mode_format(width,height,1u);}
void kvideo_deactivate_vofa(void){if(card){card=0;full=1;pending=0;}}
void kvideo_select(const volatile uint8_t *screen)
{
 if(front==screen)return;
 front=screen;
 if(card)full=1;else kvideo_shifter_base(screen);
}
void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h)
{
 unsigned first,last,end,pixel_shift;uint16_t maxw;
 if(!card||!row_span_lo||w<=0||h<=0||x>=(int32_t)mode_w||y>=(int32_t)mode_h)return;
 if(x<0){if(x<=-w)return;w+=x;x=0;}
 if(y<0){if(y<=-h)return;h+=y;y=0;}
 if(w>(int32_t)mode_w-x)w=(int32_t)mode_w-x;
 if(h>(int32_t)mode_h-y)h=(int32_t)mode_h-y;
 maxw=row_words();
 pixel_shift=mode_bpp==8u?1u:4u;
 first=(unsigned)x>>pixel_shift;
 last=((unsigned)(x+w)+(1u<<pixel_shift)-1u)>>pixel_shift;end=(unsigned)(y+h);
 if(last>maxw)last=maxw;
 if(first>=last)return;
 pending=1;
 kdbg_dmg_px+=(uint32_t)w*(uint32_t)h;
 {unsigned start=(unsigned)y;
  if(start<row_min)row_min=start;
  if(end>row_max)row_max=end;
  for(; (unsigned)y<end;y++)span_add((uint16_t)y,(uint16_t)first,(uint16_t)last);}
}
static int (*flush_gate)(void);
static uint8_t flush_gated;
void kvideo_set_flush_gate(int (*gate)(void)){flush_gate=gate;}
volatile uint8_t *kvideo_card8(uint16_t *pitch)
{
 if(!card||mode_bpp!=8u)return 0;
 if(pitch)*pitch=mode_pitch;
 return card;
}
void kvideo_set_palette565(uint8_t index,uint16_t color12)
{
 pal565[index]=kgfx_rgb12_to_565(color12);
 map16_dirty=1u;
}
static void rebuild_map16(void)
{
 unsigned i,k;
 for(i=0;i<256u;++i){
  if(i<16u){map16[i]=(uint8_t)i;continue;}   /* systemowe BEZ ZMIAN */
  {unsigned best=0;uint32_t bestd=0xffffffffu;
   for(k=0;k<16u;++k){
    int dr=(int)((pal565[i]>>11)&31u)-(int)((pal565[k]>>11)&31u);
    int dg=(int)((pal565[i]>>5)&63u)-(int)((pal565[k]>>5)&63u);
    int db=(int)(pal565[i]&31u)-(int)(pal565[k]&31u);
    uint32_t d=(uint32_t)(dr*dr*4+dg*dg*2+db*db*4);
    if(d<bestd){bestd=d;best=k;}
   }
   map16[i]=(uint8_t)best;}
 }
 map16_dirty=0u;
}

void kvideo_flush(void)
{
 unsigned y,x;const volatile uint16_t *src=(const volatile uint16_t *)front;
 uint16_t maxw=row_words();
 ++kvideo_dbg_calls;++kdbg_flush_calls;
 if(flush_gate&&flush_gate()){flush_gated=1u;return;}
 if(flush_gated){flush_gated=0;full=1;}
 if(!card||!front||!shadow||!row_span_lo||(!full&&!pending))return;
 /* The boot console draws straight into VRAM. Reading the aperture back
    into itself costs a full bus scan without presenting anything new.
    A later front-buffer switch sets full=1 and rebuilds the shadow. */
 if(front==card){full=0;pending=0;damage_clear_rows();return;}
 /* Tryb 4 bpp PLANARNY (VOFA_MODE_640_4): front jest obrazem 8 bpp chunky,
    a karta oczekuje czterech planow - 4 bajty na grupe 8 pikseli, bit 7-(x&7),
    piksel = b0 | b1<<1 | b2<<2 | b3<<3.  Uklad jest WSPOLNY z modelem karty
    w Hatari (src/vofa.c) i tabelami sterownika (seq4/gdc4/attr4).
    Konwertujemy wylacznie uszkodzone wiersze.  Shadow jest w jednostkach
    slow, wiec w tym trybie go nie porownujemy - i tak zapisujemy caly wiersz. */
 /* Tryb 16 bpp HICOLOR: front jest 8 bpp chunky, a karta oczekuje 5:6:5
    pakowanego po 2 bajty.  Kolor bierze sie z TABELI palety systemowej, bo
    w tym trybie DAC karty nie jest uzywany.  Uklad wspolny z modelem w Hatari
    (src/vofa.c) i tabelami sterownika (gdc16[6] = 0x08). */
 if(mode_packed16){
  const uint8_t *src8=(const uint8_t *)front;
  uint16_t row_bytes=(uint16_t)((uint32_t)mode_w*2u);
  unsigned ry;
  for(ry=0;ry<mode_h;++ry){
   if(!full&&row_span_n[ry]==0u)continue;
   if(kgfx_present_packed16((uint8_t *)(uintptr_t)card+(uint32_t)ry*(uint32_t)row_bytes,
                            row_bytes,src8+(uint32_t)ry*(uint32_t)mode_w,mode_w,
                            pal565,mode_w,1u)!=0)break;
  }
  full=0;pending=0;damage_clear_rows();
  return;
 }
 if(mode_planar4){
  const uint8_t *src8=(const uint8_t *)front;
  uint16_t row_bytes=(uint16_t)(((mode_w+7u)/8u)*4u);
  unsigned ry;
  for(ry=0;ry<mode_h;++ry){
   if(!full&&row_span_n[ry]==0u)continue;
   if(map16_dirty)rebuild_map16();
   if(kgfx_present_planar4((uint8_t *)(uintptr_t)card+(uint32_t)ry*(uint32_t)row_bytes,
                           row_bytes,src8+(uint32_t)ry*(uint32_t)mode_w,mode_w,
                           map16,mode_w,1u)!=0)break;
  }
  full=0;pending=0;damage_clear_rows();
  return;
 }
 ++kvideo_dbg_copies;
 {unsigned y0=full?0u:row_min,y1=full?mode_h:row_max;
  if(y0>=y1){full=0;pending=0;return;}
  for(y=y0;y<y1;y++){
   /* Step over every DISJOINT damaged range of this row.  A full repaint is
      just one range covering the whole scanline. */
   uint8_t k,n=full?1u:row_span_n[y];
   const uint16_t *slo=row_span_lo+(uint32_t)y*KVIDEO_SPANS;
   const uint16_t *shi=row_span_hi+(uint32_t)y*KVIDEO_SPANS;
   for(k=0;k<n;++k){
   unsigned begin=full?0u:slo[k],end=full?maxw:shi[k];
   /* Copy TWO words per store.  The card is on a 16-bit bus, so the bus cycles
      are the same, but the CPU issues half as many stores - and in the
      256-colour mode a full frame is 240000 words, where that matters.  An
      even word index keeps the 32-bit store aligned (every mode's words-per-row
      is even), so a leading odd word and a trailing one are done 16-bit. */
   if((begin&1u)&&begin<end){
    unsigned offset=y*(unsigned)maxw+begin;uint16_t v=src[offset];
    if(full||shadow[offset]!=v){card_put16(card,offset,v);shadow[offset]=v;++kdbg_flush_words;}
    ++begin;
   }
   for(x=begin;x+1u<end;x+=2u){
    unsigned offset=y*(unsigned)maxw+x;
    uint16_t lo=src[offset],hi=src[offset+1];
    if(full||shadow[offset]!=lo||shadow[offset+1]!=hi){
     card_put32(card,offset,lo,hi);
     shadow[offset]=lo;shadow[offset+1]=hi;kdbg_flush_words+=2u;
    }
   }
   if(x<end){
    unsigned offset=y*(unsigned)maxw+x;uint16_t v=src[offset];
    if(full||shadow[offset]!=v){card_put16(card,offset,v);shadow[offset]=v;++kdbg_flush_words;}
   }
   }
   row_span_n[y]=0;
  }}
 row_min=mode_h;row_max=0;
 full=0;pending=0;
}
