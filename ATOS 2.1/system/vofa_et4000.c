/* SPDX-License-Identifier: GPL-2.0-or-later
 * VOFA-only adaptation of EmuTOS bios/nova.c (Christian Zietz and the
 * EmuTOS development team, Copyright 2018-2025). See licenses/emutos-NOTICE.
 * VGA tables and ET4000 register sequence are from that implementation.
 * Our bus-fault callbacks, timeouts, readback, palette and the extra
 * 800x600 / 1024x768 CRTC tables are local changes.
 */
#include "vofa_et4000.h"
#define RD(p,v) do{if(!io->read(VOFA_IO+(p),&(v)))return VOFA_IO_ERROR;}while(0)
#define WR(p,v) do{if(!io->write(VOFA_IO+(p),(v)))return VOFA_IO_ERROR;io->delay();}while(0)
#define IDX(p,r,v) do{WR(p,r);WR((p)+1,v);}while(0)
static const uint8_t seq[]={1,1,0,6};
static const uint8_t extseq[]={0,0xf4,3};
/* 640x400 @ 70 Hz (25.175 MHz).  The H timing is the licensed EmuTOS/Nova
 * VGA set (regs 00..05, 13 are identical to vga_CRTC_0_0x18); only the vertical
 * timing differs, because the ATOS view is a true 640x400 - the original
 * EmuTOS table scans 480 lines, which left an 80-line black band below the
 * 80x25 text/desktop.  VTotal 449 keeps the standard 640x400@70 timing. */
static const uint8_t crtc[]={0x5f,0x4f,0x50,0x82,0x54,0x80,0xbf,0x1f,0,0x40,0,0,0,0,0,
 0,0x9c,0x0e,0x8f,0x28,0,0x96,0xb9,0xc3,0xff};
/* 800x600 @ 60 Hz (VESA 1024x628 line time, 40 MHz pixel clock).  Every
 * horizontal value is in 8-dot character clocks, exactly as for 640x400;
 * 0x13 is the row offset in 16-bit CRTC units, here 100 bytes / 2. */
static const uint8_t crtc_800[]={0x7f,0x63,0x64,0x82,0x68,0x00,0x72,0xf0,0,0x40,0,0,0,0,0,
 0,0x62,0x04,0x57,0x32,0,0x60,0x6a,0xc3,0xff};
/* 1024x768 @ 60 Hz (VESA 1344x806 line time, 65 MHz pixel clock). */
static const uint8_t crtc_1024[]={0xa3,0x7f,0x80,0x86,0x84,0x84,0x24,0xfd,0,0x40,0,0,0,0,0,
 0,0x0a,0x0c,0xff,0x40,0,0x08,0x1c,0xc3,0xff};
/* 800x600 at 8 bits per pixel (Chain-4 linear).  The CRTC row offset counts
 * 8-byte units in this mode, so 800 pixels / 8 = 100 = 0x64 in reg 0x13 (in
 * mono the same register holds 100 bytes / 2 = 0x32). */
static const uint8_t crtc_800_8[]={0x7f,0x63,0x64,0x82,0x68,0x00,0x72,0xf0,0,0x40,0,0,0,0,0,
 0,0x62,0x04,0x57,0x64,0,0x60,0x6a,0xc3,0xff};
/* 800x600 z 65 536 kolorami (16 bpp): ten sam raster co 800x600x8, ale piksel
   ma DWA bajty, wiec linia to 1600 bajtow.  CRTC 0x13 liczy jednostki
   8-bajtowe: 1600/8 = 200 = 0xC8 (przy 8 bpp bylo 800/8 = 0x64).  Model karty
   w Hatari wylicza z tego szerokosc jako crtc[0x13]*4 = 800 - zgodnie. */
static const uint8_t crtc_800_16[]={0x7f,0x63,0x64,0x82,0x68,0x00,0x72,0xf0,0,0x40,0,0,0,0,0,
 0,0x62,0x04,0x57,0xc8,0,0x60,0x6a,0xc3,0xff};
/* 1024x768 at 8 bpp: the mono 1024x768 timing with the Chain-4 row offset in
   8-byte units - 1024 pixels / 8 = 128 = 0x80 (mono holds 128 bytes / 2 = 0x40). */
static const uint8_t crtc_1024_8[]={0xa3,0x7f,0x80,0x86,0x84,0x84,0x24,0xfd,0,0x40,0,0,0,0,0,
 0,0x0a,0x0c,0xff,0x80,0,0x08,0x1c,0xc3,0xff};
