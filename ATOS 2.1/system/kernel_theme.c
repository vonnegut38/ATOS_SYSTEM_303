/* System themes.  The shell's palette lives here, not in the desktop: the window
   manager draws its chrome from entries 16..19 and the compositor fills the
   desktop with entry 2, so a theme must be applied through one privileged path.
   The desktop only picks an index (OPTIONS -> SETTINGS -> THEMES). */
#include "kernel_theme.h"
#include "kernel_graphics.h"
#include "icons_ids.h"

/* The UI ramp lives in the LAST entries of the palette, after the TOS window's
   client range (236..251).  It used to sit directly after the icon palette -
   i.e. exactly where the TOS client palette goes - so every TOS program
   recoloured the desktop top bar. */
#define THEME_RAMP_BASE 252u

typedef struct {
    uint16_t sys[20];                    /* palette 0..19 */
    uint16_t ramp[KGFX_THEME_RAMP];      /* UI ramp at THEME_RAMP_BASE */
    uint8_t bar_gradient;                /* 1 = desktop draws a ramp top bar */
    uint8_t status_ink;                  /* text drawn on the desktop background */
} theme_t;

/* 0 = the look ATOS shipped with, kept verbatim so the picker can always go back.
   1 = dark shell: graphite desktop, steel top bar, accent-blue active titles;
       application windows stay light so every existing app stays readable. */
static const theme_t themes[]={
 { /* 0 CLASSIC */
   {0x000,0xfff,0x088,0x888,0x000,0xfff,0xfff,0x000,0x000,0xfff,0xfff,0xf00,
    0x333,0x555,0x777,0xaaa,0x005,0x009,0x00c,0x0ff},
   {0,0,0,0},                            /* flat black bar */
   0u,0u },
 { /* 1 STEEL */
   {0x000, /* 0 ink: app text stays dark, app bodies stay light */
    0xddd, /* 1 paper: near-white bodies + icon labels */
    0x122, /* 2 desktop: graphite */
    0x889, /* 3 secondary grey */
    0x000,0xfff,0xfff,0x000,0x000,0xfff,0xfff,0xf00,0x333,0x555,0x777,0xaaa,
    0x223, /* 16 frame shade */
    0x235, /* 17 inactive title */
    0x59f, /* 18 active title: accent blue */
    0x8cf},/* 19 accent bright */
   {0xaab,0x99a,0x889,0x778},             /* steel, light -> dark: the four
                                             steps the top bar gradient uses */
   1u,1u }
};
#define THEME_COUNT ((unsigned)(sizeof(themes)/sizeof(themes[0])))

static unsigned current;

unsigned kgfx_theme_count(void){return THEME_COUNT;}
unsigned kgfx_theme_current(void){return current;}
unsigned kgfx_theme_ramp_base(void){return THEME_RAMP_BASE;}

/* Kolor TLA wybrany przez uzytkownika (SETTINGS -> DESKTOP).  Nadpisuje
   wylacznie wpis 2 palety (tlo pulpitu), wiec motyw dalej rzadzi chromem okien
   i paskiem.  0xFFFF = brak nadpisania (wracamy do koloru motywu). */
static uint16_t desktop_override=0xffffu;
static void apply_bar_color(void);
static void apply_desktop_color(void)
{
    (void)kgfx_set_palette(2u,desktop_override==0xffffu?themes[current].sys[2]:desktop_override);
}
int kgfx_theme_apply(unsigned i)
{
    unsigned k;
    if(i>=THEME_COUNT)return -1;
    for(k=0;k<20u;++k)(void)kgfx_set_palette((uint8_t)k,themes[i].sys[k]);
    for(k=0;k<KGFX_THEME_RAMP;++k)
        (void)kgfx_set_palette((uint8_t)(THEME_RAMP_BASE+k),themes[i].ramp[k]);
    current=i;
    apply_desktop_color();
    apply_bar_color();
    return 0;
}
/* 0xFFFF przywraca kolor motywu.  Zwraca 0 / -1. */
int kgfx_theme_set_desktop(unsigned rgb)
{
    desktop_override=(rgb>=0x1000u)?0xffffu:(uint16_t)rgb;
    apply_desktop_color();
    return 0;
}
unsigned kgfx_theme_desktop(void)
{return (unsigned)(desktop_override==0xffffu?themes[current].sys[2]:desktop_override);}

