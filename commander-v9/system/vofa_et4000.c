/* SPDX-License-Identifier: GPL-2.0-or-later
 * VOFA-only adaptation of EmuTOS bios/nova.c (Christian Zietz and the
 * EmuTOS development team, Copyright 2018-2025). See licenses/emutos-NOTICE.
 * VGA tables and ET4000 register sequence are from that implementation.
 * Our bus-fault callbacks, timeouts, readback and palette are local changes.
 */
#include "vofa_et4000.h"
#define RD(p,v) do{if(!io->read(VOFA_IO+(p),&(v)))return VOFA_IO_ERROR;}while(0)
#define WR(p,v) do{if(!io->write(VOFA_IO+(p),(v)))return VOFA_IO_ERROR;io->delay();}while(0)
#define IDX(p,r,v) do{WR(p,r);WR((p)+1,v);}while(0)
static const uint8_t seq[]={1,1,0,6};
static const uint8_t extseq[]={0,0xf4,3};
static const uint8_t crtc[]={0x5f,0x4f,0x50,0x82,0x54,0x80,0x0b,0x3e,0,0x40,0,0,0,0,0,
 0,0xea,0x0c,0xdf,0x28,0,0xe7,4,0xc3,0xff};
_Static_assert(sizeof(crtc)==25,"VGA CRTC registers 00..18");
static const uint8_t attr[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1,0xff,1,0x10,0,5,0};
static const uint8_t gdc[]={0,0,0,0,0,0,1,0x0f,0xff};
static int count_frames(const vofa_io_t *io)
{
 uint32_t start=io->ticks(),tries;uint8_t status,previous=0;int frames=0;
 for(tries=0;tries<200000UL;tries++){
  RD(0x3da,status);
  if((status&8)&&!previous)frames++;
  previous=status&8;
  if((uint32_t)(io->ticks()-start)>=13u)return frames; /* 260 ms at 50 Hz */
 }
 return VOFA_CLOCK_ERROR;
}
int vofa_et4000_init(const vofa_io_t *io)
{
 uint8_t value,old;unsigned i;int frames;
 /* Match the VOFA decode, excluding the common Viking mapping. No writes
  * occur until all three read-only presence probes succeed. */
 if(!io->read(VOFA_IO+0x3c3,&value)||!io->read(VOFA_MEM,&value)||
    !io->read(VOFA_MEM+0x80000UL,&value))return VOFA_ABSENT;
 WR(0x3c3,1);WR(0x3c2,1);
 RD(0x3da,value);WR(0x3c0,0x0a);WR(0x3c0,0x55);RD(0x3c0,value);
 if(value!=0x0a)return VOFA_ABSENT;
 WR(0x3c0,0x20);
 WR(0x3bf,3);WR(0x3d8,0xa0);
 IDX(0x3d4,0x34,0);IDX(0x3d4,0x31,0);
 IDX(0x3c4,2,0x0f);IDX(0x3c4,4,0x0c);
 IDX(0x3ce,1,0);IDX(0x3ce,3,0);IDX(0x3ce,6,5);IDX(0x3ce,8,0xff);
 WR(0x3cd,0);IDX(0x3d4,0x32,0x28);
 IDX(0x3d4,0x36,0x73); /* VOFA: 8-bit IO, not Nova's 0xf3 */
 IDX(0x3d4,0x37,0x0f);
 RD(0x3c8,value);io->delay();
 for(i=0;i<4;i++){RD(0x3c6,value);io->delay();}
 WR(0x3c6,0);RD(0x3c8,value); /* HiColor DAC -> indexed palette */
 IDX(0x3c4,0,1);for(i=0;i<64;i++)io->delay();IDX(0x3c4,0,3);
 WR(0x3bf,3);WR(0x3d8,0xa0);
 for(i=0;i<sizeof(seq);i++){IDX(0x3c4,i+1,seq[i]);}
 for(i=0;i<sizeof(extseq);i++){IDX(0x3c4,i+6,extseq[i]);}
 WR(0x3c2,0xe3);IDX(0x3d4,0x11,0);
 for(i=0;i<sizeof(crtc);i++){IDX(0x3d4,i,crtc[i]);}
 for(i=0;i<3;i++){IDX(0x3d4,0x33+i,0);}
 WR(0x3d4,0x11);RD(0x3d5,value);IDX(0x3d4,0x11,value|0x80);
 RD(0x3da,value);
 for(i=0;i<sizeof(attr);i++){WR(0x3c0,i);WR(0x3c0,attr[i]);}
 WR(0x3c0,0x20);
 for(i=0;i<sizeof(gdc);i++){IDX(0x3ce,i,gdc[i]);}
 IDX(0x3d4,0x36,0x53); /* VOFA mono, 8-bit IO */
 IDX(0x3c4,1,0x21);WR(0x3c6,0xff);WR(0x3c8,0);
 /* Kernel convention: bit 0 black, bit 1 white; overscan black. */
 for(i=0;i<6;i++){WR(0x3c9,i<3?0:63);}
 WR(0x3c8,255);for(i=0;i<3;i++){WR(0x3c9,0);}
 IDX(0x3c4,1,1);
 frames=count_frames(io);if(frames<0)return frames;
 if(frames<12){IDX(0x3c4,7,0xa4);frames=count_frames(io);}
 if(frames<12||frames>19)return VOFA_CLOCK_ERROR;
 /* Exercise both byte lanes and the end of the displayed aperture. Restore
  * each sampled byte before testing the next, then clear the whole display. */
 for(i=0;i<4;i++){
  uint32_t a=VOFA_MEM+(i<2?i:VOFA_BYTES-4+i);
  if(!io->read(a,&old))return VOFA_MEMORY_ERROR;
  if(!io->write(a,0x55)||!io->read(a,&value)||value!=0x55){io->write(a,old);return VOFA_MEMORY_ERROR;}
  if(!io->write(a,0xaa)||!io->read(a,&value)||value!=0xaa){io->write(a,old);return VOFA_MEMORY_ERROR;}
  if(!io->write(a,old))return VOFA_MEMORY_ERROR;
 }
 for(i=0;i<VOFA_BYTES;i++)if(!io->write(VOFA_MEM+i,0))return VOFA_MEMORY_ERROR;
 return VOFA_OK;
}
