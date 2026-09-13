#include "console.h"
#include "diagnostics.h"
#include "rtc.h"
#include "system.h"
#include "ide.h"
#include "damage.h"

static void line(const char *name, const char *value)
{
    console_puts(name);
    console_puts(value);
    console_putc('\n');
}

static void show_mb(uint32_t sectors)
{
    console_put_u32(sectors/2048u);
    console_puts(" MB");
}

/* Storage inventory: IDE (TF536) first, then ACSI.  The scan updates the
 * shared system fields so F4 and the bootloader reuse the same result. */
static void storage_show(void)
{
    char name[25];
    uint8_t id,mask,any=0u;
    console_puts("STORAGE\n");
    for(id=0u;id<2u;++id){
        if(!ide_present(id))continue;
        any=1u;
        console_puts("  IDE UNIT ");
        console_put_u32((uint32_t)id);
        console_puts("  ");
        name[0]='\0';
        if(!ide_model(id,name,sizeof(name)))console_puts("IDE DISK");
        else console_puts(name);
        console_puts("  ");
        show_mb(ide_capacity_sectors(id));
        console_putc('\n');
    }
    mask=acsi_detect_devices();
    g_system.acsi_devices=mask;
    g_system.acsi_scanned=1u;
    for(id=0u;id<8u;++id){
        if(!(mask&(uint8_t)(1u<<id)))continue;
        any=1u;
        console_puts("  ACSI ID ");
        console_put_u32((uint32_t)id);
        console_puts("  ");
        name[0]='\0';
        if(!acsi_inquiry_name(id,name,sizeof(name)))console_puts("ACSI DEVICE");
        else console_puts(name);
        console_putc('\n');
    }
    if(!any)console_puts("  NO DISK DEVICES DETECTED\n");
}

/* Cartridge port: NetUSBee is the expected card.  A USB mass-storage stick
 * would be enumerated by the USB stack and then appear as the last disk. */
static void cartridge_show(void)
{
    console_puts("CARTRIDGE PORT\n");
    console_puts("  NETUSBEE ETH  ");
    console_puts(g_system.netusbee_ethernet ? "RTL8019 PRESENT\n" : "NOT PRESENT\n");
    console_puts("  NETUSBEE USB  ");
    console_puts(g_system.netusbee_usb ? "ISP1160 PRESENT\n" : "NOT PRESENT\n");
    console_puts("  USB DISK      NOT ENUMERATED\n");
    console_puts("  CARTRIDGE ROM ");
    if(g_system.cart_rom_present){console_puts(g_system.cart_rom_name[0]?g_system.cart_rom_name:"PRESENT");}
    else console_puts("NOT PRESENT");
    console_putc('\n');
}

void hardware_inventory_run(void)
{
    int key;

    damage_reset();
    console_clear();
    console_set_cursor(0,0);console_set_inverse(1);
    console_puts("  HARDWARE INVENTORY  ");
    {uint8_t i;for(i=21u;i<80u;++i)console_putc(' ');}
    console_set_inverse(0);
    console_set_cursor(0,1);
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
    cartridge_show();
    storage_show();
    {uint8_t r=damage_count();uint32_t p=damage_pixels();
     console_puts("\n REDRAW: ");console_put_u32(r);console_puts(" RECT  ");
     console_put_u32(p);console_puts(" PX    FULL SCREEN = 307200 PX");}
    console_puts("\nESC/ENTER BACK");

    for (;;) {
        key=ikbd_scancode();
        if (key==0x01 || key==0x1c || key==0x72)
            return;
    }
}
