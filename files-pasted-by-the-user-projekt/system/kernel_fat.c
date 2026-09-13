#include "kernel_fat.h"
#include "kernel_block.h"

static uint8_t sector_buffer[512];
static char partition_id[4];
static uint16_t logical_sector_size;
static uint32_t volume_start,fat_start,root_start,data_start,cluster_count,fat_span;
typedef struct {uint32_t lba;uint16_t off;uint32_t cluster;uint32_t size;uint8_t attr;} fat_entry_loc_t;
static uint16_t root_physical_sectors,cluster_physical_sectors;
static uint8_t fat32;
static enum fat_health volume_health;

static uint16_t le16(const uint8_t *p)
{return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int power_of_two(uint16_t value)
{return value&&!(value&(uint16_t)(value-1u));}

const char *kfat_partition_id(void){return partition_id;}
uint8_t kfat_fat32(void){return fat32;}
uint16_t kfat_sector_size(void){return logical_sector_size;}
enum fat_health kfat_health(void){return volume_health;}

static int same_name(const uint8_t *entry,const char name[11])
{uint8_t i;for(i=0;i<11u;++i)if(entry[i]!=(uint8_t)name[i])return 0;return 1;}

static uint32_t eoc_min(void){return fat32?0x0FFFFFF8u:0xFFF8u;}
static uint32_t cluster_bad(void){return fat32?0x0FFFFFF7u:0xFFF7u;}

static int read_fat_entry(uint32_t cluster,uint32_t *next)
{
    uint32_t offset=(uint32_t)cluster*(fat32?4u:2u);
    if(!kblock_read(fat_start+offset/512u,1u,sector_buffer))return 0;
    if(fat32)*next=(le32(sector_buffer+(uint16_t)(offset&511u))&0x0FFFFFFFu);
    else *next=le16(sector_buffer+(uint16_t)(offset&511u));
    return 1;
}

static enum fat_file_state scan_directory_sector(uint32_t lba,const char name[11],
                                                  uint8_t want_directory,uint32_t *cluster,uint32_t *size)
{
    uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;
        if(e[0]==0)return FAT_FILE_NOT_FOUND;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u))continue;
        if(((e[11]&0x10u)!=0)!=want_directory)continue;
        if(same_name(e,name)){if(cluster)*cluster=(fat32?(((uint32_t)le16(e+20)<<16)|le16(e+26)):le16(e+26));if(size)*size=le32(e+28);return FAT_FILE_FOUND;}
    }
    return FAT_FILE_CORRUPT; /* Sector valid, requested entry not in this sector. */
}

