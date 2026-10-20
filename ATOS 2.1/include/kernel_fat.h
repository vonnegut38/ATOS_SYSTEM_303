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
/* Per-console mounts: select the calling console's volume before any access.
   Returns whether that console has a mounted volume. */
int kfat_select_session(unsigned session);
const char *kfat_partition_id(void);
uint16_t kfat_sector_size(void);
enum fat_health kfat_health(void);
/* Naprawa znacznika czystosci (FAT[1]): sprawdza obie kopie FAT, potem ustawia
   bity 14+15.  Zwraca 0 albo ujemny FAT_WRITE_*.  Tylko na zadanie uzytkownika. */
int kfat_volume_repair(void);
/* Geometry of the active volume in TOS logical sectors; fails if no volume
   is mounted or its geometry cannot be represented by the nine BPB words. */
int kfat_tos_bpb(uint16_t words[9]);
int kfat_free_clusters(uint32_t *count);
enum fat_file_state kfat_find_system_file(const char name[11],uint32_t *size);
/* Graphics drivers live in \SYSTEM\DRV; the normal loader reads \SYSTEM flat. */
enum fat_file_state kfat_find_driver_file(const char name[11],uint32_t *size);
enum fat_file_state kfat_load_driver_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size);
enum fat_file_state kfat_load_system_file(const char name[11],void *destination,
                                          uint32_t capacity,uint32_t *size);
/* datetime is the packed FAT write date/time: (date<<16)|time, 0 if absent. */
typedef int (*kfat_list_callback_t)(const char *name,uint32_t size,
                                    uint32_t attributes,uint32_t datetime,
                                    void *context);
enum fat_file_state kfat_stat_path(const char *path,uint32_t *size,uint8_t *attributes);
enum fat_file_state kfat_load_path(const char *path,void *destination,
                                   uint32_t capacity,uint32_t *size);
enum fat_file_state kfat_read_path_at(const char *path,uint32_t offset,
                                      void *destination,uint32_t capacity,
                                      uint32_t *size);
/* Sequential-read cursor.
   kfat_read_path_at() resolves the path and then walks the FAT chain from the
   FIRST cluster up to the requested offset, every single call.  A file read in
   chunks therefore walks the same chain once per chunk: for one large file that
   was tens of thousands of FAT reads for 512-byte chunks against a few thousand
   for 4 KB ones.  A cursor remembers the cluster the previous read stopped in,
   plus the file's start cluster and size, so a continuation read starts where
   the last one ended and needs no chain walk and no directory search at all.
   Zeroed (or valid==0) means "no cached position": the call then behaves like
   kfat_read_path_at() and fills the cursor in. */
typedef struct {
    uint32_t offset;        /* file offset the cached cluster starts at */
    uint32_t size;          /* file size seen when the cursor was filled */
    uint16_t cluster;       /* cluster holding byte 'offset' */
    uint16_t start_cluster; /* first cluster, so a rewind needs no lookup */
    uint8_t  valid;
    uint8_t  reserved;
} fat_cursor_t;
enum fat_file_state kfat_read_cursor(const char *path,uint32_t offset,
                                     void *destination,uint32_t capacity,
                                     uint32_t *size,fat_cursor_t *cursor);
/* Diagnostics: FAT entries read since mount, and the peak per single call.
   A continuation read that needs zero entries proves the cursor works. */
extern uint32_t kfat_counts[4];
/* Write inside an existing file (sector transactions with rollback). Growing a
   file is a later step; offset beyond EOF reports zero written. */
enum fat_file_state kfat_write_path_at(const char *path,uint32_t offset,
                                       const void *source,uint32_t count,
                                       uint32_t *size);
enum fat_file_state kfat_list_path(const char *path,kfat_list_callback_t callback,
                                   void *context);

/* Mutations return zero or a negative FAT_WRITE_* code. Existing targets
 * are never overwritten. Deletes accept files and empty directories only. */
enum {FAT_WRITE_IO=1,FAT_WRITE_INVALID,FAT_WRITE_EXISTS,FAT_WRITE_FULL,
      FAT_WRITE_NOT_EMPTY,FAT_WRITE_READ_ONLY,FAT_WRITE_LIMIT};
int kfat_copy_path(const char *source,const char *target);
/* Create or replace a file with exactly these bytes (editor save). Shrinking is
 * supported: the old chain is released before the new content is written. */
int kfat_save_path(const char *path,const void *source,uint32_t size);
int kfat_mkdir_path(const char *path);
int kfat_create_path(const char *path);
int kfat_delete_path(const char *path);
/* Trash is a metadata move; purge is restricted to descendants of /TRASH.
 * Purge commits each item separately; on failure, remaining items stay visible. */
int kfat_move_path(const char *source,const char *target);
/* Compatibility shims for the TOS persona (older, richer FAT API). */
static inline int kfat_rename_path(const char *src,const char *dst){return kfat_move_path(src,dst);}
static inline int kfat_create_dir(const char *path){return kfat_mkdir_path(path);}
static inline int kfat_attrib_path(const char *path,uint16_t mode,uint16_t value,uint8_t *attributes)
{
    uint8_t a=0;int r;
    (void)mode;
    (void)value;            /* setting attributes is not supported yet */
    r=kfat_stat_path(path,0,&a);
    if(attributes)*attributes=a;
    return r;
}
int kfat_trash_path(const char *source);
int kfat_purge_path(const char *source);
#endif
