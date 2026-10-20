/* Frozen v2 read path for comparative read-only benchmarks. */
#include "kernel_block.h"
#include "kernel_memory.h"
extern int kernel_acsi_read_blocks(uint8_t,uint32_t,uint16_t,void *);

#define BLOCK_OWNER 0x0101u
#define CACHE_SECTORS 8u
#define SECTOR_BYTES 512u

static uint8_t *dma_buffer;
static uint8_t *cache_buffer;
static uint8_t block_device;
static uint32_t cache_first;
static uint16_t cache_count;

static void copy_bytes(uint8_t *destination,const uint8_t *source,uint32_t count)
{while(count--)*destination++=*source++;}

int kblock_cache_is_tt(void)
{return (uint32_t)(uintptr_t)cache_buffer>=0x01000000UL;}

enum block_state kblock_init(uint8_t device)
{
    dma_buffer=kmem_alloc(CACHE_SECTORS*SECTOR_BYTES,512u,BLOCK_OWNER,
                          KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
    if(!dma_buffer)return BLOCK_NO_MEMORY;
    cache_buffer=kmem_alloc(CACHE_SECTORS*SECTOR_BYTES,16u,BLOCK_OWNER,
                            KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);
    if(!cache_buffer){kmem_free(dma_buffer,BLOCK_OWNER);dma_buffer=0;return BLOCK_NO_MEMORY;}
    block_device=device;cache_first=0;cache_count=0;
    if(!kernel_acsi_read_blocks(device,0,CACHE_SECTORS,dma_buffer)){
        kmem_free(cache_buffer,BLOCK_OWNER);kmem_free(dma_buffer,BLOCK_OWNER);
        cache_buffer=dma_buffer=0;return BLOCK_IO_FAILED;
    }
    copy_bytes(cache_buffer,dma_buffer,CACHE_SECTORS*SECTOR_BYTES);
    cache_first=0;cache_count=CACHE_SECTORS;
    return BLOCK_READY;
}

int kblock_read(uint32_t sector,uint16_t count,void *destination)
{
    uint8_t *out=(uint8_t *)destination;
    uint32_t bytes;
    if(!cache_buffer||!dma_buffer||!destination||!count||count>CACHE_SECTORS||
       (uint32_t)count-1u>0xffffffffUL-sector)return 0;
    if(sector>=cache_first&&sector+(uint32_t)count<=cache_first+cache_count){
        copy_bytes(out,cache_buffer+(sector-cache_first)*SECTOR_BYTES,(uint32_t)count*SECTOR_BYTES);
        return 1;
    }
    if(!kernel_acsi_read_blocks(block_device,sector,count,dma_buffer))return 0;
    bytes=(uint32_t)count*SECTOR_BYTES;
    copy_bytes(cache_buffer,dma_buffer,bytes);copy_bytes(out,dma_buffer,bytes);
    cache_first=sector;cache_count=count;return 1;
}


int kblock_write(uint32_t s,uint16_t n,const void *p){(void)s;(void)n;(void)p;return 0;}
