#include "kernel_video.h"
#include "kernel_memory.h"
/* Comparing against ST-RAM avoids slow VRAM reads and writes. Each dirty
 * scanline is bounded in word units; no scan happens on an idle frame. */
static const volatile uint8_t *front;
static volatile uint8_t *card;
/* Shadow copy of the card image. A STATIC buffer, not kmem_alloc: the
   allocation failed at boot, which left card==0 and made every flush a
   no-op (the screen never updated). 32000 bytes = 16000 words. */
static uint16_t shadow_buf[16000];
static uint16_t *shadow=shadow_buf;
static uint8_t left[400],right[400],full,pending;
uint32_t kvideo_dbg_active,kvideo_dbg_calls,kvideo_dbg_copies;
void kvideo_reset(const volatile uint8_t *screen)
{unsigned y;front=screen;card=0;shadow=shadow_buf;full=0;pending=0;for(y=0;y<400;y++){left[y]=40;right[y]=0;}}
int kvideo_is_vofa(void){return card!=0;}
const void *kvideo_front(void){return (const void *)front;}
int kvideo_activate_vofa(volatile uint8_t *vram)
{
 /* static shadow buffer - kmem_alloc here failed at boot and left
    card==0, which made every flush a no-op (frozen screen). */
 card=vram;full=1;kvideo_dbg_active=1u;return 1;
}
void kvideo_select(const volatile uint8_t *screen)
{
 if(front==screen)return;
 front=screen;
 if(card)full=1;else kvideo_shifter_base(screen);
}
void kvideo_damage(int32_t x,int32_t y,int32_t w,int32_t h)
{
 unsigned first,last,end;
 if(!card||w<=0||h<=0||x>=640||y>=400)return;
 if(x<0){if(x<=-w)return;w+=x;x=0;}
 if(y<0){if(y<=-h)return;h+=y;y=0;}
 if(w>640-x)w=640-x;
 if(h>400-y)h=400-y;
 pending=1;first=(unsigned)x/16;last=((unsigned)(x+w)+15)/16;end=(unsigned)(y+h);
 for(; (unsigned)y<end;y++){if(first<left[y])left[y]=(uint8_t)first;if(last>right[y])right[y]=(uint8_t)last;}
}
void kvideo_flush(void)
{
 unsigned y,x;const volatile uint16_t *src=(const volatile uint16_t *)front;
 ++kvideo_dbg_calls;
 if(!card||!front||(!full&&!pending))return;
 ++kvideo_dbg_copies;
 for(y=0;y<400;y++){
  unsigned begin=full?0:left[y],end=full?40:right[y];
  for(x=begin;x<end;x++){
   unsigned offset=y*40+x;uint16_t value=src[offset];
   if(full||shadow[offset]!=value){
    /* VOFA is byte-addressed. Explicit byte stores preserve pixel order
     * without relying on adapter word swapping or VGA latch reads. */
    card[offset*2]=(uint8_t)(value>>8);card[offset*2+1]=(uint8_t)value;
    shadow[offset]=value;
   }
  }
  left[y]=40;right[y]=0;
 }
 full=0;pending=0;
}
