#include "kernel_fat.h"
#include "kernel_block.h"
#include "kernel_memory.h"
#include "kernel_sessions.h"   /* SESSION_COUNT for the per-console mounts */

static uint8_t sector_buffer[512];
/* chain_seen to bitmapa lancucha FAT uzywana tylko przez CPU (bez DMA i
   przerwan), wiec trzymamy ja w TT RAM - w ST zostaje tylko io_data,
   ktora uczestniczy w przesylach sektorow. */
#define CHAIN_SEEN_BYTES 8192u
static uint8_t *chain_seen;      /* alokowane w kfat_mount (prefer TT) */
/* Per-console mount state (v10: independent mounts per session).  Every FAT
   access works on the active context; kfat_select_session() switches it and
   follows the context's block transport, so two consoles can sit on different
   volumes.  The macro aliases keep the rest of the file unchanged. */
typedef struct {
    char id[4];
    uint16_t sector_size;
    uint32_t volume_start,fat_start,root_start,data_start,cluster_count;
    uint16_t root_sectors,cluster_sectors;
    enum fat_health health;
    uint32_t fat_sectors;
    uint8_t fat_copies,fat_bits,device,pad;
} fat_mount_t;
static fat_mount_t fat_mounts[SESSION_COUNT];
static fat_mount_t *fat=&fat_mounts[0];
#define partition_id            (fat->id)
#define logical_sector_size     (fat->sector_size)
#define volume_start            (fat->volume_start)
#define fat_start               (fat->fat_start)
#define root_start              (fat->root_start)
#define data_start              (fat->data_start)
#define cluster_count           (fat->cluster_count)
#define root_physical_sectors   (fat->root_sectors)
#define cluster_physical_sectors (fat->cluster_sectors)
#define volume_health           (fat->health)
#define fat_sectors             (fat->fat_sectors)
#define fat_copies              (fat->fat_copies)
#define fat_bits                (fat->fat_bits)
static int write_failed;

