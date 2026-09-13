#ifndef ASF_BOOT_PROTOCOL_H
#define ASF_BOOT_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>

#define BOOT_IMAGE_MAGIC "ASYSBOOT"
#define BOOT_IMAGE_VERSION 1u
#define BOOT_IMAGE_HEADER_SIZE 32u
#define BOOT_HANDOFF_MAGIC 0x4153484fUL /* ASHO */
#define BOOT_HANDOFF_VERSION 5u
#define MODULE_IMAGE_MAGIC "ASYSMOD0"
#define MODULE_ABI_VERSION 3u

typedef struct {uint32_t file_size,memory_size,entry_offset;} module_image_info_t;

enum { BOOT_FPU_NONE, BOOT_FPU_EXTERNAL, BOOT_FPU_INTEGRATED };

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t machine_type;
    uint8_t cpu_type;
    uint8_t video_mode;
    uint8_t boot_acsi_id;
    uint8_t boot_partition;
    uint8_t fpu_type;
    uint8_t clock_mhz;
    uint8_t clock_known;
    uint8_t blitter_present;
    uint8_t blitter_enabled;
    uint8_t reserved0;
    uint8_t reserved1;
    uint32_t st_ram_bytes;
    uint32_t tt_ram_bytes;
    void (*console_clear)(void);
    void (*console_puts)(const char *);
    void (*console_put_u32)(uint32_t);
    void (*start_kernel)(void);
    int (*load_module)(const char name[11],void *destination,uint32_t capacity,module_image_info_t *info);
    int (*acsi_read_sector)(uint8_t id,uint32_t sector,void *buffer);
    int (*acsi_write_sector)(uint8_t id,uint32_t sector,const void *buffer);
    /* ET4000 8bpp graphics presentation (T2 desktop compositor). */
    void (*video_enter)(void);
    void (*video_leave)(void);
    void (*video_clear)(uint8_t color);
    void (*video_fill)(int x,int y,int w,int h,uint8_t color);
    void (*video_text)(int x,int y,const char *text,uint8_t fg,uint8_t bg);
    void (*video_blit_mono)(int x,int y,int w,int h,const uint8_t *source,int stride,
                            uint8_t fg,uint8_t bg);
} boot_handoff_t;

/* MC68000 faults on unaligned longword access. Keep every 32-bit handoff
   member even-aligned and make ABI drift fail at compile time. */
_Static_assert(offsetof(boot_handoff_t,st_ram_bytes)==20u,"boot handoff RAM alignment");
_Static_assert(offsetof(boot_handoff_t,console_clear)==28u,"boot handoff callback alignment");
_Static_assert(sizeof(boot_handoff_t)==80u,"boot handoff ABI size");

#endif
