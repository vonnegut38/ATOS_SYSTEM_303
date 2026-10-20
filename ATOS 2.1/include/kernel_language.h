#ifndef KERNEL_LANGUAGE_H
#define KERNEL_LANGUAGE_H
#include "kernel_fat.h"
/* Narrow setter while the native file API has no general write operation.
 * Only the installed three-byte setting is writable; no user pointers/paths. */
static int kernel_language_save(uint32_t language)
{
 uint32_t size=0,written=0;uint8_t attr=0;
 const char *value=language?"PL\n":"EN\n";
 if(language>1)return -FAT_WRITE_INVALID;
 if(kfat_stat_path("\\SYSTEM\\LANGUAGE.CFG",&size,&attr)!=FAT_FILE_FOUND||size!=3||(attr&0x11))return -FAT_WRITE_INVALID;
 if(kfat_write_path_at("\\SYSTEM\\LANGUAGE.CFG",0,value,3,&written)!=FAT_FILE_FOUND||written!=3)return -FAT_WRITE_IO;
 return 0;
}
#endif
