#ifndef ASF_FDC_H
#define ASF_FDC_H
#include <stdint.h>

/* WD1772 floppy controller access through the ST DMA chip (ST/STE).
   Reads one 512-byte sector.  Returns 1 on success, 0 on error/timeout. */
int fdc_read_sector(uint8_t drive,uint8_t track,uint8_t side,uint8_t sector,void *buffer);
/* Read the boot sector (track 0, side 0, sector 1); retries for motor spin-up. */
int fdc_read_boot_sector(uint8_t drive,void *buffer);
/* Result of the last probe: 1 = ok, 0 = error; first four bytes big-endian. */
extern volatile uint32_t fdc_dbg_status,fdc_dbg_sig;

#endif
