#include "hardware.h"
#include "system.h"
#include "ide.h"
#include "block_device.h"
#include "fdc.h"

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
        if (!wait_acsi_irq(20000UL)) {
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

int acsi_read_sector(uint8_t id,uint32_t sector,void *buffer)
{
    uint8_t command[6];
    /* Boot floppy: 9 sectors x 2 heads geometry, reached as a block device so
       the loader's FAT reader works unchanged. */
    if(id==BLOCK_FLOPPY_ID){
        uint16_t track;uint8_t side,sec;
        if(sector>0xffffu)return 0;
        track=(uint16_t)(sector/18u);
        side=(uint8_t)((sector/9u)&1u);
        sec=(uint8_t)((sector%9u)+1u);
        return fdc_read_sector(0u,(uint8_t)track,side,sec,buffer);
    }
    /* Phase 4 (v10): id bit 7 selects the IDE transport, so loader/FAT/kernel
       call sites stay transport-agnostic (block_device.h documents the ids). */
    if(id&IDE_ID_FLAG)return ide_read_sectors((uint8_t)(id&0x7fu),sector,1u,buffer);
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

/* Plan item 1 (2026-09-26): real ACSI write.  The register sequence mirrors
   the kernel's proven native ACSI write path (system/kernel_block.c:disk_write):
   0x90 then 0x190 selects the write direction (DMA_WRBIT=0x100), the sector
   count goes through SCREG, command bytes are clocked with 0x18a, and the
   returned status is read back with 0x8a.  IDE keeps its own driver. */
static int acsi_write_command6(uint8_t id,uint8_t *command,uint8_t sectors,
                               uint32_t buffer_address)
{
    uint16_t control;
    uint8_t i,status;

    command[0]|=(uint8_t)(id<<5);
    REG8(DMA_ADDR_HIGH)=(uint8_t)(buffer_address>>16);
    REG8(DMA_ADDR_MID)=(uint8_t)(buffer_address>>8);
    REG8(DMA_ADDR_LOW)=(uint8_t)buffer_address;
    /* Write direction: 0x90 then 0x190 (the reverse of the read start). */
    REG16(DMA_CONTROL)=(uint16_t)(DMA_DRQ_FLOPPY|DMA_SCREG);
    short_delay();
    REG16(DMA_CONTROL)=(uint16_t)(DMA_DRQ_FLOPPY|DMA_SCREG|DMA_WRBIT);
    short_delay();
    REG16(DMA_DATA)=sectors;
    control=(uint16_t)(DMA_DRQ_FLOPPY|DMA_CS_ACSI|DMA_WRBIT);
    REG16(DMA_CONTROL)=control;
    control|=DMA_NOT_NEWCDB;

    for (i=0;i<5u;++i) {
        dma_send(command[i],control);
        if (!wait_acsi_irq(20000UL)) {
            REG16(DMA_CONTROL)=DMA_FLOPPY;
            return -1;
        }
    }
    dma_send(command[5],DMA_WRBIT);
    if (!wait_acsi_irq(400000UL)) {
        REG16(DMA_CONTROL)=DMA_FLOPPY;
        return -1;
    }

    /* Any returned status, including CHECK CONDITION, proves that the ID
     * answered; only status 0 means the write committed. */
    REG16(DMA_CONTROL)=(uint16_t)(control&~DMA_WRBIT);
    status=(uint8_t)REG16(DMA_DATA);
    REG16(DMA_CONTROL)=DMA_FLOPPY;
    return status;
}

int acsi_write_sector(uint8_t id,uint32_t sector,const void *buffer)
{
    uint8_t command[6];
    /* Phase 4 (v10): writes are routed through the same opaque id space. */
    if(id&IDE_ID_FLAG)return ide_write_sectors((uint8_t)(id&0x7fu),sector,1u,buffer);
    if (id>=8u || sector>0x1fffffUL || ((uint32_t)(uintptr_t)buffer&1u))
        return 0;
    command[0]=0x0a;                 /* WRITE(6): one 512-byte sector. */
    command[1]=(uint8_t)(sector>>16);
    command[2]=(uint8_t)(sector>>8);
    command[3]=(uint8_t)sector;
    command[4]=1;
    command[5]=0;
    return acsi_write_command6(id,command,1,(uint32_t)(uintptr_t)buffer)==0;
}

static int acsi_test_unit(uint8_t id)
{
    uint8_t command[6]={0,0,0,0,0,0};
    return acsi_command6(id,command,0,0x00020000UL)>=0;
}

int acsi_probe_device(uint8_t id){return id<8u&&acsi_test_unit(id);}

int acsi_inquiry_name(uint8_t id,char *name,uint8_t capacity)
{
    static uint16_t inquiry_words[256];
    uint8_t command[6]={0x12,0,0,0,36,0};uint8_t *p=(uint8_t *)inquiry_words,i,n=0,start,end;
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

int acsi_inspect_device(system_info_t *info,uint8_t id)
{
    static uint16_t root_words[256];
    uint8_t command[6]={0x08,0,0,0,1,0};
    uint8_t *root=(uint8_t *)root_words;
    uint8_t index;
    uint16_t sum=0;

    info->acsi_root_read=0;
    info->acsi_root_bootable=0;
    info->acsi_partitions=0;
    /* Phase 4 (v10): the inspector works for ACSI and IDE ids; only the
       presence bitmap differs per transport. */
    if (id<8u) { if(!(info->acsi_devices&(uint8_t)(1u<<id))) return 0; }
    else       { if(!(info->ide_units&(uint8_t)(1u<<(uint8_t)(id&0x7fu)))) return 0; }
    (void)command;
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
        info->acsi_part_id[index][0]=(char)entry[1];
        info->acsi_part_id[index][1]=(char)entry[2];
        info->acsi_part_id[index][2]=(char)entry[3];
        info->acsi_part_id[index][3]='\0';
        info->acsi_part_start[index]=read_be32(entry+4);
        if (entry[0]&1u)
            info->acsi_partitions|=(uint8_t)(1u<<index);
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