/* Walk one directory cluster chain looking for a single entry. */
static enum fat_file_state scan_dir_chain(uint32_t directory,const char name[11],uint8_t want_dir,uint32_t *found_cluster,uint32_t *size)
{
    uint32_t cluster=directory,next,slow=directory,fast=directory;uint32_t i;enum fat_file_state state;
    for(i=0;i<256u;++i){uint16_t s;
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors;++s){uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;uint32_t found=0;state=scan_directory_sector(lba,name,want_dir,&found,size);if(state==FAT_FILE_FOUND){if(found_cluster)*found_cluster=found;return state;}if(state==FAT_FILE_IO_FAILED||state==FAT_FILE_NOT_FOUND)return state;}
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(next>=eoc_min())return FAT_FILE_NOT_FOUND;
        if(next<2u||next==cluster_bad())return FAT_FILE_CORRUPT;
        cluster=next;
        if(slow<eoc_min()&&!read_fat_entry(slow,&slow))return FAT_FILE_IO_FAILED;
        if(fast<eoc_min()&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(fast<eoc_min()&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(slow<eoc_min()&&fast<eoc_min()&&slow==fast)return FAT_FILE_CORRUPT;
    }
    return FAT_FILE_CORRUPT;
}

/* System files live in \SYSTEM\KERNEL\ (new layout); \SYSTEM\<name> is kept as
 * a fallback so older volumes still boot. */
static enum fat_file_state locate_system_file(const char name[11],uint32_t *file_cluster,uint32_t *size)
{
    static const char system_name[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    static const char kernel_name[11]={'K','E','R','N','E','L',' ',' ',' ',' ',' '};
    uint32_t i;uint32_t directory=0,kdir=0;enum fat_file_state state;
    if(!logical_sector_size||!name||!size)return FAT_FILE_CORRUPT;
    for(i=0;i<root_physical_sectors;++i){state=scan_directory_sector(root_start+i,system_name,1,&directory,0);if(state==FAT_FILE_FOUND)break;if(state==FAT_FILE_IO_FAILED||state==FAT_FILE_NOT_FOUND)return state;}
    if(!directory)return FAT_FILE_NOT_FOUND;
    state=scan_dir_chain(directory,kernel_name,1,&kdir,0);
    if(state==FAT_FILE_FOUND&&kdir>=2u){
        state=scan_dir_chain(kdir,name,0,file_cluster,size);
        if(state==FAT_FILE_FOUND)return state;
    }
    return scan_dir_chain(directory,name,0,file_cluster,size);
}

enum fat_file_state kfat_find_system_file(const char name[11],uint32_t *size)
{return locate_system_file(name,0,size);}

static void copy_bytes(uint8_t *destination,const uint8_t *source,uint16_t count)
{while(count--)*destination++=*source++;}

enum fat_file_state kfat_load_system_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size)
{
    uint32_t cluster,next;uint16_t s;uint32_t file_size,remaining,needed,index;uint8_t *out=(uint8_t *)destination;
    enum fat_file_state state=locate_system_file(name,&cluster,&file_size);
    if(state!=FAT_FILE_FOUND)return state;
    if(!destination||!file_size||capacity<file_size)return FAT_FILE_CORRUPT;
    needed=(file_size+(uint32_t)cluster_physical_sectors*512u-1u)/((uint32_t)cluster_physical_sectors*512u);
    if(!needed||needed>cluster_count)return FAT_FILE_CORRUPT;
    remaining=file_size;
    for(index=0;index<needed;++index){
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors&&remaining;++s){uint16_t amount;
            uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;
            if(!kblock_read(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
            amount=remaining<512u?(uint16_t)remaining:512u;copy_bytes(out,sector_buffer,amount);out+=amount;remaining-=amount;
        }
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(index+1u<needed){if(next<2u||next>=eoc_min()||next==cluster_bad())return FAT_FILE_CORRUPT;cluster=next;}
        else if(next<eoc_min())return FAT_FILE_CORRUPT;
    }
    if(remaining)return FAT_FILE_CORRUPT;
    *size=file_size;
    return FAT_FILE_FOUND;
}

static int path_component(const char **path,char name[11])
{
    uint8_t base=0,ext=0,i;const char *p=*path;int in_ext=0;
    while(*p=='\\'||*p=='/')++p;
    if(!*p){*path=p;return 0;}
    for(i=0;i<11u;++i)name[i]=' ';
    while(*p&&*p!='\\'&&*p!='/'){
        char c=*p++;if(c>='a'&&c<='z')c=(char)(c-'a'+'A');
        if(c=='.'){if(in_ext)return -1;in_ext=1;continue;}
        if(c<=' '||c=='*'||c=='?'||c==':'||c=='"'||c=='<'||c=='>'||c=='|')return -1;
        if(!in_ext){if(base>=8u)return -1;name[base++]=c;}
        else {if(ext>=3u)return -1;name[8u+ext++]=c;}
    }
    if(!base)return -1;
    while(*p=='\\'||*p=='/')++p;
    *path=p;
    return 1;
}

static int scan_any_sector(uint32_t lba,const char name[11],uint32_t *cluster,
                           uint32_t *size,uint8_t *attributes)
{
    uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return -1;
    for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;
        if(e[0]==0)return 0;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u))continue;
        if(same_name(e,name)){*cluster=(fat32?(((uint32_t)le16(e+20)<<16)|le16(e+26)):le16(e+26));*size=le32(e+28);*attributes=e[11];return 1;}
    }
    return 2;
}

