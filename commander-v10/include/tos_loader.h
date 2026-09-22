#ifndef ASF_TOS_LOADER_H
#define ASF_TOS_LOADER_H
#include <stdint.h>

#define TOS_CATALOG_MAX 8

typedef struct {
    char name[13];
    uint32_t size;
    uint32_t first_sector;
    uint16_t version;
    uint32_t rom_address;
    uint8_t compatible;
    uint8_t loadable;
    uint8_t acsi_id;
    uint8_t sectors_per_cluster;
    uint8_t sector_scale;
    uint16_t first_cluster;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t payload_offset;
    uint32_t payload_size;
    uint32_t load_address;
} tos_image_t;

typedef struct {
    tos_image_t image[TOS_CATALOG_MAX];
    uint8_t count;
    uint8_t fat_valid;
    uint8_t directory_found;
} tos_catalog_t;

void tos_catalog_scan(tos_catalog_t *catalog,uint8_t acsi_id,uint32_t partition_start);
int tos_load_once(const tos_image_t *image);
/* Boot a ROM-mapped image by mapping RAM under its logical address (68030 MMU). */
int mmu_boot_rom(const tos_image_t *image);
void tos_menu_run(void);
#endif
