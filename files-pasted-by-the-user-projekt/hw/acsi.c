#include "hardware.h"
#include "system.h"
#include "ide.h"

enum {
    DMA_NOT_NEWCDB=0x0002,
    DMA_CS_ACSI=0x0008,
    DMA_SCREG=0x0010,
    DMA_DRQ_FLOPPY=0x0080,
    DMA_WRBIT=0x0100,
    DMA_FLOPPY=0x0080
};

static void short_delay(void)
{
    volatile uint16_t count;
    for (count=0;count<96u;++count)
        __asm__ volatile("nop");
}

static int wait_acsi_irq(uint32_t limit)
{
    /* Existing hardware asserts GPIP almost immediately for each command
     * byte. Keep enough margin for slow converters, without multiplying a
     * multi-second delay by seven absent IDs. */
    volatile uint32_t timeout=limit;
    while (timeout--) {
        if ((REG8(MFP_GPIP)&0x20u)==0)
            return 1;
    }
    return 0;
}

static void dma_start(uint16_t control)
{
    control|=DMA_SCREG;
    REG16(DMA_CONTROL)=(uint16_t)(control^DMA_WRBIT);
    short_delay();
    REG16(DMA_CONTROL)=control;
    short_delay();
}

static void dma_send(uint8_t data,uint16_t next_control)
{
    /* Separate word accesses avoid the later STE DMA data-corruption bug. */
    REG16(DMA_DATA)=data;
    __asm__ volatile("nop");
    REG16(DMA_CONTROL)=next_control;
}

static int acsi_command6(uint8_t id,uint8_t *command,uint8_t sectors,
                         uint32_t buffer_address)
{
    uint16_t control=DMA_DRQ_FLOPPY;
    uint8_t i,status;

    command[0]|=(uint8_t)(id<<5);
    REG8(DMA_ADDR_HIGH)=(uint8_t)(buffer_address>>16);
    REG8(DMA_ADDR_MID)=(uint8_t)(buffer_address>>8);
    REG8(DMA_ADDR_LOW)=(uint8_t)buffer_address;
    dma_start(control);
    REG16(DMA_DATA)=sectors;
    control|=DMA_CS_ACSI;
    REG16(DMA_CONTROL)=control;
    control|=DMA_NOT_NEWCDB;

    for (i=0;i<5u;++i) {
        dma_send(command[i],control);
        if (!wait_acsi_irq(50000UL)) {
            REG16(DMA_CONTROL)=DMA_FLOPPY;
            return -1;
        }
    }
    dma_send(command[5],(uint16_t)(control&0xff00u));
    if (!wait_acsi_irq(sectors?400000UL:50000UL)) {
        REG16(DMA_CONTROL)=DMA_FLOPPY;
        return -1;
    }

    /* Any returned status, including CHECK CONDITION, proves that the ID
     * answered. The status value itself is not interpreted during discovery. */
    REG16(DMA_CONTROL)=(uint16_t)(control&~DMA_WRBIT);
    status=(uint8_t)REG16(DMA_DATA);
    REG16(DMA_CONTROL)=DMA_FLOPPY;
    return status;
}

static int acsi_read_sector_impl(uint8_t id,uint32_t sector,void *buffer)
{
    uint8_t command[6];
    if (id>=8u || sector>0x1fffffUL || ((uint32_t)(uintptr_t)buffer&1u))
        return 0;
    command[0]=0x08;
    command[1]=(uint8_t)(sector>>16);
    command[2]=(uint8_t)(sector>>8);
    command[3]=(uint8_t)sector;
    command[4]=1;
    command[5]=0;
    return acsi_command6(id,command,1,(uint32_t)(uintptr_t)buffer)==0;
}

/* One read entry point for both transports: bit 7 of the id selects IDE. */
int acsi_read_sector(uint8_t id,uint32_t sector,void *buffer)
{
    if(id&IDE_ID_FLAG)return ide_read_sectors((uint8_t)(id&0x7fu),sector,1u,buffer);
    return acsi_read_sector_impl(id,sector,buffer);
}

/* Writes are currently provided by the IDE transport only; ACSI write is a
 * follow-up (the FDC/ACSI DMA write sequence needs its own validation). */
int acsi_write_sector(uint8_t id,uint32_t sector,const void *buffer)
{
    if(id&IDE_ID_FLAG)return ide_write_sectors((uint8_t)(id&0x7fu),sector,1u,buffer);
    return 0;
}

static int acsi_test_unit(uint8_t id)
{
    uint8_t command[6]={0,0,0,0,0,0};
    return acsi_command6(id,command,0,0x00020000UL)>=0;
}

int acsi_probe_device(uint8_t id)
{
    if(id&IDE_ID_FLAG)return ide_present((uint8_t)(id&0x7fu));
    return id<8u&&acsi_test_unit(id);
}

