#ifndef ASF_KERNEL_SURFACE_H
#define ASF_KERNEL_SURFACE_H
#include <stdint.h>

#define KSURFACE_MAX 8u
#define KSURFACE_FULLSCREEN 0x0001u
typedef struct {uint32_t handle,owner;uint16_t width,height,flags;} ksurface_info_t;

void ksurface_init(uint16_t screen_width,uint16_t screen_height);
uint32_t ksurface_create(uint32_t owner,uint16_t width,uint16_t height,uint16_t flags);
int ksurface_destroy(uint32_t owner,uint32_t handle);
int ksurface_query(uint32_t owner,uint32_t handle,ksurface_info_t *info);
void ksurface_release_owner(uint32_t owner);
int ksurface_self_test(void);

#endif
