#include "console.h"
#include "system.h"
#include "ide.h"
#include "config.h"
#include "damage.h"

#define BOOT_MAX_DEVS 10
#define ROW_TITLE 0u
#define ROW_DET   2u
#define ROW_CART  3u
#define ROW_DEV   5u
#define ROW_PART  6u
#define ROW_RESCAN 7u
#define ROW_BACK  8u
#define ROW_HELP  10u
#define ROW_COST  12u

static uint8_t dev_ids[BOOT_MAX_DEVS];
static uint8_t dev_count;
static uint8_t dev_sel;
static uint8_t part_sel;
static uint8_t field;

static void dev_label(uint8_t id,char *out)
{
    uint8_t n=0u;
    if(id&IDE_ID_FLAG){
        out[n++]='I';out[n++]='D';out[n++]='E';out[n++]=' ';
        out[n++]='U';out[n++]='N';out[n++]='I';out[n++]='T';out[n++]=' ';
        out[n++]=(char)('0'+(id&0x7fu));
    } else {
        out[n++]='A';out[n++]='C';out[n++]='S';out[n++]='I';out[n++]=' ';
        out[n++]='I';out[n++]='D';out[n++]=' ';
        out[n++]=(char)('0'+(id&0x7fu));
    }
    out[n]='\0';
}

static uint8_t partition_count(void)
{return g_system.acsi_part_count?g_system.acsi_part_count:1u;}

static uint8_t first_partition(void)
{
    uint8_t p,n=partition_count();
    for(p=0u;p<n;++p)if(g_system.acsi_partitions&((uint32_t)1u<<p))return p;
    return 0u;
}

static uint8_t next_partition(uint8_t current,int direction)
{
    uint8_t count,n=partition_count(),p=current;
    for(count=0u;count<n;++count){
        p=(uint8_t)((p+(direction>0?1u:(uint8_t)(n-1u)))%n);
        if(g_system.acsi_partitions&((uint32_t)1u<<p))return p;
    }
    return current;
}

/* IDE first so the TF536 device is the default on our target. */
static void scan_storage(void)
{
    uint8_t id,unit;
    dev_count=0u;dev_sel=0u;part_sel=0u;
    (void)ide_detect();
    g_system.ide_units=ide_units_present();
    for(unit=0u;unit<2u;++unit)
        if(g_system.ide_units&(uint8_t)(1u<<unit))dev_ids[dev_count++]=(uint8_t)(IDE_ID_FLAG|unit);
    g_system.acsi_devices=acsi_detect_devices();
    for(id=0u;id<8u;++id)
        if(g_system.acsi_devices&(uint8_t)(1u<<id))dev_ids[dev_count++]=id;
    if(dev_count){ (void)acsi_inspect_device(&g_system,dev_ids[0]); part_sel=first_partition(); }
}

static void draw_row(uint8_t row)
{
    char label[16];
    uint8_t i;
    console_clear_text_row(row);
    console_set_cursor(0,row);
    switch(row){
        case ROW_TITLE:
            console_set_inverse(1);
            console_puts("  BOOT DEVICE SETUP  -  WYBOR URZADZENIA BOOTUJACEGO  ");
            for(i=54u;i<80u;++i)console_putc(' ');
            console_set_inverse(0);
            break;
        case ROW_DET:
            console_puts(" DETECTED: ");
            if(!dev_count)console_puts("NONE");
            else for(i=0u;i<dev_count;++i){
                if(i)console_putc(' ');
                console_puts(i==dev_sel?"[" : "");
                dev_label(dev_ids[i],label);console_puts(label);
                console_puts(i==dev_sel?"]" : "");
            }
            break;
        case ROW_CART:
            console_puts(" CARTRIDGE: ");
            if(g_system.netusbee_ethernet||g_system.netusbee_usb||g_system.cart_rom_present){
                if(g_system.netusbee_ethernet)console_puts("NETUSBEE ETH ");
                if(g_system.netusbee_usb)console_puts("NETUSBEE USB ");
                if(g_system.cart_rom_present){console_puts("ROM ");console_puts(g_system.cart_rom_name[0]?g_system.cart_rom_name:"(UNNAMED)");}
            } else console_puts("NONE DETECTED");
            break;
        case ROW_DEV:
            console_set_inverse((uint8_t)(field==0u));
            console_puts(" DEVICE      ");
            if(dev_count){dev_label(dev_ids[dev_sel],label);console_puts(label);}
            else console_puts("NONE");
            console_set_inverse(0);
            break;
        case ROW_PART:
            console_set_inverse((uint8_t)(field==1u));
            console_puts(" PARTITION   ");
            if(dev_count&&(g_system.acsi_partitions&((uint32_t)1u<<part_sel))){
                console_puts("P");console_putc((char)('0'+part_sel));console_putc(' ');
                console_puts(g_system.acsi_part_id[part_sel]);
                console_puts(" START ");console_put_u32(g_system.acsi_part_start[part_sel]);
            } else console_puts("NOT AVAILABLE");
            console_set_inverse(0);
            break;
        case ROW_RESCAN:
            console_set_inverse((uint8_t)(field==2u));console_puts(" RESCAN STORAGE ");console_set_inverse(0);
            break;
        case ROW_BACK:
            console_set_inverse((uint8_t)(field==3u));console_puts(" BACK ");console_set_inverse(0);
            break;
        case ROW_HELP:
            console_puts(" UP/DOWN SELECT   LEFT/RIGHT CHANGE   ENTER SAVE   ESC CANCEL");
            break;
        default: break;
    }
}

