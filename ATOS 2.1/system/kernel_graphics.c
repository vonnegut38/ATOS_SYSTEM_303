extern void kernel_ikbd_poll_capture(void);
#include "kernel_game.h"
#include "text_terminal.h"
#include "kernel_sessions.h"
#include "kernel_console.h"
#include "kernel_video.h"
#include "gui_damage.h"
#include "kernel_graphics.h"
#include "kgfx_planar4.h"
#include "kgfx_present16.h"
#include "kgfx_mono.h"
#include "st_raster.h"
#include "font8.h"
#include "chunky_raster.h"
#include "kernel_blitter.h"
#include "kernel_wm.h"
#include "kernel_memory.h"
#include "emutos_vdi_mono.h"
#include "font.h"
#include "hardware.h"
#include "emutos_resources.inc"
#include "icons_abi.h"
#include "png_icon.h"

/* chunky: 8 bits per pixel, one BYTE per pixel (800x600x256).  words still
   addresses the buffer, line_words is the pitch in 16-bit units (width/2), so
   a pixel lives at byte (y*pitch + x).  planes is 1 in both formats. */
typedef struct {volatile uint16_t *words;uint16_t width,height,line_words;uint8_t planes,chunky;} surface_t;
/* 1024x768 mono = 128 bytes/row, the widest compositor frame. */
/* 1024x768 mono = 98304 B; 800x600x256 chunky = 480000 B; 1024x768x256 = 786432 B. */
#define KGFX_MAX_FRAME_BYTES 786432u
static kgfx_info_t graphics;
static term_display_t terminal_display;
/* Retained text frames: a session switch must not depend on its app waking. */
/* Bufory danych graficznych mieszkaja w TT-RAM (alokacja w kgfx_init): w ST-RAM
   jest tylko 4 MB, a to dane uzywane wylacznie przez CPU - bez DMA i blittera. */
#define DYN_ICON_MAX 8u
/* Dane ikon (254 KB) leza w pliku i w TT-RAM; tu tylko wskazniki. */
static const icon_color_t *icon_color_table;
static const uint16_t *icon_color_palette;
static uint16_t icon_color_count;
/* BIEZACY rozmiar ikon (32/48/64) - wybiera go uzytkownik w SETTINGS, a jadro
   dekoduje odpowiednia sekcje ICONS.DAT.  icon_want_px to zyczenie na nastepne
   kgfx_icons_load() (kernel_main czyta je z \SYSTEM\ICONSZ.CFG). */
static uint16_t icon_px=ICON_SIZE,icon_mask_bytes=ICON_MASK_BYTES,icon_pixel_bytes=ICON_PIXEL_BYTES;
static uint16_t icon_want_px=ICON_SIZE;
static void *icon_alloc_table,*icon_alloc_data;   /* do zwolnienia przy zmianie rozmiaru */
/* --- Drugi cache: ikony 16 px dla paska na dole (dock) --------------------
   Pulpit rysuje ikony w rozmiarze wybranym w SETTINGS (32/48/64), a pasek
   potrzebuje 16 px JEDNOCZESNIE.  Dekodujemy wiec sekcje 16 px raz, na starcie,
   i trzymamy ja w osobnych buforach; rysowanie uzywa kgfx_icon_small_draw().
   Wskazniki tabeli 16 px wskazuja do wlasnego bufora danych, wiec zmiana
   glownego rozmiaru ich nie rusza. */
#define ICON_SMALL_PX 16u
static icon_color_t icon_small_table[ICON_COLOR_COUNT];
static uint16_t icon_small_count;
uint32_t kdbg_icon_small;   /* ile ikon w cache 16 px (diagnostyka) */
static void *icon_small_alloc_table,*icon_small_alloc_data;
static term_cell_t (*session_cells)[TERM_CELLS];
static uint8_t (*dyn_mask)[ICON_MAX_MASK_BYTES];
static uint8_t (*dyn_pixels)[ICON_MAX_PIXEL_BYTES];
static uint8_t *png_rgba,*png_work;
static uint8_t session_cells_valid[SESSION_COUNT];
static unsigned presented_session;
static uint8_t terminal_font[256][16],terminal_font_ready;
void kgfx_terminal_invalidate(void)
{term_display_invalidate(&terminal_display);if(!kgfx_desktop_active())kconsole_invalidate();}
static kgfx_rect_t dirty[KGFX_DIRTY_MAX];
static uint8_t dirty_count;
static volatile uint16_t cursor_mono_background[16u*2u*4u];
/* 8 bpp pointer: one byte per pixel, 16x16 worst case. */
static uint8_t cursor_chunky_background[16u*16u];
static int16_t cursor_x,cursor_y;
static uint8_t cursor_width,cursor_height,cursor_is_visible;
static volatile uint8_t *desktop_framebuffer;
static volatile uint8_t *desktop_backbuffer;
static uint8_t desktop_is_active,desktop_hidden;
static uint16_t console_palette[16];
/* The buffer kvideo_flush() copies into the adapter. While the text console
   owns the ET4000 it renders straight into the card VRAM, so the session
   overlay and the mouse cursor must target that same buffer; pointing them at
   the (unscanned) desktop_framebuffer made the indicator and pointer vanish on
   the card in the text profile. Falls back to the Shifter base. */
static volatile uint16_t *system_scanout(void)
{
    const void *base=kvideo_front();
    return (volatile uint16_t *)(uintptr_t)(base?base:(const void *)(uintptr_t)SCREEN_BASE);
}
static const uint16_t cursor_shape[16]={
    0x8000u,0xc000u,0xe000u,0xf000u,0xf800u,0xfc00u,0xfe00u,0xff00u,
    0xff80u,0xfc00u,0xdc00u,0x8e00u,0x0600u,0x0700u,0x0300u,0x0000u
};
/* Pointer colours.  The chunky path used to XOR bit 0 of the pixel under the
   cursor, which made the pointer nearly invisible over a busy background (the
   48x48 colour icons: "our mouse cursor is switched off entirely").  It now
   writes real palette entries - paper inside the shape, ink around it - so it
   stays readable on light, dark and dithered backgrounds alike.  Entries 0/1
   are the theme's ink/paper pair and the TOS personality pins the same two
   meanings for entry 0, so the pointer survives a TOS program's palette too. */
#define CURSOR_FILL 1u
#define CURSOR_OUTLINE 0u
/* Keep the freestanding 68000 build from turning the short cursor save/restore
   loops into a libc memcpy call. */
static __attribute__((noinline)) void cursor_mono_save(uint16_t index,uint16_t value)
{cursor_mono_background[index]=value;}
static __attribute__((noinline)) uint16_t cursor_mono_load(uint16_t index)
{return cursor_mono_background[index];}

static int clip_rect(int32_t *x,int32_t *y,int32_t *width,int32_t *height)
{
    if(*width<=0||*height<=0)return 0;
    /* Avoid negating INT32_MIN: adding a negative origin to a known-positive
       extent clips the same amount without an overflowing intermediate. */
    if(*x<0){if(*x<=-*width)return 0;*width+=*x;*x=0;}
    if(*y<0){if(*y<=-*height)return 0;*height+=*y;*y=0;}
    if(*x>=graphics.width||*y>=graphics.height)return 0;
    if(*width>(int32_t)graphics.width-*x)*width=(int32_t)graphics.width-*x;
    if(*height>(int32_t)graphics.height-*y)*height=(int32_t)graphics.height-*y;
    return *width>0&&*height>0;
}

/* Clip source and destination as one translated rectangle.  Cropping either
   side advances the opposite origin by the same amount, preserving the exact
   source/destination pixel mapping required by an overlap-safe blit. */
static int clip_copy_rect(int32_t *sx,int32_t *sy,int32_t *width,int32_t *height,
                          int32_t *dx,int32_t *dy)
{
    int32_t cut;
    if(*width<=0||*height<=0)return 0;
    if(*sx<0){if(*sx<=-*width)return 0;cut=-*sx;*sx=0;*dx+=cut;*width-=cut;}
    if(*sy<0){if(*sy<=-*height)return 0;cut=-*sy;*sy=0;*dy+=cut;*height-=cut;}
    if(*dx<0){if(*dx<=-*width)return 0;cut=-*dx;*dx=0;*sx+=cut;*width-=cut;}
    if(*dy<0){if(*dy<=-*height)return 0;cut=-*dy;*dy=0;*sy+=cut;*height-=cut;}
    if(*sx>=graphics.width||*dx>=graphics.width||
       *sy>=graphics.height||*dy>=graphics.height)return 0;
    if(*width>(int32_t)graphics.width-*sx)*width=(int32_t)graphics.width-*sx;
    if(*width>(int32_t)graphics.width-*dx)*width=(int32_t)graphics.width-*dx;
    if(*height>(int32_t)graphics.height-*sy)*height=(int32_t)graphics.height-*sy;
    if(*height>(int32_t)graphics.height-*dy)*height=(int32_t)graphics.height-*dy;
    return *width>0&&*height>0;
}

static void mark_dirty(int32_t x,int32_t y,int32_t width,int32_t height)
{
    uint8_t i;int32_t right,bottom,old_right,old_bottom;
    if(!clip_rect(&x,&y,&width,&height))return;
    kvideo_damage(x,y,width,height);
    right=x+width;bottom=y+height;
    for(i=0;i<dirty_count;++i){
        old_right=dirty[i].x+dirty[i].width;old_bottom=dirty[i].y+dirty[i].height;
        if(x<=old_right&&right>=dirty[i].x&&y<=old_bottom&&bottom>=dirty[i].y){
            if(x>dirty[i].x)x=dirty[i].x;
            if(y>dirty[i].y)y=dirty[i].y;
            if(right<old_right)right=old_right;
            if(bottom<old_bottom)bottom=old_bottom;
            dirty[i].x=(int16_t)x;dirty[i].y=(int16_t)y;
            dirty[i].width=(uint16_t)(right-x);dirty[i].height=(uint16_t)(bottom-y);return;
        }
    }
    if(dirty_count==KGFX_DIRTY_MAX){dirty_count=1;dirty[0].x=dirty[0].y=0;
        dirty[0].width=graphics.width;dirty[0].height=graphics.height;return;}
    dirty[dirty_count].x=(int16_t)x;dirty[dirty_count].y=(int16_t)y;
    dirty[dirty_count].width=(uint16_t)width;dirty[dirty_count].height=(uint16_t)height;
    ++dirty_count;
}

static uint8_t surface_pixel_get(const surface_t *surface,uint16_t x,uint16_t y)
{
    if(surface->chunky)return *(volatile uint8_t *)surface->words+
        (uint32_t)y*(uint32_t)surface->line_words*2u+(uint32_t)x;
    {
    uint16_t group=(uint16_t)(x>>4),mask=(uint16_t)(0x8000u>>(x&15u));
    volatile uint16_t *word=surface->words+(uint32_t)y*surface->line_words+
                            (uint32_t)group*surface->planes;
    uint8_t plane,color=0;
    for(plane=0;plane<surface->planes;++plane)if(word[plane]&mask)color|=(uint8_t)(1u<<plane);
    return color;
    }
}

static void surface_pixel_set(const surface_t *surface,uint16_t x,uint16_t y,uint8_t color)
{
    if(surface->chunky){
        volatile uint8_t *p=(volatile uint8_t *)surface->words+
            (uint32_t)y*(uint32_t)surface->line_words*2u+(uint32_t)x;
        *p=color;return;
    }
    {
    uint16_t group=(uint16_t)(x>>4),mask=(uint16_t)(0x8000u>>(x&15u));
    volatile uint16_t *word=surface->words+(uint32_t)y*surface->line_words+
                            (uint32_t)group*surface->planes;
    uint8_t plane;
    for(plane=0;plane<surface->planes;++plane){
        if(color&(uint8_t)(1u<<plane))word[plane]|=mask;else word[plane]&=(uint16_t)~mask;
    }
    }
}

static void surface_fill_reference(const surface_t *surface,uint16_t x,uint16_t y,
                                   uint16_t width,uint16_t height,uint8_t color)
{
    uint16_t first=(uint16_t)(x>>4),last=(uint16_t)((x+width-1u)>>4),row,group,mask;
    uint16_t left=(uint16_t)(0xffffu>>(x&15u));
    uint16_t right=(uint16_t)(0xffffu<<(15u-((x+width-1u)&15u)));
    uint8_t plane;volatile uint16_t *word;
    for(row=y;row<(uint16_t)(y+height);++row)for(group=first;group<=last;++group){
        mask=0xffffu;if(group==first)mask&=left;if(group==last)mask&=right;
        word=surface->words+(uint32_t)row*surface->line_words+(uint32_t)group*surface->planes;
        for(plane=0;plane<surface->planes;++plane){
            if(color&(uint8_t)(1u<<plane))word[plane]|=mask;
            else word[plane]&=(uint16_t)~mask;
        }
    }
}

static void surface_fill(const surface_t *surface,uint16_t x,uint16_t y,
                         uint16_t width,uint16_t height,uint8_t color)
{
    static const uint16_t solid[1]={0xffffu};
    if(surface->chunky){
        chunky_fill((volatile uint8_t *)surface->words+
            (uint32_t)y*surface->line_words*2u+x,
            (uint32_t)surface->line_words*2u,width,height,color);
        return;
    }
    emutos_vdi_planar_fill(surface->words,surface->line_words,surface->planes,
        x,y,width,height,color,0,solid,0);
}

static surface_t active_surface(void)
{
    surface_t result;result.words=(volatile uint16_t *)(uintptr_t)graphics.framebuffer;
    result.width=graphics.width;result.height=graphics.height;
    result.line_words=(uint16_t)(graphics.line_bytes/2u);result.planes=graphics.planes;
    result.chunky=(uint8_t)(graphics.mode==8u);   /* 8bpp chunky (256 colours) */
    return result;
}

/* Fast memmove-equivalent path for 16-pixel-aligned one-plane rectangles.
   Volatile word accesses keep the freestanding compiler from introducing a
   libc call and preserve the exact overlap-safe traversal order. */
static void surface_copy_mono_words_reference(const surface_t *surface,uint16_t sx,uint16_t sy,
                                              uint16_t width,uint16_t height,uint16_t dx,uint16_t dy)
{
    int32_t row,row_end,row_step,group,group_end,group_step;
    uint16_t groups=(uint16_t)(width>>4);volatile uint16_t *source,*destination;
    if(dy>sy){row=(int32_t)height-1;row_end=-1;row_step=-1;}
    else{row=0;row_end=height;row_step=1;}
    if(dx>sx){group_end=-1;group_step=-1;}
    else{group_end=groups;group_step=1;}
    for(;row!=row_end;row+=row_step){
        source=surface->words+(uint32_t)(sy+row)*surface->line_words+(sx>>4);
        destination=surface->words+(uint32_t)(dy+row)*surface->line_words+(dx>>4);
        for(group=(group_step<0)?(int32_t)groups-1:0;group!=group_end;group+=group_step)
            destination[group]=source[group];
    }
}

