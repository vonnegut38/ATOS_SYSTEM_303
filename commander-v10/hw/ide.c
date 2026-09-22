#include "hardware.h"
#include "system.h"
#include "ide.h"

/* Falcon/TF536 IDE register window.  Registers are byte-wide at 4-byte
 * spacing; the data register is a 16-bit word at offset 0:
 *   0x00 data       0x05 error/features   0x09 sector count
 *   0x0d LBA0       0x11 LBA1             0x15 LBA2
 *   0x19 drive/head 0x1d command/status   0x39 control/alt status
 * Hatari maps its IDE controller at 0x00F00000 (Falcon form 0xFFF00000 is
 * folded onto the 24-bit bus); the TF536 decodes the same window. */
#define IDE_REG_DATA    0x00UL
#define IDE_REG_ERROR   0x05UL
#define IDE_REG_COUNT   0x09UL
#define IDE_REG_LBA0    0x0dUL
#define IDE_REG_LBA1    0x11UL
#define IDE_REG_LBA2    0x15UL
#define IDE_REG_DRIVE   0x19UL
#define IDE_REG_COMMAND 0x1dUL
#define IDE_REG_ALTSTAT 0x39UL

#define IDE_BASE_TF536  0x00f00000UL
#define IDE_BASE_FALCON 0xfff00000UL

static uint32_t ide_base;
static uint8_t  ide_units;
static uint16_t ide_ident[256];
static char     ide_model_cache[2][24];
static uint8_t  ide_model_cached[2];

static uint8_t  ide_in(uint32_t off){return REG8(ide_base+off);}
static void     ide_out(uint32_t off,uint8_t value){REG8(ide_base+off)=value;}
static uint16_t ide_in16(void){return REG16(ide_base+IDE_REG_DATA);}

/* The status register must be read four times (or ~400 ns) after a command. */
static void ide_delay(void){uint32_t i;for(i=0u;i<8u;++i)(void)ide_in(IDE_REG_ALTSTAT);}

static int ide_region_readable(uint32_t base)
{
    uint8_t value;
    return probe_read8(base+IDE_REG_COMMAND,&value)&&probe_read8(base+IDE_REG_ALTSTAT,&value);
}

/* Reject a random RAM window: require an ATA device and a mostly printable
 * model string (words 27..46). */
static int ide_valid_identify(const uint16_t *w)
{
    uint8_t i,printable=0;
    if(w[0]&0x8000u)return 0;
    for(i=27u;i<47u;++i){uint8_t hi=(uint8_t)(w[i]>>8),lo=(uint8_t)w[i];
        if(hi>=32u&&hi<127u)++printable;
        if(lo>=32u&&lo<127u)++printable;}
    return printable>=8u;
}

static int ide_identify(uint8_t unit)
{
    uint8_t status;uint32_t i;
    ide_out(IDE_REG_DRIVE,(uint8_t)(0xa0u|(unit?0x10u:0u)));
    ide_delay();
    ide_out(IDE_REG_COUNT,0u);ide_out(IDE_REG_LBA0,0u);ide_out(IDE_REG_LBA1,0u);ide_out(IDE_REG_LBA2,0u);
    ide_out(IDE_REG_COMMAND,0xecu);          /* IDENTIFY DEVICE */
    ide_delay();
    status=ide_in(IDE_REG_COMMAND);
    if(status==0x00u||status==0xffu)return 0;
    for(i=0u;i<1000000u;++i){status=ide_in(IDE_REG_COMMAND);if(!(status&0x80u))break;}
    if((status&0x01u)||!(status&0x08u))return 0;
    for(i=0u;i<256u;++i)ide_ident[i]=ide_in16();
    return ide_valid_identify(ide_ident);
}

static void ide_cache_model(uint8_t unit)
{
    uint8_t i,n=0u;
    for(i=0u;i<40u&&n<23u;i+=2u){
        uint8_t hi=(uint8_t)(ide_ident[27u+i/2u]>>8),lo=(uint8_t)ide_ident[27u+i/2u];
        if(hi>=32u&&hi<127u)ide_model_cache[unit][n++]=(char)hi;
        if(lo>=32u&&lo<127u)ide_model_cache[unit][n++]=(char)lo;
    }
    while(n&&ide_model_cache[unit][n-1u]==' ')--n;
    ide_model_cache[unit][n]='\0';
    ide_model_cached[unit]=(uint8_t)(n!=0u);
}

int ide_present(uint8_t unit){return unit<2u&&(ide_units&(uint8_t)(1u<<unit))!=0u;}
uint8_t ide_units_present(void){return ide_units;}
uint32_t ide_base_address(void){return ide_base;}

int ide_detect(void)
{
    uint8_t unit;
    ide_units=0u;ide_base=0u;ide_model_cached[0]=0u;ide_model_cached[1]=0u;
    if(ide_region_readable(IDE_BASE_TF536))ide_base=IDE_BASE_TF536;
    else if(ide_region_readable(IDE_BASE_FALCON))ide_base=IDE_BASE_FALCON;
    else return 0;
    for(unit=0u;unit<2u;++unit)if(ide_identify(unit)){ide_units|=(uint8_t)(1u<<unit);ide_cache_model(unit);}
    if(!ide_units)ide_base=0u;
    return ide_units!=0u;
}

