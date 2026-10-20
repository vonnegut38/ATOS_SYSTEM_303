#ifndef ASF_KERNEL_BLOCK_H
#define ASF_KERNEL_BLOCK_H

#include <stdint.h>

enum block_state {BLOCK_NO_MEMORY,BLOCK_IO_FAILED,BLOCK_READY};

enum block_state kblock_init(uint8_t device);
/* Switch the active block device (ACSI id 0..7 or IDE 0x80/0x81) and clear the
   sector cache, which is keyed by sector only.  Used by select_drive() when a
   volume lives on another transport. */
int kblock_select(uint8_t device);
/* Whether the active device can actually be read: kblock_select() clears this
   before probing and only sets it again on a successful read, so a failed
   transport switch must be visible to callers that cache a mount context. */
int kblock_is_ready(void);
int kblock_read(uint32_t sector,uint16_t count,void *destination);
int kblock_cache_is_tt(void);
uint32_t kblock_search_count(void);
uint32_t kblock_cache_sectors(void);
/* Clearing drops only clean read copies; there is nothing to flush. */
void kblock_cache_clear(void);
/* searches counts set/way probes: a read served entirely from cache should
   cost one per sector, not two. */
typedef struct {uint32_t hits,misses,reads,writes,searches;} kblock_stats_t;
void kblock_cache_stats(kblock_stats_t *out);
int kblock_write(uint32_t sector,uint16_t count,const void *source);

/* Diagnostics are read-only and use the existing ST-RAM DMA staging buffer. */
int kblock_read_uncached(uint32_t sector,uint16_t count,void *destination);
uint8_t kblock_device(void);
void kblock_identify(char name[32],uint32_t *sectors,uint32_t *sector_bytes);

/* Compatibility shims for code written against the older block API (the TOS
   persona uses these names). Thin inline wrappers, no extra state. */
static inline int kblock_cache_enabled(void){return kblock_cache_sectors()?1:0;}
static inline uint32_t kblock_cache_slots(void){return kblock_cache_sectors();}
static inline uint32_t kblock_cache_hits(void){kblock_stats_t s;kblock_cache_stats(&s);return s.hits;}
static inline uint32_t kblock_cache_misses(void){kblock_stats_t s;kblock_cache_stats(&s);return s.misses;}
static inline uint32_t kblock_cache_flushes(void){kblock_stats_t s;kblock_cache_stats(&s);return s.writes;}
static inline void kblock_flush(void){kblock_cache_clear();}
static inline void kblock_cache_release(void){kblock_cache_clear();}
static inline int kblock_cache_is_enabled(void){return kblock_cache_sectors()?1:0;}
/* The cache is always on in this kernel; the SMARTDRV toggle is a no-op. */
static inline void kblock_cache_enable(uint32_t on){(void)on;}

#endif
