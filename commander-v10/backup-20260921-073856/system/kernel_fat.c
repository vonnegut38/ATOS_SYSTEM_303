#include "kernel_fat.h"
#include "kernel_block.h"

static uint8_t sector_buffer[512];
static char partition_id[4];
static uint16_t logical_sector_size;
static uint32_t volume_start,fat_start,root_start,data_start,cluster_count;
static uint16_t root_physical_sectors,cluster_physical_sectors;
static enum fat_health volume_health;
static uint32_t fat_sectors;
static uint8_t fat_copies;
static int write_failed;

static uint16_t le16(const uint8_t *p)
{return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int power_of_two(uint16_t value)
{return value&&!(value&(uint16_t)(value-1u));}

const char *kfat_partition_id(void){return partition_id;}
uint16_t kfat_sector_size(void){return logical_sector_size;}
enum fat_health kfat_health(void){return volume_health;}

static int same_name(const uint8_t *entry,const char name[11])
{uint8_t i;for(i=0;i<11u;++i)if(entry[i]!=(uint8_t)name[i])return 0;return 1;}

static int read_fat_entry(uint16_t cluster,uint16_t *next)
{
    uint32_t offset=(uint32_t)cluster*2u;
    if(!kblock_read(fat_start+offset/512u,1u,sector_buffer))return 0;
    *next=le16(sector_buffer+(uint16_t)(offset&511u));return 1;
}

static enum fat_file_state scan_directory_sector(uint32_t lba,const char name[11],
                                                  uint8_t want_directory,uint16_t *cluster,uint32_t *size)
{
    uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return FAT_FILE_IO_FAILED;
    for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;
        if(e[0]==0)return FAT_FILE_NOT_FOUND;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u))continue;
        if(((e[11]&0x10u)!=0)!=want_directory)continue;
        if(same_name(e,name)){if(cluster)*cluster=le16(e+26);if(size)*size=le32(e+28);return FAT_FILE_FOUND;}
    }
    return FAT_FILE_CORRUPT; /* Sector valid, requested entry not in this sector. */
}

static enum fat_file_state locate_system_file(const char name[11],uint16_t *file_cluster,uint32_t *size)
{
    static const char system_name[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    uint32_t i;uint16_t directory=0,cluster,next,slow,fast;enum fat_file_state state;
    if(!logical_sector_size||!name||!size)return FAT_FILE_CORRUPT;
    for(i=0;i<root_physical_sectors;++i){state=scan_directory_sector(root_start+i,system_name,1,&directory,0);if(state==FAT_FILE_FOUND)break;if(state==FAT_FILE_IO_FAILED||state==FAT_FILE_NOT_FOUND)return state;}
    if(!directory)return FAT_FILE_NOT_FOUND;
    cluster=slow=fast=directory;
    for(i=0;i<256u;++i){uint16_t s;
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors;++s){uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;uint16_t found=0;state=scan_directory_sector(lba,name,0,&found,size);if(state==FAT_FILE_FOUND){if(file_cluster)*file_cluster=found;return state;}if(state==FAT_FILE_IO_FAILED||state==FAT_FILE_NOT_FOUND)return state;}
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(next>=0xfff8u)return FAT_FILE_NOT_FOUND;
        if(next<2u||next==0xfff7u)return FAT_FILE_CORRUPT;
        cluster=next;
        if(slow<0xfff8u&&!read_fat_entry(slow,&slow))return FAT_FILE_IO_FAILED;
        if(fast<0xfff8u&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(fast<0xfff8u&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(slow<0xfff8u&&fast<0xfff8u&&slow==fast)return FAT_FILE_CORRUPT;
    }
    return FAT_FILE_CORRUPT;
}

enum fat_file_state kfat_find_system_file(const char name[11],uint32_t *size)
{return locate_system_file(name,0,size);}

static void copy_bytes(uint8_t *destination,const uint8_t *source,uint16_t count)
{while(count--)*destination++=*source++;}

enum fat_file_state kfat_load_system_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size)
{
    uint16_t cluster,next,s;uint32_t file_size,remaining,needed,index;uint8_t *out=(uint8_t *)destination;
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
        if(index+1u<needed){if(next<2u||next>=0xfff8u||next==0xfff7u)return FAT_FILE_CORRUPT;cluster=next;}
        else if(next<0xfff8u)return FAT_FILE_CORRUPT;
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

static int scan_any_sector(uint32_t lba,const char name[11],uint16_t *cluster,
                           uint32_t *size,uint8_t *attributes)
{
    uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return -1;
    for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;
        if(e[0]==0)return 0;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u))continue;
        if(same_name(e,name)){*cluster=le16(e+26);*size=le32(e+28);*attributes=e[11];return 1;}
    }
    return 2;
}

