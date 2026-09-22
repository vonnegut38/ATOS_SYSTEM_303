#include "boot_protocol.h"
#include "console.h"
#include "system.h"
#include "block_device.h"

typedef struct {uint8_t id,spc,scale;uint16_t cluster;uint32_t size,fat_start,data_start;} boot_file_t;
static uint16_t io_words[256],fat_words[256];static boot_handoff_t handoff;
static uint8_t active_id,active_part;
static char disk_name[25];
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int same_name(const uint8_t *e,const char name[11]){uint8_t i;for(i=0;i<11u;++i)if(e[i]!=(uint8_t)name[i])return 0;return 1;}
static uint16_t fat_next(uint8_t id,uint32_t fat_start,uint16_t cluster){uint32_t offset=(uint32_t)cluster*2u;uint8_t *p=(uint8_t *)fat_words;if(!acsi_read_sector(id,fat_start+offset/512u,fat_words))return 0xffffu;return le16(p+(uint16_t)(offset&511u));}

static int locate_system_file(uint8_t id,uint32_t part,const char file_name[11],boot_file_t *file)
{
    static const char system_name[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    uint8_t *b=(uint8_t *)io_words,spc,fats,scale;uint16_t bps,reserved,roots,spf,cluster=0,walk,i;uint32_t fat_start,root_start,root_phys,data_start,lba;
    if(!acsi_read_sector(id,part,io_words))return -2;
    bps=le16(b+11);if(bps<512u||bps>8192u||(bps&(bps-1u)))return -2;scale=(uint8_t)(bps/512u);
    spc=b[13];reserved=le16(b+14);fats=b[16];roots=le16(b+17);spf=le16(b+22);if(!spc||!reserved||!fats||!roots||!spf)return -2;
    fat_start=part+(uint32_t)reserved*scale;root_start=fat_start+(uint32_t)fats*spf*scale;root_phys=((uint32_t)roots*32u+bps-1u)/bps*scale;data_start=root_start+root_phys;
    for(lba=root_start;lba<root_start+root_phys&&!cluster;++lba){if(!acsi_read_sector(id,lba,io_words))return -2;for(i=0;i<16u;++i){const uint8_t *e=b+i*32u;if(e[0]==0)break;if(e[0]!=0xe5u&&(e[11]&0x10u)&&same_name(e,system_name))cluster=le16(e+26);}}
    if(cluster<2u)return -1;
    for(walk=0;walk<64u&&cluster>=2u&&cluster<0xfff8u;++walk){uint32_t first=data_start+(uint32_t)(cluster-2u)*spc*scale;
        for(lba=0;lba<(uint32_t)spc*scale;++lba){if(!acsi_read_sector(id,first+lba,io_words))return -2;for(i=0;i<16u;++i){const uint8_t *e=b+i*32u;if(e[0]==0)return 0;if(e[0]!=0xe5u&&!(e[11]&0x18u)&&same_name(e,file_name)){file->id=id;file->spc=spc;file->scale=scale;file->cluster=le16(e+26);file->size=le32(e+28);file->fat_start=fat_start;file->data_start=data_start;return file->cluster>=2u&&file->size?1:-2;}}}
        cluster=fat_next(id,fat_start,cluster);if(cluster==0xffffu)return -2;}
    return 0;
}

static int validate_chain(const boot_file_t *f)
{
    uint32_t cluster_bytes=(uint32_t)f->spc*f->scale*512u,needed=(f->size+cluster_bytes-1u)/cluster_bytes,i;uint16_t cluster=f->cluster,slow=cluster,fast=cluster;
    if(!cluster_bytes||!needed||needed>65520UL)return 0;
    for(i=0;i<needed;++i){if(cluster<2u||cluster>=0xfff8u)return 0;if(i+1u<needed){cluster=fat_next(f->id,f->fat_start,cluster);if(cluster==0xffffu)return 0;}if(slow>=2u&&slow<0xfff8u)slow=fat_next(f->id,f->fat_start,slow);if(fast>=2u&&fast<0xfff8u)fast=fat_next(f->id,f->fat_start,fast);if(fast>=2u&&fast<0xfff8u)fast=fat_next(f->id,f->fat_start,fast);if(i>0u&&slow>=2u&&slow<0xfff8u&&slow==fast)return 0;}
    return 1;
}

static int load_image(const boot_file_t *f,const char magic[8],uint16_t header_version,uint32_t load,uint32_t header_load,uint32_t *entry)
{
    uint32_t rounded=(f->size+511u)&~511UL,remaining=f->size,sector=0,sum=0,i,memory_size;uint16_t cluster=f->cluster;uint8_t *dst=(uint8_t *)(uintptr_t)load,*src=(uint8_t *)io_words;uint32_t cluster_sectors=(uint32_t)f->spc*f->scale;
    if(f->size<BOOT_IMAGE_HEADER_SIZE||f->size>512UL*1024UL||rounded<f->size||load+rounded>g_system.ram_bytes-8192u)return 0;
    if(!validate_chain(f))return 0;
    while(remaining){uint32_t lba=f->data_start+(uint32_t)(cluster-2u)*cluster_sectors+sector;uint16_t amount=remaining<512u?(uint16_t)remaining:512u,copy;if(!acsi_read_sector(f->id,lba,io_words))return 0;for(copy=0;copy<amount;++copy)*dst++=src[copy];remaining-=amount;if(++sector>=cluster_sectors){sector=0;if(remaining){cluster=fat_next(f->id,f->fat_start,cluster);if(cluster<2u||cluster>=0xfff8u)return 0;}}}
    system_image_cache_sync(g_system.cpu_type);
    dst=(uint8_t *)(uintptr_t)load;for(i=0;i<8u;++i)if(dst[i]!=(uint8_t)magic[i])return 0;
    if(be16(dst+8)!=header_version||be16(dst+10)!=BOOT_IMAGE_HEADER_SIZE||be32(dst+12)!=f->size||be32(dst+16)!=header_load)return 0;
    memory_size=be32(dst+24);if(memory_size<f->size||load+memory_size<load||load+memory_size>g_system.ram_bytes-8192u)return 0;
    *entry=load+be32(dst+20);if(*entry<load+BOOT_IMAGE_HEADER_SIZE||*entry>=load+f->size)return 0;
    for(i=0;i<f->size;++i)if(i<28u||i>=32u)sum+=dst[i];
    return sum==be32(dst+28);
}

/* --- Phase 1 (v10): readable KERNEL.SYS load diagnostics ------------------- */
typedef struct {uint32_t load,entry,size;uint16_t version,header_size;} kernel_header_t;

static void put_hex32(uint32_t value)
{
    static const char hx[]="0123456789ABCDEF";int i;
    for(i=7;i>=0;--i){char c[2];c[0]=hx[(value>>(i*4))&15u];c[1]=0;console_puts(c);}
}

/* Read the 32-byte boot image header from the first sector of the file. */
static int read_kernel_header(const boot_file_t *f,kernel_header_t *h)
{
    uint8_t *b=(uint8_t *)io_words;
    uint32_t lba=f->data_start+(uint32_t)(f->cluster-2u)*(uint32_t)f->spc*f->scale;
    if(!acsi_read_sector(f->id,lba,io_words))return 0;
    h->version=be16(b+8);h->header_size=be16(b+10);
    h->size=be32(b+12);h->load=be32(b+16);h->entry=h->load+be32(b+20);
    return 1;
}

static void start_kernel(void)
{
    static const char kernel_name[11]={'K','E','R','N','E','L',' ',' ','S','Y','S'};
    static const char kernel_magic[8]={'A','S','Y','S','K','E','R','N'};
    boot_file_t file;uint32_t entry;kernel_header_t hdr;
    console_puts("\nSEARCHING KERNEL.SYS...");
    if(locate_system_file(active_id,g_system.acsi_part_start[active_part],kernel_name,&file)!=1){
        console_puts("\nKERNEL.SYS NOT FOUND\n");return;
    }
    if(!read_kernel_header(&file,&hdr)){console_puts("\nKERNEL HEADER READ FAILED\n");return;}
    console_puts("\nKERNEL.SYS\nLOAD: ");put_hex32(hdr.load);
    console_puts("\nENTRY: ");put_hex32(hdr.entry);
    console_puts("\nSIZE: ");console_put_u32(hdr.size);
    if(hdr.load!=KERNEL_IMAGE_LOAD_ADDRESS){
        console_puts("\nEXPECTED LOAD ADDRESS: ");put_hex32(KERNEL_IMAGE_LOAD_ADDRESS);
        console_puts("\nACTUAL LOAD ADDRESS:   ");put_hex32(hdr.load);
        console_puts("\nSTATUS: KERNEL LOAD ADDRESS MISMATCH\n");return;
    }
    console_puts("\nSTATUS: OK\n");
    if(load_image(&file,kernel_magic,BOOT_IMAGE_VERSION,KERNEL_IMAGE_LOAD_ADDRESS,KERNEL_IMAGE_LOAD_ADDRESS,&entry)){
        console_puts("STARTING KERNEL.SYS...\n");
        ((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);
    } else {
        console_puts("KERNEL IMAGE INVALID (magic/size/checksum/ABI)\n");
    }
}

static int load_module_service(const char name[11],void *destination,uint32_t capacity,module_image_info_t *info)
{
    static const char module_magic[8]={'A','S','Y','S','M','O','D','0'};
    boot_file_t file;uint32_t first,entry,memory_size;uint8_t *h=(uint8_t *)io_words;int found;
    if(!name||!info)return 0;
    found=locate_system_file(active_id,g_system.acsi_part_start[active_part],name,&file);if(found!=1)return 0;
    first=file.data_start+(uint32_t)(file.cluster-2u)*file.spc*file.scale;
    if(!acsi_read_sector(file.id,first,io_words))return 0;
    if(file.size<BOOT_IMAGE_HEADER_SIZE)return 0;
    {uint8_t i;for(i=0;i<8u;++i)if(h[i]!=(uint8_t)module_magic[i])return 0;}
    if(be16(h+8)!=MODULE_ABI_VERSION||be16(h+10)!=BOOT_IMAGE_HEADER_SIZE||be32(h+12)!=file.size||be32(h+16)!=0)return 0;
    memory_size=be32(h+24);entry=be32(h+20);if(memory_size<file.size||entry<BOOT_IMAGE_HEADER_SIZE||entry>=file.size)return 0;
    info->file_size=file.size;info->memory_size=memory_size;info->entry_offset=entry;
    if(!destination)return 1;
    if(capacity<memory_size||((uint32_t)(uintptr_t)destination&15u))return 0;
    if(load_image(&file,module_magic,MODULE_ABI_VERSION,(uint32_t)(uintptr_t)destination,0,&entry)){uint8_t *p=(uint8_t *)destination;uint32_t i;for(i=file.size;i<memory_size;++i)p[i]=0;system_image_cache_sync(g_system.cpu_type);return 1;}return 0;
}

static void wait_return(void){int key;console_puts("\n\nESC/ENTER RETURN");for(;;){key=ikbd_scancode();if(key==0x01||key==0x1c||key==0x72)return;}}
void boot_stage2_run(void)
{
    static const char boot_name[11]={'B','O','O','T',' ',' ',' ',' ','S','Y','S'};
    static const char boot_magic[8]={'A','S','Y','S','B','O','O','T'};
    boot_file_t file;uint32_t entry;uint8_t id,part;int result;console_clear();console_puts("ATARI SYSTEM BOOT\n\nSCANNING BOOT DEVICE...");
    if(!g_system.boot_selection_valid){g_system.acsi_devices=0;g_system.acsi_scanned=1;{uint8_t k,found=0;for(k=0u;k<10u&&!found;++k){uint8_t cand=(k<8u)?(uint8_t)k:(uint8_t)(0x80u|(k-8u));console_puts((cand&0x80u)?"\nPROBING IDE UNIT ":"\nPROBING ACSI ID ");console_put_u32((uint32_t)(cand&0x7fu));if(block_probe(cand)){id=cand;found=1;if(cand&0x80u)g_system.ide_units=(uint8_t)(g_system.ide_units|(uint8_t)(1u<<(cand&0x7fu)));else g_system.acsi_devices=(uint8_t)(1u<<cand);}}if(!found){console_puts("\n\nBOOT DISK NOT FOUND");wait_return();return;}}console_puts((id&0x80u)?"\nFOUND IDE UNIT ":"\nFOUND ACSI ID ");console_put_u32((uint32_t)(id&0x7fu));console_puts("\nDISK: ");if(acsi_inquiry_name(id,disk_name,sizeof(disk_name)))console_puts(disk_name);else console_puts("HARD DISK");if(!acsi_inspect_device(&g_system,id)){console_puts("\n\nBOOT DISK READ ERROR");wait_return();return;}for(part=0;part<4u;++part)if(g_system.acsi_partitions&(uint8_t)(1u<<part))break;if(part>=4u){console_puts("\n\nBOOT PARTITION NOT FOUND");wait_return();return;}}
    else{id=g_system.boot_acsi_id;part=g_system.boot_partition;if(!acsi_inspect_device(&g_system,id)){console_puts("\n\nBOOT DISK READ ERROR");wait_return();return;}}
    console_puts("\nPARTITION: ");console_puts(g_system.acsi_part_id[part]);console_puts(" ");console_put_u32(part+1u);console_puts("\nSEARCHING SYSTEM/BOOT.SYS...");
    handoff.fpu_type=g_system.fpu_type;handoff.clock_mhz=g_system.clock_mhz;
    handoff.clock_known=g_system.clock_known;
    handoff.blitter_present=g_system.blitter_present;
    handoff.blitter_enabled=g_system.blitter_enabled;
    result=locate_system_file(id,g_system.acsi_part_start[part],boot_name,&file);
    if(result==1&&load_image(&file,boot_magic,BOOT_IMAGE_VERSION,0x00020000UL,0x00020000UL,&entry)){console_puts("\nSTARTING BOOT.SYS...");active_id=id;active_part=part;handoff.magic=BOOT_HANDOFF_MAGIC;handoff.version=BOOT_HANDOFF_VERSION;handoff.size=sizeof(handoff);handoff.machine_type=g_system.machine_type;handoff.cpu_type=g_system.cpu_type;handoff.fpu_type=g_system.fpu_type;handoff.clock_mhz=g_system.clock_mhz;handoff.clock_known=g_system.clock_known;handoff.reserved0=(uint8_t)(0x80u|g_system.nova_adapter);handoff.reserved1=g_system.nova_chipset;handoff.video_mode=g_system.video_mode;handoff.boot_acsi_id=id;handoff.boot_partition=part;handoff.blitter_present=g_system.blitter_present;handoff.blitter_enabled=g_system.blitter_enabled;handoff.st_ram_bytes=g_system.ram_bytes;handoff.tt_ram_bytes=g_system.tt_ram_bytes;handoff.console_clear=console_clear;handoff.console_puts=console_puts;handoff.console_put_u32=console_put_u32;handoff.start_kernel=start_kernel;handoff.load_module=load_module_service;handoff.acsi_read_sector=acsi_read_sector;((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);console_clear();console_puts("ATARI SYSTEM BOOT\n\nBOOT.SYS RETURNED UNEXPECTEDLY");wait_return();return;}
    console_clear();console_puts("ATARI SYSTEM BOOT\n\n");if(result==-2)console_puts("BOOT FILESYSTEM ERROR");else if(result<=0)console_puts("SYSTEM/BOOT.SYS NOT FOUND");else console_puts("BOOT.SYS INVALID OR LOAD FAILED");wait_return();
}
