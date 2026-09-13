#include "kernel_surface.h"

typedef struct {uint32_t owner;uint16_t width,height,flags,generation;uint8_t used;} surface_slot_t;
static surface_slot_t slots[KSURFACE_MAX];
static uint16_t maximum_width,maximum_height;

static uint32_t make_handle(uint8_t slot,uint16_t generation)
{return ((uint32_t)generation<<8)|(uint32_t)(slot+1u);}

static surface_slot_t *lookup(uint32_t owner,uint32_t handle)
{
    uint32_t index=(handle&0xffu);surface_slot_t *slot;
    if(!index||index>KSURFACE_MAX)return 0;
    slot=&slots[index-1u];
    if(!slot->used||slot->owner!=owner||make_handle((uint8_t)(index-1u),slot->generation)!=handle)return 0;
    return slot;
}

void ksurface_init(uint16_t screen_width,uint16_t screen_height)
{
    uint8_t i;maximum_width=screen_width;maximum_height=screen_height;
    for(i=0;i<KSURFACE_MAX;++i){slots[i].used=0;slots[i].owner=0;
        if(!slots[i].generation)slots[i].generation=1u;}
}

uint32_t ksurface_create(uint32_t owner,uint16_t width,uint16_t height,uint16_t flags)
{
    uint8_t i;if(!owner||!width||!height||width>maximum_width||height>maximum_height)return 0;
    if(flags&~KSURFACE_FULLSCREEN)return 0;
    for(i=0;i<KSURFACE_MAX;++i)if(!slots[i].used){slots[i].used=1;slots[i].owner=owner;
        slots[i].width=width;slots[i].height=height;slots[i].flags=flags;
        return make_handle(i,slots[i].generation);}
    return 0;
}

int ksurface_destroy(uint32_t owner,uint32_t handle)
{
    surface_slot_t *slot=lookup(owner,handle);if(!slot)return 0;
    slot->used=0;slot->owner=0;if(++slot->generation==0u)slot->generation=1u;return 1;
}

int ksurface_query(uint32_t owner,uint32_t handle,ksurface_info_t *info)
{
    surface_slot_t *slot=lookup(owner,handle);if(!slot||!info)return 0;
    info->handle=handle;info->owner=owner;info->width=slot->width;info->height=slot->height;
    info->flags=slot->flags;return 1;
}

void ksurface_release_owner(uint32_t owner)
{uint8_t i;if(!owner)return;for(i=0;i<KSURFACE_MAX;++i)if(slots[i].used&&slots[i].owner==owner)
    (void)ksurface_destroy(owner,make_handle(i,slots[i].generation));}

int ksurface_self_test(void)
{
    uint32_t first,second,handles[KSURFACE_MAX];ksurface_info_t info;uint8_t i;
    ksurface_init(maximum_width,maximum_height);
    first=ksurface_create(11u,maximum_width,maximum_height,KSURFACE_FULLSCREEN);
    if(!first||!ksurface_query(11u,first,&info)||info.owner!=11u||
       ksurface_query(22u,first,&info)||ksurface_destroy(22u,first))return 0;
    if(!ksurface_destroy(11u,first)||ksurface_query(11u,first,&info))return 0;
    second=ksurface_create(11u,1u,1u,0u);if(!second||second==first)return 0;
    (void)ksurface_destroy(11u,second);
    for(i=0;i<KSURFACE_MAX;++i){handles[i]=ksurface_create(22u,1u,1u,0u);if(!handles[i])return 0;}
    if(ksurface_create(22u,1u,1u,0u))return 0;
    ksurface_release_owner(22u);
    for(i=0;i<KSURFACE_MAX;++i)if(ksurface_query(22u,handles[i],&info))return 0;
    ksurface_init(maximum_width,maximum_height);return 1;
}
