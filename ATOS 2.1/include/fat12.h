#ifndef ASF_FAT12_H
#define ASF_FAT12_H
#include <stdint.h>

/* Read-only FAT12 over the floppy controller (drive A). */
int fat12_mount(void);                 /* parse the boot sector; 1 = ok */
int fat12_find(const char *name,       /* "TEST.TXT" (8.3) */
               uint32_t *size,uint16_t *cluster);
int fat12_read(uint16_t cluster,uint32_t size,void *buffer,uint32_t capacity,
               uint32_t *read_size);
/* Basic open/read entry point used by diagnostics. */
int fat12_read_file(const char *name,void *buffer,uint32_t capacity,uint32_t *size);
/* Enumerate the root directory (index from 0); name becomes "NAME.EXT". */
int fat12_dir(uint32_t index,char name[13],uint32_t *size,uint8_t *attr);
/* Probe used by F3 HARDWARE: mount and read TEST.TXT, publish the result. */
int fat12_probe(void);
extern volatile uint32_t fat12_dbg_status,fat12_dbg_sig;

#endif
