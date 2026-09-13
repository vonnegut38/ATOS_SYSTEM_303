/* Block layer with a SMARTDRV-style write-back sector cache.
 *
 * The old code kept a single sector window (and re-read every miss).  This
 * version keeps a direct-mapped cache of CACHE_SLOTS sectors in TT-RAM,
 * reads ahead on a miss and defers writes (dirty slots) until eviction or
 * an explicit kblock_flush().  kblock_flush() must be called before the
 * machine is reset or when persistence is required. */
#include "kernel_block.h"
#include "kernel_memory.h"
#include "kernel_module.h"

#define BLOCK_OWNER    0x0101u
#define SECTOR_BYTES   512u
#define CACHE_SLOTS    2000u        /* ~1 MB of 512-byte sectors */
#define READAHEAD      32u          /* sectors per fill */

typedef struct {uint32_t sector;uint8_t valid,dirty,pad0,pad1;} cache_tag_t;

static uint8_t *cache_data;         /* CACHE_SLOTS * 512 */
static cache_tag_t *cache_tags;
static uint8_t *dma_buffer;         /* READAHEAD * 512, ST-RAM for ACSI DMA */
static uint8_t block_device;
static uint8_t cache_enabled;
static uint32_t cache_hits,cache_misses,cache_flushes;

