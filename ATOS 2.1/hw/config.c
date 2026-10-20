#include "hardware.h"
#include "console.h"
#include "system.h"
#include "config.h"
#include "nvram.h"
#include "block_device.h"

bios_config_t g_config;
uint8_t g_config_loaded;
/* Diagnostics for the configuration search: the console line printed by main()
   reports these, so a "config ignored" symptom can be traced to the exact step
   (probe / inspect / mount / directory / read) without guessing. */
uint8_t g_dbg_cfg_device,g_dbg_cfg_probe,g_dbg_cfg_inspect,g_dbg_cfg_part,
        g_dbg_cfg_mounted,g_dbg_cfg_dir,g_dbg_cfg_read,g_dbg_cfg_error;
uint8_t g_config_saved;
uint8_t g_config_error;

/* \BIOS\BIOS.CFG on the boot volume; a root \BIOS.CFG is still read for
   compatibility with older volumes. */
static const char config_name[11]={'B','I','O','S',' ',' ',' ',' ','C','F','G'};
static const char bios_dir_name[11]={'B','I','O','S',' ',' ',' ',' ',' ',' ',' '};

static uint8_t  cfg_id;
static uint32_t cfg_part,cfg_fat_start,cfg_root_start,cfg_data_start,cfg_cluster_count;
static uint16_t cfg_root_entries,cfg_spf;
static uint8_t cfg_fat_copies;
static uint8_t  cfg_spc,cfg_mounted;
static uint8_t  cfg_sector[512];
static uint16_t cfg_bios_cluster;

static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

static uint8_t cfg_find_boot(void)
{
    uint8_t id,u;
    for(id=0u;id<8u;++id){
        if(block_probe(id)){
            g_system.acsi_devices=(uint8_t)(g_system.acsi_devices|(uint8_t)(1u<<id));
            if(acsi_inspect_device(&g_system,id))return id;
        }
    }
    for(u=0u;u<2u;++u){
        uint8_t di=BLOCK_IDE_ID(u);
        if(block_probe(di)){
            g_system.ide_units=(uint8_t)(g_system.ide_units|(uint8_t)(1u<<u));
            if(acsi_inspect_device(&g_system,di))return di;
        }
    }
    return 0xffu;
}

static uint8_t cfg_boot_partition(void)
{
    uint8_t p;
    for(p=0u;p<4u;++p){if(g_system.acsi_partitions&(uint8_t)(1u<<p))return p;}
    return 0xffu;
}

static int cfg_mount(void)
{
    uint16_t bps,reserved,roots,spf,total16;uint32_t total32,total;
    if(!acsi_read_sector(cfg_id,cfg_part,cfg_sector))return 0;
    bps=le16(cfg_sector+11);
    if(bps!=512u)return 0;
    cfg_spc=cfg_sector[13];
    reserved=le16(cfg_sector+14);
    roots=le16(cfg_sector+17);
    spf=le16(cfg_sector+22);
    cfg_fat_copies=cfg_sector[16];cfg_spf=spf;
    if(!cfg_spc||!reserved||!roots||!spf||!cfg_fat_copies||cfg_fat_copies>2u)return 0;
    cfg_root_entries=roots;
    cfg_fat_start=cfg_part+(uint32_t)reserved;
    cfg_root_start=cfg_fat_start+(uint32_t)cfg_sector[16]*spf;
    cfg_data_start=cfg_root_start+(((uint32_t)roots*32u)+511u)/512u;
    total16=le16(cfg_sector+19);
    total32=le32(cfg_sector+32);
    total=total16?(uint32_t)total16:total32;
    if(total<=(cfg_data_start-cfg_part))return 0;
    cfg_cluster_count=(total-(cfg_data_start-cfg_part))/cfg_spc;
    cfg_mounted=(uint8_t)(cfg_cluster_count>2u);
    return cfg_mounted;
}

static int cfg_fat_get(uint16_t cluster,uint16_t *value)
{
    uint32_t offset=(uint32_t)cluster*2u;
    if(!acsi_read_sector(cfg_id,cfg_fat_start+offset/512u,cfg_sector))return 0;
    *value=le16(cfg_sector+(uint16_t)(offset&511u));
    return 1;
}

