#include "boot_protocol.h"
#include "config.h"
#include "console.h"
#include "fdc.h"
#include "fat12.h"
#include "system.h"
/* Boot progress is drawn on two FIXED rows and rewritten in place, so the boot
   log never scrolls: the header stays and only the value changes (user request). */
#define PROBE_ROW  21u
#define STATUS_ROW 22u
#include "block_device.h"
#include "ide.h"

typedef struct {uint8_t id,spc,scale,fat12;uint16_t cluster;uint32_t size,fat_start,data_start;} boot_file_t;
static uint16_t io_words[256],fat_words[256];static boot_handoff_t handoff;
static uint8_t active_id,active_part;
static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|p[1];}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int same_name(const uint8_t *e,const char name[11]){uint8_t i;for(i=0;i<11u;++i)if(e[i]!=(uint8_t)name[i])return 0;return 1;}
static uint16_t fat_straddle[256];
/* FAT12 marks end-of-chain at 0xFF8, FAT16 at 0xFFF8. */
static uint16_t fat_eoc(uint8_t fat12){return fat12?0x0ff8u:0xfff8u;}
static uint16_t fat_next(uint8_t id,uint32_t fat_start,uint16_t cluster,uint8_t fat12)
{
    uint8_t *p=(uint8_t *)fat_words;
    if(fat12){
        /* FAT12 packs 1.5 bytes per entry; an entry may straddle a sector. */
        uint32_t offset=(uint32_t)cluster+(cluster>>1);uint16_t off=(uint16_t)(offset&511u),v;uint8_t hi;
        if(!acsi_read_sector(id,fat_start+(offset>>9),fat_words))return 0xffffu;
        if(off==511u){if(!acsi_read_sector(id,fat_start+(offset>>9)+1u,fat_straddle))return 0xffffu;hi=((uint8_t *)fat_straddle)[0];}
        else hi=p[off+1u];
        v=(uint16_t)((uint16_t)p[off]|((uint16_t)hi<<8));
        return (cluster&1u)?(uint16_t)(v>>4):(uint16_t)(v&0x0fffu);
    }
    {uint32_t offset=(uint32_t)cluster*2u;
     if(!acsi_read_sector(id,fat_start+offset/512u,fat_words))return 0xffffu;
     return le16(p+(uint16_t)(offset&511u));}
}