static int alloc_cache(void)
{
    uint32_t i;
    if(cache_data&&cache_tags)return 1;
    if(!cache_data)cache_data=kmem_alloc(CACHE_SLOTS*SECTOR_BYTES,512u,BLOCK_OWNER,
                                         KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,1);
    if(!cache_tags)cache_tags=kmem_alloc(CACHE_SLOTS*sizeof(cache_tag_t),16u,BLOCK_OWNER,
                                         KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
    if(!cache_data||!cache_tags)return 0;
    for(i=0u;i<CACHE_SLOTS;i++){cache_tags[i].valid=0u;cache_tags[i].dirty=0u;cache_tags[i].sector=0u;}
    return 1;
}
void kblock_cache_enable(uint8_t on)
{
    if(on){if(!alloc_cache()){cache_enabled=0u;return;}cache_enabled=1u;}
    else cache_enabled=0u;
}
uint8_t kblock_cache_enabled(void){return cache_enabled;}
/* Turn the cache off and release its memory (SMARTDRV -off). */
void kblock_cache_release(void)
{
    kblock_flush();
    if(cache_data){kmem_free(cache_data,BLOCK_OWNER);cache_data=0;}
    if(cache_tags){kmem_free(cache_tags,BLOCK_OWNER);cache_tags=0;}
    cache_enabled=0u;
}

static void copy_bytes(uint8_t *destination,const uint8_t *source,uint32_t count)
{while(count--)*destination++=*source++;}

int kblock_cache_is_tt(void)
{return (uint32_t)(uintptr_t)cache_data>=0x01000000UL;}

uint32_t kblock_cache_hits(void){return cache_hits;}
uint32_t kblock_cache_misses(void){return cache_misses;}
uint32_t kblock_cache_flushes(void){return cache_flushes;}
uint32_t kblock_cache_slots(void){return CACHE_SLOTS;}

static uint32_t slot_of(uint32_t sector){return sector%((uint32_t)CACHE_SLOTS);}

static int flush_slot(uint32_t slot)
{
    if(!cache_tags[slot].valid||!cache_tags[slot].dirty)return 1;
    if(!kernel_acsi_write_sector(block_device,cache_tags[slot].sector,
                                 cache_data+(uint32_t)slot*SECTOR_BYTES))return 0;
    cache_tags[slot].dirty=0;
    cache_flushes++;
    return 1;
}

/* Read one sector (and a read-ahead window) from the device into the cache. */
static int fill_sector(uint32_t sector)
{
    uint32_t slot=slot_of(sector);
    uint32_t base;
    uint16_t i;

    if(!flush_slot(slot))return 0;
    base=sector&~(uint32_t)(READAHEAD-1u);
    if(base<=sector&&sector-base<READAHEAD&&
       kernel_acsi_read_blocks(block_device,base,READAHEAD,dma_buffer)){
        for(i=0u;i<READAHEAD;i++){
            uint32_t s=base+i,sl=slot_of(s);
            if(!flush_slot(sl))continue;
            copy_bytes(cache_data+(uint32_t)sl*SECTOR_BYTES,
                       dma_buffer+(uint32_t)i*SECTOR_BYTES,SECTOR_BYTES);
            cache_tags[sl].sector=s;cache_tags[sl].valid=1;cache_tags[sl].dirty=0;
        }
        return 1;
    }
    if(!kernel_acsi_read_blocks(block_device,sector,1u,dma_buffer))return 0;
    slot=slot_of(sector);
    if(!flush_slot(slot))return 0;
    copy_bytes(cache_data+(uint32_t)slot*SECTOR_BYTES,dma_buffer,SECTOR_BYTES);
    cache_tags[slot].sector=sector;cache_tags[slot].valid=1;cache_tags[slot].dirty=0;
    return 1;
}

enum block_state kblock_init(uint8_t device)
{
    dma_buffer=kmem_alloc(READAHEAD*SECTOR_BYTES,512u,BLOCK_OWNER,
                          KMEM_READ|KMEM_WRITE|KMEM_SUPERVISOR,0);
    if(!dma_buffer||!alloc_cache())return BLOCK_NO_MEMORY;
    block_device=device;cache_hits=cache_misses=cache_flushes=0u;
    if(!kernel_acsi_read_blocks(device,0,1u,dma_buffer))return BLOCK_IO_FAILED;
    return BLOCK_READY;
}

int kblock_read(uint32_t sector,uint16_t count,void *destination)
{
    uint8_t *out=(uint8_t *)destination;
    uint16_t i;
    if(!destination||!count||
       (uint32_t)count-1u>0xffffffffUL-sector)return 0;
    if(!cache_enabled||!cache_data||!cache_tags){
        uint16_t done=0u;
        while(done<count){
            uint16_t chunk=(uint16_t)(count-done);
            if(chunk>READAHEAD)chunk=READAHEAD;
            if(!kernel_acsi_read_blocks(block_device,sector+done,chunk,dma_buffer))return 0;
            copy_bytes(out,dma_buffer,(uint32_t)chunk*SECTOR_BYTES);
            out+=(uint32_t)chunk*SECTOR_BYTES;
            done=(uint16_t)(done+chunk);
        }
        return 1;
    }
    for(i=0u;i<count;i++){
        uint32_t s=sector+i,slot=slot_of(s);
        if(cache_tags[slot].valid&&cache_tags[slot].sector==s){
            cache_hits++;
        }else{
            cache_misses++;
            if(!fill_sector(s))return 0;
            slot=slot_of(s);
        }
        copy_bytes(out,cache_data+(uint32_t)slot*SECTOR_BYTES,SECTOR_BYTES);
        out+=SECTOR_BYTES;
    }
    return 1;
}

int kblock_write(uint32_t sector,uint16_t count,const void *source)
{
    const uint8_t *in=(const uint8_t *)source;
    uint16_t i;
    if(!source||!count||
       (uint32_t)count-1u>0xffffffffUL-sector)return 0;
    if(!cache_enabled||!cache_data||!cache_tags){
        uint16_t done=0u;
        while(done<count){
            uint16_t chunk=(uint16_t)(count-done);
            if(chunk>READAHEAD)chunk=READAHEAD;
            copy_bytes(dma_buffer,in,(uint32_t)chunk*SECTOR_BYTES);
            if(!kernel_acsi_write_blocks(block_device,sector+done,chunk,dma_buffer))return 0;
            in+=(uint32_t)chunk*SECTOR_BYTES;
            done=(uint16_t)(done+chunk);
        }
        return 1;
    }
    for(i=0u;i<count;i++){
        uint32_t s=sector+i,slot=slot_of(s);
        if(!(cache_tags[slot].valid&&cache_tags[slot].sector==s)){
            if(!flush_slot(slot))return 0;
            cache_tags[slot].valid=0u;
        }
        copy_bytes(cache_data+(uint32_t)slot*SECTOR_BYTES,in,SECTOR_BYTES);
        cache_tags[slot].sector=s;cache_tags[slot].valid=1u;cache_tags[slot].dirty=0u;
        /* Write-through: push the sector to the device immediately. */
        if(!kernel_acsi_write_sector(block_device,s,cache_data+(uint32_t)slot*SECTOR_BYTES))return 0;
        in+=SECTOR_BYTES;
    }
    return 1;
}

int kblock_flush(void)
{
    uint32_t slot;
    if(!cache_tags)return 0;
    for(slot=0u;slot<(uint32_t)CACHE_SLOTS;slot++)if(!flush_slot(slot))return 0;
    return 1;
}
