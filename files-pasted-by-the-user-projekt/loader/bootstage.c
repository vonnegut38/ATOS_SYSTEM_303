#include "boot_protocol.h"
#include "console.h"
#include "system.h"
#include "ide.h"
#include "config.h"
#include "video8.h"

typedef struct {uint8_t id,spc,scale;uint32_t cluster;uint32_t size,fat_start,data_start;} boot_file_t;
static uint16_t io_words[256],fat_words[256];static boot_handoff_t handoff;
static uint8_t active_id,active_part;
static char disk_name[25];
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int same_name(const uint8_t *e,const char name[11]){uint8_t i;for(i=0;i<11u;++i)if(e[i]!=(uint8_t)name[i])return 0;return 1;}

/* --- mounted FAT16/FAT32 geometry --- */
static uint8_t  geo_id, geo_spc, geo_scale, geo_valid, geo_fat32;
static uint32_t geo_fat_start, geo_root_start, geo_data_start, geo_root_cluster;
static uint16_t geo_root_entries;

#define FAT_BAD 0xFFFFFFFFu
static int cluster_end(uint32_t c){return geo_fat32?(c>=0x0FFFFFF8u):(c>=0xFFF8u);}

static uint32_t fat_next(uint8_t id,uint32_t fat_start,uint32_t cluster)
{
    uint8_t *p=(uint8_t *)fat_words;uint32_t offset=(geo_fat32?cluster*4u:cluster*2u),v;
    if(!acsi_read_sector(id,fat_start+offset/512u,fat_words))return FAT_BAD;
    offset&=511u;
    if(!geo_fat32)return (uint32_t)(p[offset]|(p[offset+1u]<<8));
    v=(uint32_t)p[offset]|((uint32_t)p[offset+1u]<<8)|((uint32_t)p[offset+2u]<<16)|((uint32_t)p[offset+3u]<<24);
    return v&0x0FFFFFFFu;
}

static int mount_geometry(uint8_t id,uint32_t part)
{
    uint8_t *b=(uint8_t *)io_words,spc,fats,scale;uint16_t bps,reserved,roots,spf16;uint32_t spf32,root_phys,spf;
    geo_valid=0u;geo_fat32=0u;geo_root_cluster=0u;
    if(!acsi_read_sector(id,part,io_words))return -2;
    bps=le16(b+11);if(bps<512u||bps>8192u||(bps&(bps-1u)))return -2;
    scale=(uint8_t)(bps/512u);
    spc=b[13];reserved=le16(b+14);fats=b[16];roots=le16(b+17);spf16=le16(b+22);spf32=le32(b+36);
    if(!spc||!reserved||!fats||(fats!=1u&&fats!=2u))return -2;
    if(spf16){spf=spf16;if(!roots)return -2;}
    else if(spf32){spf=spf32;geo_fat32=1u;geo_root_cluster=le32(b+44);if(geo_root_cluster<2u)return -2;}
    else return -2;
    geo_id=id;geo_spc=spc;geo_scale=scale;geo_root_entries=roots;
    geo_fat_start=part+(uint32_t)reserved*scale;
    geo_root_start=geo_fat_start+(uint32_t)fats*spf*scale;
    if(geo_fat32){geo_data_start=geo_root_start;}
    else{root_phys=(((uint32_t)roots*32u+bps-1u)/bps)*scale;geo_data_start=geo_root_start+root_phys;}
    geo_valid=1u;
    return 0;
}