static int cfg_fat_put(uint16_t cluster,uint16_t value)
{
    uint32_t offset=(uint32_t)cluster*2u,lba;uint16_t at=(uint16_t)(offset&511u);uint8_t copy;
    for(copy=0;copy<cfg_fat_copies;copy++){
        lba=cfg_fat_start+(uint32_t)copy*cfg_spf+offset/512u;
        if(!acsi_read_sector(cfg_id,lba,cfg_sector))return 0;
        cfg_sector[at]=(uint8_t)value;
        cfg_sector[at+1u]=(uint8_t)(value>>8);
        if(!acsi_write_sector(cfg_id,lba,cfg_sector))return 0;
    }
    return 1;
}

static uint16_t cfg_alloc(void)
{
    uint32_t c;uint16_t value;
    for(c=2u;c<cfg_cluster_count+2u;++c){
        if(!cfg_fat_get((uint16_t)c,&value))return 0u;
        if(value==0u){
            if(!cfg_fat_put((uint16_t)c,0xffffu))return 0u;
            return (uint16_t)c;
        }
    }
    return 0u;
}

static uint16_t dir_cluster;
static uint32_t dir_lba,dir_cur_lba;
static uint16_t dir_left,dir_ent;

static void dir_open(uint16_t first)
{
    dir_cluster=first;dir_ent=16u;
    if(first==0u){
        dir_lba=cfg_root_start;
        dir_left=(uint16_t)(((uint32_t)cfg_root_entries*32u+511u)/512u);
    }else{
        dir_lba=cfg_data_start+((uint32_t)first-2u)*cfg_spc;
        dir_left=cfg_spc;
    }
    dir_cur_lba=dir_lba;
}

static const uint8_t *dir_next(void)
{
    for(;;){
        if(dir_ent>=16u){
            if(!dir_left){
                uint16_t next;
                if(dir_cluster==0u)return 0;
                if(!cfg_fat_get(dir_cluster,&next)||next<2u||next>=0xfff8u)return 0;
                dir_cluster=next;
                dir_lba=cfg_data_start+((uint32_t)next-2u)*cfg_spc;
                dir_left=cfg_spc;
            }
            if(!acsi_read_sector(cfg_id,dir_lba,cfg_sector))return 0;
            dir_cur_lba=dir_lba;dir_lba++;dir_left--;dir_ent=0u;
        }
        {const uint8_t *e=cfg_sector+((uint32_t)dir_ent*32u);dir_ent++;return e;}
    }
}

static int dir_find(uint16_t first,const char name[11],uint16_t *out_first,uint32_t *out_size)
{
    const uint8_t *e;uint8_t k;
    dir_open(first);
    while((e=dir_next())!=0){
        if(e[0]==0u)return 0;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu)continue;
        for(k=0u;k<11u;++k){if(e[k]!=(uint8_t)name[k])break;}
        if(k==11u){
            if(out_first)*out_first=le16(e+26);
            if(out_size)*out_size=le32(e+28);
            return 1;
        }
    }
    return 0;
}

static int dir_add(uint16_t first,const char name[11],uint8_t attr,uint16_t cluster,uint32_t size)
{
    const uint8_t *e;uint8_t k;uint8_t *w;
    dir_open(first);
    while((e=dir_next())!=0){
        if(e[0]==0u||e[0]==0xe5u){
            w=cfg_sector+((uint32_t)(dir_ent-1u)*32u);
            for(k=0u;k<32u;++k){w[k]=0u;}
            for(k=0u;k<11u;++k){w[k]=(uint8_t)name[k];}
            w[11]=attr;
            w[26]=(uint8_t)cluster;w[27]=(uint8_t)(cluster>>8);
            w[28]=(uint8_t)size;w[29]=(uint8_t)(size>>8);w[30]=(uint8_t)(size>>16);w[31]=(uint8_t)(size>>24);
            return acsi_write_sector(cfg_id,dir_cur_lba,cfg_sector);
        }
    }
    return 0;
}

