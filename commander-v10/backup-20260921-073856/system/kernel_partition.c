#include "kernel_partition.h"
#include "kernel_block.h"

#define AHDI_TABLE_OFFSET 0x1c6u
#define AHDI_ENTRY_SIZE 12u

typedef struct {char id[4];uint32_t start,size;uint8_t accessible;} volume_t;
static volume_t volumes[KPART_MAX_VOLUMES];
static uint8_t volume_count,boot_volume,scan_truncated;
static uint8_t sector_buffer[512];

static uint32_t be32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static int id_is(const uint8_t *entry,char a,char b,char c)
{return entry[1]==(uint8_t)a&&entry[2]==(uint8_t)b&&entry[3]==(uint8_t)c;}
static int volume_id(const uint8_t *entry)
{return id_is(entry,'G','E','M')||id_is(entry,'B','G','M')||id_is(entry,'F','3','2');}
static int add_volume(const uint8_t *entry,uint32_t base,uint8_t root_slot)
{
    uint32_t relative=be32(entry+4),size=be32(entry+8),start;
    if(!relative||!size||relative>0xffffffffUL-base)return 0;
    start=base+relative;if(size-1u>0xffffffffUL-start)return 0;
    if(volume_count>=KPART_MAX_VOLUMES){scan_truncated=1;return 1;}
    volumes[volume_count].id[0]=(char)entry[1];volumes[volume_count].id[1]=(char)entry[2];
    volumes[volume_count].id[2]=(char)entry[3];volumes[volume_count].id[3]='\0';
    volumes[volume_count].start=start;volumes[volume_count].size=size;
    volumes[volume_count].accessible=1;
    if(root_slot!=0xffu&&root_slot==boot_volume)boot_volume=volume_count;
    ++volume_count;return 1;
}

static enum partition_state scan_xgm(uint32_t chain_base)
{
    uint32_t descriptor=chain_base,visited[KPART_MAX_VOLUMES];uint8_t visits=0;
    if(!chain_base)return KPART_INVALID;
    while(descriptor){const uint8_t *logical,*link;uint32_t next,relative;uint8_t i;
        if(visits>=KPART_MAX_VOLUMES){scan_truncated=1;return KPART_READY;}
        for(i=0;i<visits;++i)if(visited[i]==descriptor)return KPART_INVALID;
        visited[visits++]=descriptor;
        if(!kblock_read(descriptor,1u,sector_buffer))return KPART_IO_FAILED;
        logical=sector_buffer+AHDI_TABLE_OFFSET;link=logical+AHDI_ENTRY_SIZE;
        if((logical[0]&1u)&&volume_id(logical)&&!add_volume(logical,descriptor,0xffu))return KPART_INVALID;
        if(!(link[0]&1u)||!id_is(link,'X','G','M'))break;
        relative=be32(link+4);if(!relative||relative>0xffffffffUL-chain_base)return KPART_INVALID;
        next=chain_base+relative;descriptor=next;
    }
    return KPART_READY;
}

enum partition_state kpart_scan(uint8_t boot_root_slot)
{
    uint8_t root[512],slot;enum partition_state state;
    volume_count=0;boot_volume=boot_root_slot;scan_truncated=0;
    if(boot_root_slot>=4u)return KPART_INVALID;
    if(!kblock_read(0,1u,root))return KPART_IO_FAILED;
    for(slot=0;slot<4u;++slot){const uint8_t *entry=root+AHDI_TABLE_OFFSET+(uint16_t)slot*AHDI_ENTRY_SIZE;
        if(!(entry[0]&1u))continue;
        if(id_is(entry,'X','G','M')){state=scan_xgm(be32(entry+4));if(state!=KPART_READY)return state;}
        else if(volume_id(entry)&&!add_volume(entry,0,slot))return KPART_INVALID;
    }
    if(boot_volume>=volume_count||!volumes[boot_volume].accessible)return KPART_INVALID;
    return KPART_READY;
}

uint8_t kpart_count(void){return volume_count;}
int kpart_truncated(void){return scan_truncated;}
const char *kpart_id(uint8_t index){return index<volume_count?volumes[index].id:"???";}
uint32_t kpart_size(uint8_t index){return index<volume_count?volumes[index].size:0;}
uint32_t kpart_start(uint8_t index){return index<volume_count?volumes[index].start:0;}
int kpart_accessible(uint8_t index){return index<volume_count&&volumes[index].accessible;}
int kpart_is_boot(uint8_t index){return index<volume_count&&index==boot_volume;}
uint32_t kpart_boot_start(void){return volumes[boot_volume].start;}
const char *kpart_boot_id(void){return volumes[boot_volume].id;}