/* Find one 8.3 entry in directory dir_cluster (0 = root). */
static int dir_find(uint32_t dir_cluster,const char name[11],uint8_t want_dir,uint32_t *out_cluster,uint32_t *out_size)
{
    uint8_t *b=(uint8_t *)io_words;uint16_t i,walk;
    if(!geo_valid)return 0;
    if(dir_cluster<2u){
        if(geo_fat32)dir_cluster=geo_root_cluster;
        else{
            uint32_t root_sectors=((uint32_t)geo_root_entries*32u+511u)/512u,lba;
            for(lba=0u;lba<root_sectors;++lba){
                if(!acsi_read_sector(geo_id,geo_root_start+lba,io_words))return 0;
                for(i=0u;i<16u;++i){const uint8_t *e=b+(uint32_t)i*32u;
                    if(e[0]==0u)return 0;
                    if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu)continue;
                    if(!same_name(e,name))continue;
                    if(want_dir){if(!(e[11]&0x10u))continue;}else{if(e[11]&0x18u)continue;}
                    if(out_cluster)*out_cluster=le16(e+26);
                    if(out_size)*out_size=le32(e+28);
                    return 1;}
            }
            return 0;
        }
    }
    for(walk=0u;walk<64u&&dir_cluster>=2u&&!cluster_end(dir_cluster);++walk){
        uint32_t base=geo_data_start+((uint32_t)dir_cluster-2u)*geo_spc*geo_scale,lba;
        for(lba=0u;lba<(uint32_t)geo_spc*geo_scale;++lba){
            if(!acsi_read_sector(geo_id,base+lba,io_words))return 0;
            for(i=0u;i<16u;++i){const uint8_t *e=b+(uint32_t)i*32u;
                if(e[0]==0u)return 0;
                if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu)continue;
                if(!same_name(e,name))continue;
                if(want_dir){if(!(e[11]&0x10u))continue;}else{if(e[11]&0x18u)continue;}
                if(out_cluster)*out_cluster=(geo_fat32?(((uint32_t)le16(e+20)<<16)|le16(e+26)):le16(e+26));
                if(out_size)*out_size=le32(e+28);
                return 1;}
        }
        dir_cluster=fat_next(geo_id,geo_fat_start,dir_cluster);if(dir_cluster==FAT_BAD)return 0;
    }
    return 0;
}

static int path_lookup(const char dirs[][11],uint8_t ndirs,const char file[11],boot_file_t *out)
{
    uint32_t cluster=0u,next=0u,size=0u;uint8_t i;
    if(!geo_valid)return -2;
    for(i=0u;i<ndirs;++i){
        if(!dir_find(cluster,dirs[i],1u,&next,0))return -1;
        cluster=next;
    }
    if(!dir_find(cluster,file,0u,&next,&size))return -1;
    if(next<2u||!size)return -2;
    out->id=geo_id;out->spc=geo_spc;out->scale=geo_scale;
    out->cluster=next;out->size=size;
    out->fat_start=geo_fat_start;out->data_start=geo_data_start;
    return 1;
}

static int validate_chain(const boot_file_t *f)
{
    uint32_t cluster_bytes=(uint32_t)f->spc*f->scale*512u,needed=(f->size+cluster_bytes-1u)/cluster_bytes,i;uint32_t cluster=f->cluster,slow=cluster,fast=cluster;
    if(!cluster_bytes||!needed||needed>1048576UL)return 0;
    for(i=0;i<needed;++i){if(cluster<2u||cluster_end(cluster))return 0;if(i+1u<needed){cluster=fat_next(f->id,f->fat_start,cluster);if(cluster==FAT_BAD)return 0;}if(slow>=2u&&!cluster_end(slow))slow=fat_next(f->id,f->fat_start,slow);if(fast>=2u&&!cluster_end(fast))fast=fat_next(f->id,f->fat_start,fast);if(fast>=2u&&!cluster_end(fast))fast=fat_next(f->id,f->fat_start,fast);if(i>0u&&slow>=2u&&!cluster_end(slow)&&slow==fast)return 0;}
    return 1;
}

static int load_image(const boot_file_t *f,const char magic[8],uint16_t header_version,uint32_t load,uint32_t header_load,uint32_t *entry)
{
    uint32_t rounded=(f->size+511u)&~511UL,remaining=f->size,sector=0,sum=0,i,memory_size,cluster=f->cluster;uint8_t *dst=(uint8_t *)(uintptr_t)load,*src=(uint8_t *)io_words;uint32_t cluster_sectors=(uint32_t)f->spc*f->scale;
    if(f->size<BOOT_IMAGE_HEADER_SIZE||f->size>1024UL*1024UL||rounded<f->size||load+rounded>g_system.ram_bytes-8192u)return 0;
    if(!validate_chain(f))return 0;
    while(remaining){uint32_t lba=f->data_start+((uint32_t)cluster-2u)*cluster_sectors+sector;uint16_t amount=remaining<512u?(uint16_t)remaining:512u,copy;if(!acsi_read_sector(f->id,lba,io_words))return 0;for(copy=0;copy<amount;++copy)*dst++=src[copy];remaining-=amount;if(++sector>=cluster_sectors){sector=0;if(remaining){cluster=fat_next(f->id,f->fat_start,cluster);if(cluster<2u||cluster==FAT_BAD||cluster_end(cluster))return 0;}}}
    system_image_cache_sync(g_system.cpu_type);
    dst=(uint8_t *)(uintptr_t)load;for(i=0;i<8u;++i)if(dst[i]!=(uint8_t)magic[i])return 0;
    if(be16(dst+8)!=header_version||be16(dst+10)!=BOOT_IMAGE_HEADER_SIZE||be32(dst+12)!=f->size||be32(dst+16)!=header_load)return 0;
    memory_size=be32(dst+24);if(memory_size<f->size||load+memory_size<load||load+memory_size>g_system.ram_bytes-8192u)return 0;
    *entry=load+be32(dst+20);if(*entry<load+BOOT_IMAGE_HEADER_SIZE||*entry>=load+f->size)return 0;
    for(i=0;i<f->size;++i)if(i<28u||i>=32u)sum+=dst[i];
    return sum==be32(dst+28);
}