static uint16_t le16(const uint8_t *p)
{return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int power_of_two(uint16_t value)
{return value&&!(value&(uint16_t)(value-1u));}
/* FAT12 marks end-of-chain at 0xFF8, FAT16 at 0xFFF8; reserved/bad similarly. */
static int fat_is_eoc(uint16_t v){return v>=(fat_bits==12u?0x0ff8u:0xfff8u);}
static int fat_is_bad(uint16_t v){return v==(fat_bits==12u?0x0ff7u:0xfff7u);}

const char *kfat_partition_id(void){return partition_id;}
uint16_t kfat_sector_size(void){return logical_sector_size;}
enum fat_health kfat_health(void){return volume_health;}
/* Switch the active mount context to a console's own volume.  Also follows the
   transport, but only when the device actually changes (kblock_select clears
   the sector cache, so a no-op call would thrash it).  Returns whether that
   console currently has a USABLE volume: a failed transport switch leaves the
   block layer not-ready, so the context must not be reported as mounted - the
   caller would otherwise read the failure as "file not found". */
int kfat_select_session(unsigned session)
{
    if(session>=SESSION_COUNT)session=0;
    if(fat!=&fat_mounts[session])fat=&fat_mounts[session];
    if(!fat->sector_size)return 0;
    /* Follow the session's transport.  A select is attempted when the device
       differs OR the block layer is not ready - the failed select above leaves
       block_device pointing at the unreachable device, so comparing devices
       alone would report a dead transport as usable.  A no-op call (same
       device, ready) is still avoided: kblock_select clears the sector cache. */
    if(fat->device!=kblock_device()||!kblock_is_ready())
        if(!kblock_select(fat->device))return 0;
    return 1;
}

static int same_name(const uint8_t *entry,const char name[11])
{uint8_t i;for(i=0;i<11u;++i)if(entry[i]!=(uint8_t)name[i])return 0;return 1;}

static uint8_t fat_straddle[512];
static int read_fat_entry(uint16_t cluster,uint16_t *next)
{
    ++kfat_counts[0];
    if(fat_bits==12u){
        /* FAT12 packs 1.5 bytes per entry; an entry may straddle a sector. */
        uint32_t offset=(uint32_t)cluster+(cluster>>1);
        uint16_t off=(uint16_t)(offset&511u),v;uint8_t hi;
        if(!kblock_read(fat_start+(offset>>9),1u,sector_buffer))return 0;
        if(off==511u){if(!kblock_read(fat_start+(offset>>9)+1u,1u,fat_straddle))return 0;hi=fat_straddle[0];}
        else hi=sector_buffer[off+1u];
        v=(uint16_t)((uint16_t)sector_buffer[off]|((uint16_t)hi<<8));
        *next=(cluster&1u)?(uint16_t)(v>>4):(uint16_t)(v&0x0fffu);
        return 1;
    }
    {uint32_t offset=(uint32_t)cluster*2u;
     if(!kblock_read(fat_start+offset/512u,1u,sector_buffer))return 0;
     *next=le16(sector_buffer+(uint16_t)(offset&511u));return 1;}
}

int kfat_tos_bpb(uint16_t words[9])
{
    uint32_t scale,v[9];unsigned i;
    if(!words||!logical_sector_size||!fat_copies)return 0;
    scale=logical_sector_size/512u;
    if(!scale)return 0;
    v[0]=logical_sector_size;
    v[1]=cluster_physical_sectors/scale;
    v[2]=(uint32_t)cluster_physical_sectors*512u;
    v[3]=root_physical_sectors/scale;
    v[4]=fat_sectors/scale;
    v[5]=(fat_start-volume_start+fat_sectors*(fat_copies-1u))/scale;
    v[6]=(data_start-volume_start)/scale;
    v[7]=cluster_count;
    v[8]=(fat_bits==16u?1u:0u)|(fat_copies==1u?2u:0u);
    for(i=0;i<9u;i++)if(v[i]>65535u)return 0;
    for(i=0;i<9u;i++)words[i]=(uint16_t)v[i];
    return 1;
}
int kfat_free_clusters(uint32_t *count)
{
    uint32_t c,n=0;uint16_t entry;
    if(!count||!logical_sector_size||cluster_count>65524u)return 0;
    for(c=2u;c<cluster_count+2u;c++){
        if(!read_fat_entry((uint16_t)c,&entry))return 0;
        if(!entry)++n;
    }
    *count=n;return 1;
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
        if(fat_is_eoc(next))return FAT_FILE_NOT_FOUND;
        if(next<2u||fat_is_bad(next))return FAT_FILE_CORRUPT;
        cluster=next;
        if(!fat_is_eoc(slow)&&!read_fat_entry(slow,&slow))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(slow)&&!fat_is_eoc(fast)&&slow==fast)return FAT_FILE_CORRUPT;
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
        if(index+1u<needed){if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;cluster=next;}
        else if(!fat_is_eoc(next))return FAT_FILE_CORRUPT;
    }
    if(remaining)return FAT_FILE_CORRUPT;
    *size=file_size;
    return FAT_FILE_FOUND;
}

/* ---- driver directory (\SYSTEM\DRV) ----------------------------------------
   Graphics drivers live in their own directory, so adding a chip is a new file
   plus one manifest entry and never touches the rest of the tree.  The normal
   module loader reads \SYSTEM flat, so this pair walks one level deeper. */
