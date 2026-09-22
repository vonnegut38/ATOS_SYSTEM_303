#include "text_terminal.h"
#include "kernel_console.h"
#include "kernel_video.h"
#include "gui_damage.h"
#include "kernel_graphics.h"
#include "font8.h"
#include "kernel_blitter.h"
#include "kernel_memory.h"
#include "emutos_vdi_mono.h"
#include "font.h"
#include "hardware.h"
#include "emutos_resources.inc"

typedef struct {volatile uint16_t *words;uint16_t width,height,line_words;uint8_t planes;} surface_t;
static kgfx_info_t graphics;
static term_display_t terminal_display;
static uint8_t terminal_font[256][16],terminal_font_ready;
void kgfx_terminal_invalidate(void)
{term_display_invalidate(&terminal_display);if(!kgfx_desktop_active())kconsole_invalidate();}
static kgfx_rect_t dirty[KGFX_DIRTY_MAX];
static uint8_t dirty_count;
static volatile uint16_t cursor_mono_background[16u*2u*4u];
static int16_t cursor_x,cursor_y;
static uint8_t cursor_width,cursor_height,cursor_is_visible;
static volatile uint8_t *desktop_framebuffer;
static volatile uint8_t *desktop_backbuffer;
static uint8_t desktop_is_active,desktop_hidden;
static uint16_t console_palette[16];
static const uint16_t cursor_shape[16]={
    0x8000u,0xc000u,0xe000u,0xf000u,0xf800u,0xfc00u,0xfe00u,0xff00u,
    0xff80u,0xfc00u,0xdc00u,0x8e00u,0x0600u,0x0700u,0x0300u,0x0000u
};

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
    uint16_t group=(uint16_t)(x>>4),mask=(uint16_t)(0x8000u>>(x&15u));
    volatile uint16_t *word=surface->words+(uint32_t)y*surface->line_words+
                            (uint32_t)group*surface->planes;
    uint8_t plane,color=0;
    for(plane=0;plane<surface->planes;++plane)if(word[plane]&mask)color|=(uint8_t)(1u<<plane);
    return color;
}

