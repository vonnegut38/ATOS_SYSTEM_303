#include "hardware.h"
#include "system.h"
#include "ide.h"

static int rom_isa_read8(uint16_t port, uint8_t *value)
{
    uint16_t word;
    if (!probe_read16(ROM_ISA_ADDR(port),&word))
        return 0;
    *value=(uint8_t)(word>>8);
    return 1;
}

/* NetUSBee 16-bit writes are encoded as two reads from the cartridge
 * windows. This follows the documented Atari ROM-port ISA wiring. */
static int rom_isa_write16(uint16_t port,uint16_t value)
{
    uint16_t dummy;
    uint32_t address=ROM_ISA_ADDR(port);
    uint32_t low=(address&0xffff0000UL)+(((uint32_t)value&0xffUL)<<1);
    uint32_t high=(address|0x00010000UL)+(((uint32_t)value>>8)<<1);
    return probe_read16(low,&dummy) && probe_read16(high,&dummy);
}

static int detect_rtl8019(void)
{
    uint8_t id0,id1;
    return rom_isa_read8(0x030a,&id0) && rom_isa_read8(0x030b,&id1)
        && id0==0x50u && id1==0x70u;
}

static int detect_isp1160(void)
{
    uint16_t chip_id;
    /* HCCHIPID is read-only; selecting it does not reset or configure USB. */
    if (!rom_isa_write16(0x0360,0x0027u))
        return 0;
    if (!probe_read16(ROM_ISA_ADDR(0x0340),&chip_id))
        return 0;
    return (chip_id&0xff00u)==0x6100u || (chip_id&0x00ffu)==0x0061u;
}

/* A cartridge ROM starts with 0xABCD and carries its program name at 0x14.
 * The window is the same ROM/imager port NetUSBee uses; read via probe so an
 * empty socket just returns "not present" instead of faulting. */
static void cartridge_detect(system_info_t *info)
{
    uint16_t magic;uint8_t i;
    info->cart_rom_present=0u;info->cart_rom_name[0]='\0';
    if(!probe_read16(0xfffa0000UL,&magic)||magic!=0xABCDu)return;
    info->cart_rom_present=1u;
    for(i=0u;i<23u;++i){
        uint8_t c;
        if(!probe_read8(0xfffa0000UL+0x14u+i,&c))break;
        if(c==0u||c==0xffu||c<0x20u||c>0x7eu)break;
        info->cart_rom_name[i]=(char)c;
    }
    info->cart_rom_name[i]='\0';
}

void expansions_detect(system_info_t *info)
{
    nova_detect(info);
    info->netusbee_ethernet=(uint8_t)detect_rtl8019();
    info->netusbee_usb=(uint8_t)detect_isp1160();
    cartridge_detect(info);
    info->ide_present=(uint8_t)ide_detect();
    info->ide_units=ide_units_present();
}
