#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../system/vofa_et4000.c"
static uint8_t memory[VOFA_MAX_BYTES],sregs[256],cregs[256],gregs[256],areg[32];
static unsigned si,ci,gi,ai,flip,clock_calls,reads,writes,accesses,fail_at;
static int absent,divide,stopped,bad_vram,bad_clock,saw73,saw53;
static int rd(uint32_t a,uint8_t *v){
 reads++;if(++accesses==fail_at||absent)return 0;
 if(a>=VOFA_MEM&&a<VOFA_MEM+VOFA_MAX_BYTES){*v=memory[a-VOFA_MEM];if(bad_vram&&a==VOFA_MEM+1)*v^=1;return 1;}
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
 if(a>=VOFA_MEM&&a<VOFA_MEM+VOFA_MAX_BYTES){memory[a-VOFA_MEM]=(uint8_t)v;return 1;}
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
static const vofa_io_t bus={rd,wr,delay,ticks,0,0};
/* Fast path: a bus that offers the bulk fill.  It records what the driver asked
   for and runs the real hardware sequence into a local buffer, so the wide
   stores are exercised without ever dereferencing the physical aperture. */
static uint32_t fill_sink[8000];static unsigned fill_calls;static uint32_t fill_address,fill_bytes;static uint8_t fill_value;
static void tracked_fill(uintptr_t address,uint32_t bytes,uint8_t value)
{
 fill_calls++;fill_address=(uint32_t)address;fill_bytes=bytes;fill_value=value;
 assert(bytes<=sizeof fill_sink);
 vofa_et4000_vram_fill((uintptr_t)fill_sink,bytes,value);
}
static const vofa_io_t bus_fill={rd,wr,delay,ticks,0,tracked_fill};
static void reset(void){
 memset(memory,0xc9,sizeof memory);memset(sregs,0,sizeof sregs);memset(cregs,0,sizeof cregs);
 si=ci=gi=ai=flip=clock_calls=reads=writes=accesses=fail_at=0;
 absent=divide=stopped=bad_vram=bad_clock=saw73=saw53=0;
}
int main(void){
 reset();absent=1;assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_ABSENT&&writes==0);
 reset();assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(saw73&&saw53&&cregs[0x36]==0x53&&sregs[2]==1&&sregs[4]==6);
 assert(cregs[0x12]==0x8f&&cregs[0x13]==0x28&&(cregs[0x07]&0x02)&&!(cregs[0x07]&0x40)&&gregs[8]==255&&areg[0x12]==1);
 for(unsigned i=0;i<32000;i++)assert(memory[i]==0);  /* mode 640 clears the 640x400 aperture */
 /* 800x600: row offset 100 bytes / 2 = 0x32, 600 display lines (0x57 + bit9). */
 reset();assert(vofa_et4000_init(&bus,VOFA_MODE_800)==VOFA_OK);
 assert(cregs[0x13]==0x32&&cregs[0x12]==0x57&&(cregs[0x07]&0x40)&&!(cregs[0x07]&0x02));
 for(unsigned i=0;i<60000;i++)assert(memory[i]==0);
 /* 1024x768: offset 128 bytes / 2 = 0x40, 768 display lines (0xff + bit9). */
 reset();assert(vofa_et4000_init(&bus,VOFA_MODE_1024)==VOFA_OK);
 assert(cregs[0x13]==0x40&&cregs[0x12]==0xff&&(cregs[0x07]&0x40)&&!(cregs[0x07]&0x02));
 for(unsigned i=0;i<98304;i++)assert(memory[i]==0);
 /* Live switch: CRTC-only reprogram must adopt the new offset/geometry. */
 reset();assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_800)==VOFA_OK);
 assert(cregs[0x13]==0x32&&cregs[0x12]==0x57&&(cregs[0x07]&0x40)&&!(cregs[0x07]&0x02));
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_1024)==VOFA_OK);
 assert(cregs[0x13]==0x40&&cregs[0x12]==0xff);
 /* Back to 640: a true 640x400 card mode (no 480-line band to leave stale). */
 memory[31999]=0x5a;memory[0]=0x5a;
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(cregs[0x13]==0x28&&cregs[0x12]==0x8f&&(cregs[0x07]&0x02)&&!(cregs[0x07]&0x40));
 assert(memory[31999]==0x5a&&memory[0]==0x5a);  /* visible rows untouched */
 /* 640x480x256 (full-screen games): chain-4 offset 640/8 = 0x50, 480 display
    lines (0xdf + bit 9 clear, bit 8 in overflow bit 1), linear 8 bpp sequencer,
    and the 80 tail rows of the mono 640x400 view are not touched by the switch. */
 assert(vofa_et4000_mode_index(640,480)==VOFA_MODE_640_8);
 assert(vofa_et4000_mode(VOFA_MODE_640_8)->width==640&&vofa_et4000_mode(VOFA_MODE_640_8)->height==480&&
        vofa_et4000_mode(VOFA_MODE_640_8)->bytes==307200UL);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640_8)==VOFA_OK);
 assert(cregs[0x13]==0x50&&cregs[0x12]==0xdf&&cregs[0x06]==0x0b&&(cregs[0x07]&0x02)&&!(cregs[0x07]&0x40));
 assert(sregs[4]==0x0e&&gregs[5]==0x40&&areg[0x10]==0x41);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(cregs[0x13]==0x28&&sregs[4]==6&&gregs[5]==0);
 /* 640x480 z 16 KOLORAMI: 4 bpp PLANARNY.  Ten sam timing VGA co 640x480x8,
    ale inny format piksela: cztery plany po jednym bicie, 4 bajty na grupe
    8 pikseli.  Wspolna konwencja z modelem karty w Hatari (src/vofa.c):
      CRTC 0x13 = szerokosc/8, uklad base + (x/8)*4 + p, bit 7-(x&7),
      piksel = b0 | b1<<1 | b2<<2 | b3<<3.
    WYROZNIK: gregs[6] bit 1 (chain odd/even).  Mono ma tam 0x01, wiec sam
    sekwencer 0x06 NIE rozroznia tych dwoch trybow - i dlatego Hatari sprawdza
    dodatkowo ten bit.  Gdyby ktos kiedys zmienil jedna strone bez drugiej,
    ten test to wykryje. */
 assert(vofa_et4000_mode(VOFA_MODE_640_4)->width==640&&
        vofa_et4000_mode(VOFA_MODE_640_4)->height==480&&
        vofa_et4000_mode(VOFA_MODE_640_4)->bytes==153600UL);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640_4)==VOFA_OK);
 assert(cregs[0x13]==0x50&&cregs[0x12]==0xdf&&(cregs[0x07]&0x02)&&!(cregs[0x07]&0x40));
 assert(sregs[4]==0x06&&sregs[2]==0x0f);   /* odd/even + wszystkie 4 plany */
 assert(gregs[6]==0x02&&gregs[8]==255);    /* WYROZNIK 4 bpp + pelna maska */
 assert(areg[0x10]==0x01&&areg[0x12]==0x0f); /* grafika, 4 plany wlaczone */
 assert(vofa_et4000_init(&bus,VOFA_MODE_640_4)==VOFA_OK);
 assert(sregs[4]==0x06&&sregs[2]==0x0f&&gregs[6]==0x02&&areg[0x12]==0x0f);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(cregs[0x13]==0x28&&sregs[4]==6&&gregs[6]==1&&areg[0x12]==1);
 /* 800x600 z 65 536 KOLORAMI: 16 bpp, 5:6:5 pakowane, 2 bajty na piksel,
    liniowo (bez Chain 4 i bez odd/even).  Ten sam raster co 800x600x8.
    WYROZNIK: gregs[6] == 0x08 - mono ma 0x01, 4 bpp 0x02, 8 bpp 0x40.
    Model karty w Hatari (src/vofa.c) sprawdza DOKLADNIE ten rejestr, wiec
    zmiana jednej strony bez drugiej zostanie wykryta. */
 assert(vofa_et4000_mode(VOFA_MODE_800_16)->width==800&&
        vofa_et4000_mode(VOFA_MODE_800_16)->height==600&&
        vofa_et4000_mode(VOFA_MODE_800_16)->bytes==960000UL);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_800_16)==VOFA_OK);
 /* Offset liczy sie w jednostkach 8-bajtowych: 800*2/8 = 200 = 0xC8
    (8 bpp mialo 800/8 = 0x64, mono 800/2 = 0x32). */
 assert(cregs[0x13]==0xc8&&cregs[0x12]==0x57);
 assert(sregs[4]==0x0e&&gregs[6]==0x08&&gregs[8]==255);
 assert(vofa_et4000_init(&bus,VOFA_MODE_800_16)==VOFA_OK);
 assert(sregs[4]==0x0e&&gregs[6]==0x08);
 assert(vofa_et4000_set_mode(&bus,VOFA_MODE_640)==VOFA_OK);
 assert(sregs[4]==6&&gregs[6]==1);
 /* Out-of-range mode falls back to 640 instead of writing garbage. */
 assert(vofa_et4000_mode_index(800,600)==VOFA_MODE_800);
 assert(vofa_et4000_mode_index(1024,768)==VOFA_MODE_1024);
 assert(vofa_et4000_mode_index(123,456)==-1);
 assert(vofa_et4000_mode(99)->width==640);
 reset();divide=1;assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_OK&&!divide&&sregs[7]==0xa4);
 reset();stopped=1;assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_CLOCK_ERROR);
 reset();bad_clock=1;assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_CLOCK_ERROR);
 reset();bad_vram=1;assert(vofa_et4000_init(&bus,VOFA_MODE_640)==VOFA_MEMORY_ERROR);
 /* Failed register reads/writes must stop boot, never claim ready. */
 for(unsigned failure=1;failure<450;failure++){
  reset();fail_at=failure;assert(vofa_et4000_init(&bus,VOFA_MODE_640)!=VOFA_OK);
 }
 reset();fail_at=31000;assert(vofa_et4000_init(&bus,VOFA_MODE_640)!=VOFA_OK);
 /* The driver must never touch the aperture itself.  With fill == NULL every
    clear byte went through the bus callbacks (fault injection at access 31000
    above lands inside that loop); with fill installed the driver must route the
    whole clear through the bus instead, and the hardware sequence must clear
    exactly the requested range, byte tail included. */
 reset();fill_calls=0;fill_address=0;fill_bytes=0;fill_value=0xff;memset(fill_sink,0xa5,sizeof fill_sink);
 assert(vofa_et4000_init(&bus_fill,VOFA_MODE_640)==VOFA_OK);
 assert(fill_calls==1&&fill_address==VOFA_MEM&&fill_bytes==32000u&&fill_value==0);
 {unsigned k;for(k=0;k<32000;k++)assert(((uint8_t *)fill_sink)[k]==0);}
 /* Tail: 10 bytes is not a multiple of four, and byte 10 must stay untouched. */
 reset();memset(fill_sink,0xa5,sizeof fill_sink);
 vofa_et4000_vram_fill((uintptr_t)fill_sink,10u,0x3cu);
 {unsigned k;for(k=0;k<10u;k++)assert(((uint8_t *)fill_sink)[k]==0x3cu);
  assert(((uint8_t *)fill_sink)[10]==0xa5);}
 puts("VOFA PASS: absent/no writes, VGA mode, VOFA 8-bit IO, divider, timer/stuck sync, VRAM lanes, injected bus faults, generic and wide-store aperture clear");
 return 0;
}
