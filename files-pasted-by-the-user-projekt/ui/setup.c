#include "console.h"
#include "system.h"
#include "rtc.h"
#include "config.h"
#include "damage.h"

static rtc_datetime_t edit_dt;
static uint8_t edit_clock, edit_video, edit_blitter, edit_boot;
static int video_field, blitter_field, boot_field, field_count;

static void put2(uint8_t v){console_putc((char)('0'+v/10u));console_putc((char)('0'+v%10u));}
static void put4(uint16_t v){console_putc((char)('0'+(v/1000u)%10u));console_putc((char)('0'+(v/100u)%10u));console_putc((char)('0'+(v/10u)%10u));console_putc((char)('0'+v%10u));}
static void draw_value(int selected,int n,uint16_t v,int four)
{console_set_inverse((uint8_t)(selected==n));if(four)put4(v);else put2((uint8_t)v);console_set_inverse(0);}
static const char *video_pref_name(uint8_t value)
{if(value==VIDEO_PREF_SHIFTER)return "SHIFTER";if(value==VIDEO_PREF_NOVA)return "NOVA VGA";return "AUTO";}
static const char *boot_dev_name(uint8_t value)
{if(value==1u)return "ACSI";if(value==2u)return "IDE";return "AUTO";}

/* One row per logical control so navigation can repaint just two lines. */
static uint8_t field_row(int f)
{
    if(f<=2)return 4u;
    if(f<=4)return 5u;
    if(f==video_field)return 8u;
    if(f==blitter_field)return 9u;
    if(f==boot_field)return 10u;
    return 7u;
}

static void draw_title(void)
{
    uint8_t i;
    console_set_cursor(0,0);console_set_inverse(1);
    console_puts("  ATARI BIOS SETUP  -  KONFIGURACJA PODSTAWOWA  ");
    for(i=47u;i<80u;++i)console_putc(' ');
    console_set_inverse(0);
    console_clear_text_row(1);
    console_set_cursor(0,1);console_puts(" RTC  ");console_puts(rtc_type_name(g_system.rtc_type));
}

static void draw_row(int selected,uint8_t row)
{
    console_clear_text_row(row);
    console_set_cursor(0,row);
    switch(row){
        case 4u:
            console_puts(" DATE   ");
            draw_value(selected,0,edit_dt.day,0);console_putc('-');
            draw_value(selected,1,edit_dt.month,0);console_putc('-');
            draw_value(selected,2,edit_dt.year,1);
            break;
        case 5u:
            console_puts(" TIME   ");
            draw_value(selected,3,edit_dt.hour,0);console_putc(':');
            draw_value(selected,4,edit_dt.minute,0);console_puts(":00");
            break;
        case 7u:
            console_puts(" CPU CLOCK    ");
            if(g_system.mega_ste){console_set_inverse((uint8_t)(selected==5));console_put_u32(edit_clock);console_puts(" MHZ");console_set_inverse(0);}
            else console_puts("NOT AVAILABLE");
            break;
        case 8u:
            console_puts(" VIDEO DEVICE  ");
            console_set_inverse((uint8_t)(selected==video_field));console_puts(video_pref_name(edit_video));console_set_inverse(0);
            if(!g_system.nova_present)console_puts("  NOVA DISABLED");
            break;
        case 9u:
            console_puts(" BLITTER       ");
            if(g_system.blitter_present){console_set_inverse((uint8_t)(selected==blitter_field));console_puts(edit_blitter?"ENABLED":"DISABLED");console_set_inverse(0);}
            else console_puts("NOT PRESENT");
            break;
        case 10u:
            console_puts(" BOOT DEVICE   ");
            console_set_inverse((uint8_t)(selected==boot_field));console_puts(boot_dev_name(edit_boot));console_set_inverse(0);
            break;
        default: break;
    }
}

/* Live proof of incremental drawing. */
static void draw_cost(uint8_t rects,uint32_t px)
{
    console_clear_text_row(13);console_set_cursor(0,13);
    console_puts(" REDRAW: ");console_put_u32(rects);console_puts(" RECT  ");
    console_put_u32(px);console_puts(" PX    FULL SCREEN = 307200 PX");
}

