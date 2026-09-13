#include <stdio.h>
#include <stdint.h>
#include "kernel_fat.h"

static FILE *disk;
int kblock_read(uint32_t sector,uint16_t count,void *buffer)
{
    if(fseek(disk,(long)sector*512L,SEEK_SET))return 0;
    return fread(buffer,512u,count,disk)==count;
}

static int show(const char *name,uint32_t size,uint32_t attributes,void *context)
{
    (void)context;printf("%s %lu %02lx\n",name,(unsigned long)size,
                         (unsigned long)attributes);return 1;
}

int main(void)
{
    uint32_t size=0,got=0;uint8_t attributes=0,buffer[700];
    disk=fopen("/tmp/atari-bios-autoboot-test/acsi.img","rb");
    if(!disk)return 1;
    printf("mount=%d\n",kfat_mount(2,"BGM"));
    printf("stat=%d size=%lu attr=%02x\n",kfat_stat_path("\\system",&size,&attributes),
           (unsigned long)size,attributes);
    printf("list=%d\n",kfat_list_path("\\system",show,0));
    {int state=kfat_read_path_at("\\system\\kernel.sys",0,buffer,sizeof(buffer),&got);
     printf("read0=%d got=%lu\n",state,(unsigned long)got);}
    {int state=kfat_read_path_at("\\system\\kernel.sys",650,buffer,sizeof(buffer),&got);
     printf("readcross=%d got=%lu\n",state,(unsigned long)got);}
    kfat_stat_path("\\system\\kernel.sys",&size,&attributes);
    {int state=kfat_read_path_at("\\system\\kernel.sys",size,buffer,sizeof(buffer),&got);
     printf("readeof=%d got=%lu\n",state,(unsigned long)got);}
    fclose(disk);return 0;
}