static void surface_copy_mono_words(const surface_t *surface,uint16_t sx,uint16_t sy,
                                    uint16_t width,uint16_t height,uint16_t dx,uint16_t dy)
{
    emutos_vdi_mono_copy_words(surface->words,surface->line_words,sx,sy,width,height,dx,dy);
}

static uint16_t surface_mono_word_reference(const surface_t *surface,uint16_t row,int32_t group)
{
    if(group<0||group>=surface->line_words)return 0u;
    return surface->words[(uint32_t)row*surface->line_words+(uint32_t)group];
}

/* Shifted one-plane copy. One complete destination scanline is assembled
   before any write, so horizontal overlap cannot alter unread source bits.
   Only the first and last destination words are masked. */
static void surface_copy_mono_shifted_reference(const surface_t *surface,uint16_t sx,uint16_t sy,
                                      uint16_t width,uint16_t height,uint16_t dx,uint16_t dy)
{
    uint16_t assembled[40];uint16_t first=(uint16_t)(dx>>4);
    uint16_t last=(uint16_t)((dx+width-1u)>>4),count=(uint16_t)(last-first+1u);
    uint16_t row_index,group,mask,left=(uint16_t)(0xffffu>>(dx&15u));
    uint16_t right=(uint16_t)(0xffffu<<(15u-((dx+width-1u)&15u)));
    int32_t row,row_end,row_step,source_x,source_group;uint8_t shift;
    volatile uint16_t *destination;uint16_t high,low;
    if(dy>sy){row=(int32_t)height-1;row_end=-1;row_step=-1;}
    else{row=0;row_end=height;row_step=1;}
    for(;row!=row_end;row+=row_step){
        row_index=(uint16_t)(sy+row);
        for(group=0;group<count;++group){
            source_x=(int32_t)sx+(int32_t)(first+group)*16-(int32_t)dx;
            if(source_x<0){source_group=-1;shift=(uint8_t)(source_x+16);}
            else{source_group=source_x>>4;shift=(uint8_t)(source_x&15);}
            high=surface_mono_word_reference(surface,row_index,source_group);
            if(!shift)assembled[group]=high;
            else{low=surface_mono_word_reference(surface,row_index,source_group+1);
                assembled[group]=(uint16_t)((uint16_t)(high<<shift)|(uint16_t)(low>>(16u-shift)));}
        }
        destination=surface->words+(uint32_t)(dy+row)*surface->line_words+first;
        for(group=0;group<count;++group){
            mask=0xffffu;if(!group)mask&=left;if(group==count-1u)mask&=right;
            destination[group]=(uint16_t)((destination[group]&~mask)|(assembled[group]&mask));
        }
    }
}

static void surface_copy_mono_shifted(const surface_t *surface,uint16_t sx,uint16_t sy,
                                      uint16_t width,uint16_t height,uint16_t dx,uint16_t dy)
{
    emutos_vdi_mono_copy_shifted(surface->words,surface->line_words,sx,sy,width,height,dx,dy);
}

typedef struct {int16_t x,y;uint8_t active;} cursor_guard_t;

static int cursor_intersects(int32_t x,int32_t y,int32_t width,int32_t height)
{
    int32_t right,bottom,cursor_right,cursor_bottom;
    if(!cursor_is_visible||width<=0||height<=0)return 0;
    right=x+width;bottom=y+height;
    cursor_right=(int32_t)cursor_x+cursor_width;
    cursor_bottom=(int32_t)cursor_y+cursor_height;
    return x<cursor_right&&right>cursor_x&&y<cursor_bottom&&bottom>cursor_y;
}

static cursor_guard_t cursor_guard_begin(int32_t x,int32_t y,int32_t width,int32_t height)
{
    kgfx_overlay_hide();
    cursor_guard_t guard;guard.x=cursor_x;guard.y=cursor_y;guard.active=0;
    if(cursor_intersects(x,y,width,height)){guard.active=1;kgfx_cursor_hide();}
    return guard;
}

static void cursor_guard_end(cursor_guard_t guard)
{if(guard.active)kgfx_cursor_show(guard.x,guard.y);}

/* The system palette.  Entry 2 is the colour-mode desktop background (a
   Windows 95 style teal) so the 256-colour desktop is obvious at a glance.
   It has to be re-applied once the card owns the screen, because at kgfx_init()
   the adapter is not marked active yet and the RAMDAC would keep its defaults. */
