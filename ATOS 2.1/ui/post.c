#include "console.h"
#include "system.h"
#include "diagnostics.h"
#include "ide.h"
#include "lang.h"

system_info_t g_system;

/* ---- small formatters (the ROM is freestanding, no libc) ---------------- */
static uint16_t slen(const char *s){uint16_t n=0;while(s[n])++n;return n;}
/* ---- POST diagnostics: run once, cache, show a PASS/FAIL summary -------- */
#define POST_TEST_COUNT 9
static const char *const post_test_label[POST_TEST_COUNT]={
    "CPU","ST-RAM","RAM-PAT","SHIFTER","BLITTER","MFP","IKBD","RTC","ROM"};
static enum diag_result post_test_result[POST_TEST_COUNT];
static uint8_t post_tests_done;
static const char *post_short(enum diag_result r)
{
    if(r==DIAG_PASS)return "PASS";
    if(r==DIAG_FAIL)return "FAIL";
    if(r==DIAG_NOT_PRESENT)return "--";
    if(r==DIAG_UNSUPPORTED)return "N/A";
    return "?";
}
static void post_run_tests(void)
{
    if(post_tests_done)return;
    post_test_result[0]=diag_cpu();
    post_test_result[1]=diag_stram();
    post_test_result[2]=diag_ram_pattern();
    post_test_result[3]=diag_shifter();
    post_test_result[4]=diag_blitter();
    post_test_result[5]=diag_mfp();
    post_test_result[6]=diag_ikbd();
    post_test_result[7]=diag_rtc();
    post_test_result[8]=diag_rom_checksum();
    post_tests_done=1u;
}
int post_tests_failed(void)
{
    unsigned i;
    for(i=0u;i<POST_TEST_COUNT;++i)if(post_test_result[i]==DIAG_FAIL)return 1;
    return 0;
}

/* ROM console logo: visible before the kernel graphics backend is ready. */
static void centered(uint16_t row,const char *text)
{
    uint16_t cols=shifter_text_columns(),len=slen(text);
    console_set_cursor(cols>len?(uint16_t)((cols-len)/2u):0u,row);
    console_puts_thin(text);
}
/* Rectangle fill straight into the ROM's framebuffer (the console's own
   target): the generic video driver is not part of the ROM link, but the
   console already paints glyphs there, both on the Shifter (planar mono,
   1 bpp) and on the card (8 bpp chunky). */
static void post_fill_rect(int x,int y,int w,int h,uint8_t ink)
{
    volatile uint8_t *screen=console_screen_ptr();
    uint16_t line=shifter_line_bytes();
    int row;
    if(!screen||w<=0||h<=0)return;
    if(x<0){w+=x;x=0;}
    if(y<0){h+=y;y=0;}
    if(shifter_chunky()){
        for(row=0;row<h;row++){
            volatile uint8_t *d=screen+(uint32_t)(y+row)*line+(uint32_t)x;
            int cx;
            for(cx=0;cx<w;cx++)d[cx]=(uint8_t)(ink?1u:0u);
        }
        return;
    }
    {   /* planar mono: one word per 16 pixels of plane 0 */
        uint16_t group=shifter_group_bytes();
        uint16_t first=(uint16_t)(x>>4),last=(uint16_t)((x+w-1)>>4);
        uint16_t left=(uint16_t)(0xffffu>>(x&15));
        uint16_t right=(uint16_t)(0xffffu<<(15u-((x+w-1)&15u)));
        uint16_t g;
        if(first==last)left&=right;
        for(row=0;row<h;row++){
            volatile uint8_t *p=screen+(uint32_t)(y+row)*line+(uint32_t)first*group;
            for(g=first;g<=last;g++,p+=group){
                uint16_t mask=(uint16_t)((g==first?left:0xffffu)&(g==last?right:0xffffu));
                volatile uint16_t *wd=(volatile uint16_t *)p;
                *wd=ink?(uint16_t)(*wd|mask):(uint16_t)(*wd&~mask);
            }
        }
    }
}

/* The mirrored-E mark, the same shape the kernel splash draws. */
static uint16_t post_title_row(void)
{
    int W=shifter_text_columns()*8,H=shifter_frame_bytes()/shifter_line_bytes();
    int lh=(W/7)*3/4,ch=g_system.video_mode==VIDEO_ST_MONO?16:8;
    return (uint16_t)((H/4+lh/2+16+ch-1)/ch);
}
static uint16_t post_cell_height(void)
{return g_system.video_mode==VIDEO_ST_MONO?16u:8u;}
static uint16_t progress_fill;
static uint8_t progress_ready;

static void post_draw_logo_pixels(void)
{
    /* Width and height must come from the ACTIVE mode, not constants: the card
       boots in 800x600 while the Shifter is 640x400, and a logo drawn at the
       wrong height looked like a jump when the kernel splash redrew it.  With
       matching geometry the mark does not move between the two screens.
       frame/line gives the visible row count in both planar and chunky modes. */
    int32_t W=(int32_t)shifter_text_columns()*8;
    int32_t H=(shifter_line_bytes()?(int32_t)(shifter_frame_bytes()/shifter_line_bytes()):(int32_t)400);
    int32_t lw,lh,lx,ly,tw;
    if(H<200)H=400;
    lw=W/7;lh=(lw*3)/4;lx=(W-lw)/2;ly=H/4-lh/2;
    tw=lw/5;if(tw<2)tw=2;
    post_fill_rect(lx+lw-tw,ly,tw,lh,1);              /* right vertical */
    post_fill_rect(lx,ly,lw,tw,1);                    /* top bar */
    post_fill_rect(lx+lw/4,ly+lh/2-tw/2,lw-lw/4,tw,1);      /* middle bar, shorter */
    post_fill_rect(lx,ly+lh-tw,lw,tw,1);              /* bottom bar */
}

