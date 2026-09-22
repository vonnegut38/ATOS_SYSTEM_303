#include "console.h"
#include "system.h"

static uint8_t next_id(uint8_t current,int direction)
{
    uint8_t count,id=current;
    for (count=0;count<8u;++count) {
        id=(uint8_t)((id+(direction>0?1u:7u))&7u);
        if (g_system.acsi_devices&(uint8_t)(1u<<id)) return id;
    }
    return current;
}

static uint8_t next_partition(uint8_t current,int direction)
{
    uint8_t count,p=current;
    for (count=0;count<4u;++count) {
        p=(uint8_t)((p+(direction>0?1u:3u))&3u);
        if (g_system.acsi_partitions&(uint8_t)(1u<<p)) return p;
    }
    return current;
}

static void draw(uint8_t field,uint8_t id,uint8_t part)
{
    uint8_t row;
    for(row=3;row<7u;++row)console_clear_text_row(row);
    console_set_cursor(0,0);console_puts("BOOT DEVICE SETUP\n\n");
    console_puts("DETECTED ACSI ");
    if (!g_system.acsi_devices) console_puts("NONE\n");
    else { uint8_t i; for(i=0;i<8u;++i)if(g_system.acsi_devices&(uint8_t)(1u<<i)){console_putc((char)('0'+i));console_putc(' ');}console_putc('\n'); }
    console_set_inverse(field==0); console_puts(" ACSI DEVICE   ID ");console_putc((char)('0'+id));console_putc(' ');console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==1); console_puts(" BOOT PARTITION P");console_putc((char)('0'+part));console_putc(' ');
    if(g_system.acsi_partitions&(uint8_t)(1u<<part)){console_puts(g_system.acsi_part_id[part]);console_puts(" START ");console_put_u32(g_system.acsi_part_start[part]);}
    else {
        console_puts("NOT AVAILABLE");
    }
    console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==2);console_puts(" RESCAN STORAGE ");console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==3);console_puts(" BACK ");console_set_inverse(0);
    console_puts("\n\nUP/DOWN SELECT  LEFT/RIGHT CHANGE\nENTER SELECT  ESC MAIN");
}

void boot_menu_run(void)
{
    uint8_t field=0,id=0,part=0,ready=1; int key;
scan:
    console_clear();console_puts("BOOT DEVICE SETUP\n\nSCANNING ACSI ID 0-7...");
    g_system.acsi_devices=acsi_detect_devices();g_system.acsi_scanned=1;
    if(g_system.acsi_devices){for(id=0;id<8u;++id)if(g_system.acsi_devices&(uint8_t)(1u<<id))break;acsi_inspect_device(&g_system,id);for(part=0;part<4u;++part)if(g_system.acsi_partitions&(uint8_t)(1u<<part))break;if(part==4u)part=0;}
    console_clear();
    draw(field,id,part);
    for(;;){key=ikbd_scancode();if(key==0x148||key==0x150||key==0x14b||key==0x14d){ready=1;continue;}if(!ready)continue;
        if(key==0x48){ready=0;field=field?field-1:3;draw(field,id,part);}
        else if(key==0x50){ready=0;field=(uint8_t)((field+1)%4);draw(field,id,part);}
        else if((key==0x4b||key==0x4d)&&field==0&&g_system.acsi_devices){ready=0;id=next_id(id,key==0x4d?1:-1);acsi_inspect_device(&g_system,id);part=next_partition(0,1);draw(field,id,part);}
        else if((key==0x4b||key==0x4d)&&field==1){ready=0;part=next_partition(part,key==0x4d?1:-1);draw(field,id,part);}
        else if(key==0x01)return;
        else if(key==0x1c||key==0x72){if(field==2)goto scan;if(field==3)return;if(g_system.acsi_devices&&(g_system.acsi_partitions&(uint8_t)(1u<<part))){g_system.boot_acsi_id=id;g_system.boot_partition=part;g_system.boot_selection_valid=1;return;}}
    }
}