static void surface_pixel_set(const surface_t *surface,uint16_t x,uint16_t y,uint8_t color)
{
    uint16_t group=(uint16_t)(x>>4),mask=(uint16_t)(0x8000u>>(x&15u));
    volatile uint16_t *word=surface->words+(uint32_t)y*surface->line_words+
                            (uint32_t)group*surface->planes;
    uint8_t plane;
    for(plane=0;plane<surface->planes;++plane){
        if(color&(uint8_t)(1u<<plane))word[plane]|=mask;else word[plane]&=(uint16_t)~mask;
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
    emutos_vdi_planar_fill(surface->words,surface->line_words,surface->planes,
        x,y,width,height,color,0,solid,0);
}

static surface_t active_surface(void)
{
    surface_t result;result.words=(volatile uint16_t *)(uintptr_t)graphics.framebuffer;
    result.width=graphics.width;result.height=graphics.height;
    result.line_words=(uint16_t)(graphics.line_bytes/2u);result.planes=graphics.planes;
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

int kgfx_init(uint8_t mode,uint8_t blitter_present,uint8_t blitter_enabled)
{
    if(mode>2u)return 0;
    graphics.mode=mode;graphics.framebuffer=(volatile uint8_t *)(uintptr_t)SCREEN_BASE;
    kvideo_reset(graphics.framebuffer);
    if(mode==0u){graphics.width=320u;graphics.height=200u;graphics.planes=4u;graphics.line_bytes=160u;}
    else if(mode==1u){graphics.width=640u;graphics.height=200u;graphics.planes=2u;graphics.line_bytes=160u;}
    else{graphics.width=640u;graphics.height=400u;graphics.planes=1u;graphics.line_bytes=80u;}
    kblit_init(blitter_present,blitter_enabled);
    /* Accelerator cards are known to disagree with the blitter (TF536 reports
       exist), so verify it before any code path relies on it. On failure the
       kernel silently falls back to the CPU fill. */
    if(blitter_present&&blitter_enabled&&!kblit_self_test())kblit_init(blitter_present,0u);
    graphics.capabilities=KGFX_CAP_LINE;
    if(mode!=2u)graphics.capabilities|=KGFX_CAP_PALETTE;
    if(kblit_present())graphics.capabilities|=KGFX_CAP_BLITTER_PRESENT;
    if(kblit_enabled())graphics.capabilities|=KGFX_CAP_BLITTER_ENABLED;
    if(mode==2u)graphics.capabilities|=KGFX_CAP_MONO_WORD_COPY;
    (void)kblit_self_test();
    if(!kblit_enabled())graphics.capabilities&=(uint16_t)~KGFX_CAP_BLITTER_ENABLED;
    desktop_framebuffer=(volatile uint8_t *)kmem_alloc(SCREEN_BYTES,256u,KMEM_OWNER_KERNEL,
                                                       KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
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
    surface_t test={storage+1,16u,4u,1u,1u};
    surface_t four_plane={planar+1,16u,1u,4u,4u};surface_t live;uint8_t i,original,replacement,max_color;
    int32_t csx,csy,cw,ch,cdx,cdy;
    surface_t copy_test={copy_words,64u,3u,4u,1u};
    surface_t copy_reference_test={copy_reference_words,64u,3u,4u,1u};
    surface_t fast_test={fast_words,64u,2u,4u,1u};
    surface_t reference_test={reference_words,64u,2u,4u,1u};
    surface_t emutos_fill_test={emutos_words,64u,2u,4u,1u};
    surface_t reference_fill_test={fill_reference,64u,2u,4u,1u};
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
uint8_t *kgfx_set_target(uint8_t *target)
{
    uint8_t *previous=(uint8_t *)graphics.framebuffer;
    if(target)graphics.framebuffer=target;
    /* The cell cache tracks one target; switching must force a full grid
       render or the other surface would only receive the changed cells. */
    term_display_invalidate(&terminal_display);
    kgfx_dirty_reset();
    return previous;
}
int kgfx_console_render(const void *cells,uint8_t *target,
                        uint16_t *dx,uint16_t *dy,uint16_t *dw,uint16_t *dh)
{
    const term_cell_t *c=(const term_cell_t *)cells;
    uint16_t x,y,minx=TERM_COLS,miny=TERM_ROWS,maxx=0,maxy=0;int any=0;
    static const uint8_t *last_target;
    if(!c||!target)return 0;
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
    term_render_mono(&terminal_display,c,(volatile uint16_t *)target,terminal_font);
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
    if(bpp)*bpp=(uint8_t)(graphics.planes==1u?1u:8u);
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
void kgfx_fill_rect(int32_t x,int32_t y,int32_t width,int32_t height,uint8_t color)
{surface_t surface;cursor_guard_t guard;if(!clip_rect(&x,&y,&width,&height))return;
 guard=cursor_guard_begin(x,y,width,height);surface=active_surface();
 if(!kblit_fill_planar(surface.words,surface.line_words,surface.planes,(uint16_t)x,(uint16_t)y,
                       (uint16_t)width,(uint16_t)height,color))
     surface_fill(&surface,(uint16_t)x,(uint16_t)y,(uint16_t)width,(uint16_t)height,color);
 mark_dirty(x,y,width,height);cursor_guard_end(guard);}
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
        emutos_vdi_vertical(surface.words,surface.line_words,surface.planes,
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
    if(graphics.mode==2u||index>15u)return 0;
    palette=(volatile uint16_t *)(uintptr_t)SHIFTER_PALETTE;
    palette[index]=(uint16_t)(color&0x0777u);
    return 1;
}

void kgfx_copy_rect(int32_t sx,int32_t sy,int32_t width,int32_t height,int32_t dx,int32_t dy)
{
    surface_t surface;cursor_guard_t guard;int32_t x,y,x_start,x_end,x_step,y_start,y_end,y_step;
    if(!clip_copy_rect(&sx,&sy,&width,&height,&dx,&dy))return;
    guard=cursor_guard_begin(sx<dx?sx:dx,sy<dy?sy:dy,
                             (sx>dx?sx:dx)-(sx<dx?sx:dx)+width,
                             (sy>dy?sy:dy)-(sy<dy?sy:dy)+height);
    surface=active_surface();
    if(dy>sy){y_start=height-1;y_end=-1;y_step=-1;}else{y_start=0;y_end=height;y_step=1;}
    if(dx>sx){x_start=width-1;x_end=-1;x_step=-1;}else{x_start=0;x_end=width;x_step=1;}
    if(surface.planes==1u){
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
        if(surface.planes==1u&&x>=0&&y>=0&&x+8<=graphics.width&&y+16<=graphics.height)
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
    surface=active_surface();surface.words=(volatile uint16_t *)(uintptr_t)((kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE));group=(uint16_t)cursor_x>>4;shift=(uint16_t)cursor_x&15u;
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
    surface=active_surface();surface.words=(volatile uint16_t *)(uintptr_t)((kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE));group=(uint16_t)x>>4;shift=(uint16_t)x&15u;
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
    if(!desktop_is_active)for(i=0;i<SCREEN_BYTES/4u;++i)destination[i]=0;
    /* MagiC-style: when the ET4000 is active, draw straight into the card
       VRAM (v_bas_ad), with no intermediate buffer - that buffer was where the
       composed desktop got lost. */
    /* Shifter displays ST-RAM at SCREEN_BASE; desktop_framebuffer lives in
       TT-RAM and is NEVER displayed there - the compositor was drawing into a
       buffer nobody shows (screen kept the stale console image). Use VOFA VRAM
       only when the ET4000 path is active, otherwise the Shifter screen. */
    graphics.framebuffer=(volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?(uintptr_t)desktop_framebuffer:(uintptr_t)SCREEN_BASE);kgfx_dirty_reset();
    if(!desktop_hidden)kvideo_select((const volatile uint8_t *)(uintptr_t)(kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE));
    desktop_is_active=1;return 1;
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

int kgfx_desktop_active(void){return desktop_is_active&&!desktop_hidden;}
void kgfx_session_view(int show)
{
 uint8_t hidden=!show;
 if(hidden==desktop_hidden)return;
 kgfx_overlay_hide();kgfx_cursor_hide();desktop_hidden=hidden;
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
                       const char *label)
{
    int32_t x,y,k;uint16_t bit;const uint8_t *glyph;
    for(y=0;y<32;y++)for(x=0;x<32;x++){
        int32_t px=ix+x,py=iy+y;
        if(px<cx||py<cy||px>=cx+cw||py>=cy+ch)continue;
        bit=(uint16_t)(0x8000u>>(x&15));
        if(mask[y*2+(x>>4)]&bit)
            surface_pixel_set(s,(uint16_t)px,(uint16_t)py,(data[y*2+(x>>4)]&bit)?0:1);
    }
    for(k=0;label[k];k++){
        glyph=emutos_system_font[(uint8_t)label[k]];
        for(y=0;y<16;y++)for(x=0;x<8;x++){
            int32_t px=ix-8+k*8+x,py=iy+32+y;
            if(px>=cx&&py>=cy&&px<cx+cw&&py<cy+ch)
                surface_pixel_set(s,(uint16_t)px,(uint16_t)py,(glyph[y]&(0x80u>>x))?0:1);
        }
    }
}

void kgfx_emutos_background(int32_t x,int32_t y,int32_t w,int32_t h)
{
    /* EmuDesk INF_Q defaults: IP_4PATT, black in mono, green in colour.
       vdi_fill.c DITHER intensity 8. Screen coordinates preserve its phase. */
    static const uint16_t pattern[4]={0xaaaa,0x5555,0xaaaa,0x5555};
    surface_t s;cursor_guard_t guard;
    if(!clip_rect(&x,&y,&w,&h))return;
    guard=cursor_guard_begin(x,y,w,h);s=active_surface();
    emutos_vdi_planar_fill(s.words,s.line_words,s.planes,x,y,w,h,
        s.planes==1?0:2,1,pattern,3);
    theme_icon(&s,x,y,w,h,24,30,rs_iconmask0,rs_icondata0,"DISK C");
    theme_icon(&s,x,y,w,h,24,90,rs_iconmask0,rs_icondata0,"DISK D");
    theme_icon(&s,x,y,w,h,24,150,rs_iconmask3,rs_icondata3,"TRASH");
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

/* Bounded display list. Window ownership/composition remains in user space. */
void kgfx_draw_batch(const gui_draw_t *list,uint32_t count)
{
    uint32_t i;int visible=kgfx_cursor_visible();int16_t cx=cursor_x,cy=cursor_y;
    int left,top,right,bottom;uint32_t first,words;
    if(count>GUI_DRAW_MAX)return;
    if(!desktop_is_active)return;
    if(!gui_damage(list,count,graphics.width,graphics.height,&left,&top,&right,&bottom))return;
    first=(uint32_t)(left/16)*graphics.planes;
    words=(uint32_t)((right+15)/16-left/16)*graphics.planes;
    if(!desktop_backbuffer){
        desktop_backbuffer=(volatile uint8_t *)kmem_alloc(SCREEN_BYTES,256u,KMEM_OWNER_KERNEL,
            KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
        if(desktop_backbuffer)kmem_pin((void *)desktop_backbuffer,KMEM_OWNER_KERNEL);
    }
    kgfx_overlay_hide();
    if(visible)kgfx_cursor_hide();
    if(desktop_backbuffer){
        volatile uint16_t *dst=(volatile uint16_t *)desktop_backbuffer;
        const volatile uint16_t *src=(const volatile uint16_t *)desktop_framebuffer;
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
        case GUI_TEXT:kgfx_text(c.x,c.y,c.text,c.color,c.background,1);break;
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
    if(desktop_backbuffer){
        /* Keep the displayed address stable. Only completed damage is copied
         * to the visible screen; no blank intermediate scene is presented. */
        volatile uint16_t *dst=(volatile uint16_t *)desktop_framebuffer;
        const volatile uint16_t *src=(const volatile uint16_t *)desktop_backbuffer;
        gui_damage_copy(dst,src,graphics.line_bytes/2u,first,words,(unsigned)top,(unsigned)bottom);
        graphics.framebuffer=desktop_framebuffer;
    }
    if(visible)kgfx_cursor_show(cx,cy);
}

/* Mark changed character pairs before the renderer updates its cache.
 * ET4000 never scans the full bitmap just because one shell character changed. */
static void terminal_video_damage(const term_display_t *cache,const term_cell_t *cells)
{
 unsigned row,col;
 if(!kvideo_is_vofa())return;
 if(!cache->valid){kvideo_damage(0,0,640,400);return;}
 for(row=0;row<TERM_ROWS;row++)for(col=0;col<TERM_COLS;col+=2){
  unsigned i=row*TERM_COLS+col;
  if(cache->cells[i]!=cells[i]||cache->cells[i+1]!=cells[i+1])
   kvideo_damage((int32_t)col*8,(int32_t)row*16,16,16);
 }
}

/* Foreground terminal presentation: no bitmap copies or framebuffer flips.
 * Hide the pointer while updating its saved-under pixels, then restore it. */
int kgfx_terminal_present(const term_cell_t *cells)
{
 int visible;int16_t x,y;unsigned pairs;
 if(!desktop_is_active||graphics.planes!=1||graphics.width!=640||graphics.height!=400)return -0x3e6;
 if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
 kgfx_overlay_hide();visible=kgfx_cursor_visible();x=cursor_x;y=cursor_y;
 if(visible)kgfx_cursor_hide();
 if(kgfx_desktop_active())terminal_video_damage(&terminal_display,cells);
 /* Render into the CURRENT target: kgfx_set_target() points this at an
    application's window surface while its draw list is being processed. */
 pairs=term_render_mono(&terminal_display,cells,(volatile uint16_t *)graphics.framebuffer,terminal_font);
 if(visible)kgfx_cursor_show(x,y);
 return (int)pairs;
}

int kgfx_console_present(term_display_t *cache,const term_cell_t *cells)
{
 int visible;int16_t x,y;unsigned pairs;
 if(kgfx_desktop_active()||graphics.planes!=1||graphics.width!=640||graphics.height!=400)return -0x3e6;
 if(!terminal_font_ready){term_font_prepare(terminal_font,emutos_system_font);terminal_font_ready=1;}
 kgfx_overlay_hide();visible=kgfx_cursor_visible();x=cursor_x;y=cursor_y;
 if(visible)kgfx_cursor_hide();
 terminal_video_damage(cache,cells);
 pairs=term_render_mono(cache,cells,(volatile uint16_t *)(uintptr_t)SCREEN_BASE,terminal_font);
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
 overlay_screen=(volatile uint16_t *)(uintptr_t)(kvideo_is_vofa()?desktop_framebuffer:(volatile uint8_t *)(uintptr_t)SCREEN_BASE);
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
