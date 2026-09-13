#ifndef ASF_KERNEL_FAT_H
#define ASF_KERNEL_FAT_H

#include <stdint.h>

enum fat_state {FAT_PARTITION_INVALID,FAT_IO_FAILED,FAT_BPB_INVALID,
                FAT_UNSUPPORTED,FAT_MOUNTED};
enum fat_health {FAT_HEALTH_UNKNOWN,FAT_HEALTH_CLEAN,FAT_HEALTH_DIRTY,
                 FAT_HEALTH_HARD_ERROR};
enum fat_file_state {FAT_FILE_NOT_FOUND,FAT_FILE_IO_FAILED,
                     FAT_FILE_CORRUPT,FAT_FILE_FOUND};

enum fat_state kfat_mount(uint32_t start,const char id[4]);
const char *kfat_partition_id(void);
uint16_t kfat_sector_size(void);
uint8_t kfat_fat32(void);
enum fat_health kfat_health(void);
enum fat_file_state kfat_find_system_file(const char name[11],uint32_t *size);
enum fat_file_state kfat_load_system_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size);
typedef int (*kfat_list_callback_t)(const char *name,uint32_t size,
                                    uint32_t attributes,void *context);
enum fat_file_state kfat_stat_path(const char *path,uint32_t *size,uint8_t *attributes);
enum fat_file_state kfat_load_path(const char *path,void *destination,
                                   uint32_t capacity,uint32_t *size);
enum fat_file_state kfat_read_path_at(const char *path,uint32_t offset,
                                      void *destination,uint32_t capacity,
                                      uint32_t *size);
enum fat_file_state kfat_list_path(const char *path,kfat_list_callback_t callback,
                                   void *context);
enum fat_file_state kfat_create_path(const char *path);
enum fat_file_state kfat_write_path_at(const char *path,uint32_t offset,
                                       const void *source,uint32_t size,uint32_t *written);
enum fat_file_state kfat_delete_path(const char *path);
enum fat_file_state kfat_rename_path(const char *from,const char *to);
enum fat_file_state kfat_create_dir(const char *path);
enum fat_file_state kfat_attrib_path(const char *path,uint8_t set,uint8_t attr,uint8_t *old);

#endif