static enum fat_file_state find_in_directory(uint32_t directory,const char name[11],
                                              uint32_t *cluster,uint32_t *size,uint8_t *attributes)
{
    uint32_t i;uint32_t current=directory,next,s;int result;
    if(!directory){for(i=0;i<root_physical_sectors;++i){result=scan_any_sector(root_start+i,name,cluster,size,attributes);if(result<0)return FAT_FILE_IO_FAILED;if(result==0)return FAT_FILE_NOT_FOUND;if(result==1)return FAT_FILE_FOUND;}return FAT_FILE_NOT_FOUND;}
    for(i=0;i<256u;++i){if(current<2u||(uint32_t)current>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors;++s){result=scan_any_sector(data_start+(uint32_t)(current-2u)*cluster_physical_sectors+s,name,cluster,size,attributes);if(result<0)return FAT_FILE_IO_FAILED;if(result==0)return FAT_FILE_NOT_FOUND;if(result==1)return FAT_FILE_FOUND;}
        if(!read_fat_entry(current,&next))return FAT_FILE_IO_FAILED;
        if(next>=eoc_min())return FAT_FILE_NOT_FOUND;
        if(next<2u||next==cluster_bad())return FAT_FILE_CORRUPT;
        current=next;
    }
    return FAT_FILE_CORRUPT;
}

static enum fat_file_state resolve_path(const char *path,uint32_t *cluster,
                                        uint32_t *size,uint8_t *attributes)
{
    char name[11];uint32_t directory=0,found=0;uint32_t found_size=0;uint8_t attr=0,depth=0;int part;
    enum fat_file_state state;if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    while((part=path_component(&path,name))>0){if(++depth>8u)return FAT_FILE_CORRUPT;
        state=find_in_directory(directory,name,&found,&found_size,&attr);if(state!=FAT_FILE_FOUND)return state;
        if(*path){if(!(attr&0x10u))return FAT_FILE_NOT_FOUND;directory=found;}
    }
    if(part<0)return FAT_FILE_CORRUPT;
    if(!depth){*cluster=0;*size=0;*attributes=0x10u;return FAT_FILE_FOUND;}
    *cluster=found;*size=found_size;*attributes=attr;return FAT_FILE_FOUND;
}

enum fat_file_state kfat_stat_path(const char *path,uint32_t *size,uint8_t *attributes)
{uint32_t cluster;if(!size||!attributes)return FAT_FILE_CORRUPT;return resolve_path(path,&cluster,size,attributes);}

