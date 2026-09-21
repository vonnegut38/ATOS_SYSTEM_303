#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../system/kernel_video.c"
static uint16_t storage[16000];static int allocated,fail_alloc,shifter_calls;
void *kmem_alloc(uint32_t bytes,uint32_t align,uint16_t owner,uint16_t flags,int region)
{(void)align;(void)owner;(void)flags;(void)region;assert(bytes==32000);if(fail_alloc)return 0;allocated=1;return storage;}
int kmem_pin(void *p,uint16_t owner){(void)owner;assert(p==storage);return 1;}
int kmem_free(void *p,uint16_t owner){(void)p;(void)owner;return 1;}
void kvideo_shifter_base(const volatile uint8_t *p){assert(p);shifter_calls++;}
int main(void){
 static uint16_t a[16000],b[16000];static uint8_t vram[38402];
 kvideo_reset((uint8_t *)a);kvideo_select((uint8_t *)b);assert(shifter_calls==1&&!kvideo_is_vofa());
 kvideo_reset((uint8_t *)a);fail_alloc=1;assert(!kvideo_activate_vofa(vram+1));fail_alloc=0;
 memset(vram,0xa5,sizeof vram);assert(kvideo_activate_vofa(vram+1)&&allocated);
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
 puts("Video output PASS: Shifter selection, allocation fallback, big-endian pixels, changed words, idle, clipping, edges and session buffer switches");
 return 0;
}
