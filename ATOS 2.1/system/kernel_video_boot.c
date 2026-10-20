#include "kernel_video.h"
#include "vofa_et4000.h"
#include "kernel_video_service.h"
#include "kernel_module.h"
#include "kernel_fat.h"
#include "kernel_console.h"
static const kernel_timer_service_t *video_timer;
static uint32_t video_ticks(void){return video_timer->poll();}
/* Kept for a live mode switch: the module service stays resident, and the mode
   can be reprogrammed without a reboot (OPTIONS -> 800x600 / 1024x768). */
static const vofa_io_t *vofa_io;
static const kernel_video_service_t *vofa_service;
static uint16_t vofa_mode=VOFA_MODE_640;
/* Runs inside VOFA.SYS's initialiser once the ET4000 is scanning. Paint the
   whole console grid into the card VRAM now: kvideo_flush then keeps it in
   step, and the takeover shows the log instead of a black screen. */
static void video_render_log(void)
{
 if(!kvideo_is_vofa()&&!kvideo_activate_vofa((volatile uint8_t *)(uintptr_t)VOFA_MEM))return;
 kconsole_invalidate();kconsole_flush();
}
static void video_delay(void)
{volatile unsigned i;for(i=0;i<64;i++)__asm__ volatile("nop");}
static void append_text(char *line,unsigned *k,const char *text)
{while(*text)line[(*k)++]=*text++;}
static void append_u32(char *line,unsigned *k,uint32_t value)
{char digits[10];unsigned n=0;
 if(!value){line[(*k)++]='0';return;}
 while(value&&n<sizeof(digits)){digits[n++]=(char)('0'+value%10u);value/=10u;}
 while(n)line[(*k)++]=digits[--n];}
/* "VOFA" selects the default 640x480 card mode; an optional size token picks
   800x600 or 1024x768.  Returns the mode index, or -1 when the file is not a
   VOFA request. */
static int requested_mode(const char *text,uint32_t size)
{
 unsigned i;unsigned val[4];int nv=0;int cur=-1;
 /* Bez inicjalizatora tablicy: kernel jest -ffreestanding i nie ma libc, a GCC
    zamienial {0,0,0,0} na wywolanie memset, ktorego nie ma czym linkowac. */
 val[0]=0;val[1]=0;val[2]=0;val[3]=0;
 if(size<4||text[0]!='V'||text[1]!='O'||text[2]!='F'||text[3]!='A')return -1;
 /* Zbieramy KOLEJNE liczby rozdzielone 'x' albo bialym znakiem.  Dzieki temu
    rozumiemy pelny zapis WxHxD, ktory zapisuje okno SETTINGS (RES=800x600x8),
    a nie tylko jedna liczbe.  Poprzednia wersja patrzyla na JEDNA wartosc i
    szukala podciagow "256"/"16" - przez co "1024x768x8" dawalo mono 1024,
    "640x480x8"/"800x600x8" byly ignorowane, a "800x600x16" (zawiera "16")
    mylnie wybieralo tryb 640x480x4. */
 for(i=4;i<size;i++){
  char c=text[i];
  if(c>='0'&&c<='9'){
   if(cur<0){
    if(nv>=4)return -1;
    cur=nv;val[nv]=0;++nv;
   }
   if(val[cur]<100000u)val[cur]=val[cur]*10u+(unsigned)(c-'0');
   continue;
  }
  /* 'x' i bialy znak KONCZA biezaca liczbe - tak samo rozumiemy "800x600x16"
     jak i "800 600 16". */
  if(c=='x'||c=='X'||c==' '||c=='\r'||c=='\n'||c=='\t'||c==','||c=='='){
   cur=-1;
   continue;
  }
  return -1;
 }
 if(!nv)return (int)VOFA_MODE_640;
 /* Pelny zapis WxHxD. */
 if(nv>=3){
  unsigned w=val[0],h=val[1],d=val[2];
  if(d==8u){
   if(w==640u&&h==480u)return (int)VOFA_MODE_640_8;
   if(w==800u&&h==600u)return (int)VOFA_MODE_800_8;
   if(w==1024u&&h==768u)return (int)VOFA_MODE_1024_8;
   return -1;
  }
  if(d==4u){
   /* 16 kolorow: 640x480 ORAZ 1024x768 - dokladnie te dwa wpisy ma lista
      SETTINGS i oba zapisuje sie do DESKTOP.CFG jako RES=WxHx4.  Brak
      drugiego z nich powodowal, ze zapisane "1024x768x16 colors" bylo po
      restarcie odrzucane przez parser i wracalo domyslne 800x600x8. */
   if(w==640u&&h==480u)return (int)VOFA_MODE_640_4;
   if(w==1024u&&h==768u)return (int)VOFA_MODE_1024_4;
   return -1;
  }
  if(d==16u)return (w==800u&&h==600u)?(int)VOFA_MODE_800_16:-1;
  return -1;
 }
 /* Dwie liczby: sam raster, czyli tryb mono (zgodnosc ze starymi wpisami). */
 if(nv==2){
  unsigned w=val[0],h=val[1];
  if((w==640u&&h==400u)||(w==640u&&h==480u))return (int)VOFA_MODE_640;
  if(w==800u&&h==600u)return (int)VOFA_MODE_800;
  if(w==1024u&&h==768u)return (int)VOFA_MODE_1024;
  return -1;
 }
 /* Jedna liczba: stare formy "VOFA 256" (8 bpp) i "VOFA 16" (16 kolorow),
    a takze sam raster 800/600/1024/768 jako tryb mono. */
 {unsigned v=val[0];
  if(v==256u)return (int)VOFA_MODE_800_8;
  if(v==16u)return (int)VOFA_MODE_640_4;
  if(v==640u)return (int)VOFA_MODE_640;
  if(v==800u||v==600u)return (int)VOFA_MODE_800;
  if(v==1024u||v==768u)return (int)VOFA_MODE_1024;}
 return -1;
}
/* Load VOFA.SYS and publish its service WITHOUT touching the card.  The ROM
   already programmed the adapter on the boot path, so the kernel only needs the
   service vtable to be able to reprogram it later (OPTIONS menu). */
