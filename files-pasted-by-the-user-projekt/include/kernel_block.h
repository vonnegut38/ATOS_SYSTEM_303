#ifndef ASF_KERNEL_BLOCK_H
#define ASF_KERNEL_BLOCK_H

#include <stdint.h>

enum block_state {BLOCK_NO_MEMORY,BLOCK_IO_FAILED,BLOCK_READY};

enum block_state kblock_init(uint8_t device);
int kblock_read(uint32_t sector,uint16_t count,void *destination);
int kblock_write(uint32_t sector,uint16_t count,const void *source);
int kblock_flush(void);
void kblock_cache_enable(uint8_t on);
void kblock_cache_release(void);
uint8_t kblock_cache_enabled(void);
int kblock_cache_is_tt(void);
uint32_t kblock_cache_hits(void);
uint32_t kblock_cache_misses(void);
uint32_t kblock_cache_flushes(void);
uint32_t kblock_cache_slots(void);

#endif
