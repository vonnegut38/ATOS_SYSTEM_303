#include "system.h"
#include "diagnostics.h"
#include "console.h"
#include "lang.h"
#include "config.h"
#include "tos_loader.h"
#include "vofa_et4000.h"
#include "kernel_blitter.h"

extern void _reset(void);

/* Auto-TOS diagnostics: readable through the BIOS symbol table, so a silent
   no-op can be told apart from a refused image. */
uint8_t g_dbg_auto_tos,g_dbg_boot_sel,g_dbg_boot_id,g_dbg_boot_part,g_dbg_cat_count;
uint8_t g_dbg_img_loadable[4],g_dbg_img_compat[4];
uint32_t g_dbg_img_size[4],g_dbg_img_name[4];
/* Warm reset: reset the peripherals, then re-enter the ROM startup. Used by
   Ctrl+Alt+Del (both here and, via the handoff, from the kernel). */
void bios_reset(void)
{
    __asm__ volatile("move.w #0x2700,%sr");
    __asm__ volatile("reset");
    _reset();
}
/* The kernel reached the shell on the card, so the card path is good: clear the
   failover latch. Done here because the kernel has no usable IDE write path
   when the ACSI module is absent (the ACSI DMA registers do not drive TF536 IDE). */
void bios_video_boot_ok(void)
{
    if(g_config.video_pending){g_config.video_pending=0u;(void)cfg_save();}
}
/* Hold Alt from power-on to keep the ROM console on the Shifter and skip the
   adapter entirely: the way back when a card cannot produce a usable picture.
   Bounded by MFP timer D, same 50 Hz source the boot countdown uses. */
static int alt_failsafe_requested(void)
{
    uint8_t control=REG8(MFP_TCDCR),data=REG8(MFP_TCDR),last;
    uint32_t ticks=0;int result=0;
    REG8(MFP_TCDCR)=(uint8_t)((control&0x0fu)|0x70u);
    REG8(MFP_TCDR)=246u;
    last=REG8(MFP_TCDR);
    while(ticks<50u){   /* ~1 s: a press any time before or during this window */
        uint8_t now=REG8(MFP_TCDR);int key=ikbd_scancode();
        if(now>last)++ticks;
        last=now;
        if(key>=0){
            /* Alt make, or its break if the one-byte ACIA already overwrote
               the make: a break can only follow a press. */
            if(key==0x38||key==0x138){result=1;break;}
            if(!(key&0x100))break;             /* another key: not the failsafe */
        }
    }
    REG8(MFP_TCDCR)=(uint8_t)(control&0x0fu);
    REG8(MFP_TCDR)=data;
    REG8(MFP_TCDCR)=control;
    return result;
}

static int autoboot_countdown(void)
{
    uint8_t seconds,old_control=REG8(MFP_TCDCR),old_data=REG8(MFP_TCDR),last;
    uint8_t delay=g_config.boot_delay;
    int result=-1;
    if(delay>30u)delay=30u;
    if(!delay)return -1;   /* 0 = do not wait for a key */
    /* Keep the quick-boot cap, but honour a saved one-second delay too.
       Zero skips the countdown; any key opens the BIOS menu. */
    if(delay>2u)delay=2u;
    REG8(MFP_TCDCR)=(uint8_t)((old_control&0x0fu)|0x70u);
    REG8(MFP_TCDR)=246u; /* 2.4576 MHz / 200 / 246: 50 reloads per second. */
    last=REG8(MFP_TCDR);
    for(seconds=delay;seconds>0u;--seconds){
        uint8_t ticks=50u;post_draw_countdown(seconds);
        while(ticks){
            /* Timer D counts DOWN at 12288 Hz and reloads from 246, so one
               50 Hz tick is the wrap (now > last), not every change. */
            uint8_t now=REG8(MFP_TCDR);int key=ikbd_scancode();
            if(key>=0){result=key;goto done;}
            if(now>last)--ticks;
            last=now;
        }
    }
done:
    REG8(MFP_TCDCR)=(uint8_t)(old_control&0x0fu);
    REG8(MFP_TCDR)=old_data;
    REG8(MFP_TCDCR)=old_control;
    console_clear_text_row(23);
    return result;
}

