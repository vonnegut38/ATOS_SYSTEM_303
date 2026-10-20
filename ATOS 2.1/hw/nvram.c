#include "hardware.h"
#include "system.h"
#include "config.h"
#include "nvram.h"

/* 16-byte record in the last part of the 50-byte NVRAM, away from the clock
   (0..11) and the common TOS fields.  Magic+CRC make a collision harmless. */
#define NVRAM_BOOT_OFFSET 34u
#define NVRAM_BOOT_BYTES  16u
#define NVRAM_BOOT_MAGIC  0x414e5652UL   /* 'ANVR' */

static uint8_t nvget(uint8_t index){REG8(NVRAM_SELECT)=index;return REG8(NVRAM_DATA);}
static void nvput(uint8_t index,uint8_t value){REG8(NVRAM_SELECT)=index;REG8(NVRAM_DATA)=value;}

int nvram_available(void)
{
    uint8_t dummy;
    if(g_system.rtc_type!=RTC_NVRAM)return 0;   /* MegaSTE/TT/Falcon only */
    return probe_read8(NVRAM_SELECT,&dummy)&&probe_read8(NVRAM_DATA,&dummy);
}

static uint16_t nvram_crc(const uint8_t *p)
{
    uint16_t sum=0u;uint8_t i;
    for(i=0u;i<14u;++i)sum=(uint16_t)(((sum<<1)|(sum>>15))^p[i]);
    return sum;
}

int nvram_load_boot(bios_config_t *cfg)
{
    uint8_t rec[NVRAM_BOOT_BYTES],i;uint32_t magic;uint16_t stored;
    if(!cfg||!nvram_available())return 0;
    for(i=0u;i<NVRAM_BOOT_BYTES;++i)rec[i]=nvget((uint8_t)(NVRAM_BOOT_OFFSET+i));
    magic=((uint32_t)rec[0]<<24)|((uint32_t)rec[1]<<16)|((uint32_t)rec[2]<<8)|rec[3];
    stored=(uint16_t)(((uint16_t)rec[14]<<8)|rec[15]);
    if(magic!=NVRAM_BOOT_MAGIC||nvram_crc(rec)!=stored)return 0;
    cfg->boot_device=rec[4];cfg->boot_id=rec[5];cfg->boot_part=rec[6];cfg->boot_valid=rec[7];
    cfg->boot_delay=rec[8];cfg->auto_tos=rec[9];cfg->video_driver=rec[10];
    cfg->blitter=rec[11];cfg->cpu_clock_mhz=rec[12];
    return 1;
}

int nvram_save_boot(const bios_config_t *cfg)
{
    uint8_t rec[NVRAM_BOOT_BYTES],i;uint16_t crc;
    if(!cfg||!nvram_available())return 0;
    rec[0]=(uint8_t)(NVRAM_BOOT_MAGIC>>24);rec[1]=(uint8_t)(NVRAM_BOOT_MAGIC>>16);
    rec[2]=(uint8_t)(NVRAM_BOOT_MAGIC>>8);rec[3]=(uint8_t)NVRAM_BOOT_MAGIC;
    rec[4]=cfg->boot_device;rec[5]=cfg->boot_id;rec[6]=cfg->boot_part;rec[7]=cfg->boot_valid;
    rec[8]=cfg->boot_delay;rec[9]=cfg->auto_tos;rec[10]=cfg->video_driver;
    rec[11]=cfg->blitter;rec[12]=cfg->cpu_clock_mhz;rec[13]=0u;
    crc=nvram_crc(rec);rec[14]=(uint8_t)(crc>>8);rec[15]=(uint8_t)crc;
    for(i=0u;i<NVRAM_BOOT_BYTES;++i)nvput((uint8_t)(NVRAM_BOOT_OFFSET+i),rec[i]);
    return 1;
}