static int cfg_bios_dir_open(void)
{
    uint16_t first=0u;
    if(dir_find(0u,bios_dir_name,&first,0)&&first>=2u){cfg_bios_cluster=first;return 1;}
    return 0;
}

static int cfg_bios_dir_make(void)
{
    uint16_t cluster=cfg_alloc(),s;uint8_t zero[512];uint16_t j;
    if(!cluster)return 0;
    for(j=0u;j<512u;++j){zero[j]=0u;}
    for(s=0u;s<cfg_spc;++s){
        uint32_t lba=cfg_data_start+((uint32_t)cluster-2u)*cfg_spc+s;
        if(!acsi_write_sector(cfg_id,lba,zero))return 0;
    }
    if(!dir_add(0u,bios_dir_name,0x10u,cluster,0u))return 0;
    cfg_bios_cluster=cluster;
    return 1;
}

static int cfg_ensure_dir(void)
{
    cfg_bios_cluster=0u;
    if(cfg_bios_dir_open())return 1;
    return cfg_bios_dir_make();
}

static int cfg_file_read(void *destination,uint32_t capacity,uint32_t *out_size)
{
    uint16_t first,cluster;uint32_t size,remaining,offset=0u;uint8_t *out=(uint8_t *)destination;
    if(!dir_find(cfg_bios_cluster,config_name,&first,&size)||first<2u)return 0;
    if(size>capacity)return 0;
    cluster=first;remaining=size;
    while(remaining){
        uint8_t s;
        for(s=0u;s<cfg_spc&&remaining;++s){
            uint32_t lba=cfg_data_start+((uint32_t)cluster-2u)*cfg_spc+s;
            uint16_t amount=(remaining<512u)?(uint16_t)remaining:512u,k;
            if(!acsi_read_sector(cfg_id,lba,cfg_sector))return 0;
            for(k=0u;k<amount;++k){out[offset+k]=cfg_sector[k];}
            offset+=amount;remaining-=amount;
        }
        if(remaining){
            uint16_t next;
            if(!cfg_fat_get(cluster,&next))return 0;
            if(next<2u||next>=0xfff8u)return 0;
            cluster=next;
        }
    }
    *out_size=size;return 1;
}

static int cfg_file_write(const void *source,uint32_t size)
{
    const uint8_t *in=(const uint8_t *)source;uint16_t first=0u,cluster;uint32_t lba;uint16_t k;
    if(size>512u)return 0;
    if(dir_find(cfg_bios_cluster,config_name,&first,0)&&first>=2u){
        uint32_t entry_lba=dir_cur_lba;uint16_t entry_no=dir_ent;
        lba=cfg_data_start+((uint32_t)first-2u)*cfg_spc;
        for(k=0u;k<512u;++k){cfg_sector[k]=(k<size)?in[k]:0u;}
        if(!acsi_write_sector(cfg_id,lba,cfg_sector))return 0;
        /* An existing entry keeps its old size field; update it, otherwise a
           grown file still reads back as the stale shorter length. */
        if(entry_no&&acsi_read_sector(cfg_id,entry_lba,cfg_sector)){
            uint8_t *w=cfg_sector+((uint32_t)(entry_no-1u)*32u);
            w[28]=(uint8_t)size;w[29]=(uint8_t)(size>>8);
            w[30]=(uint8_t)(size>>16);w[31]=(uint8_t)(size>>24);
            (void)acsi_write_sector(cfg_id,entry_lba,cfg_sector);
        }
        return 1;
    }
    cluster=cfg_alloc();
    if(!cluster)return 0;
    lba=cfg_data_start+((uint32_t)cluster-2u)*cfg_spc;
    for(k=0u;k<512u;++k){cfg_sector[k]=(k<size)?in[k]:0u;}
    if(!acsi_write_sector(cfg_id,lba,cfg_sector))return 0;
    return dir_add(cfg_bios_cluster,config_name,0x20u,cluster,size);
}

