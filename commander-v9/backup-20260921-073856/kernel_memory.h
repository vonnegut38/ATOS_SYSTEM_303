#ifndef ASF_KERNEL_MEMORY_H
#define ASF_KERNEL_MEMORY_H
#include <stdint.h>

enum { KMEM_READ=1u,KMEM_WRITE=2u,KMEM_EXEC=4u,KMEM_SUPERVISOR=8u };
enum { KMEM_OWNER_KERNEL=1u };
typedef struct {uint32_t total,free,reserved,largest;} kmem_stats_t;

int kmem_init(uint32_t st_total,uint32_t tt_total,uint32_t kernel_end);
void *kmem_alloc(uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags,int prefer_tt);
void *kmem_alloc_user(uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags,int prefer_tt);
int kmem_free(void *pointer,uint16_t owner);
int kmem_change_owner(void *pointer,uint16_t old_owner,uint16_t new_owner);
uint16_t kmem_owner(const void *pointer);
int kmem_pin(void *pointer,uint16_t owner);
int kmem_release(void *pointer,uint16_t owner);
void kmem_get_st_stats(kmem_stats_t *stats);
void kmem_get_tt_stats(kmem_stats_t *stats);
int kmem_self_test(void);

#endif