static enum fat_file_state locate_in_directory(uint16_t directory,const char name[11],
                                               uint16_t *file_cluster,uint32_t *size)
{
    uint16_t cluster=directory,next,s,slow,fast;uint32_t i;
    if(directory<2u)return FAT_FILE_NOT_FOUND;
    slow=fast=cluster;
    for(i=0;i<256u;++i){
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
        for(s=0;s<cluster_physical_sectors;++s){
            uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;
            uint16_t found=0;enum fat_file_state state=scan_directory_sector(lba,name,0,&found,size);
            if(state==FAT_FILE_FOUND){if(file_cluster)*file_cluster=found;return state;}
            if(state==FAT_FILE_IO_FAILED)return state;
        }
        if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
        if(fat_is_eoc(next))return FAT_FILE_NOT_FOUND;
        if(next<2u||fat_is_bad(next))return FAT_FILE_CORRUPT;
        cluster=next;
        if(!fat_is_eoc(slow)&&!read_fat_entry(slow,&slow))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return FAT_FILE_IO_FAILED;
        if(!fat_is_eoc(slow)&&!fat_is_eoc(fast)&&slow==fast)return FAT_FILE_CORRUPT;
    }
    return FAT_FILE_CORRUPT;
}
/* Cluster of a directory \SYSTEM\<dir>, or 0 when it does not exist. */
static uint16_t system_subdirectory(const char dir[11])
{
    static const char system_name[11]={'S','Y','S','T','E','M',' ',' ',' ',' ',' '};
    uint16_t directory=0,cluster,next,s,slow,fast;uint32_t i;
    if(!logical_sector_size)return 0;
    for(i=0;i<root_physical_sectors;++i){
        enum fat_file_state st=scan_directory_sector(root_start+i,system_name,1,&directory,0);
        if(st==FAT_FILE_FOUND)break;
        if(st==FAT_FILE_IO_FAILED||st==FAT_FILE_NOT_FOUND)return 0;
    }
    if(!directory)return 0;
    cluster=slow=fast=directory;
    for(i=0;i<256u;++i){
        if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return 0;
        for(s=0;s<cluster_physical_sectors;++s){
            uint32_t lba=data_start+(uint32_t)(cluster-2u)*cluster_physical_sectors+s;
            uint16_t found=0;enum fat_file_state st=scan_directory_sector(lba,dir,1,&found,0);
            if(st==FAT_FILE_FOUND)return found;
            if(st==FAT_FILE_IO_FAILED)return 0;
        }
        if(!read_fat_entry(cluster,&next))return 0;
        if(fat_is_eoc(next))break;
        if(next<2u||fat_is_bad(next))return 0;
        cluster=next;
        if(!fat_is_eoc(slow)&&!read_fat_entry(slow,&slow))return 0;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return 0;
        if(!fat_is_eoc(fast)&&!read_fat_entry(fast,&fast))return 0;
        if(!fat_is_eoc(slow)&&!fat_is_eoc(fast)&&slow==fast)return 0;
    }
    return 0;
}
static uint16_t driver_directory(void)
{
    static const char drv_name[11]={'D','R','V',' ',' ',' ',' ',' ',' ',' ',' '};
    return system_subdirectory(drv_name);
}
enum fat_file_state kfat_find_driver_file(const char name[11],uint32_t *size)
{
    uint16_t dir=driver_directory();
    if(!dir)return FAT_FILE_NOT_FOUND;
    return locate_in_directory(dir,name,0,size);
}
enum fat_file_state kfat_load_driver_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size)
{
    uint16_t cluster,next,s;uint32_t file_size,remaining,needed,index;uint8_t *out=(uint8_t *)destination;
    uint16_t dir=driver_directory();
    enum fat_file_state state;
    if(!dir)return FAT_FILE_NOT_FOUND;
    state=locate_in_directory(dir,name,&cluster,&file_size);
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
        if(index+1u<needed){if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;cluster=next;}
        else if(!fat_is_eoc(next))return FAT_FILE_CORRUPT;
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
        if(fat_is_eoc(next))return FAT_FILE_NOT_FOUND;
        if(next<2u||fat_is_bad(next))return FAT_FILE_CORRUPT;
        current=next;
    }
    return FAT_FILE_CORRUPT;
}

static enum fat_file_state resolve_path(const char *path,uint16_t *cluster,
                                        uint32_t *size,uint8_t *attributes)
{
    char name[11];uint16_t directory=0,found=0;uint32_t found_size=0;uint8_t attr=0,depth=0;int part;
    enum fat_file_state state;if(!logical_sector_size||!path)return FAT_FILE_CORRUPT;
    /* Accept an optional drive prefix ("C:\...") and resolve it on the mounted
       volume: the FAT layer owns one volume and callers must be able to name it
       the TOS way without the kernel depending on the physical device. */
    if(((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))&&path[1]==':')path+=2;
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
        if(index+1u<needed){if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;cluster=next;}else if(!fat_is_eoc(next))return FAT_FILE_CORRUPT;
    }
    if(remaining)return FAT_FILE_CORRUPT;
    *size=file_size;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_load_path(const char *path,void *destination,uint32_t capacity,uint32_t *size)
{uint16_t cluster;uint32_t file_size;uint8_t attr;enum fat_file_state state=resolve_path(path,&cluster,&file_size,&attr);if(state!=FAT_FILE_FOUND)return state;if(attr&0x10u)return FAT_FILE_CORRUPT;return load_cluster_file(cluster,file_size,destination,capacity,size);}