/* 1024x768 z 16 kolorami (4 bpp PLANARNY): ten sam timing co 1024x768x8, ale
   offset CRTC (rejestr 0x13, indeks 19) = WIDTH/8 = 128 = 0x80, tak samo jak
   1024x768x8.  Uwaga: NIE pitch/8 - to byl blad, ktory znalazl test automatyczny:
   z 0x40 (512/8) Hatari liczyl szerokosc jako crtc13*8 = 512 i karta nie
   przejmowala ekranu (drive=0).  Dla 640x480x4 jest 0x50 = 640/8, dla
   800x600x16 0xC8 = 1600/8 - regula to width/8.
   VRAM: 512 * 768 = 393 216 bajtow. */
static const uint8_t crtc_1024_4[]={0xa3,0x7f,0x80,0x86,0x84,0x84,0x24,0xfd,0,0x40,0,0,0,0,0,
 0,0x0a,0x0c,0xff,0x80,0,0x08,0x1c,0xc3,0xff};
_Static_assert(sizeof(crtc_1024_4)==25,"1024x768x4 CRTC registers 00..18");
/* 640x480 at 8 bpp (Chain-4 linear), the standard VGA 640x480@60 timing on the
 * 25.175 MHz clock: HTotal 800, VTotal 525, negative/negative sync (0x3C2=0xE3,
 * which the driver writes for every mode).  The row offset counts 8-byte units
 * in the chain-4 mode: 640 / 8 = 80 = 0x50.  Used by full-screen games that
 * double a 256x224 / 320x240 frame. */
static const uint8_t crtc_640_8[]={0x5f,0x4f,0x50,0x82,0x54,0x80,0x0b,0x3e,0,0x40,0,0,0,0,0,
 0,0xea,0x0c,0xdf,0x50,0,0xe7,0x04,0xc3,0xff};
_Static_assert(sizeof(crtc_640_8)==25,"640x480x8 CRTC registers 00..18");
_Static_assert(sizeof(crtc)==25,"VGA CRTC registers 00..18");
_Static_assert(sizeof(crtc_800_8)==25,"800x600x8 CRTC registers 00..18");
_Static_assert(sizeof(crtc_800)==25,"800x600 CRTC registers 00..18");
_Static_assert(sizeof(crtc_1024)==25,"1024x768 CRTC registers 00..18");
_Static_assert(sizeof(crtc_1024_8)==25,"1024x768x8 CRTC registers 00..18");
static const uint8_t attr[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,1,0xff,1,0x10,0,5,0};
static const uint8_t gdc[]={0,0,0,0,0,0,1,0x0f,0xff};
/* 256-colour (Chain-4) variants: linear 8-bit buffer (sequencer 0x04 = 0x0E),
   graphics controller in 256-colour shift with the 64 KB mapping (0x05 = 0x40,
   0x06 = 0x05) and the attribute controller in graphics mode with all planes
   enabled (0x10 = 0x41, 0x12 = 0x0F). */
static const uint8_t seq8[]={1,1,0,0x0e};
static const uint8_t gdc8[]={0,0,0,0,0,0x40,0x05,0x0f,0xff};
/* 16 kolorow, 4 bpp PLANARNY.  Sekwencer: mapa planow 0x0F (przy odd/even i tak
   ignorowana, ale wyrazamy intencje), tryb pamieci 0x06 = odd/even WLACZONY,
   Chain 4 WYLACZONY - ta sama wartosc co tryb mono, dlatego format rozpoznaje
   sie po GDC[6].  GDC: tryb 0, mapowanie 0x02 (bit 1 = chain odd/even, czego
   mono nie ustawia), color-dont-care 0x0F, bit mask 0xFF.  Atrybut: ta sama
   tablica co mono - 16 wpisow palety i wszystkie 4 plany wlaczone. */
/* Uwaga na konwencje: element k tablicy trafia do REJESTRU k+1 sekwencera
   (petla: IDX(0x3c4,i+1,sq[i])).  Dlatego mapa planow 0x0F jest DRUGIM
   elementem.  Poprzednia wersja {1,1,0x0f,0x06} wpisywala 0x0f do rejestru 3
   (char map), a do mapy planow dawala 1 - test hostowy to wykryl. */
