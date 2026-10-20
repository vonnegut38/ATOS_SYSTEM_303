#include "console.h"
#include "system.h"
#include "rtc.h"
#include "config.h"

static rtc_datetime_t edit_dt;
static uint8_t edit_clock;
static uint8_t edit_video;
static uint8_t edit_mode;
static uint8_t edit_blitter;
static uint8_t edit_auto_tos;
static uint8_t edit_boot;
/* Nominal values a user can select when the bus carries no readable clock. */
static const uint8_t clock_steps[]={0u,8u,16u,25u,32u,40u,50u};
static void put2(uint8_t v){console_putc((char)('0'+v/10u));console_putc((char)('0'+v%10u));}
static void put4(uint16_t v){console_putc((char)('0'+(v/1000u)%10u));console_putc((char)('0'+(v/100u)%10u));console_putc((char)('0'+(v/10u)%10u));console_putc((char)('0'+v%10u));}
static void draw_value(int selected,int n,uint16_t v,int four)
{console_set_inverse((uint8_t)(selected==n));if(four)put4(v);else put2((uint8_t)v);console_set_inverse(0);}
static const char *video_pref_name(uint8_t value)
{if(value==VIDEO_PREF_SHIFTER)return "SHIFTER";if(value==VIDEO_PREF_NOVA)return "NOVA VGA";return "AUTO";}
/* Card modes offered by the setup menu.  The order is what UP/DOWN cycles. */
static const uint8_t mode_list[4]={CFG_MODE_640,CFG_MODE_800,CFG_MODE_1024,CFG_MODE_800_8};
static const char *video_mode_name(uint8_t value)
{
    if(value==CFG_MODE_800)return "800X600 MONO";
    if(value==CFG_MODE_1024)return "1024X768 MONO";
    if(value==CFG_MODE_800_8)return "800X600X256";
    return "640X400 MONO";
}
static unsigned mode_slot(uint8_t value)
{unsigned k;for(k=0u;k<sizeof(mode_list);++k)if(mode_list[k]==value)return k;return 0u;}
static const char *boot_pref_name(uint8_t value)
{if(value==CFG_BOOT_ACSI)return "ACSI FIRST";if(value==CFG_BOOT_IDE)return "IDE FIRST";return "AUTO";}
static void draw_editor(int selected,int video_field,int mode_field,int blitter_field,int auto_tos_field,int boot_field)
{
    console_clear_text_row(4);console_set_cursor(0,4);console_puts("DATE  ");
    draw_value(selected,0,edit_dt.day,0);console_putc('-');draw_value(selected,1,edit_dt.month,0);console_putc('-');draw_value(selected,2,edit_dt.year,1);
    console_clear_text_row(5);console_set_cursor(0,5);console_puts("TIME  ");
    draw_value(selected,3,edit_dt.hour,0);console_putc(':');draw_value(selected,4,edit_dt.minute,0);console_puts(":00");
    console_clear_text_row(7);console_set_cursor(0,7);console_puts("CPU CLOCK  ");
    console_set_inverse((uint8_t)(selected==5));
    if(edit_clock){console_put_u32(edit_clock);console_puts(" MHZ");}
    else console_puts("UNKNOWN");
    console_set_inverse(0);
    console_puts(g_system.mega_ste?"  HARDWARE":"  SETUP VALUE");
    console_clear_text_row(8);console_set_cursor(0,8);console_puts("VIDEO DEVICE  ");
    console_set_inverse((uint8_t)(selected==video_field));console_puts(video_pref_name(edit_video));console_set_inverse(0);
    if(!g_system.nova_present)console_puts("  NOVA DISABLED");
    console_clear_text_row(9);console_set_cursor(0,9);console_puts("VIDEO MODE  ");
    console_set_inverse((uint8_t)(selected==mode_field));console_puts(video_mode_name(edit_mode));console_set_inverse(0);
    if(!g_system.nova_present)console_puts("  NOVA REQUIRED");
    console_clear_text_row(10);console_set_cursor(0,10);console_puts("BLITTER  ");
    if(g_system.blitter_present){console_set_inverse((uint8_t)(selected==blitter_field));
        console_puts(edit_blitter?"ENABLED":"DISABLED");console_set_inverse(0);}
    else console_puts("NOT PRESENT");
    console_clear_text_row(11);console_set_cursor(0,11);console_puts("AUTO TOS  ");
    console_set_inverse((uint8_t)(selected==auto_tos_field));
    console_puts(edit_auto_tos?"EMUTOS FIRST":"OFF");console_set_inverse(0);
    console_clear_text_row(12);console_set_cursor(0,12);console_puts("BOOT DEVICE  ");
    console_set_inverse((uint8_t)(selected==boot_field));
    console_puts(boot_pref_name(edit_boot));console_set_inverse(0);
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
    int video_field=6;   /* field 5 is always CPU CLOCK */
    int mode_field=video_field+1;
    int blitter_field=g_system.blitter_present?mode_field+1:-1;
    int auto_tos_field=(blitter_field>=0?blitter_field:mode_field)+1;
    int boot_field=auto_tos_field+1;
    int field_count=boot_field+1;
    rtc_datetime_t original=g_system.datetime;edit_dt=original;edit_clock=g_system.clock_mhz;edit_video=g_system.video_preference;edit_mode=g_config.video_mode;edit_blitter=g_system.blitter_enabled;edit_auto_tos=g_config.auto_tos;edit_boot=g_config.boot_device;
    if(!g_system.nova_present)edit_mode=CFG_MODE_640;
    console_clear();console_puts("ATARI BIOS SETUP\n\nRTC  ");console_puts(rtc_type_name(g_system.rtc_type));
    console_set_cursor(0,14);console_puts("LEFT/RIGHT FIELD   UP/DOWN VALUE\nENTER APPLY   ESC CANCEL");draw_editor(selected,video_field,mode_field,blitter_field,auto_tos_field,boot_field);
    for(;;){key=ikbd_scancode();
        if(key==0x14b||key==0x14d||key==0x148||key==0x150){ready=1;continue;}
        if(!ready)continue;
        if(key==0x4b){ready=0;selected=selected?selected-1:field_count-1;draw_editor(selected,video_field,mode_field,blitter_field,auto_tos_field,boot_field);}
        else if(key==0x4d){ready=0;selected=(selected+1)%field_count;draw_editor(selected,video_field,mode_field,blitter_field,auto_tos_field,boot_field);}
        else if(key==0x48||key==0x50){ready=0;
            if(selected==5){unsigned k;for(k=0u;k<sizeof(clock_steps);++k)if(clock_steps[k]==edit_clock)break;k=(unsigned)((k+(key==0x48?1u:(unsigned)(sizeof(clock_steps)-1u)))%sizeof(clock_steps));edit_clock=clock_steps[k];}
            else if(selected==video_field){if(g_system.nova_present)edit_video=(uint8_t)((edit_video+(key==0x48?1u:2u))%3u);else edit_video=edit_video==VIDEO_PREF_AUTO?VIDEO_PREF_SHIFTER:VIDEO_PREF_AUTO;}
            else if(selected==mode_field){if(g_system.nova_present){unsigned k=mode_slot(edit_mode);k=(unsigned)((k+(key==0x48?1u:3u))%4u);edit_mode=mode_list[k];}}
            else if(selected==blitter_field)edit_blitter^=1u;
            else if(selected==auto_tos_field)edit_auto_tos^=1u;
            else if(selected==boot_field)edit_boot=(uint8_t)((edit_boot+(key==0x48?1u:2u))%3u);
            else change(selected,key==0x48?1:-1);
            draw_editor(selected,video_field,mode_field,blitter_field,auto_tos_field,boot_field);}
        else if(key==0x01){g_system.datetime=original;return;}
            else if (key == 0x1c || key == 0x72) {
                edit_dt.second = 0;
                g_system.datetime = edit_dt;
                rtc_write(&edit_dt);
                if (g_system.mega_ste) machine_set_clock(edit_clock);
                else if(edit_clock){g_system.clock_mhz=edit_clock;g_system.clock_known=2u;}
                g_config.cpu_clock_mhz=edit_clock;
                g_system.video_preference=edit_video;
                g_system.blitter_enabled=(uint8_t)(g_system.blitter_present&&edit_blitter);
                /* Persist: the editor used to touch only g_system, so nothing
                   was ever written back to BIOSBIOS.CFG. */
                g_config.video_driver=(uint8_t)edit_video;
                /* Persist the card mode: the kernel reads it from the handoff
                   and brings the desktop (and the text sessions) up in it. */
                g_config.video_mode=(uint8_t)((g_system.nova_present&&edit_video!=VIDEO_PREF_SHIFTER)?edit_mode:CFG_MODE_640);
                g_config.blitter=(uint8_t)(g_system.blitter_present&&edit_blitter?1u:0u);
                g_config.auto_tos=edit_auto_tos;
                g_config.boot_device=edit_boot;
                (void)cfg_save();
                return;
            }
    }
}
