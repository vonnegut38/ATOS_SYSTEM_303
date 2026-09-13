#include "hardware.h"
#include "system.h"
#include "config.h"
#include "ide.h"

bios_config_t g_config;
uint8_t g_config_loaded;
uint8_t g_config_saved;
uint8_t g_config_error;
uint8_t g_config_device;
uint8_t g_config_location;

/* Configuration lives in \BIOS\BIOS.CFG on the boot volume.  A root-level
 * \BIOS.CFG is still read for compatibility with older volumes. */
static const char config_name[11]={'B','I','O','S',' ',' ',' ',' ','C','F','G'};
static const char bios_dir_name[11]={'B','I','O','S',' ',' ',' ',' ',' ',' ',' '};

static uint8_t  cfg_id;
static uint32_t cfg_part;
static uint32_t cfg_fat_start;
static uint32_t cfg_root_start;
static uint32_t cfg_data_start;
static uint32_t cfg_cluster_count;
static uint16_t cfg_root_entries;
static uint8_t  cfg_spc;
static uint8_t  cfg_fats;
static uint8_t  cfg_mounted;
static uint8_t  cfg_sector[512];
static uint16_t cfg_bios_cluster;   /* 0 = use the root directory */

static uint16_t le16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}

uint32_t cfg_crc(const bios_config_t *config)
{
    const uint8_t *p=(const uint8_t *)config;uint32_t i,sum=0u;
    for(i=0u;i<sizeof(*config);++i){if(i>=8u&&i<12u)continue;sum=(uint32_t)((sum<<1)|(sum>>31))^(uint32_t)p[i];}
    return sum;
}

void cfg_defaults(void)
{
    uint8_t *p=(uint8_t *)&g_config;uint32_t i;
    for(i=0u;i<sizeof(g_config);++i)p[i]=0u;
    g_config.magic=CONFIG_MAGIC;g_config.version=CONFIG_VERSION;g_config.size=(uint16_t)sizeof(g_config);
    g_config.video_pref=0u;g_config.blitter=1u;g_config.boot_delay=4u;g_config.boot_device=0u;
    g_config.fast_boot=0u;g_config.beep=1u;
}

static uint8_t cfg_find_boot(void)
{
    uint8_t id,unit;
    for(id=0u;id<8u;++id)if(acsi_probe_device(id)){
        g_system.acsi_devices=(uint8_t)(g_system.acsi_devices|(uint8_t)(1u<<id));
        if(acsi_inspect_device(&g_system,id))return id;}
    for(unit=0u;unit<2u;++unit){uint8_t di=(uint8_t)(IDE_ID_FLAG|unit);
        if(acsi_probe_device(di)){
            g_system.ide_units=(uint8_t)(g_system.ide_units|(uint8_t)(1u<<unit));
            if(acsi_inspect_device(&g_system,di))return di;}}
    return 0xffu;
}

static uint8_t cfg_boot_partition(void)
{
    uint8_t p;
    for(p=0u;p<g_system.acsi_part_count;++p)if(g_system.acsi_partitions&((uint32_t)1u<<p))return p;
    return 0xffu;
}

static int cfg_mount(void)
{
    uint16_t bps,reserved,roots,spf,total16;uint32_t total32,total;
    if(!acsi_read_sector(cfg_id,cfg_part,cfg_sector))return 0;
    bps=le16(cfg_sector+11);
    if(bps!=512u)return 0;
    cfg_spc=cfg_sector[13];cfg_fats=cfg_sector[16];
    reserved=le16(cfg_sector+14);roots=le16(cfg_sector+17);spf=le16(cfg_sector+22);
    if(!cfg_spc||!cfg_fats||!reserved||!roots||!spf)return 0;
    cfg_root_entries=roots;
    cfg_fat_start=cfg_part+(uint32_t)reserved;
    cfg_root_start=cfg_fat_start+(uint32_t)cfg_fats*spf;
    cfg_data_start=cfg_root_start+(((uint32_t)roots*32u)+511u)/512u;
    total16=le16(cfg_sector+19);total32=le32(cfg_sector+32);total=total16?(uint32_t)total16:total32;
    if(total<=(cfg_data_start-cfg_part))return 0;
    cfg_cluster_count=(total-(cfg_data_start-cfg_part))/cfg_spc;
    cfg_mounted=cfg_cluster_count>2u;
    return cfg_mounted;
}

static int cfg_fat_get(uint16_t cluster,uint16_t *value)
{
    uint32_t offset=(uint32_t)cluster*2u;
    if(!acsi_read_sector(cfg_id,cfg_fat_start+offset/512u,cfg_sector))return 0;
    *value=le16(cfg_sector+(uint16_t)(offset&511u));return 1;
}

