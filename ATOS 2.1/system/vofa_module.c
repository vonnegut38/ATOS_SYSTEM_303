#include "kernel_module.h"
#include "kernel_video_service.h"

/* The module is loaded at an arbitrary base and the loader does not relocate,
   so the service vtable must be filled with runtime (PC-relative) addresses at
   entry time, exactly like the assembly modules do with lea sym(%pc). */
static kernel_video_service_t vofa_service;

int vofa_module_main(const kernel_module_api_t *api)
{
    if(!api||api->magic!=KERNEL_MODULE_API_MAGIC)return 0;
    if(!api->register_service)return 0;
    vofa_service.magic=KERNEL_VIDEO_MAGIC;
    vofa_service.version=KERNEL_VIDEO_VERSION;
    vofa_service.size=(uint16_t)sizeof(vofa_service);
    vofa_service.init=vofa_et4000_init;
    vofa_service.set_mode=vofa_et4000_set_mode;
    vofa_service.mode_info=vofa_et4000_mode;
    vofa_service.mode_index=vofa_et4000_mode_index;
    vofa_service.set_palette=vofa_et4000_set_palette;
    vofa_service.dac_type=vofa_et4000_dac;
    vofa_service.dac_name=vofa_et4000_dac_name;
    return api->register_service(KERNEL_SERVICE_VIDEO,KERNEL_VIDEO_VERSION,&vofa_service);
}
