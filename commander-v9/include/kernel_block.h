#ifndef ASF_KERNEL_BLOCK_H
#define ASF_KERNEL_BLOCK_H

#include <stdint.h>

enum block_state {BLOCK_NO_MEMORY,BLOCK_IO_FAILED,BLOCK_READY};

enum block_state kblock_init(uint8_t device);
int kblock_read(uint32_t sector,uint16_t count,void *destination);
int kblock_cache_is_tt(void);
uint32_t kblock_cache_sectors(void);
/* Clearing drops only clean read copies; there is nothing to flush. */
void kblock_cache_clear(void);
typedef struct {uint32_t hits,misses,reads,writes;} kblock_stats_t;
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