/* Diagnostics: [0]=FAT entries read, [1]=reads served from the cursor,
   [2]=chain steps walked, [3]=peak entries in one call. */
uint32_t kfat_counts[4];

/* Resolve once and remember where the data is, so a continuation read neither
   searches the directory nor walks the chain from the first cluster.  `filled`
   says whether the cached start cluster/size may be trusted for this path. */
struct fat_cursor_state {uint16_t cluster,start;uint32_t offset;uint32_t size;uint8_t valid;};

static enum fat_file_state read_at_cursor(const char *path,uint32_t offset,
                                          void *destination,uint32_t capacity,
                                          uint32_t *size,fat_cursor_t *cursor)
{
    uint16_t cluster=0,next,sector;
    uint32_t file_size=0,cluster_bytes,within,remaining,requested,walked=0;
    uint8_t attr=0,*out=(uint8_t *)destination;enum fat_file_state state;
    int used_cache=0;
    uint32_t entries_at_entry=kfat_counts[0];

    if(cursor&&cursor->valid&&offset>=cursor->offset&&cursor->offset<=cursor->size){
        /* Continuation inside a file we have already located. */
        cluster=cursor->cluster;file_size=cursor->size;used_cache=1;
    }else{
        state=resolve_path(path,&cluster,&file_size,&attr);
        if(state!=FAT_FILE_FOUND)return state;
        if(!size||(attr&0x10u)||offset>file_size)return FAT_FILE_CORRUPT;
        if(cursor){cursor->start_cluster=cluster;cursor->size=file_size;}
    }
    if(offset>file_size)return FAT_FILE_CORRUPT;
    requested=file_size-offset;
    if(requested>capacity)requested=capacity;
    if(!requested){*size=0;return FAT_FILE_FOUND;}
    if(!destination)return FAT_FILE_CORRUPT;
    cluster_bytes=(uint32_t)cluster_physical_sectors*512u;
    if(!cluster_bytes)return FAT_FILE_CORRUPT;

    /* How far into the chain must we step?  From the cursor position when we
       have one, from the file start otherwise. */
    {uint32_t from=used_cache?cursor->offset:0u;
     uint32_t skip_clusters=(offset/cluster_bytes)-(from/cluster_bytes);
     within=offset%cluster_bytes;
     while(skip_clusters--){
         if(cluster<2u||(uint32_t)cluster>cluster_count+1u)return FAT_FILE_CORRUPT;
         if(!read_fat_entry(cluster,&next))return FAT_FILE_IO_FAILED;
         ++kfat_counts[2];
         if(++walked>cluster_count)return FAT_FILE_CORRUPT;
         if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;
         cluster=next;
     }
    }
    if(used_cache)++kfat_counts[1];
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
            ++kfat_counts[2];
            if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;
            cluster=next;within=0;
            if(++walked>cluster_count)return FAT_FILE_CORRUPT;
        }
    }
    /* Remember where the NEXT byte lives, so the caller's next chunk continues
       here instead of walking the chain again.  cursor->cluster must be the
       cluster that CONTAINS cursor->offset: if the read ended exactly on a
       cluster boundary the next byte is in the following cluster, and stopping
       short of that step made the next call read the wrong cluster's data. */
    if(cursor){
        if(remaining==0&&within==cluster_bytes&&(offset+requested)<file_size){
            /* Only when the file actually continues: at end of file the next FAT
               entry is the end-of-chain marker, and following it reported the
               LAST chunk of every file as corrupt. */
            uint16_t follow;
            ++kfat_counts[2];
            if(!read_fat_entry(cluster,&follow))return FAT_FILE_IO_FAILED;
            if(follow<2u||fat_is_eoc(follow)||fat_is_bad(follow))return FAT_FILE_CORRUPT;
            cluster=follow;
        }
        cursor->offset=offset+requested;
        cursor->cluster=cluster;
        cursor->valid=1;
    }
    {uint32_t used=kfat_counts[0]-entries_at_entry;
     if(used>kfat_counts[3])kfat_counts[3]=used;}
    *size=requested;
    return FAT_FILE_FOUND;
}

