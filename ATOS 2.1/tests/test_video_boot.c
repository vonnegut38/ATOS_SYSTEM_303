#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../system/kernel_video_boot.c"
static const char *contents;static int probes,activations,result_code,alloc_ok=1;static char logtext[256];
static unsigned mode_sets,deactivations;static uint16_t mode_w,mode_h;static uint8_t mode_bpp;
static unsigned renderer_restores;
int kgfx_video_reconfigure_format(uint16_t w,uint16_t h,uint8_t b)
{++renderer_restores;return kvideo_program_mode_format(w,h,b);}
static uint32_t tick(void){return 0;}
enum fat_file_state kfat_load_system_file(const char name[11],void *out,uint32_t cap,uint32_t *size)
{assert(!memcmp(name,"VIDEO   CFG",11));if(!contents)return FAT_FILE_NOT_FOUND;*size=(uint32_t)strlen(contents);if(*size>cap)return FAT_FILE_IO_FAILED;memcpy(out,contents,*size);return FAT_FILE_FOUND;}
void kconsole_puts(const char *s){strcat(logtext,s);}
/* The boot path may flush the console once the adapter scans out; the test only
   checks the log text, so the rendering calls stay inert. */
void kconsole_flush(void){}
void kconsole_invalidate(void){}
/* Zaslepka: kvideo_guard_active() pyta o zegar uzytkownika. */
static uint32_t test_clock;
uint32_t kernel_user_clock(void){return test_clock;}
int kvideo_read8(uint32_t a,uint8_t *b){(void)a;(void)b;assert(0);return 0;}
int kvideo_write8(uint32_t a,uint32_t b){(void)a;(void)b;assert(0);return 0;}
int vofa_et4000_init(const vofa_io_t *io,uint8_t mode){assert(io->read&&io->write&&io->ticks);(void)mode;probes++;return result_code;}
int kvideo_activate_vofa(volatile uint8_t *p){assert((uintptr_t)p==VOFA_MEM);activations++;return alloc_ok;}
int kvideo_is_vofa(void){return activations&&alloc_ok;}
void kvideo_deactivate_vofa(void){deactivations++;}
void kvideo_set_mode(uint16_t w,uint16_t h){mode_w=w;mode_h=h;mode_bpp=1u;mode_sets++;}
void kvideo_set_mode_format(uint16_t w,uint16_t h,uint8_t bpp){mode_w=w;mode_h=h;mode_bpp=bpp;mode_sets++;}
/* Only reached for a chip in the driver manifest; the test never sets one, so
   the VOFA.SYS fallback below is the path under test. */
enum module_state kernel_load_driver_module(const char name[11],uint16_t version,int prefer_tt)
{(void)name;(void)version;(void)prefer_tt;return MODULE_NOT_FOUND;}
static vofa_mode_info_t test_mode={640u,400u,32000UL};
/* Realna tabela trybow (te same wpisy i ta sama kolejnosc co
   system/vofa_et4000.c).  test_vofa.c sprawdza zgodnosc tabel sterownika,
   a ten blok sprawdza, ze (w,h,bpp) wybiera wlasciwy wpis i ze format KARTY
   dociera do warstwy wideo. */
/* Tablica WSKAZNIKOW, zeby wpis 640x480x8 (indeks 5) pozostal MUTOWALNY:
   dotychczasowe testy zmieniaja test_mode.width/height/bytes, zeby zasymulowac
   rozne wersje modulu sterownika, i ta mozliwosc musi zostac. */
static vofa_mode_info_t test_mono640={640u,400u,32000UL};
static vofa_mode_info_t test_mono800={800u,600u,60000UL};
static vofa_mode_info_t test_mono1024={1024u,768u,98304UL};
static vofa_mode_info_t test_bpp8_800={800u,600u,480000UL};
static vofa_mode_info_t test_bpp8_1024={1024u,768u,786432UL};
static vofa_mode_info_t test_bpp4_640={640u,480u,153600UL};
static vofa_mode_info_t test_bpp16_800={800u,600u,960000UL};
/* Indeks 8 = 1024x768 z 16 kolorami.  Wpis MUSI tu byc: bez niego test
   trybow nie widzi, ze jadro w ogole zna ten tryb (dokladnie ta luka
   przepuscila brak VOFA_MODE_1024_4 na liscie dozwolonych indeksow). */