/* ---- graphics driver manifest ----------------------------------------------
   The BIOS detects the adapter/chipset and passes them in the boot handoff
   (reserved0 bit7 = valid, bits0..6 = adapter; reserved1 = chipset).  A chip
   maps to candidate driver modules in \SYSTEM\DRV, most specific first; the
   first one that loads and publishes the video service wins.  Adding Mach32 /
   Mach64 / W32i acceleration is therefore a new file plus one entry here - the
   compositor, the VDI and the desktop never change. */
typedef struct {uint8_t chip;char name[11];} gfx_driver_t;
static const gfx_driver_t gfx_drivers[]={
 /* ET4000 family: prefer the accelerated W32i driver, fall back to plain. */
 {1u,{'W','3','2','I',' ',' ',' ',' ','S','Y','S'}},
 {1u,{'E','T','4','0','0','0',' ',' ','S','Y','S'}},
 {2u,{'M','A','C','H','3','2',' ',' ','S','Y','S'}},
 {3u,{'M','A','C','H','6','4',' ',' ','S','Y','S'}}
};
static uint8_t gfx_detected_chip;
void kvideo_set_detected(uint8_t adapter_field,uint8_t chip)
{
 gfx_detected_chip=(adapter_field&0x80u)?(uint8_t)(chip&0x7fu):0u;
}
static void driver_announce(const char *prefix,const char name[11])
{
 char line[40];unsigned k=0,j;
 while(prefix[k]&&k<30u){line[k]=prefix[k];++k;}
 for(j=0;j<11u&&k<38u;++j)line[k++]=name[j];
 line[k++]='\n';line[k]=0;
 kconsole_puts(line);
}
static int load_driver_for_chip(void)
{
 unsigned i,k;enum module_state state=MODULE_NOT_FOUND;
 for(i=0;i<sizeof(gfx_drivers)/sizeof(gfx_drivers[0]);++i){
  char name[11];
  if(gfx_drivers[i].chip!=gfx_detected_chip)continue;
  for(k=0;k<11u;++k)name[k]=(char)gfx_drivers[i].name[k];
  driver_announce("VIDEO DRV TRY: ",name);
  state=kernel_load_driver_module(name,0x0108u,0);
  if(state==MODULE_READY){driver_announce("VIDEO DRV: ",name);return 1;}
 }
 /* Compatibility fallback: an image without the DRV directory still boots. */
 {static const char flat[11]={'V','O','F','A',' ',' ',' ',' ','S','Y','S'};
  driver_announce("VIDEO DRV TRY: ",flat);
  if(kernel_load_system_module(flat,0x0108u,0)!=MODULE_READY)return 0;
  driver_announce("VIDEO DRV: ",flat);return 1;}
}
int kvideo_module_attach(const kernel_timer_service_t *timer)
{
 static const vofa_io_t io={kvideo_read8,kvideo_write8,video_delay,video_ticks,video_render_log,
                            vofa_et4000_vram_fill};
 enum module_state state;
 const kernel_video_service_t *service;
 if(!timer||!timer->poll||timer->frequency_hz!=50)return 0;
 video_timer=timer;
 state=load_driver_for_chip()?MODULE_READY:MODULE_NOT_FOUND;
 service=(const kernel_video_service_t *)kernel_find_service(KERNEL_SERVICE_VIDEO,KERNEL_VIDEO_VERSION);
 if(state!=MODULE_READY||!service||service->magic!=KERNEL_VIDEO_MAGIC||
    service->version<KERNEL_VIDEO_VERSION||service->size<sizeof(*service)||
    !service->init||!service->set_mode||!service->mode_info||!service->mode_index)return 0;
 vofa_io=&io;vofa_service=service;
 return 1;
}
void kvideo_boot(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *timer)
{
 static const char name[11]={'V','I','D','E','O',' ',' ',' ','C','F','G'};
 char config[32];uint32_t size;int result,mode_index;
 if(kfat_load_system_file(name,config,sizeof(config),&size)!=FAT_FILE_FOUND)return;
 mode_index=requested_mode(config,size);
 if(mode_index<0)return;
 kconsole_puts("VIDEO: VOFA REQUESTED / ");(void)cpu;
 /* Boot probe recovery currently supports only the 68000 exception frame.
  * ST-RAM reaching the adapter aperture must never be mistaken for VRAM. */
 if(mode!=2||ram>=VOFA_MEM||!timer||!timer->poll||timer->frequency_hz!=50){
  kconsole_puts("UNSUPPORTED CONFIG - SHIFTER\n");return;
 }
 /* VOFA/ET4000 mode programming lives in the optional VOFA.SYS module. */
 if(!kvideo_module_attach(timer)){kconsole_puts("MODULE MISSING - SHIFTER\n");return;}
 vofa_mode=(uint16_t)mode_index;
 result=vofa_service->init(vofa_io,(uint8_t)mode_index);
 if(result!=VOFA_OK)kvideo_deactivate_vofa();
 if(result==VOFA_OK){
  const vofa_mode_info_t *m=vofa_service?vofa_service->mode_info((uint8_t)vofa_mode):0;
  if(!m)return;
  /* 800x600x256 boots with the chunky pitch; every other mode stays planar. */
  kvideo_set_mode_format(m->width,m->height,
                         (uint8_t)((mode_index==(int)VOFA_MODE_800_8)?8u:1u));
  if(kvideo_activate_vofa((volatile uint8_t *)(uintptr_t)VOFA_MEM)){
   char line[72];unsigned k=0;
   append_text(line,&k,"");
   append_u32(line,&k,m->width);append_text(line,&k,"x");append_u32(line,&k,m->height);
   append_text(line,&k," MONO / VIEW ");
   append_u32(line,&k,m->width);append_text(line,&k,"x");append_u32(line,&k,m->height);
   append_text(line,&k,"\nDAC: ");
   /* The RAMDAC family decides how colour is programmed (Nova ships one driver
      per family); report what was detected or assumed. */
   if(vofa_service->version>=4u&&vofa_service->dac_name)
       append_text(line,&k,vofa_service->dac_name(vofa_service->dac_type?vofa_service->dac_type():VOFA_DAC_DEFAULT));
   else append_text(line,&k,"UNKNOWN");
   append_text(line,&k,"\n");
   kconsole_puts(line);
  }else kconsole_puts("NO SHADOW RAM - SHIFTER\n");
 }else if(result==VOFA_ABSENT)kconsole_puts("NOT FOUND - SHIFTER\n");
 else if(result==VOFA_CLOCK_ERROR)kconsole_puts("CLOCK CHECK FAILED - SHIFTER\n");
 else if(result==VOFA_MEMORY_ERROR)kconsole_puts("VRAM CHECK FAILED - SHIFTER\n");
 else kconsole_puts("BUS ACCESS FAILED - SHIFTER\n");
}
/* Live switch used by the OPTIONS menu (GFX opcode 30).  Reprograms the card
   and adopts the new geometry; the caller reconfigures the compositor. */
