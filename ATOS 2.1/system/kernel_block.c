#include "kernel_block.h"
#include "kernel_memory.h"
#ifndef KBLOCK_TEST
#include "kernel_module.h"
#else
extern int kernel_acsi_read_blocks(uint8_t,uint32_t,uint16_t,void *);
#endif

/* Read cache, synchronous write-through. No dirty data or deferred writes.
 * Four ways retain colliding FAT/directory/data sectors; ranks are bounded
 * (no timestamp wrap). DMA always uses the dedicated ST-RAM staging buffer. */
#define BLOCK_OWNER 0x0101u
#define TRANSFER_SECTORS 8u
#define SECTOR_BYTES 512u
/* 1024 sectors = 512 KB.  The cache lives in TT-RAM (64 MB here), and the
   boot reads a 127 KB kernel plus a dozen modules: 128 sectors (64 KB) thrashed
   on every file, so the disk was re-read continuously. */
#define CACHE_SETS 256u
#define CACHE_WAYS 4u
#define CACHE_SECTORS (CACHE_SETS*CACHE_WAYS)
typedef struct { uint32_t sector; uint8_t valid,rank; } cache_tag_t;
static cache_tag_t tags[CACHE_SECTORS];
static uint8_t *dma_buffer,*cache_buffer;
static uint8_t block_device,ready;
static kblock_stats_t stats;
/* Diagnostic: how many set/way searches lookup() performed.  The all-hit path
   in kblock_read() used to search twice per sector; this counter makes that
   measurable instead of a claim. */
static uint32_t cache_searches;
static void copy_bytes(uint8_t *d,const uint8_t *s,uint32_t n)
{
    /* Word-copy the aligned middle: the byte loop made every sector copy ~4x
       slower and dominated the disk-read benchmark.  The 68030 faults on an
       unaligned long, so byte-align first and only use 32-bit when d and s
       share the same alignment. */
    if(((uintptr_t)d&3u)==((uintptr_t)s&3u)){
        while(n&&((uintptr_t)d&3u)){*d++=*s++;--n;}
        while(n>=4u){*(uint32_t *)(void *)d=*(const uint32_t *)(const void *)s;d+=4;s+=4;n-=4;}
    }
    while(n--)*d++=*s++;
}
void kblock_cache_clear(void)
{unsigned i;for(i=0;i<CACHE_SECTORS;i++){tags[i].valid=0;tags[i].rank=0;}}
void kblock_cache_stats(kblock_stats_t *out)
{
    /* Field by field: a struct assignment would pull in memcpy, and this
       kernel is freestanding. */
    if(!out)return;
    out->hits=stats.hits;
    out->misses=stats.misses;
    out->searches=cache_searches;
    out->reads=stats.reads;
    out->writes=stats.writes;
}
uint32_t kblock_search_count(void){return cache_searches;}
uint32_t kblock_cache_sectors(void)
{return cache_buffer?CACHE_SECTORS:0;}
int kblock_cache_is_tt(void)
{return (uintptr_t)cache_buffer>=0x01000000UL;}
static unsigned set_first(uint32_t sector)
{return (unsigned)(sector&(CACHE_SETS-1u))*CACHE_WAYS;}
static int lookup(uint32_t sector)
{
    unsigned i,first=set_first(sector);
    ++cache_searches;
    for(i=first;i<first+CACHE_WAYS;i++)
        if(tags[i].valid&&tags[i].sector==sector)return (int)i;
    return -1;
}
static void touch(unsigned slot)
{unsigned i,first=slot&~(CACHE_WAYS-1u);uint8_t rank=tags[slot].rank;
 for(i=first;i<first+CACHE_WAYS;i++)if(tags[i].valid&&tags[i].rank<rank)tags[i].rank++;
 tags[slot].rank=0;}
static void invalidate(unsigned slot)
{unsigned i,first=slot&~(CACHE_WAYS-1u);uint8_t rank=tags[slot].rank;
 for(i=first;i<first+CACHE_WAYS;i++)if(tags[i].valid&&tags[i].rank>rank)tags[i].rank--;
 tags[slot].valid=0;}
