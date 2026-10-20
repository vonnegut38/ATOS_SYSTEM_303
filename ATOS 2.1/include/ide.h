#ifndef ASF_IDE_H
#define ASF_IDE_H
#include <stdint.h>

/* Low-level ATA/IDE driver for the TF536 (and any Falcon-style) IDE window.
 * The boot id space uses bit 7 to mark an IDE unit, so the existing AHDI/FAT
 * loader can share one sector-read entry point:
 *     0x00..0x07  ACSI targets
 *     0x80..0x81  IDE master/slave
 */
#define IDE_ID_FLAG 0x80u

int ide_detect(void);
uint8_t ide_units_present(void);
int ide_present(uint8_t unit);
int ide_model(uint8_t unit,char *name,uint8_t capacity);
uint32_t ide_capacity_sectors(uint8_t unit);
/* Capacity captured by the last ide_detect() (no extra IDENTIFY I/O). */
uint32_t ide_capacity_cached(uint8_t unit);
int ide_read_sectors(uint8_t unit,uint32_t lba,uint16_t count,void *buffer);
int ide_write_sectors(uint8_t unit,uint32_t lba,uint16_t count,const void *buffer);
uint32_t ide_base_address(void);

#endif
