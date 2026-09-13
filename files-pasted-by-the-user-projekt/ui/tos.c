#include "console.h"
#include "system.h"
#include "config.h"
#include "ide.h"
#include "tos_loader.h"

/* Make sure we have a boot device and partition to read \ROMS from.  Uses the
 * persisted F4 selection when present, otherwise probes in the configured
 * order (0 auto = ACSI then IDE, 1 ACSI only, 2 IDE only). */
static int ensure_selection(void)
{
    uint8_t id=0u,unit,part,found=0u;
    if(g_system.boot_selection_valid)return 1;
    if(g_config.boot_device!=2u){
        for(id=0u;id<8u;++id)if(acsi_probe_device(id)){found=1u;break;}
    }
    if(!found&&g_config.boot_device!=1u){
        for(unit=0u;unit<2u;++unit)
            if(acsi_probe_device((uint8_t)(IDE_ID_FLAG|unit))){id=(uint8_t)(IDE_ID_FLAG|unit);found=1u;break;}
    }
    if(!found)return 0;
    if(!acsi_inspect_device(&g_system,id))return 0;
    for(part=0u;part<g_system.acsi_part_count;++part)if(g_system.acsi_partitions&((uint32_t)1u<<part))break;
    if(part>=4u)return 0;
    g_system.boot_acsi_id=id;
    g_system.boot_partition=part;
    g_system.boot_selection_valid=1u;
    g_system.boot_ide=(uint8_t)((id&IDE_ID_FLAG)?1u:0u);
    return 1;
}

static void draw_version(uint16_t version)
{
    console_putc((char)('0'+((version>>8)&15u)));
    console_putc('.');
    console_putc((char)('0'+((version>>4)&15u)));
    console_putc((char)('0'+(version&15u)));
}

static void draw(const tos_catalog_t *cat,uint8_t selected)
{
    uint8_t i;
    console_clear();
    console_puts("TOS / EMUTOS SELECTION\n\n");
    if(!cat->fat_valid){console_puts("\\ROMS: PARTITION IS NOT FAT16\n\nESC BACK");return;}
    if(!cat->directory_found){console_puts("DIRECTORY \\ROMS NOT FOUND\n\nESC BACK");return;}
    if(!cat->count){console_puts("\\ROMS IS EMPTY\n\nESC BACK");return;}
    for(i=0u;i<cat->count;++i){
        const tos_image_t *im=&cat->image[i];
        console_set_inverse((uint8_t)(i==selected));
        console_putc(' ');
        console_puts(im->name);
        if(im->compatible){
            console_puts("  TOS ");
            draw_version(im->version);
            console_puts(im->loadable?"  RAM LOAD":"  ROM MAP ");
        } else {
            console_puts("  INVALID ");
        }
        console_set_inverse(0);
        console_putc('\n');
    }
    console_puts("\nUP/DOWN SELECT   ENTER LOAD+START   ESC BACK");
}

void tos_menu_run(void)
{
    tos_catalog_t cat;uint8_t selected=0u,ready=1u;int key;
    console_clear();
    console_puts("TOS / EMUTOS SELECTION\n\n");
    if(!ensure_selection()){
        console_puts("NO BOOT PARTITION FOUND\n\nESC BACK");
        for(;;){key=ikbd_scancode();if(key==0x01||key==0x1c||key==0x72)return;}
    }
    console_puts("SCANNING \\ROMS ...");
    tos_catalog_scan(&cat,g_system.boot_acsi_id,g_system.acsi_part_start[g_system.boot_partition]);
    draw(&cat,selected);
    for(;;){
        key=ikbd_scancode();
        if(key==0x148||key==0x150){ready=1u;continue;}
        if(key==0x01)return;
        if(!ready)continue;
        if(key==0x48&&cat.count){ready=0u;selected=selected?selected-1u:(uint8_t)(cat.count-1u);draw(&cat,selected);}
        else if(key==0x50&&cat.count){ready=0u;selected=(uint8_t)((selected+1u)%cat.count);draw(&cat,selected);}
        else if((key==0x1c||key==0x72)&&cat.count){
            const tos_image_t *im=&cat.image[selected];
            if(im->compatible&&im->rom_address&&!im->loadable){
                console_clear();
                console_puts("LOADING ");console_puts(im->name);
                console_puts(" ...\n68030 MMU MAP -> STARTING AS ROM");
                (void)mmu_boot_rom(im);
                console_puts("\nMMU BOOT FAILED - ESC BACK");
                continue;
            }
            if(!im->loadable){
                console_puts("\n\nNOT LOADABLE (NO MMU ADDRESS / PRG)");
                continue;
            }
            console_clear();
            console_puts("LOADING ");console_puts(im->name);
            console_puts(" ...\nSTARTING AS ROM");
            (void)tos_load_once(im);
            console_puts("\nLOAD FAILED - ESC BACK");
        }
    }
}
