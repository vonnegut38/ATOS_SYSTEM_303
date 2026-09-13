#include "console.h"
#include "system.h"
#include "diagnostics.h"
#include "ide.h"
#include "config.h"
#include "damage.h"
#include "boot_protocol.h"

system_info_t g_system;

/* --- small string builder (no libc) ------------------------------------ */
typedef struct {char b[72];uint8_t n;} sb_t;
static void sb_init(sb_t *s){s->n=0;s->b[0]='\0';}
static void sb_str(sb_t *s,const char *t){while(*t&&s->n<71u)s->b[s->n++]=*t++;s->b[s->n]='\0';}
static void sb_dec(sb_t *s,uint32_t v){char t[10];uint8_t n=0,i;if(!v){sb_str(s,"0");return;}while(v&&n<10u){t[n++]=(char)('0'+v%10u);v/=10u;}for(i=0u;i<n;++i){s->b[s->n++]=t[n-1u-i];}s->b[s->n]='\0';}
/* 80-column AWARD-style frame: | LABEL(16) : VALUE(59) | */
static void post_border(void){uint8_t i;console_putc('+');for(i=0u;i<78u;++i)console_putc('-');console_putc('+');console_putc('\n');}
static void post_title(const char *title)
{
    uint8_t len=0u,i,pad;
    while(title[len])++len;
    pad=(uint8_t)((78u-len)/2u);
    console_putc('|');
    for(i=0u;i<pad;++i)console_putc(' ');
    console_puts(title);
    for(i=(uint8_t)(pad+len);i<78u;++i)console_putc(' ');
    console_putc('|');console_putc('\n');
}
static void post_field(const char *label,const char *value)
{
    uint8_t n=0u;
    console_putc('|');console_putc(' ');
    for(n=0u;label[n]&&n<16u;++n)console_putc(label[n]);
    for(;n<16u;++n)console_putc(' ');
    console_putc(':');console_putc(' ');
    for(n=0u;value[n]&&n<59u;++n)console_putc(value[n]);
    for(;n<59u;++n)console_putc(' ');
    console_putc('|');console_putc('\n');
}

static const char *fpu_name(uint8_t fpu)
{
    if(fpu==BOOT_FPU_EXTERNAL)return "68882 PRESENT";
    if(fpu==BOOT_FPU_INTEGRATED)return "INTEGRATED";
    return "NONE";
}

void post_draw_choices(uint8_t selected)
{
    static const char *items[8]={"F1 SETUP","F2 TOS","F3 HW","F4 BOOT","F5 DIAG","F6 GFX","F10 EXIT","CONTINUE"};
    uint8_t i,r;uint32_t p;
    damage_reset();
    console_clear_text_row(20);console_set_cursor(0,20);
    for(i=0u;i<8u;++i){
        console_putc(' ');
        console_set_inverse((uint8_t)(i==selected));
        console_putc('[');console_puts(items[i]);console_putc(']');
        console_set_inverse(0);
    }
    r=damage_count();p=damage_pixels();
    console_clear_text_row(21);console_set_cursor(0,21);
    console_puts(" REDRAW ");console_put_u32(r);console_puts(" RECT  ");
    console_put_u32(p);console_puts(" PX    FULL SCREEN = 307200 PX");
}

void post_draw_countdown(uint8_t seconds)
{
    console_clear_text_row(22);console_set_cursor(0,22);
    console_puts("AUTO BOOT: ");console_putc((char)('0'+seconds));
    console_puts("   PRESS ANY KEY TO INTERRUPT");
}