static const uint8_t seq4[]={1,0x0f,0,0x06};
static const uint8_t gdc4[]={0,0,0,0,0,0,0x02,0x0f,0xff};
/* Atrybut dla 4 bpp: tryb graficzny (0x01, bez bitu 8-bit color ktory ma 8 bpp)
   i WLACZONE WSZYSTKIE CZTERY PLANY (0x0F).  Tablica mono ma tam 1, czyli
   tylko plan 0 - dlatego nie mozna jej uzyc dla 16 kolorow. */
static const uint8_t attr4[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0x01,0xff,0x0f,0x10,0,5,0};
/* 16 bpp HICOLOR: liniowy bufor (bez Chain 4 i bez odd/even) i WYROZNIK
   gdc[6] = 0x08.  Paleta nie jest uzywana - kolor jest w pikselu. */
static const uint8_t seq16[]={1,1,0,0x0e};
static const uint8_t gdc16[]={0,0,0,0,0,0,0x08,0x0f,0xff};
static const uint8_t attr8[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0x41,0xff,0x0f,0x10,0,5,0};
/* One entry per mode.  card_height is the CRT's visible line count and now
 * equals the compositor height for every mode (640x400, 800x600, 1024x768), so
 * the whole scanned screen is always painted - no unused band.  surface_bytes
 * is width/8*height, the kernel shadow. */
typedef struct {
 uint16_t width,height,card_height;
 uint32_t surface_bytes;
} vofa_mode_t;
/* NOTE: this table must not store POINTERS to the CRTC arrays.  VOFA.SYS is
   linked at 0 and loaded at an arbitrary base without relocation, so an
   absolute pointer in .rodata/.data would still point at the link-time address
   (the BIOS/low RAM) and the CRTC would be programmed with garbage.  The table
   is selected at runtime by crtc_for() below, which emits a PC-relative lea. */
static const vofa_mode_t mode_table[VOFA_MODE_COUNT]={
 {640u,400u,400u,32000UL},
 {800u,600u,600u,60000UL},
 {1024u,768u,768u,98304UL},
 /* 800x600 with 8 bits per pixel: 480000 bytes of VRAM. */
 {800u,600u,600u,480000UL},
 /* 1024x768 with 8 bits per pixel: 786432 bytes of VRAM. */
 {1024u,768u,768u,786432UL},
 /* 640x480 with 8 bits per pixel: 307200 bytes of VRAM. */
 {640u,480u,480u,307200UL},
 /* 640x480 z 16 kolorami (4 bpp PLANARNY): 4 plany po 80 bajtow na linie
    (640/8) * 480 linii = 153600 bajtow VRAM.  Timing jest ten sam co
    640x480x8 (VGA 25 MHz), rozni sie tylko format piksela i sposob pobierania
    (4 bajty z 4 planow na grupe 8 pikseli). */
 {640u,480u,480u,153600UL},
 /* 800x600 z 65 536 kolorami (16 bpp, 5:6:5 pakowane): 800*2 = 1600 bajtow
    na linie * 600 = 960 000 bajtow VRAM. */
 {800u,600u,600u,960000UL},
 /* 1024x768 z 16 kolorami (4 bpp planarne): (1024/8)*4 = 512 bajtow na linie
    * 768 = 393 216 bajtow VRAM.  Miesci sie w 1 MB karty. */
 {1024u,768u,768u,393216UL}
};
/* ---------------- RAMDAC family ------------------------------------------
   The pixel mask register 0x3C6 is the one register every VGA-compatible
   RAMDAC shares, and the families differ in what they do with it: a plain
   8-bit part stores what is written (00 -> 00, FF -> FF), while the
   Sierra-style 15/24-bit parts alias it or keep only some bits.  Classify on
   that read-back; anything inconclusive falls back to the CIC10487 (15-bit)
   family the VoFa cards carry. */