static void draw_cost(uint8_t rects,uint32_t px)
{
    console_clear_text_row(ROW_COST);console_set_cursor(0,ROW_COST);
    console_puts(" REDRAW: ");console_put_u32(rects);console_puts(" RECT  ");
    console_put_u32(px);console_puts(" PX    FULL SCREEN = 307200 PX");
}

static void redraw_field(uint8_t old_field,uint8_t new_field)
{
    uint8_t r;uint32_t p;
    damage_reset();
    if(old_field!=new_field)draw_row((uint8_t)(ROW_DEV+old_field));
    draw_row((uint8_t)(ROW_DEV+new_field));
    r=damage_count();p=damage_pixels();draw_cost(r,p);
}

static void apply(void)
{
    uint8_t id=dev_ids[dev_sel];
    g_system.boot_acsi_id=id;
    g_system.boot_partition=part_sel;
    g_system.boot_selection_valid=1u;
    g_system.boot_ide=(uint8_t)((id&IDE_ID_FLAG)?1u:0u);
    g_config.boot_id=id;
    g_config.boot_part=part_sel;
    g_config.boot_valid=1u;
    g_config.boot_device=(uint8_t)((id&IDE_ID_FLAG)?2u:1u);
    (void)cfg_save();
}

void boot_menu_run(void)
{
    int key;uint8_t ready=1u;
    field=0u;
    scan_storage();
    damage_reset();
    console_clear();
    draw_row(ROW_TITLE);draw_row(ROW_DET);draw_row(ROW_CART);
    draw_row(ROW_DEV);draw_row(ROW_PART);draw_row(ROW_RESCAN);
    draw_row(ROW_BACK);draw_row(ROW_HELP);
    draw_cost(damage_count(),damage_pixels());
    for(;;){
        key=ikbd_scancode();
        if(key==0x148||key==0x150||key==0x14b||key==0x14d){ready=1u;continue;}
        if(!ready)continue;
        if(key==0x48){ready=0u;{uint8_t o=field;field=field?field-1u:3u;redraw_field(o,field);}}
        else if(key==0x50){ready=0u;{uint8_t o=field;field=(uint8_t)((field+1u)%4u);redraw_field(o,field);}}
        else if(key==0x4b||key==0x4d){
            ready=0u;damage_reset();
            if(field==0u&&dev_count){
                dev_sel=(uint8_t)((dev_sel+(key==0x4d?1u:dev_count-1u))%dev_count);
                (void)acsi_inspect_device(&g_system,dev_ids[dev_sel]);
                part_sel=first_partition();
                draw_row(ROW_DET);draw_row(ROW_DEV);draw_row(ROW_PART);
            } else if(field==1u&&dev_count){
                part_sel=next_partition(part_sel,key==0x4d?1:-1);
                draw_row(ROW_PART);
            }
            {uint8_t r=damage_count();uint32_t p=damage_pixels();draw_cost(r,p);}
        }
        else if(key==0x01)return;
        else if(key==0x1c||key==0x72){
            if(field==2u){damage_reset();scan_storage();draw_row(ROW_DET);draw_row(ROW_DEV);draw_row(ROW_PART);{uint8_t r=damage_count();uint32_t p=damage_pixels();draw_cost(r,p);}}
            else if(field==3u)return;
            else if(dev_count&&(g_system.acsi_partitions&((uint32_t)1u<<part_sel))){apply();return;}
        }
    }
}
