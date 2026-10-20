/* Read-only FAT12 reader on top of the WD1772 driver (floppy drive A).
   Floppies use a 16-bit total-sector count, so the forward lookup is simple. */
#include "hardware.h"
#include "fdc.h"
#include "fat12.h"

typedef struct {
    uint16_t bps,reserved,root_entries,total_sectors,spf,spt,heads,root_dir_sectors;
    uint16_t fat_start,root_dir_start,data_start;
    uint8_t spc,fats,mounted;
} fat12_t;
static fat12_t fs;
static uint8_t sec[512];

/* Result of the last probe, readable from the debugger/tests. */
volatile uint32_t fat12_dbg_status,fat12_dbg_sig;

static uint16_t rd16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}

static int read_lba(uint16_t lba,void *buffer)
{
    uint16_t per_track=(uint16_t)(fs.spt*fs.heads);
    uint16_t track,side,sector;
    if(!fs.spt||!fs.heads)return 0;
    track=(uint16_t)(lba/per_track);
    side=(uint8_t)((lba/fs.spt)%fs.heads);
    sector=(uint8_t)((lba%fs.spt)+1u);
    return fdc_read_sector(0u,(uint8_t)track,side,sector,buffer);
}

int fat12_mount(void)
{
    uint8_t boot[512];uint32_t total;
    fs.mounted=0;
    if(!fdc_read_boot_sector(0u,boot))return 0;
    if(rd16(boot+11)!=512u)return 0;
    fs.spc=boot[13];fs.reserved=rd16(boot+14);fs.fats=boot[16];
    fs.root_entries=rd16(boot+17);fs.spf=rd16(boot+22);
    fs.spt=rd16(boot+24);if(!fs.spt)fs.spt=9u;
    fs.heads=rd16(boot+26);if(!fs.heads)fs.heads=2u;
    total=rd16(boot+19);if(!total)total=rd16(boot+32);
    fs.total_sectors=(uint16_t)(total<65536u?total:0u);
    if(!fs.spc||!fs.fats||fs.fats>2u||!fs.root_entries||!fs.spf)return 0;
    fs.root_dir_sectors=(uint16_t)(((uint32_t)fs.root_entries*32u+511u)/512u);
    fs.fat_start=fs.reserved;
    fs.root_dir_start=(uint16_t)(fs.fat_start+(uint16_t)fs.fats*fs.spf);
    fs.data_start=(uint16_t)(fs.root_dir_start+fs.root_dir_sectors);
    fs.mounted=1u;
    return 1;
}

static uint16_t fat12_next(uint16_t cluster)
{
    uint32_t off=(uint32_t)cluster+(cluster/2u);
    uint16_t so=(uint16_t)(fs.fat_start+off/512u),bo=(uint16_t)(off%512u);
    uint8_t a,b;uint16_t v;
    if(!read_lba(so,sec))return 0xfffu;
    a=sec[bo];
    if(bo==511u){if(!read_lba((uint16_t)(so+1u),sec))return 0xfffu;b=sec[0];}else b=sec[bo+1u];
    v=(uint16_t)(a|((uint16_t)b<<8));
    return (cluster&1u)?(uint16_t)(v>>4):(uint16_t)(v&0x0fffu);
}

static void name83(const char *name,uint8_t out[11])
{
    uint8_t i=0,j;
    for(j=0u;j<11u;++j)out[j]=' ';
    while(*name&&*name!='.'&&i<8u){char c=*name++;if(c>='a'&&c<='z')c=(char)(c-'a'+'A');out[i++]=(uint8_t)c;}
    while(*name&&*name!='.')++name;
    if(*name=='.'){
        ++name;i=8u;
        while(*name&&i<11u){char c=*name++;if(c>='a'&&c<='z')c=(char)(c-'a'+'A');out[i++]=(uint8_t)c;}
    }
}