int acsi_inquiry_name(uint8_t id,char *name,uint8_t capacity)
{
    static uint16_t inquiry_words[256];
    uint8_t command[6]={0x12,0,0,0,36,0};uint8_t *p=(uint8_t *)inquiry_words,i,n=0,start,end;
    if(id&IDE_ID_FLAG)return ide_model((uint8_t)(id&0x7fu),name,capacity);
    if(!name||capacity<2u||id>=8u)return 0;
    name[0]='\0';
    if(acsi_command6(id,command,1,(uint32_t)(uintptr_t)inquiry_words)!=0)return 0;
    for(start=8u;start<32u&&(p[start]==' '||p[start]==0);++start){}
    for(end=32u;end>start&&(p[end-1u]==' '||p[end-1u]==0);--end){}
    for(i=start;i<end&&n+1u<capacity;++i){uint8_t c=p[i];if(c<32u||c>126u)return 0;name[n++]=(char)c;}
    name[n]='\0';return n!=0u;
}

uint8_t acsi_detect_devices(void)
{
    uint8_t id,mask=0;
    for (id=0;id<8u;++id) {
        if (acsi_test_unit(id))
            mask|=(uint8_t)(1u<<id);
        /* Atari recommends at least 5 ms between independent ACSI I/Os. */
        {
            volatile uint16_t gap;
            for (gap=0;gap<2500u;++gap)
                __asm__ volatile("nop");
        }
    }
    return mask;
}

uint8_t acsi_detect_first_device(void)
{
    uint8_t id;
    for(id=0;id<8u;++id){
        if(acsi_test_unit(id))return (uint8_t)(1u<<id);
        {
            volatile uint16_t gap;
            for(gap=0;gap<2500u;++gap)__asm__ volatile("nop");
        }
    }
    return 0;
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static int entry_id_is(const uint8_t *e,char a,char b,char c)
{return e[1]==(uint8_t)a&&e[2]==(uint8_t)b&&e[3]==(uint8_t)c;}

static void add_partition(system_info_t *info,const uint8_t *entry,uint32_t base)
{
    uint32_t relative,start;
    if(!(entry[0]&1u))return;
    relative=read_be32(entry+4);
    if(!relative)return;
    if(info->acsi_part_count>=16u)return;
    start=base+relative;
    info->acsi_part_id[info->acsi_part_count][0]=(char)entry[1];
    info->acsi_part_id[info->acsi_part_count][1]=(char)entry[2];
    info->acsi_part_id[info->acsi_part_count][2]=(char)entry[3];
    info->acsi_part_id[info->acsi_part_count][3]='\0';
    info->acsi_part_start[info->acsi_part_count]=start;
    info->acsi_partitions|=(uint32_t)(1u<<info->acsi_part_count);
    ++info->acsi_part_count;
}

int acsi_inspect_device(system_info_t *info,uint8_t id)
{
    static uint16_t root_words[256];
    static uint16_t desc_words[256];
    uint8_t *root=(uint8_t *)root_words;
    uint8_t index;
    uint16_t sum=0;

    info->acsi_root_read=0;
    info->acsi_root_bootable=0;
    info->acsi_partitions=0;
    info->acsi_part_count=0;
    if (id&IDE_ID_FLAG) {
        if (!ide_present((uint8_t)(id&0x7fu)))
            return 0;
    } else if (id>=8u || !(info->acsi_devices&(uint8_t)(1u<<id))) {
        return 0;
    }
    if (!acsi_read_sector(id,0,root_words))
        return 0;

    info->acsi_root_read=1;
    info->acsi_root_id=id;
    for (index=0;index<4u;++index) {
        uint16_t word_index;
        for (word_index=(uint16_t)(index*64u);word_index<(uint16_t)((index+1u)*64u);++word_index)
            sum=(uint16_t)(sum+root_words[word_index]);
    }
    info->acsi_root_bootable=(uint8_t)(sum==0x1234u);

    for (index=0;index<4u;++index) {
        const uint8_t *entry=root+0x1c6u+(uint16_t)index*12u;
        if(!(entry[0]&1u))continue;
        if(entry_id_is(entry,'X','G','M')){
            /* Extended partition chain: each descriptor sector holds one
             * logical volume plus a link to the next descriptor. */
            uint32_t descriptor=read_be32(entry+4);
            uint8_t hops=0u;
            while(descriptor&&hops<16u){
                uint8_t *d=(uint8_t *)desc_words;
                const uint8_t *link;
                uint32_t relative;
                if(!acsi_read_sector(id,descriptor,desc_words))break;
                add_partition(info,d+0x1c6u,descriptor);
                link=d+0x1c6u+12u;
                if(!(link[0]&1u)||!entry_id_is(link,'X','G','M'))break;
                relative=read_be32(link+4);
                if(!relative)break;
                descriptor=descriptor+relative;
                ++hops;
            }
        } else {
            add_partition(info,entry,0u);
        }
    }
    return 1;
}

int acsi_inspect_first(system_info_t *info)
{
    uint8_t id;
    for (id=0;id<8u;++id)
        if (info->acsi_devices&(uint8_t)(1u<<id))
            return acsi_inspect_device(info,id);
    return 0;
}
