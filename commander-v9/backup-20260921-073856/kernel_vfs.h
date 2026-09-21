#ifndef ASF_KERNEL_VFS_H
#define ASF_KERNEL_VFS_H
#include <stdint.h>

#define KERNEL_VFS_MAGIC 0x56465320UL
#define KERNEL_VFS_VERSION 3u
typedef int (*kernel_vfs_list_callback_t)(const char *name,uint32_t size,
                                          uint32_t attributes,void *context);
typedef struct {
    uint32_t magic;
    uint16_t version,size;
    int (*stat_system)(const char name[11],uint32_t *size);
    int (*read_system)(const char name[11],void *destination,
                       uint32_t capacity,uint32_t *size);
    int (*stat_path)(const char *path,uint32_t *size,uint8_t *attributes);
    int (*read_path)(const char *path,void *destination,
                     uint32_t capacity,uint32_t *size);
    int (*list_path)(const char *path,kernel_vfs_list_callback_t callback,
                     void *context);
    int (*read_path_at)(const char *path,uint32_t offset,void *destination,
                        uint32_t capacity,uint32_t *size);
} kernel_vfs_service_t;

#endif
