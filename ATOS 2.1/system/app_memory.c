/* Freestanding support for small compiler-generated structure copies. */
#include <stddef.h>
void *memcpy(void *dst,const void *src,size_t size)
{
    volatile unsigned char *d=dst;
    const volatile unsigned char *s=src;
    while(size--)*d++=*s++;
    return dst;
}