void kgfx_palette_defaults(void)
{
    static const uint16_t default_palette[20]={0x000,0xfff,0x088,0x888,0x000,0xfff,0xfff,0x000,
         0x000,0xfff,0xfff,0xf00,0x333,0x555,0x777,0xaaa,
         0x005,0x009,0x00c,0x0ff};  /* 16..19 = Norton-Commander blue theme */
    uint8_t pi;
    for(pi=0;pi<20u;++pi)(void)kgfx_set_palette(pi,default_palette[pi]);
    kgfx_icon_palette();   /* colour icons: RAMDAC entries 20.. */
}
int kgfx_init(uint8_t mode,uint8_t blitter_present,uint8_t blitter_enabled)
{
    if(mode>2u)return 0;
    /* Jednorazowo: bufory danych w TT-RAM (patrz deklaracje wyzej). */
    if(!session_cells)session_cells=(term_cell_t (*)[TERM_CELLS])kmem_alloc(
        (uint32_t)sizeof(term_cell_t)*(uint32_t)SESSION_COUNT*(uint32_t)TERM_CELLS,16u,
        KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!dyn_mask)dyn_mask=(uint8_t (*)[ICON_MAX_MASK_BYTES])kmem_alloc(
        DYN_ICON_MAX*ICON_MAX_MASK_BYTES,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!dyn_pixels)dyn_pixels=(uint8_t (*)[ICON_MAX_PIXEL_BYTES])kmem_alloc(
        DYN_ICON_MAX*ICON_MAX_PIXEL_BYTES,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!png_rgba)png_rgba=(uint8_t *)kmem_alloc(96u*96u*4u,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!png_work)png_work=(uint8_t *)kmem_alloc(65536u,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    /* Brak pamieci na te bufory to blad inicjalizacji grafiki (jadro to zglasza
       przy starcie), dlatego sciezki rysowania nie musza ich sprawdzac. */
    if(!session_cells||!dyn_mask||!dyn_pixels||!png_rgba||!png_work)return 0;
    graphics.mode=mode;graphics.framebuffer=(volatile uint8_t *)(uintptr_t)SCREEN_BASE;
    kvideo_reset(graphics.framebuffer);
    if(mode==0u){graphics.width=320u;graphics.height=200u;graphics.planes=4u;graphics.line_bytes=160u;}
    else if(mode==1u){graphics.width=640u;graphics.height=200u;graphics.planes=2u;graphics.line_bytes=160u;}
    else{graphics.width=640u;graphics.height=400u;graphics.planes=1u;graphics.line_bytes=80u;}
    /* Program the system palette before anything draws: the 1 bpp native
       renderers read entries 0/1 from it, and an application recolours the
       whole system by changing these entries (OPTIONS -> THEME). */
    /* Entry 2 is the "colour mode" background: a Windows 95 style teal, so the
       256-colour desktop is visibly different from the mono one at a glance.
       Programmed through kgfx_set_palette() so the card RAMDAC gets it too. */
    kgfx_palette_defaults();
    kblit_init(blitter_present,blitter_enabled);
    /* Accelerator cards are known to disagree with the blitter (TF536 reports
       exist), so verify it before any code path relies on it. On failure the
       kernel silently falls back to the CPU fill. */
    if(blitter_present&&blitter_enabled&&!kblit_self_test())kblit_init(blitter_present,0u);
    graphics.capabilities=KGFX_CAP_LINE;
    if(mode!=2u)graphics.capabilities|=KGFX_CAP_PALETTE;
    if(kblit_present())graphics.capabilities|=KGFX_CAP_BLITTER_PRESENT;
    if(kblit_enabled())graphics.capabilities|=KGFX_CAP_BLITTER_ENABLED;
    if(kblit_copy_ready())graphics.capabilities|=KGFX_CAP_BLITTER_COPY;
    if(mode==2u)graphics.capabilities|=KGFX_CAP_MONO_WORD_COPY;
    (void)kblit_self_test();
    if(!kblit_enabled())graphics.capabilities&=(uint16_t)~KGFX_CAP_BLITTER_ENABLED;
    /* Allocate the widest mode the adapter can select (1024x768 mono = 98,304
       bytes) so a live resolution switch never moves the compositor buffer. */
    /* TT-RAM: bufor jest wylacznie rysowany przez CPU (blitter i tak jest
       wylaczony na 030), a 768 KB w ST-RAM to 1/5 calego ST-RAM.  Staly adres
       i przypiecie dziala tak samo w TT, wiec zmiana trybu go nie przenosi. */
    desktop_framebuffer=(volatile uint8_t *)kmem_alloc(KGFX_MAX_FRAME_BYTES,256u,KMEM_OWNER_KERNEL,
                                                       KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);
    if(desktop_framebuffer&&kmem_pin((void *)desktop_framebuffer,KMEM_OWNER_KERNEL))
        graphics.capabilities|=KGFX_CAP_DESKTOP_BUFFER;
    else desktop_framebuffer=0;
    desktop_is_active=0;
    cursor_is_visible=0;cursor_width=cursor_height=0;kgfx_dirty_reset();return 1;
}

int kgfx_self_test(void)
{
    uint16_t storage[6]={0x55aau,0,0,0,0,0xaa55u};
    uint16_t planar[6]={0x33ccu,0,0,0,0,0xcc33u};
    uint16_t copy_words[12],copy_reference_words[12];
    uint16_t fast_words[8],reference_words[8],emutos_words[8],fill_reference[8];
    surface_t test={storage+1,16u,4u,1u,1u,0u};
    surface_t four_plane={planar+1,16u,1u,4u,4u,0u};surface_t live;uint8_t i,original,replacement,max_color;
    int32_t csx,csy,cw,ch,cdx,cdy;
    surface_t copy_test={copy_words,64u,3u,4u,1u,0u};
    surface_t copy_reference_test={copy_reference_words,64u,3u,4u,1u,0u};
    surface_t fast_test={fast_words,64u,2u,4u,1u,0u};
    surface_t reference_test={reference_words,64u,2u,4u,1u,0u};
    surface_t emutos_fill_test={emutos_words,64u,2u,4u,1u,0u};
    surface_t reference_fill_test={fill_reference,64u,2u,4u,1u,0u};
    for(i=0;i<12u;++i){copy_words[i]=(uint16_t)(i+1u);copy_reference_words[i]=copy_words[i];}
    surface_copy_mono_words(&copy_test,0u,0u,48u,1u,16u,0u);
    surface_copy_mono_words_reference(&copy_reference_test,0u,0u,48u,1u,16u,0u);
    for(i=0;i<12u;++i)if(copy_words[i]!=copy_reference_words[i])return 0;
    if(copy_words[0]!=1u||copy_words[1]!=1u||copy_words[2]!=2u||copy_words[3]!=3u)return 0;
    for(i=0;i<12u;++i){copy_words[i]=(uint16_t)(i+1u);copy_reference_words[i]=copy_words[i];}
    surface_copy_mono_words(&copy_test,0u,0u,64u,2u,0u,1u);
    surface_copy_mono_words_reference(&copy_reference_test,0u,0u,64u,2u,0u,1u);
    for(i=0;i<12u;++i)if(copy_words[i]!=copy_reference_words[i])return 0;
    if(copy_words[4]!=1u||copy_words[5]!=2u||copy_words[6]!=3u||copy_words[7]!=4u||
       copy_words[8]!=5u||copy_words[9]!=6u||copy_words[10]!=7u||copy_words[11]!=8u)return 0;
    for(i=0;i<12u;++i){copy_words[i]=(uint16_t)(0x1000u+i);copy_reference_words[i]=copy_words[i];}
    surface_copy_mono_words(&copy_test,16u,1u,48u,2u,0u,0u);
    surface_copy_mono_words_reference(&copy_reference_test,16u,1u,48u,2u,0u,0u);
    for(i=0;i<12u;++i)if(copy_words[i]!=copy_reference_words[i])return 0;
    for(i=0;i<8u;++i){fast_words[i]=(uint16_t)(0x1111u*(i+1u));reference_words[i]=fast_words[i];}
    surface_copy_mono_shifted_reference(&reference_test,3u,0u,47u,1u,11u,0u);
    surface_copy_mono_shifted(&fast_test,3u,0u,47u,1u,11u,0u);
    for(i=0;i<8u;++i)if(fast_words[i]!=reference_words[i])return 0;
    for(i=0;i<8u;++i){fast_words[i]=(uint16_t)(0x8421u^(uint16_t)(i*0x1234u));
        reference_words[i]=fast_words[i];}
    surface_copy_mono_shifted_reference(&reference_test,9u,0u,49u,1u,2u,1u);
    surface_copy_mono_shifted(&fast_test,9u,0u,49u,1u,2u,1u);
    for(i=0;i<8u;++i)if(fast_words[i]!=reference_words[i])return 0;
    for(i=0;i<8u;++i){fast_words[i]=(uint16_t)(0x9249u^(uint16_t)(i*0x2345u));
        reference_words[i]=fast_words[i];}
    surface_copy_mono_shifted(&fast_test,12u,1u,37u,1u,1u,0u);
    surface_copy_mono_shifted_reference(&reference_test,12u,1u,37u,1u,1u,0u);
    for(i=0;i<8u;++i)if(fast_words[i]!=reference_words[i])return 0;
    /* Differential guard for the imported EmuTOS solid-fill path: cover
       single-word masks, multiple centre words, set and clear operations. */
    for(i=0;i<8u;++i){emutos_words[i]=(uint16_t)(0xa55au^(uint16_t)(i*0x1111u));
        fill_reference[i]=emutos_words[i];}
    surface_fill(&emutos_fill_test,3u,0u,5u,1u,1u);
    surface_fill_reference(&reference_fill_test,3u,0u,5u,1u,1u);
    surface_fill(&emutos_fill_test,11u,1u,43u,1u,1u);
    surface_fill_reference(&reference_fill_test,11u,1u,43u,1u,1u);
    surface_fill(&emutos_fill_test,19u,0u,29u,2u,0u);
    surface_fill_reference(&reference_fill_test,19u,0u,29u,2u,0u);
    for(i=0;i<8u;++i)if(emutos_words[i]!=fill_reference[i])return 0;
    csx=-3;csy=2;cw=20;ch=10;cdx=5;cdy=-2;
    if(!clip_copy_rect(&csx,&csy,&cw,&ch,&cdx,&cdy)||
       csx!=0||csy!=4||cdx!=8||cdy!=0||cw!=17||ch!=8)return 0;
    csx=-20;csy=0;cw=20;ch=1;cdx=0;cdy=0;
    if(clip_copy_rect(&csx,&csy,&cw,&ch,&cdx,&cdy))return 0;
    surface_fill(&test,0,0,2,2,1);
    if(storage[0]!=0x55aau||storage[5]!=0xaa55u||storage[1]!=0xc000u||storage[2]!=0xc000u)return 0;
    surface_pixel_set(&test,15,3,1);
    if(surface_pixel_get(&test,15,3)!=1u||storage[4]!=1u)return 0;
    surface_pixel_set(&four_plane,0,0,0x0au);
    if(planar[0]!=0x33ccu||planar[5]!=0xcc33u||planar[1]!=0u||
       planar[2]!=0x8000u||planar[3]!=0u||planar[4]!=0x8000u||
       surface_pixel_get(&four_plane,0,0)!=0x0au)return 0;
    surface_fill(&four_plane,4,0,4,1,0x05u);
    if(surface_pixel_get(&four_plane,4,0)!=0x05u||
       surface_pixel_get(&four_plane,0,0)!=0x0au)return 0;
    kgfx_dirty_reset();mark_dirty(-2,-2,4,4);mark_dirty(2,0,2,2);
    if(dirty_count!=1u||dirty[0].x!=0||dirty[0].y!=0||
       dirty[0].width!=4u||dirty[0].height!=2u)return 0;
    kgfx_dirty_reset();
    for(i=0;i<=KGFX_DIRTY_MAX;++i)mark_dirty((int32_t)i*2,10,1,1);
    if(dirty_count!=1u||dirty[0].width!=graphics.width||
       dirty[0].height!=graphics.height)return 0;
    /* Verify that an overlapping primitive updates the saved background,
       without leaving either the cursor or the temporary pixel on screen. */
    live=active_surface();max_color=(uint8_t)((1u<<live.planes)-1u);
    original=surface_pixel_get(&live,0,0);replacement=(uint8_t)(original^max_color);
    kgfx_cursor_show(0,0);kgfx_pixel(0,0,replacement);
    if(surface_pixel_get(&live,0,0)!=original){kgfx_cursor_hide();return 0;}
    kgfx_cursor_hide();
    if(surface_pixel_get(&live,0,0)!=replacement){surface_pixel_set(&live,0,0,original);return 0;}
    surface_pixel_set(&live,0,0,original);
    kgfx_dirty_reset();return 1;
}

const kgfx_info_t *kgfx_get_info(void){return &graphics;}
/* Requests-per-frame measurement (ETAP 2.6).  The broker counts every serviced
   GFX request; the compositor samples the counter once per composed frame, so
   the desktop can report the real dispatch load instead of guessing it. */
static uint32_t gfx_requests;
void kgfx_note_request(void){++gfx_requests;}
uint32_t kgfx_request_count(void){return gfx_requests;}
/* The surface the compositor presents: the desktop frame that kvideo_select()
   scans out.  Drawing that has to appear on screen - a TOS program's window
   client, for example - must land here and NOT in kgfx_set_target()'s
   application buffer, which the compositor overwrites from its own back
   buffer on the next flush. */
uint8_t *kgfx_present_target(uint16_t *pitch,uint8_t *bpp)
{
    uint8_t *target=(uint8_t *)(uintptr_t)desktop_framebuffer;
    if(!target)target=(uint8_t *)graphics.framebuffer;
    if(pitch)*pitch=graphics.line_bytes;
    if(bpp)*bpp=(uint8_t)((graphics.mode==8u)?8u:1u);
    return target;
}
uint8_t *kgfx_set_target(uint8_t *target)
{
    uint8_t *previous=(uint8_t *)graphics.framebuffer;
    if(target)graphics.framebuffer=target;
    /* The console renderer keeps its own per-target validity (kgfx_console_render
       compares last_target), so switching the batch target must NOT invalidate
       the console cache: doing so repainted the whole shell window after every
       application draw. */
    kgfx_dirty_reset();
    return previous;
}
static uint8_t console_render_force;
/* Switching targets/settings: the next render repaints the whole grid. */
void kgfx_console_force_redraw(void){console_render_force=1u;}
int kgfx_console_render(const void *cells,uint8_t *target,
                        uint16_t *dx,uint16_t *dy,uint16_t *dw,uint16_t *dh)
{
    const term_cell_t *c=(const term_cell_t *)cells;
    uint16_t x,y,minx=TERM_COLS,miny=TERM_ROWS,maxx=0,maxy=0;int any=0;
    static const uint8_t *last_target;
    if(!c||!target)return 0;
    if(console_render_force){console_render_force=0;last_target=0;}
    /* The cell cache belongs to ONE target: without this, a window would only
       receive the cells that differ from the other window's grid. */
    if(last_target!=target){last_target=target;term_display_invalidate(&terminal_display);}
    if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
    /* Bounding box of the cells that actually changed: only that band is
       re-blitted by the compositor. */
    if(terminal_display.valid){
        for(y=0;y<TERM_ROWS;++y)for(x=0;x<TERM_COLS;++x)
            if(terminal_display.cells[y*TERM_COLS+x]!=c[y*TERM_COLS+x]){
                if(x<minx)minx=x;
                if(x>maxx)maxx=x;
                if(y<miny)miny=y;
                if(y>maxy)maxy=y;
                any=1;}
    }else{any=1;minx=0;miny=0;maxx=TERM_COLS-1;maxy=TERM_ROWS-1;}
    if(graphics.mode==8u)
        term_render_chunky_polled(&terminal_display,c,(volatile uint8_t *)target,
                                  graphics.line_bytes,terminal_font,kernel_ikbd_poll_capture);
    else
        term_render_mono_polled(&terminal_display,c,(volatile uint16_t *)target,terminal_font,kernel_ikbd_poll_capture);
    if(any){
        if(dx)*dx=(uint16_t)(minx*8u);
        if(dy)*dy=(uint16_t)(miny*16u);
        if(dw)*dw=(uint16_t)((maxx-minx+1u)*8u);
        if(dh)*dh=(uint16_t)((maxy-miny+1u)*16u);}
    return any;
}
uint8_t *kgfx_framebuffer(uint16_t *pitch,uint8_t *bpp)
{
    /* Return the LIVE framebuffer: the graphics layer switches it (console,
       desktop, card VRAM in VOFA mode). Returning the cached desktop buffer
       made the compositor and kvideo_flush use DIFFERENT buffers, so drawn
       content never reached the screen. */
    if(!graphics.framebuffer)return 0;
    if(pitch)*pitch=graphics.line_bytes;
    /* The depth comes from the MODE, not from the plane count: chunky 8 bpp also
       has planes==1, so deriving it from planes reported 1 bpp to the
       compositor - which then painted mono rows into an 800-byte chunky line
       (a narrow strip on screen). */
    if(bpp)*bpp=(uint8_t)((graphics.mode==8u)?8u:1u);
    return (uint8_t *)graphics.framebuffer;
}
void kgfx_dirty_reset(void){dirty_count=0;}
uint8_t kgfx_dirty_count(void){return dirty_count;}
int kgfx_dirty_get(uint8_t index,kgfx_rect_t *rectangle)
{if(index>=dirty_count||!rectangle)return 0;*rectangle=dirty[index];return 1;}

void kgfx_clear(uint8_t color)
{surface_t surface=active_surface();cursor_guard_t guard=cursor_guard_begin(0,0,graphics.width,graphics.height);
 surface_fill(&surface,0,0,graphics.width,graphics.height,color);mark_dirty(0,0,graphics.width,graphics.height);
 cursor_guard_end(guard);}
void kgfx_pixel(int32_t x,int32_t y,uint8_t color)
{surface_t surface;cursor_guard_t guard;if(x<0||y<0||x>=graphics.width||y>=graphics.height)return;
 guard=cursor_guard_begin(x,y,1,1);surface=active_surface();
 surface_pixel_set(&surface,(uint16_t)x,(uint16_t)y,color);mark_dirty(x,y,1,1);cursor_guard_end(guard);}
/* Wypelnienie prostokata WZOREM (tapeta pulpitu).  Rysuje bezposrednio w
   aktywna powierzchnie, wiec jest to jedna operacja na wiersz zamiast setek
   prostokatow w liscie rysowania.  Wzory uzywaja dwoch wpisow palety: 2 (tlo,
   kolor wybrany w SETTINGS) i 3 (akcent z motywu), wiec zmiana koloru tla
   dziala tez na tapete.  0 = jednolite tlo. */
/* WLASNY KAFELEK 16x16 (1 bpp, 32 B = 16 wierszy po 2 bajty).  Wzory 1..3 sa
   proceduralne, wzor 4 = ten kafelek, edytowany w SETTINGS -> TILE. */
static uint8_t custom_tile[32];
static uint16_t tile_fg=0x889u;      /* kolor ZAPALONEGO piksela (wpis 3 palety) */
void kgfx_tile_set_fg(unsigned rgb12){tile_fg=(uint16_t)(rgb12&0xfffu);(void)kgfx_set_palette(3u,tile_fg);}
unsigned kgfx_tile_fg(void){return (unsigned)tile_fg;}
void kgfx_tile_clear(void){unsigned i;for(i=0;i<sizeof custom_tile;++i)custom_tile[i]=0u;}
uint32_t kdbg_tile_sets, kdbg_tile_ons;
void kgfx_tile_set(int x,int y,int on)
{
    ++kdbg_tile_sets; if(on)++kdbg_tile_ons;
    if(x<0||x>15||y<0||y>15)return;
    {unsigned off=(unsigned)y*2u+(unsigned)(x>>3);uint8_t bit=(uint8_t)(0x80u>>(x&7));
     if(on)custom_tile[off]|=bit;else custom_tile[off]=(uint8_t)(custom_tile[off]&~bit);}
}
int kgfx_tile_get(int x,int y)
{
    if(x<0||x>15||y<0||y>15)return 0;
    return (custom_tile[(unsigned)y*2u+(unsigned)(x>>3)]>>(7-(x&7)))&1;
}
const uint8_t *kgfx_tile_bytes(void){return custom_tile;}
uint32_t kdbg_tile_put;
void kgfx_tile_put_byte(unsigned i,unsigned v){++kdbg_tile_put;if(i<sizeof custom_tile)custom_tile[i]=(uint8_t)v;}
void kgfx_tile_from_bytes(const uint8_t *src){unsigned i;if(!src)return;for(i=0;i<sizeof custom_tile;++i)custom_tile[i]=src[i];}
/* Siatka edycyna 16x16: kazda komorka cell_w x cell_h, tlo = paleta 1 (paper),
   zaznaczony piksel = paleta 0 (ink).  Rysowana w JADRZE, bo 256 komorek nie
   miesci sie w liscie rysowania aplikacji (LIST_MAX 48). */
void kgfx_tile_grid(int32_t x,int32_t y,int32_t cw,int32_t ch)
{
    int gx,gy;
    /* Komorki + SIATKA: bez linii cala plansza zlewala sie w bialy prostokat i
       nie bylo widac, gdzie sie klika ("rozjechane" zgloszenie uzytkownika).
       Linie rysujemy wpisem 3 (akcent motywu), 1 px miedzy komorkami. */
    kgfx_fill_rect(x,y,16*cw,16*ch,1u);
    for(gy=0;gy<16;++gy)for(gx=0;gx<16;++gx){
        if(kgfx_tile_get(gx,gy))
            kgfx_fill_rect(x+gx*cw+1,y+gy*ch+1,cw-1,ch-1,0u);
    }
    for(gy=0;gy<=16;++gy)kgfx_fill_rect(x,y+gy*ch-((gy==16)?1:0),16*cw,1u,3u);
    for(gx=0;gx<=16;++gx)kgfx_fill_rect(x+gx*cw-((gx==16)?1:0),y,1u,16*ch,3u);
}
void kgfx_pattern_fill(int32_t x,int32_t y,int32_t width,int32_t height,unsigned pattern)
{
    surface_t surface;int32_t yy;const uint8_t base=2u,accent=3u;unsigned tx,ty;
    /* KAFELEK 16x16 budowany RAZ na stosie (256 B), potem kazdy wiersz ekranu
       to 50 kopii 16-bajtowego fragmentu kafelka (skok 16 px).  Wczesniej wzor
       byl liczony NA KAZDY PIKSEL (480 000 iteracji z rozwinieciem switcha);
       teraz 256 + 50 kopii na wiersz.  Gdy x lub szerokosc nie sa wielokrotnoscia
       16, schodzimy do petli bajtowej (poprawnosc ponad szybkosc). */
    uint8_t tile[16u*16u];
    if(!clip_rect(&x,&y,&width,&height))return;
    if(pattern==0u){kgfx_fill_rect(x,y,width,height,base);return;}
    surface=active_surface();
    if(!surface.chunky){kgfx_fill_rect(x,y,width,height,base);return;}
    for(ty=0;ty<16u;++ty)for(tx=0;tx<16u;++tx){
        uint8_t on=0;
        switch(pattern){
            case 1u: on=(uint8_t)(((tx^ty)&1u));break;              /* dither 50% */
            case 2u: on=(uint8_t)((((tx>>3)+(ty>>3))&1u));break;    /* szachownica 8x8 */
            case 3u: on=(uint8_t)((((tx+ty)%16u)<2u));break;        /* skos */
            case 4u: on=(uint8_t)kgfx_tile_get((int)tx,(int)ty);break; /* wlasny kafelek */
            default: on=0;break;
        }
        tile[ty*16u+tx]=on?accent:base;
    }
    {volatile uint8_t *px=(volatile uint8_t *)surface.words;
     uint32_t pitch=(uint32_t)surface.line_words*2u;
     int fast=(x==0)&&((width&15)==0)&&((pitch&1u)==0u);
     for(yy=0;yy<height;++yy){
         volatile uint8_t *dst=px+(uint32_t)(y+yy)*pitch+(uint32_t)x;
         const uint8_t *t=tile+(((unsigned)(y+yy)&15u)<<4);
         if(fast){
             /* 16 bajtow = 8 slow 16-bit (pitch parzysty, x=0) */
             int32_t o;
             for(o=0;o<width;o+=16){
                 volatile uint16_t *dw=(volatile uint16_t *)(dst+o);
                 dw[0]=(uint16_t)(((uint16_t)t[0]<<8)|t[1]);
                 dw[1]=(uint16_t)(((uint16_t)t[2]<<8)|t[3]);
                 dw[2]=(uint16_t)(((uint16_t)t[4]<<8)|t[5]);
                 dw[3]=(uint16_t)(((uint16_t)t[6]<<8)|t[7]);
                 dw[4]=(uint16_t)(((uint16_t)t[8]<<8)|t[9]);
                 dw[5]=(uint16_t)(((uint16_t)t[10]<<8)|t[11]);
                 dw[6]=(uint16_t)(((uint16_t)t[12]<<8)|t[13]);
                 dw[7]=(uint16_t)(((uint16_t)t[14]<<8)|t[15]);
             }
         }else{
             int32_t o;
             for(o=0;o<width;++o)dst[o]=t[(unsigned)(x+o)&15u];
         }
     }}
    mark_dirty(x,y,width,height);
}
void kgfx_fill_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color)
{surface_t surface;cursor_guard_t guard;if(!clip_rect(&x,&y,&width,&height))return;
 guard=cursor_guard_begin(x,y,width,height);surface=active_surface();
 /* The planar blitter cannot fill chunky pixels; surface_fill has a byte path. */
 if(surface.chunky||
    !kblit_fill_planar(surface.words,surface.line_words,surface.planes,(uint16_t)x,(uint16_t)y,
                       (uint16_t)width,(uint16_t)height,color))
     surface_fill(&surface,(uint16_t)x,(uint16_t)y,(uint16_t)width,(uint16_t)height,color);
 mark_dirty(x,y,width,height);cursor_guard_end(guard);}