enum fat_file_state kfat_read_cursor(const char *path,uint32_t offset,
                                     void *destination,uint32_t capacity,
                                     uint32_t *size,fat_cursor_t *cursor)
{return read_at_cursor(path,offset,destination,capacity,size,cursor);}

enum fat_file_state kfat_read_path_at(const char *path,uint32_t offset,
                                      void *destination,uint32_t capacity,uint32_t *size)
{
    /* No cursor: behaves exactly as before, and shares the implementation with
       the cursor path so the two cannot drift apart. */
    return read_at_cursor(path,offset,destination,capacity,size,0);
}

static void display_name(const uint8_t *entry,char name[13])
{uint8_t i,n=0;for(i=0;i<8u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];if(entry[8]!=' '){name[n++]='.';for(i=8;i<11u&&entry[i]!=' ';++i)name[n++]=(char)entry[i];}name[n]='\0';}

static int list_sector(uint32_t lba,kfat_list_callback_t callback,void *context)
{uint8_t i;if(!kblock_read(lba,1u,sector_buffer))return -1;for(i=0;i<16u;++i){const uint8_t *e=sector_buffer+(uint16_t)i*32u;char name[13];if(e[0]==0)return 0;if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu||(e[11]&0x08u)||e[0]=='.')continue;display_name(e,name);if(!callback(name,le32(e+28),e[11],((uint32_t)le16(e+24)<<16)|le16(e+22),context))return 0;}return 1;}