static vofa_mode_info_t test_bpp4_1024={1024u,768u,393216UL};
static vofa_mode_info_t *const test_modes[]={
 &test_mono640,&test_mono800,&test_mono1024,
 &test_bpp8_800,&test_bpp8_1024,&test_mode,   /* 5 = mutowalny 640x480x8 */
 &test_bpp4_640,&test_bpp16_800,&test_bpp4_1024
};
static unsigned hardware_sets;
static const vofa_mode_info_t *test_mode_info(uint8_t m)
{return (m<sizeof(test_modes)/sizeof(test_modes[0]))?test_modes[m]:0;}
static int test_mode_index(uint16_t w,uint16_t h)
{return (w==640u&&h==400u)?0:((w==800u&&h==600u)?1:((w==1024u&&h==768u)?2:-1));}
static int test_set_mode(const vofa_io_t *io,uint8_t m){(void)io;(void)m;++hardware_sets;return VOFA_OK;}
enum module_state kernel_load_system_module(const char name[11],uint16_t owner,int prefer_tt)
{(void)name;(void)owner;(void)prefer_tt;return MODULE_READY;}
const void *kernel_find_service(uint32_t id,uint16_t minimum_version)
{
 /* Field order follows kernel_video_service_t exactly; every member is listed
    so a new one cannot silently shift the vtable. */
 static const kernel_video_service_t service={KERNEL_VIDEO_MAGIC,KERNEL_VIDEO_VERSION,
  (uint16_t)sizeof(kernel_video_service_t),vofa_et4000_init,test_set_mode,
  test_mode_info,test_mode_index,0,0,0};
 (void)id;(void)minimum_version;return &service;}