int kvideo_set_palette(uint8_t index,uint16_t color)
{
 uint8_t r,g,b;
 if(!vofa_service||!vofa_io||!vofa_service->set_palette)return -1;
 r=(uint8_t)((color>>8)&0x0fu);g=(uint8_t)((color>>4)&0x0fu);b=(uint8_t)(color&0x0fu);
 return vofa_service->set_palette(vofa_io,index,
                                  (uint8_t)((r<<2)|(r>>2)),(uint8_t)((g<<2)|(g>>2)),(uint8_t)((b<<2)|(b>>2)));
}
/* Full-precision RAMDAC entry (8 bits per gun in, 6 bits per gun to the
   driver, which scales to the detected DAC).  Used by the exclusive game mode,
   whose palettes are finer than the 12-bit system colours. */
int kvideo_set_palette_rgb(uint8_t index,uint8_t r,uint8_t g,uint8_t b)
{
 if(!vofa_service||!vofa_io||!vofa_service->set_palette)return -1;
 return vofa_service->set_palette(vofa_io,index,(uint8_t)(r>>2),(uint8_t)(g>>2),(uint8_t)(b>>2));
}
/* ---- zabezpieczenie zmiany trybu (include/kvideo_guard.h) ----------------- */
#include "kvideo_guard.h"
static kvideo_guard_t video_guard={0u,0u,0u,0u,0u};
static uint32_t video_guard_owner;
void kvideo_guard_watch(uint16_t w,uint16_t h,uint8_t bpp,uint32_t now,uint32_t owner)
{video_guard_owner=owner;kvideo_guard_arm(&video_guard,w,h,bpp,now);}
int kvideo_guard_active(void){return video_guard.pending!=0u;}
/* Uwaga: wygasanie obsluguje kvideo_guard_rollback(), wolany z tykniecia
   50 Hz.  Duplikowanie tego tutaj bylo bledne: przy przekreconym liczniku
   (uint32) potrafilo zdjac straznika, ktory jeszcze nie wygasl - zlapal to
   test test_video_boot.c.  Zaklinowanie bralo sie TYLKO z ponownego
   uzbrajania w rollback() i tam jest naprawione. */
