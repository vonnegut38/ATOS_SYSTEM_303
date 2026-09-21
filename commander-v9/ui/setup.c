#include "console.h"
#include "system.h"
#include "rtc.h"

static rtc_datetime_t edit_dt;
static uint8_t edit_clock;
static uint8_t edit_video;
static uint8_t edit_blitter;
static void put2(uint8_t v){console_putc((char)('0'+v/10u));console_putc((char)('0'+v%10u));}
static void put4(uint16_t v){console_putc((char)('0'+(v/1000u)%10u));console_putc((char)('0'+(v/100u)%10u));console_putc((char)('0'+(v/10u)%10u));console_putc((char)('0'+v%10u));}
static void draw_value(int selected,int n,uint16_t v,int four)
{console_set_inverse((uint8_t)(selected==n));if(four)put4(v);else put2((uint8_t)v);console_set_inverse(0);}
static const char *video_pref_name(uint8_t value)
{if(value==VIDEO_PREF_SHIFTER)return "SHIFTER";if(value==VIDEO_PREF_NOVA)return "NOVA VGA";return "AUTO";}
static void draw_editor(int selected,int video_field,int blitter_field)
{
    console_clear_text_row(4);console_set_cursor(0,4);console_puts("DATE  ");
    draw_value(selected,0,edit_dt.day,0);console_putc('-');draw_value(selected,1,edit_dt.month,0);console_putc('-');draw_value(selected,2,edit_dt.year,1);
    console_clear_text_row(5);console_set_cursor(0,5);console_puts("TIME  ");
    draw_value(selected,3,edit_dt.hour,0);console_putc(':');draw_value(selected,4,edit_dt.minute,0);console_puts(":00");
    console_clear_text_row(7);console_set_cursor(0,7);console_puts("CPU CLOCK  ");
    if(g_system.mega_ste){console_set_inverse((uint8_t)(selected==5));console_put_u32(edit_clock);console_puts(" MHZ");console_set_inverse(0);}
    else console_puts("NOT AVAILABLE");
    console_clear_text_row(8);console_set_cursor(0,8);console_puts("VIDEO DEVICE  ");
    console_set_inverse((uint8_t)(selected==video_field));console_puts(video_pref_name(edit_video));console_set_inverse(0);
    if(!g_system.nova_present)console_puts("  NOVA DISABLED");
    console_clear_text_row(9);console_set_cursor(0,9);console_puts("BLITTER  ");
    if(g_system.blitter_present){console_set_inverse((uint8_t)(selected==blitter_field));
        console_puts(edit_blitter?"ENABLED":"DISABLED");console_set_inverse(0);}
    else console_puts("NOT PRESENT");
}
static void change(int f,int d)
{
    if(f==0)edit_dt.day=(uint8_t)((edit_dt.day-1+d+31)%31+1);
    else if(f==1)edit_dt.month=(uint8_t)((edit_dt.month-1+d+12)%12+1);
    else if(f==2){int y=(int)edit_dt.year+d;if(y<1976)y=2099;if(y>2099)y=1976;edit_dt.year=(uint16_t)y;}
    else if(f==3)edit_dt.hour=(uint8_t)((edit_dt.hour+d+24)%24);
    else edit_dt.minute=(uint8_t)((edit_dt.minute+d+60)%60);
}
void setup_run(void)
{
    int key,selected=0,ready=1;
    int video_field=g_system.mega_ste?6:5;
    int blitter_field=g_system.blitter_present?video_field+1:-1;
    int field_count=video_field+1+(g_system.blitter_present?1:0);
    rtc_datetime_t original=g_system.datetime;edit_dt=original;edit_clock=g_system.clock_mhz;edit_video=g_system.video_preference;edit_blitter=g_system.blitter_enabled;
    console_clear();console_puts("ATARI BIOS SETUP\n\nRTC  ");console_puts(rtc_type_name(g_system.rtc_type));
    console_set_cursor(0,11);console_puts("LEFT/RIGHT FIELD   UP/DOWN VALUE\nENTER APPLY   ESC CANCEL");draw_editor(selected,video_field,blitter_field);
    for(;;){key=ikbd_scancode();
        if(key==0x14b||key==0x14d||key==0x148||key==0x150){ready=1;continue;}
        if(!ready)continue;
        if(key==0x4b){ready=0;selected=selected?selected-1:field_count-1;draw_editor(selected,video_field,blitter_field);}
        else if(key==0x4d){ready=0;selected=(selected+1)%field_count;draw_editor(selected,video_field,blitter_field);}
        else if(key==0x48||key==0x50){ready=0;if(selected==video_field){if(g_system.nova_present)edit_video=(uint8_t)((edit_video+(key==0x48?1u:2u))%3u);else edit_video=edit_video==VIDEO_PREF_AUTO?VIDEO_PREF_SHIFTER:VIDEO_PREF_AUTO;}else if(selected==blitter_field)edit_blitter^=1u;else if(selected==5&&g_system.mega_ste)edit_clock=edit_clock==16u?8u:16u;else change(selected,key==0x48?1:-1);draw_editor(selected,video_field,blitter_field);}
        else if(key==0x01){g_system.datetime=original;return;}
            else if (key == 0x1c || key == 0x72) {
                edit_dt.second = 0;
                g_system.datetime = edit_dt;
                rtc_write(&edit_dt);
                if (g_system.mega_ste)
                    machine_set_clock(edit_clock);
                g_system.video_preference=edit_video;
                g_system.blitter_enabled=(uint8_t)(g_system.blitter_present&&edit_blitter);
                return;
            }
    }
}