/* Blit a PLANAR Atari screen (1, 2 or 4 bitplanes, MSB first, planes stored
   one after another) into the chunky framebuffer as index-per-pixel.
   This is what lets a TOS program run in a window: the program keeps thinking
   in ST screen formats (320x200x4 planes, 640x200x2, 640x400x1) while the
   desktop is 8 bpp, so every pixel has to be expanded on the way in.
   Colour indices are copied verbatim, so the window shows whatever the program
   put in its palette (the caller installs that palette). */
/* D2: when a TOS window is registered in the WM, kgfx_blit_planar writes its
   client into this surface (top-left origin); the WM compositor then blits the
   surface with occlusion.  NULL = the direct presented-frame path. */
uint8_t *kgfx_planar_surface_override;
uint16_t kgfx_planar_surface_pitch;

void kgfx_blit_planar(const uint8_t *source,uint16_t source_width,uint16_t source_height,
                      uint8_t source_bpp,uint16_t source_stride,uint32_t source_capacity,
                      int32_t dx,int32_t dy,int32_t clip_x,int32_t clip_y,
                      int32_t clip_width,int32_t clip_height,uint8_t palette_base)
{
    surface_t surface;cursor_guard_t guard;
    int32_t x,y,x0,y0,x1,y1;
    uint16_t present_pitch;
    uint8_t present_bpp;
    uint8_t *present;
    uint32_t required,row_bytes;
    if(!source)return;
    if(graphics.mode!=8u)return;                 /* chunky target only */
    if(source_bpp!=1u&&source_bpp!=2u&&source_bpp!=4u)return;
    /* Standard ST screen layout: the bitplanes are INTERLEAVED at 16-bit word
       granularity.  One 16-pixel group holds `source_bpp` words (plane 0 first,
       big-endian, MSB = leftmost pixel).  A row is therefore (width/8)*planes
       bytes, never `planes` separate blocks (that old model read plane 3 at
       offset 96000 out of a 64 KB buffer). */
    row_bytes=(uint16_t)(((uint32_t)((source_width+15u)/16u))*2u*(uint32_t)source_bpp);
    if(source_stride<row_bytes)return;
    /* Capacity limit so a geometry bug cannot overrun the private buffer. */
    required=(uint32_t)source_stride*(uint32_t)source_height;
    if(source_capacity&&required>source_capacity)return;
    /* Write into the PRESENTED frame, not the application target: the
       compositor copies its own back buffer over the target on every flush,
       which erased the window client.  The pitch therefore comes from the
       presented surface, not from graphics.line_bytes. */
    if(kgfx_planar_surface_override){
        present=kgfx_planar_surface_override;
        present_pitch=kgfx_planar_surface_pitch;
        present_bpp=8u;
        /* The clip arrives in SCREEN coordinates (destination_x,
           destination_y+band); the client surface's origin is (0,0), so
           translate both the position AND the clip by the window offset. */
        clip_x-=dx;clip_y-=dy;
        dx=0;dy=0;
    }else{
        present=kgfx_present_target(&present_pitch,&present_bpp);
    }
    if(!present||present_bpp!=8u)return;
    surface.words=(volatile uint16_t *)(uintptr_t)present;
    surface.width=graphics.width;surface.height=graphics.height;
    surface.line_words=(uint16_t)(present_pitch/2u);surface.planes=1u;surface.chunky=1u;
    /* Clip to the target surface and then to the caller's rectangle. */
    x0=dx;y0=dy;x1=dx+(int32_t)source_width;y1=dy+(int32_t)source_height;
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int32_t)graphics.width)x1=(int32_t)graphics.width;
    if(y1>(int32_t)graphics.height)y1=(int32_t)graphics.height;
    if(x0<clip_x)x0=clip_x;
    if(y0<clip_y)y0=clip_y;
    if(x1>clip_x+clip_width)x1=clip_x+clip_width;
    if(y1>clip_y+clip_height)y1=clip_y+clip_height;
    if(x1<=x0||y1<=y0)return;
    guard=cursor_guard_begin(x0,y0,(int32_t)(x1-x0),(int32_t)(y1-y0));
    for(y=y0;y<y1;++y){
        volatile uint8_t *dst=(volatile uint8_t *)surface.words+
            (uint32_t)y*(uint32_t)surface.line_words*2u+(uint32_t)x0;
        for(x=x0;x<x1;++x){
            uint8_t value=st_raster_pixel(source,source_stride,source_bpp,
                                          (uint16_t)(x-dx),(uint16_t)(y-dy));
            /* The program's colours live in their own RAMDAC range, so the
               window never repaints the desktop's own palette entries. */
            *dst++=(uint8_t)(value+palette_base);
        }
    }
    mark_dirty(x0,y0,(int32_t)(x1-x0),(int32_t)(y1-y0));
    cursor_guard_end(guard);
}
/* Blit a PACKED 1-bpp mono bitmap (MSB first) into the chunky framebuffer,
   mapping set bits to `ink` and clear bits to `paper`.  Used by the TOS window's
   2:1 mono client, where the bitmap is already a compact 320x200 row-packed
   image (40 bytes/row) rather than the interleaved ST screen. */
/* 256 x 8 expansion for the mono blit; rebuilt only when ink/paper change. */
static uint8_t mono_expand[256][8];
/* Two ready 32-bit words per source byte: the aligned path then needs two loads
   and two stores, with no shifts/ORs and no byte reads at all (the compiler
   really did emit that packing for every eight pixels). */
static struct { uint32_t lo,hi; } mono_words[256];
static uint16_t mono_expand_key=0xFFFFu;
uint32_t kdbg_perf_blit_pixels;
uint32_t kdbg_pat_calls;
uint32_t kdbg_last_opcode,kdbg_last_color;   /* ile razy klient zadal tapety (GUI_PATTERN) */
static void mono_expand_build(uint8_t ink,uint8_t paper)
{
    unsigned b,i;
    for(b=0u;b<256u;++b){
        for(i=0u;i<8u;++i)
            mono_expand[b][i]=(b&(0x80u>>i))?ink:paper;
        mono_words[b].lo=kgfx_mono_word_be(mono_expand[b]);
        mono_words[b].hi=kgfx_mono_word_be(mono_expand[b]+4);
    }
    mono_expand_key=(uint16_t)(((uint16_t)ink<<8)|(uint16_t)paper);
}

void kgfx_blit_mono(const uint8_t *source,int32_t dx,int32_t dy,int32_t width,int32_t height,
                    int32_t stride,uint8_t ink,uint8_t paper)
{
    uint16_t pitch;uint8_t bpp;uint8_t *present;
    int32_t y;
    if(!source)return;
    if(graphics.mode!=8u)return;
    if(kgfx_planar_surface_override){
        present=kgfx_planar_surface_override;
        pitch=kgfx_planar_surface_pitch;
        bpp=8u;
    }else{
        present=kgfx_present_target(&pitch,&bpp);
    }
    if(!present||bpp!=8u)return;
    if(dx<0||dy<0||dx+width>(int32_t)graphics.width||dy+height>(int32_t)graphics.height)return;
    /* Byte-at-a-time expansion through a cached 256x8 table: the old loop did a
       shift and a bit test PER PIXEL (256 000 iterations for a full mono
       screen).  The table turns one source byte into its eight output bytes, and
       an aligned destination is filled with two 32-bit stores per 8 pixels. */
    if(mono_expand_key!=(uint16_t)(((uint16_t)ink<<8)|(uint16_t)paper))
        mono_expand_build(ink,paper);
    for(y=0;y<height;++y){
        const uint8_t *row=source+y*stride;
        uint8_t *dst=(uint8_t *)(present+(uint32_t)(dy+y)*pitch+(uint32_t)dx);
        int32_t xb=0;
        if(((uintptr_t)dst&3u)==0u){
            uint32_t *d32=(uint32_t *)dst;
            for(;xb+8<=width;xb+=8){
                /* Two precomputed BIG-endian words per source byte: two loads and
                   two stores, no shifts.  (Assembling them here little-endian once
                   reversed every four bytes on screen and shredded the glyphs.) */
                d32[(xb>>2)]=mono_words[row[xb>>3]].lo;
                d32[(xb>>2)+1]=mono_words[row[xb>>3]].hi;
            }
        }else{
            for(;xb+8<=width;xb+=8){
                const uint8_t *e=mono_expand[row[xb>>3]];
                dst[xb]=e[0];dst[xb+1]=e[1];dst[xb+2]=e[2];dst[xb+3]=e[3];
                dst[xb+4]=e[4];dst[xb+5]=e[5];dst[xb+6]=e[6];dst[xb+7]=e[7];
            }
        }
        for(;xb<width;++xb){
            uint8_t bit=(uint8_t)((row[xb>>3]>>(7-(xb&7)))&1u);
            dst[xb]=bit?ink:paper;
        }
        kdbg_perf_blit_pixels+=(uint32_t)width;
    }
    if(!kgfx_planar_surface_override)mark_dirty(dx,dy,width,height);
}

void kgfx_frame_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color)
{
    int32_t cx=x,cy=y,cw=width,ch=height;int64_t right,bottom;cursor_guard_t guard;
    if(!clip_rect(&cx,&cy,&cw,&ch))return;
    right=(int64_t)x+width-1;bottom=(int64_t)y+height-1;
    guard=cursor_guard_begin(cx,cy,cw,ch);
    if(y>=0&&y<graphics.height)kgfx_fill_rect(cx,y,cw,1,color);
    if(bottom!=y&&bottom>=0&&bottom<graphics.height)
        kgfx_fill_rect(cx,(int32_t)bottom,cw,1,color);
    if(x>=0&&x<graphics.width)kgfx_line(x,cy,x,cy+ch-1,color);
    if(right!=x&&right>=0&&right<graphics.width)
        kgfx_line((int32_t)right,cy,(int32_t)right,cy+ch-1,color);
    cursor_guard_end(guard);
}

static uint8_t line_outcode(int32_t x,int32_t y)
{
    uint8_t code=0;
    if(x<0)code|=1u;else if(x>=graphics.width)code|=2u;
    if(y<0)code|=4u;else if(y>=graphics.height)code|=8u;
    return code;
}

static int clip_line(int32_t *x0,int32_t *y0,int32_t *x1,int32_t *y1)
{
    uint8_t c0,c1,out;int32_t x=0,y=0;int64_t numerator,denominator;
    for(;;){
        c0=line_outcode(*x0,*y0);c1=line_outcode(*x1,*y1);
        if(!(c0|c1))return 1;
        if(c0&c1)return 0;
        out=c0?c0:c1;
        if(out&8u){denominator=(int64_t)*y1-*y0;if(!denominator)return 0;
            y=(int32_t)graphics.height-1;numerator=((int64_t)*x1-*x0)*(y-*y0);
            x=*x0+(int32_t)(numerator/denominator);
        }else if(out&4u){denominator=(int64_t)*y1-*y0;if(!denominator)return 0;
            y=0;numerator=((int64_t)*x1-*x0)*(y-*y0);
            x=*x0+(int32_t)(numerator/denominator);
        }else if(out&2u){denominator=(int64_t)*x1-*x0;if(!denominator)return 0;
            x=(int32_t)graphics.width-1;numerator=((int64_t)*y1-*y0)*(x-*x0);
            y=*y0+(int32_t)(numerator/denominator);
        }else{denominator=(int64_t)*x1-*x0;if(!denominator)return 0;
            x=0;numerator=((int64_t)*y1-*y0)*(x-*x0);
            y=*y0+(int32_t)(numerator/denominator);
        }
        if(out==c0){*x0=x;*y0=y;}else{*x1=x;*y1=y;}
    }
}

void kgfx_line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint8_t color)
{
    surface_t surface;cursor_guard_t guard;int32_t dx,dy,error,e2,sx,sy,minx,miny,maxx,maxy;
    if(!clip_line(&x0,&y0,&x1,&y1))return;
    if(y0==y1){kgfx_fill_rect(x0<x1?x0:x1,y0,(x0<x1?x1-x0:x0-x1)+1,1,color);return;}
    if(x0==x1){
        miny=y0<y1?y0:y1;maxy=y0>y1?y0:y1;
        guard=cursor_guard_begin(x0,miny,1,maxy-miny+1);surface=active_surface();
        if(surface.chunky)surface_fill(&surface,(uint16_t)x0,(uint16_t)miny,1u,(uint16_t)(maxy-miny+1),color);
        else emutos_vdi_vertical(surface.words,surface.line_words,surface.planes,
            (uint16_t)x0,(uint16_t)miny,(uint16_t)(maxy-miny+1),color);
        mark_dirty(x0,miny,1,maxy-miny+1);cursor_guard_end(guard);return;
    }
    minx=x0<x1?x0:x1;maxx=x0>x1?x0:x1;miny=y0<y1?y0:y1;maxy=y0>y1?y0:y1;
    guard=cursor_guard_begin(minx,miny,maxx-minx+1,maxy-miny+1);
    surface=active_surface();dx=x1>x0?x1-x0:x0-x1;dy=y1>y0?y1-y0:y0-y1;
    sx=x0<x1?1:-1;sy=y0<y1?1:-1;error=dx-dy;
    for(;;){surface_pixel_set(&surface,(uint16_t)x0,(uint16_t)y0,color);
        if(x0==x1&&y0==y1)break;
        e2=error+error;
        if(e2>-dy){error-=dy;x0+=sx;}if(e2<dx){error+=dx;y0+=sy;}}
    mark_dirty(minx,miny,maxx-minx+1,maxy-miny+1);cursor_guard_end(guard);
}