/* Kolor PASKA GORNEGO (SETTINGS -> DESKTOP).  Pasek rysuje jadro z czterech
   wpisow rampy (THEME_RAMP_BASE..+3), przechodzac od jasniejszego do
   ciemniejszego - tutaj budujemy te cztery stopnie z jednego koloru, wiec
   wybor jest jednym "kolorem paska", a nie czterema recznie strojonymi. */
static uint16_t bar_base=0xffffu;
static uint16_t shade12(uint16_t rgb,int num,int den)
{
    unsigned r=(rgb>>8)&0xfu,g=(rgb>>4)&0xfu,b=rgb&0xfu;
    r=(unsigned)((int)r*num/den);g=(unsigned)((int)g*num/den);b=(unsigned)((int)b*num/den);
    if(r>15u)r=15u;
    if(g>15u)g=15u;
    if(b>15u)b=15u;
    return (uint16_t)((r<<8)|(g<<4)|b);
}
static void apply_bar_color(void)
{
    static const int num[4]={16,11,7,4};
    unsigned k;
    for(k=0;k<4u;++k)
        (void)kgfx_set_palette((uint8_t)(THEME_RAMP_BASE+k),
            bar_base==0xffffu?themes[current].ramp[k]:shade12(bar_base,num[k],16));
}
int kgfx_theme_set_bar(unsigned rgb)
{
    bar_base=(rgb>=0x1000u)?0xffffu:(uint16_t)rgb;
    apply_bar_color();
    {extern void kernel_wm_damage_all(void);extern void kernel_wm_desktop_repaint(void);kernel_wm_damage_all();kernel_wm_desktop_repaint();}
    return 0;
}
/* 0xFFFF = pasek w kolorze motywu (domyslnie). */
unsigned kgfx_theme_bar(void){return (unsigned)(bar_base==0xffffu?0xffffu:bar_base);}
/* Tapeta pulpitu: 0 = jednolite tlo, 1..3 = wzory proceduralne, 4 = WLASNY
   KAFELEK 16x16 (edytor SETTINGS -> TILE).  Trzymana w jadrze, zeby SETTINGS
   mogl ja zmienic, a pulpit zauwazyl zmiane (polling) i przemalowal tlo.
   UWAGA: zakres MUSI obejmowac 4 - wczesniejsze `n>3u?0u:n` zamienialo CUSTOM
   na SOLID, wiec kafelek wgrany do jadra nie byl nigdy rysowany. */
static unsigned wall_pattern;
int kgfx_theme_set_wall(unsigned n){wall_pattern=n>4u?0u:n;{extern void kernel_wm_damage_all(void);extern void kernel_wm_desktop_repaint(void);kernel_wm_damage_all();kernel_wm_desktop_repaint();}return 0;}
unsigned kgfx_theme_wall(void){return wall_pattern;}

static unsigned flags_of(unsigned i)
{return (unsigned)themes[i].status_ink|((unsigned)themes[i].bar_gradient<<8);}

unsigned kgfx_theme_flags(void){return flags_of(current);}
unsigned kgfx_theme_flags_of(unsigned i){return i<THEME_COUNT?flags_of(i):0u;}
unsigned kgfx_theme_ink_on_bar(void){return themes[current].bar_gradient?1u:0u;}

uint32_t kgfx_theme_preview(unsigned i)
{
    if(i>=THEME_COUNT)return 0;
    return (uint32_t)themes[i].sys[2]
         |((uint32_t)themes[i].sys[18]<<12)
         |((uint32_t)themes[i].ramp[0]<<24);
}
