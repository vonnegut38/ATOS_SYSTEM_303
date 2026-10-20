#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel_pcm.h"
#include "kernel_audio.h"
#include "psg.h"
#include "irq.h"
uint8_t kpcm_host_regs[256],psg_host_regs[8],psg_host_log[32][2];
uint32_t kpcm_host_vector=0x12345678;
volatile uint32_t psg_host_locks,psg_host_log_n,kaudio_host_locks;
uint32_t kaudio_host_now(void){return 0;}
void kpcm_irq(void){}
static unsigned allocations;
void *kmem_alloc(uint32_t n,uint32_t align,uint16_t owner,uint16_t flags,int tt){(void)align;(void)owner;(void)flags;(void)tt;allocations++;return malloc(n);}
int kmem_free(void *p,uint16_t owner){(void)owner;assert(allocations);allocations--;free(p);return 1;}
int main(void){
 uint8_t b[512];uint32_t s[8],i,port;
 for(i=0;i<512;i++)b[i]=(uint8_t)i;
 kpcm_host_regs[0x17]=0x40;psg_mixer_set_directions(0x40);psg_port_a_modify(0xff,0x55);port=psg_shadow(14);
 assert(kaudio_claim(7,0)==0);assert(kpcm_control(9,0,0)<0);kaudio_release(7);
 kpcm_host_regs[0x19]=1;assert(kpcm_control(9,0,0)<0);assert(kaudio_owner()==0);kpcm_host_regs[0x19]=0;
 assert(kpcm_control(9,0,0)==12288);assert(kpcm_control(9,0,0)==12288);
 assert(kpcm_control(10,0,0)<0);assert(kpcm_submit(10,b,512)<0);
 assert(kpcm_control(9,0,1)<0);assert(kaudio_tone(9,0,100,15)<0);
 for(i=0;i<16;i++)assert(kpcm_submit(9,b,512)==512);
 assert(kpcm_submit(9,b,1)==0);kpcm_state(s);assert(s[2]==8192&&s[3]==0);
 assert(kpcm_control(9,0,1)==0);assert(kpcm_active&&kpcm_host_regs[0x19]==1);
 kpcm_read=8000;assert(kpcm_submit(9,b,512)==512);assert(kpcm_ring[0]==0); /* wrap */
 assert(kpcm_control(9,0,2)==0);kpcm_state(s);assert(!s[1]&&s[2]==704);
 assert(kpcm_control(9,0,1)==0);kaudio_tos_teardown();assert(kpcm_owner==9&&kpcm_active);
 assert(kaudio_tos_reg_write(8,15)<0);kaudio_release(10);assert(kpcm_active);
 kaudio_release(9);assert(!kpcm_owner&&!kpcm_active&&!kaudio_owner());
 assert(kpcm_host_vector==0x12345678&&!irq_handler_count(IRQ_SRC_MFP_A));
 assert((psg_mixer_shadow()&0xc0)==0x40&&psg_shadow(14)==port);
 assert(kpcm_control(10,0,0)==12288);assert(kpcm_read==0&&kpcm_write==0);kaudio_release(10);
 assert(kpcm_control(11,0,0)==12288);assert(kpcm_clip_create(12,1024)<0);
 assert(kpcm_clip_create(11,9u*1024u*1024u)<0);
 assert(kpcm_clip_create(11,1024)==0&&allocations==1);
 assert(kpcm_clip_create(11,1024)<0);
 assert(kpcm_submit(11,b,512)==512);assert(kpcm_control(11,0,1)<0);
 assert(kpcm_submit(11,b,512)==512);assert(kpcm_control(11,0,1)==0);
 assert(kpcm_submit(11,b,512)<0);assert(kpcm_control(11,0,2)==0);
 assert(kpcm_submit(11,b,512)==0); /* complete clips remain immutable */
 kaudio_tos_teardown();assert(kpcm_owner==11&&allocations==1);
 kaudio_release(11);assert(!allocations&&!kpcm_owner);
 puts("PCM: ownership, timer reservation, wrap/full, pause/resume, TOS isolation, teardown PASS");
}