static uint8_t dac_type=VOFA_DAC_UNKNOWN;
static uint8_t mode_index(uint8_t mode){return mode<VOFA_MODE_COUNT?mode:(uint8_t)VOFA_MODE_640;}
const char *vofa_et4000_dac_name(uint8_t type)
{
 switch(type){
  case VOFA_DAC_8BIT:return "8-BIT (TR9C1710/ADV476/KDA0476)";
  case VOFA_DAC_15BIT:return "15-BIT (CIC10487/SC11486)";
  case VOFA_DAC_24BIT:return "24-BIT (ATT20C491/SC15025)";
  default:return "UNKNOWN";
 }
}
uint8_t vofa_et4000_dac(void){return dac_type;}
int vofa_et4000_dac_detect(const vofa_io_t *io,uint8_t override)
{
 uint8_t a=0,b=0,c=0;
 if(override!=VOFA_DAC_UNKNOWN){dac_type=override;return VOFA_OK;}
 if(!io||!io->read||!io->write)return VOFA_IO_ERROR;
 WR(0x3c6,0x00u);if(!io->read(VOFA_IO+0x3c6,&a))return VOFA_IO_ERROR;
 WR(0x3c6,0xffu);if(!io->read(VOFA_IO+0x3c6,&b))return VOFA_IO_ERROR;
 WR(0x3c6,0xa5u);if(!io->read(VOFA_IO+0x3c6,&c))return VOFA_IO_ERROR;
 if(a==0x00u&&b==0xffu)dac_type=VOFA_DAC_8BIT;        /* true 8-bit pixel mask */
 else if(c==0xa5u)dac_type=VOFA_DAC_24BIT;            /* mask keeps the pattern */
 else dac_type=VOFA_DAC_DEFAULT;                      /* CIC10487 class */
 return VOFA_OK;
}
/* Palette values arrive as 6 bits per gun (the kernel expands the ST 4-bit
   colour); scale them to what the detected part expects and program the mask. */
static uint8_t dac_scale(uint8_t value)
{
 value&=(uint8_t)0x3fu;
 switch(dac_type){
  case VOFA_DAC_15BIT:return (uint8_t)(value>>1);                 /* 5 bits/gun */
  case VOFA_DAC_24BIT:return (uint8_t)((value<<2)|(value>>4));    /* 8 bits/gun */
  default:return value;                                           /* 8-bit: 6 bits/gun */
 }
}
/* The two linear (Chain-4) modes are 8 bits per pixel. */
static uint8_t mode_is_8bpp(uint8_t mode)
{
    uint8_t m=mode_index(mode);
    return (uint8_t)(m==VOFA_MODE_800_8||m==VOFA_MODE_1024_8||m==VOFA_MODE_640_8);
}
/* 640x480 z 16 kolorami: standardowy VGA PLANAR (4 plany po 1 bicie).
   Wspolna konwencja z modelem karty w Hatari (src/vofa.c):
     CRTC 0x13 = szerokosc/8,  uklad base + (x/8)*4 + p, bit 7-(x&7),
     piksel = b0 | b1<<1 | b2<<2 | b3<<3.
   WYROZNIK formatu: rejestr 6 sterownika graficznego (3CF) = 0x02 (chain
   odd/even).  Sam sekwencer 0x06 NIE wystarcza, bo tryb mono programuje
   dokladnie ta sama wartosc - dlatego Hatari sprawdza takze ten bit. */
static uint8_t mode_is_4bpp(uint8_t mode)
{uint8_t m=mode_index(mode);
 return (uint8_t)(m==VOFA_MODE_640_4||m==VOFA_MODE_1024_4);}
/* 16 bpp HICOLOR (65 536 kolorow): 5:6:5 pakowane, 2 bajty na piksel.
   WYROZNIK formatu: rejestr 6 sterownika graficznego (3CF) = 0x08 - ta sama
   konwencja, ktora rozpoznaje model karty w Hatari (src/vofa.c). */
static uint8_t mode_is_16bpp(uint8_t mode)
{return (uint8_t)(mode_index(mode)==VOFA_MODE_800_16);}
static const uint8_t *crtc_for(uint8_t mode)
{
 switch(mode_index(mode)){
  case VOFA_MODE_800_8:return crtc_800_8;
  case VOFA_MODE_1024_8:return crtc_1024_8;
  case VOFA_MODE_640_8:return crtc_640_8;
  case VOFA_MODE_640_4:return crtc_640_8;   /* ten sam timing VGA 25 MHz */
  case VOFA_MODE_800_16:return crtc_800_16; /* raster 800x600, offset 16 bpp */
  case VOFA_MODE_1024_4:return crtc_1024_4; /* 1024x768, offset 4 bpp */
  case VOFA_MODE_800:return crtc_800;
  case VOFA_MODE_1024:return crtc_1024;
  default:return crtc;
 }
}
const vofa_mode_info_t *vofa_et4000_mode(uint8_t mode)
{
 static vofa_mode_info_t info;
 const vofa_mode_t *m=&mode_table[mode_index(mode)];
 info.width=m->width;info.height=m->height;info.bytes=m->surface_bytes;
 return &info;
}
int vofa_et4000_mode_index(uint16_t width,uint16_t height)
{
 unsigned i;
 for(i=0;i<VOFA_MODE_COUNT;i++)
  if(mode_table[i].width==width&&mode_table[i].height==height)return (int)i;
 return -1;
}
/* Live mode switch: rewrite only the CRTC timing/offset.  The sequencer,
   graphics controller, attribute controller and palette are already in the
   VOFA mono configuration, and the kernel repaints the whole frame, so no VRAM
   clear or multi-frame clock probe is needed (that path is for cold boot). */