static int locate_system_file(uint8_t id,uint32_t part,const char file_name[11],boot_file_t *file)
{
    static const char system_name[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    uint8_t *b=(uint8_t *)io_words,spc,fats,scale,fat12;uint16_t bps,reserved,roots,spf,cluster=0,walk,i;uint32_t fat_start,root_start,root_phys,data_start,lba;
    if(!acsi_read_sector(id,part,io_words))return -2;
    bps=le16(b+11);if(bps<512u||bps>8192u||(bps&(bps-1u)))return -2;scale=(uint8_t)(bps/512u);
    spc=b[13];reserved=le16(b+14);fats=b[16];roots=le16(b+17);spf=le16(b+22);if(!spc||!reserved||!fats||!roots||!spf)return -2;
    fat_start=part+(uint32_t)reserved*scale;root_start=fat_start+(uint32_t)fats*spf*scale;root_phys=((uint32_t)roots*32u+bps-1u)/bps*scale;data_start=root_start+root_phys;
    {uint32_t total=le16(b+19),data_sectors,clusters;uint16_t root_sectors=((uint32_t)roots*32u+bps-1u)/bps;
     if(!total)total=le32(b+32);
     data_sectors=total-(uint32_t)reserved-(uint32_t)fats*spf-root_sectors;
     clusters=data_sectors/spc;
     if(clusters<2u||clusters>=65525u)return -2;
     fat12=(uint8_t)(clusters<4085u);}   /* Microsoft rule: < 4085 clusters = FAT12 */
    for(lba=root_start;lba<root_start+root_phys&&!cluster;++lba){if(!acsi_read_sector(id,lba,io_words))return -2;for(i=0;i<16u;++i){const uint8_t *e=b+i*32u;if(e[0]==0)break;if(e[0]!=0xe5u&&(e[11]&0x10u)&&same_name(e,system_name))cluster=le16(e+26);}}
    if(cluster<2u)return -1;
    for(walk=0;walk<64u&&cluster>=2u&&cluster<fat_eoc(fat12);++walk){uint32_t first=data_start+(uint32_t)(cluster-2u)*spc*scale;
        for(lba=0;lba<(uint32_t)spc*scale;++lba){if(!acsi_read_sector(id,first+lba,io_words))return -2;for(i=0;i<16u;++i){const uint8_t *e=b+i*32u;if(e[0]==0)return 0;if(e[0]!=0xe5u&&!(e[11]&0x18u)&&same_name(e,file_name)){file->id=id;file->spc=spc;file->scale=scale;file->cluster=le16(e+26);file->size=le32(e+28);file->fat_start=fat_start;file->data_start=data_start;file->fat12=fat12;return file->cluster>=2u&&file->size?1:-2;}}}
        cluster=fat_next(id,fat_start,cluster,fat12);if(cluster==0xffffu)return -2;}
    return 0;
}

static int validate_chain(const boot_file_t *f)
{
    uint32_t cluster_bytes=(uint32_t)f->spc*f->scale*512u,needed=(f->size+cluster_bytes-1u)/cluster_bytes,i;uint16_t cluster=f->cluster,slow=cluster,fast=cluster,eoc=fat_eoc(f->fat12);
    if(!cluster_bytes||!needed||needed>65520UL)return 0;
    for(i=0;i<needed;++i){if(cluster<2u||cluster>=eoc)return 0;if(i+1u<needed){cluster=fat_next(f->id,f->fat_start,cluster,f->fat12);if(cluster==0xffffu)return 0;}if(slow>=2u&&slow<eoc)slow=fat_next(f->id,f->fat_start,slow,f->fat12);if(fast>=2u&&fast<eoc)fast=fat_next(f->id,f->fat_start,fast,f->fat12);if(fast>=2u&&fast<eoc)fast=fat_next(f->id,f->fat_start,fast,f->fat12);if(i>0u&&slow>=2u&&slow<eoc&&slow==fast)return 0;}
    return 1;
}

static int load_image(const boot_file_t *f,const char magic[8],uint16_t header_version,uint32_t load,uint32_t header_load,uint32_t *entry)
{
    uint32_t rounded=(f->size+511u)&~511UL,remaining=f->size,sector=0,sum=0,i,memory_size;uint16_t cluster=f->cluster,shown=0xffffu;uint8_t *dst=(uint8_t *)(uintptr_t)load,*src=(uint8_t *)io_words;uint32_t cluster_sectors=(uint32_t)f->spc*f->scale;
    if(f->size<BOOT_IMAGE_HEADER_SIZE||f->size>512UL*1024UL||rounded<f->size||load+rounded>g_system.ram_bytes-8192u)return 0;
    if(!validate_chain(f))return 0;
    while(remaining){uint32_t lba=f->data_start+(uint32_t)(cluster-2u)*cluster_sectors+sector;uint16_t amount=remaining<512u?(uint16_t)remaining:512u,copy;if(!acsi_read_sector(f->id,lba,io_words))return 0;for(copy=0;copy<amount;++copy)*dst++=src[copy];remaining-=amount;
        {uint16_t percent=(uint16_t)(((f->size-remaining)*100u)/f->size);
         if(magic[4]!='M'&&(shown==0xffffu||percent==100u||percent>=shown+5u)){post_draw_progress((uint16_t)(magic[4]=='K'?5u+percent*40u/100u:percent*5u/100u),magic[4]=='K'?"LOADING KERNEL.SYS":"LOADING BOOT.SYS");shown=percent;}}
        if(++sector>=cluster_sectors){sector=0;if(remaining){cluster=fat_next(f->id,f->fat_start,cluster,f->fat12);if(cluster<2u||cluster>=fat_eoc(f->fat12))return 0;}}}
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
    post_draw_progress(5u,"LOADING KERNEL.SYS");
    if(locate_system_file(active_id,g_system.acsi_part_start[active_part],kernel_name,&file)!=1){
        console_clear_text_row(STATUS_ROW);console_set_cursor(0,STATUS_ROW);console_puts("KERNEL.SYS NOT FOUND        ");return;
    }
    if(!read_kernel_header(&file,&hdr)){
        console_clear_text_row(STATUS_ROW);console_set_cursor(0,STATUS_ROW);console_puts("KERNEL HEADER READ FAILED   ");return;
    }
    if(hdr.load!=KERNEL_IMAGE_LOAD_ADDRESS){
        console_clear_text_row(STATUS_ROW);console_set_cursor(0,STATUS_ROW);
        console_puts("KERNEL LOAD ADDRESS MISMATCH: EXPECTED ");put_hex32(KERNEL_IMAGE_LOAD_ADDRESS);
        console_puts(" GOT ");put_hex32(hdr.load);console_puts("    ");return;
    }
    if(load_image(&file,kernel_magic,BOOT_IMAGE_VERSION,KERNEL_IMAGE_LOAD_ADDRESS,KERNEL_IMAGE_LOAD_ADDRESS,&entry)){
        ((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);
    } else {
        console_clear_text_row(STATUS_ROW);console_set_cursor(0,STATUS_ROW);
        console_puts("KERNEL IMAGE INVALID (MAGIC/SIZE/CHECKSUM/ABI)  ");
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
/* A boot floppy in drive A is usable when it carries a 512-byte FAT BPB. */
static int floppy_bootable(void)
{
    static uint8_t fb[512];
    return fdc_read_boot_sector(0u,fb)&&le16(fb+11)==512u&&le16(fb+22)!=0u;
}
static void floppy_set_partition(void)
{
    g_system.acsi_part_start[0]=0u;
    g_system.acsi_part_id[0][0]='F';g_system.acsi_part_id[0][1]='D';g_system.acsi_part_id[0][2]='D';g_system.acsi_part_id[0][3]=0;
}

/* Probe all storage, filling BOTH transport bitmaps so the kernel can mount
   partitions from IDE *and* ACSI, and return the first present device as the
   boot device.  The scan order follows the persisted boot preference
   (g_config.boot_device: AUTO/ACSI first, or IDE first). */
static uint8_t boot_scan_device(void)
{
    uint8_t k,j,n=0,id=0xffu,order[10];
    g_system.acsi_devices=0;g_system.acsi_scanned=1;
    g_system.ide_units=ide_units_present();
    /* Explicit priority list first (the floppy is tried by the caller after
       the disks, or first when it is the remembered device). */
    if(g_config.boot_order_valid){
        for(k=0u;k<4u;++k){
            uint8_t d=g_config.boot_order[k];
            if(d==0xffu)break;
            if(d==BLOCK_FLOPPY_ID)continue;
            order[n++]=d;
        }
    }
    /* Then the generic order chosen by boot_device. */
    for(k=0u;k<10u&&n<10u;++k){
        uint8_t d=(g_config.boot_device==CFG_BOOT_IDE)
            ?((k<2u)?(uint8_t)(0x80u|k):(uint8_t)(k-2u))
            :((k<8u)?(uint8_t)k:(uint8_t)(0x80u|(k-8u)));
        int dup=0;
        for(j=0u;j<n;++j)if(order[j]==d)dup=1;
        if(!dup)order[n++]=d;
    }
    for(k=0u;k<n;++k){
        uint8_t cand=order[k];
        post_draw_progress(0u,(cand&0x80u)?"SCANNING IDE":"SCANNING ACSI");
        if(block_probe(cand)){
            if(cand&0x80u)g_system.ide_units=(uint8_t)(g_system.ide_units|(uint8_t)(1u<<(cand&0x7fu)));
            else g_system.acsi_devices=(uint8_t)(g_system.acsi_devices|(uint8_t)(1u<<cand));
            if(id==0xffu)id=cand;
        }
    }
    return id;
}
/* 1 = rescan (Space), 0 = give up (Esc/Return). */
static int wait_retry(void)
{
    int key;
    for(;;){
        key=ikbd_scancode();
        if(key==0x39||key==0x139)return 1;
        if(key==0x01||key==0x1c||key==0x72)return 0;
    }
}
void boot_stage2_run(void)
{
    static const char boot_name[11]={'B','O','O','T',' ',' ',' ',' ','S','Y','S'};
    static const char boot_magic[8]={'A','S','Y','S','B','O','O','T'};
    boot_file_t file;uint32_t entry;uint8_t id=0xffu,part=0u;int result;post_begin_boot();console_set_cursor(0,22);
    if(g_system.boot_selection_valid){
        id=g_system.boot_acsi_id;part=g_system.boot_partition;
        if(id==BLOCK_FLOPPY_ID){
            /* Remembered floppy: usable only while a bootable disk is inserted. */
            if(floppy_bootable()){part=0;floppy_set_partition();}
            else{console_puts("\n\nREMEMBERED BOOT FLOPPY UNAVAILABLE");g_system.boot_selection_valid=0;}
        }else
        /* The remembered drive may be gone (moved disk, failed adapter). Do not
           dead-end on it: fall back to the normal scan/retry path below. */
        if(!acsi_inspect_device(&g_system,id)){
            console_puts("\n\nREMEMBERED BOOT DEVICE UNAVAILABLE");
            g_system.boot_selection_valid=0;
        }
    }
    if(!g_system.boot_selection_valid){
        for(;;){
            id=boot_scan_device();
            if(id!=0xffu)break;
            /* No hard disk: try a boot floppy in drive A. */
            if(floppy_bootable()){id=BLOCK_FLOPPY_ID;break;}
            console_puts("\n\nNO BOOT DEVICE - RETRY? (SPACE)");
            if(!wait_retry())return;
            console_puts("\n\nRESCANNING...");
        }
        if(id==BLOCK_FLOPPY_ID){
            
            part=0;floppy_set_partition();
        }else{
            if(!acsi_inspect_device(&g_system,id)){console_puts("\n\nBOOT DISK READ ERROR");wait_return();return;}
            for(part=0;part<4u;++part)if(g_system.acsi_partitions&(uint8_t)(1u<<part))break;
            if(part>=4u){console_puts("\n\nBOOT PARTITION NOT FOUND");wait_return();return;}
        }
    }
    post_draw_progress(0u,"LOADING BOOT.SYS");
    handoff.fpu_type=g_system.fpu_type;handoff.clock_mhz=g_system.clock_mhz;
    handoff.clock_known=g_system.clock_known;
    handoff.blitter_present=g_system.blitter_present;
    handoff.blitter_enabled=g_system.blitter_enabled;
    result=locate_system_file(id,g_system.acsi_part_start[part],boot_name,&file);
    if(result==1&&load_image(&file,boot_magic,BOOT_IMAGE_VERSION,0x00008000UL,0x00008000UL,&entry)){post_draw_progress(5u,"STARTING BOOT.SYS");active_id=id;active_part=part;handoff.magic=BOOT_HANDOFF_MAGIC;handoff.version=BOOT_HANDOFF_VERSION;handoff.size=sizeof(handoff);handoff.machine_type=g_system.machine_type;handoff.cpu_type=g_system.cpu_type;handoff.fpu_type=g_system.fpu_type;handoff.clock_mhz=g_system.clock_mhz;handoff.clock_known=g_system.clock_known;handoff.reserved0=(uint8_t)(0x80u|g_system.nova_adapter);handoff.reserved1=g_system.nova_chipset;handoff.netusbee_ethernet=g_system.netusbee_ethernet;handoff.netusbee_usb=g_system.netusbee_usb;handoff.video_mode=g_system.video_mode;handoff.boot_acsi_id=id;handoff.boot_partition=part;handoff.blitter_present=g_system.blitter_present;handoff.blitter_enabled=g_system.blitter_enabled;handoff.st_ram_bytes=g_system.ram_bytes;handoff.tt_ram_bytes=g_system.tt_ram_bytes;handoff.boot_progress=post_boot_progress;handoff.console_clear=console_clear;handoff.console_puts=console_puts;handoff.console_put_u32=console_put_u32;handoff.start_kernel=start_kernel;handoff.load_module=load_module_service;handoff.acsi_read_sector=acsi_read_sector;handoff.console_text_copy=console_text_copy;handoff.nova_initialized=g_system.nova_initialized;handoff.card_video_mode=(g_system.nova_initialized&&g_config.video_mode==CFG_MODE_640)?CFG_MODE_800:g_config.video_mode;handoff.video_force_shifter=g_system.video_force_shifter;handoff.reset=bios_reset;handoff.video_boot_ok=bios_video_boot_ok;handoff.acsi_write_sector=acsi_write_sector;handoff.floppy_mount=fat12_mount;handoff.floppy_dir=fat12_dir;handoff.floppy_read_file=fat12_read_file;
handoff.disk_known=0;handoff.disk_name[0]=0;handoff.disk_sectors=0;handoff.disk_sector_bytes=0;
handoff.acsi_devices=g_system.acsi_devices;handoff.ide_units=g_system.ide_units;
if(id==BLOCK_FLOPPY_ID)handoff.disk_known=1;   /* floppy: identity known, no model */
else if(id&IDE_ID_FLAG){uint8_t u=(uint8_t)(id&0x7fu);(void)ide_model(u,handoff.disk_name,(uint8_t)sizeof(handoff.disk_name));handoff.disk_sectors=ide_capacity_sectors(u);handoff.disk_sector_bytes=512u;handoff.disk_known=1;}
else {if(acsi_inquiry_name(id,handoff.disk_name,(uint8_t)sizeof(handoff.disk_name)))handoff.disk_known=1;}((void (*)(const boot_handoff_t *))(uintptr_t)entry)(&handoff);console_clear();console_puts("ATARI SYSTEM BOOT\n\nBOOT.SYS RETURNED UNEXPECTEDLY");wait_return();return;}
    console_clear();console_puts("ATARI SYSTEM BOOT\n\n");if(result==-2)console_puts("BOOT FILESYSTEM ERROR");else if(result<=0)console_puts("SYSTEM/BOOT.SYS NOT FOUND");else console_puts("BOOT.SYS INVALID OR LOAD FAILED");wait_return();
}