#include "hardware.h"
#include "system.h"
#include "tos_loader.h"

/* 68030 two-level translation table.
 *   TC = 0x80F09800: enable, IS=0, PS=15 (32 KiB pages), TIA=9, TIB=8.
 *   Table A index = addr[31:23], table B index = addr[22:15], offset = addr[14:0].
 * Only the regions the OS needs are mapped; everything else stays invalid and
 * faults exactly like absent hardware would.  The tables live in TF536 ALT-RAM
 * so the OS RAM pool (which starts at 0x1000) cannot overwrite them. */

#define MMU_ROM_LOGICAL  0x00E00000UL
#define MMU_IMAGE_BYTES  0x00080000UL
#define MMU_ROOT_PHYS    0x003F0000UL
#define MMU_SEC_PHYS     0x003F1000UL
#define MMU_TIA_MASK     0x1FFu
#define MMU_TIB_MASK     0xFFu

static const uint16_t mmu_tias[] = {0u,1u,2u,3u,4u,5u,6u,7u,8u,9u,510u,511u};
#define MMU_TIA_COUNT (sizeof(mmu_tias)/sizeof(mmu_tias[0]))

uint8_t mmu_crp_block[8] __attribute__((aligned(16)));
static uint8_t mmu_tramp[64] __attribute__((aligned(16)));

extern uint8_t mmu_boot_code[];
extern uint8_t mmu_boot_code_end[];

/* Read the whole catalogued image into RAM by following the FAT16 chain. */
static int load_image(const tos_image_t *im,uint8_t *dest)
{
    uint32_t remaining=im->size,offset=0u;
    uint16_t cluster=im->first_cluster;
    uint16_t io[256];
    uint32_t cluster_sectors=(uint32_t)im->sectors_per_cluster*im->sector_scale;
    if(!cluster_sectors||!remaining)return 0;
    while(remaining){
        uint32_t s;
        for(s=0u;s<cluster_sectors&&remaining;++s){
            uint32_t lba=im->data_start+((uint32_t)cluster-2u)*cluster_sectors+s;
            uint16_t amount=(remaining<512u)?(uint16_t)remaining:512u,k;
            if(!acsi_read_sector(im->acsi_id,lba,io))return 0;
            for(k=0u;k<amount;++k)dest[offset+k]=((uint8_t *)io)[k];
            offset+=amount;remaining-=amount;
        }
        if(remaining){
            uint32_t off=(uint32_t)cluster*2u;
            uint16_t next;
            if(!acsi_read_sector(im->acsi_id,im->fat_start+off/512u,io))return 0;
            next=(uint16_t)(((uint8_t *)io)[off&511u]|(((uint8_t *)io)[(off&511u)+1u]<<8));
            if(next<2u||next>=0xfff8u)return 0;
            cluster=next;
        }
    }
    return 1;
}

int mmu_boot_rom(const tos_image_t *im)
{
    uint32_t phys,i,phystop=0x003F0000UL;
    uint32_t *root,*sec;
    uint8_t *dst;
    if(!im||!im->compatible||!im->rom_address)return 0;
    if(im->size>MMU_IMAGE_BYTES)return 0;
    if(g_system.tt_ram_bytes<MMU_IMAGE_BYTES)return 0;   /* image and tables need ALT-RAM */
    phys=0x01000000UL;
    root=(uint32_t *)(uintptr_t)MMU_ROOT_PHYS;
    sec=(uint32_t *)(uintptr_t)MMU_SEC_PHYS;
    dst=(uint8_t *)(uintptr_t)phys;
    for(i=0u;i<MMU_IMAGE_BYTES;++i)dst[i]=0u;
    if(!load_image(im,dst))return 0;
    for(i=0u;i<512u;++i)root[i]=0u;
    for(i=0u;i<MMU_TIA_COUNT;++i){
        uint32_t tia=mmu_tias[i],j;
        uint32_t *t=sec+i*256u;
        root[tia]=((uint32_t)(uintptr_t)t&0xFFFFFFF0UL)|0x0Au;   /* valid 4-byte table, used */
        for(j=0u;j<256u;++j){
            uint32_t e=((tia<<23)|(j<<15))|0x19u;                /* 32 KiB page, used, modified */
            if(tia>=510u)e|=0x40u;                               /* cache inhibit: hardware */
            t[j]=e;
        }
    }
    {uint32_t rtia=(MMU_ROM_LOGICAL>>23)&MMU_TIA_MASK,rtib=(MMU_ROM_LOGICAL>>15)&MMU_TIB_MASK;
     for(i=0u;i<MMU_TIA_COUNT;++i)if(mmu_tias[i]==rtia)sec[i*256u+rtib]=(phys&0xFFFFFF00UL)|0x19u;}
    mmu_crp_block[0]=0u;mmu_crp_block[1]=0u;mmu_crp_block[2]=0u;mmu_crp_block[3]=2u;
    {uint32_t t=MMU_ROOT_PHYS;
     mmu_crp_block[4]=(uint8_t)t;mmu_crp_block[5]=(uint8_t)(t>>8);
     mmu_crp_block[6]=(uint8_t)(t>>16);mmu_crp_block[7]=(uint8_t)(t>>24);}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    *(volatile uint32_t *)0x000006fcUL=0u;
    *(volatile uint32_t *)0x00000426UL=0u;
    *(volatile uint32_t *)0x00000420UL=0x752019f3UL;
    *(volatile uint8_t  *)0x00000424UL=REG8(MMU_MEMCONF);
    *(volatile uint32_t *)0x0000042eUL=phystop;
    *(volatile uint32_t *)0x0000043aUL=0x237698aaUL;
    *(volatile uint32_t *)0x0000051aUL=0x5555aaaaUL;
    *(volatile uint32_t *)0x000005a4UL=0u;
    *(volatile uint32_t *)0x000005a8UL=0x1357bd13UL;
#pragma GCC diagnostic pop
    {uint32_t n=(uint32_t)(mmu_boot_code_end-mmu_boot_code),k;
     for(k=0u;k<n&&k<sizeof(mmu_tramp);++k)mmu_tramp[k]=mmu_boot_code[k];}
    system_image_cache_sync(g_system.cpu_type);
    ((void (*)(void))(uintptr_t)mmu_tramp)();
    return 1;
}