static void start_kernel(void)
{
    static const char kernel_name[11]={'K','E','R','N','E','L',' ',' ','S','Y','S'};
    static const char kernel_magic[8]={'A','S','Y','S','K','E','R','N'};
    static const char dirs[2][11]={{'S','Y','S','T','E','M',' ',' ',' ',' ',' '},{'K','E','R','N','E','L',' ',' ',' ',' ',' '}};
    boot_file_t file;uint32_t entry;int r;
    console_puts("\nSEARCHING \\SYSTEM\\KERNEL\\KERNEL.SYS...");
    r=mount_geometry(active_id,g_system.acsi_part_start[active_part]);
    if(r!=0){console_puts("\nMOUNT FAIL ");console_put_u32((uint32_t)(-r));return;}
    r=path_lookup(dirs,2u,kernel_name,&file);
    if(r!=1){console_puts("\nLOOKUP FAIL ");console_put_u32((uint32_t)(r<0?(-r):r));return;}
    console_puts(" FOUND ");console_put_u32(file.size);console_puts(" B");
    if(!load_image(&file,kernel_magic,BOOT_IMAGE_VERSION,0x00040000UL,0x00040000UL,&entry)){console_puts("\nIMAGE FAIL");return;}
    console_puts("\nSTARTING KERNEL.SYS...");
    ((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);
}

static int load_module_service(const char name[11],void *destination,uint32_t capacity,module_image_info_t *info)
{
    static const char module_magic[8]={'A','S','Y','S','M','O','D','0'};
    static const char dirs[2][11]={{'S','Y','S','T','E','M',' ',' ',' ',' ',' '},{'K','E','R','N','E','L',' ',' ',' ',' ',' '}};
    static const char sysdirs[1][11]={{'S','Y','S','T','E','M',' ',' ',' ',' ',' '}};
    boot_file_t file;uint32_t first,entry,memory_size;uint8_t *h=(uint8_t *)io_words;int found;
    if(!name||!info)return 0;
    found=-1;
    if(mount_geometry(active_id,g_system.acsi_part_start[active_part])==0){
        found=path_lookup(dirs,2u,name,&file);
        if(found!=1)found=path_lookup(sysdirs,1u,name,&file);
    }
    if(found!=1)return 0;
    first=file.data_start+((uint32_t)file.cluster-2u)*file.spc*file.scale;
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
    static const char boot_dirs[2][11]={{'S','Y','S','T','E','M',' ',' ',' ',' ',' '},{'B','O','O','T',' ',' ',' ',' ',' ',' ',' '}};
    boot_file_t file;uint32_t entry;uint8_t id,part;int result;console_clear();console_puts("ATARI SYSTEM BOOT\n\nCONFIG: ");console_puts(g_config_loaded?(g_config_location?"LOADED FROM \\BIOS\\BIOS.CFG":"LOADED FROM \\BIOS.CFG"):(g_config_saved?(g_config_location?"DEFAULTS SAVED TO \\BIOS\\BIOS.CFG":"DEFAULTS SAVED TO \\BIOS.CFG"):"DEFAULTS (READ-ONLY)"));console_puts("  DELAY=");console_put_u32(g_config.boot_delay);console_puts(" VIDEO=");console_put_u32(g_config.video_pref);console_puts(" BLITTER=");console_put_u32(g_config.blitter);console_puts(" ERR=");console_put_u32(g_config_error);console_puts(" DEV=");console_put_u32((uint32_t)(g_config_device&0xffu));console_puts("\nSCANNING BOOT DEVICE...");
    if(!g_system.boot_selection_valid){uint8_t found=0,unit,pref=g_config.boot_device;g_system.acsi_devices=0;g_system.acsi_scanned=1;g_system.boot_ide=0;
        if(pref!=2u){for(id=0;id<8u;++id){console_puts("\nPROBING ACSI ID ");console_put_u32(id);if(acsi_probe_device(id)){g_system.acsi_devices=(uint8_t)(1u<<id);found=1;break;}}}
        if(!found&&pref!=1u){for(unit=0;unit<2u;++unit){console_puts("\nPROBING IDE UNIT ");console_put_u32(unit);if(acsi_probe_device((uint8_t)(IDE_ID_FLAG|unit))){id=(uint8_t)(IDE_ID_FLAG|unit);g_system.boot_ide=1;g_system.ide_units=ide_units_present();found=1;break;}}}if(!found){console_puts("\n\nBOOT DISK NOT FOUND");wait_return();return;}console_puts("\nFOUND ");console_puts(g_system.boot_ide?"IDE UNIT ":"ACSI ID ");console_put_u32(g_system.boot_ide?(uint32_t)(id&0x7fu):(uint32_t)id);console_puts("\nDISK: ");if(acsi_inquiry_name(id,disk_name,sizeof(disk_name)))console_puts(disk_name);else console_puts(g_system.boot_ide?"IDE HARD DISK":"ACSI HARD DISK");if(!acsi_inspect_device(&g_system,id)){console_puts("\n\nBOOT DISK READ ERROR");wait_return();return;}for(part=0;part<g_system.acsi_part_count;++part)if(g_system.acsi_partitions&((uint32_t)1u<<part))break;if(part>=g_system.acsi_part_count){console_puts("\n\nBOOT PARTITION NOT FOUND");wait_return();return;}}
    else{id=g_system.boot_acsi_id;part=g_system.boot_partition;if(!acsi_inspect_device(&g_system,id)){console_puts("\n\nBOOT DISK READ ERROR");wait_return();return;}if(!(g_system.acsi_partitions&((uint32_t)1u<<part))){console_puts("\n\nBOOT PARTITION NOT FOUND");wait_return();return;}}
    console_puts("\nPARTITION: ");console_puts(g_system.acsi_part_id[part]);console_puts(" ");console_put_u32(part+1u);console_puts("\nSEARCHING \\SYSTEM\\BOOT\\BOOT.SYS...");
    handoff.fpu_type=g_system.fpu_type;handoff.clock_mhz=g_system.clock_mhz;
    handoff.clock_known=g_system.clock_known;
    handoff.blitter_present=g_system.blitter_present;
    handoff.blitter_enabled=g_system.blitter_enabled;
    result=mount_geometry(id,g_system.acsi_part_start[part]);
    if(result==0){console_puts(" FS=");console_puts(geo_fat32?"FAT32":"FAT16");result=path_lookup(boot_dirs,2u,boot_name,&file);}
    if(result==1&&load_image(&file,boot_magic,BOOT_IMAGE_VERSION,0x00020000UL,0x00020000UL,&entry)){console_puts("\nSTARTING BOOT.SYS...");active_id=id;active_part=part;handoff.magic=BOOT_HANDOFF_MAGIC;handoff.version=BOOT_HANDOFF_VERSION;handoff.size=sizeof(handoff);handoff.machine_type=g_system.machine_type;handoff.cpu_type=g_system.cpu_type;handoff.video_mode=g_system.video_mode;handoff.boot_acsi_id=id;handoff.boot_partition=part;handoff.blitter_present=g_system.blitter_present;handoff.blitter_enabled=g_system.blitter_enabled;handoff.st_ram_bytes=g_system.ram_bytes;handoff.tt_ram_bytes=g_system.tt_ram_bytes;handoff.console_clear=console_clear;handoff.console_puts=console_puts;handoff.console_put_u32=console_put_u32;handoff.start_kernel=start_kernel;handoff.load_module=load_module_service;handoff.acsi_read_sector=acsi_read_sector;handoff.acsi_write_sector=acsi_write_sector;
handoff.video_enter=video8_enter;handoff.video_leave=nova_enter_text_vga;
handoff.video_clear=video8_clear;handoff.video_fill=video8_fill;handoff.video_text=video8_text;
handoff.video_blit_mono=video8_blit_mono;((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);console_clear();console_puts("ATARI SYSTEM BOOT\n\nBOOT.SYS RETURNED UNEXPECTEDLY");wait_return();return;}
    console_clear();console_puts("ATARI SYSTEM BOOT\n\nBOOT DEVICE ");console_puts(g_system.boot_ide?"IDE UNIT ":"ACSI ID ");console_put_u32(g_system.boot_ide?(uint32_t)(id&0x7fu):(uint32_t)id);console_puts("  PARTITION ");console_put_u32((uint32_t)part);console_puts("\n\n");if(result==-2)console_puts("BOOT FILESYSTEM ERROR");else if(result<=0)console_puts("\\SYSTEM\\BOOT\\BOOT.SYS NOT FOUND");else console_puts("BOOT.SYS INVALID OR LOAD FAILED");wait_return();
}