void setup_run(void)
{
    int key,selected=0,ready=1;
    rtc_datetime_t original;
    video_field=g_system.mega_ste?6:5;
    blitter_field=g_system.blitter_present?video_field+1:-1;
    boot_field=(blitter_field>=0?blitter_field:video_field)+1;
    field_count=boot_field+1;
    original=g_system.datetime;edit_dt=original;
    edit_clock=g_system.clock_mhz;edit_video=g_system.video_preference;
    edit_blitter=g_system.blitter_enabled;edit_boot=g_config.boot_device;

    damage_reset();
    console_clear();
    draw_title();
    draw_row(selected,4u);draw_row(selected,5u);draw_row(selected,7u);
    draw_row(selected,8u);draw_row(selected,9u);draw_row(selected,10u);
    console_clear_text_row(11);console_set_cursor(0,11);
    console_puts(" LEFT/RIGHT FIELD   UP/DOWN VALUE   ENTER APPLY   ESC CANCEL");
    draw_cost(damage_count(),damage_pixels());

    for(;;){
        key=ikbd_scancode();
        if(key==0x14b||key==0x14d||key==0x148||key==0x150){ready=1;continue;}
        if(!ready)continue;
        if(key==0x4b||key==0x4d){
            int old=selected;uint8_t r;uint32_t p;
            ready=0;
            if(key==0x4b)selected=selected?selected-1:field_count-1;
            else selected=(selected+1)%field_count;
            damage_reset();
            if(field_row(old)!=field_row(selected))draw_row(selected,field_row(old));
            draw_row(selected,field_row(selected));
            r=damage_count();p=damage_pixels();draw_cost(r,p);
        }
        else if(key==0x48||key==0x50){
            uint8_t r;uint32_t p;
            ready=0;
            if(selected==video_field){if(g_system.nova_present)edit_video=(uint8_t)((edit_video+(key==0x48?1u:2u))%3u);else edit_video=edit_video==VIDEO_PREF_AUTO?VIDEO_PREF_SHIFTER:VIDEO_PREF_AUTO;}
            else if(selected==blitter_field)edit_blitter^=1u;
            else if(selected==boot_field)edit_boot=(uint8_t)((edit_boot+1u)%3u);
            else if(selected==5&&g_system.mega_ste)edit_clock=edit_clock==16u?8u:16u;
            else if(selected==0)edit_dt.day=(uint8_t)((edit_dt.day-1+(key==0x48?1:-1)+31)%31+1);
            else if(selected==1)edit_dt.month=(uint8_t)((edit_dt.month-1+(key==0x48?1:-1)+12)%12+1);
            else if(selected==2){int y=(int)edit_dt.year+(key==0x48?1:-1);if(y<1976)y=2099;if(y>2099)y=1976;edit_dt.year=(uint16_t)y;}
            else if(selected==3)edit_dt.hour=(uint8_t)((edit_dt.hour+(key==0x48?1:-1)+24)%24);
            else edit_dt.minute=(uint8_t)((edit_dt.minute+(key==0x48?1:-1)+60)%60);
            damage_reset();
            draw_row(selected,field_row(selected));
            r=damage_count();p=damage_pixels();draw_cost(r,p);
        }
        else if(key==0x01){g_system.datetime=original;return;}
        else if(key==0x1c||key==0x72){
            edit_dt.second=0;
            g_system.datetime=edit_dt;
            rtc_write(&edit_dt);
            if(g_system.mega_ste)machine_set_clock(edit_clock);
            g_system.video_preference=edit_video;
            g_system.blitter_enabled=(uint8_t)(g_system.blitter_present&&edit_blitter);
            g_config.video_pref=edit_video;
            g_config.blitter=(uint8_t)(edit_blitter?1u:0u);
            g_config.boot_device=edit_boot;
            (void)cfg_save();
            return;
        }
    }
}
