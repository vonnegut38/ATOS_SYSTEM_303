#include "console.h"
#include "diagnostics.h"
#include "rtc.h"
#include "system.h"

static void line(const char *name, const char *value)
{
    console_puts(name);
    console_puts(value);
    console_putc('\n');
}

void hardware_inventory_run(void)
{
    int key;

    console_clear();
    console_puts("HARDWARE INVENTORY\n\n");
    line("COMPUTER      ", machine_name(g_system.machine_type));
    line("CPU           ", cpu_type_name(g_system.cpu_type));
    console_puts("CLOCK         ");
    if (g_system.clock_known) { console_put_u32(g_system.clock_mhz); console_puts(" MHZ\n"); }
    else { console_puts("UNKNOWN\n"); }
    console_puts("ST-RAM        "); console_put_u32(g_system.ram_bytes/1024u); console_puts(" KB\n");
    console_puts("TT/FAST-RAM   ");
    if (g_system.tt_ram_bytes) {
        console_put_u32(g_system.tt_ram_bytes/1024u); console_puts(" KB MINIMUM\n");
    } else { console_puts("NOT PRESENT\n"); }
    console_puts("VIDEO         SHIFTER / ");
    console_puts(shifter_mode_name((video_mode_t)g_system.video_mode)); console_putc('\n');
    line("NOVA ADAPTER  ",nova_adapter_name(g_system.nova_adapter));
    line("VGA CHIPSET   ",nova_chipset_name(g_system.nova_chipset));
    line("BLITTER       ", g_system.blitter_present ? "PRESENT" : "NOT PRESENT");
    line("RTC           ", rtc_type_name(g_system.rtc_type));
    line("MFP 68901     ", "PRESENT");
    line("IKBD          ", g_system.keyboard_present ? "PRESENT" : "NOT PRESENT");
    console_puts("\nNETUSBEE ETH  ");
    console_puts(g_system.netusbee_ethernet ? "RTL8019 PRESENT\n" : "NOT PRESENT\n");
    console_puts("NETUSBEE USB  ");
    console_puts(g_system.netusbee_usb ? "ISP1160 PRESENT\n" : "NOT PRESENT\n");
    if(g_system.ide_units){
        console_puts("IDE           PRESENT, UNIT MASK ");
        console_put_u32(g_system.ide_units); console_putc('\n');
    } else { console_puts("IDE           NOT PRESENT\n"); }
    if(g_system.acsi_devices){
        console_puts("ACSI          DEVICE MASK ");
        console_put_u32(g_system.acsi_devices); console_putc('\n');
    } else { console_puts("ACSI          NOT PRESENT\n"); }
    console_puts("STORAGE       USE F4 BOOT\n");
    console_puts("\nESC/ENTER BACK");

    for (;;) {
        key=ikbd_scancode();
        if (key==0x01 || key==0x1c || key==0x72)
            return;
    }
}