static int guard_restore(void)
{
    extern int kgfx_video_reconfigure_format(uint16_t,uint16_t,uint8_t);
    /* Restore the renderer, compositor and hardware as one mode transition. */
    return kgfx_video_reconfigure_format(video_guard.prev_w,video_guard.prev_h,video_guard.prev_bpp);
}
int kvideo_guard_rollback(uint32_t now)
{
    if(!kvideo_guard_expired(&video_guard,now))return 0;
    if(guard_restore()<0){
        /* Nie udalo sie wrocic: zdejmij blokade zamiast uzbrajac w kolko.
           Tryb zostaje jaki jest - wazniejsze, zeby dalo sie wybrac inny. */
        video_guard.pending=0u;
        return -1;}
    return 1;
}
int kvideo_guard_control(uint32_t owner,uint32_t op,uint32_t now)
{
    (void)kvideo_guard_rollback(now);
    if(op==2u)return video_guard.pending&&video_guard_owner==owner;
    if(!video_guard.pending||video_guard_owner!=owner)return -1;
    if(op==0u){kvideo_guard_confirm(&video_guard);return 0;}
    if(op==1u){
        if(guard_restore()<0){
            /* Nie udalo sie wrocic: zdejmij blokade DOKLADNIE tak, jak robi to
               wygasniecie (kvideo_guard_rollback).  Bez tego aplikacja zeruje
               swoje confirm_pending, a kazde kolejne APPLY trafia na
               kvideo_guard_active() -> -0x3e5 (-997) az do wygasniecia.
               Zgloszony objaw "MODE REFUSED przy APPLY" mial wlasnie ta droge. */
            kvideo_guard_confirm(&video_guard);
            return -1;}
        kvideo_guard_confirm(&video_guard);return 0;
    }
    return -1;
}
int kvideo_program_mode_format(uint16_t width,uint16_t height,uint8_t bpp)
{
 int index,result;const vofa_mode_info_t *m;
 if(!vofa_io||!vofa_service||!vofa_service->set_mode||!vofa_service->mode_index||!vofa_service->mode_info)return -1;
 if(bpp!=1u&&bpp!=8u&&bpp!=4u&&bpp!=16u)return -2;   /* 4 = 16 kolorow, 16 = HICOLOR */
 /* Geometria i GLEBIA razem wybieraja wpis.  Poprzednia wersja miala galezie
    bpp==4 i bpp==16 WEWNATRZ bpp==8, wiec byly nieosiagalne: zadanie
    800x600x16 konczylo sie jako 8 bpp, a kontrola rozmiaru znala tylko
    width (8 bpp) i width/8 (mono). */
 if(bpp==1u){
  index=vofa_service->mode_index(width,height);
 }else if(bpp==8u){
  if(width==1024u&&height==768u)index=(int)VOFA_MODE_1024_8;
  else if(width==640u&&height==480u)index=(int)VOFA_MODE_640_8;
  else if(width==800u&&height==600u)index=(int)VOFA_MODE_800_8;
  else return -2;
 }else if(bpp==4u){
  /* 16 kolorow: 640x480 oraz 1024x768 (standardowy tryb VGA 4 bpp). */
  if(width==640u&&height==480u)index=(int)VOFA_MODE_640_4;
  else if(width==1024u&&height==768u)index=(int)VOFA_MODE_1024_4;
  else return -2;
 }else{
  if(width!=800u||height!=600u)return -2;
  index=(int)VOFA_MODE_800_16;
 }
 if(index<0)return -2;
 /* Rozmiar klatki zalezy od FORMATU: mono width/8, 4 bpp width/8*4,
    8 bpp width, 16 bpp width*2.  Bez tego tryby 4 i 16 odpadaly na -4. */
 m=vofa_service->mode_info((uint8_t)index);
 if(!m||m->width!=width||m->height!=height||
    m->bytes!=(uint32_t)(((bpp==1u)?((width+7u)/8u):
                          (bpp==4u)?((((width+7u)/8u))*4u):
                          (bpp==16u)?(width*2u):width))*(uint32_t)height)return -4;
 result=vofa_service->set_mode(vofa_io,(uint8_t)index);
 /* Encode the failing step so a probe can tell where a mode switch stopped:
    -0x100 - index, -0x200 - set_mode result. */
 if(result!=VOFA_OK)return (int)(-0x200-(int)index*0x10-(int)result);
 /* UWAGA: to lista ZAMKNIETA - kazda galaz wyboru trybu powyzej musi byc tu
    wpisana.  Brak wpisu jest grozny, bo set_mode() JUZ przeprogramowal karte:
    funkcja zwraca wtedy blad, mimo ze ekran jest w nowym trybie, jadro nie
    aktualizuje geometrii/format/u i NIE UZBROI straznika zmiany trybu (op 40
    uzbraja tylko przy r>=0) - obraz zostaje smieciami na zawsze.  Tak zniknal
    VOFA_MODE_1024_4, dodany 15.10 jako wpis SETTINGS "1024 x 768 - 16 COLORS":
    640x480x4 (wpisany) dzialal, 1024x768x4 zawieszal obraz. */
 if((bpp==8u||bpp==4u||bpp==16u)&&index!=(int)VOFA_MODE_800_8&&index!=(int)VOFA_MODE_1024_8&&
    index!=(int)VOFA_MODE_640_8&&index!=(int)VOFA_MODE_640_4&&
    index!=(int)VOFA_MODE_1024_4&&
    index!=(int)VOFA_MODE_800_16)return -0x310-(int)index;
 m=vofa_service->mode_info((uint8_t)index);
 if(!m)return -4;
 /* Do warstwy wideo idzie format KARTY (1/4/8/16): ona wybiera sposob zapisu
    (mono / 4 plany / chunky 8 bpp / 5:6:5), a obraz zrodlowy jest ZAWSZE 8 bpp
    chunky (albo mono dla 1 bpp).  Wczesniej wszystko poza 8 redukowalo sie tu
    do 1, wiec 4 i 16 renderowaly sie jako mono. */
 kvideo_set_mode_format(m->width,m->height,bpp);
 if(!kvideo_activate_vofa((volatile uint8_t *)(uintptr_t)VOFA_MEM))return -5;
 vofa_mode=(uint16_t)index;
 return 0;
}
int kvideo_program_mode(uint16_t width,uint16_t height)
{return kvideo_program_mode_format(width,height,1u);}
int kvideo_mode_supported(uint16_t width,uint16_t height)
{
 if(!vofa_service||!vofa_service->mode_index)return width==640u&&height==400u;
 return vofa_service->mode_index(width,height)>=0;
}