static enum fat_file_state load_cluster_file(uint32_t cluster,uint32_t file_size,
                                              void *destination,uint32_t capacity,uint32_t *size)
{
    uint32_t next;uint16_t s;uint32_t remaining,needed,index;uint8_t *out=(uint8_t *)destination;
    if(!size||capacity<file_size||(!destination&&file_size))return FAT_FILE_CORRUPT;
    if(!file_size){*size=0;return FAT_FILE_FOUND;}
    needed=(file_size+(uint32_t)cluster_physical_sectors*512u-1u)/((uint32_t)cluster_physical_sectors*512u);
    if(!needed||needed>cluster_count)return FAT_FILE_CORRUPT;
    remaining=file_size;
    for(index=0;index<needed;++index){if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors&&remaining;++s){uint16_t amount;uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;
            if(!kblock_read(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
            amount=remaining<512u?(uint16_t)remaining:512u;
            copy_bytes(out,sector_buffer,amount);out+=amount;remaining-=amount;}
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(index+1u<needed){if(next<2u||next>=eoc_min()||next==cluster_bad())return FAT_FILE_CORRUPT;cluster=next;}else if(next<eoc_min())return FAT_FILE_CORRUPT;
    }
    if(remaining)return FAT_FILE_CORRUPT;
    *size=file_size;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_load_path(const char *path,void *destination,uint32_t capacity,uint32_t *size)
{uint32_t cluster;uint32_t file_size;uint8_t attr;enum fat_file_state state=resolve_path(path,&cluster,&file_size,&attr);if(state!=FAT_FILE_FOUND)return state;if(attr&0x10u)return FAT_FILE_CORRUPT;return load_cluster_file(cluster,file_size,destination,capacity,size);}

enum fat_file_state kfat_read_path_at(const char *path,uint32_t offset,
                                      void *destination,uint32_t capacity,uint32_t *size)
{
    uint32_t cluster,next;uint16_t sector;uint32_t file_size,cluster_bytes,skip,within;
    uint32_t remaining,requested,walked=0;uint8_t attr,*out=(uint8_t *)destination;
    enum fat_file_state state=resolve_path(path,&cluster,&file_size,&attr);
    if(state!=FAT_FILE_FOUND)return state;
    if(!size||(attr&0x10u)||offset>file_size)return FAT_FILE_CORRUPT;
    requested=file_size-offset;
    if(requested>capacity)requested=capacity;
    if(!requested){*size=0;return FAT_FILE_FOUND;}
    if(!destination)return FAT_FILE_CORRUPT;
    cluster_bytes=(uint32_t)cluster_physical_sectors*512u;
    if(!cluster_bytes)return FAT_FILE_CORRUPT;
    skip=offset/cluster_bytes;within=offset%cluster_bytes;
    while(skip--){
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(next<2u||next>=eoc_min()||next==cluster_bad())return FAT_FILE_CORRUPT;
        cluster=next;
        if(++walked>cluster_count)return FAT_FILE_CORRUPT;
    }
    remaining=requested;
    while(remaining){uint16_t start=(uint16_t)(within&511u),amount;
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        sector=(uint16_t)(within/512u);
        if(sector>=cluster_physical_sectors)return FAT_FILE_CORRUPT;
        if(!kblock_read(data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+sector,
                        1u,sector_buffer))return FAT_FILE_IO_FAILED;
        amount=(uint16_t)(512u-start);
        if((uint32_t)amount>remaining)amount=(uint16_t)remaining;
        copy_bytes(out,sector_buffer+start,amount);
        out+=amount;remaining-=amount;within+=(uint32_t)amount;
        if(remaining&&within==cluster_bytes){
            if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
            if(next<2u||next>=eoc_min()||next==cluster_bad())return FAT_FILE_CORRUPT;
            cluster=next;within=0;
            if(++walked>cluster_count)return FAT_FILE_CORRUPT;
        }
    }
    *size=requested;
    return FAT_FILE_FOUND;
}

/* --- write support -------------------------------------------------------- */
static int write_fat_sector(uint32_t sector)
{
    if(!kblock_write(fat_start+sector,1u,sector_buffer))return 0;
    if(fat_span){if(!kblock_write(fat_start+fat_span+sector,1u,sector_buffer))return 0;}
    return 1;
}
static int write_fat_entry(uint32_t cluster,uint32_t value)
{
    uint32_t offset=(uint32_t)cluster*(fat32?4u:2u),sector=offset/512u;
    if(!kblock_read(fat_start+sector,1u,sector_buffer))return 0;
    if(fat32){
        uint16_t at=(uint16_t)(offset&511u);uint32_t v=le32(sector_buffer+at);
        v=(v&0xF0000000u)|((uint32_t)value&0x0FFFFFFFu);
        sector_buffer[at]=(uint8_t)v;sector_buffer[at+1u]=(uint8_t)(v>>8);
        sector_buffer[at+2u]=(uint8_t)(v>>16);sector_buffer[at+3u]=(uint8_t)(v>>24);
    } else {
        sector_buffer[offset&511u]=(uint8_t)value;
        sector_buffer[(offset&511u)+1u]=(uint8_t)(value>>8);
    }
    return write_fat_sector(sector);
}
static uint32_t alloc_cluster(void)
{
    uint32_t sector=0u,base=0u;
    while(base<(uint32_t)cluster_count+2u){
        uint16_t i;
        if(!kblock_read(fat_start+sector,1u,sector_buffer))return 0u;
        for(i=0u;i<256u;i++){
            uint32_t c=base+i;
            if(c>=2u&&(uint32_t)c<cluster_count+2u&&le16(sector_buffer+(uint16_t)i*2u)==0u){
                sector_buffer[i*2u]=0xffu;sector_buffer[i*2u+1u]=0xffu;
                if(!write_fat_sector(sector))return 0u;
                return c;
            }
        }
        ++sector;base+=256u;
    }
    return 0u;
}
static int scan_dir_entry_loc(uint32_t lba,const char name[11],fat_entry_loc_t *loc,int *free_off)
{
    uint8_t i;
    if(!kblock_read(lba,1u,sector_buffer))return -1;
    for(i=0u;i<16u;i++){
        const uint8_t *e=sector_buffer+(uint16_t)i*32u;
        if(e[0]==0u){if(free_off&&*free_off<0)*free_off=(int)i;return 0;}
        if(e[0]==0xe5u){if(free_off&&*free_off<0)*free_off=(int)i;continue;}
        if((e[11]&0x0fu)==0x0fu||(e[11]&0x08u))continue;
        if(same_name(e,name)){
            if(loc){loc->lba=lba;loc->off=(uint16_t)i;loc->cluster=le16(e+26);loc->size=le32(e+28);loc->attr=e[11];}
            return 1;
        }
    }
    return 2;
}
static int scan_parent(uint32_t parent,const char name[11],fat_entry_loc_t *loc,
                       uint32_t *free_lba,uint16_t *free_off)
{
    uint32_t i;uint32_t c,next;uint16_t s;
    if(!parent){
        for(i=0u;i<root_physical_sectors;i++){
            int fi=-1,r=scan_dir_entry_loc(root_start+i,name,loc,&fi);
            if(r==1)return 1;
            if(r<0)return -1;
            if(fi>=0&&free_lba&&*free_lba==0u){*free_lba=root_start+i;*free_off=(uint16_t)fi;}
        }
        return 0;
    }
    c=parent;
    for(i=0u;i<256u;i++){
        if(c<2u||(uint32_t)c>cluster_count+1u)return -1;
        for(s=0u;s<cluster_physical_sectors;s++){
            uint32_t lba=data_start+(uint32_t)(c-2u)*cluster_physical_sectors+s;
            int fi=-1,r=scan_dir_entry_loc(lba,name,loc,&fi);
            if(r==1)return 1;
            if(r<0)return -1;
            if(fi>=0&&free_lba&&*free_lba==0u){*free_lba=lba;*free_off=(uint16_t)fi;}
        }
        if(!read_fat_entry(c,&next))return -1;
        if(next>=eoc_min())return 0;
        if(next<2u)return -1;
        c=next;
    }
    return -1;
}
static int split_path(const char *path,uint32_t *parent,char name[11])
{
    char cur[11];
    uint32_t dir=0u,found=0u;uint32_t size=0u;uint8_t attr=0u;
    int p=path_component(&path,cur);
    if(p<=0)return 0;
    while(p>0){
        char next[11];
        int q=path_component(&path,next);
        if(q<=0){
            int i;*parent=dir;for(i=0;i<11;i++)name[i]=cur[i];
            return 1;
        }
        {enum fat_file_state st=find_in_directory(dir,cur,&found,&size,&attr);
         if(st!=FAT_FILE_FOUND||!(attr&0x10u))return 0;
         dir=found;}
        {int i;for(i=0;i<11;i++)cur[i]=next[i];}
        p=q;
    }
    return 0;
}

enum fat_file_state kfat_create_path(const char *path)
{
    uint32_t parent=0u;char name[11];fat_entry_loc_t loc;uint32_t free_lba=0u;uint16_t free_off=0u;
    int r;
    if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    if(!split_path(path,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,&free_lba,&free_off);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r==1){
        /* Truncate: free the chain and zero the entry. */
        uint32_t c=loc.cluster;
        uint16_t guard=0u;
        while(c>=2u&&c<cluster_count+2u&&guard++<cluster_count){
            uint32_t n;if(!read_fat_entry(c,&n))break;
            if(!write_fat_entry(c,0u))break;
            if(n<2u||n>=eoc_min())break;
            c=n;
        }
        if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        {uint8_t *e=sector_buffer+loc.off*32u;e[26]=0u;e[27]=0u;e[28]=0u;e[29]=0u;e[30]=0u;e[31]=0u;}
        if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        return FAT_FILE_FOUND;
    }
    if(!free_lba)return FAT_FILE_NOT_FOUND;
    if(!kblock_read(free_lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    {uint8_t *e=sector_buffer+free_off*32u;uint8_t i;
     for(i=0u;i<11u;i++)e[i]=(uint8_t)name[i];
     for(i=11u;i<32u;i++)e[i]=0u;
     e[11]=0x20u;}
    if(!kblock_write(free_lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_write_path_at(const char *path,uint32_t offset,
                                       const void *source,uint32_t size,uint32_t *written)
{
    uint32_t parent=0u;char name[11];fat_entry_loc_t loc;
    uint32_t cluster_bytes,skip,within,remaining,total;
    uint32_t cluster,next;const uint8_t *in=(const uint8_t *)source;
    int r;
    if(!logical_sector_size||!path||!source)return FAT_FILE_CORRUPT;
    if(!split_path(path,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,0,0);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r!=1)return FAT_FILE_NOT_FOUND;
    if(loc.attr&0x10u)return FAT_FILE_NOT_FOUND;
    cluster_bytes=(uint32_t)cluster_physical_sectors*512u;
    cluster=loc.cluster;total=loc.size;
    if(cluster<2u){
        cluster=alloc_cluster();
        if(cluster<2u)return FAT_FILE_IO_FAILED;
        if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        sector_buffer[loc.off*32u+26u]=(uint8_t)cluster;
        sector_buffer[loc.off*32u+27u]=(uint8_t)(cluster>>8);
        if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        total=0u;
    }
    skip=offset/cluster_bytes;within=offset%cluster_bytes;
    while(skip--){
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(next<2u||next>=eoc_min()||next==cluster_bad()){
            next=alloc_cluster();if(next<2u)return FAT_FILE_IO_FAILED;
            if(!write_fat_entry(cluster,next))return FAT_FILE_IO_FAILED;
        }
        cluster=next;
    }
    remaining=size;
    while(remaining){
        uint16_t sector=(uint16_t)(within/512u),start=(uint16_t)(within&511u),amount,i;
        uint32_t lba;
        if(sector>=cluster_physical_sectors)return FAT_FILE_IO_FAILED;
        lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+sector;
        amount=(uint16_t)(512u-start);
        if((uint32_t)amount>remaining)amount=(uint16_t)remaining;
        if(start==0u&&amount==512u){
            for(i=0u;i<512u;i++)sector_buffer[i]=in[i];   /* full sector: no read */
        }else{
            if(!kblock_read(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
            for(i=0u;i<amount;i++)sector_buffer[start+i]=in[i];
        }
        if(!kblock_write(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        in+=amount;remaining-=amount;within+=amount;
        if(remaining&&within==cluster_bytes){
            if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
            if(next<2u||next>=eoc_min()||next==cluster_bad()){
                next=alloc_cluster();if(next<2u)return FAT_FILE_IO_FAILED;
                if(!write_fat_entry(cluster,next))return FAT_FILE_IO_FAILED;
            }
            cluster=next;within=0u;
        }
    }
    if(offset+size>total){
        total=offset+size;
        if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
        {uint8_t *e=sector_buffer+loc.off*32u;
         e[28]=(uint8_t)total;e[29]=(uint8_t)(total>>8);e[30]=(uint8_t)(total>>16);e[31]=(uint8_t)(total>>24);}
        if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    }
    if(written)*written=size;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_delete_path(const char *path)
{
    uint32_t parent=0u;char name[11];fat_entry_loc_t loc;
    uint32_t c;uint16_t guard=0u;int r;
    if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    if(!split_path(path,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,0,0);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r!=1)return FAT_FILE_NOT_FOUND;
    c=loc.cluster;
    while(c>=2u&&c<cluster_count+2u&&guard++<cluster_count){
        uint32_t n;if(!read_fat_entry(c,&n))break;
        if(!write_fat_entry(c,0u))break;
        if(n<2u||n>=eoc_min())break;
        c=n;
    }
    if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    sector_buffer[loc.off*32u]=0xe5u;
    if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_attrib_path(const char *path,uint8_t set,uint8_t attr,uint8_t *old)
{
    uint32_t parent=0u;char name[11];fat_entry_loc_t loc;int r;
    if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    if(!split_path(path,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,0,0);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r!=1)return FAT_FILE_NOT_FOUND;
    if(old)*old=loc.attr;
    if(!set)return FAT_FILE_FOUND;
    if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    sector_buffer[loc.off*32u+11u]=attr;
    if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_rename_path(const char *from,const char *to)
{
    uint32_t parent=0u;char name[11],newname[11];fat_entry_loc_t loc;int r;
    if(!logical_sector_size||!from||!to)return FAT_FILE_CORRUPT;
    if(!split_path(from,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,0,0);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r!=1)return FAT_FILE_NOT_FOUND;
    {uint32_t p2=0u;if(!split_path(to,&p2,newname))return FAT_FILE_NOT_FOUND;if(p2!=parent)return FAT_FILE_CORRUPT;}
    if(!kblock_read(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    {uint8_t *e=sector_buffer+loc.off*32u;uint8_t i;for(i=0u;i<11u;i++)e[i]=(uint8_t)newname[i];}
    if(!kblock_write(loc.lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_create_dir(const char *path)
{
    uint32_t parent=0u;char name[11];fat_entry_loc_t loc;uint32_t free_lba=0u;
    uint16_t free_off=0u;uint32_t cluster;int r;
    if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    if(!split_path(path,&parent,name))return FAT_FILE_NOT_FOUND;
    r=scan_parent(parent,name,&loc,&free_lba,&free_off);
    if(r<0)return FAT_FILE_IO_FAILED;
    if(r==1)return FAT_FILE_CORRUPT;
    if(!free_lba)return FAT_FILE_NOT_FOUND;
    cluster=alloc_cluster();
    if(cluster<2u)return FAT_FILE_IO_FAILED;
    {uint32_t base=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors,j;
     for(j=0u;j<cluster_physical_sectors;j++){
         uint32_t k;
         if(!kblock_read(base+j,1u,sector_buffer))return FAT_FILE_IO_FAILED;
         for(k=0u;k<512u;k++)sector_buffer[k]=0u;
         if(j==0u){
             uint8_t i;
             sector_buffer[0]='.';for(i=1u;i<11u;i++)sector_buffer[i]=' ';
             sector_buffer[11]=0x10u;
             sector_buffer[26]=(uint8_t)cluster;sector_buffer[27]=(uint8_t)(cluster>>8);
             sector_buffer[32]='.';sector_buffer[33]='.';for(i=34u;i<43u;i++)sector_buffer[i]=' ';
             sector_buffer[43]=0x10u;
             if(parent>=2u){sector_buffer[58]=(uint8_t)parent;sector_buffer[59]=(uint8_t)(parent>>8);}
         }
         if(!kblock_write(base+j,1u,sector_buffer))return FAT_FILE_IO_FAILED;
     }}
    if(!kblock_read(free_lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    {uint8_t *e=sector_buffer+free_off*32u;uint8_t i;
     for(i=0u;i<11u;i++)e[i]=(uint8_t)name[i];
     for(i=11u;i<32u;i++)e[i]=0u;
     e[11]=0x10u;
     e[26]=(uint8_t)cluster;e[27]=(uint8_t)(cluster>>8);}
    if(!kblock_write(free_lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    return FAT_FILE_FOUND;
}

static void display_name(const uint8_t *entry,char name[13])
{uint8_t i,n=0;for(i=0;i<8u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];if(entry[8]!=' '){name[n++]='.';for(i=8;i<11u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];}name[n]='\0';}

static int list_sector(uint32_t lba,kfat_list_callback_t callback,void *context)
{uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return -1;for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;char name[13];if(e[0]==0)return 0;if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u)||e[0]=='.')continue;display_name(e,name);if(!callback(name,le32(e+28),e[11],context))return 0;}return 1;}

enum fat_file_state kfat_list_path(const char *path,kfat_list_callback_t callback,void *context)
{
    uint32_t directory,next;uint16_t s;uint32_t size,i;uint8_t attr;int result;enum fat_file_state state;
    if(!callback)return FAT_FILE_CORRUPT;
    state=resolve_path(path,&directory,&size,&attr);
    if(state!=FAT_FILE_FOUND)return state;
    if(!(attr&0x10u))return FAT_FILE_CORRUPT;
    if(!directory){for(i=0;i<root_physical_sectors;++i){result=list_sector(root_start+i,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}return FAT_FILE_FOUND;}
    for(i=0;i<256u;++i){if(directory<2u||(uint32_t)directory>cluster_count+1u)return FAT_FILE_CORRUPT;for(s=0;s<cluster_physical_sectors;++s){result=list_sector(data_start+(uint32_t)(directory-2u)*cluster_physical_sectors+s,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}if(!read_fat_entry(directory,&next))return FAT_FILE_IO_FAILED;if(next>=eoc_min())return FAT_FILE_FOUND;if(next<2u||next==cluster_bad())return FAT_FILE_CORRUPT;directory=next;}
    return FAT_FILE_CORRUPT;
}

enum fat_state kfat_mount(uint32_t start,const char id[4])
{
    uint32_t total,data_sectors,clusters,root_sectors;
    uint16_t bytes_per_sector,reserved,root_entries,sectors_per_fat;
    uint8_t sectors_per_cluster,fats;
    partition_id[0]=partition_id[1]=partition_id[2]='?';partition_id[3]='\0';
    logical_sector_size=0;
    volume_health=FAT_HEALTH_UNKNOWN;
    if(!start||start>0x001fffffUL||!id)return FAT_PARTITION_INVALID;
    partition_id[0]=id[0];partition_id[1]=id[1];partition_id[2]=id[2];
    if(!kblock_read(start,1u,sector_buffer))return FAT_IO_FAILED;

    bytes_per_sector=le16(sector_buffer+11);sectors_per_cluster=sector_buffer[13];
    reserved=le16(sector_buffer+14);fats=sector_buffer[16];
    root_entries=le16(sector_buffer+17);sectors_per_fat=le16(sector_buffer+22);
    {uint32_t spf32=le32(sector_buffer+36);fat32=0u;
     if(!sectors_per_fat&&spf32){fat32=1u;sectors_per_fat=(uint16_t)(spf32>0xFFFFu?0xFFFFu:spf32);root_entries=0u;}}
    total=le16(sector_buffer+19);if(!total)total=le32(sector_buffer+32);
    if(bytes_per_sector<512u||bytes_per_sector>8192u||!power_of_two(bytes_per_sector)||
       !power_of_two(sectors_per_cluster)||!reserved||!fats||fats>2u||
       !sectors_per_fat||!total)return FAT_BPB_INVALID;
    if(!fat32&&!root_entries)return FAT_BPB_INVALID;
    root_sectors=fat32?0u:((uint32_t)root_entries*32u+bytes_per_sector-1u)/bytes_per_sector;
    if(total<reserved+(uint32_t)fats*sectors_per_fat+root_sectors)return FAT_BPB_INVALID;
    data_sectors=total-reserved-(uint32_t)fats*sectors_per_fat-root_sectors;
    clusters=data_sectors/sectors_per_cluster;
    if(fat32){if(clusters<2u||clusters>0x0FFFFFF5u)return FAT_UNSUPPORTED;}
    else if(clusters<4085u||clusters>=65525u)return FAT_UNSUPPORTED;
    logical_sector_size=bytes_per_sector;volume_start=start;
    fat_span=(uint32_t)sectors_per_fat*(bytes_per_sector/512u);
    fat_start=volume_start+(uint32_t)reserved*(bytes_per_sector/512u);
    root_start=fat_start+(uint32_t)fats*sectors_per_fat*(bytes_per_sector/512u);
    if(fat32){root_physical_sectors=(uint16_t)((uint16_t)sectors_per_cluster*(bytes_per_sector/512u));data_start=root_start;}
    else{root_physical_sectors=(uint16_t)(root_sectors*(bytes_per_sector/512u));data_start=root_start+root_physical_sectors;}
    cluster_physical_sectors=(uint16_t)((uint16_t)sectors_per_cluster*(bytes_per_sector/512u));
    cluster_count=clusters;
    if(!kblock_read(fat_start,1u,sector_buffer)){logical_sector_size=0;return FAT_IO_FAILED;}
    if(fat32){uint16_t ext=le16(sector_buffer+40);volume_health=(ext&0x4000u)?FAT_HEALTH_HARD_ERROR:((ext&0x8000u)?FAT_HEALTH_DIRTY:FAT_HEALTH_CLEAN);}
    else {uint16_t flags=le16(sector_buffer+2);if(!(flags&0x4000u))volume_health=FAT_HEALTH_HARD_ERROR;else if(!(flags&0x8000u))volume_health=FAT_HEALTH_DIRTY;else volume_health=FAT_HEALTH_CLEAN;}
    return FAT_MOUNTED;
}
