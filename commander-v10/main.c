#include "system.h"
#include "diagnostics.h"
#include "console.h"
#include "lang.h"
#include "config.h"
#include "tos_loader.h"

static int autoboot_countdown(void)
{
    uint8_t seconds,old_control=REG8(MFP_TCDCR),old_data=REG8(MFP_TCDR),last;
    uint8_t delay=g_config.boot_delay;
    int result=-1;
    if(delay>30u)delay=30u;
    if(!delay)return -1;   /* 0 = do not wait for a key */
    REG8(MFP_TCDCR)=(uint8_t)((old_control&0x0fu)|0x70u);
    REG8(MFP_TCDR)=246u; /* 2.4576 MHz / 200 / 246: about 50 ticks/s. */
    last=REG8(MFP_TCDR);
    for(seconds=delay;seconds>0u;--seconds){
        uint8_t ticks=50u;post_draw_countdown(seconds);
        while(ticks){
            uint8_t now=REG8(MFP_TCDR);int key=ikbd_scancode();
            if(key>=0){result=key;goto done;}
            if(now!=last){last=now;--ticks;}
        }
    }
done:
    REG8(MFP_TCDCR)=(uint8_t)(old_control&0x0fu);
    REG8(MFP_TCDR)=old_data;
    REG8(MFP_TCDCR)=old_control;
    console_clear_text_row(21);
    return result;
}

void firmware_main(void)
{
    int key,pending;
    uint8_t selected=0,nav_ready=1;
    lang_init();                 /* built-in English; disk language loads later */
    g_system.ram_bytes = memory_detect();
    g_system.cpu_type = cpu_detect_type();
    g_system.fpu_type = cpu_detect_fpu(g_system.cpu_type);
    machine_detect(&g_system);
    g_system.tt_ram_bytes = memory_detect_fast();
    expansions_detect(&g_system);
    (void)cfg_load();                     /* \\BIOS\\BIOS.CFG; defaults on error */
    if(g_config.boot_valid){
        g_system.boot_acsi_id=g_config.boot_id;
        g_system.boot_partition=g_config.boot_part;
        g_system.boot_selection_valid=1u;
    }
    g_system.video_preference=g_config.video_driver;
    if(g_system.blitter_present)g_system.blitter_enabled=(uint8_t)(g_config.blitter?1u:0u);
    lang_set_code((const char *)g_config.language);
    /* v10: optional headless compatibility boot - load the first ROM TOS/EmuTOS
       image through the 68030 MMU, without any keyboard interaction. */
    if(g_config.auto_tos&&g_system.boot_selection_valid){
        tos_catalog_t cat;uint8_t ci;
        tos_catalog_scan(&cat,g_system.boot_acsi_id,g_system.acsi_part_start[g_system.boot_partition]);
        for(ci=0u;ci<cat.count;++ci){
            if(cat.image[ci].loadable){
                (void)tos_load_once(&cat.image[ci]);   /* RAM TOS from a .PRG payload */
            }else if(cat.image[ci].compatible&&cat.image[ci].rom_address&&g_system.cpu_type>=CPU_68030){
                (void)mmu_boot_rom(&cat.image[ci]);    /* ROM image through the MMU */
            }
        }
    }
    rtc_init();
    shifter_init(); ikbd_init(); g_system.keyboard_present = 1;
    post_run();
    pending=autoboot_countdown();
    if(pending==-1){boot_stage2_run();post_run();}
    for (;;) {
        key=pending>=0?pending:ikbd_scancode();pending=-1;
        if (key==0x148 || key==0x150) { nav_ready=1; continue; }
        if (key==0x48 && nav_ready) {
            nav_ready=0; selected=selected?selected-1:6; post_draw_choices(selected); continue;
        }
        if (key==0x50 && nav_ready) {
            nav_ready=0; selected=(uint8_t)((selected+1)%7); post_draw_choices(selected); continue;
        }
        if (key==0x3b || ((key==0x1c || key==0x72) && selected==0)) {
            setup_run(); selected=0; post_run(); continue;
        }
        if (key==0x3c || ((key==0x1c || key==0x72) && selected==1)) {
            diagnostics_run(); selected=0; post_run(); continue;
        }
        if (key==0x3d || ((key==0x1c || key==0x72) && selected==2)) {
            hardware_inventory_run(); selected=0; post_run(); continue;
        }
        if (key==0x3e || ((key==0x1c || key==0x72) && selected==3)) {
            boot_menu_run(); selected=0; post_run(); continue;
        }
        if (key==0x3f || ((key==0x1c || key==0x72) && selected==4)) {
            tos_menu_run(); selected=0; post_run(); continue;
        }
        if (key==0x44 || ((key==0x1c || key==0x72) && selected==5)) {
            boot_stage2_run(); selected=0; post_run(); continue;
        }
        if ((key==0x1c || key==0x72) && selected==6) {
            boot_stage2_run(); selected=0; post_run(); continue;
        }
        if (key==0x01) { selected=0; post_run(); }
    }
}