int vofa_et4000_set_palette(const vofa_io_t *io,uint8_t index,uint8_t r,uint8_t g,uint8_t b)
{
 if(!io||!io->write)return VOFA_IO_ERROR;
 /* Scale to the detected part: 6 bits/gun on the 8-bit DACs, 5 on the
    CIC10487/SC11486 (15-bit) and 8 on the 24-bit family. */
 WR(0x3c8,index);
 WR(0x3c9,dac_scale(r));WR(0x3c9,dac_scale(g));WR(0x3c9,dac_scale(b));
 return VOFA_OK;
}
int vofa_et4000_set_mode(const vofa_io_t *io,uint8_t mode)
{
 const uint8_t *table=crtc_for(mode);
 uint8_t value;unsigned i;uint8_t eight=mode_is_8bpp(mode);
 WR(0x3c2,0xe3);IDX(0x3d4,0x11,0);
 for(i=0;i<sizeof(crtc);i++){IDX(0x3d4,i,table[i]);}
 for(i=0;i<3;i++){IDX(0x3d4,0x33+i,0);}
 WR(0x3d4,0x11);RD(0x3d5,value);IDX(0x3d4,0x11,value|0x80);
 /* Switching between mono and 8 bits per pixel changes the PIXEL FORMAT, not
    just the timing, so the sequencer / graphics controller / attribute
    controller must be reprogrammed as well. */
 {uint8_t four=mode_is_4bpp(mode);
  uint8_t six=mode_is_16bpp(mode);
  const uint8_t *sq=six?seq16:(eight?seq8:(four?seq4:seq)),
                *gd=six?gdc16:(eight?gdc8:(four?gdc4:gdc)),
                *at=eight?attr8:(four?attr4:attr);
  for(i=0;i<sizeof(seq);i++){IDX(0x3c4,i+1,sq[i]);}
  for(i=0;i<sizeof(gdc);i++){IDX(0x3ce,i,gd[i]);}
  WR(0x3c6,0xff);WR(0x3c8,0);
  RD(0x3da,value);
  for(i=0;i<sizeof(attr);i++){WR(0x3c0,i);WR(0x3c0,at[i]);}
  WR(0x3c0,0x20);}
 /* The kernel paints only the mode's VIEW height (m->height), while the CRT
  * scans m->card_height lines - 480 for the legacy 640 mode.  A previous larger
  * mode left its pixels in that tail band, which showed as corrupted lines
  * 401..480 after 800x600/1024x768 -> 640.  Clear exactly the band the kernel
  * never repaints; the visible rows are overwritten by the next full flush. */
 {
  const vofa_mode_t *m=&mode_table[mode_index(mode)];
  /* Planarny 4 bpp zajmuje 4 bajty na grupe 8 pikseli, czyli width/8*4 na
     linie (wszystkie plany). */
  uint32_t pitch=(uint32_t)(mode_is_16bpp(mode)?((uint32_t)m->width*2u):
                            (mode_is_8bpp(mode)?m->width:
                            (mode_is_4bpp(mode)?(m->width/8u)*4u:(m->width/8u))));
  uint32_t at=(uint32_t)m->height*pitch;
  uint32_t card_bytes=pitch*(uint32_t)m->card_height;
  for(;at<card_bytes;++at)if(!io->write(VOFA_MEM+at,0))return VOFA_MEMORY_ERROR;
 }
 return VOFA_OK;
}
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
int vofa_et4000_init(const vofa_io_t *io,uint8_t mode)
{
 const vofa_mode_t *m=&mode_table[mode_index(mode)];
 const uint8_t *table=crtc_for(mode);
 uint32_t card_bytes=(uint32_t)((mode_is_4bpp(mode)?(m->width/8u)*4u:(m->width/8u)))
                     *(uint32_t)m->card_height;
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
 {uint8_t four=mode_is_4bpp(mode);
  uint8_t six=mode_is_16bpp(mode);
  const uint8_t *sq=six?seq16:(mode_is_8bpp(mode)?seq8:(four?seq4:seq));
  for(i=0;i<sizeof(seq);i++){IDX(0x3c4,i+1,sq[i]);}}
 for(i=0;i<sizeof(extseq);i++){IDX(0x3c4,i+6,extseq[i]);}
 /* Exercise both byte lanes and the end of the displayed aperture. Restore
  * each sampled byte before testing the next, then clear the whole display.
  * This runs BEFORE the CRTC mode is programmed: the ROM console still owns
  * the screen, so the long VRAM fill is not visible as a black gap. */
 for(i=0;i<4;i++){
  uint32_t a=VOFA_MEM+(i<2?i:card_bytes-4u+i);
  if(!io->read(a,&old))return VOFA_MEMORY_ERROR;
  if(!io->write(a,0x55)||!io->read(a,&value)||value!=0x55){io->write(a,old);return VOFA_MEMORY_ERROR;}
  if(!io->write(a,0xaa)||!io->read(a,&value)||value!=0xaa){io->write(a,old);return VOFA_MEMORY_ERROR;}
  if(!io->write(a,old))return VOFA_MEMORY_ERROR;
 }
 /* Clear the aperture.  The byte-wise io->write() loop cost one indirect call
    per BYTE - 32000 in mono but 480000 in the 256-colour mode (15x the frame),
    which dominated the card boot.  The wide-store sequence now lives in the bus
    implementation behind io->fill, so the driver never dereferences a hardware
    address itself and the host test can exercise BOTH paths: with fill set the
    wide stores run, with fill NULL this byte loop runs.  The presence probes
    above already proved the aperture is readable/writable, so the direct stores
    the bus implementation performs are safe (the ROM console writes there
    directly too). */
 if(io->fill)io->fill(VOFA_MEM,card_bytes,0);
 else{uint32_t at;for(at=0;at<card_bytes;++at)if(!io->write(VOFA_MEM+at,0))return VOFA_MEMORY_ERROR;}
 WR(0x3c2,0xe3);IDX(0x3d4,0x11,0);
 for(i=0;i<sizeof(crtc);i++){IDX(0x3d4,i,table[i]);}
 for(i=0;i<3;i++){IDX(0x3d4,0x33+i,0);}
 WR(0x3d4,0x11);RD(0x3d5,value);IDX(0x3d4,0x11,value|0x80);
 RD(0x3da,value);
 {uint8_t four=mode_is_4bpp(mode);
  uint8_t six=mode_is_16bpp(mode);
  const uint8_t *at=mode_is_8bpp(mode)?attr8:(four?attr4:attr),
                *gd=six?gdc16:(mode_is_8bpp(mode)?gdc8:(four?gdc4:gdc));
  for(i=0;i<sizeof(attr);i++){WR(0x3c0,i);WR(0x3c0,at[i]);}
  WR(0x3c0,0x20);
  for(i=0;i<sizeof(gdc);i++){IDX(0x3ce,i,gd[i]);}}
 IDX(0x3d4,0x36,0x53); /* VOFA mono, 8-bit IO */
 /* Identify the RAMDAC family before the palette is programmed; the default
    (CIC10487, 15-bit) applies when the probe cannot tell. */
 (void)vofa_et4000_dac_detect(io,VOFA_DAC_UNKNOWN);
 /* On the Sierra-style parts the mask register also selects the colour
    resolution; the plain 8-bit parts just take 0xFF. */
 WR(0x3c6,(uint8_t)(dac_type==VOFA_DAC_15BIT?0x0fu:0xffu));
 IDX(0x3c4,1,0x21);WR(0x3c8,0);
 /* Kernel convention: bit 0 black, bit 1 white; overscan black. */
 for(i=0;i<6;i++){WR(0x3c9,i<3?0:63);}
 WR(0x3c8,255);for(i=0;i<3;i++){WR(0x3c9,0);}
 IDX(0x3c4,1,1);
 /* The adapter scans out from this point. Let the kernel fill the cleared
    VRAM with the boot log before the multi-frame clock measurement. */
 if(io->render)io->render();
 frames=count_frames(io);if(frames<0)return frames;
 if(frames<12){IDX(0x3c4,7,0xa4);frames=count_frames(io);}
 if(frames<12||frames>19)return VOFA_CLOCK_ERROR;
 return VOFA_OK;
}