static enum fat_file_state find_in_directory(uint16_t directory,const char name[11],
                                              uint16_t *cluster,uint32_t *size,uint8_t *attributes)
{
    uint32_t i;uint16_t current=directory,next,s;int result;
    if(!directory){for(i=0;i<root_physical_sectors;++i){result=scan_any_sector(root_start+i,name,cluster,size,attributes);if(result<0)return FAT_FILE_IO_FAILED;if(result==0)return FAT_FILE_NOT_FOUND;if(result==1)return FAT_FILE_FOUND;}return FAT_FILE_NOT_FOUND;}
    for(i=0;i<256u;++i){if(current<2u||(uint32_t)current>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors;++s){result=scan_any_sector(data_start+(uint32_t)(current-2u)*cluster_physical_sectors+s,name,cluster,size,attributes);if(result<0)return FAT_FILE_IO_FAILED;if(result==0)return FAT_FILE_NOT_FOUND;if(result==1)return FAT_FILE_FOUND;}
        if(!read_fat_entry(current,&next))return FAT_FILE_IO_FAILED;
        if(next>=0xfff8u)return FAT_FILE_NOT_FOUND;
        if(next<2u||next==0xfff7u)return FAT_FILE_CORRUPT;
        current=next;
    }
    return FAT_FILE_CORRUPT;
}

static enum fat_file_state resolve_path(const char *path,uint16_t *cluster,
                                        uint32_t *size,uint8_t *attributes)
{
    char name[11];uint16_t directory=0,found=0;uint32_t found_size=0;uint8_t attr=0,depth=0;int part;
    enum fat_file_state state;if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    while((part=path_component(&path,name))>0){if(++depth>16u)return FAT_FILE_CORRUPT;
        state=find_in_directory(directory,name,&found,&found_size,&attr);if(state!=FAT_FILE_FOUND)return state;
        if(*path){if(!(attr&0x10u))return FAT_FILE_NOT_FOUND;directory=found;}
    }
    if(part<0)return FAT_FILE_CORRUPT;
    if(!depth){*cluster=0;*size=0;*attributes=0x10u;return FAT_FILE_FOUND;}
    *cluster=found;*size=found_size;*attributes=attr;return FAT_FILE_FOUND;
}

enum fat_file_state kfat_stat_path(const char *path,uint32_t *size,uint8_t *attributes)
{uint16_t cluster;if(!size||!attributes)return FAT_FILE_CORRUPT;return resolve_path(path,&cluster,size,attributes);}

static enum fat_file_state load_cluster_file(uint16_t cluster,uint32_t file_size,
                                              void *destination,uint32_t capacity,uint32_t *size)
{
    uint16_t next,s;uint32_t remaining,needed,index;uint8_t *out=(uint8_t *)destination;
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
        if(index+1u<needed){if(next<2u||next>=0xfff8u||next==0xfff7u)return FAT_FILE_CORRUPT;cluster=next;}else if(next<0xfff8u)return FAT_FILE_CORRUPT;
    }
    if(remaining)return FAT_FILE_CORRUPT;
    *size=file_size;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_load_path(const char *path,void *destination,uint32_t capacity,uint32_t *size)
{uint16_t cluster;uint32_t file_size;uint8_t attr;enum fat_file_state state=resolve_path(path,&cluster,&file_size,&attr);if(state!=FAT_FILE_FOUND)return state;if(attr&0x10u)return FAT_FILE_CORRUPT;return load_cluster_file(cluster,file_size,destination,capacity,size);}

enum fat_file_state kfat_read_path_at(const char *path,uint32_t offset,
                                      void *destination,uint32_t capacity,uint32_t *size)
{
    uint16_t cluster,next,sector;uint32_t file_size,cluster_bytes,skip,within;
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
        if(next<2u||next>=0xfff8u||next==0xfff7u)return FAT_FILE_CORRUPT;
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
            if(next<2u||next>=0xfff8u||next==0xfff7u)return FAT_FILE_CORRUPT;
            cluster=next;within=0;
            if(++walked>cluster_count)return FAT_FILE_CORRUPT;
        }
    }
    *size=requested;
    return FAT_FILE_FOUND;
}

