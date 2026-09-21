/* Read-only benchmark of real FAT traversal on an image; no GUI timing. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel_block.h"
#include "kernel_memory.h"
#include "kernel_fat.h"
static FILE *disk;
static unsigned reads,sectors;
static uint32_t checksum;
static unsigned entries;
void *kmem_alloc(uint32_t n,uint32_t a,uint16_t o,uint16_t f,int tt)
{(void)a;(void)o;(void)f;(void)tt;return calloc(1,n);}
int kmem_free(void *p,uint16_t o){(void)o;free(p);return 1;}
int kernel_acsi_read_blocks(uint8_t d,uint32_t s,uint16_t n,void *p)
{(void)d;reads++;sectors+=n;return fseek(disk,(long)s*512,SEEK_SET)==0&&fread(p,512,n,disk)==n;}
int kblock_test_disk_write(uint8_t d,uint32_t s,uint16_t n,const void *p)
{(void)d;(void)s;(void)n;(void)p;assert(!"read-only benchmark attempted a write");return 0;}
static int visit(const char *name,uint32_t size,uint32_t attr,void *ctx)
{(void)ctx;while(*name)checksum=checksum*33u+(unsigned char)*name++;checksum=checksum*33u+size+attr;return ++entries<32;}
static void page(const char *path){entries=0;assert(kfat_list_path(path,visit,NULL)==FAT_FILE_FOUND);}
int main(int argc,char **argv)
{unsigned char root[512];uint32_t part;unsigned i,r,s;assert(argc==2);disk=fopen(argv[1],"rb");assert(disk);assert(fread(root,512,1,disk)==1);
 part=((uint32_t)root[458]<<24)|((uint32_t)root[459]<<16)|((uint32_t)root[460]<<8)|root[461];
 assert(kblock_init(0)==BLOCK_READY);assert(kfat_mount(part,"BGM")==FAT_MOUNTED);
 r=reads;s=sectors;page("/");page("/SYSTEM/APP");page("/SYSTEM/TEST");printf("First 3 pages: %u physical reads, %u sectors\n",reads-r,sectors-s);
 r=reads;s=sectors;checksum=0;for(i=0;i<100;i++){page("/");page("/SYSTEM/APP");page("/SYSTEM/TEST");}
 printf("300 warm pages: %u physical reads, %u sectors; checksum %08x\n",reads-r,sectors-s,checksum);fclose(disk);return 0;}
