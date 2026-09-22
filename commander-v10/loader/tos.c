#include "tos_loader.h"
#include "system.h"

static uint16_t sector_words[256];
static uint16_t header_words[256];
static uint16_t header2_words[256];
static uint16_t fat_words[256];
extern void ram_image_start(uint32_t load_address,uint32_t phystop,uint32_t memctrl,uint32_t cpu_type);

static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint16_t be16(const uint8_t *p){return ((uint16_t)p[0]<<8)|(uint16_t)p[1];}

static int name_is_tos_bios(const uint8_t *e)
{
    /* v10: the installer only creates dirs under SYSTEM, so the catalog scans the
       SYSTEM directory (was TOS_BIOS at the root). */
    static const char n[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    uint8_t i;
    for(i=0;i<11u;++i)if(e[i]!=(uint8_t)n[i])return 0;
    return 1;
}

static int image_extension(const uint8_t *e)
{
    return (e[8]=='R'&&e[9]=='O'&&e[10]=='M')||
           (e[8]=='I'&&e[9]=='M'&&e[10]=='G')||
           (e[8]=='B'&&e[9]=='I'&&e[10]=='N')||
           (e[8]=='P'&&e[9]=='R'&&e[10]=='G');
}

static void copy_name(char *out,const uint8_t *e)
{
    uint8_t i,n=0;
    for(i=0;i<8u&&e[i]!=' ';++i)out[n++]=(char)e[i];
    if(e[8]!=' '){out[n++]='.';for(i=8;i<11u&&e[i]!=' ';++i)out[n++]=(char)e[i];}
    out[n]='\0';
}

static uint32_t cluster_sector(uint16_t cluster,uint32_t data_start,uint8_t spc,uint8_t scale)
{
    return data_start+(uint32_t)(cluster-2u)*spc*scale;
}

static uint16_t next_cluster(uint8_t id,uint32_t fat_start,uint16_t cluster)
{
    uint32_t offset=(uint32_t)cluster*2u;
    uint8_t *f=(uint8_t *)fat_words;
    if(!acsi_read_sector(id,fat_start+offset/512u,fat_words))return 0xffffu;
    return le16(f+(uint16_t)(offset&511u));
}

static void add_image(tos_catalog_t *cat,const uint8_t *e,uint32_t fat_start,uint32_t data_start,uint8_t spc,uint8_t scale,uint8_t id)
{
    tos_image_t *im;
    uint16_t cluster=le16(e+26);
    uint8_t *s=(uint8_t *)header_words;
    uint16_t offset;
    if(cat->count>=TOS_CATALOG_MAX||cluster<2u||!image_extension(e))return;
    im=&cat->image[cat->count];
    copy_name(im->name,e);im->size=le32(e+28);
    im->first_sector=cluster_sector(cluster,data_start,spc,scale);
    im->version=0;im->rom_address=0;im->compatible=0;im->loadable=0;
    im->acsi_id=id;im->sectors_per_cluster=spc;im->sector_scale=scale;
    im->first_cluster=cluster;im->fat_start=fat_start;im->data_start=data_start;
    im->payload_offset=0;im->payload_size=0;im->load_address=0;
    if(!acsi_read_sector(id,im->first_sector,header_words))return;
    if(e[8]=='P'&&e[9]=='R'&&e[10]=='G'){
        uint8_t *s2=(uint8_t *)header2_words;
        if(!acsi_read_sector(id,im->first_sector+1u,header2_words))return;
        for(offset=0;offset<1024u-12u;offset+=2u){
            const uint8_t *p=offset<512u?s+offset:s2+(offset-512u);
            uint16_t version=be16(p+2);uint32_t address=((uint32_t)p[8]<<24)|((uint32_t)p[9]<<16)|((uint32_t)p[10]<<8)|p[11];
            if((p[0]==0x60u||p[0]==0x4eu)&&version>=0x0100u&&version<0x0500u&&address>=0x800u&&address<0x80000UL){
                uint32_t text_size=((uint32_t)s[2]<<24)|((uint32_t)s[3]<<16)|((uint32_t)s[4]<<8)|s[5];
                uint32_t text_end=28u+text_size;
                uint32_t entry=((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|((uint32_t)p[6]<<8)|p[7];
                if(text_end>offset&&text_end<=im->size&&entry>=address&&entry<address+(text_end-offset)){
                    im->version=version;im->payload_offset=offset;im->payload_size=text_end-offset;im->load_address=address;im->compatible=1;
                    im->loadable=(uint8_t)(g_system.machine_type<MACHINE_TT&&g_system.cpu_type<=CPU_68030);
                }
                break;
            }
        }
    } else {
        im->version=be16(s+2);
        if(im->size==196608UL)im->rom_address=0x00fc0000UL;
        else if(im->size==262144UL||im->size==524288UL)im->rom_address=0x00e00000UL;
        if(im->rom_address)im->compatible=1;
    }
    ++cat->count;
}

void tos_catalog_scan(tos_catalog_t *cat,uint8_t id,uint32_t part)
{
    uint8_t *b=(uint8_t *)sector_words,spc,fats,scale;
    uint16_t bytes_per_sector,reserved,root_entries,spf,dir_cluster=0;
    uint32_t fat_start,root_start,data_start,root_sectors,lba;
    uint16_t i,walk;
    cat->count=0;cat->fat_valid=0;cat->directory_found=0;
    if(!acsi_read_sector(id,part,sector_words))return;
    bytes_per_sector=le16(b+11);
    if(bytes_per_sector<512u||bytes_per_sector>8192u||
       (bytes_per_sector&(bytes_per_sector-1u)))return;
    scale=(uint8_t)(bytes_per_sector/512u);
    spc=b[13];reserved=le16(b+14);fats=b[16];root_entries=le16(b+17);spf=le16(b+22);
    if(!spc||!reserved||!fats||!root_entries||!spf)return;
    root_sectors=((uint32_t)root_entries*32u+bytes_per_sector-1u)/bytes_per_sector;
    fat_start=part+(uint32_t)reserved*scale;
    root_start=fat_start+(uint32_t)fats*spf*scale;
    data_start=root_start+root_sectors*scale;cat->fat_valid=1;
    for(lba=root_start;lba<root_start+root_sectors*scale&&!dir_cluster;++lba){
        if(!acsi_read_sector(id,lba,sector_words))return;
        for(i=0;i<16u;++i){const uint8_t *e=b+i*32u;if(e[0]==0)break;if(e[0]!=0xe5u&&(e[11]&0x10u)&&name_is_tos_bios(e))dir_cluster=le16(e+26);}
    }
    if(dir_cluster<2u)return;
    cat->directory_found=1;
    for(walk=0;walk<64u&&dir_cluster>=2u&&dir_cluster<0xfff8u&&cat->count<TOS_CATALOG_MAX;++walk){
        for(lba=0;lba<(uint32_t)spc*scale&&cat->count<TOS_CATALOG_MAX;++lba){
            if(!acsi_read_sector(id,cluster_sector(dir_cluster,data_start,spc,scale)+lba,sector_words))return;
            for(i=0;i<16u&&cat->count<TOS_CATALOG_MAX;++i){const uint8_t *e=b+i*32u;if(e[0]==0)return;if(e[0]!=0xe5u&&!(e[11]&0x18u))add_image(cat,e,fat_start,data_start,spc,scale,id);}
        }
        dir_cluster=next_cluster(id,fat_start,dir_cluster);
    }
}

static void copy_bytes(uint8_t *dst,const uint8_t *src,uint16_t count)
{
    while(count--)*dst++=*src++;
}

static void move_bytes(uint8_t *dst,const uint8_t *src,uint32_t count)
{
    if(dst<src){while(count--)*dst++=*src++;}
    else {dst+=count;src+=count;while(count--)*--dst=*--src;}
}

static int validate_chain(const tos_image_t *im)
{
    uint32_t bytes_per_cluster=(uint32_t)im->sectors_per_cluster*im->sector_scale*512u;
    uint32_t clusters=(im->size+bytes_per_cluster-1u)/bytes_per_cluster,i;
    uint16_t cluster=im->first_cluster,slow=cluster,fast=cluster;
    if(!bytes_per_cluster||!clusters||clusters>65520UL)return 0;
    for(i=0;i<clusters;++i){
        if(cluster<2u||cluster>=0xfff8u)return 0;
        if(i+1u<clusters){cluster=next_cluster(im->acsi_id,im->fat_start,cluster);if(cluster==0xffffu)return 0;}
        if(slow>=2u&&slow<0xfff8u)slow=next_cluster(im->acsi_id,im->fat_start,slow);
        if(fast>=2u&&fast<0xfff8u)fast=next_cluster(im->acsi_id,im->fat_start,fast);
        if(fast>=2u&&fast<0xfff8u)fast=next_cluster(im->acsi_id,im->fat_start,fast);
        if(i>0u&&slow>=2u&&slow<0xfff8u&&slow==fast)return 0;
    }
    return 1;
}

int tos_load_once(const tos_image_t *im)
{
    uint32_t position=im->payload_offset,remaining,sector_index,within_cluster,stage_address,load_end;
    uint16_t cluster=im->first_cluster,skip,amount;
    uint8_t *buffer=(uint8_t *)sector_words,*destination;
    uint32_t cluster_sectors=(uint32_t)im->sectors_per_cluster*im->sector_scale;
    if(!im->loadable||!im->payload_size||im->payload_offset+im->payload_size>im->size||!cluster_sectors)return 0;
    if(g_system.ram_bytes<=im->payload_size+8192u)return 0;
    if(!validate_chain(im))return 0;
    load_end=im->load_address+im->payload_size;
    if(load_end<im->load_address||load_end>g_system.ram_bytes-8192u)return 0;
    stage_address=(g_system.ram_bytes-8192u-im->payload_size)&~3UL;
    if(stage_address<0x10000UL)return 0;
    destination=(uint8_t *)(uintptr_t)stage_address;
    remaining=im->payload_size;sector_index=position/512u;skip=(uint16_t)(position&511u);
    while(sector_index>=cluster_sectors){cluster=next_cluster(im->acsi_id,im->fat_start,cluster);if(cluster<2u||cluster>=0xfff8u)return 0;sector_index-=cluster_sectors;}
    within_cluster=sector_index;
    while(remaining){
        uint32_t physical=cluster_sector(cluster,im->data_start,im->sectors_per_cluster,im->sector_scale)+within_cluster;
        if(!acsi_read_sector(im->acsi_id,physical,sector_words))return 0;
        amount=(uint16_t)(512u-skip);if((uint32_t)amount>remaining)amount=(uint16_t)remaining;
        copy_bytes(destination,buffer+skip,amount);destination+=amount;remaining-=amount;skip=0;
        if(++within_cluster>=cluster_sectors){cluster=next_cluster(im->acsi_id,im->fat_start,cluster);within_cluster=0;if(remaining&&(cluster<2u||cluster>=0xfff8u))return 0;}
    }
    move_bytes((uint8_t *)(uintptr_t)im->load_address,(const uint8_t *)(uintptr_t)stage_address,im->payload_size);
    ram_image_start(im->load_address,g_system.ram_bytes,REG8(MMU_MEMCONF),g_system.cpu_type);
    return 0;
}