static void display_name(const uint8_t *entry,char name[13])
{uint8_t i,n=0;for(i=0;i<8u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];if(entry[8]!=' '){name[n++]='.';for(i=8;i<11u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];}name[n]='\0';}

static int list_sector(uint32_t lba,kfat_list_callback_t callback,void *context)
{uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return -1;for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;char name[13];if(e[0]==0)return 0;if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u)||e[0]=='.')continue;display_name(e,name);if(!callback(name,le32(e+28),e[11],context))return 0;}return 1;}

enum fat_file_state kfat_list_path(const char *path,kfat_list_callback_t callback,void *context)
{
    uint16_t directory,next,s;uint32_t size,i;uint8_t attr;int result;enum fat_file_state state;
    if(!callback)return FAT_FILE_CORRUPT;
    state=resolve_path(path,&directory,&size,&attr);
    if(state!=FAT_FILE_FOUND)return state;
    if(!(attr&0x10u))return FAT_FILE_CORRUPT;
    if(!directory){for(i=0;i<root_physical_sectors;++i){result=list_sector(root_start+i,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}return FAT_FILE_FOUND;}
    for(i=0;i<256u;++i){if(directory<2u||(uint32_t)directory>cluster_count+1u)return FAT_FILE_CORRUPT;for(s=0;s<cluster_physical_sectors;++s){result=list_sector(data_start+(uint32_t)(directory-2u)*cluster_physical_sectors+s,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}if(!read_fat_entry(directory,&next))return FAT_FILE_IO_FAILED;if(next>=0xfff8u)return FAT_FILE_FOUND;if(next<2u||next==0xfff7u)return FAT_FILE_CORRUPT;directory=next;}
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
    total=le16(sector_buffer+19);if(!total)total=le32(sector_buffer+32);
    if(bytes_per_sector<512u||bytes_per_sector>8192u||!power_of_two(bytes_per_sector)||
       !power_of_two(sectors_per_cluster)||!reserved||!fats||fats>2u||
       !root_entries||!sectors_per_fat||!total)return FAT_BPB_INVALID;
    root_sectors=((uint32_t)root_entries*32u+bytes_per_sector-1u)/bytes_per_sector;
    if(total<reserved+(uint32_t)fats*sectors_per_fat+root_sectors)return FAT_BPB_INVALID;
    data_sectors=total-reserved-(uint32_t)fats*sectors_per_fat-root_sectors;
    clusters=data_sectors/sectors_per_cluster;
    if(clusters<4085u||clusters>=65525u)return FAT_UNSUPPORTED;
    logical_sector_size=bytes_per_sector;volume_start=start;
    fat_start=volume_start+(uint32_t)reserved*(bytes_per_sector/512u);
    root_start=fat_start+(uint32_t)fats*sectors_per_fat*(bytes_per_sector/512u);
    root_physical_sectors=(uint16_t)(root_sectors*(bytes_per_sector/512u));
    data_start=root_start+root_physical_sectors;
    cluster_physical_sectors=(uint16_t)((uint16_t)sectors_per_cluster*(bytes_per_sector/512u));
    cluster_count=clusters;fat_sectors=(uint32_t)sectors_per_fat*(bytes_per_sector/512u);fat_copies=fats;write_failed=0;
    if(clusters+2u>fat_sectors*256u){logical_sector_size=0;return FAT_BPB_INVALID;}
    if(!kblock_read(fat_start,1u,sector_buffer)){logical_sector_size=0;return FAT_IO_FAILED;}
    {uint16_t flags=le16(sector_buffer+2);if(!(flags&0x4000u))volume_health=FAT_HEALTH_HARD_ERROR;else if(!(flags&0x8000u))volume_health=FAT_HEALTH_DIRTY;else volume_health=FAT_HEALTH_CLEAN;}
    return FAT_MOUNTED;
}

/* Bounded metadata undo journal. Data is written only to newly allocated
 * clusters, and the directory entry is published last. This is not an
 * on-disk crash journal: interrupted writes leave the FAT dirty. */
#define TX_SECTORS 128u
#define COPY_LIMIT (1024u*1024u)
static struct {uint32_t lba;uint8_t old[512];} undo[TX_SECTORS];
static unsigned undo_count;
static uint8_t chain_seen[8192],io_data[512];
static void put16(uint8_t *p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t *p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static int valid_cluster(uint16_t c){return c>=2u&&c<0xfff0u&&(uint32_t)c<cluster_count+2u;}
static int tx_write(uint32_t lba,const uint8_t *data)
{
    unsigned i;for(i=0;i<undo_count;i++)if(undo[i].lba==lba)break;
    if(i==undo_count){if(i==TX_SECTORS)return -FAT_WRITE_LIMIT;
        if(!kblock_read(lba,1,undo[i].old))return -FAT_WRITE_IO;
        undo[i].lba=lba;undo_count++;}
    return kblock_write(lba,1,data)?0:-FAT_WRITE_IO;
}
static int set_fat(uint16_t c,uint16_t value)
{
    unsigned f;int r;uint32_t lba;
    for(f=0;f<fat_copies;f++){lba=fat_start+f*fat_sectors+c/256u;
        if(!kblock_read(lba,1,sector_buffer))return -FAT_WRITE_IO;
        put16(sector_buffer+(c%256u)*2u,value);r=tx_write(lba,sector_buffer);if(r)return r;}
    return 0;
}
static int rollback(int error)
{
    while(undo_count){--undo_count;if(!kblock_write(undo[undo_count].lba,1,undo[undo_count].old))write_failed=1;}
    if(write_failed){volume_health=FAT_HEALTH_HARD_ERROR;return -FAT_WRITE_IO;}
    return error;
}
static int tx_begin(void)
{
    uint16_t flags;int r;
    if(!logical_sector_size||write_failed||volume_health!=FAT_HEALTH_CLEAN)return -FAT_WRITE_READ_ONLY;
    undo_count=0;if(!read_fat_entry(1,&flags))return -FAT_WRITE_IO;
    r=set_fat(1,(uint16_t)(flags&~0x8000u));return r?rollback(r):0;
}
static int tx_commit(void)
{
    uint16_t flags;int r;if(!read_fat_entry(1,&flags))return rollback(-FAT_WRITE_IO);
    r=set_fat(1,(uint16_t)(flags|0x8000u));if(r)return rollback(r);undo_count=0;return 0;
}
static int mutation_parent(const char *path,uint16_t *parent,char name[11])
{
    uint16_t c=0;uint32_t size;uint8_t attr,depth=0;int part;enum fat_file_state r;
    if(!path||!logical_sector_size)return -FAT_WRITE_INVALID;
    while((part=path_component(&path,name))>0){if(++depth>16)return -FAT_WRITE_INVALID;
        if(!*path){*parent=c;return 0;}
        r=find_in_directory(c,name,&c,&size,&attr);
        if(r!=FAT_FILE_FOUND||!(attr&16)||!valid_cluster(c))return -FAT_WRITE_INVALID;
        if(attr&1)return -FAT_WRITE_READ_ONLY;
    }
    return -FAT_WRITE_INVALID;
}
/* Locate existing name or first free entry without extending a full directory. */
static int entry_slot(uint16_t dir,const char name[11],int existing,uint32_t *lba,uint16_t *off)
{
    uint32_t walked=0,base,free_lba=0;uint16_t c=dir,s,n,next,free_off=0;unsigned j;int ended=0;
    do{
        if(dir){if(!valid_cluster(c)||++walked>cluster_count)return -FAT_WRITE_INVALID;base=data_start+(uint32_t)(c-2)*cluster_physical_sectors;n=cluster_physical_sectors;}
        else{base=root_start;n=root_physical_sectors;}
        for(s=0;s<n;s++){
            if(!kblock_read(base+s,1,sector_buffer))return -FAT_WRITE_IO;
            for(j=0;j<512;j+=32){uint8_t *e=sector_buffer+j;
                if(!e[0])ended=1;
                if(ended||e[0]==0xe5){if(!free_lba){free_lba=base+s;free_off=j;}if(ended)goto scanned;continue;}
                if((e[11]&15)==15)continue;
                if(same_name(e,name)){
                    /* existing=0 create only (error if present), 1 find only,
                       2 create-or-overwrite: reuse this very slot. */
                    if(existing==0)return -FAT_WRITE_EXISTS;
                    *lba=base+s;*off=j;return 0;
                }
            }
        }
        if(!dir)break;
        if(!read_fat_entry(c,&next))return -FAT_WRITE_IO;
        if(next>=0xfff8)break;
        c=next;
    }while(1);
scanned:
    if(existing==1)return -FAT_WRITE_INVALID;
    if(!free_lba)return -FAT_WRITE_FULL;
    *lba=free_lba;*off=free_off;return 0;
}
static int alloc_cluster(uint16_t *result,uint16_t *cursor)
{
    uint32_t c;uint16_t v;int r;
    for(c=*cursor;c<cluster_count+2u&&c<0xfff0u;c++){
        if(!read_fat_entry((uint16_t)c,&v))return -FAT_WRITE_IO;
        if(!v){r=set_fat((uint16_t)c,0xffff);if(r)return r;*result=c;*cursor=c+1;return 0;}}
    return -FAT_WRITE_FULL;
}
static int check_chain(uint16_t c,uint32_t expected,int exact)
{
    uint32_t count=0;uint16_t next;unsigned i;
    for(i=0;i<sizeof(chain_seen);i++)chain_seen[i]=0;
    if(!c)return expected?-FAT_WRITE_INVALID:0;
    for(;;){if(!valid_cluster(c)||(chain_seen[c>>3]&(1u<<(c&7))))return -FAT_WRITE_INVALID;
        chain_seen[c>>3]|=1u<<(c&7);count++;
        if(!read_fat_entry(c,&next))return -FAT_WRITE_IO;
        if(next>=0xfff8)break;
        c=next;
    }
    return exact&&count!=expected?-FAT_WRITE_INVALID:0;
}
static int publish_entry(uint32_t lba,uint16_t off,const char name[11],uint16_t c,uint32_t size,int dir)
{
    unsigned i;uint8_t *e;if(!kblock_read(lba,1,sector_buffer))return -FAT_WRITE_IO;
    e=sector_buffer+off;for(i=0;i<32;i++)e[i]=0;for(i=0;i<11;i++)e[i]=name[i];
    e[11]=dir?16:32;put16(e+26,c);put32(e+28,size);put16(e+24,33); /* 1980-01-01 until RTC API exists */
    return tx_write(lba,sector_buffer);
}
int kfat_copy_path(const char *source,const char *target)
{
    uint16_t src,parent,off,c=0,first=0,previous=0,cursor=2,s,next;
    uint32_t size,lba,remaining,bytes;uint8_t attr;char name[11];int r;
    if(resolve_path(source,&src,&size,&attr)!=FAT_FILE_FOUND||(attr&16))return -FAT_WRITE_INVALID;
    if(size>COPY_LIMIT)return -FAT_WRITE_LIMIT;
    bytes=(uint32_t)cluster_physical_sectors*512u;
    r=check_chain(src,(size+bytes-1)/bytes,1);if(r)return r;
    r=mutation_parent(target,&parent,name);if(r)return r;
    r=entry_slot(parent,name,2,&lba,&off);if(r)return r;   /* create or overwrite */
    r=tx_begin();if(r)return r;remaining=size;
    while(remaining){r=alloc_cluster(&c,&cursor);if(r)goto fail;
        if(previous){r=set_fat(previous,c);if(r)goto fail;}else first=c;
        for(s=0;s<cluster_physical_sectors;s++){
            uint32_t amount=remaining<512?remaining:512;unsigned i;
            if(amount&&!kblock_read(data_start+(uint32_t)(src-2)*cluster_physical_sectors+s,1,io_data)){r=-FAT_WRITE_IO;goto fail;}
            for(i=amount;i<512;i++)io_data[i]=0;
            if(!kblock_write(data_start+(uint32_t)(c-2)*cluster_physical_sectors+s,1,io_data)){r=-FAT_WRITE_IO;goto fail;}
            remaining-=amount;
        }
        previous=c;if(remaining){if(!read_fat_entry(src,&next)){r=-FAT_WRITE_IO;goto fail;}src=next;}
    }
    r=publish_entry(lba,off,name,first,size,0);if(r)goto fail;return tx_commit();
fail:return rollback(r);
}
enum fat_file_state kfat_write_path_at(const char *path,uint32_t offset,
                                       const void *source,uint32_t count,uint32_t *size)
{
    uint16_t cluster,next,sector;
    uint32_t file_size,cluster_bytes,skip,within,remaining,walked=0;
    uint8_t attr;const uint8_t *in=(const uint8_t *)source;
    enum fat_file_state state=resolve_path(path,&cluster,&file_size,&attr);
    if(state!=FAT_FILE_FOUND)return state;
    if(!size||!source||(attr&0x10u))return FAT_FILE_CORRUPT;
    if(offset>=file_size){*size=0;return FAT_FILE_FOUND;}
    if(count>file_size-offset)count=file_size-offset;
    *size=count;
    if(!count)return FAT_FILE_FOUND;
    cluster_bytes=(uint32_t)cluster_physical_sectors*512u;
    if(!cluster_bytes)return FAT_FILE_CORRUPT;
    skip=offset/cluster_bytes;within=offset%cluster_bytes;
    while(skip--){
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(next<2u||next>=0xfff8u||next==0xfff7u)return FAT_FILE_CORRUPT;
        cluster=next;
        if(++walked>cluster_count)return FAT_FILE_CORRUPT;
    }
    {int r=tx_begin();
     if(r)return FAT_FILE_IO_FAILED;
     remaining=count;
     while(remaining){
         uint16_t start=(uint16_t)(within&511u);uint32_t amount,i;
         if(cluster<2u||(uint32_t)cluster>cluster_count+1u){rollback(-FAT_WRITE_IO);return FAT_FILE_CORRUPT;}
         sector=(uint16_t)(within/512u);
         if(sector>=cluster_physical_sectors){rollback(-FAT_WRITE_IO);return FAT_FILE_CORRUPT;}
         if(!kblock_read(data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+sector,1u,sector_buffer)){
             rollback(-FAT_WRITE_IO);return FAT_FILE_IO_FAILED;}
         amount=512u-start;
         if(amount>remaining)amount=remaining;
         for(i=0;i<amount;i++)sector_buffer[start+i]=in[i];
         {int w=tx_write(data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+sector,sector_buffer);
          if(w){rollback(w);return FAT_FILE_IO_FAILED;}}
         in+=amount;remaining-=amount;within+=amount;
         if(within>=cluster_bytes){
             within=0;
             if(!read_fat_entry(cluster,&next)){rollback(-FAT_WRITE_IO);return FAT_FILE_IO_FAILED;}
             if(next<2u||next>=0xfff8u){rollback(-FAT_WRITE_IO);return FAT_FILE_CORRUPT;}
             cluster=next;
         }
     }
     if(tx_commit())return FAT_FILE_IO_FAILED;}
    return FAT_FILE_FOUND;
}
int kfat_create_path(const char *path)
{
    char name[11];uint16_t parent,off;uint32_t lba;int r;
    r=mutation_parent(path,&parent,name);
    if(r)return r;
    r=entry_slot(parent,name,0,&lba,&off);
    if(r)return r;
    r=tx_begin();
    if(r)return r;
    r=publish_entry(lba,off,name,0,0,0);   /* empty file: no cluster, size 0 */
    if(r)goto fail;
    return tx_commit();
fail:return rollback(r);
}
int kfat_mkdir_path(const char *path)
{
    char name[11];uint16_t parent,off,c,cursor=2,s;uint32_t lba;unsigned i;int r;
    r=mutation_parent(path,&parent,name);if(r)return r;
    r=entry_slot(parent,name,0,&lba,&off);if(r)return r;
    r=tx_begin();if(r)return r;r=alloc_cluster(&c,&cursor);if(r)goto fail;
    for(s=0;s<cluster_physical_sectors;s++){
        for(i=0;i<512;i++)io_data[i]=0;
        if(!s){for(i=0;i<11;i++)io_data[i]=io_data[32+i]=' ';
            io_data[0]=io_data[32]=io_data[33]='.';io_data[11]=io_data[43]=16;
            put16(io_data+26,c);put16(io_data+58,parent);}
        if(!kblock_write(data_start+(uint32_t)(c-2)*cluster_physical_sectors+s,1,io_data)){r=-FAT_WRITE_IO;goto fail;}}
    r=publish_entry(lba,off,name,c,0,1);if(r)goto fail;return tx_commit();
fail:return rollback(r);
}
int kfat_delete_path(const char *path)
{
    char name[11];uint16_t parent,off,c,next,s,current;uint32_t lba;uint8_t attr;unsigned j;int r;
    r=mutation_parent(path,&parent,name);if(r)return r;
    r=entry_slot(parent,name,1,&lba,&off);if(r)return r;
    if(!kblock_read(lba,1,sector_buffer))return -FAT_WRITE_IO;
    attr=sector_buffer[off+11];c=le16(sector_buffer+off+26);
    if(attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
    if((attr&16)&&!valid_cluster(c))return -FAT_WRITE_INVALID;
    r=check_chain(c,0,0);if(r)return r;
    if(attr&16){current=c;do{for(s=0;s<cluster_physical_sectors;s++){
        if(!kblock_read(data_start+(uint32_t)(current-2)*cluster_physical_sectors+s,1,sector_buffer))return -FAT_WRITE_IO;
        for(j=0;j<512;j+=32){if(!sector_buffer[j])goto empty;
            if(sector_buffer[j]!=0xe5&&sector_buffer[j]!='.')return -FAT_WRITE_NOT_EMPTY;}}
        if(!read_fat_entry(current,&next))return -FAT_WRITE_IO;
        current=next;
    }while(current<0xfff8);}
empty:
    r=tx_begin();if(r)return r;
    if(!kblock_read(lba,1,sector_buffer)){r=-FAT_WRITE_IO;goto fail;}
    sector_buffer[off]=0xe5;r=tx_write(lba,sector_buffer);if(r)goto fail;
    while(c){if(!read_fat_entry(c,&next)){r=-FAT_WRITE_IO;goto fail;}r=set_fat(c,0);if(r)goto fail;if(next>=0xfff8)break;
        c=next;}
    return tx_commit();
fail:return rollback(r);
}

/* Return 1 for TRASH itself, 2 for its descendants, 0 for other paths. */
static int trash_scope(const char *path)
{char name[11];if(path_component(&path,name)!=1)return 0;
 return same_name((const uint8_t *)name,"TRASH      ")?(*path?2:1):0;}
int kfat_move_path(const char *source,const char *target)
{
    char sn[11],tn[11];uint16_t sp,tp,so,to,c,up;uint32_t sl,tl,dl;uint8_t saved[32],attr;unsigned i,depth;int r;
    if(trash_scope(source)==1||trash_scope(target)==1)return -FAT_WRITE_READ_ONLY;
    r=mutation_parent(source,&sp,sn);if(r)return r;
    r=mutation_parent(target,&tp,tn);if(r)return r;
    r=entry_slot(sp,sn,1,&sl,&so);if(r)return r;
    if(!kblock_read(sl,1,sector_buffer))return -FAT_WRITE_IO;
    for(i=0;i<32;i++)saved[i]=sector_buffer[so+i];
    attr=saved[11];c=le16(saved+26);
    if(attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
    if(attr&16){
        if(!valid_cluster(c))return -FAT_WRITE_INVALID;
        /* Follow destination ancestry before changing anything: no cycles. */
        up=tp;for(depth=0;up;depth++){
            if(depth>=16||!valid_cluster(up)||up==c)return -FAT_WRITE_INVALID;
            if(!kblock_read(data_start+(uint32_t)(up-2)*cluster_physical_sectors,1,sector_buffer))return -FAT_WRITE_IO;
            if(!same_name(sector_buffer,".          ")||!same_name(sector_buffer+32,"..         ")||
               !(sector_buffer[11]&16)||!(sector_buffer[43]&16)||le16(sector_buffer+26)!=up)return -FAT_WRITE_INVALID;
            up=le16(sector_buffer+58);
        }
        dl=data_start+(uint32_t)(c-2)*cluster_physical_sectors;
        if(!kblock_read(dl,1,sector_buffer))return -FAT_WRITE_IO;
        if(!same_name(sector_buffer,".          ")||!same_name(sector_buffer+32,"..         ")||
           !(sector_buffer[11]&16)||!(sector_buffer[43]&16)||le16(sector_buffer+26)!=c||le16(sector_buffer+58)!=sp)return -FAT_WRITE_INVALID;
    }else dl=0;
    r=entry_slot(tp,tn,0,&tl,&to);if(r)return r;
    r=tx_begin();if(r)return r;
    if(dl&&sp!=tp){if(!kblock_read(dl,1,sector_buffer)){r=-FAT_WRITE_IO;goto failed;}
        put16(sector_buffer+58,tp);r=tx_write(dl,sector_buffer);if(r)goto failed;}
    if(!kblock_read(tl,1,sector_buffer)){r=-FAT_WRITE_IO;goto failed;}
    for(i=0;i<32;i++)sector_buffer[to+i]=saved[i];
    for(i=0;i<11;i++)sector_buffer[to+i]=(uint8_t)tn[i];
    r=tx_write(tl,sector_buffer);if(r)goto failed;
    if(!kblock_read(sl,1,sector_buffer)){r=-FAT_WRITE_IO;goto failed;}
    sector_buffer[so]=0xe5;r=tx_write(sl,sector_buffer);if(r)goto failed;
    return tx_commit();
failed:return rollback(r);
}
int kfat_trash_path(const char *source)
{
    char name[11],display[13],bucket[17],target[32];
    uint16_t parent,c;uint32_t size,id;uint8_t attr;unsigned i,j;int r;enum fat_file_state state;
    for(i=0;i<16;i++)bucket[i]="\\TRASH\\T0000000"[i];
    if(trash_scope(source))return -FAT_WRITE_READ_ONLY;
    r=mutation_parent(source,&parent,name);if(r)return r;
    state=resolve_path(source,&c,&size,&attr);
    if(state!=FAT_FILE_FOUND)return state==FAT_FILE_IO_FAILED?-FAT_WRITE_IO:-FAT_WRITE_INVALID;
    if(attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
    state=resolve_path("\\TRASH",&c,&size,&attr);
    if(state==FAT_FILE_NOT_FOUND){r=kfat_mkdir_path("\\TRASH");if(r)return r;}
    else if(state!=FAT_FILE_FOUND)return -FAT_WRITE_IO;
    else if(!(attr&16)||(attr&(1|4|8)))return -FAT_WRITE_READ_ONLY;
    for(id=1;id<65536u;id++){
        uint32_t value=id;for(i=0;i<7;i++){unsigned digit=value&15u;bucket[14-i]=(char)(digit<10?'0'+digit:'A'+digit-10);value>>=4;}
        state=resolve_path(bucket,&c,&size,&attr);
        if(state==FAT_FILE_NOT_FOUND)break;
        if(state!=FAT_FILE_FOUND)return -FAT_WRITE_IO;
    }
    if(id==65536u)return -FAT_WRITE_FULL;
    r=kfat_mkdir_path(bucket);if(r)return r;
    display_name((const uint8_t *)name,display);
    for(i=0;bucket[i];i++)target[i]=bucket[i];
    target[i++]='\\';
    for(j=0;display[j];j++)target[i++]=display[j];
    target[i]=0;
    r=kfat_move_path(source,target);
    /* On a move failure, leave the empty bucket visible. Do not hide a
       second I/O failure while trying to clean up after a failed operation. */
    return r;
}

/* Bounded traversal: preflight the entire subtree, then delete bottom-up.
 * No recursive C stack and no directory entry pointers survive another read. */
static struct {unsigned wanted,seen;char name[13];uint8_t attr;int found;} purge_pick;
static int pick_entry(const char *name,uint32_t size,uint32_t attr,void *context)
{unsigned i;(void)size;(void)context;if(purge_pick.seen++!=purge_pick.wanted)return 1;
 for(i=0;i<13;i++){purge_pick.name[i]=name[i];if(!name[i])break;}
 purge_pick.attr=(uint8_t)attr;purge_pick.found=1;return 0;}
static int purge_walk(const char *source,int erase)
{
    char path[64];unsigned offsets[17],indices[17],depth=0,n=0,total=0,j;uint16_t c;uint32_t size;uint8_t attr;int r;
    while(source[n]){if(n>=63)return -FAT_WRITE_LIMIT;path[n]=source[n];n++;}path[n]=0;
    if(resolve_path(path,&c,&size,&attr)!=FAT_FILE_FOUND)return -FAT_WRITE_INVALID;
    if(attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
    if(!(attr&16))return erase?kfat_delete_path(path):check_chain(c,0,0);
    offsets[0]=n;indices[0]=0;
    for(;;){
        if(++total>4096u)return -FAT_WRITE_LIMIT;
        purge_pick.wanted=erase?0:indices[depth]++;purge_pick.seen=0;purge_pick.found=0;
        if(kfat_list_path(path,pick_entry,0)!=FAT_FILE_FOUND)return -FAT_WRITE_IO;
        if(!purge_pick.found){
            if(erase){r=kfat_delete_path(path);if(r)return r;}
            if(!depth)return 0;
            depth--;n=offsets[depth];path[n]=0;continue;
        }
        if(purge_pick.attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
        n=offsets[depth];if(n+1>=63)return -FAT_WRITE_LIMIT;path[n++]='\\';
        for(j=0;purge_pick.name[j];j++){if(n>=63)return -FAT_WRITE_LIMIT;path[n++]=purge_pick.name[j];}path[n]=0;
        if(resolve_path(path,&c,&size,&attr)!=FAT_FILE_FOUND)return -FAT_WRITE_INVALID;
        r=check_chain(c,0,0);if(r)return r;
        if(attr&16){if(depth>=15)return -FAT_WRITE_LIMIT;depth++;offsets[depth]=n;indices[depth]=0;}
        else{if(erase){r=kfat_delete_path(path);if(r)return r;}n=offsets[depth];path[n]=0;}
    }
}
int kfat_purge_path(const char *source)
{int r;if(trash_scope(source)!=2)return -FAT_WRITE_READ_ONLY;
 r=purge_walk(source,0);return r?r:purge_walk(source,1);}