static void put(uint32_t sector,const uint8_t *data)
{int found=lookup(sector);unsigned i,slot,first=set_first(sector);
 if(!cache_buffer)return;
 if(found>=0)slot=(unsigned)found;
 else{slot=first;for(i=first;i<first+CACHE_WAYS;i++){
   if(!tags[i].valid){slot=i;break;}if(tags[i].rank>tags[slot].rank)slot=i;}
   tags[slot].rank=CACHE_WAYS;}
 touch(slot);copy_bytes(cache_buffer+slot*SECTOR_BYTES,data,SECTOR_BYTES);
 tags[slot].sector=sector;tags[slot].valid=1;
}
enum block_state kblock_init(uint8_t device)
{
 ready=0;kblock_cache_clear();block_device=device;
 stats.hits=stats.misses=stats.reads=stats.writes=0;
 if(!dma_buffer)dma_buffer=kmem_alloc(TRANSFER_SECTORS*SECTOR_BYTES,512u,BLOCK_OWNER,KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
 if(!dma_buffer)return BLOCK_NO_MEMORY;
 /* Optional cache: low memory must not prevent boot or disk access. */
 if(!cache_buffer)cache_buffer=kmem_alloc(CACHE_SECTORS*SECTOR_BYTES,16u,BLOCK_OWNER,KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);
 stats.reads++;
 if(!kernel_acsi_read_blocks(device,0,1,dma_buffer))return BLOCK_IO_FAILED;
 put(0,dma_buffer);ready=1;return BLOCK_READY;
}
int kblock_select(uint8_t device)
{
    if(device==block_device&&ready)return 1;
    ready=0;
    block_device=device;
    kblock_cache_clear();          /* cached sectors belong to the old device */
    if(!dma_buffer)return 0;
    stats.reads++;
    if(!kernel_acsi_read_blocks(device,0,1,dma_buffer))return 0;
    put(0,dma_buffer);
    ready=1;
    return 1;
}
int kblock_is_ready(void){return ready!=0;}
int kblock_read(uint32_t sector,uint16_t count,void *destination)
{
 /* Slots found while probing are kept, so the all-hit path does not search the
    set a second time for each sector.  TRANSFER_SECTORS is the bound, so this
    stays on the stack. */
 unsigned i;int slot,all=1;int slots[TRANSFER_SECTORS];uint8_t *out=destination;
 if(!ready||!destination||!count||count>TRANSFER_SECTORS||(uint32_t)count-1u>0xffffffffUL-sector)return 0;
 for(i=0;i<count;i++){slot=cache_buffer?lookup(sector+i):-1;slots[i]=slot;
   if(slot<0){stats.misses++;all=0;}else stats.hits++;}
 if(all){for(i=0;i<count;i++){touch((unsigned)slots[i]);copy_bytes(out+i*SECTOR_BYTES,cache_buffer+(unsigned)slots[i]*SECTOR_BYTES,SECTOR_BYTES);}return 1;}
 /* A requested run is read as one transfer, without speculative read-ahead.
  * Publish nothing until the whole transfer has succeeded. */
 stats.reads++;
 if(!kernel_acsi_read_blocks(block_device,sector,count,dma_buffer)){kblock_cache_clear();return 0;}
 for(i=0;i<count;i++)put(sector+i,dma_buffer+i*SECTOR_BYTES);
 copy_bytes(out,dma_buffer,(uint32_t)count*SECTOR_BYTES);return 1;
}

#ifdef KBLOCK_TEST
extern int kblock_test_disk_write(uint8_t device,uint32_t sector,uint16_t count,const void *source);
static int disk_write(uint32_t sector,uint16_t count,const void *source)
{copy_bytes(dma_buffer,source,(uint32_t)count*SECTOR_BYTES);return kblock_test_disk_write(block_device,sector,count,dma_buffer);}
#else
#define DISK_REG8(a) (*(volatile uint8_t *)(uintptr_t)(a))
#define DISK_REG16(a) (*(volatile uint16_t *)(uintptr_t)(a))
static void disk_delay(void)
{volatile unsigned n;for(n=0;n<96;n++)__asm__ volatile("nop");}
extern void kernel_ikbd_poll_capture(void);
static int disk_irq(unsigned limit)
{while(limit--){
 if(DISK_REG8(0xfffffc00u)&1u)kernel_ikbd_poll_capture();
 if(!(DISK_REG8(0xfffffa01u)&32u))return 1;
 }return 0;}
static int disk_write(uint32_t sector,uint16_t count,const void *source)
{
    uint8_t cmd[11];unsigned n,i;uint32_t address=(uint32_t)(uintptr_t)dma_buffer;int ok=0;
    if(!dma_buffer||!source||!count||count>TRANSFER_SECTORS||(uint32_t)count-1u>0xffffffffu-sector)return 0;
    copy_bytes(dma_buffer,source,(uint32_t)count*512u);
    if(sector<=0x1fffffu){n=6;cmd[0]=(uint8_t)((block_device<<5)|0x0a);cmd[1]=sector>>16;cmd[2]=sector>>8;cmd[3]=sector;cmd[4]=count;cmd[5]=0;}
    else{n=11;cmd[0]=(uint8_t)((block_device<<5)|31);cmd[1]=0x2a;cmd[2]=0;cmd[3]=sector>>24;cmd[4]=sector>>16;cmd[5]=sector>>8;cmd[6]=sector;cmd[7]=cmd[8]=cmd[10]=0;cmd[9]=count;}
    DISK_REG8(0xffff8609u)=address>>16;DISK_REG8(0xffff860bu)=address>>8;DISK_REG8(0xffff860du)=address;
    DISK_REG16(0xffff8606u)=0x90;disk_delay();DISK_REG16(0xffff8606u)=0x190;disk_delay();
    DISK_REG16(0xffff8604u)=count;DISK_REG16(0xffff8606u)=0x188;
    for(i=0;i+1<n;i++){DISK_REG16(0xffff8604u)=cmd[i];__asm__ volatile("nop");DISK_REG16(0xffff8606u)=0x18a;if(!disk_irq(50000))goto done;}
    DISK_REG16(0xffff8604u)=cmd[n-1];__asm__ volatile("nop");DISK_REG16(0xffff8606u)=0x100;
    if(!disk_irq(400000))goto done;
    DISK_REG16(0xffff8606u)=0x8a;ok=(DISK_REG16(0xffff8604u)&255u)==0;
 done:DISK_REG16(0xffff8606u)=0x80;return ok;
}

#endif
int kblock_write(uint32_t sector,uint16_t count,const void *source)
{
 unsigned i;int slot;
 if(!ready||!source||!count||count>TRANSFER_SECTORS||(uint32_t)count-1u>0xffffffffUL-sector)return 0;
 /* Invalidate first: a failed DMA may have written only part of the run. */
 for(i=0;i<count;i++){slot=lookup(sector+i);if(slot>=0)invalidate((unsigned)slot);}
 stats.writes++;
#ifdef KBLOCK_TEST
 if(!disk_write(sector,count,source)){kblock_cache_clear();return 0;}
#else
 /* The kernel's native write path drives the ST DMA (ACSI) registers, which do
    not reach a TF536 IDE disk. IDE writes go through the ROM's IDE driver with
    the transfer staged in the 512-byte-aligned DMA buffer. */
 if(block_device&0x80u){
  uint16_t s;copy_bytes(dma_buffer,source,(uint32_t)count*SECTOR_BYTES);
  for(s=0u;s<count;++s)if(!kernel_acsi_write_blocks(block_device,sector+s,1u,dma_buffer+(uint32_t)s*SECTOR_BYTES)){kblock_cache_clear();return 0;}
 }else if(!disk_write(sector,count,source)){kblock_cache_clear();return 0;}
#endif
 for(i=0;i<count;i++)put(sector+i,dma_buffer+i*SECTOR_BYTES);
 return 1;
}

uint8_t kblock_device(void){return block_device;}
int kblock_read_uncached(uint32_t sector,uint16_t count,void *destination)
{
 if(!ready||!destination||!count||count>TRANSFER_SECTORS||(uint32_t)count-1>0xffffffffUL-sector)return 0;
 stats.reads++;
 if(!kernel_acsi_read_blocks(block_device,sector,count,dma_buffer))return 0;
 copy_bytes(destination,dma_buffer,(uint32_t)count*512u);return 1;
}
#ifndef KBLOCK_TEST
/* Read-only INQUIRY and ICD extended READ CAPACITY(10), boot device only.
 * ACSI bridges may reject either command; zero remains explicitly unknown. */
static int diagnostic_command(const uint8_t *cmd,unsigned bytes)
{
 unsigned i;uint32_t address=(uint32_t)(uintptr_t)dma_buffer;int ok=0;
 for(i=0;i<512;i++)dma_buffer[i]=0;
 DISK_REG8(0xffff8609u)=address>>16;DISK_REG8(0xffff860bu)=address>>8;DISK_REG8(0xffff860du)=address;
 DISK_REG16(0xffff8606u)=0x190;disk_delay();DISK_REG16(0xffff8606u)=0x90;disk_delay();
 DISK_REG16(0xffff8604u)=1;DISK_REG16(0xffff8606u)=0x88;
 for(i=0;i+1<bytes;i++){DISK_REG16(0xffff8604u)=cmd[i];__asm__ volatile("nop");DISK_REG16(0xffff8606u)=0x8a;if(!disk_irq(50000))goto done;}
 DISK_REG16(0xffff8604u)=cmd[bytes-1];__asm__ volatile("nop");DISK_REG16(0xffff8606u)=0;
 if(!disk_irq(400000))goto done;
 DISK_REG16(0xffff8606u)=0x8a;ok=(DISK_REG16(0xffff8604u)&255u)==0;
 done:DISK_REG16(0xffff8606u)=0x80;return ok;
}
static uint32_t diagnostic_be32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
void kblock_identify(char name[32],uint32_t *sectors,uint32_t *sector_bytes)
{
 static uint8_t done;static char saved[32];static uint32_t total,bytes;
 unsigned i,n=0;uint8_t cmd[11];
 if(!done&&ready){
  done=1;for(i=0;i<11;i++)cmd[i]=0;
  cmd[0]=(uint8_t)((block_device<<5)|0x12);cmd[4]=36;
  if(diagnostic_command(cmd,6)){
   for(i=8;i<32;i++){uint8_t c=dma_buffer[i];if(c<32||c>126){n=0;break;}saved[n++]=(char)c;}
   while(n&&saved[n-1]==' ')n--;
   saved[n]=0;
  }
  for(i=0;i<11;i++)cmd[i]=0;
  cmd[0]=(uint8_t)((block_device<<5)|31);cmd[1]=0x25;
  if(diagnostic_command(cmd,11)){
   uint32_t last=diagnostic_be32(dma_buffer),block=diagnostic_be32(dma_buffer+4);
   if(last&&last!=0xffffffffUL&&block==512){total=last+1;bytes=block;}
  }
 }
 for(i=0;i<32;i++)name[i]=saved[i];
 *sectors=total;*sector_bytes=bytes;
}
#endif
