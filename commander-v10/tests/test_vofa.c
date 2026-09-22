#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/vofa_et4000.c"
static uint8_t memory[VOFA_BYTES],sregs[256],cregs[256],gregs[256],areg[32];
static unsigned si,ci,gi,ai,flip,clock_calls,reads,writes,accesses,fail_at;
static int absent,divide,stopped,bad_vram,bad_clock,saw73,saw53;
static int rd(uint32_t a,uint8_t *v){
 reads++;if(++accesses==fail_at||absent)return 0;
 if(a>=VOFA_MEM&&a<VOFA_MEM+VOFA_BYTES){*v=memory[a-VOFA_MEM];if(bad_vram&&a==VOFA_MEM+1)*v^=1;return 1;}
 if(a==VOFA_MEM+0x80000){*v=0;return 1;}
 assert(a>=VOFA_IO&&a<VOFA_IO+0x400);
 switch(a-VOFA_IO){
 case 0x3c0:*v=(uint8_t)ai;break;
 case 0x3da:flip=0;*v=bad_clock?0:((clock_calls%(divide?166:83))<8?8:0);break;
 case 0x3d5:*v=cregs[ci];break;
 default:*v=0;break;
 }return 1;
}
static int wr(uint32_t a,uint32_t v){
 writes++;if(++accesses==fail_at)return 0;
 if(a>=VOFA_MEM&&a<VOFA_MEM+VOFA_BYTES){memory[a-VOFA_MEM]=(uint8_t)v;return 1;}
 assert(a>=VOFA_IO&&a<VOFA_IO+0x400);
 switch(a-VOFA_IO){
 case 0x3c4:si=v;break;
 case 0x3c5:sregs[si]=(uint8_t)v;if(si==7&&v==0xa4)divide=0;break;
 case 0x3d4:ci=v;break;
 case 0x3d5:cregs[ci]=(uint8_t)v;if(ci==0x36){assert(v!=0xf3&&v!=0xd3);saw73|=v==0x73;saw53|=v==0x53;}break;
 case 0x3ce:gi=v;break;
 case 0x3cf:gregs[gi]=(uint8_t)v;break;
 case 0x3c0:if(!flip)ai=v&31;else areg[ai]=(uint8_t)v;flip^=1;break;
 default:break;
 }return 1;
}
static void delay(void){}
static uint32_t ticks(void){return stopped?0:clock_calls++/100;}
static const vofa_io_t bus={rd,wr,delay,ticks};
static void reset(void){
 memset(memory,0xc9,sizeof memory);memset(sregs,0,sizeof sregs);memset(cregs,0,sizeof cregs);
 si=ci=gi=ai=flip=clock_calls=reads=writes=accesses=fail_at=0;
 absent=divide=stopped=bad_vram=bad_clock=saw73=saw53=0;
}
int main(void){
 reset();absent=1;assert(vofa_et4000_init(&bus)==VOFA_ABSENT&&writes==0);
 reset();assert(vofa_et4000_init(&bus)==VOFA_OK);
 assert(saw73&&saw53&&cregs[0x36]==0x53&&sregs[2]==1&&sregs[4]==6);
 assert(cregs[0x12]==0xdf&&cregs[0x13]==0x28&&gregs[8]==255&&areg[0x12]==1);
 for(unsigned i=0;i<sizeof memory;i++)assert(memory[i]==0);
 reset();divide=1;assert(vofa_et4000_init(&bus)==VOFA_OK&&!divide&&sregs[7]==0xa4);
 reset();stopped=1;assert(vofa_et4000_init(&bus)==VOFA_CLOCK_ERROR);
 reset();bad_clock=1;assert(vofa_et4000_init(&bus)==VOFA_CLOCK_ERROR);
 reset();bad_vram=1;assert(vofa_et4000_init(&bus)==VOFA_MEMORY_ERROR);
 /* Failed register reads/writes must stop boot, never claim ready. */
 for(unsigned failure=1;failure<450;failure++){
  reset();fail_at=failure;assert(vofa_et4000_init(&bus)!=VOFA_OK);
 }
 reset();fail_at=38000;assert(vofa_et4000_init(&bus)!=VOFA_OK);
 puts("VOFA PASS: absent/no writes, VGA mode, VOFA 8-bit IO, divider, timer/stuck sync, VRAM lanes and injected bus faults");
 return 0;
}
