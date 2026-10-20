#include "console.h"
#include "system.h"
#include "block_device.h"
#include "config.h"
#include "fdc.h"

/* Up to eight ACSI targets, two IDE units and the floppy. */
#define BOOT_MAX_DEVICES 11u
static uint8_t devices[BOOT_MAX_DEVICES],device_count;

static uint16_t f_le16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static int floppy_bootable(void)
{
    static uint8_t fb[512];
    return fdc_read_boot_sector(0u,fb)&&f_le16(fb+11)==512u&&f_le16(fb+22)!=0u;
}
static void print_device(uint8_t id)
{
    if(id==BLOCK_FLOPPY_ID)console_puts("FLOPPY ");
    else{console_puts((id&0x80u)?"IDE ":"ACSI ");console_putc((char)('0'+(id&0x7fu)));console_putc(' ');}
}
static void print_device_brief(uint8_t id)
{
    if(id==BLOCK_FLOPPY_ID)console_puts("FDD ");
    else{console_puts((id&0x80u)?"IDE":"ACSI");console_putc((char)('0'+(id&0x7fu)));console_putc(' ');}
}
/* The persisted boot priority: first entry is tried first, the floppy can be
   an entry too.  Empty list = AUTO (boot_device order, floppy last). */
static void print_order(void)
{
    uint8_t i;int any=0;
    if(!g_config.boot_order_valid){console_puts("AUTO");return;}
    for(i=0u;i<4u;++i){
        uint8_t d=g_config.boot_order[i];
        if(d==0xffu)break;
        if(any)console_putc('>');
        print_device_brief(d);any=1;
    }
    if(!any)console_puts("AUTO");
}

static uint8_t device_index(uint8_t id)
{
    uint8_t i;
    for(i=0;i<device_count;++i)if(devices[i]==id)return i;
    return 0;
}
static void scan_devices(void)
{
    uint8_t i;
    device_count=0;
    g_system.acsi_devices=acsi_detect_devices();g_system.acsi_scanned=1;
    for(i=0;i<8u;++i)if(g_system.acsi_devices&(uint8_t)(1u<<i))devices[device_count++]=i;
    g_system.ide_units=0u;
    for(i=0;i<2u;++i){
        uint8_t id=BLOCK_IDE_ID(i);
        if(block_probe(id)){g_system.ide_units|=(uint8_t)(1u<<i);devices[device_count++]=id;}
    }
    if(device_count<BOOT_MAX_DEVICES&&floppy_bootable())devices[device_count++]=BLOCK_FLOPPY_ID;
}
static uint8_t next_id(uint8_t current,int direction)
{
    uint8_t idx;
    if(!device_count)return current;
    idx=device_index(current);
    idx=(uint8_t)((idx+(direction>0?1u:(uint8_t)(device_count-1u)))%device_count);
    return devices[idx];
}
static uint8_t next_partition(uint8_t current,int direction)
{
    uint8_t count,p=current;
    for(count=0;count<4u;++count){
        p=(uint8_t)((p+(direction>0?1u:3u))&3u);
        if(g_system.acsi_partitions&(uint8_t)(1u<<p))return p;
    }
    return current;
}
static void draw(uint8_t field,uint8_t id,uint8_t part)
{
    uint8_t row,i;
    for(row=3;row<7u;++row)console_clear_text_row(row);
    console_set_cursor(0,0);console_puts("BOOT DEVICE SETUP\n\n");
    console_puts("DETECTED  ");
    if(!device_count)console_puts("NONE\n");
    else{
        for(i=0;i<device_count;++i)print_device_brief(devices[i]);
        console_putc('\n');
    }
    console_set_inverse(field==0);console_puts(" DEVICE  ");print_device(id);console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==1);console_puts(" BOOT PARTITION P");console_putc((char)('0'+part));console_putc(' ');
    if(id==BLOCK_FLOPPY_ID)console_puts("FDD START 0");
    else if(g_system.acsi_partitions&(uint8_t)(1u<<part)){console_puts(g_system.acsi_part_id[part]);console_puts(" START ");console_put_u32(g_system.acsi_part_start[part]);}
    else console_puts("NOT AVAILABLE");
    console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==2);console_puts(" RESCAN STORAGE ");console_set_inverse(0);console_putc('\n');
    console_set_inverse(field==3);console_puts(" BACK ");console_set_inverse(0);
    console_clear_text_row(7);console_set_cursor(0,7);console_puts("PRIORITY  ");print_order();
    console_puts("\n\nUP/DOWN SELECT  LEFT/RIGHT CHANGE\nENTER APPEND TO PRIORITY + SAVE  ESC MAIN");
}
void boot_menu_run(void)
{
    uint8_t field=0,id=0,part=0,ready=1;int key;
scan:
    console_clear();console_puts("BOOT DEVICE SETUP\n\nSCANNING STORAGE...");
    scan_devices();
    if(device_count){
        id=devices[0];
        if(id==BLOCK_FLOPPY_ID)part=0;
        else{acsi_inspect_device(&g_system,id);for(part=0;part<4u;++part)if(g_system.acsi_partitions&(uint8_t)(1u<<part))break;if(part==4u)part=0;}
    }
    console_clear();
    draw(field,id,part);
    for(;;){key=ikbd_scancode();
        if(key==0x148||key==0x150||key==0x14b||key==0x14d){ready=1;continue;}
        if(!ready)continue;
        if(key==0x48){ready=0;field=field?field-1:3;draw(field,id,part);}
        else if(key==0x50){ready=0;field=(uint8_t)((field+1)%4);draw(field,id,part);}
        else if((key==0x4b||key==0x4d)&&field==0&&device_count){
            ready=0;id=next_id(id,key==0x4d?1:-1);
            if(id==BLOCK_FLOPPY_ID)part=0;
            else{acsi_inspect_device(&g_system,id);part=next_partition(0,1);}
            draw(field,id,part);
        }
        else if((key==0x4b||key==0x4d)&&field==1){ready=0;part=next_partition(part,key==0x4d?1:-1);draw(field,id,part);}
        else if(key==0x01)return;
        else if(key==0x1c||key==0x72){
            if(field==2)goto scan;
            if(field==3)return;
            if(device_count&&(id==BLOCK_FLOPPY_ID||(g_system.acsi_partitions&(uint8_t)(1u<<part)))){
                uint8_t o[4],i,n=0;
                g_system.boot_acsi_id=id;g_system.boot_partition=part;g_system.boot_selection_valid=1;
                g_config.boot_valid=1u;g_config.boot_id=id;g_config.boot_part=part;
                /* Prepend the choice (dedup, max 4): the most recent pick is the
                   top priority, the earlier picks stay as fallbacks.  The
                   floppy (BLOCK_FLOPPY_ID) is a valid entry. */
                o[n++]=id;
                for(i=0u;i<4u&&n<4u;++i){
                    uint8_t d=g_config.boot_order_valid?g_config.boot_order[i]:0xffu;
                    if(d==0xffu)break;
                    if(d!=id)o[n++]=d;
                }
                while(n<4u)o[n++]=0xffu;
                for(i=0u;i<4u;++i)g_config.boot_order[i]=o[i];
                g_config.boot_order_valid=1u;
                (void)cfg_save();
                return;
            }
        }
    }
}