#ifndef ASF_NVRAM_H
#define ASF_NVRAM_H
#include "config.h"

/* Battery-backed NVRAM (MegaSTE/TT/Falcon at 0xffff8961/63).  The clock uses
   offsets 0..11, so a magic+CRC record with the boot preference lives near the
   end.  Everything degrades to the on-disk \BIOS\BIOS.CFG when the NVRAM is
   absent or the record is invalid: the boot priority the user set in SETUP
   should survive even when the config file cannot be reached. */
int nvram_available(void);
int nvram_load_boot(bios_config_t *cfg);
int nvram_save_boot(const bios_config_t *cfg);
#endif