int kgfx_set_palette(uint8_t index,uint16_t color)
{
    volatile uint16_t *palette;
    /* The system palette is meaningful in every mode: the VOFA native renderer
       shows entries 0/1 even in the 1 bpp mono mode, so the desktop can choose
       the two system colours (a future colour mode uses all 16). */
    /* The Shifter has 16 entries; the card RAMDAC has 256.  In the 1 bpp mono
       modes only 0/1 are visible, and an 8 bpp (256-colour) mode will use the
       full range, so the limit is the RAMDAC's, not the Shifter's. */
    /* index is a uint8_t, so 0..255 is the whole range. */
    if(index<16u){
        palette=(volatile uint16_t *)(uintptr_t)SHIFTER_PALETTE;
        palette[index]=(uint16_t)(color&0x0fffu);
    }
    /* Tryb 16 bpp nie uzywa DAC: kolor jest w pikselu, wiec warstwa wideo
       musi znac te sama palete w 5:6:5. */
    kvideo_set_palette565(index,(uint16_t)(color&0x0fffu));
    /* On the ET4000 the Shifter register is not what is scanned out: the same
       system colour must also reach the card RAMDAC. */
    if(kvideo_is_vofa())(void)kvideo_set_palette(index,(uint16_t)(color&0x0fffu));
    return 1;
}

void kgfx_copy_rect(int32_t sx,int32_t sy,int32_t width,int32_t height,int32_t dx,int32_t dy)
{
    surface_t surface;cursor_guard_t guard;int32_t x,y,x_start,x_end,x_step,y_start,y_end,y_step;
    if(!clip_copy_rect(&sx,&sy,&width,&height,&dx,&dy))return;
    guard=cursor_guard_begin(sx<dx?sx:dx,sy<dy?sy:dy,
                             (sx>dx?sx:dx)-(sx<dx?sx:dx)+width,
                             (sy>dy?sy:dy)-(sy<dy?sy:dy)+height);
    /* Assign fields explicitly: avoid an out-of-line structure memcpy in
       freestanding m68k builds when this routine grows. */
    surface.words=(volatile uint16_t *)(uintptr_t)graphics.framebuffer;
    surface.width=graphics.width;surface.height=graphics.height;
    surface.line_words=(uint16_t)(graphics.line_bytes/2u);
    surface.planes=graphics.planes;surface.chunky=(uint8_t)(graphics.mode==8u);
    if(dy>sy){y_start=height-1;y_end=-1;y_step=-1;}else{y_start=0;y_end=height;y_step=1;}
    if(dx>sx){x_start=width-1;x_end=-1;x_step=-1;}else{x_start=0;x_end=width;x_step=1;}
    if(surface.chunky){
        chunky_copy((volatile uint8_t *)surface.words,(uint32_t)surface.line_words*2u,
                    sx,sy,width,height,dx,dy);
    }else if(kblit_copy_planar(surface.words,surface.line_words,surface.planes,
                         (uint16_t)sx,(uint16_t)sy,(uint16_t)width,(uint16_t)height,
                         (uint16_t)dx,(uint16_t)dy)){
        /* Hardware copy for 1/2/4bpp: only reached when the blitter was
           detected and its skewed copy passed the boot self-test for that
           plane count. */
    }else if(surface.planes==1u){
        if((((uint32_t)sx|(uint32_t)dx|(uint32_t)width)&15u)==0u)
            surface_copy_mono_words(&surface,(uint16_t)sx,(uint16_t)sy,(uint16_t)width,
                                    (uint16_t)height,(uint16_t)dx,(uint16_t)dy);
        else surface_copy_mono_shifted(&surface,(uint16_t)sx,(uint16_t)sy,(uint16_t)width,
                                       (uint16_t)height,(uint16_t)dx,(uint16_t)dy);
    }else for(y=y_start;y!=y_end;y+=y_step)for(x=x_start;x!=x_end;x+=x_step)
            surface_pixel_set(&surface,(uint16_t)(dx+x),(uint16_t)(dy+y),
                              surface_pixel_get(&surface,(uint16_t)(sx+x),(uint16_t)(sy+y)));
    mark_dirty(dx,dy,width,height);cursor_guard_end(guard);
}

void kgfx_text(int32_t x,int32_t y,const char *text,uint8_t foreground,
               uint8_t background,int opaque)
{
    surface_t surface;cursor_guard_t guard;int32_t origin=x;uint16_t row;uint8_t bits,bit;
    const uint8_t *glyph;if(!text)return;
    kgfx_overlay_hide();guard.x=cursor_x;guard.y=cursor_y;guard.active=cursor_is_visible;
    if(guard.active)kgfx_cursor_hide();
    surface=active_surface();
    while(*text){if(*text=='\n'){x=origin;y+=16;text++;continue;}{
        uint8_t code=(uint8_t)*text++;
        glyph=(desktop_is_active&&code!=0xf6u&&code!=0xf7u)?
            emutos_system_font[code]:font_glyph((char)code,1u);
    }
        if(surface.chunky&&x>=0&&y>=0&&x+8<=graphics.width&&y+16<=graphics.height)
            chunky_glyph((volatile uint8_t *)surface.words+(uint32_t)y*surface.line_words*2u+x,
                (uint32_t)surface.line_words*2u,glyph,foreground,background,opaque);
        else if(!surface.chunky&&surface.planes==1u&&x>=0&&y>=0&&x+8<=graphics.width&&y+16<=graphics.height)
            mono_glyph(surface.words,surface.line_words,(uint16_t)x,(uint16_t)y,glyph,foreground,background,opaque);
        else for(row=0;row<16u;++row){bits=glyph[row];for(bit=0;bit<8u;++bit){
            int32_t px=x+bit,py=y+row;if(px>=0&&py>=0&&px<graphics.width&&py<graphics.height){
                if(bits&(uint8_t)(0x80u>>bit))surface_pixel_set(&surface,(uint16_t)px,(uint16_t)py,foreground);
                else if(opaque)surface_pixel_set(&surface,(uint16_t)px,(uint16_t)py,background);}}}
        mark_dirty(x,y,8,16);x+=8;
    }
    cursor_guard_end(guard);
}

/* Word-oriented save/restore follows EmuTOS vdi/vdi_mouse.c cur_display:
   at most two words per plane and row, including clipped right/bottom edges.
   Keep the existing XOR pointer shape and coordinate convention. */
void kgfx_cursor_hide(void)
{
    surface_t surface;uint16_t row,plane,group,shift,index=0;
    if(!cursor_is_visible)return;
    surface=active_surface();surface.words=system_scanout();group=(uint16_t)cursor_x>>4;shift=(uint16_t)cursor_x&15u;
    if(surface.chunky){
        volatile uint8_t *base=(volatile uint8_t *)surface.words+
            (uint32_t)cursor_y*(uint32_t)surface.line_words*2u+(uint32_t)cursor_x;
        uint16_t c;
        for(row=0;row<cursor_height;++row){
            volatile uint8_t *p=base+(uint32_t)row*(uint32_t)surface.line_words*2u;
            for(c=0;c<cursor_width;++c)p[c]=cursor_chunky_background[row*16u+c];
        }
        mark_dirty(cursor_x,cursor_y,cursor_width,cursor_height);
        cursor_is_visible=0;cursor_width=cursor_height=0;return;
    }
    for(row=0;row<cursor_height;++row){
        volatile uint16_t *word=surface.words+(uint32_t)(cursor_y+row)*surface.line_words+group*surface.planes;
        for(plane=0;plane<surface.planes;++plane){
            word[plane]=cursor_mono_load(index++);
            if(shift&&(uint16_t)(group+1u)<surface.width/16u)
                word[plane+surface.planes]=cursor_mono_load(index++);
        }
    }
    mark_dirty(cursor_x,cursor_y,cursor_width,cursor_height);
    cursor_is_visible=0;cursor_width=cursor_height=0;
}

void kgfx_cursor_show(int32_t x,int32_t y)
{
    /* Exclusive game mode: no pointer over the game's screen. */
    if(kgame_owns_screen())return;
    surface_t surface;uint16_t row,plane,group,shift,index=0,remaining;
    if(graphics.width==0u||graphics.height==0u)return;
    if(x<0)x=0;else if(x>=graphics.width)x=(int32_t)graphics.width-1;
    if(y<0)y=0;else if(y>=graphics.height)y=(int32_t)graphics.height-1;
    if(cursor_is_visible)kgfx_cursor_hide();
    cursor_x=(int16_t)x;cursor_y=(int16_t)y;
    remaining=(uint16_t)(graphics.width-(uint16_t)x);
    cursor_width=(uint8_t)(remaining<16u?remaining:16u);
    remaining=(uint16_t)(graphics.height-(uint16_t)y);
    cursor_height=(uint8_t)(remaining<16u?remaining:16u);
    surface=active_surface();surface.words=system_scanout();group=(uint16_t)x>>4;shift=(uint16_t)x&15u;
    if(surface.chunky){
        volatile uint8_t *base=(volatile uint8_t *)surface.words+
            (uint32_t)y*(uint32_t)surface.line_words*2u+(uint32_t)x;
        uint16_t c;
        for(row=0;row<cursor_height;++row){
            volatile uint8_t *p=base+(uint32_t)row*(uint32_t)surface.line_words*2u;
            uint16_t bits=cursor_shape[row],mask=0x8000u;
            /* Dilate once per row, not four neighbour lookups per pixel. */
            uint16_t edge=(uint16_t)((bits<<1)|(bits>>1));
            if(row)edge|=cursor_shape[row-1u];
            if(row<15u)edge|=cursor_shape[row+1u];
            for(c=0;c<cursor_width;++c,mask>>=1){
                uint8_t was=p[c];
                cursor_chunky_background[row*16u+c]=was;
                if(bits&mask){
                    p[c]=(uint8_t)CURSOR_FILL;              /* body */
                }else if(edge&mask){
                    p[c]=(uint8_t)CURSOR_OUTLINE;           /* one-pixel outline */
                }
            }
        }
        cursor_is_visible=1;mark_dirty(x,y,cursor_width,cursor_height);return;
    }
    for(row=0;row<cursor_height;++row){
        volatile uint16_t *word=surface.words+(uint32_t)(y+row)*surface.line_words+group*surface.planes;
        uint16_t left=(uint16_t)(cursor_shape[row]>>shift);
        uint16_t right=shift?(uint16_t)(cursor_shape[row]<<(16u-shift)):0;
        for(plane=0;plane<surface.planes;++plane){
            cursor_mono_save(index++,word[plane]);word[plane]^=left;
            if(shift&&(uint16_t)(group+1u)<surface.width/16u){
                cursor_mono_save(index++,word[plane+surface.planes]);word[plane+surface.planes]^=right;
            }
        }
    }
    cursor_is_visible=1;mark_dirty(x,y,cursor_width,cursor_height);
}

void kgfx_cursor_move(int32_t x,int32_t y)
{if(cursor_is_visible&&cursor_x==x&&cursor_y==y)return;kgfx_cursor_show(x,y);}

int kgfx_cursor_visible(void){return cursor_is_visible!=0u;}

int kgfx_enter_desktop(void)
{
    kgfx_overlay_hide();
    kconsole_flush();
    kgfx_terminal_invalidate();
    uint32_t i;volatile uint32_t *destination;
    if(!desktop_framebuffer)return 0;
    kgfx_cursor_hide();destination=(volatile uint32_t *)desktop_framebuffer;
    if(!desktop_is_active){
        static const uint16_t colors[16]={0x000,0x777,0x070,0x700,0x007,0x077,0x770,0x707,
            0x555,0x333,0x733,0x373,0x337,0x377,0x773,0x737};
        for(i=0;i<16;i++){console_palette[i]=REG16(SHIFTER_PALETTE+i*2);
            if(graphics.mode!=2u)REG16(SHIFTER_PALETTE+i*2)=colors[i];}
    }
    /* v10: clear only on a real mode entry; re-entering must not wipe the
       already composed desktop (that caused a full-frame black/gray flicker). */
    if(!desktop_is_active)for(i=0;i<((uint32_t)graphics.line_bytes*(uint32_t)graphics.height)/4u;++i)destination[i]=0;
    /* MagiC-style: when the ET4000 is active, draw straight into the card
       VRAM (v_bas_ad), with no intermediate buffer - that buffer was where the
       composed desktop got lost. */
    /* Shifter displays ST-RAM at SCREEN_BASE; desktop_framebuffer lives in
       TT-RAM and is NEVER displayed there - the compositor was drawing into a
       buffer nobody shows (screen kept the stale console image). Use VOFA VRAM
       only when the ET4000 path is active, otherwise the Shifter screen. */
    graphics.framebuffer=(volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?(uintptr_t)desktop_framebuffer:(uintptr_t)SCREEN_BASE);kgfx_dirty_reset();
    if(!desktop_hidden)kvideo_select((const volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE));
    desktop_is_active=1;
#if CONFIG_WM
    /* Opt-in compositor: from here on a GUI client's draw batches compose as
       windows instead of painting the legacy full-screen desktop. */
    (void)kernel_wm_enable();
#endif
    return 1;
}

static uint8_t gem_sticky;
void kgfx_gem_desktop_sticky(int on){gem_sticky=(uint8_t)(on?1:0);}
void kgfx_restore_console(void)
{ if(gem_sticky)return;  /* desktop mode is sticky until Exit */
    kgfx_overlay_hide();
    kgfx_terminal_invalidate();
    uint16_t i;
    kgfx_cursor_hide();
    if(desktop_is_active)for(i=0;i<16;i++)REG16(SHIFTER_PALETTE+i*2)=console_palette[i];
    graphics.framebuffer=(volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?(uintptr_t)desktop_framebuffer:(uintptr_t)SCREEN_BASE);
    desktop_is_active=0;kconsole_flush();
    kvideo_select(graphics.framebuffer);kgfx_dirty_reset();
}

/* Adopt the video layer's pixel depth after the card has been programmed at
   boot: 8 bpp means one byte per pixel and the compositor must know it before
   the first paint. */