int fat12_find(const char *name,uint32_t *size,uint16_t *cluster)
{
    uint8_t want[11];uint16_t s;
    if(!fs.mounted)return 0;
    name83(name,want);
    for(s=0u;s<fs.root_dir_sectors;++s){
        uint16_t e;
        if(!read_lba((uint16_t)(fs.root_dir_start+s),sec))return 0;
        for(e=0u;e<16u;++e){
            const uint8_t *p=sec+(uint32_t)e*32u;uint8_t k;
            if(p[0]==0u)return 0;
            if(p[0]==0xe5u)continue;
            if((p[11]&0x0fu)==0x0fu)continue;
            for(k=0u;k<11u;++k)if(p[k]!=want[k])break;
            if(k==11u){
                if(size)*size=(uint32_t)rd16(p+28)|((uint32_t)rd16(p+30)<<16);
                if(cluster)*cluster=rd16(p+26);
                return 1;
            }
        }
    }
    return 0;
}

int fat12_read(uint16_t cluster,uint32_t size,void *buffer,uint32_t capacity,uint32_t *read_size)
{
    uint8_t *out=(uint8_t *)buffer;uint32_t remaining=size,offset=0;uint16_t guard=0;
    if(!fs.mounted||!buffer||size>capacity)return 0;
    while(remaining&&cluster>=2u&&cluster<0xff8u&&guard<4096u){
        uint8_t s;
        for(s=0u;s<fs.spc&&remaining;++s){
            uint16_t lba=(uint16_t)(fs.data_start+((uint32_t)cluster-2u)*fs.spc+s);
            uint16_t amount=(remaining<512u)?(uint16_t)remaining:512u,k;
            if(!read_lba(lba,sec))return 0;
            for(k=0u;k<amount;++k)out[offset+k]=sec[k];
            offset+=amount;remaining-=amount;
        }
        ++guard;
        if(remaining&&cluster<0xff8u)cluster=fat12_next(cluster);
    }
    if(read_size)*read_size=offset;
    return remaining==0u;
}

int fat12_read_file(const char *name,void *buffer,uint32_t capacity,uint32_t *size)
{
    uint32_t file_size=0,got=0;uint16_t cluster=0;
    if(!fs.mounted&&!fat12_mount())return 0;
    if(!fat12_find(name,&file_size,&cluster))return 0;
    if(file_size>capacity)return 0;
    if(!fat12_read(cluster,file_size,buffer,capacity,&got))return 0;
    if(size)*size=got;
    return got==file_size;
}

int fat12_dir(uint32_t index,char name[13],uint32_t *size,uint8_t *attr)
{
    uint16_t s,e;uint32_t seen=0;
    if(!fs.mounted)return 0;
    for(s=0u;s<fs.root_dir_sectors;++s){
        if(!read_lba((uint16_t)(fs.root_dir_start+s),sec))return 0;
        for(e=0u;e<16u;++e){
            const uint8_t *p=sec+(uint32_t)e*32u;uint8_t k,n=0;
            if(p[0]==0u)return 0;
            if(p[0]==0xe5u)continue;
            if((p[11]&0x0fu)==0x0fu)continue;
            if(p[11]&0x08u)continue;               /* volume label */
            if(seen==index){
                for(k=0u;k<8u&&p[k]!=' ';++k)name[n++]=(char)p[k];
                if(p[8]!=' '){name[n++]='.';for(k=8u;k<11u&&p[k]!=' ';++k)name[n++]=(char)p[k];}
                name[n]=0;
                if(size)*size=(uint32_t)rd16(p+28)|((uint32_t)rd16(p+30)<<16);
                if(attr)*attr=p[11];
                return 1;
            }
            ++seen;
        }
    }
    return 0;
}

int fat12_probe(void)
{
    static uint8_t probe[256];uint32_t got=0;
    fat12_dbg_status=0u;fat12_dbg_sig=0u;
    if(!fat12_mount())return 0;
    if(!fat12_read_file("TEST.TXT",probe,sizeof probe,&got))return 0;
    fat12_dbg_status=1u;
    fat12_dbg_sig=((uint32_t)probe[0]<<24)|((uint32_t)probe[1]<<16)|((uint32_t)probe[2]<<8)|probe[3];
    return 1;
}
