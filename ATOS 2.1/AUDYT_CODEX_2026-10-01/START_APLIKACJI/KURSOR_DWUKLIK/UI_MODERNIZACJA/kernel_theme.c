/* System themes.  The shell's palette lives here, not in the desktop: the window
   manager draws its chrome from entries 16..19 and the compositor fills the
   desktop with entry 2, so a theme must be applied through one privileged path.
   The desktop only picks an index (OPTIONS -> SETTINGS -> THEMES). */
#include "kernel_theme.h"
#include "kernel_graphics.h"
#include "icons_color.h"

/* The UI ramp starts right after the generated icon palette, so adding icons can
   never overwrite a theme colour and vice versa. */
#define THEME_RAMP_BASE ((unsigned)ICON_PAL_BASE+(unsigned)ICON_PAL_COLORS)

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
   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
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
   {0xaab,0x99a,0x889,0x778,0x667,0x556,   /* 0..5  steel, light -> dark */
    0x59f,0x8bf,0xacf,0xcdf,               /* 6..9  accent ramp */
    0x111,0x222,0x333,0x444,0x555,0x666},  /* 10..15 neutrals */
   1u,1u }
};
#define THEME_COUNT ((unsigned)(sizeof(themes)/sizeof(themes[0])))

static unsigned current;

unsigned kgfx_theme_count(void){return THEME_COUNT;}
unsigned kgfx_theme_current(void){return current;}
unsigned kgfx_theme_ramp_base(void){return THEME_RAMP_BASE;}

int kgfx_theme_apply(unsigned i)
{
    unsigned k;
    if(i>=THEME_COUNT)return -1;
    for(k=0;k<20u;++k)(void)kgfx_set_palette((uint8_t)k,themes[i].sys[k]);
    for(k=0;k<KGFX_THEME_RAMP;++k)
        (void)kgfx_set_palette((uint8_t)(THEME_RAMP_BASE+k),themes[i].ramp[k]);
    current=i;
    return 0;
}

static unsigned flags_of(unsigned i)
{return (unsigned)themes[i].status_ink|((unsigned)themes[i].bar_gradient<<8);}

unsigned kgfx_theme_flags(void){return flags_of(current);}
unsigned kgfx_theme_flags_of(unsigned i){return i<THEME_COUNT?flags_of(i):0u;}

uint32_t kgfx_theme_preview(unsigned i)
{
    if(i>=THEME_COUNT)return 0;
    return (uint32_t)themes[i].sys[2]
         |((uint32_t)themes[i].sys[18]<<12)
         |((uint32_t)themes[i].ramp[0]<<24);
}