void post_draw_logo(void)
{
    /* The logo in GRAPHICS, not characters: the same mirrored E the kernel
       splash paints, so the mark is identical from the first screen onwards.
       The text console cannot make that shape, so it goes through the video
       driver's rectangles on the freshly cleared screen. */
    post_draw_logo_pixels();
    centered(post_title_row(),"ATOS 2.1");
}
/* The loader reports completed bytes/stages, never a timed animation. */
/* One graphical bar for the whole boot. Only its growing span and the
   status label are touched; changing a filename must not reset progress. */
void post_draw_progress(uint16_t percent,const char *label)
{
    uint16_t W=shifter_text_columns()*8u,ch=post_cell_height();
    uint16_t width=W/2u,x=(W-width)/2u;
    uint16_t row=post_title_row()+3u,y=row*ch,fill;
    if(percent>100u)percent=100u;
    fill=(uint16_t)((uint32_t)width*percent/100u);
    if(!progress_ready){
        post_fill_rect(x-2,y-2,width+4,14,1);
        post_fill_rect(x,y,width,10,0);
        progress_fill=0;progress_ready=1;
    }
    if(fill>progress_fill){
        post_fill_rect(x+progress_fill,y,fill-progress_fill,10,1);
        progress_fill=fill;
    }
    console_clear_text_row(row+2u);
    centered(row+2u,label?label:"");
}

void post_begin_boot(void)
{
    /* Preserve the logo/title pixels. Drop the BIOS hardware and F-key rows.
       post_run() owns a fresh screen on every return from setup. */
    uint16_t row=post_title_row()+2u,ch=post_cell_height();
    uint16_t h=shifter_frame_bytes()/shifter_line_bytes();
    post_fill_rect(0,row*ch,shifter_text_columns()*8u,h-row*ch,0);
    progress_ready=0;progress_fill=0;
    post_draw_progress(0u,"SCANNING BOOT DEVICE");
}

static void post_draw_tests(void)
{
    unsigned i;uint8_t bad=post_tests_failed();
    console_set_cursor(0,14);
    console_puts_thin("POST TESTS    ");console_puts_thin(bad?"FAIL":"ALL PASS");
    console_clear_text_row(15);console_set_cursor(0,15);
    for(i=0u;i<4u;++i){console_puts_thin(post_test_label[i]);console_putc_thin(':');console_puts_thin(post_short(post_test_result[i]));console_puts_thin("  ");}
    console_clear_text_row(16);console_set_cursor(0,16);
    for(i=4u;i<POST_TEST_COUNT;++i){console_puts_thin(post_test_label[i]);console_putc_thin(':');console_puts_thin(post_short(post_test_result[i]));console_puts_thin("  ");}
    console_clear_text_row(23);
    if(bad){console_set_cursor(0,23);console_set_inverse(1);
        console_puts_thin("*** POST FAILURE - F2 DIAGNOSTICS / F4 BOOT / F10 SAVE ***");
        console_set_inverse(0);}
}

/* Direct F-key shortcuts, one horizontal strip on the very bottom row. */
void post_draw_choices(uint8_t selected)
{
    static const char *const items[7]={
        "F1 SETUP","F2 DIAGNOSTICS","F3 HARDWARE","F4 BOOT",
        "F5 TOS","F10 SAVE","CONTINUE"};
    uint8_t i;
    /* Inverse video is invisible on this black ROM background (the selected
       "F1 SETUP" simply disappeared), so the selection is bracketed instead. */
    console_clear_text_row(24);console_set_cursor(0,24);
    for(i=0u;i<7u;++i){
        if(i==selected)console_puts_thin("[");
        console_puts_thin(items[i]);
        if(i==selected)console_puts_thin("]");
        if(i<6u)console_puts_thin("  ");
    }
}

void post_draw_countdown(uint8_t seconds)
{
    console_clear_text_row(23u);console_set_cursor(0,23u);
    console_puts_thin("AUTO BOOT: ");console_put_u32(seconds);
    console_puts_thin("   PRESS ANY KEY");
}

void post_run(void)
{
    console_clear();post_draw_logo();progress_ready=0;
    console_set_cursor(0,21u);console_puts_thin("CPU: ");
    console_put_u32(g_system.cpu_type==4u?68040u:68000u+10u*g_system.cpu_type);
    if(g_system.clock_known){console_puts_thin("  ");console_put_u32(g_system.clock_mhz);console_puts_thin(" MHz");}
    console_set_cursor(0,22u);console_puts_thin("ST-RAM: ");
    console_put_u32(g_system.ram_bytes/1024u);console_puts_thin(" KB   TT-RAM: ");
    console_put_u32(g_system.tt_ram_bytes/1024u);console_puts_thin(" KB");
    post_draw_choices(0);
    post_run_tests();
    if(post_tests_failed())post_draw_tests();
}

void post_boot_progress(uint16_t percent,const char *label)
{
    /* The boot screen is drawn ONCE (post_run) and never cleared again: the logo
       stays visible from power-on to the desktop and only the file name, the bar
       and the percentage change.  The old console_clear()+post_draw_logo() here
       wiped the screen and repainted the logo just as the kernel started, which
       produced the full-screen flash between POST and the boot log. */
    post_draw_progress(percent,label);
}
