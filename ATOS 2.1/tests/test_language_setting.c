#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "kernel_language.h"
static unsigned writes,size=3,attr;static int missing,fail,short_write;
static char saved[3];
enum fat_file_state kfat_stat_path(const char *p,uint32_t *s,uint8_t *a)
{assert(!strcmp(p,"\\SYSTEM\\LANGUAGE.CFG"));*s=size;*a=attr;return missing?FAT_FILE_NOT_FOUND:FAT_FILE_FOUND;}
enum fat_file_state kfat_write_path_at(const char *p,uint32_t off,const void *data,uint32_t n,uint32_t *out)
{assert(!strcmp(p,"\\SYSTEM\\LANGUAGE.CFG")&&!off&&n==3);writes++;memcpy(saved,data,3);*out=short_write?2:3;return fail?FAT_FILE_IO_FAILED:FAT_FILE_FOUND;}
int main(void)
{assert(!kernel_language_save(0)&&!memcmp(saved,"EN\n",3));assert(!kernel_language_save(1)&&!memcmp(saved,"PL\n",3));
 assert(kernel_language_save(2)<0&&writes==2);size=2;assert(kernel_language_save(0)<0&&writes==2);size=3;
 attr=1;assert(kernel_language_save(0)<0&&writes==2);attr=16;assert(kernel_language_save(0)<0&&writes==2);attr=0;
 missing=1;assert(kernel_language_save(0)<0&&writes==2);missing=0;
 fail=1;assert(kernel_language_save(0)<0);fail=0;short_write=1;assert(kernel_language_save(0)<0);
 puts("Language setting PASS: EN/PL write, bounded selector, missing/invalid/read-only file and failed/short writes");return 0;}