int ide_model(uint8_t unit,char *name,uint8_t capacity)
{
    uint8_t i,n=0u;
    if(!name||capacity<2u)return 0;
    name[0]='\0';
    if(!ide_present(unit)||!ide_model_cached[unit])return 0;
    for(i=0u;ide_model_cache[unit][i]&&n+1u<capacity;++i)name[n++]=ide_model_cache[unit][i];
    name[n]='\0';
    return n!=0u;
}

uint32_t ide_capacity_sectors(uint8_t unit)
{
    uint32_t lba28,lba48;
    if(!ide_present(unit)||!ide_identify(unit))return 0u;
    lba28=((uint32_t)ide_ident[61]<<16)|ide_ident[60];
    lba48=((uint32_t)ide_ident[103]<<16)|ide_ident[102];
    return lba28?lba28:lba48;
}

static int ide_read_chunk(uint8_t unit,uint32_t lba,uint16_t chunk,uint8_t *out)
{
    uint8_t status;uint32_t t,s;uint16_t *p=(uint16_t *)(void *)out;
    if(lba>0x0ffffffful)return 0;
    ide_out(IDE_REG_DRIVE,(uint8_t)(0xe0u|(unit?0x10u:0u)|(uint8_t)((lba>>24)&0x0fu)));
    ide_out(IDE_REG_COUNT,(uint8_t)(chunk==256u?0u:chunk));
    ide_out(IDE_REG_LBA0,(uint8_t)lba);
    ide_out(IDE_REG_LBA1,(uint8_t)(lba>>8));
    ide_out(IDE_REG_LBA2,(uint8_t)(lba>>16));
    ide_out(IDE_REG_COMMAND,0x20u);          /* READ SECTORS (LBA28) */
    for(s=0u;s<chunk;++s){
        for(t=0u;t<1000000u;++t){
            status=ide_in(IDE_REG_COMMAND);
            if(status&0x80u)continue;
            if(status&0x01u)return 0;
            if(status&0x08u)break;
        }
        if(!(status&0x08u))return 0;
        for(t=0u;t<256u;++t)*p++=ide_in16();
    }
    return 1;
}

int ide_read_sectors(uint8_t unit,uint32_t lba,uint16_t count,void *buffer)
{
    uint8_t *out=(uint8_t *)buffer;
    if(!ide_present(unit)||!count)return 0;
    while(count){
        uint16_t chunk=(uint16_t)(count>256u?256u:count);
        if(!ide_read_chunk(unit,lba,chunk,out))return 0;
        lba+=chunk;out+=(uint32_t)chunk*512u;count=(uint16_t)(count-chunk);
    }
    return 1;
}

static int ide_write_chunk(uint8_t unit,uint32_t lba,uint16_t chunk,const uint8_t *in)
{
    uint8_t status;uint32_t t,s;const uint16_t *p=(const uint16_t *)(const void *)in;
    if(lba>0x0ffffffful)return 0;
    ide_out(IDE_REG_DRIVE,(uint8_t)(0xe0u|(unit?0x10u:0u)|(uint8_t)((lba>>24)&0x0fu)));
    ide_out(IDE_REG_COUNT,(uint8_t)(chunk==256u?0u:chunk));
    ide_out(IDE_REG_LBA0,(uint8_t)lba);
    ide_out(IDE_REG_LBA1,(uint8_t)(lba>>8));
    ide_out(IDE_REG_LBA2,(uint8_t)(lba>>16));
    ide_out(IDE_REG_COMMAND,0x30u);          /* WRITE SECTORS (LBA28) */
    for(s=0u;s<chunk;++s){
        for(t=0u;t<1000000u;++t){
            status=ide_in(IDE_REG_COMMAND);
            if(status&0x80u)continue;
            if(status&0x01u)return 0;
            if(status&0x08u)break;
        }
        if(!(status&0x08u))return 0;
        for(t=0u;t<256u;++t)REG16(ide_base+IDE_REG_DATA)=*p++;
    }
    ide_out(IDE_REG_COMMAND,0xe7u);          /* FLUSH CACHE */
    for(t=0u;t<1000000u;++t){status=ide_in(IDE_REG_COMMAND);if(!(status&0x80u))break;}
    return 1;
}

int ide_write_sectors(uint8_t unit,uint32_t lba,uint16_t count,const void *buffer)
{
    const uint8_t *in=(const uint8_t *)buffer;
    if(!ide_present(unit)||!count)return 0;
    while(count){
        uint16_t chunk=(uint16_t)(count>256u?256u:count);
        if(!ide_write_chunk(unit,lba,chunk,in))return 0;
        lba+=chunk;in+=(uint32_t)chunk*512u;count=(uint16_t)(count-chunk);
    }
    return 1;
}
