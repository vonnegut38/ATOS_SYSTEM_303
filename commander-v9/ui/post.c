#include "console.h"
#include "system.h"
#include "diagnostics.h"

system_info_t g_system;
static void put2(uint8_t v){console_putc((char)('0'+v/10u));console_putc((char)('0'+v%10u));}
static void put4(uint16_t v){console_putc((char)('0'+(v/1000u)%10u));console_putc((char)('0'+(v/100u)%10u));console_putc((char)('0'+(v/10u)%10u));console_putc((char)('0'+v%10u));}

void post_draw_choices(uint8_t selected)
{
    console_clear_text_row(12); console_set_cursor(0,12);
    console_set_inverse(selected==0); console_puts(" F1 SETUP ");
    console_set_inverse(0);
    console_clear_text_row(13); console_set_cursor(0,13);
    console_set_inverse(selected==1); console_puts(" F2 DIAGNOSTICS ");
    console_set_inverse(0);
    console_clear_text_row(14); console_set_cursor(0,14);
    console_set_inverse(selected==2); console_puts(" F3 HARDWARE ");
    console_set_inverse(0);
    console_clear_text_row(15); console_set_cursor(0,15);
    console_set_inverse(selected==3); console_puts(" F4 BOOT ");
    console_set_inverse(0);
    console_clear_text_row(16); console_set_cursor(0,16);
    console_set_inverse(selected==4); console_puts(" F5 LOAD TOS ");
    console_set_inverse(0);
    console_clear_text_row(17); console_set_cursor(0,17);
    console_set_inverse(selected==5); console_puts(" F10 SAVE & EXIT ");
    console_set_inverse(0);
    console_clear_text_row(19); console_set_cursor(0,19);
    console_set_inverse(selected==6); console_puts(" CONTINUE BOOT ");
    console_set_inverse(0);
}

void post_draw_countdown(uint8_t seconds)
{
    console_clear_text_row(21);console_set_cursor(0,21);
    console_puts("AUTO BOOT: ");console_putc((char)('0'+seconds));
    console_puts("  PRESS ANY KEY");
}

void post_run(void)
{
    console_clear();
    console_puts("ATARI BIOS 0.03\n\n");
    console_puts("DATE/TIME     ");put2(g_system.datetime.day);console_putc('-');put2(g_system.datetime.month);console_putc('-');put4(g_system.datetime.year);
    console_putc(' ');put2(g_system.datetime.hour);console_putc(':');put2(g_system.datetime.minute);console_putc(':');put2(g_system.datetime.second);console_putc('\n');
    console_puts("COMPUTER      "); console_puts(machine_name(g_system.machine_type)); console_putc('\n');
    console_puts("CPU           MOTOROLA-COMPATIBLE "); console_puts(cpu_type_name(g_system.cpu_type)); console_putc('\n');
    console_puts("CLOCK         ");
    if(g_system.clock_known){console_put_u32(g_system.clock_mhz);console_puts(" MHZ\n");}
    else console_puts("UNKNOWN\n");
    console_puts("CACHE         "); console_puts(g_system.cache_enabled ? "ENABLED\n" : "DISABLED\n");
    console_puts("RAM           "); console_put_u32(g_system.ram_bytes/1024u); console_puts(" KB\n\n");
    console_puts("BLITTER       "); console_puts(g_system.blitter_present ? "PRESENT\n" : "NOT PRESENT\n");
    console_puts("VIDEO         SHIFTER / ");
    console_puts(shifter_mode_name((video_mode_t)g_system.video_mode));
    console_puts("\n\n");
    post_draw_choices(0);
}
