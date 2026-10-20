#include "block_device.h"
#include "hardware.h"
#include "system.h"
#include "ide.h"

/* ATA/FAT sector size.  Kept local so this layer does not depend on the
   kernel block cache headers. */
#define BLOCK_SECTOR_BYTES 512u

int block_probe(uint8_t id)
{
    if(BLOCK_IS_IDE(id))return ide_present(BLOCK_UNIT(id));
    if(id>=8u)return 0;
    return acsi_probe_device(id);
}

int block_read(uint8_t id,uint32_t lba,uint16_t count,void *buffer)
{
    if(!count)return 1;
    if(BLOCK_IS_IDE(id))return ide_read_sectors(BLOCK_UNIT(id),lba,count,buffer);
    if(id>=8u)return 0;
    /* ACSI reads one sector per command. */
    {uint8_t *p=(uint8_t *)buffer;uint16_t i;
     for(i=0u;i<count;++i){if(!acsi_read_sector(id,lba+i,p))return 0;p+=BLOCK_SECTOR_BYTES;}
     return 1;}
}

int block_write(uint8_t id,uint32_t lba,uint16_t count,const void *buffer)
{
    if(!count)return 1;
    if(BLOCK_IS_IDE(id))return ide_write_sectors(BLOCK_UNIT(id),lba,count,buffer);
    if(id>=8u)return 0;
    {const uint8_t *p=(const uint8_t *)buffer;uint16_t i;
     for(i=0u;i<count;++i){if(!acsi_write_sector(id,lba+i,p))return 0;p+=BLOCK_SECTOR_BYTES;}
     return 1;}
}

int block_flush(uint8_t id)
{
    (void)id;
    return BLOCK_IS_IDE(id)?(ide_present(BLOCK_UNIT(id))?1:0):1;
}

int block_get_info(uint8_t id,block_device_info_t *info)
{
    if(!info)return 0;
    info->id=id;info->is_ide=(uint8_t)BLOCK_IS_IDE(id);
    info->present=(uint8_t)block_probe(id);
    info->model[0]='\0';info->sectors=0u;
    if(!info->present)return 0;
    if(info->is_ide){
        (void)ide_model(BLOCK_UNIT(id),info->model,sizeof(info->model));
        info->sectors=ide_capacity_sectors(BLOCK_UNIT(id));
    } else {
        (void)acsi_inquiry_name(id,info->model,sizeof(info->model));
    }
    return 1;
}

uint8_t block_detect_acsi(void)
{
    uint8_t id,mask=0u;
    for(id=0u;id<8u;++id)if(acsi_probe_device(id))mask=(uint8_t)(mask|(uint8_t)(1u<<id));
    return mask;
}
