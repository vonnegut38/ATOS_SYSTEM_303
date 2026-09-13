#include "system.h"
#include "diagnostics.h"
#include "console.h"
#include "config.h"
#include "ide.h"
#include "video8.h"

static uint8_t bios_ctrl,bios_alt;

void bios_warm_reset(void)
{
    __asm__ volatile("reset" ::: "memory");
    __asm__ volatile("move.l #0x00e00030,%%a0\n\tjmp (%%a0)" ::: "a0","memory");
    for(;;){}
}

/* Returns 1 when Ctrl+Alt+Del was pressed (the caller should warm reset). */
static int bios_cad_check(int key)
{
    uint8_t sc,up;
    if(key<0)return 0;
    sc=(uint8_t)key;up=(uint8_t)((key&0x100)!=0);
    if(sc==0x1du)bios_ctrl=up?0u:1u;
    else if(sc==0x38u)bios_alt=up?0u:1u;
    else if(sc==0x53u&&!up&&bios_ctrl&&bios_alt)return 1;
    return 0;
}

/* Countdown based on the MFP VBL input (GPIP bit 4).  This keeps the POST
 * visible for the configured number of seconds regardless of the CPU clock,
 * unlike the former MFP Timer-D scheme whose tick rate varied per machine. */
static int autoboot_countdown(void)
{
    uint8_t delay=g_config.boot_delay;
    uint16_t frame,frames;
    int result=-1;
    if(delay>30u)delay=30u;
    if(!delay)return -1;
    frames=(uint16_t)((uint16_t)delay*50u);
    for(frame=0u;frame<frames;++frame){
        uint32_t guard=12000ul;
        while((REG8(MFP_GPIP)&0x10u)&&guard)--guard;   /* wait for VBL low  */
        guard=12000ul;
        while(!(REG8(MFP_GPIP)&0x10u)&&guard)--guard;  /* wait for VBL high */
        if((frame%50u)==0u)post_draw_countdown((uint8_t)(delay-(frame/50u)));
        {int key=ikbd_scancode();if(key>=0){if(bios_cad_check(key))bios_warm_reset();result=key;break;}}
    }
    console_clear_text_row(22);
    return result;
}

void firmware_main(void)
{
    int key,pending;
    uint8_t selected=0,nav_ready=1;
    g_system.ram_bytes = memory_detect();
    g_system.cpu_type = cpu_detect_type();
    g_system.fpu_type = cpu_detect_fpu(g_system.cpu_type);
    machine_detect(&g_system);
    g_system.tt_ram_bytes = memory_detect_fast();
    expansions_detect(&g_system);
    g_config_loaded=(uint8_t)cfg_load();
    if(!g_config_loaded)g_config_saved=(uint8_t)cfg_save();
    if(g_config.boot_valid){
        g_system.boot_acsi_id=g_config.boot_id;
        g_system.boot_partition=g_config.boot_part;
        g_system.boot_selection_valid=1u;
        g_system.boot_ide=(uint8_t)((g_config.boot_id&IDE_ID_FLAG)?1u:0u);
    }
    g_system.video_preference=g_config.video_pref;
    if(g_system.blitter_present)g_system.blitter_enabled=(uint8_t)(g_config.blitter?1u:0u);
    rtc_init();
    shifter_init(); ikbd_init(); g_system.keyboard_present = 1;
    post_run();
    pending=autoboot_countdown();
    if(pending==-1){boot_stage2_run();post_run();}
    for (;;) {
        key=pending>=0?pending:ikbd_scancode();pending=-1;
        if(bios_cad_check(key))bios_warm_reset();
        if (key==0x148 || key==0x150) { nav_ready=1; continue; }
        if (key==0x48 && nav_ready) {
            nav_ready=0; selected=selected?selected-1:7; post_draw_choices(selected); continue;
        }
        if (key==0x50 && nav_ready) {
            nav_ready=0; selected=(uint8_t)((selected+1)%8); post_draw_choices(selected); continue;
        }
        if (key==0x3b || ((key==0x1c || key==0x72) && selected==0)) {
            setup_run(); selected=0; post_run(); continue;
        }
        if (key==0x3c || ((key==0x1c || key==0x72) && selected==1)) {
            tos_menu_run(); selected=0; post_run(); continue;
        }
        if (key==0x3d || ((key==0x1c || key==0x72) && selected==2)) {
            hardware_inventory_run(); selected=0; post_run(); continue;
        }
        if (key==0x3e || ((key==0x1c || key==0x72) && selected==3)) {
            boot_menu_run(); selected=0; post_run(); continue;
        }
        if (key==0x3f || ((key==0x1c || key==0x72) && selected==4)) {
            diagnostics_run(); selected=0; post_run(); continue;
        }
        if (key==0x40 || ((key==0x1c || key==0x72) && selected==5)) {
            video8_demo(); selected=0; post_run(); continue;
        }
        if (key==0x44 || ((key==0x1c || key==0x72) && selected==6)) {
            boot_stage2_run(); selected=0; post_run(); continue;
        }
        if ((key==0x1c || key==0x72) && selected==7) {
            boot_stage2_run(); selected=0; post_run(); continue;
        }
        if (key==0x01) { selected=0; post_run(); }
    }
}