void post_run(void)
{
    sb_t s;
    console_clear();
    post_border();
    post_title("ATARI BIOS (C) 2026  ASF  v0.04");
    post_border();

    {sb_t dt;sb_init(&dt);sb_dec(&dt,g_system.datetime.year);sb_str(&dt,"-");
     {char b[4];uint8_t n=0u,d=g_system.datetime.month;if(d>=10u)b[n++]=(char)('0'+d/10u);b[n++]=(char)('0'+d%10u);b[n]='\0';sb_str(&dt,b);}
     sb_str(&dt,"-");
     {char b[4];uint8_t n=0u,d=g_system.datetime.day;if(d>=10u)b[n++]=(char)('0'+d/10u);b[n++]=(char)('0'+d%10u);b[n]='\0';sb_str(&dt,b);}
     sb_str(&dt,"  ");
     {char b[4];uint8_t n=0u,h=g_system.datetime.hour;if(h>=10u)b[n++]=(char)('0'+h/10u);b[n++]=(char)('0'+h%10u);b[n]='\0';sb_str(&dt,b);}
     sb_str(&dt,":");
     {char b[4];uint8_t n=0u,m=g_system.datetime.minute;if(m>=10u)b[n++]=(char)('0'+m/10u);b[n++]=(char)('0'+m%10u);b[n]='\0';sb_str(&dt,b);}
     post_field("DATE / TIME",dt.b);}

    post_field("MAIN PROCESSOR",cpu_type_name(g_system.cpu_type));
    post_field("CO-PROCESSOR",fpu_name(g_system.fpu_type));
    sb_init(&s);
    if(g_system.clock_known){sb_dec(&s,g_system.clock_mhz);sb_str(&s," MHZ");}else sb_str(&s,"UNKNOWN");
    post_field("CPU CLOCK",s.b);
    post_field("CACHE",g_system.cache_enabled?"ENABLED":"DISABLED");
    sb_init(&s);sb_dec(&s,g_system.ram_bytes/1024u);sb_str(&s," KB OK");
    post_field("MEMORY TEST",s.b);
    sb_init(&s);
    if(g_system.tt_ram_bytes){sb_dec(&s,g_system.tt_ram_bytes/1024u);sb_str(&s," KB @ 0x01000000");}
    else sb_str(&s,"NOT PRESENT");
    post_field("ALT / TT RAM",s.b);
    sb_init(&s);sb_str(&s,nova_adapter_name(g_system.nova_adapter));sb_str(&s," ");
    sb_str(&s,nova_chipset_name(g_system.nova_chipset));
    post_field("GRAPHICS ADAPTER",s.b);
    sb_init(&s);sb_str(&s,"SHIFTER ");sb_str(&s,shifter_mode_name((video_mode_t)g_system.video_mode));sb_str(&s," / VGA 640x480");
    post_field("VIDEO MODE",s.b);
    post_field("BLITTER",g_system.blitter_present?(g_system.blitter_enabled?"PRESENT / ENABLED":"PRESENT / DISABLED"):"NOT PRESENT");
    sb_init(&s);
    if(g_system.ide_present){uint8_t u;for(u=0u;u<2u;++u)if(ide_present(u)){char nm[24];sb_str(&s,"IDE");sb_dec(&s,u);sb_str(&s," ");if(ide_model(u,nm,sizeof(nm)))sb_str(&s,nm);else sb_str(&s,"ATA DEVICE");sb_str(&s," ");}}
    else sb_str(&s,"NOT DETECTED");
    post_field("IDE CHANNEL",s.b);
    sb_init(&s);
    if(g_system.acsi_devices){uint8_t u;sb_str(&s,"MASK ");for(u=0u;u<8u;++u)if(g_system.acsi_devices&(uint8_t)(1u<<u)){sb_dec(&s,u);sb_str(&s," ");}}
    else sb_str(&s,"NONE");
    post_field("ASCI DEVICES",s.b);
    post_field("RTC / NVRAM",rtc_type_name(g_system.rtc_type));
    sb_init(&s);
    if(g_config_loaded)sb_str(&s,g_config_location?"LOADED \\BIOS\\BIOS.CFG":"LOADED \\BIOS.CFG");
    else if(g_config_saved)sb_str(&s,g_config_location?"DEFAULTS SAVED \\BIOS\\BIOS.CFG":"DEFAULTS SAVED \\BIOS.CFG");
    else sb_str(&s,"DEFAULTS (READ-ONLY)");
    post_field("CONFIGURATION",s.b);
    post_border();
    post_draw_choices(0);
}