static int cfg_read_valid(bios_config_t *candidate)
{
    uint32_t size=0u;
    if(!cfg_file_read(candidate,sizeof *candidate,&size))return 0;
    if(size!=sizeof *candidate)return 0;
    return cfg_validate(candidate);
}
/* Try one device: every partition, \BIOS\BIOS.CFG first and the root
   \BIOS.CFG fallback.  Returns 1 and fills *candidate on the first valid hit. */
static int cfg_try_device(uint8_t id,bios_config_t *candidate)
{
    uint8_t part;
    cfg_id=id;cfg_bios_cluster=0u;
    g_dbg_cfg_device=id;
    if(!acsi_inspect_device(&g_system,id))return 0;
    g_dbg_cfg_inspect=1u;
    for(part=0u;part<4u;++part){
        if(!(g_system.acsi_partitions&(uint8_t)(1u<<part)))continue;
        cfg_part=g_system.acsi_part_start[part];
        cfg_bios_cluster=0u;
        g_dbg_cfg_part=part;
        if(!cfg_mount())continue;
        g_dbg_cfg_mounted=1u;
        (void)cfg_bios_dir_open();
        g_dbg_cfg_dir=(uint8_t)(cfg_bios_cluster?1u:0u);
        if(cfg_read_valid(candidate)){g_dbg_cfg_read=1u;return 1;}
        if(cfg_bios_cluster){cfg_bios_cluster=0u;if(cfg_read_valid(candidate)){g_dbg_cfg_read=2u;return 1;}}
    }
    return 0;
}
int cfg_load(void)
{
    uint8_t order[10],k;bios_config_t candidate;int found=0;
    cfg_defaults(&g_config);
    (void)nvram_load_boot(&g_config);   /* NVRAM boot preference (if present) */
    g_config_loaded=0u;g_config_saved=0u;g_config_error=0u;cfg_mounted=0u;cfg_id=0xffu;cfg_bios_cluster=0u;
    /* Device order follows the (possibly NVRAM-set) boot preference, and every
       device is tried until one carries a VALID config: with an IDE system disk
       plus an ACSI disk, the config must be found on whichever disk holds it,
       not merely on the first device with an AHDI table. */
    for(k=0u;k<10u;++k){
        if(g_config.boot_device==CFG_BOOT_IDE)
            order[k]=(k<2u)?(uint8_t)(0x80u|k):(uint8_t)(k-2u);
        else
            order[k]=(k<8u)?(uint8_t)k:(uint8_t)(0x80u|(k-8u));
    }
    for(k=0u;k<10u&&!found;++k){
        uint8_t id=order[k];
        if(!block_probe(id))continue;
        ++g_dbg_cfg_probe;
        found=cfg_try_device(id,&candidate);
    }
    if(!found){g_config_error=(cfg_id==0xffu)?1u:5u;g_dbg_cfg_error=g_config_error;return 0;}
    {const uint8_t *s=(const uint8_t *)&candidate;uint8_t *d=(uint8_t *)&g_config;uint32_t i;
     for(i=0u;i<sizeof g_config;++i){d[i]=s[i];}}
    (void)nvram_load_boot(&g_config);   /* NVRAM wins over the disk copy */
    g_config_loaded=1u;
    return 1;
}

int cfg_save(void)
{
    if(!cfg_mounted){
        uint8_t part;
        if(cfg_id==0xffu)cfg_id=cfg_find_boot();
        if(cfg_id==0xffu)return 0;
        part=cfg_boot_partition();
        if(part==0xffu)return 0;
        cfg_part=g_system.acsi_part_start[part];
        if(!cfg_mount())return 0;
    }
    (void)cfg_ensure_dir();
    g_config.magic=CONFIG_MAGIC;
    g_config.version=CONFIG_VERSION;
    g_config.size=(uint16_t)sizeof g_config;
    g_config.crc=cfg_crc(&g_config);
    g_config_saved=(uint8_t)cfg_file_write(&g_config,sizeof g_config);
    (void)nvram_save_boot(&g_config);   /* mirror the boot preference in NVRAM */
    return g_config_saved;
}
