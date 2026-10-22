#include <stdlib.h>
#include <stdint.h>
void *kmem_alloc(uint32_t bytes,uint32_t alignment,uint16_t owner,uint16_t flags,int prefer_tt){(void)alignment;(void)owner;(void)flags;(void)prefer_tt;return calloc(1,bytes);}
int kmem_free(void *p,uint16_t owner){(void)owner;free(p);return 0;}
void kvideo_shifter_base(const volatile uint8_t *s){(void)s;}
