#ifndef ASF_BLOCK_DEVICE_H
#define ASF_BLOCK_DEVICE_H
#include <stdint.h>

/* Phase 4 (v10): one block layer for ACSI and IDE (SCSI later).
 * FAT/VFS/kernel use only these calls and never test the transport.
 *
 * Device id encoding (opaque handle):
 *     0x00..0x07   ACSI target
 *     0x80..0x81   IDE master / slave
 */
#define BLOCK_IDE_FLAG   0x80u
#define BLOCK_ACSI_ID(n) ((uint8_t)((n)&0x07u))
#define BLOCK_IDE_ID(n)  ((uint8_t)(0x80u|((n)&0x01u)))
#define BLOCK_IS_IDE(id) (((id)&BLOCK_IDE_FLAG)!=0u)
#define BLOCK_UNIT(id)   ((uint8_t)((id)&0x7fu))

typedef struct {
    uint8_t  id;
    uint8_t  is_ide;      /* 0 = ACSI, 1 = IDE */
    uint8_t  present;
    char     model[24];
    uint32_t sectors;     /* 0 when the capacity could not be determined */
} block_device_info_t;

/* probe: 1 = the device answered. */
int block_probe(uint8_t id);
/* read/write: count sectors of 512 bytes.  write is experimental and returns
   0 when the transport is read-only (current ACSI). */
int block_read(uint8_t id,uint32_t lba,uint16_t count,void *buffer);
int block_write(uint8_t id,uint32_t lba,uint16_t count,const void *buffer);
int block_flush(uint8_t id);
int block_get_info(uint8_t id,block_device_info_t *info);
/* Fill a bitmap of present ACSI ids (bits 0..7) via 'acsi_out'. */
uint8_t block_detect_acsi(void);

#endif