void kgfx_set_depth(uint16_t width,uint16_t height,uint8_t bpp)
{
    graphics.width=width;graphics.height=height;
    graphics.mode=(uint8_t)((bpp==1u)?2u:8u);
    graphics.planes=1u;
    graphics.line_bytes=(uint16_t)((bpp==1u)?((width+7u)/8u):width);
    kgfx_dirty_reset();
}
int kgfx_desktop_active(void){return desktop_is_active&&!desktop_hidden;}
/* Live resolution switch (GFX opcode 30).  Reprograms the adapter, adopts the
   new geometry, clears the (already large enough) compositor frame and asks the
   WM to repaint at the new pitch.  Only the ET4000 path can do this; the ST
   Shifter has a single 640x400 mono mode. */
int kgfx_video_reconfigure(uint16_t width,uint16_t height)
{
    /* No adapter (or not activated yet): the Shifter has one mono mode. */
    return kgfx_video_reconfigure_format(width,height,1u);
}
int32_t kdbg_mode_result=777;   /* diagnostics: last kgfx_video_reconfigure_format step */
int kgfx_video_reconfigure_format(uint16_t width,uint16_t height,uint8_t bpp)
{
    int result;
    /* bpp==4 (16 kolorow): front pozostaje 8 bpp CHUNKY - desktop rysuje jak
       dotad, a konwersje na cztery plany robi kvideo_flush().  Dlatego rozmiar
       klatki, tryb i stride liczymy jak dla 8 bpp, a nie jak dla mono. */
    uint32_t bytes=(uint32_t)((bpp==1u)?((width+7u)/8u):width)*(uint32_t)height;
    if(!kvideo_is_vofa())return -0x3f3;
    /* Reject a mode the pinned desktop framebuffer cannot hold, BEFORE the
       card is reprogrammed (1024x768x256 needs 786432 bytes). */
    if(bytes>KGFX_MAX_FRAME_BYTES)return -0x3f4;
#if CONFIG_WM
    /* Reserve all backing stores before changing the adapter. */
    if(kernel_wm_prepare_mode(width,height,bpp)<0)return -0x3f4;
#endif
    result=kvideo_program_mode_format(width,height,bpp);
    kdbg_mode_result=result;
    if(result<0)return result;
    graphics.width=width;graphics.height=height;
    /* RAM jest CHUNKY 8 bpp dla kazdej glebi > 1 (mono tylko dla bpp==1).
       Wczesniej tryby 4 i 16 tez trafialy tu jako mode=2 (mono) i pitch
       width/8, wiec bufor mial ZLY format przy przelaczaniu 4/16 bpp. */
    graphics.mode=(uint8_t)((bpp==1u)?2u:8u);
    graphics.planes=1u;
    graphics.line_bytes=(uint16_t)((bpp==1u)?((width+7u)/8u):width);
    if(desktop_framebuffer){
        uint32_t i;volatile uint32_t *d=(volatile uint32_t *)desktop_framebuffer;
        graphics.framebuffer=desktop_framebuffer;
        for(i=0;i<bytes/4u;++i)d[i]=0;
        if(!desktop_hidden)kvideo_select(desktop_framebuffer);
    }
    kgfx_dirty_reset();
#if CONFIG_WM
    kernel_wm_reconfigure();
#endif
    return 0;
}
/* A desktop has taken over this session: its retained console image must not be
   restored on the next switch to it, or the old text screen paints over the new
   desktop.  The compositor repaints the desktop's own canvas instead. */
void kgfx_session_forget_cells(unsigned session)
{if(session<SESSION_COUNT)session_cells_valid[session]=0;}

void kgfx_session_view(int show)
{
 if(kconsole_boot_active())return;
 uint8_t hidden=!show;
 if(hidden==desktop_hidden&&presented_session==ksessions_current())return;
 kgfx_overlay_hide();kgfx_cursor_hide();desktop_hidden=hidden;
 presented_session=ksessions_current();
 kgfx_terminal_invalidate();kconsole_invalidate();
 if(show&&session_cells_valid[presented_session]){
  desktop_is_active=1;
  (void)kgfx_terminal_present(session_cells[presented_session]);
 }
 kvideo_select((const volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE));
}

const kgfx_service_t *kgfx_get_service(void)
{
    static const kgfx_service_t service={
        KGFX_SERVICE_MAGIC,KGFX_SERVICE_VERSION,sizeof(kgfx_service_t),
        kgfx_get_info,kgfx_clear,kgfx_pixel,kgfx_fill_rect,kgfx_frame_rect,
        kgfx_line,kgfx_set_palette,kgfx_copy_rect,kgfx_text,
        kgfx_dirty_reset,kgfx_dirty_count,kgfx_dirty_get
    };
    return &service;
}

/* Fixed EmuTOS resource renderer. The caller supplies only a clipped damage
 * rectangle or window geometry; no user pointers or scheduler policy enter
 * this layer. Placement/interaction remains owned by DESKTOP.APP. */
static void theme_icon(const surface_t *s,int32_t cx,int32_t cy,int32_t cw,int32_t ch,
                       int32_t ix,int32_t iy,const uint16_t *mask,const uint16_t *data,
                       uint8_t ink,uint8_t paper,const char *label,int invert)
{
    int32_t x,y,k,label_x;unsigned len=0;uint16_t bit;const uint8_t *glyph;
    while(label[len]&&len<12u)++len;
    label_x=ix+(GUI_ICON_MONO_SIZE-(int32_t)len*8)/2;
    for(y=0;y<32;y++)for(x=0;x<32;x++){
        int32_t px=ix+x,py=iy+y;
        if(px<cx||py<cy||px>=cx+cw||py>=cy+ch)continue;
        bit=(uint16_t)(0x8000u>>(x&15));
        if(mask[y*2+(x>>4)]&bit)
            surface_pixel_set(s,(uint16_t)px,(uint16_t)py,
                (data[y*2+(x>>4)]&bit)?(invert?paper:ink):(invert?ink:paper));
    }
    for(k=0;k<(int32_t)len;k++){
        glyph=emutos_system_font[(uint8_t)label[k]];
        for(y=0;y<16;y++)for(x=0;x<8;x++){
            int32_t px=label_x+k*8+x,py=iy+32+y;
            if(px<cx||py<cy||px>=cx+cw||py>=cy+ch)continue;
            if(s->chunky){
                /* Colour desktop: white label glyphs on the transparent teal;
                   inverted selection draws them in the outline colour instead. */
                if(glyph[y]&(0x80u>>x))surface_pixel_set(s,(uint16_t)px,(uint16_t)py,invert?paper:1u);
            }else{
                surface_pixel_set(s,(uint16_t)px,(uint16_t)py,(glyph[y]&(0x80u>>x))?(invert?1u:0u):(invert?0u:1u));
            }
        }
    }
}

void kgfx_icon(int32_t x,int32_t y,uint8_t icon,uint8_t ink,uint8_t paper,const char *label,int invert)
{
    static const uint16_t *const masks[4]={rs_iconmask0,rs_iconmask1,rs_iconmask2,rs_iconmask3};
    static const uint16_t *const datas[4]={rs_icondata0,rs_icondata1,rs_icondata2,rs_icondata3};
    surface_t s;cursor_guard_t guard;unsigned len=0;int32_t w,cx,cy,cw,ch;
    if(icon>3u||!label)return;
    while(label[len]&&len<12u)++len;
    w=(int32_t)(len*8u);
    s=active_surface();
    if(w<GUI_ICON_MONO_SIZE)w=GUI_ICON_MONO_SIZE;
    cx=x+(GUI_ICON_MONO_SIZE-w)/2;cy=y;cw=w;ch=GUI_ICON_MONO_H;   /* ROM mono icons stay 32x32 */
    if(!clip_rect(&cx,&cy,&cw,&ch))return;
    guard=cursor_guard_begin(cx,cy,cw,ch);
    theme_icon(&s,cx,cy,cw,ch,x,y,masks[icon],datas[icon],ink,paper,label,invert);
    cursor_guard_end(guard);
    mark_dirty(cx,cy,cw,ch);
}

/* Colour icons (system/ICONS): 32x32 indexed pixels + a 1 bpp mask, copied
   straight into the 8 bpp canvas.  Mono falls back to the caller's ink/paper
   icon path, so this is a no-op there. */
/* Dynamic per-app icons registered at runtime by the desktop (foo.png).  They
   are quantized to the shared icon palette, so they draw like built-ins. */
#define DYN_ICON_MAX 8u
static icon_color_t dyn_icon[DYN_ICON_MAX];
void kgfx_icon_register(uint8_t slot,const uint8_t *mask,const uint8_t *pixels)
{
    unsigned i;
    if(slot>=DYN_ICON_MAX||!mask||!pixels)return;
    for(i=0;i<icon_mask_bytes;++i)dyn_mask[slot][i]=mask[i];
    for(i=0;i<icon_pixel_bytes;++i)dyn_pixels[slot][i]=pixels[i];
    dyn_icon[slot].mask=dyn_mask[slot];
    dyn_icon[slot].pixels=dyn_pixels[slot];
}
/* Decode a PNG (foo.png) and register it as a dynamic icon at slot.  The
   kernel owns the large scratch buffers; the app only ships the PNG bytes. */
/* Conversion targets sized from the ICON contract, not from a remembered 32x32.
   They used to be pixels[1024]/mask[128] - the geometry of 32x32 icons - while
   png_icon_convert() writes ICON_SIZE*ICON_SIZE indices and a matching mask.
   With the set at 48x48 that wrote 2304 bytes into a 1024-byte buffer and 288
   into 128: a 1440-byte stack smash on every per-app foo.png icon.  Static, and
   derived from the constants, so a future size change cannot silently overflow
   again. */
/* Bufory na NAJWIEKSZY obslugiwany rozmiar (ICON_MAX_SIZE), bo uzytkownik moze
   wybrac 64 px; konwersja PNG liczy sie do BIEZACEGO icon_px. */
static uint8_t png_icon_pixels[ICON_MAX_PIXEL_BYTES];
static uint8_t png_icon_mask[ICON_MAX_MASK_BYTES];
/* The arrays must hold at least what the converter can write. */
typedef char kgfx_png_pixels_fit[(ICON_MAX_PIXEL_BYTES>=ICON_MAX_SIZE*ICON_MAX_SIZE)?1:-1];
typedef char kgfx_png_mask_fit[(ICON_MAX_MASK_BYTES>=(ICON_MAX_SIZE*ICON_MAX_SIZE+7u)/8u)?1:-1];
int kgfx_icon_from_png(uint8_t slot,const uint8_t *png,uint32_t png_size)
{
    uint32_t w=0,h=0;
    if(png_decode_rgba(png,png_size,png_rgba,&w,&h,png_work))return -1;
    if(w>96u||h>96u)return -1;
    if(png_icon_convert(png_rgba,w,h,icon_color_palette,ICON_PAL_COLORS,
                        png_icon_pixels,png_icon_mask,(uint32_t)icon_px))return -1;
    kgfx_icon_register(slot,png_icon_mask,png_icon_pixels);
    return 0;
}
/* Nominalny rozmiar kontraktu (48) nadal musi sie zgadzac z gui_draw.h - realny
   rozmiar jest runtime (icon_px), a granice uszkodzen bierze sie z pol w/h
   elementu GUI_ICON_RGB. */
typedef char kgfx_icon_size_matches_contract[(ICON_SIZE==GUI_ICON_COLOR_SIZE)?1:-1];
void kgfx_icon_color(int32_t x,int32_t y,uint8_t id,const char *label)
{
    const icon_color_t *ic;surface_t s;cursor_guard_t guard;
    unsigned len=0;int32_t w,cx,cy,cw,ch;uint16_t px,py;const uint8_t *glyph;int32_t k;
    if(id>=(int32_t)icon_color_count){
        if(id-(int32_t)icon_color_count>=(int32_t)DYN_ICON_MAX)return;
        ic=&dyn_icon[id-(int32_t)icon_color_count];
        if(!ic->mask||!ic->pixels)return;
    }else
        ic=&icon_color_table[id];
    if(label){while(label[len]&&len<12u)++len;}
    w=(int32_t)(len*8u);
    s=active_surface();
    if(!s.chunky)return;              /* mono: the ink/paper path still applies */
    if(w<(int32_t)icon_px)w=(int32_t)icon_px;
    cx=x+((int32_t)icon_px-w)/2;cy=y;cw=w;ch=(int32_t)(icon_px+16u);
    if(!clip_rect(&cx,&cy,&cw,&ch))return;
    guard=cursor_guard_begin(cx,cy,cw,ch);
    for(py=0;py<icon_px;py++)for(px=0;px<icon_px;px++){
        if(x+px<cx||y+py<cy||x+px>=cx+cw||y+py>=cy+ch)continue;
        if(ic->mask[((uint32_t)py*icon_px+px)>>3]&(uint8_t)(0x80u>>(px&7u)))
            surface_pixel_set(&s,(uint16_t)(x+px),(uint16_t)(y+py),
                (uint8_t)(ICON_PAL_BASE+ic->pixels[(uint32_t)py*icon_px+px]));
    }
    /* label: white glyphs on the transparent desktop, same as theme_icon */
    for(k=0;label&&k<(int32_t)len;k++){
        glyph=emutos_system_font[(uint8_t)label[k]];
        for(py=0;py<16u;py++)for(px=0;px<8u;px++){
            int32_t tx=x+((int32_t)icon_px-(int32_t)len*8)/2+k*8+(int32_t)px,ty=y+(int32_t)icon_px+(int32_t)py;
            if(tx<cx||ty<cy||tx>=cx+cw||ty>=cy+ch)continue;
            if(glyph[py]&(uint8_t)(0x80u>>px))
                surface_pixel_set(&s,(uint16_t)tx,(uint16_t)ty,1u);
        }
    }
    cursor_guard_end(guard);
    mark_dirty(cx,cy,cw,ch);
}

/* --- Wczytanie ikon z \SYSTEM\ICONS.DAT -------------------------------------
   Pola bloba sa big-endian (natywne dla 68000), offsety wzgledne wobec poczatku
   pliku.  Obslugiwane uklady (include/icons_blob.h):
     v1       - surowy, jeden rozmiar, czytany WPROST z bufora pliku,
     v2/v3/v4 - jeden rozmiar spakowany (RLE par / PackBits / LZSS),
     v5       - sekcje rozmiarow (32/48/64); kazda to samodzielny uklad v1
                spakowany LZSS, wiec rozwijamy TYLKO wybrany rozmiar.
   Biezacy rozmiar trzyma icon_px i jest zrodlem prawdy dla rysowania (pulpit
   pyta o niego opcodem 49).  Zmiana rozmiaru: kgfx_icons_unload() + load(). */

static uint32_t icons_be32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];}

/* Dekodery.  Kazdy zwraca 1 tylko wtedy, gdy odtworzyl DOKLADNIE `raw` bajtow.
   floor_off = najmniejszy dozwolony offset dopasowania (v1-uklad z prefiksem
   naglowka/wpisow/palety nie moze kopiowac z prefiksu). */