static int cfg_fat_put(uint16_t cluster,uint16_t value)
{
    uint32_t offset=(uint32_t)cluster*2u;uint16_t at=(uint16_t)(offset&511u);
    if(!acsi_read_sector(cfg_id,cfg_fat_start+offset/512u,cfg_sector))return 0;
    cfg_sector[at]=(uint8_t)value;cfg_sector[at+1u]=(uint8_t)(value>>8);
    return acsi_write_sector(cfg_id,cfg_fat_start+offset/512u,cfg_sector);
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

/* ---- FAT16 directory walk (root or a subdirectory cluster chain) ---- */

static uint16_t dir_cluster;
static uint32_t dir_lba;
static uint32_t dir_cur_lba;
static uint16_t dir_left;
static uint16_t dir_ent;

static void dir_open(uint16_t first)
{
    dir_cluster=first;dir_ent=16u;
    if(first==0u){
        dir_lba=cfg_root_start;
        dir_left=(uint16_t)(((uint32_t)cfg_root_entries*32u+511u)/512u);
    } else {
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
        {
            const uint8_t *e=cfg_sector+((uint32_t)dir_ent*32u);
            dir_ent++;
            return e;
        }
    }
}

static int dir_find(uint16_t first,const char name[11],uint16_t *out_first,uint32_t *out_size)
{
    const uint8_t *e;uint8_t k;
    dir_open(first);
    while((e=dir_next())!=0){
        if(e[0]==0u)return 0;
        if(e[0]==0xe5u||(e[11]&0x0fu)==0x0fu)continue;
        for(k=0u;k<11u;++k)if(e[k]!=(uint8_t)name[k])break;
        if(k==11u){if(out_first)*out_first=le16(e+26);if(out_size)*out_size=le32(e+28);return 1;}
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
            for(k=0u;k<32u;++k)w[k]=0u;
            for(k=0u;k<11u;++k)w[k]=(uint8_t)name[k];
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
    uint16_t cluster=cfg_alloc(),s;
    uint8_t zero[512];uint16_t j;
    if(!cluster)return 0;
    for(j=0u;j<512u;++j)zero[j]=0u;
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
            for(k=0u;k<amount;++k)out[offset+k]=cfg_sector[k];
            offset+=amount;remaining-=amount;
        }
        if(remaining){uint16_t next;if(!cfg_fat_get(cluster,&next))return 0;if(next<2u||next>=0xfff8u)return 0;cluster=next;}
    }
    *out_size=size;return 1;
}

static int cfg_file_write(const void *source,uint32_t size)
{
    const uint8_t *in=(const uint8_t *)source;uint16_t first=0u,cluster;uint32_t lba;uint16_t k;
    if(size>512u)return 0;
    if(dir_find(cfg_bios_cluster,config_name,&first,0)&&first>=2u){
        lba=cfg_data_start+((uint32_t)first-2u)*cfg_spc;
        for(k=0u;k<512u;++k)cfg_sector[k]=(k<size)?in[k]:0u;
        return acsi_write_sector(cfg_id,lba,cfg_sector);
    }
    cluster=cfg_alloc();
    if(!cluster)return 0;
    lba=cfg_data_start+((uint32_t)cluster-2u)*cfg_spc;
    for(k=0u;k<512u;++k)cfg_sector[k]=(k<size)?in[k]:0u;
    if(!acsi_write_sector(cfg_id,lba,cfg_sector))return 0;
    return dir_add(cfg_bios_cluster,config_name,0x20u,cluster,size);
}

int cfg_load(void)
{
    uint8_t part;uint32_t size=0u;bios_config_t candidate;
    cfg_defaults();g_config_loaded=0u;g_config_saved=0u;g_config_error=0u;cfg_mounted=0u;cfg_id=0xffu;cfg_bios_cluster=0u;
    cfg_id=cfg_find_boot();g_config_device=cfg_id;
    if(cfg_id==0xffu){g_config_error=1u;return 0;}
    part=cfg_boot_partition();
    if(part==0xffu){g_config_error=2u;return 0;}
    cfg_part=g_system.acsi_part_start[part];
    if(!cfg_mount()){g_config_error=3u;return 0;}
    (void)cfg_bios_dir_open();
    g_config_location=(uint8_t)(cfg_bios_cluster?1u:0u);
    if(!cfg_file_read(&candidate,sizeof(candidate),&size)){g_config_error=4u;return 0;}
    if(size!=sizeof(candidate)){g_config_error=5u;return 0;}
    if(candidate.magic!=CONFIG_MAGIC||candidate.version!=CONFIG_VERSION||candidate.size!=(uint16_t)sizeof(candidate)){g_config_error=6u;return 0;}
    if(candidate.crc!=cfg_crc(&candidate)){g_config_error=7u;return 0;}
    {const uint8_t *s=(const uint8_t *)&candidate;uint8_t *d=(uint8_t *)&g_config;uint32_t i;
     for(i=0u;i<sizeof(g_config);++i)d[i]=s[i];}
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
    g_config_location=(uint8_t)(cfg_bios_cluster?1u:0u);
    g_config.magic=CONFIG_MAGIC;g_config.version=CONFIG_VERSION;g_config.size=(uint16_t)sizeof(g_config);
    g_config.crc=cfg_crc(&g_config);
    g_config_saved=(uint8_t)cfg_file_write(&g_config,sizeof(g_config));
    return g_config_saved;
}
