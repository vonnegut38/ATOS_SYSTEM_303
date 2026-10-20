#include "console.h"
#include "system.h"
#include "tos_loader.h"

static void draw(const tos_catalog_t *cat,uint8_t selected,uint8_t confirm)
{
    uint8_t i;
    console_clear();console_puts("F5 LOAD TOS\n\n");
    if(!g_system.boot_selection_valid){console_puts("SELECT ACSI PARTITION IN F4 FIRST\n\nESC BACK");return;}
    if(!cat->fat_valid){console_puts("PARTITION IS NOT SUPPORTED FAT16\n\nESC BACK");return;}
    if(!cat->directory_found){console_puts("DIRECTORY TOS_BIOS NOT FOUND\n\nESC BACK");return;}
    if(!cat->count){console_puts("NO TOS IMAGE FILES\n\nESC BACK");return;}
    for(i=0;i<cat->count;++i){const tos_image_t *im=&cat->image[i];console_set_inverse(i==selected);console_putc(' ');console_puts(im->name);console_puts("  TOS ");console_putc((char)('0'+((im->version>>8)&15u)));console_putc('.');console_putc((char)('0'+((im->version>>4)&15u)));console_putc((char)('0'+(im->version&15u)));if(im->loadable)console_puts(" RAM LOAD ");else if(im->compatible)console_puts(" ROM MAP  ");else console_puts(" INVALID  ");console_set_inverse(0);console_putc('\n');}
    if(confirm)console_puts("\nLOAD ONCE WILL REPLACE CURRENT BIOS\nENTER CONFIRM  ESC CANCEL");
    else console_puts("\nUP/DOWN SELECT  ENTER LOAD ONCE  ESC BACK");
}

void tos_menu_run(void)
{
    tos_catalog_t cat;uint8_t selected=0,ready=1,confirm=0;int key;
    if(g_system.boot_selection_valid)tos_catalog_scan(&cat,g_system.boot_acsi_id,g_system.acsi_part_start[g_system.boot_partition]);
    else {cat.count=0;cat.fat_valid=0;cat.directory_found=0;}
    draw(&cat,selected,confirm);
    for(;;){key=ikbd_scancode();if(key==0x148||key==0x150){ready=1;continue;}if(key==0x01){if(confirm){confirm=0;draw(&cat,selected,confirm);continue;}return;}if(!ready)continue;
        if(!confirm&&key==0x48&&cat.count){ready=0;selected=selected?selected-1:(uint8_t)(cat.count-1u);draw(&cat,selected,confirm);}
        else if(!confirm&&key==0x50&&cat.count){ready=0;selected=(uint8_t)((selected+1u)%cat.count);draw(&cat,selected,confirm);}
        else if((key==0x1c||key==0x72)&&cat.count){if(!cat.image[selected].loadable){if(cat.image[selected].compatible&&cat.image[selected].rom_address&&g_system.cpu_type>=CPU_68030){console_clear();console_puts("LOADING EMUTOS VIA MMU...");(void)mmu_boot_rom(&cat.image[selected]);}console_puts("\n\nROM IMAGE NEEDS MMU OR ROM BANK");continue;}if(!confirm){confirm=1;draw(&cat,selected,confirm);}else{console_clear();console_puts("LOADING EMUTOS FROM ACSI...");if(!tos_load_once(&cat.image[selected])){extern uint8_t g_tos_load_reason;
                    console_puts("\nLOAD FAILED (reason ");
                    console_put_u32((uint32_t)g_tos_load_reason);
                    console_puts(") - ESC BACK");confirm=0;}}}
    }
}
