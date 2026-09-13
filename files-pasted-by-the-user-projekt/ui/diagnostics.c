#include "console.h"
#include "system.h"
#include "diagnostics.h"
#include "damage.h"

#define ITEM_COUNT 10
static const char *const labels[ITEM_COUNT]={
    " RUN ALL TESTS "," CPU "," ST-RAM QUICK "," TT-RAM DETECT/TEST ",
    " SHIFTER "," BLITTER "," MFP "," IKBD "," RTC "," BACK "
};

static void draw_title(void)
{
    uint8_t i;
    console_set_cursor(0,0);console_set_inverse(1);
    console_puts("  HARDWARE DIAGNOSTICS  ");
    for(i=23u;i<80u;++i)console_putc(' ');
    console_set_inverse(0);
}

static void draw_item(int item,int selected,const enum diag_result *results)
{
    console_clear_text_row((uint16_t)(2+item)); console_set_cursor(0,(uint16_t)(2+item));
    console_set_inverse((uint8_t)(item==selected)); console_puts(labels[item]); console_set_inverse(0);
    if(item>=1&&item<=8){console_set_cursor(24,(uint16_t)(2+item));console_puts(diag_result_name(results[item-1]));}
}
static enum diag_result run_one(int item)
{
    if(item==1)return diag_cpu();
    if(item==2)return diag_stram();
    if(item==3)return diag_ttram();
    if(item==4)return diag_shifter();
    if(item==5)return diag_blitter();
    if(item==6)return diag_mfp();
    if(item==7)return diag_ikbd();
    return diag_rtc();
}
static void draw_cost(uint8_t rects,uint32_t px)
{
    console_clear_text_row(17);console_set_cursor(0,17);
    console_puts(" REDRAW: ");console_put_u32(rects);console_puts(" RECT  ");
    console_put_u32(px);console_puts(" PX    FULL SCREEN = 307200 PX");
}

void diagnostics_run(void)
{
    enum diag_result results[8];
    int key,selected=0,old,i,nav_ready=1;
    for(i=0;i<8;++i)results[i]=DIAG_NOT_RUN;
    damage_reset();
    console_clear();
    draw_title();
    for(i=0;i<ITEM_COUNT;++i)draw_item(i,selected,results);
    console_set_cursor(0,13);console_puts("UP/DOWN SELECT   ENTER RUN   ESC MAIN");
    draw_cost(damage_count(),damage_pixels());
    for(;;){
        key=ikbd_scancode();
        if(key==0x148||key==0x150){nav_ready=1;continue;} old=selected;
        if(key==0x48&&nav_ready){nav_ready=0;selected=selected?selected-1:ITEM_COUNT-1;}
        if(key==0x50&&nav_ready){nav_ready=0;selected=(selected+1)%ITEM_COUNT;}
        if(old!=selected){
            uint8_t r;uint32_t p;
            damage_reset();
            draw_item(old,selected,results);draw_item(selected,selected,results);
            r=damage_count();p=damage_pixels();draw_cost(r,p);
        }
        if(key==0x01)return;
        if(key!=0x1c&&key!=0x72)continue;
        if(selected==ITEM_COUNT-1)return;
        damage_reset();
        if(selected==0)for(i=1;i<=8;++i){results[i-1]=run_one(i);draw_item(i,selected,results);}
        else{results[selected-1]=run_one(selected);draw_item(selected,selected,results);}
        if(g_system.tt_ram_bytes){console_clear_text_row(15);console_set_cursor(0,15);
            console_puts("TT-RAM DETECTED: ");console_put_u32(g_system.tt_ram_bytes/1024u);console_puts(" KB MINIMUM");}
        {uint8_t r=damage_count();uint32_t p=damage_pixels();draw_cost(r,p);}
    }
}