enum fat_file_state kfat_list_path(const char *path,kfat_list_callback_t callback,void *context)
{
    uint16_t directory,next,s;uint32_t size,i;uint8_t attr;int result;enum fat_file_state state;
    if(!callback)return FAT_FILE_CORRUPT;
    state=resolve_path(path,&directory,&size,&attr);
    if(state!=FAT_FILE_FOUND)return state;
    if(!(attr&0x10u))return FAT_FILE_CORRUPT;
    if(!directory){for(i=0;i<root_physical_sectors;++i){result=list_sector(root_start+i,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}return FAT_FILE_FOUND;}
    for(i=0;i<256u;++i){if(directory<2u||(uint32_t)directory>cluster_count+1u)return FAT_FILE_CORRUPT;for(s=0;s<cluster_physical_sectors;++s){result=list_sector(data_start+(uint32_t)(directory-2u)*cluster_physical_sectors+s,callback,context);if(result<0)return FAT_FILE_IO_FAILED;if(!result)return FAT_FILE_FOUND;}if(!read_fat_entry(directory,&next))return FAT_FILE_IO_FAILED;if(fat_is_eoc(next))return FAT_FILE_FOUND;if(next<2u||fat_is_bad(next))return FAT_FILE_CORRUPT;directory=next;}
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
    if(start>0x001fffffUL||!id)return FAT_PARTITION_INVALID;   /* 0 = floppy */
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
    if(clusters<2u||clusters>=65525u)return FAT_UNSUPPORTED;   /* low count: boot floppy */
    logical_sector_size=bytes_per_sector;volume_start=start;
    fat_start=volume_start+(uint32_t)reserved*(bytes_per_sector/512u);
    root_start=fat_start+(uint32_t)fats*sectors_per_fat*(bytes_per_sector/512u);
    root_physical_sectors=(uint16_t)(root_sectors*(bytes_per_sector/512u));
    data_start=root_start+root_physical_sectors;
    cluster_physical_sectors=(uint16_t)((uint16_t)sectors_per_cluster*(bytes_per_sector/512u));
    cluster_count=clusters;fat_sectors=(uint32_t)sectors_per_fat*(bytes_per_sector/512u);fat_copies=fats;write_failed=0;
    fat_bits=(uint8_t)(clusters<4085u?12u:16u);   /* Microsoft rule: < 4085 clusters = FAT12 */
    if((uint32_t)clusters+2u>(fat_bits==12u?(fat_sectors*1024u)/3u:fat_sectors*256u)){logical_sector_size=0;return FAT_BPB_INVALID;}
    if(!kblock_read(fat_start,1u,sector_buffer)){logical_sector_size=0;return FAT_IO_FAILED;}
    {uint16_t flags=le16(sector_buffer+2);if(!(flags&0x4000u))volume_health=FAT_HEALTH_HARD_ERROR;else if(!(flags&0x8000u))volume_health=FAT_HEALTH_DIRTY;else volume_health=FAT_HEALTH_CLEAN;}
    fat->device=kblock_device();   /* remember the transport for this console */
    /* chain_seen jest potrzebna w sciezkach zapisu (check_chain) CZESTO PRZED
       pierwszym tx_write, wiec nie wolno czekac z alokacja do transakcji.
       Bufor jest tylko roboczy (CPU, bez DMA i przerwan) - idzie do TT. */
    if(!chain_seen){chain_seen=kmem_alloc(CHAIN_SEEN_BYTES,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
        if(!chain_seen){logical_sector_size=0;return -FAT_WRITE_LIMIT;}}
    return FAT_MOUNTED;
}

/* Bounded metadata undo journal. Data is written only to newly allocated
 * clusters, and the directory entry is published last. This is not an
 * on-disk crash journal: interrupted writes leave the FAT dirty. */
#define TX_SECTORS 128u
#define COPY_LIMIT (1024u*1024u)
static struct {uint32_t lba;uint8_t old[512];} *undo; /* 66 KB, only for writes */
static unsigned undo_count;
static uint8_t io_data[512];
static void put16(uint8_t *p,uint16_t v){p[0]=v;p[1]=v>>8;}
static void put32(uint8_t *p,uint32_t v){put16(p,v);put16(p+2,v>>16);}
static int valid_cluster(uint16_t c){return c>=2u&&c<0xfff0u&&(uint32_t)c<cluster_count+2u;}
static int tx_write(uint32_t lba,const uint8_t *data)
{
    unsigned i;
    if(!undo){undo=kmem_alloc(TX_SECTORS*(uint32_t)sizeof(*undo),16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,0);if(!undo)return -FAT_WRITE_LIMIT;}
    if(!chain_seen){chain_seen=kmem_alloc(CHAIN_SEEN_BYTES,16u,KMEM_OWNER_KERNEL,KMEM_READ|KMEM_WRITE,1);
        if(!chain_seen)return -FAT_WRITE_LIMIT;}   /* 1 = najpierw TT (siatka) */
    for(i=0;i<undo_count;i++)if(undo[i].lba==lba)break;
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
    if(!logical_sector_size||write_failed||volume_health!=FAT_HEALTH_CLEAN||fat_bits==12u)return -FAT_WRITE_READ_ONLY;
    undo_count=0;if(!read_fat_entry(1,&flags))return -FAT_WRITE_IO;
    r=set_fat(1,(uint16_t)(flags&~0x8000u));return r?rollback(r):0;
}
static int tx_commit(void)
{
    uint16_t flags;int r;if(!read_fat_entry(1,&flags))return rollback(-FAT_WRITE_IO);
    r=set_fat(1,(uint16_t)(flags|0x8000u));if(r)return rollback(r);undo_count=0;return 0;
}
/* Naprawa znacznika czystosci woluminu - TYLKO na wyraźne zadanie uzytkownika.
   Minimalna i ostrozna: (1) obie kopie FAT musza byc bajt w bajt identyczne,
   (2) FAT[0] musi miec sensowny znacznik nosnika, (3) dopiero wtedy ustawiamy
   w FAT[1] bity "clean" (15) i "no error" (14) we wszystkich kopiach.
   To NIE jest fsck: identyczne kopie FAT nie dowodza spojnosci lancuchow po
   przerwanym zapisie, dlatego nie robimy tego automatycznie przy starcie.
   Zwraca 0 przy powodzeniu albo ujemny kod FAT_WRITE_*. */
int kfat_volume_repair(void)
{
    uint16_t media;uint32_t i;unsigned k,f;
    if(!logical_sector_size)return -FAT_WRITE_READ_ONLY;
    if(write_failed)return -FAT_WRITE_IO;
    if(fat_bits==12u)return -FAT_WRITE_READ_ONLY;
    if(fat_copies>1u){
        for(i=0;i<fat_sectors;i++){
            if(!kblock_read(fat_start+i,1u,sector_buffer))return -FAT_WRITE_IO;
            if(!kblock_read(fat_start+fat_sectors+i,1u,io_data))return -FAT_WRITE_IO;
            for(k=0;k<512u;k++)if(sector_buffer[k]!=io_data[k])return -FAT_WRITE_EXISTS;
        }
    }
    if(!kblock_read(fat_start,1u,sector_buffer))return -FAT_WRITE_IO;
    media=le16(sector_buffer);
    if((media&0xff00u)!=0xff00u&&media!=0xfff8u)return -FAT_WRITE_INVALID;
    for(f=0;f<fat_copies;f++){
        uint32_t lba=fat_start+f*fat_sectors;      /* FAT[1] = pierwszy sektor kazdej kopii */
        if(!kblock_read(lba,1u,sector_buffer))return -FAT_WRITE_IO;
        put16(sector_buffer+2,(uint16_t)(le16(sector_buffer+2)|0x8000u|0x4000u));
        if(!kblock_write(lba,1u,sector_buffer)){write_failed=1;volume_health=FAT_HEALTH_HARD_ERROR;return -FAT_WRITE_IO;}
    }
    volume_health=FAT_HEALTH_CLEAN;
    return 0;
}
static int mutation_parent(const char *path,uint16_t *parent,char name[11])
{
    uint16_t c=0;uint32_t size;uint8_t attr,depth=0;int part;enum fat_file_state r;
    if(!path||!logical_sector_size)return -FAT_WRITE_INVALID;
    if(((path[0]>='A'&&path[0]<='Z')||(path[0]>='a'&&path[0]<='z'))&&path[1]==':')path+=2;
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
    if(!chain_seen)return -FAT_WRITE_IO;   /* brak alokacji = brak zapisu */
    for(i=0;i<CHAIN_SEEN_BYTES;i++)chain_seen[i]=0;
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
    /* Create only: an existing target reports -FAT_WRITE_EXISTS, matching the
       header contract and the Commander's CMD_ERR_EXISTS path. */
    r=entry_slot(parent,name,0,&lba,&off);if(r)return r;
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
int kfat_save_path(const char *path,const void *source,uint32_t size)
{
    char name[11];uint32_t lba;uint16_t parent,off,c=0,first=0,previous=0,cursor=2,s,old=0,next;uint32_t remaining;const uint8_t *in=(const uint8_t *)source;uint8_t attr;int r;
    if(!source&&size)return -FAT_WRITE_INVALID;
    r=mutation_parent(path,&parent,name);if(r)return r;
    r=entry_slot(parent,name,1,&lba,&off);
    if(r==0){
        if(!kblock_read(lba,1,sector_buffer))return -FAT_WRITE_IO;
        old=le16(sector_buffer+off+26);attr=sector_buffer[off+11];
        if(attr&(1|4|8))return -FAT_WRITE_READ_ONLY;
        if(attr&16)return -FAT_WRITE_READ_ONLY;
        r=check_chain(old,0,0);if(r)return r;
    }else if(r==-FAT_WRITE_INVALID){
        r=entry_slot(parent,name,0,&lba,&off);if(r)return r;
    }else return r;
    r=tx_begin();if(r)return r;
    while(old){if(!read_fat_entry(old,&next)){r=-FAT_WRITE_IO;goto fail;}r=set_fat(old,0);if(r)goto fail;if(next>=0xfff8)break;old=next;}
    remaining=size;
    while(remaining){
        r=alloc_cluster(&c,&cursor);if(r)goto fail;
        if(previous){r=set_fat(previous,c);if(r)goto fail;}else first=c;
        for(s=0;s<cluster_physical_sectors;s++){
            uint32_t amount=remaining<512u?remaining:512u;unsigned i;
            for(i=0;i<512u;i++)io_data[i]=(uint8_t)(i<amount?in[i]:0);
            if(!kblock_write(data_start+(uint32_t)(c-2u)*cluster_physical_sectors+s,1,io_data)){r=-FAT_WRITE_IO;goto fail;}
            in+=amount;remaining-=amount;
        }
        previous=c;
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
        if(next<2u||fat_is_eoc(next)||fat_is_bad(next))return FAT_FILE_CORRUPT;
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
             if(next<2u||fat_is_eoc(next)){rollback(-FAT_WRITE_IO);return FAT_FILE_CORRUPT;}
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
static int pick_entry(const char *name,uint32_t size,uint32_t attr,uint32_t datetime,void *context)
{unsigned i;(void)size;(void)datetime;(void)context;if(purge_pick.seen++!=purge_pick.wanted)return 1;
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