static int icons_rle(const uint8_t *src,uint32_t size,uint8_t *out,uint32_t raw)
{
    const uint8_t *end=src+size;uint32_t o=0;
    while(src+1u<end&&o<raw){
        uint32_t cnt=(uint32_t)src[0];uint8_t v=src[1u];src+=2u;
        while(cnt--&&o<raw)out[o++]=v;
    }
    return o==raw;
}
static int icons_packbits(const uint8_t *src,uint32_t size,uint8_t *out,uint32_t raw)
{
    const uint8_t *end=src+size;uint32_t o=0;
    while(src<end&&o<raw){
        uint32_t ctrl=(uint32_t)*src++;
        if(ctrl&0x80u){uint32_t cnt=(ctrl&0x7fu)+1u;
            while(cnt--&&o<raw&&src<end)out[o++]=*src++;}
        else{uint32_t cnt=ctrl+1u;uint8_t v=(src<end)?*src++:0u;
            while(cnt--&&o<raw)out[o++]=v;}
    }
    return o==raw;
}
static int icons_lzss(const uint8_t *src,uint32_t size,uint8_t *out,uint32_t raw,uint32_t floor_off)
{
    const uint8_t *end=src+size;uint32_t o=0;
    while(src<end&&o<raw){
        uint32_t flags=(uint32_t)*src++;uint32_t bit;
        for(bit=0;bit<8u&&src<end&&o<raw;++bit){
            if(flags&(1u<<bit)){
                uint32_t off,len;uint8_t lo,hi;
                if(src+1u>=end)return 0;
                lo=src[0];hi=src[1];src+=2u;
                off=((uint32_t)lo<<4)|((uint32_t)hi>>4);
                len=(uint32_t)(hi&0x0fu)+3u;
                if(!off||off>o-floor_off)return 0;
                while(len--&&o<raw){out[o]=out[o-off];++o;}
            }else out[o++]=*src++;
        }
    }
    return o==raw;
}

void kgfx_icons_unload(void)
{
    icon_color_count=0;icon_color_table=0;icon_color_palette=0;
    if(icon_alloc_table){(void)kmem_free(icon_alloc_table,KMEM_OWNER_KERNEL);icon_alloc_table=0;}
    if(icon_alloc_data){(void)kmem_free(icon_alloc_data,KMEM_OWNER_KERNEL);icon_alloc_data=0;}
}

/* Zyczenie rozmiaru na NASTEPNE kgfx_icons_load() (0 = domyslne 48). */
void kgfx_icons_request(uint16_t px){icon_want_px=px?px:ICON_SIZE;}
uint16_t kgfx_icons_size(void){return icon_px;}
/* Tryb siatki pulpitu (0 COMPACT, 1 NORMAL, 2 WIDE).  Wartosc jest czysto
   ukladu, ale trzyma ja jadro z tego samego powodu co tapeta i kolor paska:
   SETTINGS zmienia, pulpit zauwaza pollingiem i przelicza layout. */
static uint8_t icon_grid=0u;   /* 0 COMPACT = dotychczasowy uklad (zrzut startowy) */
void kgfx_icon_grid_set(uint8_t mode){icon_grid=mode>2u?2u:mode;}
uint8_t kgfx_icon_grid(void){return icon_grid;}
int kgfx_icons_loaded(void){return icon_color_count!=0;}

/* Dekoduje sekcje 16 px z tego samego bloba, zapamietuje ja w malych buforach
   i przywraca glowny rozmiar.  Wywolac RAZ, po kgfx_icons_load(). */
int kgfx_icons_small_prepare(const void *blob,uint32_t size)
{
    uint16_t main_px=icon_px;
    unsigned i;
    if(icon_small_count)return 1;                 /* juz przygotowane */
    if(!blob||!size)return 0;
    kgfx_icons_unload();
    kgfx_icons_request(ICON_SMALL_PX);
    if(!kgfx_icons_load(blob,size)){             /* brak sekcji 16 px */
        kgfx_icons_request(main_px);
        (void)kgfx_icons_load(blob,size);
        return 0;
    }
    /* Przejmij bufory 16 px: loader nie moze ich zwolnic przy powrocie. */
    icon_small_count=icon_color_count;
    for(i=0;i<icon_small_count&&i<ICON_COLOR_COUNT;++i)icon_small_table[i]=icon_color_table[i];
    icon_small_alloc_table=icon_alloc_table;icon_small_alloc_data=icon_alloc_data;
    icon_alloc_table=0;icon_alloc_data=0;
    kgfx_icons_unload();
    kgfx_icons_request(main_px);
    (void)kgfx_icons_load(blob,size);
    kdbg_icon_small=icon_small_count;
    return icon_small_count?1:0;
}
int kgfx_icons_small_ready(void){return icon_small_count!=0;}
/* Rysuje ikone 16 px (pasek na dole).  x,y to lewy gorny rog. */
void kgfx_icon_small_draw(int32_t x,int32_t y,uint8_t id)
{
    const icon_color_t *ic;surface_t s;cursor_guard_t guard;
    int32_t cx=x,cy=y,cw=(int32_t)ICON_SMALL_PX,ch=(int32_t)ICON_SMALL_PX;
    uint16_t px,py;
    if(id>=icon_small_count)return;
    ic=&icon_small_table[id];
    if(!ic->mask||!ic->pixels)return;
    s=active_surface();
    if(!s.chunky)return;
    if(!clip_rect(&cx,&cy,&cw,&ch))return;
    guard=cursor_guard_begin(cx,cy,cw,ch);
    for(py=0;py<ICON_SMALL_PX;py++)for(px=0;px<ICON_SMALL_PX;px++){
        if(x+(int32_t)px<cx||y+(int32_t)py<cy||x+(int32_t)px>=cx+cw||y+(int32_t)py>=cy+ch)continue;
        if(ic->mask[((uint32_t)py*ICON_SMALL_PX+px)>>3]&(uint8_t)(0x80u>>(px&7u)))
            surface_pixel_set(&s,(uint16_t)(x+px),(uint16_t)(y+py),
                (uint8_t)(ICON_PAL_BASE+ic->pixels[(uint32_t)py*ICON_SMALL_PX+px]));
    }
    cursor_guard_end(guard);
    mark_dirty(cx,cy,cw,ch);
}