static void run(uint8_t mode,uint8_t cpu,uint32_t ram,const kernel_timer_service_t *t)
{logtext[0]=0;probes=activations=0;mode_sets=0;kvideo_boot(mode,cpu,ram,t);}
int main(void){
 kernel_timer_service_t timer={0};timer.poll=tick;timer.frequency_hz=50;
 contents=0;run(2,0,4<<20,&timer);assert(!probes&&!logtext[0]);
 contents="SHIFTER\n";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOFA_extra";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOF";run(2,0,4<<20,&timer);assert(!probes);
 contents="VOFA\r\n";
 run(1,0,4<<20,&timer);assert(!probes);
 /* The CPU is deliberately NOT a gate: the card boot runs on the TF536 (68030)
    and main.c applies the same conditions without a CPU test - only the
    68000-style recovery frame is unsupported, which is a different path. */
 run(2,3,4<<20,&timer);assert(probes==1&&!activations);
 run(2,0,VOFA_MEM,&timer);assert(!probes);
 run(2,0,4<<20,0);assert(!probes);
 timer.frequency_hz=200;run(2,0,4<<20,&timer);assert(!probes);timer.frequency_hz=50;
 for(result_code=-3;result_code<=0;result_code++){
  run(2,0,4<<20,&timer);assert(probes==1&&!activations&&strstr(logtext,"SHIFTER"));
 }
 result_code=1;run(2,0,4<<20,&timer);
 /* The log prints the driver's mode table entry, and the 640 mode is a true
    640x400 view (card_height 400) - "640x480" was the older scanning table. */
 assert(probes==1&&activations==1&&strstr(logtext,"640x400"));
 /* The adopted geometry must come from the driver's mode table. */
 assert(mode_sets==1&&mode_w==640u&&mode_h==400u&&mode_bpp==1u);
 /* Reproduce a legacy module that aliases the new 640x480x8 index to mono.
    Refusal must leave both hardware and software geometry untouched. */
 hardware_sets=0;
 assert(kvideo_program_mode_format(640,480,8)==-4);
 assert(!hardware_sets&&mode_w==640&&mode_h==400&&mode_bpp==1);
 assert(kvideo_program_mode_format(320,240,8)==-2&&!hardware_sets);
 test_mode.width=640;test_mode.height=480;test_mode.bytes=38400;
 assert(kvideo_program_mode_format(640,480,8)==-4&&!hardware_sets);
 test_mode.bytes=307200;
 assert(kvideo_program_mode_format(640,480,8)==0&&hardware_sets==1);
 assert(mode_w==640&&mode_h==480&&mode_bpp==8);
 test_mode.width=640;test_mode.height=400;test_mode.bytes=32000;
 alloc_ok=0;run(2,0,4<<20,&timer);assert(strstr(logtext,"NO SHADOW RAM"));
 /* ---- parser RES: pelny tryb WxHxD + zgodnosc wstecz (round-trip) ---------
    Okno SETTINGS zapisuje RES=<W>x<H>x<D>.  Ten blok sprawdza, ze KAZDA
    kombinacja, ktora UI moze wybrac, wraca z parsera jako TEN SAM tryb, oraz
    ze stare wpisy nadal dzialaja.  Bez tego zapis ustawien ginal po restarcie:
    "1024x768x8" czytalo sie jako mono, a "640x480x8" bylo ignorowane.
    Uwaga na przypadek "800x600x16": zawiera podciag "16", ktory w poprzedniej
    wersji parsera wybieral tryb 640x480x4 - dlatego jest tu osobno. */
 {
  static const struct {const char *text;int mode;} cases[]={
   {"VOFA 640x480x8",(int)VOFA_MODE_640_8},
   {"VOFA 800x600x8",(int)VOFA_MODE_800_8},
   {"VOFA 1024x768x8",(int)VOFA_MODE_1024_8},
   {"VOFA 640x480x4",(int)VOFA_MODE_640_4},
   {"VOFA 1024x768x4",(int)VOFA_MODE_1024_4},
   {"VOFA 800x600x16",(int)VOFA_MODE_800_16},
   {"VOFA 256",(int)VOFA_MODE_800_8},
   {"VOFA 16",(int)VOFA_MODE_640_4},
   {"VOFA 800x600",(int)VOFA_MODE_800},
   {"VOFA 1024x768",(int)VOFA_MODE_1024},
   {"VOFA 640x480",(int)VOFA_MODE_640},
   {"VOFA",(int)VOFA_MODE_640},
   {"VOFA 800x600x4",-1},
   {"VOFA 1024x768x16",-1},
   {"VOFA 640x480x16",-1}
  };
  unsigned ci;
  for(ci=0;ci<sizeof(cases)/sizeof(cases[0]);++ci)
   assert(requested_mode(cases[ci].text,(uint32_t)strlen(cases[ci].text))==cases[ci].mode);
 }
 /* ---- pelny kontrakt trybu: (w,h,bpp) -> tryb karty + format do wideo ----
    Ten blok sprawdza DOKLADNIE to, co audyt znalazl jako blad: galerie bpp==4
    i bpp==16 byly wewnatrz bpp==8 (nieosiagalne), kontrola rozmiaru znala
    tylko width i width/8, a do warstwy wideo szedl format 1 zamiast 4/16. */
 {
  /* Wczesniejsze testy mutowaly test_mode (wpis 640x480x8) - przywracamy
     standardowy wpis, zeby ten blok zaczynal od znanego stanu. */
  test_mode.width=640u;test_mode.height=480u;test_mode.bytes=307200UL;
  alloc_ok=1;   /* wczesniejszy test ustawil 0 i nie przywrocil */
  static const struct {uint16_t w,h;uint8_t bpp;int mode;uint8_t card_bpp;} cases[]={
   {640u,480u,8u,(int)VOFA_MODE_640_8,8u},
   {800u,600u,8u,(int)VOFA_MODE_800_8,8u},
   {1024u,768u,8u,(int)VOFA_MODE_1024_8,8u},
   {640u,480u,4u,(int)VOFA_MODE_640_4,4u},      /* 16 kolorow */
   {1024u,768u,4u,(int)VOFA_MODE_1024_4,4u},   /* 16 kolorow, pelny ekran */
   {800u,600u,16u,(int)VOFA_MODE_800_16,16u},  /* 65 536 kolorow */
   {800u,600u,1u,(int)VOFA_MODE_800,1u},       /* mono przez mode_index */
   {640u,400u,1u,(int)VOFA_MODE_640,1u}
  };
  unsigned ci;
  for(ci=0;ci<sizeof(cases)/sizeof(cases[0]);++ci){
   vofa_mode=0xffffu;mode_w=0;mode_h=0;mode_bpp=0;
   assert(kvideo_program_mode_format(cases[ci].w,cases[ci].h,cases[ci].bpp)==0);
   assert(vofa_mode==(uint16_t)cases[ci].mode);
   assert(mode_bpp==cases[ci].card_bpp);   /* format KARTY, nie 1 */
  }
  /* Kombinacje, ktorych nie ma: 1024x768x16, 800x600x4, 640x480x16. */
  assert(kvideo_program_mode_format(1024u,768u,16u)<0);
  assert(kvideo_program_mode_format(800u,600u,4u)<0);
  assert(kvideo_program_mode_format(640u,480u,16u)<0);
  assert(kvideo_program_mode_format(640u,480u,2u)<0);   /* nieznana glebia */
 }
 /* Exercise the production guard glue, not only the timer helper. */
 kvideo_guard_watch(800,600,8,100,7);
 assert(kvideo_guard_control(8,0,101)<0&&kvideo_guard_active());
 assert(kvideo_guard_control(7,2,102)==1);
 assert(kvideo_guard_control(7,1,103)==0&&!kvideo_guard_active());
 assert(renderer_restores==1&&mode_w==800&&mode_h==600&&mode_bpp==8);
 kvideo_guard_watch(640,480,4,200,7);
 assert(kvideo_guard_control(7,0,400)<0&&!kvideo_guard_active()); /* too late */
 assert(renderer_restores==2&&mode_bpp==4);
 kvideo_guard_watch(800,600,8,500,7);
 assert(kvideo_guard_control(7,0,600)==0&&!kvideo_guard_active());
 assert(kvideo_guard_rollback(900)==0&&renderer_restores==2);
 /* Anulowanie, ktore NIE moze wrocic (tryb powrotu nieznany sterownikowi:
    bpp=3).  Blokada MUSI wtedy zniknac - inaczej aplikacja zeruje swoje
    confirm_pending, a kazde kolejne APPLY dostaje -997 (-0x3e5) az do
    wygasniecia; to byl zgloszony objaw "MODE REFUSED przy APPLY". */
 kvideo_guard_watch(1234,567,3,1000,7);
 assert(kvideo_guard_active());
 assert(kvideo_guard_control(7,1,1001)<0);        /* op==1: powrot sie nie udal */
 assert(!kvideo_guard_active());                  /* ...ale bramka otwarta */
 assert(kvideo_guard_control(7,2,1002)==0);       /* nikt nie jest wlascicielem */
 /* Konsekwencja praktyczna: nowy podglad da sie uzbroic, czyli APPLY dziala. */
 kvideo_guard_watch(800,600,8,1100,9);
 assert(kvideo_guard_active());
 assert(kvideo_guard_control(9,0,1101)==0&&!kvideo_guard_active());
 puts("Video boot PASS: explicit opt-in, invalid config, no CPU gate, RAM/mode/timer guards and every fallback");return 0;
}