void firmware_main(void)
{
    int key,pending,failover_notice=0,card_unsupported=0;
    uint8_t selected=0,nav_ready=1,ctrl_down=0,alt_down=0;
    lang_init();                 /* built-in English; disk language loads later */
    /* The ACIA is already up from _reset (boot/reset.S) so a key held at
       power-on survives; ikbd_init() must NOT run again or it master-resets
       the ACIA and flushes that byte. */
    g_system.keyboard_present = 1;
    g_system.ram_bytes = memory_detect();
    shifter_init();
    post_draw_logo();
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
    /* A machine with no readable clock (plain ST/Mega ST + accelerator) can
       still carry a user-set value; it is reported as nominal, not measured. */
    if(!g_system.clock_known&&g_config.cpu_clock_mhz){g_system.clock_mhz=g_config.cpu_clock_mhz;g_system.clock_known=2u;}
    /* v10: optional headless compatibility boot - load the first ROM TOS/EmuTOS
       image through the 68030 MMU, without any keyboard interaction. */
    g_dbg_auto_tos=g_config.auto_tos;
    g_dbg_boot_sel=g_system.boot_selection_valid;
    g_dbg_boot_id=g_system.boot_acsi_id;
    g_dbg_boot_part=g_system.boot_partition;
    if(g_config.auto_tos&&g_system.boot_selection_valid){
        tos_catalog_t cat;uint8_t ci;
        tos_catalog_scan(&cat,g_system.boot_acsi_id,g_system.acsi_part_start[g_system.boot_partition]);
        g_dbg_cat_count=cat.count;
        for(ci=0u;ci<cat.count&&ci<4u;++ci){
            g_dbg_img_loadable[ci]=cat.image[ci].loadable;
            g_dbg_img_compat[ci]=cat.image[ci].compatible;
            g_dbg_img_size[ci]=cat.image[ci].size;
            g_dbg_img_name[ci]=((uint32_t)(uint8_t)cat.image[ci].name[0]<<24)|
                               ((uint32_t)(uint8_t)cat.image[ci].name[1]<<16)|
                               ((uint32_t)(uint8_t)cat.image[ci].name[2]<<8)|
                                (uint32_t)(uint8_t)cat.image[ci].name[3];
        }
        for(ci=0u;ci<cat.count;++ci){
            if(cat.image[ci].loadable){
                (void)tos_load_once(&cat.image[ci]);   /* RAM TOS from a .PRG payload */
            }else if(cat.image[ci].compatible&&cat.image[ci].rom_address&&g_system.cpu_type>=CPU_68030){
                (void)mmu_boot_rom(&cat.image[ci]);    /* ROM image through the MMU */
            }
        }
    }
    rtc_init();
    /* The boot device probe can miss the disk this early; retry the config once
       the machine is further along, and re-apply what depends on it. */
    if(!g_config_loaded&&cfg_load()){
        g_system.video_preference=g_config.video_driver;
        if(g_system.blitter_present)g_system.blitter_enabled=(uint8_t)(g_config.blitter?1u:0u);
        lang_set_code((const char *)g_config.language);
        if(g_config.boot_valid){g_system.boot_acsi_id=g_config.boot_id;g_system.boot_partition=g_config.boot_part;g_system.boot_selection_valid=1u;}
        if(!g_system.clock_known&&g_config.cpu_clock_mhz){g_system.clock_mhz=g_config.cpu_clock_mhz;g_system.clock_known=2u;}
    }
    /* Milestone: bring the adapter up in the ROM right after the Shifter mode
       is set (the console is blank before that) and before the first message,
       so the whole boot log runs on the card with no later mode switch.
       Falls back to the Shifter when the card or its aperture is unusable. */
    vofa_rom_status=0x101u;
    if(g_config.video_driver==CFG_VIDEO_SHIFTER)g_system.video_force_shifter=1u;
    if(alt_failsafe_requested()){
        g_system.video_force_shifter=1u;
        /* Alt doubles as the retry gesture: drop the failover latch so the next
           boot may try the adapter again. */
        if(g_config.video_pending){g_config.video_pending=0u;(void)cfg_save();}
    }
    /* A card boot that never reached the kernel leaves this set, so the machine
       is not sent down the same blind path twice. Alt (above) clears it. */
    if(g_config.video_pending){
        g_system.video_force_shifter=1u;
        failover_notice=1;   /* post_run() clears the screen, so draw it later */
    }
    if(g_system.nova_present&&nova_mem_base_is_supported(&g_system)&&
       g_config.video_driver!=CFG_VIDEO_SHIFTER&&
       !g_system.video_force_shifter){
        vofa_rom_status=0x102u;
        if(vofa_rom_init()==VOFA_OK){
            /* Commit to the card path: latch the attempt before the console
               moves, so a loader/kernel hang is remembered as a failed card
               boot. The kernel clears it once it reaches the shell. */
            if(!g_config.video_pending){g_config.video_pending=1u;(void)cfg_save();}
            vofa_rom_status=0x400u;
            console_set_screen((volatile uint8_t *)(uintptr_t)VOFA_MEM);
            g_system.nova_initialized=1u;
            g_system.video_active=VIDEO_ACTIVE_NOVA;
        }else if(g_config.video_pending){
            /* The adapter never came up, so no card boot is in flight: drop a
               stale latch instead of forcing the Shifter on the next start. */
            g_config.video_pending=0u;(void)cfg_save();
        }
    }else if(g_system.nova_present&&!nova_mem_base_is_supported(&g_system)){
        /* The card answered, but through an aperture this ROM has no driver
           for (Nova/Mach on a TT or Falcon, or the STGA window shadowed by
           12 MB or more of ST-RAM).  Say so instead of silently staying on the
           Shifter, which used to look exactly like "no card installed". */
        card_unsupported=1;
        if(g_config.video_pending){g_config.video_pending=0u;(void)cfg_save();}
    }
    kblit_init(g_system.blitter_present,g_system.blitter_enabled);
    post_run();
    /* post_run() clears the screen, so the notice is drawn on the free banner
       row to stay readable through the boot countdown. */
    if(failover_notice){console_set_cursor(0,22);console_puts("VIDEO: SHIFTER (CARD DISABLED AFTER FAILED BOOT)");}
    else if(card_unsupported){console_set_cursor(0,22);console_puts("VIDEO: SHIFTER (CARD FOUND, APERTURE NOT SUPPORTED)");}
    if(post_tests_failed()){
        /* Emergency POST screen: keep the menu up instead of auto-booting a
           machine that just failed its self-test.  F2 runs diagnostics; the
           operator can still pick F4/CONTINUE or save with F10. */
        pending=-1;
    }else{
        pending=autoboot_countdown();
        if(pending==-1){
            /* Normal boot: KEEP the screen as it is.  Clearing it here and
               re-running post_run() wiped the logo and redrew the whole page
               just before loading the kernel - the flash the user sees.  Only
               the status/progress lines change from now on. */
            boot_stage2_run();
        }
    }
    for (;;) {
        key=pending>=0?pending:ikbd_scancode();pending=-1;
        /* Alt during the boot menu is the reliable video failsafe: bring the
           console back to the Shifter and keep the kernel there too. */
        if((key==0x38||key==0x138)&&!g_system.video_force_shifter){
            g_system.video_force_shifter=1u;
            console_set_screen((volatile uint8_t *)(uintptr_t)SCREEN_BASE);
            console_puts("VIDEO: SHIFTER (ALT)\n");
            boot_stage2_run();post_run();continue;   /* keep booting on the Shifter */
        }
        /* Ctrl+Alt+Del reboots, like the reset button. */
        if(key==0x1d)ctrl_down=1;
        else if(key==0x11d)ctrl_down=0;
        else if(key==0x38)alt_down=1;
        else if(key==0x138)alt_down=0;
        else if(key==0x53&&ctrl_down&&alt_down)bios_reset();
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