/* Zainstaluj uklad v1 lezacy pod `base` (naglowek + wpisy + paleta + dane). */
static int icons_install(const uint8_t *base,uint32_t size,uint16_t px)
{
    const icons_blob_header_t *h=(const icons_blob_header_t *)base;
    const icons_blob_entry_t *e;
    icon_color_t *table;uint32_t i;
    uint16_t mb=(uint16_t)(((uint32_t)px*px+7u)/8u),pb=(uint16_t)((uint32_t)px*px);
    if(h->magic!=ICONS_BLOB_MAGIC)return 0;
    if(h->count!=ICON_COLOR_COUNT||h->count>ICONS_BLOB_MAX_COUNT)return 0;
    if(h->entries_off+(uint32_t)ICON_COLOR_COUNT*(uint32_t)sizeof(icons_blob_entry_t)>size)return 0;
    if(h->palette_off+(uint32_t)ICON_PAL_COLORS*2u>size)return 0;
    table=(icon_color_t *)kmem_alloc((uint32_t)ICON_COLOR_COUNT*(uint32_t)sizeof(icon_color_t),4u,
                                     KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
    if(!table)return 0;
    e=(const icons_blob_entry_t *)(base+h->entries_off);
    for(i=0;i<ICON_COLOR_COUNT;i++){
        if(e[i].mask_off+mb>size||e[i].pixels_off+pb>size){
            (void)kmem_free(table,KMEM_OWNER_KERNEL);return 0;}
        table[i].mask=base+e[i].mask_off;table[i].pixels=base+e[i].pixels_off;
    }
    icon_alloc_table=table;
    icon_color_table=table;
    icon_color_palette=(const uint16_t *)(base+h->palette_off);
    icon_color_count=ICON_COLOR_COUNT;
    icon_px=px;icon_mask_bytes=mb;icon_pixel_bytes=pb;
    return 1;
}

int kgfx_icons_load(const void *blob,uint32_t size)
{
    const uint8_t *base=(const uint8_t *)blob;
    const icons_blob_header_t *h;
    if(!blob||size<sizeof(icons_blob_header_t))return 0;
    if(icon_color_table)return 1;                  /* juz wczytane */
    h=(const icons_blob_header_t *)blob;
    if(h->magic!=ICONS_BLOB_MAGIC)return 0;
    if(h->version==ICONS_BLOB_VERSION_SIZES){
        /* v5: sekcja rozmiaru zyczenia, inaczej 48, inaczej pierwsza. */
        const icons_section_t *sec;uint32_t n=h->count,i,best=0xffffffffu,fallback=0xffffffffu;
        uint8_t *out;
        if(!n||n>8u||h->entries_off+n*ICONS_SECTION_STRUCT>size)return 0;
        sec=(const icons_section_t *)(base+h->entries_off);
        for(i=0;i<n;++i){
            if(sec[i].px==(uint32_t)icon_want_px)best=i;
            if(sec[i].px==(uint32_t)ICON_SIZE&&fallback==0xffffffffu)fallback=i;
        }
        if(best==0xffffffffu)best=(fallback!=0xffffffffu)?fallback:0u;
        if(sec[best].blob_off>size||sec[best].blob_size>size-sec[best].blob_off)return 0;
        if(sec[best].raw_size<sizeof(icons_blob_header_t)||sec[best].raw_size>4u*1024u*1024u)return 0;
        out=(uint8_t *)kmem_alloc(sec[best].raw_size,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
        if(!out)return 0;
        if(!icons_lzss(base+sec[best].blob_off,sec[best].blob_size,out,sec[best].raw_size,0u)||
           !icons_install(out,sec[best].raw_size,(uint16_t)sec[best].px)){
            (void)kmem_free(out,KMEM_OWNER_KERNEL);return 0;}
        icon_alloc_data=out;
        return 1;
    }
    if(h->version>=2u&&h->version<=4u){
        /* Jeden rozmiar spakowany: rozwin CALY ogon pliku do bufora, ustaw w nim
           version=1 (uklad v1) i zainstaluj - offsetom wpisow to wystarcza. */
        const icons_blob_entry_t *ee=(const icons_blob_entry_t *)(base+h->entries_off);
        uint32_t first=0xffffffffu,i,raw,total;uint8_t *out;int ok;
        for(i=0;i<ICON_COLOR_COUNT;++i){
            if(ee[i].mask_off<first)first=ee[i].mask_off;
            if(ee[i].pixels_off<first)first=ee[i].pixels_off;
        }
        if(first==0xffffffffu||first>size||first+4u>size)return 0;
        raw=icons_be32(base+first);
        if(raw>4u*1024u*1024u)return 0;
        total=first+raw;
        if(total<first)return 0;
        out=(uint8_t *)kmem_alloc(total,4u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
        if(!out)return 0;
        for(i=0;i<first;++i)out[i]=base[i];
        if(h->version==2u)ok=icons_rle(base+first+4u,size-first-4u,out+first,raw);
        else if(h->version==3u)ok=icons_packbits(base+first+4u,size-first-4u,out+first,raw);
        else ok=icons_lzss(base+first+4u,size-first-4u,out+first,raw,first);
        if(!ok){(void)kmem_free(out,KMEM_OWNER_KERNEL);return 0;}
        out[4]=0u;out[5]=0u;out[6]=0u;out[7]=1u;   /* w buforze uklad v1 */
        if(!icons_install(out,total,ICON_SIZE)){(void)kmem_free(out,KMEM_OWNER_KERNEL);return 0;}
        icon_alloc_data=out;
        return 1;
    }
    if(h->version!=ICONS_BLOB_VERSION)return 0;
    return icons_install(base,size,ICON_SIZE);   /* v1: rysowanie wprost z bufora */
}

/* Program the shared icon palette into RAMDAC entries ICON_PAL_BASE.. */
void kgfx_icon_palette(void)
{
    uint8_t i;
    if(!icon_color_palette)return;
    for(i=0;i<ICON_PAL_COLORS;i++)
        (void)kgfx_set_palette((uint8_t)(ICON_PAL_BASE+i),icon_color_palette[i]);
}

void kgfx_emutos_background(int32_t x,int32_t y,int32_t w,int32_t h)
{
    /* EmuDesk INF_Q defaults: IP_4PATT, black in mono, green in colour.
       vdi_fill.c DITHER intensity 8. Screen coordinates preserve its phase. */
    static const uint16_t pattern[4]={0xaaaa,0x5555,0xaaaa,0x5555};
    surface_t s;cursor_guard_t guard;
    if(!clip_rect(&x,&y,&w,&h))return;
    guard=cursor_guard_begin(x,y,w,h);s=active_surface();
    if(s.chunky){
        /* Solid palette entry 2, including unaligned left/right edges. */
        chunky_fill((volatile uint8_t *)s.words+(uint32_t)y*s.line_words*2u+x,
                    (uint32_t)s.line_words*2u,(uint32_t)w,(uint32_t)h,2u);
        /* Icons are app-owned now (DESKTOP.APP draws them through GUI_ICON),
           so the background is just the solid desktop colour. */
        mark_dirty(x,y,w,h);cursor_guard_end(guard);return;
    }
    emutos_vdi_planar_fill(s.words,s.line_words,s.planes,x,y,w,h,
        s.planes==1?0:2,1,pattern,3);
    mark_dirty(x,y,w,h);cursor_guard_end(guard);
}

void kgfx_emutos_window(int32_t x,int32_t y,int32_t w,int32_t h)
{
    static const uint16_t title_pattern[4]={0,0x5555,0,0x5555};
    surface_t s;cursor_guard_t guard;int32_t tx,ty,tw,th;
    /* This fixed resource has a minimum geometry; callers cannot produce
       underflowed gadget dimensions. All additions fit signed 32 bits. */
    if(w<64||h<40||w>graphics.width||h>graphics.height||x<0||y<0||
       x>graphics.width-w||y>graphics.height-h)return;
    guard=cursor_guard_begin(x,y,w,h);
    kgfx_fill_rect(x,y,w,h,1);kgfx_frame_rect(x,y,w,h,0);
    tx=x+18;ty=y+1;tw=w-19;th=17;s=active_surface();
    /* AES gemwmlib.c: active W_NAME uses opaque pattern 2, black/white. */
    emutos_vdi_planar_fill(s.words,s.line_words,s.planes,tx,ty,tw,th,0,1,title_pattern,3);
    kgfx_line(x,y+18,x+w-1,y+18,0);
    kgfx_frame_rect(x,y,18,19,0);
    kgfx_text(x+5,y+1,"\005",0,1,1); /* W_CLOSER */
    kgfx_text(x+24,y+1," ATARI DESKTOP ",0,1,1);
    kgfx_frame_rect(x+w-17,y+h-19,17,19,0);
    kgfx_text(x+w-12,y+h-18,"\006",0,1,1); /* W_SIZER */
    mark_dirty(x,y,w,h);cursor_guard_end(guard);
}

void kgfx_emutos_controls(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t state)
{
    cursor_guard_t guard;const char *mode,*size;
    if(w<220||h<130||w>graphics.width||h>graphics.height||x<0||y<0||
        x>graphics.width-w||y>graphics.height-h)return;
    guard=cursor_guard_begin(x,y,w,h);
    kgfx_fill_rect(x+16,y+38,80,22,1);kgfx_frame_rect(x+16,y+38,80,22,0);
    kgfx_fill_rect(x+112,y+38,80,22,1);kgfx_frame_rect(x+112,y+38,80,22,0);
    kgfx_frame_rect(x+((state&4u)?114:18),y+40,76,18,0);
    kgfx_text(x+32,y+41,(state&1u)?"READY":"START",0,1,1);
    kgfx_text(x+136,y+41,(state&2u)?"HIDE":"INFO",0,1,1);
    kgfx_fill_rect(x+16,y+70,208,52,1);
    if(state&2u){
        if(graphics.mode==2){mode="SHIFTER MONO";size="640 X 400";}
        else if(graphics.mode==1){mode="SHIFTER MEDIUM";size="640 X 200";}
        else{mode="SHIFTER LOW";size="320 X 200";}
        kgfx_text(x+16,y+72,mode,0,1,1);kgfx_text(x+16,y+88,size,0,1,1);
        kgfx_text(x+16,y+104,"SURFACE READY",0,1,1);
    }
    cursor_guard_end(guard);
}

/* Win98-style 2px bevel: paper on the lit edges, ink on the shaded edges. */
static void bevel_rect(int32_t x,int32_t y,int32_t w,int32_t h,uint8_t ink,uint8_t paper,int sunken)
{
    uint8_t hi=sunken?ink:paper,lo=sunken?paper:ink;
    if(w<5||h<5)return;
    kgfx_frame_rect(x,y,w,h,ink);                 /* outer ink outline */
    kgfx_fill_rect(x+1,y+1,w-2,1,hi);             /* inner lit edge */
    kgfx_fill_rect(x+1,y+1,1,h-2,hi);
    kgfx_fill_rect(x+w-2,y+1,1,h-2,lo);           /* inner shaded edge */
    kgfx_fill_rect(x+1,y+h-2,w-2,1,lo);
}
static int32_t batch_left,batch_top,batch_right,batch_bottom;static int batch_bounds_valid;
int kgfx_batch_bounds(int32_t *l,int32_t *t,int32_t *r,int32_t *b)
{
    if(!batch_bounds_valid)return 0;
    if(l)*l=batch_left;
    if(t)*t=batch_top;
    if(r)*r=batch_right;
    if(b)*b=batch_bottom;
    return 1;
}
/* Bounded display list. Window ownership/composition remains in user space. */
void kgfx_draw_batch(const gui_draw_t *list,uint32_t count)
{
    uint32_t i;int visible=kgfx_cursor_visible();int16_t cx=cursor_x,cy=cursor_y;
    int left,top,right,bottom;uint32_t first,words;int use_backbuffer;
    volatile uint8_t *target=(volatile uint8_t *)graphics.framebuffer;
    batch_bounds_valid=0;
    if(count>GUI_DRAW_MAX)return;
    if(!desktop_is_active)return;
    if(!gui_damage(list,count,graphics.width,graphics.height,&left,&top,&right,&bottom))return;
    batch_left=left;batch_top=top;batch_right=right;batch_bottom=bottom;batch_bounds_valid=1;
    first=(uint32_t)(left/16)*graphics.planes;
    words=(uint32_t)((right+15)/16-left/16)*graphics.planes;
    /* kgfx_set_target() may have pointed this batch at an application surface
       (a WM window).  The backbuffer exists only to keep the VISIBLE desktop
       from showing a half-drawn frame; an off-screen surface can be painted in
       place.  Without this test the batch always drew into desktop_framebuffer,
       so WM application windows stayed blank. */
    use_backbuffer=(target==desktop_framebuffer)||!kernel_wm_enabled();
    if(use_backbuffer&&!desktop_backbuffer){
        desktop_backbuffer=(volatile uint8_t *)kmem_alloc(KGFX_MAX_FRAME_BYTES,256u,KMEM_OWNER_KERNEL,
            KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);   /* TT-RAM, jak ramka */
        if(desktop_backbuffer)kmem_pin((void *)desktop_backbuffer,KMEM_OWNER_KERNEL);
    }
    kgfx_overlay_hide();
    if(visible)kgfx_cursor_hide();
    if(use_backbuffer&&desktop_backbuffer){
        volatile uint16_t *dst=(volatile uint16_t *)desktop_backbuffer;
        const volatile uint16_t *src=(const volatile uint16_t *)target;
        gui_damage_copy(dst,src,graphics.line_bytes/2u,first,words,(unsigned)top,(unsigned)bottom);
        graphics.framebuffer=desktop_backbuffer;
    }
    for(i=0;i<count;i++){
        gui_draw_t c;uint32_t j;
        for(j=0;j<sizeof(c);j++)((volatile uint8_t *)&c)[j]=((const uint8_t *)&list[i])[j];
        c.text[31]=0;
        switch(c.kind){
        case GUI_BG:kgfx_emutos_background(c.x,c.y,c.w,c.h);break;
        case GUI_FILL:kgfx_fill_rect(c.x,c.y,c.w,c.h,c.color);break;
        case GUI_FRAME:kgfx_frame_rect(c.x,c.y,c.w,c.h,c.color);break;
        case GUI_BEVEL:bevel_rect(c.x,c.y,c.w,c.h,(uint8_t)c.color,(uint8_t)c.background,(int)c.reserved);break;
        case GUI_TEXT:
            /* reserved bit 0 = draw glyph pixels only.  Text sitting on a bar the
               renderer knows nothing about (the steel top bar) must not carry a
               rectangular patch of its own background colour. */
            kgfx_text(c.x,c.y,c.text,c.color,c.background,(int)((c.reserved&1u)?0:1));break;
        case GUI_ICON:kgfx_icon(c.x,c.y,(uint8_t)(c.reserved&0x7fu),c.color,c.background,c.text,(int)(c.reserved>>7));break;
        case GUI_ICON_RGB:kgfx_icon_color(c.x,c.y,(uint8_t)c.color,c.text);break;
        case GUI_PATTERN:++kdbg_pat_calls;kgfx_pattern_fill(c.x,c.y,(int32_t)c.w,(int32_t)c.h,(unsigned)c.color);break;
        /* Siatka edytora kafelka jako ELEMENT BATCHA - dzieki temu rysuje sie
           w powierzchni okna (a nie w ekranie, jak natychmiastowy opcode). */
        case GUI_TILE_GRID:kgfx_tile_grid(c.x,c.y,(int32_t)c.w/16,(int32_t)c.h/16);break;
        case GUI_WINDOW:{
            static const uint16_t pattern[4]={0,0x5555,0,0x5555};
            surface_t s;uint16_t limit;
            if(c.x<0||c.y<0||c.w<64||c.h<40||c.w>graphics.width||c.h>graphics.height||
               c.x>graphics.width-c.w||c.y>graphics.height-c.h)break;
            kgfx_emutos_window(c.x,c.y,c.w,c.h);
            kgfx_fill_rect(c.x+18,c.y+1,c.w-19,17,1);
            if(c.color){s=active_surface();emutos_vdi_planar_fill(s.words,s.line_words,s.planes,
                c.x+18,c.y+1,c.w-19,17,0,1,pattern,3);}
            limit=(c.w-44)/8;if(limit>31)limit=31;c.text[limit]=0;
            kgfx_text(c.x+24,c.y+1,c.text,0,1,1);break;
        }
        default:break;
        }
    }
    if(use_backbuffer&&desktop_backbuffer){
        /* Keep the displayed address stable. Only completed damage is copied
         * to the visible screen; no blank intermediate scene is presented. */
        volatile uint16_t *dst=(volatile uint16_t *)target;
        const volatile uint16_t *src=(const volatile uint16_t *)desktop_backbuffer;
        gui_damage_copy(dst,src,graphics.line_bytes/2u,first,words,(unsigned)top,(unsigned)bottom);
    }
    graphics.framebuffer=target;   /* in-place surface draws keep their target */
    if(visible)kgfx_cursor_show(cx,cy);
}

/* Mark changed character pairs before the renderer updates its cache.
 * ET4000 never scans the full bitmap just because one shell character changed. */
static void terminal_video_damage(const term_display_t *cache,const term_cell_t *cells)
{
 unsigned row,col;
 if(!kvideo_is_vofa())return;
 if(!cache->valid){kvideo_damage(0,0,(int32_t)graphics.width,(int32_t)graphics.height);return;}
 for(row=0;row<TERM_ROWS;row++)for(col=0;col<TERM_COLS;col+=2){
  unsigned i=row*TERM_COLS+col;
  if(cache->cells[i]!=cells[i]||cache->cells[i+1]!=cells[i+1])
   kvideo_damage((int32_t)col*8,(int32_t)row*16,16,16);
 }
}

/* Foreground terminal presentation: no bitmap copies or framebuffer flips.
 * Hide the pointer while updating its saved-under pixels, then restore it. */
int kgfx_terminal_submit(unsigned session,const term_cell_t *cells)
{
 unsigned i;
 if(session>=SESSION_COUNT)return -0x3e0;
 for(i=0;i<TERM_CELLS;i++)session_cells[session][i]=cells[i];
 session_cells_valid[session]=1;
 return session==ksessions_current()?kgfx_terminal_present(cells):0;
}

int kgfx_terminal_present(const term_cell_t *cells)
{
 int visible;int16_t x,y;unsigned pairs,i;
 unsigned session=ksessions_current();
 for(i=0;i<TERM_CELLS;i++)session_cells[session][i]=cells[i];
 session_cells_valid[session]=1;
 /* Desktop mode needs a mono chunky-free surface; the SIZE follows the active
    video mode (Shifter 640x400, ET4000 up to 1024x768). */
 if(!desktop_is_active||graphics.planes!=1)return -0x3e6;
 if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
 kgfx_overlay_hide();visible=kgfx_cursor_visible();x=cursor_x;y=cursor_y;
 if(visible)kgfx_cursor_hide();
 if(kgfx_desktop_active())terminal_video_damage(&terminal_display,cells);
 /* Render into the CURRENT target: kgfx_set_target() points this at an
    application's window surface while its draw list is being processed. */
 pairs=term_render_mono_polled(&terminal_display,cells,(volatile uint16_t *)graphics.framebuffer,terminal_font,kernel_ikbd_poll_capture);
 if(visible)kgfx_cursor_show(x,y);
 return (int)pairs;
}

int kgfx_console_present(term_display_t *cache,const term_cell_t *cells)
{
 int visible;int16_t x,y;unsigned pairs;
 if(kgfx_desktop_active()||graphics.planes!=1)return -0x3e6;
 if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
 kgfx_overlay_hide();visible=kgfx_cursor_visible();x=cursor_x;y=cursor_y;
 if(visible)kgfx_cursor_hide();
 /* Console target. With the Shifter this is SCREEN_BASE. With the ET4000
    active the adapter scans out its own VRAM and Hatari blanks the Shifter
    raster every frame, so drawing into SCREEN_BASE got wiped before the copy
    reached the card. Render straight into the scanned-out card buffer and let
    kvideo_flush() keep the shadow copy in step. */
 {const void *scanout=kvideo_scanout();
  volatile uint16_t *target=(volatile uint16_t *)(uintptr_t)(scanout?scanout:(const void *)(uintptr_t)SCREEN_BASE);
  if(scanout)kvideo_select((const volatile uint8_t *)scanout);
  terminal_video_damage(cache,cells);
  /* BOOT-time renderer (the WM path uses kgfx_console_render): it used the
     mono renderer unconditionally, so a 256-colour boot drew mono glyphs into
     the card's chunky VRAM and nothing ever appeared. */
  if(graphics.mode==8u)
      pairs=term_render_chunky_polled(cache,cells,(volatile uint8_t *)target,
                                      graphics.line_bytes,terminal_font,kernel_ikbd_poll_capture);
  else
      pairs=term_render_mono_polled(cache,cells,target,terminal_font,kernel_ikbd_poll_capture);}
 if(visible)kgfx_cursor_show(x,y);
 kvideo_flush();return (int)pairs;
}

/* Small system-owned overlay: save-under is independent of app cell caches. */
static uint16_t overlay_saved[4*64];
static volatile uint16_t *overlay_screen;
static unsigned overlay_rows;
static term_cell_t overlay_cells[32];
void kgfx_overlay_hide(void)
{
 unsigned y,x;
 if(!overlay_rows)return;
 kgfx_cursor_hide();
 for(y=0;y<overlay_rows*16;y++)for(x=0;x<4;x++)
     overlay_screen[y*40+36+x]=overlay_saved[y*4+x];
 kvideo_damage(576,0,64,(int32_t)overlay_rows*16);
 overlay_rows=0;
}
void kgfx_overlay_show(const term_cell_t *cells,unsigned rows)
{
 unsigned y,x,line;volatile uint16_t *out;
 if(graphics.mode!=2u||!rows||rows>4)return;
 if(rows==overlay_rows){
  for(x=0;x<rows*8&&cells[x]==overlay_cells[x];x++);
  if(x==rows*8)return;
 }
 kgfx_overlay_hide();kgfx_cursor_hide();
 if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
 overlay_screen=system_scanout();
 overlay_rows=rows;
 kvideo_damage(576,0,64,(int32_t)rows*16);
 for(x=0;x<rows*8;x++)overlay_cells[x]=cells[x];
 for(y=0;y<rows*16;y++)for(x=0;x<4;x++)overlay_saved[y*4+x]=overlay_screen[y*40+36+x];
 for(y=0;y<rows;y++)for(x=0;x<8;x+=2){term_cell_t a=cells[y*8+x],b=cells[y*8+x+1];
  uint16_t invert=((a&0x100)?0xff00:0)|((b&0x100)?0xff:0);
  out=overlay_screen+y*640+36+x/2;
  for(line=0;line<16;line++,out+=40)*out=(((uint16_t)terminal_font[a&255][line]<<8)|terminal_font[b&255][line])^invert;
 }
}
